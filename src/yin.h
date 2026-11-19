// yin.h - YIN fundamental-frequency estimator (pure C), Chapter 4
#pragma once

// Returns detected frequency in Hz, or -1 if no confident pitch.
// x: DC-removed samples, n: sample count (uses window n/2),
// fs: true sample rate, fmin/fmax: search range, threshold: typ. 0.10-0.15.
// *confidence (optional) receives 1 - d'(tau).
float yin_detect(const float *x, int n, float fs, float fmin, float fmax,
                 float threshold, float *confidence);
