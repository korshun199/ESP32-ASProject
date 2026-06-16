/*
  ESP32-ASProject
  Three microphone point emulator

  Hardware:
    AUDIO OUT:
      GPIO25 / D25 -> resistor 1k -> headphone/speaker
      GND          -> headphone/speaker second wire

    Virtual microphones:
      MIC1 -> D34 / GPIO34
      MIC2 -> D35 / GPIO35
      MIC3 -> VP  / GPIO36

    Each potentiometer:
      edge pin   -> GND
      middle pin -> MIC input
      edge pin   -> 3.3V

  Output format:
    FREQ: M1=...Hz | M2=...Hz | M3=...Hz
    VOL : M1=...   | M2=...   | M3=...
    POINT: X=... Y=...
    DIR: ...
    GRID: ASCII coordinate field
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

const int GRID_W = 21;
const int GRID_H = 11;

unsigned long lastPrintMs = 0;
const unsigned long PRINT_INTERVAL_MS = 250;

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int normalizeAdcToPoint(int value) {
  value = clampInt(value, -4095, 4095);
  return map(value, -4095, 4095, -100, 100);
}

void readMicrophoneModel() {
  for (int i = 0; i < MIC_COUNT; i++) {
    int raw = analogRead(MIC_PINS[i]);

    micVolume[i] = raw;

    /*
      Educational model:
      For now one potentiometer changes both:
        - virtual volume
        - virtual frequency

      Later we can split it:
        - one ADC set for volume
        - another source for frequency
    */
    micFreq[i] = map(raw, 0, 4095, FREQ_MIN, FREQ_MAX);
  }
}

int getPointX() {
  int diff = micVolume[1] - micVolume[0];
  return normalizeAdcToPoint(diff);
}

int getPointY() {
  int base = (micVolume[0] + micVolume[1]) / 2;
  int diff = micVolume[2] - base;
  return normalizeAdcToPoint(diff);
}

int getMainToneFreq() {
  /*
    Sound output follows the strongest virtual microphone.
    This gives audible feedback while turning resistors.
  */
  int strongestIndex = 0;

  for (int i = 1; i < MIC_COUNT; i++) {
    if (micVolume[i] > micVolume[strongestIndex]) {
      strongestIndex = i;
    }
  }

  return micFreq[strongestIndex];
}

void printFreqLine() {
  Serial.print("FREQ: ");
  Serial.print("M1=");
  Serial.print(micFreq[0]);
  Serial.print("Hz | M2=");
  Serial.print(micFreq[1]);
  Serial.print("Hz | M3=");
  Serial.print(micFreq[2]);
  Serial.println("Hz");
}

void printVolumeLine() {
  Serial.print("VOL : ");
  Serial.print("M1=");
  Serial.print(micVolume[0]);
  Serial.print("  | M2=");
  Serial.print(micVolume[1]);
  Serial.print("  | M3=");
  Serial.println(micVolume[2]);
}

void printDirection(int x, int y) {
  Serial.print("DIR: ");

  if (x < -20) {
    Serial.print("LEFT ");
  } else if (x > 20) {
    Serial.print("RIGHT ");
  } else {
    Serial.print("CENTER-X ");
  }

  if (y < -20) {
    Serial.print("BACK");
  } else if (y > 20) {
    Serial.print("FRONT");
  } else {
    Serial.print("CENTER-Y");
  }

  Serial.println();
}

void printGrid(int x, int y) {
  int pointCol = map(x, -100, 100, 0, GRID_W - 1);
  int pointRow = map(y, 100, -100, 0, GRID_H - 1);

  int centerCol = GRID_W / 2;
  int centerRow = GRID_H / 2;

  pointCol = clampInt(pointCol, 0, GRID_W - 1);
  pointRow = clampInt(pointRow, 0, GRID_H - 1);

  Serial.println("GRID:");

  for (int row = 0; row < GRID_H; row++) {
    for (int col = 0; col < GRID_W; col++) {
      if (row == pointRow && col == pointCol) {
        Serial.print("*");
      } else if (row == centerRow && col == centerCol) {
        Serial.print("+");
      } else if (row == centerRow || col == centerCol) {
        Serial.print(".");
      } else {
        Serial.print(".");
      }
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);

  ledcAttachChannel(AUDIO_PIN, 440, PWM_RESOLUTION, PWM_CHANNEL);
  ledcWrite(AUDIO_PIN, 128);
  ledcWriteTone(AUDIO_PIN, 440);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 three microphone point emulator");
  Serial.println("MIC1 = D34 / GPIO34");
  Serial.println("MIC2 = D35 / GPIO35");
  Serial.println("MIC3 = VP  / GPIO36");
  Serial.println("AUDIO = D25 / GPIO25");
  Serial.println("================================");
}

void loop() {
  readMicrophoneModel();

  int x = getPointX();
  int y = getPointY();

  int mainTone = getMainToneFreq();
  ledcWriteTone(AUDIO_PIN, mainTone);
  ledcWrite(AUDIO_PIN, 128);

  unsigned long now = millis();

  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    lastPrintMs = now;

    Serial.println();
    Serial.println("==============================");
    printFreqLine();
    printVolumeLine();

    Serial.print("POINT: X=");
    if (x >= 0) Serial.print("+");
    Serial.print(x);

    Serial.print(" Y=");
    if (y >= 0) Serial.print("+");
    Serial.println(y);

    printDirection(x, y);
    printGrid(x, y);
  }
}
