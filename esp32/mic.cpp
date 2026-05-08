#include "mic.h"
#include "Arduino.h"
// ── Globals ───────────────────────────────────────────────────
int16_t          audioWindow[AUDIO_WINDOW_SAMPLES];
volatile int audioWindowLen = 0;

void mic_init() {
  // Force pins to a known state before the driver touches them.
  // Prevents the INMP441 from latching noise on startup.
  pinMode(MIC_SCK_PIN, OUTPUT);
  pinMode(MIC_WS_PIN,  OUTPUT);
  digitalWrite(MIC_SCK_PIN, 0);
  digitalWrite(MIC_WS_PIN,  0);
  delay(2000);

  const i2s_config_t i2s_config = {
    .mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate       = SAMPLE_RATE,
    .bits_per_sample   = I2S_BITS_PER_SAMPLE_32BIT,  // INMP441 sends 24-bit in a 32-bit frame
    .channel_format    = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags  = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count     = DMA_BUF_COUNT,
    .dma_buf_len       = DMA_BUF_LEN,
    .use_apll          = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num    = MIC_SCK_PIN,
    .ws_io_num     = MIC_WS_PIN,
    .data_out_num  = I2S_PIN_NO_CHANGE,
    .data_in_num   = MIC_SD_PIN
  };

  i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(MIC_I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(MIC_I2S_PORT);   // flush garbage from DMA before reading
  i2s_start(MIC_I2S_PORT);
}

void mic_read_sample(int16_t* out_sample) {
  int32_t raw = 0;
  size_t  bytes_read;

  i2s_read(MIC_I2S_PORT, &raw, sizeof(raw), &bytes_read, portMAX_DELAY);

  if (bytes_read > 0) {
    *out_sample = (int16_t)(raw >> 16);
  }
}
