#include "speaker.h"
#include <math.h>
#include <string.h>
#include <Preferences.h>   // ESP32 NVS wrapper (part of Arduino ESP32 core)
#include "Arduino.h"

// ── Stored reference ──────────────────────────────────────────
static float  refVec[SPEAKER_VEC_LEN];   // mean MFCC vector from enrollment
static bool   enrolled = false;

// ── Enrollment accumulator ────────────────────────────────────
static float  accumVec[SPEAKER_VEC_LEN]; // running sum across enrollment clips
static int    enrollCount = 0;           // clips collected so far

static Preferences prefs;

// ── Helpers ───────────────────────────────────────────────────

// Flatten mfccMatrix into a 1-D vector (row-major, same layout as memory)
static void flatten(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS],
                    float* out) {
  memcpy(out, mfcc, SPEAKER_VEC_LEN * sizeof(float));
}

// Cosine similarity between two vectors of length SPEAKER_VEC_LEN.
// Returns value in [-1, 1]; higher = more similar.
static float cosine_similarity(const float* a, const float* b) {
  float dot = 0.0f, normA = 0.0f, normB = 0.0f;
  for (int i = 0; i < SPEAKER_VEC_LEN; i++) {
    dot   += a[i] * b[i];
    normA += a[i] * a[i];
    normB += b[i] * b[i];
  }
  float denom = sqrtf(normA) * sqrtf(normB);
  if (denom < 1e-8f) return 0.0f;   // avoid divide-by-zero on silent input
  return dot / denom;
}

// ── Public API ────────────────────────────────────────────────

void speaker_init() {
  prefs.begin(SPEAKER_NVS_NAMESPACE, /*readOnly=*/true);
  size_t len = prefs.getBytesLength(SPEAKER_NVS_KEY);

  if (len == SPEAKER_VEC_LEN * sizeof(float)) {
    prefs.getBytes(SPEAKER_NVS_KEY, refVec, len);
    enrolled = true;
    Serial.println("[SPEAKER] Reference loaded from NVS.");
  } else {
    Serial.println("[SPEAKER] No reference found — send 'e' to enroll.");
  }
  prefs.end();
}

bool speaker_is_enrolled() {
  return enrolled;
}

void speaker_erase() {
  prefs.begin(SPEAKER_NVS_NAMESPACE, /*readOnly=*/false);
  prefs.remove(SPEAKER_NVS_KEY);
  prefs.end();
  enrolled     = false;
  enrollCount  = 0;
  memset(accumVec, 0, sizeof(accumVec));
  memset(refVec,   0, sizeof(refVec));
  Serial.println("[SPEAKER] Reference erased.");
}

// Returns true when all SPEAKER_ENROLL_SAMPLES clips have been collected
// and the reference vector has been saved to NVS.
bool speaker_enroll(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS]) {
  float flat[SPEAKER_VEC_LEN];
  flatten(mfcc, flat);

  // Accumulate into running sum
  for (int i = 0; i < SPEAKER_VEC_LEN; i++) {
    accumVec[i] += flat[i];
  }
  enrollCount++;

  Serial.printf("[SPEAKER] Enrolled %d / %d\n", enrollCount, SPEAKER_ENROLL_SAMPLES);

  if (enrollCount < SPEAKER_ENROLL_SAMPLES) {
    return false;   // not done yet
  }

  // Compute mean
  for (int i = 0; i < SPEAKER_VEC_LEN; i++) {
    refVec[i] = accumVec[i] / (float)SPEAKER_ENROLL_SAMPLES;
  }

  // Persist to NVS
  prefs.begin(SPEAKER_NVS_NAMESPACE, /*readOnly=*/false);
  prefs.putBytes(SPEAKER_NVS_KEY, refVec, SPEAKER_VEC_LEN * sizeof(float));
  prefs.end();

  enrolled    = true;
  enrollCount = 0;
  memset(accumVec, 0, sizeof(accumVec));

  Serial.println("[SPEAKER] Enrollment complete. Reference saved to NVS.");
  return true;
}

bool speaker_verify(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS]) {
  if (!enrolled) {
    // No reference — fail open or closed depending on your preference.
    // Failing OPEN (return true) means the system works without enrollment
    // but has no speaker verification. Change to false to require enrollment.
    return true;
  }

  float flat[SPEAKER_VEC_LEN];
  flatten(mfcc, flat);

  float sim = cosine_similarity(flat, refVec);
  Serial.printf("[SPEAKER] Similarity: %.4f (threshold: %.2f)\n",
                sim, SPEAKER_THRESHOLD);

  return sim >= SPEAKER_THRESHOLD;
}