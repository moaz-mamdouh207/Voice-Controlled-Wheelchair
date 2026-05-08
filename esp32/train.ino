#include "mic.h"
#include "vad.h"
#include "mfcc.h"
#include "speaker.h"

// ── Modes ─────────────────────────────────────────────────────
// Send 'm' over serial → record one MFCC clip and stream it as CSV
// Send 'e' over serial → enroll speaker (SPEAKER_ENROLL_SAMPLES clips)
// Send 'x' over serial → erase stored speaker reference from NVS

void setup() {
  Serial.begin(115200);
  mic_init();
  vad_reset();
  mfcc_init();
  speaker_init();
  Serial.println("[BOOT] Ready.");
  Serial.println("  m = record MFCC clip");
  Serial.printf( "  e = enroll speaker (%d clips)\n", SPEAKER_ENROLL_SAMPLES);
  Serial.println("  x = erase speaker reference");
}

// ── Shared: capture one VAD-triggered MFCC window ─────────────
static bool capture_window(unsigned long timeout_ms) {
  vad_reset();
  unsigned long start = millis();
  while (millis() - start < timeout_ms) {
    int16_t sample;
    mic_read_sample(&sample);
    if (vad_update(sample) == VAD_COMPLETE) {
      mfcc_compute(audioWindow, audioWindowLen);
      return true;
    }
  }
  return false;
}

// ── 'm' handler ───────────────────────────────────────────────
static void handle_record() {
  Serial.println("READY");
  if (!capture_window(6000)) {
    Serial.println("TIMEOUT");
    return;
  }
  Serial.println("START");
  for (int f = 0; f < MFCC_MAX_FRAMES; f++) {
    for (int c = 0; c < MFCC_NUM_COEFFS; c++) {
      Serial.print(mfccMatrix[f][c], 6);
      if (c < MFCC_NUM_COEFFS - 1) Serial.print(",");
    }
    Serial.println();
  }
  Serial.println("END");
}

// ── 'e' handler ───────────────────────────────────────────────
static void handle_enroll() {
  Serial.printf("[ENROLL] Starting — say the same phrase %d times.\n",
                SPEAKER_ENROLL_SAMPLES);
  speaker_erase();   // reset accumulator and NVS

  for (int i = 0; i < SPEAKER_ENROLL_SAMPLES; i++) {
    Serial.printf("[ENROLL] Clip %d/%d — speak now...\n",
                  i + 1, SPEAKER_ENROLL_SAMPLES);
    if (!capture_window(6000)) {
      Serial.println("[ENROLL] TIMEOUT — aborting.");
      return;
    }
    if (speaker_enroll(mfccMatrix)) {
      Serial.println("[ENROLL] Done. Speaker reference saved.");
      return;
    }
    delay(500);
  }
}

void loop() {
  if (!Serial.available()) return;
  char cmd = Serial.read();
  switch (cmd) {
    case 'm': handle_record(); break;
    case 'e': handle_enroll(); break;
    case 'x':
      speaker_erase();
      Serial.println("[ENROLL] Erased. Send 'e' to re-enroll.");
      break;
    default: break;
  }
}
