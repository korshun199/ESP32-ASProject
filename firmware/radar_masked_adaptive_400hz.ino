/*
  ESP32 RADAR — два микрофона, адаптивное подавление фона.

  Канал A: направленный микрофон, L/R -> GND.
  Канал B: открытый микрофон, L/R -> 3.3V.

  Общие линии I2S:
    BCLK -> GPIO4
    WS   -> GPIO5
    SD   -> GPIO6

  Очищенный сигнал:
    TARGET = A - EFFECTIVE_K * B

  EFFECTIVE_K зависит от корреляции каналов.
  Для теста отправить символ T в монитор порта.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "driver/i2s.h"
#include <math.h>
#include <stdint.h>

// TFT ST7789.
static const int TFT_CS_PIN = 10;
static const int TFT_DC_PIN = 9;
static const int TFT_RST_PIN = 8;
static const int TFT_MOSI_PIN = 11;
static const int TFT_SCLK_PIN = 12;

Adafruit_ST7789 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// I2S.
static const int I2S_BCLK_PIN = 4;
static const int I2S_WS_PIN = 5;
static const int I2S_DATA_PIN = 6;
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const bool SWAP_CHANNELS = false;

// Анализ.
static const int SAMPLE_RATE = 16000;
static const int READ_SAMPLES = 512;
static const int WAVE_POINTS = 64;
static const int SPECTRUM_BANDS = 32;
static const int MAX_LAG_SAMPLES = 8;
static const int WARMUP_BUFFERS = 8;
static const int SERIAL_EVERY_FRAMES = 6;
static const int TFT_EVERY_FRAMES = 6;

static const float TEST_TONE_HZ = 400.0f;
static const float SPECTRUM_FIRST_HZ = 125.0f;
static const float SPECTRUM_STEP_HZ = 250.0f;

static const float RMS_SCALE_MIN = 5000.0f;
static const float RMS_SCALE_MAX = 700000.0f;
static const float SPECTRUM_SCALE_MIN = 300.0f;
static const float SPECTRUM_SCALE_MAX = 150000.0f;
static const int32_t CLIP_THRESHOLD = 7500000;

// Адаптация коэффициента усиления.
static const float GAIN_UPDATE_CORRELATION = 0.85f;
static const float GAIN_MIN = 0.20f;
static const float GAIN_MAX = 5.00f;
static const float GAIN_SMOOTHING = 0.02f;
static const int32_t GAIN_UPDATE_MIN_RMS = 6000;

// Маска подавления.
// При корреляции <= 0.25 фон не вычитается.
// При корреляции >= 0.85 применяется полный коэффициент K.
static const float MASK_CORRELATION_LOW = 0.25f;
static const float MASK_CORRELATION_HIGH = 0.85f;

// Буферы.
int32_t stereoSamples[READ_SAMPLES * 2];
int32_t samplesA[READ_SAMPLES];
int32_t samplesB[READ_SAMPLES];
int32_t samplesTarget[READ_SAMPLES];

struct AudioFrame {
  int32_t mean;
  int32_t minimum;
  int32_t maximum;
  int32_t rms;
  int32_t peak;
  int32_t p2p;
  int levelPercent;
  int clip;
  float tone400Amplitude;
  uint16_t tone400Level;
  int16_t wave[WAVE_POINTS];
  uint16_t spectrum[SPECTRUM_BANDS];
  float spectrumMaximum;
  float dominantFrequency;
  float dominantAmplitude;
};

struct ComparisonFrame {
  uint32_t frameNumber;
  uint32_t timeMs;
  int count;
  int bestLag;
  float correlationRaw;
  float correlationAligned;
  float correlationMask;
  float gainCandidate;
  float adaptiveGain;
  float effectiveGain;
  float rmsRatioAB;
  float residualRatio;
  float toneRatioAB;
  float toneTargetRatio;
  float commonScore;
  float directionalScore;
  float backgroundScore;
  bool gainUpdated;
  AudioFrame channelA;
  AudioFrame channelB;
  AudioFrame target;
};

ComparisonFrame currentFrame;

bool micReady = false;
uint32_t frameCounter = 0;
uint32_t readErrors = 0;
uint32_t fpsStartedAt = 0;
uint32_t fpsFrameCount = 0;
float currentFps = 0.0f;
float adaptiveGain = 1.0f;

// Таймер контролируемого теста.
bool testRunning = false;
uint32_t testStartedAt = 0;

int clampInt(int value, int minimum, int maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

float clampFloat(float value, float minimum, float maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

int32_t absoluteInt32(int32_t value) {
  if (value == INT32_MIN) return INT32_MAX;
  return value < 0 ? -value : value;
}

int rmsToPercent(int32_t rms) {
  float value = (float)rms;
  if (value <= RMS_SCALE_MIN) return 0;
  if (value >= RMS_SCALE_MAX) return 100;

  float currentDb = 20.0f * log10f(value / RMS_SCALE_MIN);
  float maximumDb = 20.0f * log10f(RMS_SCALE_MAX / RMS_SCALE_MIN);

  return clampInt((int)roundf(currentDb / maximumDb * 100.0f), 0, 100);
}

uint16_t spectrumAmplitudeToLevel(float amplitude) {
  if (amplitude <= SPECTRUM_SCALE_MIN) return 0;
  if (amplitude >= SPECTRUM_SCALE_MAX) return 1000;

  float currentDb = 20.0f * log10f(amplitude / SPECTRUM_SCALE_MIN);
  float maximumDb = 20.0f * log10f(SPECTRUM_SCALE_MAX / SPECTRUM_SCALE_MIN);

  return (uint16_t)clampInt(
    (int)roundf(currentDb / maximumDb * 1000.0f),
    0,
    1000
  );
}

void formatCompact(double value, char *buffer, size_t bufferSize) {
  double absoluteValue = fabs(value);

  if (absoluteValue >= 1000000.0) {
    snprintf(buffer, bufferSize, "%.1fM", value / 1000000.0);
  } else if (absoluteValue >= 1000.0) {
    snprintf(buffer, bufferSize, "%.1fK", value / 1000.0);
  } else {
    snprintf(buffer, bufferSize, "%.0f", value);
  }
}

bool setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
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

  esp_err_t error = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
  if (error != ESP_OK) {
    Serial.printf("# I2S_DRIVER_ERROR=%d\n", (int)error);
    return false;
  }

  error = i2s_set_pin(I2S_PORT, &pins);
  if (error != ESP_OK) {
    Serial.printf("# I2S_PIN_ERROR=%d\n", (int)error);
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  return true;
}

void warmUpMicrophones() {
  for (int bufferNumber = 0; bufferNumber < WARMUP_BUFFERS; bufferNumber++) {
    size_t bytesRead = 0;
    i2s_read(
      I2S_PORT,
      stereoSamples,
      sizeof(stereoSamples),
      &bytesRead,
      pdMS_TO_TICKS(100)
    );
  }
}

float calculateBandAmplitude(
  const int32_t *samples,
  float frequency,
  int count,
  int32_t mean
) {
  const float pi = 3.14159265358979323846f;
  float omega = 2.0f * pi * frequency / SAMPLE_RATE;
  float coefficient = 2.0f * cosf(omega);

  float q0 = 0.0f;
  float q1 = 0.0f;
  float q2 = 0.0f;

  for (int index = 0; index < count; index++) {
    float value = (float)(samples[index] - mean);
    q0 = value + coefficient * q1 - q2;
    q2 = q1;
    q1 = q0;
  }

  float power = q1 * q1 + q2 * q2 - coefficient * q1 * q2;
  if (power < 0.0f) power = 0.0f;

  return 2.0f * sqrtf(power) / count;
}

void calculateSpectrum(
  const int32_t *samples,
  int count,
  AudioFrame &frame
) {
  frame.tone400Amplitude = calculateBandAmplitude(
    samples,
    TEST_TONE_HZ,
    count,
    frame.mean
  );

  frame.tone400Level = spectrumAmplitudeToLevel(frame.tone400Amplitude);

  float maximumAmplitude = 0.0f;
  float dominantAmplitude = 0.0f;
  float dominantFrequency = 0.0f;

  for (int band = 0; band < SPECTRUM_BANDS; band++) {
    float frequency = SPECTRUM_FIRST_HZ + band * SPECTRUM_STEP_HZ;
    float amplitude = calculateBandAmplitude(
      samples,
      frequency,
      count,
      frame.mean
    );

    frame.spectrum[band] = spectrumAmplitudeToLevel(amplitude);

    if (amplitude > maximumAmplitude) maximumAmplitude = amplitude;

    if (amplitude > dominantAmplitude) {
      dominantAmplitude = amplitude;
      dominantFrequency = frequency;
    }
  }

  frame.spectrumMaximum = maximumAmplitude;
  frame.dominantFrequency = dominantFrequency;
  frame.dominantAmplitude = dominantAmplitude;
}

void calculateWave(
  const int32_t *samples,
  int count,
  AudioFrame &frame
) {
  int32_t maximumAbsolute = 1;

  for (int index = 0; index < count; index++) {
    int32_t centered = samples[index] - frame.mean;
    int32_t absoluteValue = absoluteInt32(centered);
    if (absoluteValue > maximumAbsolute) maximumAbsolute = absoluteValue;
  }

  for (int point = 0; point < WAVE_POINTS; point++) {
    int sourceIndex = point * (count - 1) / (WAVE_POINTS - 1);
    int32_t centered = samples[sourceIndex] - frame.mean;
    int32_t normalized = ((int64_t)centered * 1000) / maximumAbsolute;

    frame.wave[point] = (int16_t)clampInt(normalized, -1000, 1000);
  }
}

void analyzeChannel(
  const int32_t *samples,
  int count,
  AudioFrame &frame
) {
  int64_t sum = 0;
  int32_t minimumValue = INT32_MAX;
  int32_t maximumValue = INT32_MIN;

  for (int index = 0; index < count; index++) {
    int32_t value = samples[index];
    sum += value;
    if (value < minimumValue) minimumValue = value;
    if (value > maximumValue) maximumValue = value;
  }

  frame.mean = (int32_t)(sum / count);

  uint64_t squareSum = 0;
  int32_t peak = 0;

  for (int index = 0; index < count; index++) {
    int32_t centered = samples[index] - frame.mean;
    int32_t absoluteValue = absoluteInt32(centered);

    if (absoluteValue > peak) peak = absoluteValue;

    int64_t square = (int64_t)centered * (int64_t)centered;
    squareSum += (uint64_t)square;
  }

  frame.minimum = minimumValue;
  frame.maximum = maximumValue;
  frame.rms = (int32_t)sqrt((double)squareSum / count);
  frame.peak = peak;
  frame.p2p = maximumValue - minimumValue;
  frame.levelPercent = rmsToPercent(frame.rms);
  frame.clip = peak >= CLIP_THRESHOLD ? 1 : 0;

  calculateWave(samples, count, frame);
  calculateSpectrum(samples, count, frame);
}

float calculateLagCorrelation(
  const int32_t *a,
  const int32_t *b,
  int count,
  int lag,
  int32_t meanA,
  int32_t meanB
) {
  int start = lag < 0 ? -lag : 0;
  int finish = lag > 0 ? count - lag : count;

  if (finish - start < 32) return 0.0f;

  double cross = 0.0;
  double energyA = 0.0;
  double energyB = 0.0;

  for (int index = start; index < finish; index++) {
    int bIndex = index + lag;
    double valueA = (double)(a[index] - meanA);
    double valueB = (double)(b[bIndex] - meanB);

    cross += valueA * valueB;
    energyA += valueA * valueA;
    energyB += valueB * valueB;
  }

  double denominator = sqrt(energyA * energyB);
  if (denominator < 1.0) return 0.0f;

  return clampFloat((float)(cross / denominator), -1.0f, 1.0f);
}

int findBestLag(
  const int32_t *a,
  const int32_t *b,
  int count,
  int32_t meanA,
  int32_t meanB,
  float &bestCorrelation
) {
  int bestLag = 0;
  bestCorrelation = calculateLagCorrelation(
    a,
    b,
    count,
    0,
    meanA,
    meanB
  );

  for (int lag = -MAX_LAG_SAMPLES; lag <= MAX_LAG_SAMPLES; lag++) {
    float correlation = calculateLagCorrelation(
      a,
      b,
      count,
      lag,
      meanA,
      meanB
    );

    if (correlation > bestCorrelation) {
      bestCorrelation = correlation;
      bestLag = lag;
    }
  }

  return bestLag;
}

float calculateGainCandidate(
  const int32_t *a,
  const int32_t *b,
  int count,
  int lag,
  int32_t meanA,
  int32_t meanB
) {
  int start = lag < 0 ? -lag : 0;
  int finish = lag > 0 ? count - lag : count;

  double cross = 0.0;
  double energyB = 0.0;

  for (int index = start; index < finish; index++) {
    int bIndex = index + lag;
    double valueA = (double)(a[index] - meanA);
    double valueB = (double)(b[bIndex] - meanB);

    cross += valueA * valueB;
    energyB += valueB * valueB;
  }

  if (energyB < 1.0) return adaptiveGain;

  return clampFloat((float)(cross / energyB), GAIN_MIN, GAIN_MAX);
}

float calculateCorrelationMask(float correlation) {
  float range = MASK_CORRELATION_HIGH - MASK_CORRELATION_LOW;
  if (range <= 0.0f) return 0.0f;

  return clampFloat(
    (correlation - MASK_CORRELATION_LOW) / range,
    0.0f,
    1.0f
  );
}

void buildTargetSignal(
  int count,
  int lag,
  int32_t meanA,
  int32_t meanB,
  float effectiveGain
) {
  for (int index = 0; index < count; index++) {
    int bIndex = index + lag;
    int32_t centeredA = samplesA[index] - meanA;
    int32_t centeredB = 0;

    if (bIndex >= 0 && bIndex < count) {
      centeredB = samplesB[bIndex] - meanB;
    }

    double target =
      (double)centeredA -
      (double)effectiveGain * centeredB;

    if (target > INT32_MAX) target = INT32_MAX;
    if (target < INT32_MIN) target = INT32_MIN;

    samplesTarget[index] = (int32_t)llround(target);
  }
}

void calculateScores() {
  float rmsA = (float)currentFrame.channelA.rms;
  float rmsBAdjusted =
    currentFrame.adaptiveGain *
    (float)currentFrame.channelB.rms;

  float maximumLevel = fmaxf(rmsA, rmsBAdjusted);
  float levelMatch = 0.0f;
  float aDominance = 0.0f;
  float bDominance = 0.0f;
  float aShare = 0.5f;

  if (maximumLevel > 1.0f) {
    levelMatch = 1.0f - fabsf(rmsA - rmsBAdjusted) / maximumLevel;
    aDominance = (rmsA - rmsBAdjusted) / maximumLevel;
    bDominance = (rmsBAdjusted - rmsA) / maximumLevel;
  }

  float totalLevel = rmsA + rmsBAdjusted;
  if (totalLevel > 1.0f) aShare = rmsA / totalLevel;

  levelMatch = clampFloat(levelMatch, 0.0f, 1.0f);
  aDominance = clampFloat(aDominance, 0.0f, 1.0f);
  bDominance = clampFloat(bDominance, 0.0f, 1.0f);
  aShare = clampFloat(aShare, 0.0f, 1.0f);

  float positiveCorrelation = clampFloat(
    currentFrame.correlationAligned,
    0.0f,
    1.0f
  );

  float commonNormalized = positiveCorrelation * levelMatch;

  currentFrame.commonScore = 100.0f * commonNormalized;

  float directionalNormalized =
    0.70f * aDominance +
    0.30f * aShare * (1.0f - currentFrame.correlationMask);

  currentFrame.directionalScore = 100.0f * clampFloat(
    directionalNormalized,
    0.0f,
    1.0f
  );

  float backgroundDominance =
    0.65f * bDominance +
    0.35f * (1.0f - aShare);

  float backgroundNormalized = fmaxf(
    commonNormalized,
    backgroundDominance
  );

  currentFrame.backgroundScore = 100.0f * clampFloat(
    backgroundNormalized,
    0.0f,
    1.0f
  );

  currentFrame.residualRatio =
    rmsA > 1.0f
      ? (float)currentFrame.target.rms / rmsA
      : 0.0f;

  currentFrame.toneRatioAB =
    currentFrame.channelB.tone400Amplitude > 1.0f
      ? currentFrame.channelA.tone400Amplitude /
        currentFrame.channelB.tone400Amplitude
      : 0.0f;

  currentFrame.toneTargetRatio =
    currentFrame.channelA.tone400Amplitude > 1.0f
      ? currentFrame.target.tone400Amplitude /
        currentFrame.channelA.tone400Amplitude
      : 0.0f;
}

bool readMicrophones() {
  size_t bytesRead = 0;

  esp_err_t error = i2s_read(
    I2S_PORT,
    stereoSamples,
    sizeof(stereoSamples),
    &bytesRead,
    pdMS_TO_TICKS(100)
  );

  if (error != ESP_OK || bytesRead == 0) {
    readErrors++;
    return false;
  }

  int stereoWords = bytesRead / sizeof(int32_t);
  int count = stereoWords / 2;

  if (count <= 0) {
    readErrors++;
    return false;
  }

  if (count > READ_SAMPLES) count = READ_SAMPLES;

  for (int index = 0; index < count; index++) {
    int32_t left = stereoSamples[index * 2] >> 8;
    int32_t right = stereoSamples[index * 2 + 1] >> 8;

    if (SWAP_CHANNELS) {
      samplesA[index] = right;
      samplesB[index] = left;
    } else {
      samplesA[index] = left;
      samplesB[index] = right;
    }
  }

  currentFrame.frameNumber = ++frameCounter;
  currentFrame.timeMs = millis();
  currentFrame.count = count;

  analyzeChannel(samplesA, count, currentFrame.channelA);
  analyzeChannel(samplesB, count, currentFrame.channelB);

  currentFrame.correlationRaw = calculateLagCorrelation(
    samplesA,
    samplesB,
    count,
    0,
    currentFrame.channelA.mean,
    currentFrame.channelB.mean
  );

  currentFrame.bestLag = findBestLag(
    samplesA,
    samplesB,
    count,
    currentFrame.channelA.mean,
    currentFrame.channelB.mean,
    currentFrame.correlationAligned
  );

  currentFrame.gainCandidate = calculateGainCandidate(
    samplesA,
    samplesB,
    count,
    currentFrame.bestLag,
    currentFrame.channelA.mean,
    currentFrame.channelB.mean
  );

  currentFrame.rmsRatioAB =
    currentFrame.channelB.rms > 0
      ? (float)currentFrame.channelA.rms /
        (float)currentFrame.channelB.rms
      : 0.0f;

  currentFrame.gainUpdated = false;

  bool levelsReasonable =
    currentFrame.rmsRatioAB >= 0.35f &&
    currentFrame.rmsRatioAB <= 2.85f;

  bool levelIsUseful =
    currentFrame.channelA.rms >= GAIN_UPDATE_MIN_RMS &&
    currentFrame.channelB.rms >= GAIN_UPDATE_MIN_RMS;

  bool noClip =
    currentFrame.channelA.clip == 0 &&
    currentFrame.channelB.clip == 0;

  if (
    currentFrame.correlationAligned >= GAIN_UPDATE_CORRELATION &&
    levelsReasonable &&
    levelIsUseful &&
    noClip
  ) {
    adaptiveGain =
      adaptiveGain * (1.0f - GAIN_SMOOTHING) +
      currentFrame.gainCandidate * GAIN_SMOOTHING;

    adaptiveGain = clampFloat(adaptiveGain, GAIN_MIN, GAIN_MAX);
    currentFrame.gainUpdated = true;
  }

  currentFrame.adaptiveGain = adaptiveGain;
  currentFrame.correlationMask = calculateCorrelationMask(
    currentFrame.correlationAligned
  );

  currentFrame.effectiveGain =
    currentFrame.adaptiveGain *
    currentFrame.correlationMask;

  buildTargetSignal(
    count,
    currentFrame.bestLag,
    currentFrame.channelA.mean,
    currentFrame.channelB.mean,
    currentFrame.effectiveGain
  );

  analyzeChannel(samplesTarget, count, currentFrame.target);
  calculateScores();

  fpsFrameCount++;
  return true;
}

void updateFps() {
  uint32_t now = millis();
  uint32_t elapsed = now - fpsStartedAt;

  if (elapsed < 1000) return;

  currentFps = ((float)fpsFrameCount * 1000.0f) / elapsed;
  fpsFrameCount = 0;
  fpsStartedAt = now;
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char command = (char)Serial.read();

    if (command == 'T' || command == 't') {
      testRunning = true;
      testStartedAt = millis();
      Serial.println("# TEST=STARTED");
      Serial.println("# PHASE_1=0..10s NATURAL_BACKGROUND");
      Serial.println("# PHASE_2=10..20s TONE_400_NEAR_A");
      Serial.println("# PHASE_3=20..30s TONE_400_SIDE");
    }
  }
}

uint32_t getTestElapsedMs() {
  if (!testRunning) return 0;
  return millis() - testStartedAt;
}

const char *getTestPhase() {
  if (!testRunning) return "WAIT";

  uint32_t elapsed = getTestElapsedMs();

  if (elapsed < 10000) return "BACKGROUND";
  if (elapsed < 20000) return "NEAR_A";
  if (elapsed < 30000) return "SIDE";
  return "DONE";
}

void drawStaticInterface() {
  tft.fillScreen(ST77XX_BLACK);

  tft.fillRect(0, 0, 240, 25, ST77XX_RED);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(6, 5);
  tft.print("MASKED RADAR");

  tft.drawRect(0, 25, 240, 42, ST77XX_WHITE);
  tft.drawRect(0, 67, 120, 63, ST77XX_WHITE);
  tft.drawRect(120, 67, 120, 63, ST77XX_WHITE);
  tft.drawRect(0, 130, 240, 72, ST77XX_WHITE);
  tft.drawRect(0, 202, 240, 94, ST77XX_WHITE);

  tft.fillRect(0, 296, 240, 24, ST77XX_RED);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(5, 304);
  tft.print("SEND T: 10s + 10s + 10s");
}

void drawNoData() {
  tft.fillRect(1, 26, 238, 40, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(7, 38);
  tft.print("MIC NO DATA");
}

void drawChannelCell(
  int x,
  int y,
  const char *name,
  const AudioFrame &frame,
  uint16_t color
) {
  char rmsText[18];
  formatCompact(frame.rms, rmsText, sizeof(rmsText));

  tft.fillRect(x + 1, y + 1, 118, 61, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.setCursor(x + 6, y + 5);
  tft.print(name);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(x + 6, y + 30);
  tft.print("RMS ");
  tft.print(rmsText);

  tft.setCursor(x + 6, y + 43);
  tft.print("400 ");
  formatCompact(frame.tone400Amplitude, rmsText, sizeof(rmsText));
  tft.print(rmsText);

  tft.setCursor(x + 6, y + 54);
  tft.print("LVL ");
  tft.print(frame.levelPercent);
  tft.print("%");
}

void drawTargetWave() {
  const int top = 220;
  const int bottom = 287;
  const int middle = (top + bottom) / 2;

  tft.fillRect(1, 203, 238, 92, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(5, 207);
  tft.print("TARGET WAVE");

  tft.drawFastHLine(3, middle, 234, ST77XX_BLUE);

  int previousX = 3;
  int previousY = middle;

  for (int point = 0; point < WAVE_POINTS; point++) {
    int x = 3 + point * 233 / (WAVE_POINTS - 1);
    int y = middle - currentFrame.target.wave[point] * 32 / 1000;
    y = clampInt(y, top, bottom);

    if (point > 0) {
      tft.drawLine(previousX, previousY, x, y, ST77XX_YELLOW);
    }

    previousX = x;
    previousY = y;
  }
}

void updateScreen() {
  tft.fillRect(1, 26, 238, 40, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.setCursor(6, 30);
  tft.print(getTestPhase());

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(6, 44);
  tft.print("TEST ");
  tft.print(getTestElapsedMs() / 1000.0f, 1);
  tft.print("s");

  tft.setCursor(127, 30);
  tft.print("FPS ");
  tft.print(currentFps, 1);

  tft.setCursor(127, 44);
  tft.print("ERR ");
  tft.print(readErrors);

  drawChannelCell(
    0,
    67,
    "A DIR",
    currentFrame.channelA,
    ST77XX_GREEN
  );

  drawChannelCell(
    120,
    67,
    "B OPEN",
    currentFrame.channelB,
    ST77XX_CYAN
  );

  tft.fillRect(1, 131, 238, 70, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(6, 136);
  tft.print("LAG ");
  tft.print(currentFrame.bestLag);
  tft.print(" CORR ");
  tft.print(currentFrame.correlationAligned, 3);

  tft.setCursor(6, 150);
  tft.print("K ");
  tft.print(currentFrame.adaptiveGain, 3);
  tft.print(" EFF ");
  tft.print(currentFrame.effectiveGain, 3);

  tft.setCursor(6, 164);
  tft.print("MASK ");
  tft.print(currentFrame.correlationMask, 2);
  tft.print(" COMMON ");
  tft.print(currentFrame.commonScore, 0);

  tft.setCursor(6, 178);
  tft.print("DIR ");
  tft.print(currentFrame.directionalScore, 0);
  tft.print(" BACK ");
  tft.print(currentFrame.backgroundScore, 0);

  tft.setCursor(6, 191);
  tft.print("TARGET ");
  tft.print(currentFrame.target.rms);
  tft.print(" 400 ");
  tft.print(currentFrame.target.tone400Amplitude, 0);

  drawTargetWave();
}

void printFrameLine(const char *prefix, const AudioFrame &frame) {
  Serial.printf(
    "%s T_MS=%lu ID=%lu COUNT=%d MEAN=%ld MIN=%ld MAX=%ld RMS=%ld PEAK=%ld P2P=%ld LEVEL=%d CLIP=%d TONE400_AMP=%.1f TONE400_LEVEL=%u DOM_HZ=%.1f DOM_AMP=%.1f SPEC_MAX=%.1f READ_ERR=%lu FPS=%.1f\n",
    prefix,
    (unsigned long)currentFrame.timeMs,
    (unsigned long)currentFrame.frameNumber,
    currentFrame.count,
    (long)frame.mean,
    (long)frame.minimum,
    (long)frame.maximum,
    (long)frame.rms,
    (long)frame.peak,
    (long)frame.p2p,
    frame.levelPercent,
    frame.clip,
    frame.tone400Amplitude,
    frame.tone400Level,
    frame.dominantFrequency,
    frame.dominantAmplitude,
    frame.spectrumMaximum,
    (unsigned long)readErrors,
    currentFps
  );
}

void printWaveLine(const char *prefix, const AudioFrame &frame) {
  Serial.printf(
    "%s T_MS=%lu DATA=",
    prefix,
    (unsigned long)currentFrame.timeMs
  );

  for (int point = 0; point < WAVE_POINTS; point++) {
    if (point > 0) Serial.print(",");
    Serial.print(frame.wave[point]);
  }

  Serial.println();
}

void printSpectrumLine(const char *prefix, const AudioFrame &frame) {
  Serial.printf(
    "%s T_MS=%lu FIRST_HZ=%.1f STEP_HZ=%.1f SCALE=ABS_LOG DATA=",
    prefix,
    (unsigned long)currentFrame.timeMs,
    SPECTRUM_FIRST_HZ,
    SPECTRUM_STEP_HZ
  );

  for (int band = 0; band < SPECTRUM_BANDS; band++) {
    if (band > 0) Serial.print(",");
    Serial.print(frame.spectrum[band]);
  }

  Serial.println();
}

void printSerialFrame() {
  printFrameLine("FRAME_A", currentFrame.channelA);
  printFrameLine("FRAME_B", currentFrame.channelB);
  printFrameLine("FRAME_TARGET", currentFrame.target);

  Serial.printf(
    "COMPARE T_MS=%lu ID=%lu TEST_MS=%lu TEST_PHASE=%s BEST_LAG=%d CORR_RAW=%.6f CORR_ALIGNED=%.6f CORR_MASK=%.6f GAIN_CANDIDATE=%.6f GAIN_K=%.6f EFFECTIVE_K=%.6f GAIN_UPDATED=%d RMS_RATIO_AB=%.6f RESIDUAL_RATIO=%.6f TONE400_RATIO_AB=%.6f TONE400_TARGET_RATIO=%.6f COMMON_SCORE=%.3f DIRECTIONAL_SCORE=%.3f BACKGROUND_SCORE=%.3f\n",
    (unsigned long)currentFrame.timeMs,
    (unsigned long)currentFrame.frameNumber,
    (unsigned long)getTestElapsedMs(),
    getTestPhase(),
    currentFrame.bestLag,
    currentFrame.correlationRaw,
    currentFrame.correlationAligned,
    currentFrame.correlationMask,
    currentFrame.gainCandidate,
    currentFrame.adaptiveGain,
    currentFrame.effectiveGain,
    currentFrame.gainUpdated ? 1 : 0,
    currentFrame.rmsRatioAB,
    currentFrame.residualRatio,
    currentFrame.toneRatioAB,
    currentFrame.toneTargetRatio,
    currentFrame.commonScore,
    currentFrame.directionalScore,
    currentFrame.backgroundScore
  );

  printWaveLine("WAVE_A", currentFrame.channelA);
  printWaveLine("WAVE_B", currentFrame.channelB);
  printWaveLine("WAVE_TARGET", currentFrame.target);

  printSpectrumLine("SPECTRUM_A", currentFrame.channelA);
  printSpectrumLine("SPECTRUM_B", currentFrame.channelB);
  printSpectrumLine("SPECTRUM_TARGET", currentFrame.target);

  Serial.println("---");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("# ESP32 RADAR MASKED ADAPTIVE TWO MICROPHONE");
  Serial.println("# CHANNEL_A=LEFT DIRECTIONAL");
  Serial.println("# CHANNEL_B=RIGHT OPEN_BACKGROUND");
  Serial.println("# TARGET=A-EFFECTIVE_K*ALIGNED_B");
  Serial.println("# TEST_TONE_HZ=400.0");
  Serial.println("# SEND_T_TO_START_30_SECOND_TEST");
  Serial.println("# 0..10s=BACKGROUND 10..20s=NEAR_A 20..30s=SIDE");

  Serial.printf(
    "# SAMPLE_RATE=%d READ_SAMPLES=%d MAX_LAG=%d GAIN_START=%.3f MASK_LOW=%.2f MASK_HIGH=%.2f\n",
    SAMPLE_RATE,
    READ_SAMPLES,
    MAX_LAG_SAMPLES,
    adaptiveGain,
    MASK_CORRELATION_LOW,
    MASK_CORRELATION_HIGH
  );

  SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
  tft.init(240, 320);
  tft.setRotation(0);

  drawStaticInterface();
  drawNoData();

  micReady = setupI2S();

  if (micReady) {
    Serial.println("# MIC=STEREO_I2S_READY");
    Serial.println("# MIC=WARMUP");
    warmUpMicrophones();
    Serial.println("# MIC=READY");
  } else {
    Serial.println("# MIC=I2S_ERROR");
  }

  fpsStartedAt = millis();
}

void loop() {
  handleSerialCommands();

  if (!micReady) {
    drawNoData();
    delay(500);
    return;
  }

  bool frameOk = readMicrophones();
  updateFps();

  if (!frameOk) return;

  if (currentFrame.frameNumber % SERIAL_EVERY_FRAMES == 0) {
    printSerialFrame();
  }

  if (currentFrame.frameNumber % TFT_EVERY_FRAMES == 0) {
    updateScreen();
  }
}
