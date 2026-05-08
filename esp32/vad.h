#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── Tuning knobs (adjust for your environment) ────────────────
// RMS amplitude that counts as "speech" vs background noise.
// In a quiet room: 300–600. Noisy environment: raise to 1000–2000.
#define VAD_THRESHOLD         300

// How many consecutive "loud" frames before we decide speech started.
// At 16 kHz with 128-sample frames → 1 frame ≈ 8 ms.
// 5 frames = ~40 ms of sustained sound needed to trigger.
#define VAD_ONSET_FRAMES      5

// How many consecutive "quiet" frames before we decide speech ended.
// 30 frames = ~240 ms of silence → speech window closes.
#define VAD_SILENCE_FRAMES    30

// ── States ────────────────────────────────────────────────────
typedef enum {
  VAD_IDLE,       // listening for speech to start
  VAD_ACTIVE,     // capturing speech
  VAD_COMPLETE    // speech ended — window is ready to process
} vad_state_t;

vad_state_t vad_update(int16_t sample);   // call once per sample
void        vad_reset();                   // call after window is consumed
vad_state_t vad_get_state();