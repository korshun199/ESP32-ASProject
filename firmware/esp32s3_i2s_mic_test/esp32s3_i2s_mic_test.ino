#include <Arduino.h>
#include "driver/i2s.h"

static const int I2S_BCLK_PIN = 4;
static const int I2S_WS_PIN   = 5;
static const int I2S_DATA_PIN = 6;

static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const int SAMPLE_RATE = 16000;
static const int READ_SAMPLES = 512;

static const int LEVEL_MIN = 5000;
static const int LEVEL_MAX = 80000;

int32_t samples[READ_SAMPLES];

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA_PIN
  };

  esp_err_t err;

  err = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  if (err != ESP_OK) {
    printBoth(String("mic=ERROR driver err=") + String((int)err));
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    printBoth(String("mic=ERROR pins err=") + String((int)err));
    return;
  }

  i2s_zero_dma_buffer(I2S_PORT);
}

int calcPercent(int32_t level) {
  int percent = map(level, LEVEL_MIN, LEVEL_MAX, 0, 100);

  if (percent < 0) {
    percent = 0;
  }

  if (percent > 100) {
    percent = 100;
  }

  return percent;
}

String makeBar(int percent) {
  int barLen = percent / 5;

  String bar = "";
  for (int i = 0; i < barLen; i++) {
    bar += "#";
  }

  return bar;
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  delay(1200);

  printBoth("");
  printBoth("# ESP32-S3 ICS-43434 MIC TEST");
  printBoth("# MIC pins: BCLK=GPIO4 WS=GPIO5 DATA=GPIO6 LR=GND");
  printBoth("# output: mic=OK lvl=% level=raw p2p=peak");
  printBoth("");

  setupI2S();
}

void loop() {
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(
    I2S_PORT,
    samples,
    sizeof(samples),
    &bytesRead,
    pdMS_TO_TICKS(1000)
  );

  if (err != ESP_OK || bytesRead == 0) {
    printBoth("mic=NO_DATA");
    delay(300);
    return;
  }

  int count = bytesRead / sizeof(int32_t);

  int64_t sum = 0;
  int32_t minVal = 2147483647;
  int32_t maxVal = -2147483647;

  for (int i = 0; i < count; i++) {
    int32_t v = samples[i] >> 8;

    sum += v;

    if (v < minVal) {
      minVal = v;
    }

    if (v > maxVal) {
      maxVal = v;
    }
  }

  int32_t mean = sum / count;

  int64_t sumCenteredAbs = 0;

  for (int i = 0; i < count; i++) {
    int32_t v = samples[i] >> 8;
    int32_t centered = v - mean;

    sumCenteredAbs += (centered < 0) ? -centered : centered;
  }

  int32_t level = sumCenteredAbs / count;
  int32_t p2p = maxVal - minVal;

  int percent = calcPercent(level);
  String bar = makeBar(percent);

  printBoth(
    String("mic=OK ") +
    String("lvl=") + String(percent) + String("% ") +
    String("level=") + String(level) + String(" ") +
    String("p2p=") + String(p2p) + String(" ") +
    String("|") + bar
  );

  delay(200);
}
