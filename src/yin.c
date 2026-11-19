// yin.c - de Cheveigne & Kawahara YIN with parabolic interpolation
#include "yin.h"

#define YIN_MAX_TAU 1024
static float d[YIN_MAX_TAU + 2];

float yin_detect(const float *x, int n, float fs, float fmin, float fmax,
                 float threshold, float *confidence)
{
    int w = n / 2;
    int tau_min = (int)(fs / fmax);
    int tau_max = (int)(fs / fmin) + 1;
    if (tau_min < 2) tau_min = 2;
    if (tau_max > w - 1) tau_max = w - 1;
    if (tau_max > YIN_MAX_TAU) tau_max = YIN_MAX_TAU;
    if (confidence) *confidence = 0.0f;
    if (tau_max <= tau_min + 1) return -1.0f;

    // 1) difference function, 2) cumulative mean normalised difference
    d[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= tau_max + 1; tau++) {
        float sum = 0.0f;
        for (int j = 0; j < w; j++) {
            float delta = x[j] - x[j + tau];
            sum += delta * delta;
        }
        running += sum;
        d[tau] = (running > 0.0f) ? sum * tau / running : 1.0f;
    }

    // 3) absolute threshold: first dip below threshold, then walk to its local minimum
    int best = -1;
    for (int tau = tau_min; tau <= tau_max; tau++) {
        if (d[tau] < threshold) {
            while (tau + 1 <= tau_max && d[tau + 1] < d[tau]) tau++;
            best = tau;
            break;
        }
    }
    if (best < 0) return -1.0f;

    // 4) parabolic interpolation around the minimum
    float t = (float)best;
    float s0 = d[best - 1], s1 = d[best], s2 = d[best + 1];
    float denom = s0 + s2 - 2.0f * s1;
    if (denom != 0.0f) t += 0.5f * (s0 - s2) / denom;

    if (confidence) *confidence = 1.0f - s1;
    return fs / t;
}
