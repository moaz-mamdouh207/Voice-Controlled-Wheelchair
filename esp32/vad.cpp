#include "vad.h"
#include "mic.h"   // for audioWindow, audioWindowLen
#include <math.h>
#include "Arduino.h"
// ── Internal state ─────────────────────────────────────────────
static vad_state_t  state          = VAD_IDLE;
static int          onset_count    = 0;   // consecutive loud frames seen
static int          silence_count  = 0;   // consecutive quiet frames seen
static int          frame_pos      = 0;   // sample position within current frame
static int32_t      frame_energy   = 0;   // accumulator for current frame RMS

// Frame size matches DMA buffer length for efficiency
#define FRAME_SAMPLES  DMA_BUF_LEN        // 128 samples ≈ 8 ms at 16 kHz

// ─────────────────────────────────────────────────────────────
static bool is_loud(int32_t energy_sum, int n) {
  // Compute RMS over the frame and compare to threshold.
  // energy_sum = Σ(sample²), n = number of samples in frame.
  float rms = sqrtf((float)energy_sum / n);
  return rms > VAD_THRESHOLD;
}

// ─────────────────────────────────────────────────────────────
void vad_reset() {
  state          = VAD_IDLE;
  onset_count    = 0;
  silence_count  = 0;
  frame_pos      = 0;
  frame_energy   = 0;
  audioWindowLen = 0;
}

vad_state_t vad_get_state() {
  return state;
}

// ─────────────────────────────────────────────────────────────
// Call this for every sample coming from mic_read_sample().
// It accumulates samples into frames, measures each frame's energy,
// and drives the state machine.
vad_state_t vad_update(int16_t sample) {

  if (state == VAD_COMPLETE) return state;  // wait until caller calls vad_reset()

  // Accumulate energy for this frame
  frame_energy += (int32_t)sample * sample;
  frame_pos++;

  // ── Store sample into the window if we are capturing ──────
  if (state == VAD_ACTIVE) {
    if (audioWindowLen < AUDIO_WINDOW_SAMPLES) {
      audioWindow[audioWindowLen++] = sample;
    } else {
      // Window full before silence detected — treat as complete
      state = VAD_COMPLETE;
      return state;
    }
  }

  // ── Process frame boundary ─────────────────────────────────
  if (frame_pos >= FRAME_SAMPLES) {
    bool loud = is_loud(frame_energy, FRAME_SAMPLES);
    frame_energy = 0;
    frame_pos    = 0;

    switch (state) {

      case VAD_IDLE:
        if (loud) {
          onset_count++;
          if (onset_count >= VAD_ONSET_FRAMES) {
            state          = VAD_ACTIVE;
            onset_count    = 0;
            silence_count  = 0;
            audioWindowLen = 0;
          }
        } 
        else {
          onset_count = 0;  // reset — must be consecutive
        }
        break;

      case VAD_ACTIVE:
        if (!loud) {
          silence_count++;
          if (silence_count >= VAD_SILENCE_FRAMES) {
            // Confirmed speech end
            state = VAD_COMPLETE;
          }
        } 
        else {
          silence_count = 0;  // reset on any loud frame
        }
        break;

      default:
        break;
    }
  }

  return state;
}