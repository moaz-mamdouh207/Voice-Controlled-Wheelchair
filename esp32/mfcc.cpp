#include "mfcc.h"
#include <math.h>
#include <string.h>
#include "esp_dsp.h"
#include <esp_attr.h>

// ── Output storage ────────────────────────────────────────────
float mfccMatrix[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS];
int   mfccFrameCount = 0;

// ── Precomputed tables (built once in mfcc_init) ──────────────
static float hannWindow[MFCC_FRAME_LEN];

typedef struct {
  int startBin;
  int peakBin;
  int endBin;
} MelBand;
static MelBand melBands[MFCC_NUM_FILTERS];

static float dctMatrix[MFCC_NUM_COEFFS][MFCC_NUM_FILTERS];

void mfcc_init() {
    dsps_fft2r_init_fc32(NULL, MFCC_FRAME_LEN);
    dsps_wind_hann_f32(hannWindow, MFCC_FRAME_LEN);

    float melLow  = 2595.0f * log10f(1.0f + 20.0f   / 700.0f);
    float melHigh = 2595.0f * log10f(1.0f + 8000.0f  / 700.0f);
    float melStep = (melHigh - melLow) / (MFCC_NUM_FILTERS + 1);

    int binCount = MFCC_FRAME_LEN / 2 + 1;
    int melPoints[MFCC_NUM_FILTERS + 2];

    for (int i = 0; i < MFCC_NUM_FILTERS + 2; i++) {
        float hz = 700.0f * (powf(10.0f, (melLow + i * melStep) / 2595.0f) - 1.0f);
        melPoints[i] = (int)(hz / (16000.0f / MFCC_FRAME_LEN) + 0.5f);
        if (melPoints[i] >= binCount) melPoints[i] = binCount - 1;
    }

    for (int b = 0; b < MFCC_NUM_FILTERS; b++) {
        melBands[b].startBin = melPoints[b];
        melBands[b].peakBin  = melPoints[b + 1];
        melBands[b].endBin   = melPoints[b + 2];
    }

    for (int c = 0; c < MFCC_NUM_COEFFS; c++) {
        float norm = (c == 0) ? sqrtf(1.0f / MFCC_NUM_FILTERS)
                               : sqrtf(2.0f / MFCC_NUM_FILTERS);
        for (int b = 0; b < MFCC_NUM_FILTERS; b++) {
            dctMatrix[c][b] = norm * cosf((float)M_PI * c * (b + 0.5f) / MFCC_NUM_FILTERS);
        }
    }
}

void mfcc_compute(const int16_t* samples, int numSamples) {
    mfccFrameCount = 0;
    int binCount = MFCC_FRAME_LEN / 2 + 1;

    // Zero the entire matrix — padding frames must be 0.0f to match
    // training data, where short clips are zero-padded before feature
    // extraction (memset of float is valid since 0x00000000 == 0.0f).
    memset(mfccMatrix, 0, sizeof(mfccMatrix));

    static DRAM_ATTR float fft_input[MFCC_FRAME_LEN * 2] __attribute__((aligned(16)));
    static float powerSpec[MFCC_FRAME_LEN / 2 + 1];

    int frameStart = 0;
    while (frameStart + MFCC_FRAME_LEN <= numSamples && mfccFrameCount < MFCC_MAX_FRAMES) {

        // 1. Windowing
        for (int i = 0; i < MFCC_FRAME_LEN; i++) {
            fft_input[i * 2]     = (float)samples[frameStart + i] * hannWindow[i];
            fft_input[i * 2 + 1] = 0.0f;
        }

        // 2. FFT
        dsps_fft2r_fc32(fft_input, MFCC_FRAME_LEN);
        dsps_bit_rev_fc32(fft_input, MFCC_FRAME_LEN);

        // 3. Power spectrum
        for (int i = 0; i < binCount; i++) {
            powerSpec[i] = (fft_input[i * 2]     * fft_input[i * 2]) +
                           (fft_input[i * 2 + 1] * fft_input[i * 2 + 1]);
        }

        // 4. Mel filterbank
        float melEnergy[MFCC_NUM_FILTERS];
        for (int b = 0; b < MFCC_NUM_FILTERS; b++) {
            float energy = 0.0f;
            int s = melBands[b].startBin;
            int p = melBands[b].peakBin;
            int e = melBands[b].endBin;

            // Ascending slope: bins [s, p)
            if (p > s) {
                float slope = 1.0f / (float)(p - s);
                for (int k = s; k < p && k < binCount; k++)
                    energy += powerSpec[k] * ((k - s) * slope);
            }

            // FIX: Peak bin (weight = 1.0) was skipped by both loops.
            // The ascending ramp uses k < p (excludes p) and the
            // descending uses k starting at p+1 effectively, so bin p
            // was never accumulated. Add it explicitly.
            if (p < binCount)
                energy += powerSpec[p];

            // Descending slope: bins (p, e)
            if (e > p) {
                float slope = 1.0f / (float)(e - p);
                for (int k = p + 1; k < e && k < binCount; k++)
                    energy += powerSpec[k] * ((e - k) * slope);
            }

            melEnergy[b] = logf(energy + 1e-6f);
        }

        // 5. DCT
        for (int c = 0; c < MFCC_NUM_COEFFS; c++) {
            float val = 0.0f;
            for (int b = 0; b < MFCC_NUM_FILTERS; b++)
                val += dctMatrix[c][b] * melEnergy[b];
            mfccMatrix[mfccFrameCount][c] = val;
        }

        frameStart += MFCC_HOP_LEN;
        mfccFrameCount++;
    }

    // Report full frame count — zero rows already in place from memset above.
    mfccFrameCount = MFCC_MAX_FRAMES;
}