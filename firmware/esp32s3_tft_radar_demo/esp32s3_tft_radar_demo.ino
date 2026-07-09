/*
  SIMONG RADAR 2026
  Экран ESP32-S3 + TFT

  Этап:
    Блок 1. Заголовок.

  Координатная система, как смотрит Олежка:
    X=0, Y=0 — левый верхний угол.
    X растёт вправо.
    Y растёт вниз.
    Экран смотрим альбомно.

  Блок 1:
    x      = 0
    y      = 0
    width  = 240
    height = 40
    фон    = голубой
    текст  = белый
    текст  = SIMONG RADAR 2026

  Подключение TFT:
    VCC    -> 3V3
    GND    -> GND
    CS     -> GPIO10
    RESET  -> GPIO8
    DC     -> GPIO9
    SDI    -> GPIO11
    SDK    -> GPIO12
    LED    -> 3V3

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
// НАСТРОЙКА ОРИЕНТАЦИИ
// ============================================================

// По результату ручной проверки нормальная базовая ориентация была ROT 0.
// Пока оставляем её. Если физически надо будет повернуть, поменяем только тут.
static const uint8_t TFT_ROTATION = 0;

// ============================================================
// ЦВЕТА
// ============================================================

static const uint16_t COLOR_SCREEN_BG    = ILI9341_BLACK;
static const uint16_t COLOR_HEADER_BG    = ILI9341_BLUE;
static const uint16_t COLOR_HEADER_TEXT  = ILI9341_WHITE;

// ============================================================
// БЛОК 1. ЗАГОЛОВОК
// ============================================================

static const int HEADER_X = 0;
static const int HEADER_Y = 0;
static const int HEADER_W = 240;
static const int HEADER_H = 40;

static const int HEADER_TEXT_X = 8;
static const int HEADER_TEXT_Y = 12;
static const int HEADER_TEXT_SIZE = 2;

static const char HEADER_TEXT[] = "SIMONG RADAR 2026";

// ============================================================
// БЛОК 2. RADAR
// ============================================================

static const int RADAR_X = 0;
static const int RADAR_Y = HEADER_Y + HEADER_H;
static const int RADAR_W = 240;
static const int RADAR_H = 160;

static const int RADAR_TEXT_X = 8;
static const int RADAR_TEXT_Y = 8;
static const int RADAR_TEXT_SIZE = 2;

static const uint16_t COLOR_RADAR_BG = ILI9341_BLACK;
static const uint16_t COLOR_RADAR_BORDER = ILI9341_DARKGREY;
static const uint16_t COLOR_RADAR_TEXT = ILI9341_WHITE;

static const char RADAR_TEXT[] = "RADAR";

// ============================================================
// БЛОК 3. FOOTER
// ============================================================

static const int FOOTER_X = 0;
static const int FOOTER_Y = RADAR_Y + RADAR_H;
static const int FOOTER_W = 240;
static const int FOOTER_H = 40;

// ============================================================
// ПРАВАЯ СВОБОДНАЯ ЗОНА
// ============================================================
//
// Сейчас она специально не занята.
// Это оставшиеся 25% ширины экрана:
//
//   x      = 240
//   y      = 0
//   width  = 80
//   height = 240
//
// Позже сюда можно поставить статус, индикаторы, шкалу микрофона,
// батарею, Wi-Fi, USB-режим или маленькую панель телеметрии.
static const int RIGHT_FREE_X = 240;
static const int RIGHT_FREE_Y = 0;
static const int RIGHT_FREE_W = 80;
static const int RIGHT_FREE_H = 240;


static const int FOOTER_TEXT_X = 8;
static const int FOOTER_TEXT_Y = 12;
static const int FOOTER_TEXT_SIZE = 2;

static const uint16_t COLOR_FOOTER_BG = COLOR_HEADER_BG;
static const uint16_t COLOR_FOOTER_TEXT = ILI9341_WHITE;

static const char FOOTER_TEXT[] = "FOOTER";


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
// SERIAL В ДВА КАНАЛА
// ============================================================

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

// ============================================================
// РИСОВАНИЕ
// ============================================================

void drawHeaderBlock() {
  // Голубой прямоугольник заголовка.
  tft.fillRect(
    HEADER_X,
    HEADER_Y,
    HEADER_W,
    HEADER_H,
    COLOR_HEADER_BG
  );

  // Белый текст внутри заголовка.
  tft.setTextSize(HEADER_TEXT_SIZE);
  tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
  tft.setCursor(HEADER_X + HEADER_TEXT_X, HEADER_Y + HEADER_TEXT_Y);
  tft.print(HEADER_TEXT);
}

void drawRadarBlock() {
  // Чёрный прямоугольник блока RADAR.
  tft.fillRect(
    RADAR_X,
    RADAR_Y,
    RADAR_W,
    RADAR_H,
    COLOR_RADAR_BG
  );

  // Тонкая рамка, чтобы видеть границы блока.
  tft.drawRect(
    RADAR_X,
    RADAR_Y,
    RADAR_W,
    RADAR_H,
    COLOR_RADAR_BORDER
  );

  // Белая подпись RADAR в левом верхнем углу блока.
  tft.setTextSize(RADAR_TEXT_SIZE);
  tft.setTextColor(COLOR_RADAR_TEXT, COLOR_RADAR_BG);
  tft.setCursor(RADAR_X + RADAR_TEXT_X, RADAR_Y + RADAR_TEXT_Y);
  tft.print(RADAR_TEXT);
}


void drawFooterBlock() {
  // Нижний блок FOOTER.
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


void drawScreen() {
  // Полностью очищаем экран перед рисованием.
  tft.fillScreen(COLOR_SCREEN_BG);

  // Блок 1: заголовок.
  drawHeaderBlock();

  // Блок 2: область радара.
  drawRadarBlock();

  // Блок 3: нижний footer.
  drawFooterBlock();
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
  printBoth("# Block 1: header");
  printBoth("# Upload USB2 / Monitor USB1");

  tft.begin();
  tft.setRotation(TFT_ROTATION);

  printBoth(String("# rotation=") + String(TFT_ROTATION));
  printBoth(String("# screen=") + String(tft.width()) + "x" + String(tft.height()));

  drawScreen();

  printBoth("# block 1 drawn");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Пока ничего не обновляем.
  // Экран статический: проверяем первый блок.
}
