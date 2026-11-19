// tuning.h - guitar string table and cents maths, Chapter 5
#pragma once
#include <stdint.h>

// >>> Stage B calibration: paste the rate measured in Stage A (TUNEBOT_STAGE 3) here <<<
#define TRUE_SAMPLE_RATE 20000.0f
// >>> Set halfway between silent-room RMS and decaying-string RMS (printed in TUNEBOT_STAGE 4) <<<
#define MIN_RMS          20.0f

#define YIN_THRESHOLD    0.15f
#define GUITAR_FMIN      70.0f
#define GUITAR_FMAX      400.0f
#define NUM_STRINGS      6

typedef struct {
    const char *name;
    float freq;
} guitar_string_t;

extern const guitar_string_t GUITAR_STRINGS[NUM_STRINGS];  // E2 A2 D3 G3 B3 E4

float cents_off(float freq, float ref);                // + sharp / - flat
int   nearest_string(float freq);                      // index into GUITAR_STRINGS
void  remove_dc(const uint16_t *in, float *out, int n);
float frame_rms(const float *x, int n);                // x must be DC-removed
