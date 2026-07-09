/*
  ESP32-S3 TFT Radar Demo

  Назначение:
    Первая тестовая прошивка для цифровой версии радара.

  Что проверяем:
    - ESP32-S3 N16R8
    - TFT 2.8" 240x320 SPI
    - базовый вывод графики
    - обновление экрана
    - будущую раскладку пинов под I2S микрофоны

  Дисплей:
    TPW-408-2.8
    TFT 240xRGBx320
    Вероятный контроллер: ILI9341
    Интерфейс: SPI

  Подключение TFT:
    VCC    -> 3V3
    GND    -> GND
    CS     -> GPIO10
    RESET  -> GPIO8
    DC     -> GPIO9
    SDI    -> GPIO11
    SDK    -> GPIO12
    LED    -> 3V3

  Пока не подключаем:
    T_IRQ
    T_D0
    T_CS
    T_CLK
    SDDC

  Будущие I2S микрофоны:
    BCLK   -> GPIO4
    WS     -> GPIO5
    DATA   -> GPIO6
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12

#define MIC_I2S_BCLK 4
#define MIC_I2S_WS   5
#define MIC_I2S_DATA 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

uint32_t frameCounter = 0;
uint32_t lastFrameMs = 0;

int sweepAngle = 0;
bool eventBlink = false;

void drawHeader() {
  tft.fillRect(0, 0, 320, 34, ILI9341_NAVY);

  tft.setTextColor(ILI9341_WHITE, ILI9341_NAVY);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.print("ESP32-S3 RADAR");
}

void drawPinPanel() {
  tft.fillRect(0, 36, 320, 78, ILI9341_BLACK);
  tft.drawRect(2, 38, 316, 74, ILI9341_DARKGREY);

  tft.setTextSize(1);

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(8, 44);
  tft.print("TFT SPI:");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(8, 58);
  tft.print("CS10 DC9 RST8 MOSI11 SCK12");

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(8, 76);
  tft.print("I2S MIC FUTURE:");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(8, 90);
  tft.print("BCLK4 WS5 DATA6");
}

void drawRadarBase() {
  const int cx = 160;
  const int cy = 190;

  tft.fillRect(0, 116, 320, 124, ILI9341_BLACK);

  tft.drawCircle(cx, cy, 20, ILI9341_DARKGREEN);
  tft.drawCircle(cx, cy, 45, ILI9341_DARKGREEN);
  tft.drawCircle(cx, cy, 70, ILI9341_DARKGREEN);
  tft.drawCircle(cx, cy, 95, ILI9341_DARKGREEN);

  tft.drawLine(cx - 110, cy, cx + 110, cy, ILI9341_DARKGREEN);
  tft.drawLine(cx, cy - 105, cx, cy + 45, ILI9341_DARKGREEN);
}

void drawSweep() {
  const int cx = 160;
  const int cy = 190;
  const int r = 95;

  float a = sweepAngle * DEG_TO_RAD;

  int x = cx + cos(a) * r;
  int y = cy + sin(a) * r;

  tft.drawLine(cx, cy, x, y, ILI9341_GREEN);

  int bx1 = cx + cos((sweepAngle + 35) * DEG_TO_RAD) * 55;
  int by1 = cy + sin((sweepAngle + 35) * DEG_TO_RAD) * 55;

  int bx2 = cx + cos((sweepAngle + 130) * DEG_TO_RAD) * 78;
  int by2 = cy + sin((sweepAngle + 130) * DEG_TO_RAD) * 78;

  tft.fillCircle(bx1, by1, 4, eventBlink ? ILI9341_RED : ILI9341_YELLOW);
  tft.fillCircle(bx2, by2, 3, ILI9341_CYAN);
}

void drawStatus() {
  tft.fillRect(0, 244, 320, 76, ILI9341_BLACK);
  tft.drawRect(2, 246, 316, 70, ILI9341_DARKGREY);

  tft.setTextSize(1);

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(8, 254);
  tft.print("EVENT:");

  tft.setTextColor(eventBlink ? ILI9341_RED : ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(60, 254);

  if (eventBlink) {
    tft.print("SIGNAL DETECTED");
  } else {
    tft.print("SCAN");
  }

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(8, 272);
  tft.print("FRAME:");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(60, 272);
  tft.print(frameCounter);

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(8, 290);
  tft.print("NEXT:");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(60, 290);
  tft.print("I2S digital microphones");
}

void drawBootScreen() {
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(35, 70);
  tft.print("RADAR DISPLAY");

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(48, 110);
  tft.print("ESP32-S3 N16R8 + TFT 2.8");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(78, 140);
  tft.print("starting demo...");

  delay(1200);
}

void setup() {
  Serial.begin(115200);

  // Ждём подключения USB Serial.
  // На ESP32-S3 без этого первые сообщения часто улетают в пустоту,
  // а потом мы смотрим в терминал как в бездну.
  unsigned long serialStartMs = millis();
  while (!Serial && (millis() - serialStartMs < 3000)) {
    delay(10);
  }

  delay(500);

  Serial.println();
  Serial.println("# ESP32-S3 TFT Radar Demo");
  Serial.println("# TFT pins:");
  Serial.println("# CS=10 DC=9 RST=8 MOSI=11 SCK=12");
  Serial.println("# Future I2S:");
  Serial.println("# BCLK=4 WS=5 DATA=6");

  tft.begin();
  tft.setRotation(1);

  drawBootScreen();
  drawHeader();
  drawPinPanel();
  drawRadarBase();
  drawStatus();
}

void loop() {
  uint32_t now = millis();

  if (now - lastFrameMs >= 90) {
    lastFrameMs = now;
    frameCounter++;

    sweepAngle += 8;

    if (sweepAngle >= 360) {
      sweepAngle = 0;
    }

    eventBlink = ((frameCounter / 10) % 2) == 0;

    drawRadarBase();
    drawSweep();
    drawStatus();
  }
}
