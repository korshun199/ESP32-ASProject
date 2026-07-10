/*
  SIMONG RADAR 2026
  ESP32-S3 + TFT

  Чистая базовая разметка экрана.

  Координаты:
    X=0, Y=0 — левый верхний угол.
    X растёт вправо.
    Y растёт вниз.
    Экран смотрим альбомно.

  Экран:
    общая рабочая логика: 320 x 240

  Левая зона:
    x = 0
    y = 0
    w = 240
    h = 240

  Правая зона:
    x = 240
    y = 0
    w = 80
    h = 240

  Блоки слева:
    HEADER: x=0, y=0,   w=240, h=40
    RADAR:  x=0, y=40,  w=240, h=160
    FOOTER: x=0, y=200, w=240, h=40

  USB:
    Прошивка: второй USB
    Монитор: первый USB
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ============================================================
// ПИНЫ TFT
// ============================================================

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12

// ============================================================
// ОРИЕНТАЦИЯ
// ============================================================

static const uint8_t TFT_ROTATION = 0;

// ============================================================
// ЦВЕТА
// ============================================================

static const uint16_t COLOR_SCREEN_BG    = ILI9341_BLACK;

static const uint16_t COLOR_HEADER_BG    = ILI9341_RED;
static const uint16_t COLOR_HEADER_TEXT  = ILI9341_WHITE;

static const uint16_t COLOR_RADAR_BG     = ILI9341_BLACK;
static const uint16_t COLOR_RADAR_BORDER = ILI9341_DARKGREY;
static const uint16_t COLOR_RADAR_TEXT   = ILI9341_WHITE;

static const uint16_t COLOR_FOOTER_BG    = ILI9341_RED;
static const uint16_t COLOR_FOOTER_TEXT  = ILI9341_WHITE;

static const uint16_t COLOR_RIGHT_BG     = ILI9341_BLACK;
static const uint16_t COLOR_RIGHT_BORDER = ILI9341_DARKGREY;
static const uint16_t COLOR_RIGHT_TEXT   = ILI9341_WHITE;
static const uint16_t COLOR_RIGHT_VALUE  = ILI9341_GREEN;

// ============================================================
// ГЕОМЕТРИЯ ЭКРАНА
// ============================================================

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

// Левая рабочая колонка.
static const int LEFT_X = 0;
static const int LEFT_Y = 0;
static const int LEFT_W = 240;
static const int LEFT_H = 240;

// Правая служебная колонка.
static const int RIGHT_X = 240;
static const int RIGHT_Y = 0;
static const int RIGHT_W = 80;
static const int RIGHT_H = 240;

// ============================================================
// БЛОК 1. HEADER
// ============================================================

static const int HEADER_X = LEFT_X;
static const int HEADER_Y = 0;
static const int HEADER_W = LEFT_W;
static const int HEADER_H = 40;

static const int HEADER_TEXT_X = 8;
static const int HEADER_TEXT_Y = 12;
static const int HEADER_TEXT_SIZE = 2;

static const char HEADER_TEXT[] = "SIMONG RADAR 2026";

// ============================================================
// БЛОК 2. RADAR
// ============================================================

static const int RADAR_X = LEFT_X;
static const int RADAR_Y = HEADER_Y + HEADER_H;
static const int RADAR_W = LEFT_W;
static const int RADAR_H = 160;

static const int RADAR_TEXT_X = 8;
static const int RADAR_TEXT_Y = 8;
static const int RADAR_TEXT_SIZE = 2;

static const char RADAR_TEXT[] = "RADAR";

// ============================================================
// БЛОК 3. FOOTER
// ============================================================

static const int FOOTER_X = LEFT_X;
static const int FOOTER_Y = RADAR_Y + RADAR_H;
static const int FOOTER_W = LEFT_W;
static const int FOOTER_H = 40;

static const int FOOTER_TEXT_X = 8;
static const int FOOTER_TEXT_Y = 12;
static const int FOOTER_TEXT_SIZE = 2;

static const char FOOTER_TEXT[] = "FOOTER";

// ============================================================
// БЛОК 4. RIGHT PANEL
// ============================================================

static const int RIGHT_PANEL_X = RIGHT_X;
static const int RIGHT_PANEL_Y = RIGHT_Y;
static const int RIGHT_PANEL_W = RIGHT_W;
static const int RIGHT_PANEL_H = RIGHT_H;

// ============================================================
// ОБЪЕКТ TFT
// ============================================================

Adafruit_ILI9341 tft = Adafruit_ILI9341(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

// ============================================================
// SERIAL
// ============================================================

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

// ============================================================
// РИСОВАНИЕ БЛОКОВ
// ============================================================

void drawHeaderBlock() {
  tft.fillRect(
    HEADER_X,
    HEADER_Y,
    HEADER_W,
    HEADER_H,
    COLOR_HEADER_BG
  );

  tft.setTextSize(HEADER_TEXT_SIZE);
  tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
  tft.setCursor(HEADER_X + HEADER_TEXT_X, HEADER_Y + HEADER_TEXT_Y);
  tft.print(HEADER_TEXT);
}

void drawRadarBlock() {
  tft.fillRect(
    RADAR_X,
    RADAR_Y,
    RADAR_W,
    RADAR_H,
    COLOR_RADAR_BG
  );

  tft.drawRect(
    RADAR_X,
    RADAR_Y,
    RADAR_W,
    RADAR_H,
    COLOR_RADAR_BORDER
  );

  tft.setTextSize(RADAR_TEXT_SIZE);
  tft.setTextColor(COLOR_RADAR_TEXT, COLOR_RADAR_BG);
  tft.setCursor(RADAR_X + RADAR_TEXT_X, RADAR_Y + RADAR_TEXT_Y);
  tft.print(RADAR_TEXT);
}

void drawFooterBlock() {
  tft.fillRect(
    FOOTER_X,
    FOOTER_Y,
    FOOTER_W,
    FOOTER_H,
    COLOR_FOOTER_BG
  );

  tft.setTextSize(FOOTER_TEXT_SIZE);
  tft.setTextColor(COLOR_FOOTER_TEXT, COLOR_FOOTER_BG);
  tft.setCursor(FOOTER_X + FOOTER_TEXT_X, FOOTER_Y + FOOTER_TEXT_Y);
  tft.print(FOOTER_TEXT);
}

void drawRightPanelBlock() {
  tft.fillRect(
    RIGHT_PANEL_X,
    RIGHT_PANEL_Y,
    RIGHT_PANEL_W,
    RIGHT_PANEL_H,
    COLOR_RIGHT_BG
  );

  tft.drawRect(
    RIGHT_PANEL_X,
    RIGHT_PANEL_Y,
    RIGHT_PANEL_W,
    RIGHT_PANEL_H,
    COLOR_RIGHT_BORDER
  );

  tft.setTextSize(2);
  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 12, RIGHT_PANEL_Y + 30);
  tft.print("SYS");

  tft.drawLine(
    RIGHT_PANEL_X + 4,
    RIGHT_PANEL_Y + 40,
    RIGHT_PANEL_X + RIGHT_PANEL_W - 5,
    RIGHT_PANEL_Y + 40,
    COLOR_RIGHT_BORDER
  );

  tft.setTextSize(1);

  int y = RIGHT_PANEL_Y + 57;

  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 8, y);
  tft.print("MIC");
  tft.setTextColor(COLOR_RIGHT_VALUE, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 42, y);
  tft.print("WAIT");

  y += 38;

  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 8, y);
  tft.print("WIFI");
  tft.setTextColor(COLOR_RIGHT_VALUE, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 42, y);
  tft.print("OFF");

  y += 38;

  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 8, y);
  tft.print("FPS");
  tft.setTextColor(COLOR_RIGHT_VALUE, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 42, y);
  tft.print("--");

  y += 38;

  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 8, y);
  tft.print("USB");
  tft.setTextColor(COLOR_RIGHT_VALUE, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 42, y);
  tft.print("OK");

  tft.setTextColor(COLOR_RIGHT_TEXT, COLOR_RIGHT_BG);
  tft.setCursor(RIGHT_PANEL_X + 8, RIGHT_PANEL_Y + RIGHT_PANEL_H - 18);
  tft.print("S3");
}

// ============================================================
// ПОЛНАЯ ОТРИСОВКА ЭКРАНА
// ============================================================

void drawScreen() {
  // Сначала полностью стираем экран.
  // Это важно: никаких старых слоёв, никаких наложений.
  tft.fillScreen(COLOR_SCREEN_BG);

  // Потом рисуем каждый блок ровно один раз.
  drawHeaderBlock();
  //drawRadarBlock();
  drawFooterBlock();
  drawRightPanelBlock();
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  delay(1200);

  printBoth("");
  printBoth("# SIMONG RADAR 2026");
  printBoth("# Clean block layout");
  printBoth("# Upload USB2 / Monitor USB1");

  tft.begin();
  tft.setRotation(TFT_ROTATION);

  printBoth(String("# rotation=") + String(TFT_ROTATION));
  printBoth(String("# screen=") + String(tft.width()) + "x" + String(tft.height()));

  drawScreen();

  printBoth("# screen drawn");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Пока экран статический.
  
}
