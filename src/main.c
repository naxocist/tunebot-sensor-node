// main.c - TuneBot Guitar node (ESP32 #1, on the headstock)
//
// Pick a stage, then Upload and Monitor:
//   1 = mic one-shot test (avg / min / max / swing on GPIO34)
//   2 = MPU6050 test (accel, gyro, "moving" flag)
//   3 = DMA sample-rate calibration -> paste result into TRUE_SAMPLE_RATE (tuning.h)
//   4 = tuner only (Hz, string, cents), no radio
//   5 = full node: tuner + MPU6050 + ESP-NOW packets to the gateway   <- checkpoint
#define NODE_STAGE 5

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "audio.h"
#include "yin.h"
#include "tuning.h"
#include "mpu6050.h"
#include "espnow_link.h"

#define IN_TUNE_CENTS     2.0f
#define MOVE_GYRO_DPS     15.0f   /* rotation faster than this = moving */
#define MOVE_ACCEL_G      0.08f   /* |a| further than this from 1 g = moving */
#define MOVE_HOLD_MS      1500    /* stay "moving" this long after the last motion */

static uint16_t s_frame[FRAME_LEN];
static float    s_signal[FRAME_LEN];
static volatile bool s_moving = false;

/* ---------- stage 1: mic one-shot test ---------- */
__attribute__((unused)) static void stage_mic_test(void)
{
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &adc));
    adc_oneshot_chan_cfg_t ccfg = { .bitwidth = ADC_BITWIDTH_12, .atten = ADC_ATTEN_DB_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &ccfg));
    while (1) {
        int lo = 4095, hi = 0, raw = 0;
        long sum = 0;
        for (int i = 0; i < 2000; i++) {
            adc_oneshot_read(adc, ADC_CHANNEL_6, &raw);
            if (raw < lo) lo = raw;
            if (raw > hi) hi = raw;
            sum += raw;
        }
        printf("avg %ld min %d max %d swing %d\n", sum / 2000, lo, hi, hi - lo);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

/* ---------- motion ---------- */
static bool motion_now(const mpu6050_sample_t *m)
{
    float g = sqrtf(m->gx * m->gx + m->gy * m->gy + m->gz * m->gz);
    float a = sqrtf(m->ax * m->ax + m->ay * m->ay + m->az * m->az);
    return g > MOVE_GYRO_DPS || fabsf(a - 1.0f) > MOVE_ACCEL_G;
}

static void motion_task(void *arg)
{
    bool verbose = (bool)(intptr_t)arg;
    int64_t last_motion = -1000000000LL;
    int n = 0;
    while (1) {
        mpu6050_sample_t m;
        if (mpu6050_read(&m) == ESP_OK) {
            if (motion_now(&m)) last_motion = esp_timer_get_time();
            bool moving = (esp_timer_get_time() - last_motion) < MOVE_HOLD_MS * 1000LL;
            if (moving != s_moving) printf(">> guitar %s\n", moving ? "MOVING" : "still");
            s_moving = moving;
            if (verbose && n++ % 10 == 0)
                printf("accel %+.2f %+.2f %+.2f g  gyro %+7.1f %+7.1f %+7.1f dps  %.1f C  %s\n",
                       m.ax, m.ay, m.az, m.gx, m.gy, m.gz, m.temp_c, s_moving ? "MOVING" : "still");
        }
        vTaskDelay(pdMS_TO_TICKS(20));      /* 50 Hz */
    }
}

/* ---------- stage 3: rate calibration ---------- */
__attribute__((unused)) static void stage_rate_calibration(void)
{
    audio_init();
    audio_read_frame(s_frame, FRAME_LEN);
    while (1) {
        int64_t t0 = esp_timer_get_time();
        long total = 0;
        for (int f = 0; f < 20; f++) total += audio_read_frame(s_frame, FRAME_LEN);
        double rate = total * 1e6 / (double)(esp_timer_get_time() - t0);
        printf("TRUE_SAMPLE_RATE = %.1f Hz (%+.2f%% vs %d)\n", rate,
               (rate - AUDIO_SAMPLE_RATE_HZ) * 100.0 / AUDIO_SAMPLE_RATE_HZ, AUDIO_SAMPLE_RATE_HZ);
    }
}

/* ---------- stages 4 and 5: tuner ---------- */
static void tuner_task(void *arg)
{
    bool send = (bool)(intptr_t)arg;
    audio_init();
    int64_t last_quiet_send = 0;
    while (1) {
        audio_read_frame(s_frame, FRAME_LEN);
        remove_dc(s_frame, s_signal, FRAME_LEN);
        float rms = frame_rms(s_signal, FRAME_LEN);
        float conf = 0.0f;
        float hz = (rms >= MIN_RMS)
                 ? yin_detect(s_signal, FRAME_LEN, TRUE_SAMPLE_RATE, GUITAR_FMIN, GUITAR_FMAX, YIN_THRESHOLD, &conf)
                 : -1.0f;

        tunebot_packet_t p = { .state = ST_NO_SIGNAL, .ai_string = 255, .moving = s_moving };
        if (hz > 0) {
            int s = nearest_string(hz);
            float c = cents_off(hz, GUITAR_STRINGS[s].freq);
            p.string_index = (uint8_t)s;
            p.hz = hz;
            p.cents = c;
            p.state = (fabsf(c) <= IN_TUNE_CENTS) ? ST_IN_TUNE : (c < 0 ? ST_FLAT : ST_SHARP);
            printf("%7.2f Hz  %-3s %+6.1f cents  %-7s rms %.0f%s\n", hz, GUITAR_STRINGS[s].name, c,
                   p.state == ST_IN_TUNE ? "IN TUNE" : (p.state == ST_FLAT ? "flat" : "sharp"),
                   rms, s_moving ? "  (moving)" : "");
            if (send) espnow_link_send(&p);
        } else if (send && esp_timer_get_time() - last_quiet_send > 1000000) {
            espnow_link_send(&p);                     /* once a second while quiet: link alive + moving flag */
            last_quiet_send = esp_timer_get_time();
        }
    }
}

void app_main(void)
{
    printf("\nTuneBot Guitar node, stage %d\n", NODE_STAGE);
#if NODE_STAGE == 1
    stage_mic_test();
#elif NODE_STAGE == 2
    ESP_ERROR_CHECK(mpu6050_init());
    xTaskCreatePinnedToCore(motion_task, "motion", 4096, (void *)1, 4, NULL, 0);
#elif NODE_STAGE == 3
    stage_rate_calibration();
#elif NODE_STAGE == 4
    xTaskCreatePinnedToCore(tuner_task, "tuner", 8192, (void *)0, 5, NULL, 1);
#else
    espnow_link_init();                               /* radio on core 0 */
    if (mpu6050_init() == ESP_OK)
        xTaskCreatePinnedToCore(motion_task, "motion", 4096, (void *)0, 4, NULL, 0);
    else
        printf("MPU6050 missing: running without motion (moving = 0)\n");
    xTaskCreatePinnedToCore(tuner_task, "tuner", 8192, (void *)1, 5, NULL, 1);   /* DSP on core 1 */
#endif
}
