/*
  ============================================================
  ESP32 RADAR — УНИВЕРСАЛЬНЫЙ АНАЛИЗАТОР ЗВУКА
  ============================================================

  Оборудование:
    ESP32-S3 N16R8
    TFT ST7789 240x320
    цифровой микрофон ICS-43434

  Возможности:
    - чтение любого звука через I2S;
    - RMS, Peak, P2P, Min, Max и Mean;
    - логарифмический индикатор громкости;
    - осциллограмма из 64 точек;
    - абсолютный спектр из 32 полос;
    - вывод данных через UART0.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "driver/i2s.h"
#include <math.h>
#include <stdint.h>

// ============================================================
// TFT ST7789
// ============================================================

static const int TFT_CS_PIN   = 10;
static const int TFT_DC_PIN   = 9;
static const int TFT_RST_PIN  = 8;
static const int TFT_MOSI_PIN = 11;
static const int TFT_SCLK_PIN = 12;

Adafruit_ST7789 tft(
  TFT_CS_PIN,
  TFT_DC_PIN,
  TFT_RST_PIN
);

// ============================================================
// МИКРОФОН ICS-43434
// ============================================================

static const int I2S_BCLK_PIN = 4;
static const int I2S_WS_PIN   = 5;
static const int I2S_DATA_PIN = 6;

static const i2s_port_t I2S_PORT =
  I2S_NUM_0;

// ============================================================
// ПАРАМЕТРЫ АНАЛИЗА
// ============================================================

static const int SAMPLE_RATE = 16000;
static const int READ_SAMPLES = 512;

static const int WAVE_POINTS = 64;
static const int SPECTRUM_BANDS = 32;

static const int WARMUP_BUFFERS = 8;

// Serial примерно 8 раз в секунду.
static const int SERIAL_EVERY_FRAMES = 3;

// TFT примерно 4 раза в секунду.
static const int TFT_EVERY_FRAMES = 6;

// Центры спектральных полос: 125, 375 ... 7875 Гц.
static const float SPECTRUM_FIRST_HZ =
  125.0f;

static const float SPECTRUM_STEP_HZ =
  250.0f;

// Диапазон логарифмического индикатора громкости.
static const float RMS_SCALE_MIN =
  5000.0f;

static const float RMS_SCALE_MAX =
  700000.0f;

// Диапазон абсолютной шкалы спектра.
static const float SPECTRUM_SCALE_MIN =
  300.0f;

static const float SPECTRUM_SCALE_MAX =
  150000.0f;

// Порог приближения к пределу 24-битного сигнала.
static const int32_t CLIP_THRESHOLD =
  7500000;

// ============================================================
// БУФЕРЫ И ДАННЫЕ
// ============================================================

int32_t samples[READ_SAMPLES];

struct AudioFrame {
  uint32_t frameNumber;
  uint32_t timeMs;

  int count;

  int32_t mean;
  int32_t minimum;
  int32_t maximum;

  int32_t rms;
  int32_t peak;
  int32_t p2p;

  int levelPercent;
  int clip;

  int16_t wave[WAVE_POINTS];
  uint16_t spectrum[SPECTRUM_BANDS];

  float spectrumMaximum;
  float dominantFrequency;
  float dominantAmplitude;
};

AudioFrame currentFrame;

bool micReady = false;

uint32_t frameCounter = 0;
uint32_t readErrors = 0;

uint32_t fpsStartedAt = 0;
uint32_t fpsFrameCount = 0;

float currentFps = 0.0f;

// ============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================

int clampInt(
  int value,
  int minimum,
  int maximum
) {
  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

int32_t absoluteInt32(int32_t value) {
  if (value == INT32_MIN) {
    return INT32_MAX;
  }

  return value < 0
    ? -value
    : value;
}

// Логарифмическая шкала громкости.
int rmsToPercent(int32_t rms) {
  float value =
    (float)rms;

  if (value <= RMS_SCALE_MIN) {
    return 0;
  }

  if (value >= RMS_SCALE_MAX) {
    return 100;
  }

  float currentDb =
    20.0f *
    log10f(
      value /
      RMS_SCALE_MIN
    );

  float maximumDb =
    20.0f *
    log10f(
      RMS_SCALE_MAX /
      RMS_SCALE_MIN
    );

  int percent =
    (int)roundf(
      currentDb /
      maximumDb *
      100.0f
    );

  return clampInt(
    percent,
    0,
    100
  );
}

// Абсолютная логарифмическая шкала спектра.
uint16_t spectrumAmplitudeToLevel(
  float amplitude
) {
  if (amplitude <= SPECTRUM_SCALE_MIN) {
    return 0;
  }

  if (amplitude >= SPECTRUM_SCALE_MAX) {
    return 1000;
  }

  float currentDb =
    20.0f *
    log10f(
      amplitude /
      SPECTRUM_SCALE_MIN
    );

  float maximumDb =
    20.0f *
    log10f(
      SPECTRUM_SCALE_MAX /
      SPECTRUM_SCALE_MIN
    );

  int level =
    (int)roundf(
      currentDb /
      maximumDb *
      1000.0f
    );

  return
    (uint16_t)clampInt(
      level,
      0,
      1000
    );
}

void formatCompact(
  double value,
  char *buffer,
  size_t bufferSize
) {
  double absoluteValue =
    fabs(value);

  if (absoluteValue >= 1000000.0) {
    snprintf(
      buffer,
      bufferSize,
      "%.1fM",
      value / 1000000.0
    );
  } else if (absoluteValue >= 1000.0) {
    snprintf(
      buffer,
      bufferSize,
      "%.1fK",
      value / 1000.0
    );
  } else {
    snprintf(
      buffer,
      bufferSize,
      "%.0f",
      value
    );
  }
}

// ============================================================
// I2S
// ============================================================

bool setupI2S() {
  i2s_config_t config = {
    .mode =
      (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_RX
      ),

    .sample_rate =
      SAMPLE_RATE,

    .bits_per_sample =
      I2S_BITS_PER_SAMPLE_32BIT,

    .channel_format =
      I2S_CHANNEL_FMT_ONLY_LEFT,

    .communication_format =
      I2S_COMM_FORMAT_STAND_I2S,

    .intr_alloc_flags =
      ESP_INTR_FLAG_LEVEL1,

    .dma_buf_count =
      8,

    .dma_buf_len =
      256,

    .use_apll =
      false,

    .tx_desc_auto_clear =
      false,

    .fixed_mclk =
      0
  };

  i2s_pin_config_t pins = {
    .bck_io_num =
      I2S_BCLK_PIN,

    .ws_io_num =
      I2S_WS_PIN,

    .data_out_num =
      I2S_PIN_NO_CHANGE,

    .data_in_num =
      I2S_DATA_PIN
  };

  esp_err_t error =
    i2s_driver_install(
      I2S_PORT,
      &config,
      0,
      nullptr
    );

  if (error != ESP_OK) {
    Serial.printf(
      "# I2S_DRIVER_ERROR=%d\n",
      (int)error
    );

    return false;
  }

  error =
    i2s_set_pin(
      I2S_PORT,
      &pins
    );

  if (error != ESP_OK) {
    Serial.printf(
      "# I2S_PIN_ERROR=%d\n",
      (int)error
    );

    return false;
  }

  i2s_zero_dma_buffer(
    I2S_PORT
  );

  return true;
}

void warmUpMicrophone() {
  for (
    int bufferNumber = 0;
    bufferNumber < WARMUP_BUFFERS;
    bufferNumber++
  ) {
    size_t bytesRead = 0;

    i2s_read(
      I2S_PORT,
      samples,
      sizeof(samples),
      &bytesRead,
      pdMS_TO_TICKS(100)
    );
  }
}

// ============================================================
// СПЕКТР
// ============================================================

float calculateBandAmplitude(
  float frequency,
  int count,
  int32_t mean
) {
  const float pi =
    3.14159265358979323846f;

  float omega =
    2.0f *
    pi *
    frequency /
    SAMPLE_RATE;

  float coefficient =
    2.0f *
    cosf(omega);

  float q0 = 0.0f;
  float q1 = 0.0f;
  float q2 = 0.0f;

  for (
    int index = 0;
    index < count;
    index++
  ) {
    float value =
      (float)(
        (samples[index] >> 8) -
        mean
      );

    q0 =
      value +
      coefficient * q1 -
      q2;

    q2 = q1;
    q1 = q0;
  }

  float power =
    q1 * q1 +
    q2 * q2 -
    coefficient * q1 * q2;

  if (power < 0.0f) {
    power = 0.0f;
  }

  return
    2.0f *
    sqrtf(power) /
    count;
}

void calculateSpectrum(
  int count,
  int32_t mean
) {
  float maximumAmplitude = 0.0f;
  float dominantAmplitude = 0.0f;
  float dominantFrequency = 0.0f;

  for (
    int band = 0;
    band < SPECTRUM_BANDS;
    band++
  ) {
    float frequency =
      SPECTRUM_FIRST_HZ +
      band *
      SPECTRUM_STEP_HZ;

    float amplitude =
      calculateBandAmplitude(
        frequency,
        count,
        mean
      );

    currentFrame.spectrum[band] =
      spectrumAmplitudeToLevel(
        amplitude
      );

    if (
      amplitude >
      maximumAmplitude
    ) {
      maximumAmplitude =
        amplitude;
    }

    if (
      amplitude >
      dominantAmplitude
    ) {
      dominantAmplitude =
        amplitude;

      dominantFrequency =
        frequency;
    }
  }

  currentFrame.spectrumMaximum =
    maximumAmplitude;

  currentFrame.dominantFrequency =
    dominantFrequency;

  currentFrame.dominantAmplitude =
    dominantAmplitude;
}

// ============================================================
// ОСЦИЛЛОГРАММА
// ============================================================

void calculateWave(
  int count,
  int32_t mean
) {
  int32_t maximumAbsolute = 1;

  for (
    int index = 0;
    index < count;
    index++
  ) {
    int32_t centered =
      (samples[index] >> 8) -
      mean;

    int32_t absoluteValue =
      absoluteInt32(centered);

    if (
      absoluteValue >
      maximumAbsolute
    ) {
      maximumAbsolute =
        absoluteValue;
    }
  }

  for (
    int point = 0;
    point < WAVE_POINTS;
    point++
  ) {
    int sourceIndex =
      (
        point *
        (count - 1)
      ) /
      (
        WAVE_POINTS - 1
      );

    int32_t centered =
      (samples[sourceIndex] >> 8) -
      mean;

    int32_t normalized =
      (
        (int64_t)centered *
        1000
      ) /
      maximumAbsolute;

    currentFrame.wave[point] =
      (int16_t)clampInt(
        normalized,
        -1000,
        1000
      );
  }
}

// ============================================================
// ЧТЕНИЕ МИКРОФОНА
// ============================================================

bool readMicrophone() {
  size_t bytesRead = 0;

  esp_err_t error =
    i2s_read(
      I2S_PORT,
      samples,
      sizeof(samples),
      &bytesRead,
      pdMS_TO_TICKS(100)
    );

  if (
    error != ESP_OK ||
    bytesRead == 0
  ) {
    readErrors++;
    return false;
  }

  int count =
    bytesRead /
    sizeof(int32_t);

  if (count <= 0) {
    readErrors++;
    return false;
  }

  int64_t sum = 0;

  int32_t minimumValue =
    INT32_MAX;

  int32_t maximumValue =
    INT32_MIN;

  for (
    int index = 0;
    index < count;
    index++
  ) {
    int32_t value =
      samples[index] >> 8;

    sum += value;

    if (value < minimumValue) {
      minimumValue = value;
    }

    if (value > maximumValue) {
      maximumValue = value;
    }
  }

  int32_t mean =
    (int32_t)(
      sum /
      count
    );

  uint64_t squareSum = 0;
  int32_t peak = 0;

  for (
    int index = 0;
    index < count;
    index++
  ) {
    int32_t centered =
      (samples[index] >> 8) -
      mean;

    int32_t absoluteValue =
      absoluteInt32(centered);

    if (absoluteValue > peak) {
      peak = absoluteValue;
    }

    int64_t square =
      (int64_t)centered *
      (int64_t)centered;

    squareSum +=
      (uint64_t)square;
  }

  double meanSquare =
    (double)squareSum /
    count;

  int32_t rms =
    (int32_t)sqrt(meanSquare);

  currentFrame.frameNumber =
    ++frameCounter;

  currentFrame.timeMs =
    millis();

  currentFrame.count =
    count;

  currentFrame.mean =
    mean;

  currentFrame.minimum =
    minimumValue;

  currentFrame.maximum =
    maximumValue;

  currentFrame.rms =
    rms;

  currentFrame.peak =
    peak;

  currentFrame.p2p =
    maximumValue -
    minimumValue;

  currentFrame.levelPercent =
    rmsToPercent(rms);

  currentFrame.clip =
    peak >= CLIP_THRESHOLD
      ? 1
      : 0;

  calculateWave(
    count,
    mean
  );

  calculateSpectrum(
    count,
    mean
  );

  fpsFrameCount++;

  return true;
}

// ============================================================
// FPS
// ============================================================

void updateFps() {
  uint32_t now =
    millis();

  uint32_t elapsed =
    now -
    fpsStartedAt;

  if (elapsed < 1000) {
    return;
  }

  currentFps =
    (
      (float)fpsFrameCount *
      1000.0f
    ) /
    elapsed;

  fpsFrameCount = 0;
  fpsStartedAt = now;
}

// ============================================================
// TFT
// ============================================================

void drawStaticInterface() {
  tft.fillScreen(
    ST77XX_BLACK
  );

  tft.fillRect(
    0,
    0,
    240,
    24,
    ST77XX_RED
  );

  tft.setTextSize(2);

  tft.setTextColor(
    ST77XX_WHITE,
    ST77XX_RED
  );

  tft.setCursor(7, 5);
  tft.print("AUDIO RADAR");

  tft.drawRect(0, 24, 240, 32, ST77XX_WHITE);
  tft.drawRect(0, 56, 240, 28, ST77XX_WHITE);

  tft.drawRect(0, 84, 120, 42, ST77XX_WHITE);
  tft.drawRect(120, 84, 120, 42, ST77XX_WHITE);

  tft.drawRect(0, 126, 120, 42, ST77XX_WHITE);
  tft.drawRect(120, 126, 120, 42, ST77XX_WHITE);

  tft.drawRect(0, 168, 240, 58, ST77XX_WHITE);
  tft.drawRect(0, 226, 240, 70, ST77XX_WHITE);

  tft.fillRect(
    0,
    296,
    240,
    24,
    ST77XX_RED
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ST77XX_WHITE,
    ST77XX_RED
  );

  tft.setCursor(5, 304);
  tft.print("LOG LEVEL | ABS SPECTRUM");
}

void drawValueCell(
  int x,
  int y,
  int width,
  int height,
  const char *label,
  double value,
  uint16_t labelColor
) {
  char valueText[20];

  formatCompact(
    value,
    valueText,
    sizeof(valueText)
  );

  tft.fillRect(
    x + 1,
    y + 1,
    width - 2,
    height - 2,
    ST77XX_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    labelColor,
    ST77XX_BLACK
  );

  tft.setCursor(
    x + 6,
    y + 5
  );

  tft.print(label);

  tft.setTextSize(2);

  tft.setTextColor(
    ST77XX_WHITE,
    ST77XX_BLACK
  );

  tft.setCursor(
    x + 6,
    y + 19
  );

  tft.print(valueText);
}

void drawWaveform() {
  const int graphX = 3;
  const int graphTop = 181;
  const int graphBottom = 221;
  const int graphMiddle =
    (
      graphTop +
      graphBottom
    ) /
    2;

  tft.fillRect(
    1,
    169,
    238,
    56,
    ST77XX_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ST77XX_CYAN,
    ST77XX_BLACK
  );

  tft.setCursor(5, 172);
  tft.print("WAVE");

  tft.drawFastHLine(
    graphX,
    graphMiddle,
    234,
    ST77XX_BLUE
  );

  int previousX =
    graphX;

  int previousY =
    graphMiddle;

  for (
    int point = 0;
    point < WAVE_POINTS;
    point++
  ) {
    int x =
      graphX +
      (
        point *
        233
      ) /
      (
        WAVE_POINTS - 1
      );

    int y =
      graphMiddle -
      (
        currentFrame.wave[point] *
        19
      ) /
      1000;

    y =
      clampInt(
        y,
        graphTop,
        graphBottom
      );

    if (point > 0) {
      tft.drawLine(
        previousX,
        previousY,
        x,
        y,
        ST77XX_GREEN
      );
    }

    previousX = x;
    previousY = y;
  }
}

void drawSpectrum() {
  const int graphX = 4;
  const int graphY = 241;
  const int graphHeight = 50;
  const int barWidth = 7;

  tft.fillRect(
    1,
    227,
    238,
    68,
    ST77XX_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ST77XX_MAGENTA,
    ST77XX_BLACK
  );

  tft.setCursor(5, 230);
  tft.print("ABS SPECTRUM 125-7875 Hz");

  for (
    int band = 0;
    band < SPECTRUM_BANDS;
    band++
  ) {
    int height =
      (
        currentFrame.spectrum[band] *
        graphHeight
      ) /
      1000;

    height =
      clampInt(
        height,
        0,
        graphHeight
      );

    int x =
      graphX +
      band *
      barWidth;

    int y =
      graphY +
      graphHeight -
      height;

    uint16_t color =
      height < 18
        ? ST77XX_BLUE
        : (
            height < 32
              ? ST77XX_GREEN
              : (
                  height < 44
                    ? ST77XX_YELLOW
                    : ST77XX_RED
                )
          );

    tft.fillRect(
      x,
      graphY,
      barWidth - 1,
      graphHeight,
      ST77XX_BLACK
    );

    if (height > 0) {
      tft.fillRect(
        x,
        y,
        barWidth - 1,
        height,
        color
      );
    }
  }
}

void drawNoData() {
  tft.fillRect(
    1,
    25,
    238,
    30,
    ST77XX_BLACK
  );

  tft.setTextSize(2);

  tft.setTextColor(
    ST77XX_YELLOW,
    ST77XX_BLACK
  );

  tft.setCursor(7, 33);
  tft.print("MIC NO DATA");
}

void updateScreen() {
  tft.fillRect(
    1,
    25,
    238,
    30,
    ST77XX_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ST77XX_GREEN,
    ST77XX_BLACK
  );

  tft.setCursor(7, 29);
  tft.print("MIC OK");

  tft.setTextColor(
    ST77XX_WHITE,
    ST77XX_BLACK
  );

  tft.setCursor(7, 43);
  tft.print("FRAME ");
  tft.print(
    currentFrame.frameNumber
  );

  tft.setCursor(142, 29);
  tft.print("FPS ");
  tft.print(
    currentFps,
    1
  );

  tft.setCursor(142, 43);
  tft.print("ERR ");
  tft.print(
    readErrors
  );

  tft.fillRect(
    1,
    57,
    238,
    26,
    ST77XX_BLACK
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ST77XX_WHITE,
    ST77XX_BLACK
  );

  tft.setCursor(6, 62);
  tft.print("LEVEL");

  tft.setCursor(6, 73);
  tft.print(
    currentFrame.levelPercent
  );
  tft.print("%");

  int barX = 48;
  int barY = 64;
  int barWidth = 184;
  int barHeight = 12;

  tft.drawRect(
    barX,
    barY,
    barWidth,
    barHeight,
    ST77XX_WHITE
  );

  tft.fillRect(
    barX + 1,
    barY + 1,
    barWidth - 2,
    barHeight - 2,
    ST77XX_BLACK
  );

  int fillWidth =
    (
      currentFrame.levelPercent *
      (barWidth - 2)
    ) /
    100;

  uint16_t levelColor =
    currentFrame.clip
      ? ST77XX_RED
      : (
          currentFrame.levelPercent < 65
            ? ST77XX_GREEN
            : (
                currentFrame.levelPercent < 85
                  ? ST77XX_YELLOW
                  : ST77XX_RED
              )
        );

  tft.fillRect(
    barX + 1,
    barY + 1,
    fillWidth,
    barHeight - 2,
    levelColor
  );

  if (currentFrame.clip) {
    tft.setTextColor(
      ST77XX_RED,
      ST77XX_BLACK
    );

    tft.setCursor(205, 57);
    tft.print("CLIP");
  }

  drawValueCell(
    0,
    84,
    120,
    42,
    "RMS",
    currentFrame.rms,
    ST77XX_CYAN
  );

  drawValueCell(
    120,
    84,
    120,
    42,
    "PEAK",
    currentFrame.peak,
    ST77XX_CYAN
  );

  drawValueCell(
    0,
    126,
    120,
    42,
    "P2P",
    currentFrame.p2p,
    ST77XX_CYAN
  );

  drawValueCell(
    120,
    126,
    120,
    42,
    "DOM Hz",
    currentFrame.dominantFrequency,
    ST77XX_CYAN
  );

  drawWaveform();
  drawSpectrum();
}

// ============================================================
// SERIAL
// ============================================================

void printSerialFrame() {
  Serial.printf(
    "FRAME T_MS=%lu ID=%lu COUNT=%d MEAN=%ld MIN=%ld MAX=%ld RMS=%ld PEAK=%ld P2P=%ld LEVEL=%d CLIP=%d DOM_HZ=%.1f DOM_AMP=%.1f SPEC_MAX=%.1f READ_ERR=%lu FPS=%.1f\n",
    (unsigned long)currentFrame.timeMs,
    (unsigned long)currentFrame.frameNumber,
    currentFrame.count,
    (long)currentFrame.mean,
    (long)currentFrame.minimum,
    (long)currentFrame.maximum,
    (long)currentFrame.rms,
    (long)currentFrame.peak,
    (long)currentFrame.p2p,
    currentFrame.levelPercent,
    currentFrame.clip,
    currentFrame.dominantFrequency,
    currentFrame.dominantAmplitude,
    currentFrame.spectrumMaximum,
    (unsigned long)readErrors,
    currentFps
  );

  Serial.printf(
    "WAVE T_MS=%lu DATA=",
    (unsigned long)currentFrame.timeMs
  );

  for (
    int point = 0;
    point < WAVE_POINTS;
    point++
  ) {
    if (point > 0) {
      Serial.print(",");
    }

    Serial.print(
      currentFrame.wave[point]
    );
  }

  Serial.println();

  Serial.printf(
    "SPECTRUM T_MS=%lu FIRST_HZ=%.1f STEP_HZ=%.1f SCALE=ABS_LOG DATA=",
    (unsigned long)currentFrame.timeMs,
    SPECTRUM_FIRST_HZ,
    SPECTRUM_STEP_HZ
  );

  for (
    int band = 0;
    band < SPECTRUM_BANDS;
    band++
  ) {
    if (band > 0) {
      Serial.print(",");
    }

    Serial.print(
      currentFrame.spectrum[band]
    );
  }

  Serial.println();
  Serial.println("---");
}

// ============================================================
// SETUP И LOOP
// ============================================================

void setup() {
  Serial.begin(115200);

  delay(800);

  Serial.println();
  Serial.println("# ESP32 RADAR UNIVERSAL AUDIO MONITOR");
  Serial.println("# ESP32-S3 + ST7789 + ICS-43434");
  Serial.println("# FORMAT=FRAME,WAVE,SPECTRUM");
  Serial.println("# LEVEL_SCALE=LOG");
  Serial.println("# SPECTRUM_SCALE=ABS_LOG");
  Serial.println("# SPECTRUM_BANDS=32");
  Serial.println("# SPECTRUM_RANGE=125..7875Hz");

  Serial.printf(
    "# SAMPLE_RATE=%d READ_SAMPLES=%d WAVE_POINTS=%d\n",
    SAMPLE_RATE,
    READ_SAMPLES,
    WAVE_POINTS
  );

  SPI.begin(
    TFT_SCLK_PIN,
    -1,
    TFT_MOSI_PIN,
    TFT_CS_PIN
  );

  tft.init(
    240,
    320
  );

  tft.setRotation(0);

  drawStaticInterface();
  drawNoData();

  micReady =
    setupI2S();

  if (micReady) {
    Serial.println("# MIC=I2S_READY");
    Serial.println("# MIC=WARMUP");

    warmUpMicrophone();

    Serial.println("# MIC=READY");
  } else {
    Serial.println("# MIC=I2S_ERROR");
  }

  fpsStartedAt =
    millis();
}

void loop() {
  if (!micReady) {
    drawNoData();
    delay(500);
    return;
  }

  bool frameOk =
    readMicrophone();

  updateFps();

  if (!frameOk) {
    return;
  }

  if (
    currentFrame.frameNumber %
    SERIAL_EVERY_FRAMES ==
    0
  ) {
    printSerialFrame();
  }

  if (
    currentFrame.frameNumber %
    TFT_EVERY_FRAMES ==
    0
  ) {
    updateScreen();
  }
}
