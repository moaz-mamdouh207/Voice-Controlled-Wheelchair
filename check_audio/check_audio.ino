#include <Arduino.h>
#include <driver/i2s.h>

// ── Pin config ─────────────────────────────
#define MIC_WS_PIN    15
#define MIC_SCK_PIN   14
#define MIC_SD_PIN    32
#define MIC_I2S_PORT  I2S_NUM_0

// ── Audio config ───────────────────────────
#define SAMPLE_RATE   16000
#define MAX_SECONDS 2
#define MAX_SAMPLES   (SAMPLE_RATE * MAX_SECONDS)

// Buffer
int16_t audioBuffer[MAX_SAMPLES];
int sampleIndex = 0;

// ── I2S INIT ───────────────────────────────
void mic_init() {
  pinMode(MIC_SCK_PIN, OUTPUT);
  pinMode(MIC_WS_PIN, OUTPUT);
  digitalWrite(MIC_SCK_PIN, 0);
  digitalWrite(MIC_WS_PIN, 0);
  delay(1000);

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 128,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = MIC_SCK_PIN,
    .ws_io_num = MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD_PIN
  };

  i2s_driver_install(MIC_I2S_PORT, &config, 0, NULL);
  i2s_set_pin(MIC_I2S_PORT, &pins);
  i2s_zero_dma_buffer(MIC_I2S_PORT);
  i2s_start(MIC_I2S_PORT);
}

// ── READ SAMPLE ────────────────────────────
int16_t read_sample() {
  int32_t raw = 0;
  size_t bytes_read;

  i2s_read(MIC_I2S_PORT, &raw, sizeof(raw), &bytes_read, portMAX_DELAY);

  if (bytes_read > 0) {
    return (int16_t)(raw >> 16);
  }
  return 0;
}

// ── SEND WAV HEADER ────────────────────────
void send_wav_header(int totalSamples) {
  int32_t dataSize = totalSamples * 2;
  int32_t fileSize = 36 + dataSize;
  int32_t byteRate = SAMPLE_RATE * 2;
  int32_t sampleRate = SAMPLE_RATE;

  Serial.write("START\n");

  Serial.write("RIFF", 4);
  Serial.write((uint8_t*)&fileSize, 4);
  Serial.write("WAVE", 4);

  Serial.write("fmt ", 4);
  int32_t subchunk1 = 16;
  Serial.write((uint8_t*)&subchunk1, 4);

  int16_t format = 1;
  int16_t channels = 1;
  int16_t bits = 16;
  int16_t align = 2;

  Serial.write((uint8_t*)&format, 2);
  Serial.write((uint8_t*)&channels, 2);
  Serial.write((uint8_t*)&sampleRate, 4);   // ✅ FIXED
  Serial.write((uint8_t*)&byteRate, 4);
  Serial.write((uint8_t*)&align, 2);
  Serial.write((uint8_t*)&bits, 2);

  Serial.write("data", 4);
  Serial.write((uint8_t*)&dataSize, 4);
}
// ── SETUP ──────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  mic_init();

  Serial.println("Press ENTER to start recording...");
}

// ── LOOP ───────────────────────────────────
void loop() {

  // Wait for ENTER to start
  if (Serial.available()) {
    Serial.read();  // clear input

    Serial.println("Recording... press ENTER again to stop");
    sampleIndex = 0;

    // Record until ENTER pressed again
    while (true) {

      // Check stop signal
      if (Serial.available()) {
        Serial.read();
        break;
      }

      if (sampleIndex < MAX_SAMPLES) {
        audioBuffer[sampleIndex++] = read_sample();
      }
    }

    Serial.println("Sending...");

    // Send WAV
    send_wav_header(sampleIndex);
    Serial.write((uint8_t*)audioBuffer, sampleIndex * 2);
    Serial.write("\nEND\n");

    Serial.println("Done.");
    Serial.println("Press ENTER to record again...");
  }
}