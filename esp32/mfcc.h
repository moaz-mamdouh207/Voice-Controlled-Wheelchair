#pragma once
#include <stdint.h>

// ── Frame / hop sizes ─────────────────────────────────────────
// Frame: 25 ms at 16 kHz = 400 samples (rounded up to 512 for FFT)
// Hop:   10 ms at 16 kHz = 160 samples
// These must match whatever you used when training your model.
#define MFCC_FRAME_LEN      512     // FFT size (power of 2)
#define MFCC_HOP_LEN        160     // samples between frames
#define MFCC_NUM_FILTERS    26      // mel filterbank bands
#define MFCC_NUM_COEFFS     13      // DCT outputs kept (1..13)

// ── Output matrix dimensions ──────────────────────────────────
// Max frames for a 1-second window: (16000 - 512) / 160 + 1 = 97
#define MFCC_MAX_FRAMES  47

// The output: mfccMatrix[frame][coeff]  (floats, ~5 KB)
extern float mfccMatrix[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS];
extern int   mfccFrameCount;   // actual number of frames computed

// ── API ───────────────────────────────────────────────────────
// Call once at boot — precomputes mel filterbank + DCT table
void mfcc_init();
void mfcc_compute(const int16_t* samples, int numSamples);