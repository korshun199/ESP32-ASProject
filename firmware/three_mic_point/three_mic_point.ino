/*
  ESP32-ASProject
  Three microphone point emulator with stable terminal screen

  AUDIO:
    D25 / GPIO25 -> resistor 1k -> headphone/speaker
    GND          -> headphone/speaker second wire

  Virtual microphones:
    MIC1 -> D34 / GPIO34
    MIC2 -> D35 / GPIO35
    MIC3 -> VP  / GPIO36

  Potentiometer connection:
    edge pin   -> GND
    middle pin -> MIC input
    edge pin   -> 3.3V

  Terminal output:
    FREQ line
    VOL line
    POINT line
    DIR line
    ASCII grid
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
const unsigned long PRINT_INTERVAL_MS = 300;

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

int normalizeAdcToPoint(int value) {
  value = clampInt(value, -4095, 4095);
  return map(value, -4095, 4095, -100, 100);
}

void clearScreenOnce() {
  Serial.print("\033[2J");
  Serial.print("\033[H");
}

void cursorHome() {
  Serial.print("\033[H");
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
  return normalizeAdcToPoint(diff);
}

int getPointY() {
  int base = (micVolume[0] + micVolume[1]) / 2;
  int diff = micVolume[2] - base;
  return normalizeAdcToPoint(diff);
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

void printFixedWidthInt(int value, int width) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%*d", width, value);
  Serial.print(buf);
}

void printSignedInt(int value, int width) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%+*d", width, value);
  Serial.print(buf);
}

void printFreqLine() {
  Serial.print("FREQ: ");
  Serial.print("M1=");
  printFixedWidthInt(micFreq[0], 4);
  Serial.print("Hz | M2=");
  printFixedWidthInt(micFreq[1], 4);
  Serial.print("Hz | M3=");
  printFixedWidthInt(micFreq[2], 4);
  Serial.println("Hz     ");
}

void printVolumeLine() {
  Serial.print("VOL : ");
  Serial.print("M1=");
  printFixedWidthInt(micVolume[0], 4);
  Serial.print("   | M2=");
  printFixedWidthInt(micVolume[1], 4);
  Serial.print("   | M3=");
  printFixedWidthInt(micVolume[2], 4);
  Serial.println("       ");
}

void printDirection(int x, int y) {
  Serial.print("DIR : ");

  if (x < -20) {
    Serial.print("LEFT     ");
  } else if (x > 20) {
    Serial.print("RIGHT    ");
  } else {
    Serial.print("CENTER-X ");
  }

  Serial.print(" / ");

  if (y < -20) {
    Serial.print("BACK     ");
  } else if (y > 20) {
    Serial.print("FRONT    ");
  } else {
    Serial.print("CENTER-Y ");
  }

  Serial.println("       ");
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
      } else if (row == centerRow) {
        Serial.print("-");
      } else if (col == centerCol) {
        Serial.print("|");
      } else {
        Serial.print(".");
      }
    }
    Serial.println("   ");
  }
}

void printScreen(int x, int y, int mainTone) {
  cursorHome();

  Serial.println("ESP32 THREE MIC POINT EMULATOR        ");
  Serial.println("MIC1=D34  MIC2=D35  MIC3=VP/GPIO36   ");
  Serial.println("AUDIO=D25                              ");
  Serial.println("--------------------------------------");

  printFreqLine();
  printVolumeLine();

  Serial.print("POINT: X=");
  printSignedInt(x, 4);
  Serial.print("  Y=");
  printSignedInt(y, 4);
  Serial.print("  MAIN_TONE=");
  printFixedWidthInt(mainTone, 4);
  Serial.println("Hz       ");

  printDirection(x, y);
  Serial.println("--------------------------------------");

  printGrid(x, y);

  Serial.println("--------------------------------------");
  Serial.println("Turn potentiometers. Ctrl+C to exit.  ");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  analogReadResolution(12);

  ledcAttachChannel(AUDIO_PIN, 440, PWM_RESOLUTION, PWM_CHANNEL);
  ledcWrite(AUDIO_PIN, 128);
  ledcWriteTone(AUDIO_PIN, 440);

  clearScreenOnce();

  Serial.println("Starting...");
  delay(500);
  clearScreenOnce();
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
    printScreen(x, y, mainTone);
  }
}
