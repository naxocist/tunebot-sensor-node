// tuning.c
#include "tuning.h"
#include <math.h>

const guitar_string_t GUITAR_STRINGS[NUM_STRINGS] = {
    {"E2", 82.41f}, {"A2", 110.00f}, {"D3", 146.83f},
    {"G3", 196.00f}, {"B3", 246.94f}, {"E4", 329.63f},
};

float cents_off(float freq, float ref)
{
    return 1200.0f * log2f(freq / ref);
}

int nearest_string(float freq)
{
    int best = 0;
    float best_abs = 1e9f;
    for (int i = 0; i < NUM_STRINGS; i++) {
        float c = fabsf(cents_off(freq, GUITAR_STRINGS[i].freq));
        if (c < best_abs) { best_abs = c; best = i; }
    }
    return best;
}

void remove_dc(const uint16_t *in, float *out, int n)
{
    float mean = 0.0f;
    for (int i = 0; i < n; i++) mean += in[i];
    mean /= n;
    for (int i = 0; i < n; i++) out[i] = (float)in[i] - mean;
}

float frame_rms(const float *x, int n)
{
    float acc = 0.0f;
    for (int i = 0; i < n; i++) acc += x[i] * x[i];
    return sqrtf(acc / n);
}
