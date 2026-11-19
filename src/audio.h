// audio.h - continuous (DMA) ADC capture of the KY-037 mic on GPIO34 (ADC1_CH6)
#pragma once
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_SAMPLE_RATE_HZ 20000   // requested rate; measure the real one in Stage A
#define FRAME_LEN            2048    // samples per frame handed to the DSP

esp_err_t audio_init(void);
// Blocks until n samples (12-bit, 0..4095) have been read. Returns samples read.
int audio_read_frame(uint16_t *out, int n);
