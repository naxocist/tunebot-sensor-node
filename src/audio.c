// audio.c - ADC1 continuous mode (I2S0 DMA on classic ESP32), Chapters 2-3
#include "audio.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"

#define MIC_CHANNEL      ADC_CHANNEL_6   // GPIO34
#define BYTES_PER_SAMPLE SOC_ADC_DIGI_RESULT_BYTES

static const char *TAG = "audio";
static adc_continuous_handle_t s_handle;
static uint8_t s_raw[FRAME_LEN * BYTES_PER_SAMPLE];

esp_err_t audio_init(void)
{
    adc_continuous_handle_cfg_t hcfg = {
        .max_store_buf_size = sizeof(s_raw) * 4,
        .conv_frame_size    = sizeof(s_raw),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&hcfg, &s_handle));

    adc_digi_pattern_config_t pattern = {
        .atten     = ADC_ATTEN_DB_12,           // ESP-IDF >= 5.2 (use ADC_ATTEN_DB_11 on older)
        .channel   = MIC_CHANNEL,
        .unit      = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };
    adc_continuous_config_t cfg = {
        .sample_freq_hz = AUDIO_SAMPLE_RATE_HZ,
        .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
        .format         = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num    = 1,
        .adc_pattern    = &pattern,
    };
    ESP_ERROR_CHECK(adc_continuous_config(s_handle, &cfg));
    ESP_ERROR_CHECK(adc_continuous_start(s_handle));
    ESP_LOGI(TAG, "ADC1 continuous started: GPIO34, %d Hz, frame %d", AUDIO_SAMPLE_RATE_HZ, FRAME_LEN);
    return ESP_OK;
}

int audio_read_frame(uint16_t *out, int n)
{
    int got = 0;
    while (got < n) {
        uint32_t want = (uint32_t)(n - got) * BYTES_PER_SAMPLE;
        if (want > sizeof(s_raw)) want = sizeof(s_raw);
        uint32_t len = 0;
        esp_err_t err = adc_continuous_read(s_handle, s_raw, want, &len, 1000);
        if (err == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "ADC read timeout");
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
            return got;
        }
        for (uint32_t i = 0; i + BYTES_PER_SAMPLE <= len && got < n; i += BYTES_PER_SAMPLE) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&s_raw[i];
            if (p->type1.channel == MIC_CHANNEL) {
                out[got++] = p->type1.data;
            }
        }
    }
    return got;
}
