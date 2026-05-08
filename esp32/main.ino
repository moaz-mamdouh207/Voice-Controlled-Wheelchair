#include "mic.h"
#include "vad.h"
#include "mfcc.h"
#include "classifier.h"

// ── Output pins ───────────────────────────────────────────────
#define PIN_FORWARD  25
#define PIN_BACKWARD 26
#define PIN_LEFT     13
#define PIN_RIGHT    27


#define TXD2 17
#define RXD2 16

// ── System state ──────────────────────────────────────────────
static bool  armed = false;

// ── Speed state ───────────────────────────────────────────────
#define SPEED_MIN  1
#define SPEED_MAX  5
#define SPEED_DEF  3
static int speedLevel = SPEED_DEF;

void clearLeds() {
  digitalWrite(PIN_FORWARD,  LOW);
  digitalWrite(PIN_BACKWARD, LOW);
  digitalWrite(PIN_LEFT,     LOW);
  digitalWrite(PIN_RIGHT,    LOW);
}

void arm() {
  armed      = true;
}

void disarm() {
  armed = false;
  clearLeds();
}

void setup() {
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(PIN_FORWARD,  OUTPUT);
  pinMode(PIN_BACKWARD, OUTPUT);
  pinMode(PIN_LEFT,     OUTPUT);
  pinMode(PIN_RIGHT,    OUTPUT);
  clearLeds();

  mic_init();
  vad_reset();
  mfcc_init();
  classifier_init();
}

void loop() {

  int16_t sample;
  mic_read_sample(&sample);

  if (vad_update(sample) != VAD_COMPLETE) return;

  // ── Speech captured ───────────────────────────────────────────
  mfcc_compute(audioWindow, audioWindowLen);
  ClassifierResult res = classifier_run(mfccMatrix, mfccFrameCount);

  if (!res.valid) {
    vad_reset();
    return;
  }

  if (strcmp(res.label, "stop") == 0) {
    clearLeds();
    Serial2.print("s");

  } else if (strcmp(res.label, "forward") == 0) {
    clearLeds();
    digitalWrite(PIN_FORWARD, HIGH);
    Serial2.print("f");

  } else if (strcmp(res.label, "backward") == 0) {
    clearLeds();
    digitalWrite(PIN_BACKWARD, HIGH);
    Serial2.print("b");

  } else if (strcmp(res.label, "left") == 0) {
    clearLeds();
    digitalWrite(PIN_LEFT, HIGH);
    Serial2.print("l");

  } else if (strcmp(res.label, "right") == 0) {
    clearLeds();
    digitalWrite(PIN_RIGHT, HIGH);
    Serial2.print("r");

  }

  vad_reset();
}