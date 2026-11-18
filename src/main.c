#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define MIC_CHANNEL ADC_CHANNEL_6 // ADC1_CH6 = GPIO34
#define N_SAMPLES 2000

void app_main(void)
{
  adc_oneshot_unit_handle_t adc;
  adc_oneshot_unit_init_cfg_t unit_cfg = {.unit_id = ADC_UNIT_1};
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

  adc_oneshot_chan_cfg_t chan_cfg = {
      .bitwidth = ADC_BITWIDTH_DEFAULT, /* 12 bits: 0..4095 */
      .atten = ADC_ATTEN_DB_12,         /* ~0..3.1 V usable range */
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, MIC_CHANNEL, &chan_cfg));

  while (1)
  {
    int lo = 4095, hi = 0, raw = 0;
    long sum = 0;
    for (int i = 0; i < N_SAMPLES; i++)
    {
      ESP_ERROR_CHECK(adc_oneshot_read(adc, MIC_CHANNEL, &raw));
      if (raw < lo)
        lo = raw;
      if (raw > hi)
        hi = raw;
      sum += raw;
    }
    printf("avg %ld min %d max %d swing %d\n", sum / N_SAMPLES, lo, hi, hi - lo);
    vTaskDelay(pdMS_TO_TICKS(300));
  }
}
