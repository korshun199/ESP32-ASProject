/*
  ESP32: один реальный микрофон через USB Serial

  Назначение:
    читать один аналоговый микрофон на GPIO34
    считать min/max/center/peak-to-peak/volume
    печатать JSON в USB Serial

  Подключение:
    OUT микрофона  -> GPIO34
    VCC усилителя  -> 3.3V
    GND усилителя  -> GND

  Wi-Fi не используется.
*/

#include <Arduino.h>

const int MIC1_PIN = 34;

// 20 ms примерно соответствует одному короткому аудио-окну.
// Для первого теста достаточно.
const unsigned long SAMPLE_WINDOW_US = 20000;

// Задержка между analogRead.
// 60 us примерно даёт до ~16 kHz теоретически, реально меньше.
const int SAMPLE_DELAY_US = 60;

// Сколько раз в секунду отдаём JSON.
// 10 Hz достаточно для радара и не душит T16.
const unsigned long SEND_INTERVAL_MS = 100;

// Подстройка чувствительности.
// Если volume слишком маленький — уменьшить, например до 300.
// Если сразу 1.000 — увеличить, например до 1500.
const float MIC_P2P_FULL_SCALE = 900.0;

unsigned long seq = 0;
unsigned long lastSendMs = 0;

struct MicMeasure {
  int raw;
  int rawMin;
  int rawMax;
  int center;
  int peakToPeak;
  float volume;
  unsigned long samples;
};

float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

MicMeasure measureMic() {
  MicMeasure m;
  m.raw = 0;
  m.rawMin = 4095;
  m.rawMax = 0;
  m.center = 0;
  m.peakToPeak = 0;
  m.volume = 0.0;
  m.samples = 0;

  unsigned long sum = 0;
  unsigned long startUs = micros();

  while ((micros() - startUs) < SAMPLE_WINDOW_US) {
    int v = analogRead(MIC1_PIN);

    if (v < m.rawMin) m.rawMin = v;
    if (v > m.rawMax) m.rawMax = v;

    sum += v;
    m.raw = v;
    m.samples++;

    delayMicroseconds(SAMPLE_DELAY_US);
  }

  if (m.samples > 0) {
    m.center = (int)(sum / m.samples);
  }

  m.peakToPeak = m.rawMax - m.rawMin;
  m.volume = clampFloat((float)m.peakToPeak / MIC_P2P_FULL_SCALE, 0.0, 1.0);

  return m;
}

void printJson(const MicMeasure& m) {
  seq++;

  Serial.print("{");
  Serial.print("\"seq\":");
  Serial.print(seq);
  Serial.print(",");
  Serial.print("\"source\":\"esp32_single_real_mic_serial\",");
  Serial.print("\"mic\":{");
  Serial.print("\"id\":1,");
  Serial.print("\"name\":\"MIC1 ВЕРХ РЕАЛЬНЫЙ GPIO34\",");
  Serial.print("\"pin\":34,");
  Serial.print("\"raw\":");
  Serial.print(m.raw);
  Serial.print(",");
  Serial.print("\"min\":");
  Serial.print(m.rawMin);
  Serial.print(",");
  Serial.print("\"max\":");
  Serial.print(m.rawMax);
  Serial.print(",");
  Serial.print("\"center\":");
  Serial.print(m.center);
  Serial.print(",");
  Serial.print("\"p2p\":");
  Serial.print(m.peakToPeak);
  Serial.print(",");
  Serial.print("\"volume\":");
  Serial.print(m.volume, 4);
  Serial.print(",");
  Serial.print("\"samples\":");
  Serial.print(m.samples);
  Serial.print("},");

  // Сразу даём формат, совместимый с /api/esp32/push.
  Serial.print("\"mics\":[");
  Serial.print("{\"id\":1,\"name\":\"MIC1 ВЕРХ РЕАЛЬНЫЙ GPIO34\",\"raw\":");
  Serial.print(m.raw);
  Serial.print(",\"volume\":");
  Serial.print(m.volume, 4);
  Serial.print("},");
  Serial.print("{\"id\":2,\"name\":\"MIC2 ПРАВО\",\"raw\":0,\"volume\":0.0},");
  Serial.print("{\"id\":3,\"name\":\"MIC3 НИЗ\",\"raw\":0,\"volume\":0.0},");
  Serial.print("{\"id\":4,\"name\":\"MIC4 ЛЕВО\",\"raw\":0,\"volume\":0.0}");
  Serial.print("],");

  Serial.print("\"audio\":{");
  Serial.print("\"volume\":");
  Serial.print(m.volume, 4);
  Serial.print(",");
  Serial.print("\"freq\":0");
  Serial.print("},");

  if (m.volume > 0.03) {
    // Один микрофон не умеет честно давать направление.
    // Пока считаем MIC1 как верхний сектор.
    Serial.print("\"object\":{\"visible\":true,\"x\":0.5,\"y\":0.12}");
  } else {
    Serial.print("\"object\":null");
  }

  Serial.println("}");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(MIC1_PIN, ADC_11db);

  Serial.println("# ESP32: прошивка одного реального микрофона запущена");
  Serial.println("# OUT -> GPIO34, VCC -> 3.3V или защищённый выход усилителя, GND -> GND");
  Serial.println("# Ниже идут строки JSON");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;

    MicMeasure m = measureMic();
    printJson(m);
  }
}
