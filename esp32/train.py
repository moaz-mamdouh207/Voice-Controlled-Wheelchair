"""
train.py — trains directly on ESP32-computed MFCCs
No MFCC reimplementation, no mismatch possible.
"""

import os
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split

MAX_FRAMES = 47
NUM_COEFFS = 13
DATA_DIR   = "mfcc_data"
HEADER_OUT = "model.h"

LABELS = ["backward", "left", "right", "stop", "forward", "faster", "slower", "noise", "wake", "sleep"]

def load_dataset():
    X, y = [], []
    for label_idx, label in enumerate(LABELS):
        folder = os.path.join(DATA_DIR, label)
        if not os.path.isdir(folder):
            print(f"  WARNING: missing {folder}")
            continue
        files = [f for f in os.listdir(folder) if f.endswith(".csv")]
        print(f"  {label}: {len(files)} clips")
        for fname in files:
            try:
                df = pd.read_csv(os.path.join(folder, fname))
                matrix = df.values.astype(np.float32)  # (MAX_FRAMES, NUM_COEFFS)
                X.append(matrix)
                y.append(label_idx)
            except Exception as e:
                print(f"    skip {fname}: {e}")

    X = np.array(X)[..., np.newaxis]  # (N, MAX_FRAMES, NUM_COEFFS, 1)
    y = np.array(y)
    return X, y


def build_model(input_shape, num_classes):
    model = keras.Sequential([
        keras.layers.Input(shape=input_shape),

        keras.layers.Conv2D(16, (3,3), padding="same", activation="relu"),
        keras.layers.BatchNormalization(),
        keras.layers.MaxPooling2D((2,2)),
        keras.layers.Dropout(0.2),

        keras.layers.DepthwiseConv2D((3,3), padding="same", activation="relu"),
        keras.layers.Conv2D(32, (1,1), activation="relu"),
        keras.layers.BatchNormalization(),
        keras.layers.MaxPooling2D((2,2)),
        keras.layers.Dropout(0.2),

        keras.layers.DepthwiseConv2D((3,3), padding="same", activation="relu"),
        keras.layers.Conv2D(64, (1,1), activation="relu"),
        keras.layers.BatchNormalization(),
        keras.layers.GlobalAveragePooling2D(),
        keras.layers.Dropout(0.3),

        keras.layers.Dense(64, activation="relu"),
        keras.layers.Dense(num_classes, activation="softmax"),
    ])
    return model


def export_tflite(model, X_train):
    def representative_dataset():
        for i in range(min(100, len(X_train))):
            yield [X_train[i:i+1]]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type  = tf.int8
    converter.inference_output_type = tf.int8
    return converter.convert()


def write_model_header(tflite_bytes, labels):
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        "const char* const COMMAND_LABELS[] = {",
        "  " + ", ".join(f'"{l}"' for l in labels),
        "};",
        f"const int NUM_LABELS = {len(labels)};",
        "",
        "const uint8_t g_model_data[] = {",
    ]
    for i in range(0, len(tflite_bytes), 12):
        chunk = tflite_bytes[i:i+12]
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines += ["};", f"const int g_model_data_len = {len(tflite_bytes)};"]
    with open(HEADER_OUT, "w") as f:
        f.write("\n".join(lines))
    print(f"Saved {HEADER_OUT}")


if __name__ == "__main__":
    print("Loading dataset...")
    X, y = load_dataset()
    print(f"Total: {len(X)} samples, shape: {X.shape}")

    X_train, X_val, y_train, y_val = train_test_split(
        X, y, test_size=0.2, random_state=42, stratify=y
    )

    model = build_model(X.shape[1:], len(LABELS))
    model.summary()
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"]
    )

    callbacks = [
        keras.callbacks.EarlyStopping(patience=8, restore_best_weights=True),
        keras.callbacks.ReduceLROnPlateau(factor=0.5, patience=4, verbose=1),
    ]
    model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=60,
        batch_size=32,
        callbacks=callbacks,
        verbose=1
    )

    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"\nValidation accuracy: {val_acc*100:.1f}%")

    tflite_bytes = export_tflite(model, X_train)
    print(f"Model size: {len(tflite_bytes)/1024:.1f} KB")
    write_model_header(tflite_bytes, LABELS)
    print("Done. Copy model.h into your Arduino sketch folder.")