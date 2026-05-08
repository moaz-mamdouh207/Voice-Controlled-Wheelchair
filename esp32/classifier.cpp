#include "classifier.h"
#include "mfcc.h"
#include "model.h"
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

constexpr int kArenaSize = 80 * 1024;
static uint8_t* tensor_arena = nullptr;

static const tflite::Model* tfModel          = nullptr;
static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* inputTensor  = nullptr;
static TfLiteTensor* outputTensor = nullptr;

// FIX (from prev): file-scope buffer so placement-new in classifier_init()
// works correctly on re-entry (e.g. after a soft reset).
static uint8_t interpreter_buf[sizeof(tflite::MicroInterpreter)];
static tflite::AllOpsResolver      resolver;
static tflite::MicroErrorReporter  micro_error_reporter;

// ── Padding value ─────────────────────────────────────────────
// mfcc.cpp zero-fills the matrix with memset before feature extraction,
// so unfilled frames are exactly 0.0f. Training was done on that same
// output, so the correct padding value here is also 0.0f — NOT the
// librosa-derived -70.45 constant used previously.
static const float kPaddingValue = 0.0f;

void classifier_init() {
  tfModel = tflite::GetModel(g_model_data);

  if (tfModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("[CLASSIFIER] ERROR: schema version mismatch");
    return;
  }

  if (tensor_arena == nullptr) {
    tensor_arena = (uint8_t*)malloc(kArenaSize);
  }
  if (tensor_arena == nullptr) {
    Serial.println("[CLASSIFIER] FATAL: Heap allocation failed!");
    return;
  }

  interpreter = new (interpreter_buf) tflite::MicroInterpreter(
    tfModel, resolver, tensor_arena, kArenaSize, &micro_error_reporter
  );

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[CLASSIFIER] ERROR: AllocateTensors failed");
    return;
  }

  inputTensor  = interpreter->input(0);
  outputTensor = interpreter->output(0);

  // Verify tensor shape matches train.py output: (1, MAX_FRAMES, NUM_COEFFS, 1)
  if (inputTensor->dims->size != 4 ||
      inputTensor->dims->data[1] != MFCC_MAX_FRAMES ||
      inputTensor->dims->data[2] != MFCC_NUM_COEFFS ||
      inputTensor->dims->data[3] != 1) {
    Serial.println("[CLASSIFIER] ERROR: input tensor shape mismatch");
    interpreter = nullptr;
    return;
  }

  Serial.println("[CLASSIFIER] Ready.");
}

ClassifierResult classifier_run(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS], int numFrames) {
  ClassifierResult result = { "noise", 0.0f, false };
  if (!interpreter || !inputTensor) return result;

  float   inScale     = inputTensor->params.scale;
  int32_t inZeroPoint = inputTensor->params.zero_point;
  int8_t* inData      = inputTensor->data.int8;

  if (numFrames > MFCC_MAX_FRAMES) {
    Serial.println("[CLASSIFIER] WARN: numFrames > MFCC_MAX_FRAMES, truncating");
    numFrames = MFCC_MAX_FRAMES;
  }

  for (int f = 0; f < MFCC_MAX_FRAMES; f++) {
    for (int c = 0; c < MFCC_NUM_COEFFS; c++) {

      // Padding frames use 0.0f — matching the memset in mfcc_compute().
      float val = (f < numFrames) ? mfcc[f][c] : kPaddingValue;

      int32_t q = (int32_t)roundf(val / inScale) + inZeroPoint;
      if (q < -128) q = -128;
      if (q >  127) q =  127;

      // 4-D index: (1, MAX_FRAMES, NUM_COEFFS, 1) — channel stride is 1
      inData[(f * MFCC_NUM_COEFFS + c) * 1] = (int8_t)q;
    }
  }

  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("[CLASSIFIER] ERROR: Invoke failed");
    return result;
  }

  float   outScale     = outputTensor->params.scale;
  int32_t outZeroPoint = outputTensor->params.zero_point;
  int8_t* outData      = outputTensor->data.int8;

  int   bestIdx   = 0;
  float bestScore = -1.0f;

  for (int i = 0; i < NUM_LABELS; i++) {
    float score = (outData[i] - outZeroPoint) * outScale;
    if (score > bestScore) {
      bestScore = score;
      bestIdx   = i;
    }
  }

  result.label = COMMAND_LABELS[bestIdx];
  result.score = bestScore;

  bool isNoise = (strcmp(result.label, "noise") == 0);
  result.valid = (bestScore >= CLASSIFIER_THRESHOLD) && !isNoise;

  return result;
}