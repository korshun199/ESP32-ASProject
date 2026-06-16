/*
  ESP32-ASProject
  Three microphone JSON serial emitter

  Purpose:
    ESP32 reads three potentiometers as virtual microphone data
    and sends structured JSON lines to PC over Serial.

  AUDIO:
    D25 / GPIO25 -> resistor 1k -> headphone/speaker
    GND          -> headphone/speaker second wire

  Virtual microphones:
    MIC1 -> D34 / GPIO34
    MIC2 -> D35 / GPIO35
    MIC3 -> VP  / GPIO36

  Potentiometer:
    edge pin   -> GND
    middle pin -> MIC input
    edge pin   -> 3.3V

  Serial output:
    DATA:{"freq":[...],"vol":[...],"x":...,"y":...}
*/

const int AUDIO_PIN = 25;

const int MIC_COUNT = 3;
const int MIC_PINS[MIC_COUNT] = {34, 35, 36};

int micVolume[MIC_COUNT] = {0, 0, 0};
int micFreq[MIC_COUNT] = {0, 0, 0};

const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8;

const int FREQ_MIN = 100;
const int FREQ_MAX = 800;

unsigned long lastSendMs = 0;
const unsigned long SEND_INTERVAL_MS = 80;

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int normalizeAdcDiffToPoint(int value) {
  value = clampInt(value, -4095, 4095);
  return map(value, -4095, 4095, -100, 100);
}

void readMicrophoneModel() {
  for (int i = 0; i < MIC_COUNT; i++) {
    int raw = analogRead(MIC_PINS[i]);

    micVolume[i] = raw;
    micFreq[i] = map(raw, 0, 4095, FREQ_MIN, FREQ_MAX);
  }
}

int getPointX() {
  int diff = micVolume[1] - micVolume[0];
  return normalizeAdcDiffToPoint(diff);
}

int getPointY() {
  int base = (micVolume[0] + micVolume[1]) / 2;
  int diff = micVolume[2] - base;
  return normalizeAdcDiffToPoint(diff);
}

int getMainToneFreq() {
  int strongestIndex = 0;

  for (int i = 1; i < MIC_COUNT; i++) {
    if (micVolume[i] > micVolume[strongestIndex]) {
      strongestIndex = i;
    }
  }

  return micFreq[strongestIndex];
}

void sendJsonFrame(int x, int y) {
  Serial.print("DATA:{\"freq\":[");
  Serial.print(micFreq[0]);
  Serial.print(",");
  Serial.print(micFreq[1]);
  Serial.print(",");
  Serial.print(micFreq[2]);

  Serial.print("],\"vol\":[");
  Serial.print(micVolume[0]);
  Serial.print(",");
  Serial.print(micVolume[1]);
  Serial.print(",");
  Serial.print(micVolume[2]);

  Serial.print("],\"x\":");
  Serial.print(x);

  Serial.print(",\"y\":");
  Serial.print(y);

  Serial.println("}");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  analogReadResolution(12);

  ledcAttachChannel(AUDIO_PIN, 440, PWM_RESOLUTION, PWM_CHANNEL);
  ledcWrite(AUDIO_PIN, 128);
  ledcWriteTone(AUDIO_PIN, 440);

  Serial.println("INFO: ESP32 three mic JSON emitter started");
  Serial.println("INFO: MIC1=D34 MIC2=D35 MIC3=VP/GPIO36 AUDIO=D25");
}

void loop() {
  readMicrophoneModel();

  int x = getPointX();
  int y = getPointY();

  int mainTone = getMainToneFreq();
  ledcWriteTone(AUDIO_PIN, mainTone);
  ledcWrite(AUDIO_PIN, 128);

  unsigned long now = millis();

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    sendJsonFrame(x, y);
  }
}
