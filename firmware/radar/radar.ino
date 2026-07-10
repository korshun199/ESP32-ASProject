/*
  SIMONG RADAR 2026
  ESP32-S3 + ST7789 TFT + ICS-43434 I2S MIC

  Рабочая база:
    TFT driver: Adafruit_ST7789
    init:       tft.init(240, 320)
    rotation:   0

  Геометрия:
    maxX = tft.width()
    maxY = tft.height()

  Никаких угадываний 320x240.
  Рисуем от реального размера, который отдал драйвер.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "driver/i2s.h"

// ============================================================
// TFT PINS
// ============================================================

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12

Adafruit_ST7789 tft(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

// ============================================================
// MIC I2S PINS
// ============================================================

static const int I2S_BCLK_PIN = 4;
static const int I2S_WS_PIN   = 5;
static const int I2S_DATA_PIN = 6;

static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const int SAMPLE_RATE = 16000;
static const int READ_SAMPLES = 512;

static const int LEVEL_MIN = 5000;
static const int LEVEL_MAX = 80000;

int32_t samples[READ_SAMPLES];

// ============================================================
// SCREEN SIZE
// ============================================================

int maxX = 0;
int maxY = 0;

// ============================================================
// UI GEOMETRY
// ============================================================

int headerX = 0;
int headerY = 0;
int headerW = 0;
int headerH = 0;

int mainX = 0;
int mainY = 0;
int mainW = 0;
int mainH = 0;

int monitorX = 0;
int monitorY = 0;
int monitorW = 0;
int monitorH = 0;

int footerX = 0;
int footerY = 0;
int footerW = 0;
int footerH = 0;

// ============================================================
// STATE
// ============================================================

int lastPercent = -1;
int32_t lastLevel = -1;
int32_t lastP2P = -1;

// ============================================================
// SERIAL
// ============================================================

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

// ============================================================
// HELPERS
// ============================================================

int clampPercent(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

int calcMicPercent(int32_t level) {
  return clampPercent(map(level, LEVEL_MIN, LEVEL_MAX, 0, 100));
}

void calcLayout() {
  maxX = tft.width();
  maxY = tft.height();

  headerX = 0;
  headerY = 0;
  headerW = maxX;
  headerH = maxY / 8;

  footerX = 0;
  footerH = maxY / 8;
  footerY = maxY - footerH;
  footerW = maxX;

  monitorW = maxX / 3;
  monitorH = maxY - headerH - footerH;
  monitorX = maxX - monitorW;
  monitorY = headerH;

  mainX = 0;
  mainY = headerH;
  mainW = maxX - monitorW;
  mainH = maxY - headerH - footerH;
}

// ============================================================
// MIC
// ============================================================

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
  printBoth("mic=I2S_READY");
}

bool readMic(int &percent, int32_t &level, int32_t &p2p) {
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(
    I2S_PORT,
    samples,
    sizeof(samples),
    &bytesRead,
    pdMS_TO_TICKS(100)
  );

  if (err != ESP_OK || bytesRead == 0) {
    return false;
  }

  int count = bytesRead / sizeof(int32_t);

  int64_t sum = 0;
  int32_t minVal = 2147483647;
  int32_t maxVal = -2147483647;

  for (int i = 0; i < count; i++) {
    int32_t v = samples[i] >> 8;

    sum += v;

    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
  }

  int32_t mean = sum / count;
  int64_t sumCenteredAbs = 0;

  for (int i = 0; i < count; i++) {
    int32_t v = samples[i] >> 8;
    int32_t centered = v - mean;

    sumCenteredAbs += (centered < 0) ? -centered : centered;
  }

  level = sumCenteredAbs / count;
  p2p = maxVal - minVal;
  percent = calcMicPercent(level);

  return true;
}

// ============================================================
// DRAW STATIC UI
// ============================================================

void drawHeader() {
  tft.fillRect(headerX, headerY, headerW, headerH, ST77XX_RED);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(headerX + 8, headerY + 8);
  tft.print("SIMONG RADAR");
}

void drawMainArea() {
  tft.fillRect(mainX, mainY, mainW, mainH, ST77XX_BLACK);
  tft.drawRect(mainX, mainY, mainW, mainH, ST77XX_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(mainX + 8, mainY + 8);
  tft.print("RADAR");
}

void drawMonitorFrame() {
  tft.fillRect(monitorX, monitorY, monitorW, monitorH, ST77XX_BLACK);
  tft.drawRect(monitorX, monitorY, monitorW, monitorH, ST77XX_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(monitorX + 8, monitorY + 8);
  tft.print("SYS");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(monitorX + 8, monitorY + 42);
  tft.print("MIC");

  tft.setCursor(monitorX + 8, monitorY + 72);
  tft.print("LVL");

  tft.setCursor(monitorX + 8, monitorY + 102);
  tft.print("P2P");

  tft.setCursor(monitorX + 8, monitorY + monitorH - 18);
  tft.print("S3");
}

void drawFooter() {
  tft.fillRect(footerX, footerY, footerW, footerH, ST77XX_RED);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(footerX + 8, footerY + 8);
  tft.print("ST7789 ");
  tft.print(maxX);
  tft.print("x");
  tft.print(maxY);
}

void drawScreen() {
  tft.fillScreen(ST77XX_BLACK);

  drawHeader();
  drawMainArea();
  drawMonitorFrame();
  drawFooter();
}

// ============================================================
// DRAW LIVE MONITOR
// ============================================================

void clearMonitorValues() {
  int x = monitorX + 38;
  int y = monitorY + 38;
  int w = monitorW - 42;
  int h = 90;

  if (w < 10) w = 10;

  tft.fillRect(x, y, w, h, ST77XX_BLACK);
}

void drawMicNoData() {
  clearMonitorValues();

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

  tft.setCursor(monitorX + 42, monitorY + 42);
  tft.print("NO");

  tft.setCursor(monitorX + 42, monitorY + 72);
  tft.print("--");

  tft.setCursor(monitorX + 42, monitorY + 102);
  tft.print("--");
}

void drawMicValues(int percent, int32_t level, int32_t p2p) {
  if (percent == lastPercent && level == lastLevel && p2p == lastP2P) {
    return;
  }

  lastPercent = percent;
  lastLevel = level;
  lastP2P = p2p;

  clearMonitorValues();

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);

  tft.setCursor(monitorX + 42, monitorY + 42);
  tft.print("OK");

  tft.setCursor(monitorX + 42, monitorY + 72);
  tft.print(percent);
  tft.print("%");

  tft.setCursor(monitorX + 42, monitorY + 102);

  if (p2p > 999999) {
    tft.print("999K");
  } else if (p2p > 9999) {
    tft.print(p2p / 1000);
    tft.print("K");
  } else {
    tft.print(p2p);
  }

  int barX = monitorX + 8;
  int barY = monitorY + 130;
  int barW = monitorW - 16;
  int barH = 10;

  if (barW < 20) return;

  int fillW = map(percent, 0, 100, 0, barW - 2);

  tft.drawRect(barX, barY, barW, barH, ST77XX_WHITE);
  tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, ST77XX_BLACK);
  tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, ST77XX_GREEN);
}

// ============================================================
// SETUP / LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  delay(1200);

  printBoth("");
  printBoth("# SIMONG RADAR 2026");
  printBoth("# ESP32-S3 + ST7789 + ICS-43434");

  tft.init(240, 320);
  tft.setRotation(0);

  calcLayout();

  printBoth(String("# tft=") + String(maxX) + "x" + String(maxY));

  drawScreen();
  drawMicNoData();

  setupI2S();

  printBoth("# screen drawn");
}

void loop() {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 200) {
    return;
  }

  lastUpdate = millis();

  int percent = 0;
  int32_t level = 0;
  int32_t p2p = 0;

  bool ok = readMic(percent, level, p2p);

  if (!ok) {
    drawMicNoData();
    printBoth("mic=NO_DATA");
    return;
  }

  drawMicValues(percent, level, p2p);

  printBoth(
    String("mic=OK ") +
    String("lvl=") + String(percent) + String("% ") +
    String("level=") + String(level) + String(" ") +
    String("p2p=") + String(p2p)
  );
}
