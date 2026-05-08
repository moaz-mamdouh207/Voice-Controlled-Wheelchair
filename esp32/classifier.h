#ifndef CLASSIFIER_H
#define CLASSIFIER_H
#define CLASSIFIER_THRESHOLD  0.10f
#include <Arduino.h>
#include "mfcc.h" // Must define MFCC_MAX_FRAMES and MFCC_NUM_COEFFS

struct ClassifierResult {
    const char* label;
    float score;
    bool valid;
};

void classifier_init();

// Use explicit dimensions to match the implementation
ClassifierResult classifier_run(const float mfcc[MFCC_MAX_FRAMES][MFCC_NUM_COEFFS], int numFrames);

#endif