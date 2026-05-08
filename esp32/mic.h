#pragma once
#include <driver/i2s.h>

// ── Pin config ────────────────────────────────────────────────
#define MIC_WS_PIN    15
#define MIC_SCK_PIN   14
#define MIC_SD_PIN    32
#define MIC_I2S_PORT  I2S_NUM_0

// ── Audio config ──────────────────────────────────────────────
#define SAMPLE_RATE       16000   // Hz  — must match your model's training rate
#define DMA_BUF_COUNT     8
#define DMA_BUF_LEN       128     // samples per DMA buffer

// ── Buffer for one audio window ───────────────────────────────
// half second of audio at 16 kHz = 8000 samples
#define AUDIO_WINDOW_SAMPLES  8000
extern int16_t audioWindow[AUDIO_WINDOW_SAMPLES];
extern volatile int audioWindowLen;   // how many samples are currently filled

void mic_init();
void mic_read_sample(int16_t* out_sample);