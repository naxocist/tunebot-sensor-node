#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_continuous.h"
#include "soc/soc_caps.h"
#define MIC_CHANNEL ADC_CHANNEL_6                  /* GPIO34 on the DOIT board */
#define SAMPLE_RATE 20000                          /* classic ESP32: 20 kHz is the minimum */
#define FRAME_LEN 2048                             /* samples per frame */
#define BYTES_PER_SAMPLE SOC_ADC_DIGI_RESULT_BYTES /* 2 on classic ESP32 */
static adc_continuous_handle_t s_adc;
void audio_init(void)
{
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 8192,                /* driver's ring
                       buffer, bytes */
        .conv_frame_size = 512 * BYTES_PER_SAMPLE, /* DMA block size,
        bytes */
    };
    adc_continuous_new_handle(&handle_cfg, &s_adc);
    static adc_digi_pattern_config_t pattern[1] = {{
        .atten = ADC_ATTEN_DB_12,
        .channel = MIC_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH, /* 12 */
    }};
    adc_continuous_config_t cfg = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1, /* classic ESP32
        format */
        .pattern_num = 1,
        .adc_pattern = pattern,
    };
    adc_continuous_config(s_adc, &cfg);
    adc_continuous_start(s_adc);
}
/* Fills out[0..FRAME_LEN-1] with raw readings (0..4095).
Blocks until a full frame has arrived. Returns the number of samples. */
int audio_read_frame(float *out)
{
    static uint8_t raw[512 * BYTES_PER_SAMPLE];
    int got = 0;
    while (got < FRAME_LEN)
    {
        uint32_t bytes = 0;
        if (adc_continuous_read(s_adc, raw, sizeof(raw), &bytes, 1000) !=
            ESP_OK)
            continue; /* timeout: try again
                       */
        for (uint32_t i = 0; i + BYTES_PER_SAMPLE <= bytes && got <
                                                                  FRAME_LEN;
             i += BYTES_PER_SAMPLE)
        {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&raw[i];
            if (p->type1.channel == MIC_CHANNEL)
                out[got++] = (float)p->type1.data;
        }
    }
    return got;
}