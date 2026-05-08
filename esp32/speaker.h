#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "mfcc.h"

// ── Enrollment config ─────────────────────────────────────────
// How many utterances to average together for the reference vector.
// More = more stable reference, but slower enrollment.
#define SPEAKER_ENROLL_SAMPLES   100

// Cosine similarity threshold [0.0, 1.0].
// 1.0 = identical, 0.0 = completely different.
// Start at 0.82 and tune down if you get too many false rejects,
// or up if other people can trigger commands.
#define SPEAKER_THRESHOLD        0.1f

// NVS key for persisting the reference vector across reboots
#define SPEAKER_NVS_NAMESPACE    "speaker"
#define SPEAKER_NVS_KEY          "ref_vec"

// ── Reference vector ──────────────────────────────────────────
// The reference is the mean MFCC vector across enrollment utterances.
// Flattening (MAX_FRAMES * NUM_COEFFS) into 1-D before averaging
// preserves temporal structure while keeping the comparison simple.
#define SPEAKER_VEC_LEN  (MFCC_MAX_FRAMES * MFCC_NUM_COEFFS)

// ── API ───────────────────────────────────────────────────────
void speaker_init();          // load reference from NVS if available

// Enroll: call once per utterance, SPEAKER_ENROLL_SAMPLES times.
// Pass the mfccMatrix and mfccFrameCount after each VAD_COMPLETE.
// Returns true when enrollment is complete (all samples collected).
bool speaker_enroll(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS]);

// Verify: returns true if the current MFCC matches the enrolled speaker.
// Call after classifier confirms a valid command.
bool speaker_verify(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS]);

// Returns true if a reference vector is loaded and ready
bool speaker_is_enrolled();

// Erase stored reference from NVS (call to re-enroll)
void speaker_erase();