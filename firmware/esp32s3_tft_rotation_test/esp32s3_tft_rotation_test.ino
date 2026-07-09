/*
  ESP32-S3 TFT Rotation Test

  Назначение:
    Проверить все варианты ориентации TFT-дисплея:
      rotation 0
      rotation 1
      rotation 2
      rotation 3

  Что смотреть:
    - какой режим реально даёт альбомную картинку
    - какой режим не перевёрнут вверх ногами
    - какие размеры сообщает tft.width() / tft.height()

  Подключение TFT:
    VCC    -> 3V3
    GND    -> GND
    CS     -> GPIO10
    RESET  -> GPIO8
    DC     -> GPIO9
    SDI    -> GPIO11
    SDK    -> GPIO12
    LED    -> 3V3
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

Adafruit_ILI9341 tft = Adafruit_ILI9341(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

uint8_t currentRotation = 0;
uint32_t lastChangeMs = 0;

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

void drawRotationScreen(uint8_t rot) {
  tft.setRotation(rot);

  int w = tft.width();
  int h = tft.height();

  tft.fillScreen(ILI9341_BLACK);

  // Рамка по реальным краям экрана.
  tft.drawRect(0, 0, w, h, ILI9341_RED);
  tft.drawRect(4, 4, w - 8, h - 8, ILI9341_YELLOW);
  tft.drawRect(8, 8, w - 16, h - 16, ILI9341_GREEN);

  // Угловые метки, чтобы понять, где верх/низ/лево/право.
  tft.setTextSize(1);

  tft.setTextColor(ILI9341_CYAN, ILI9341_BLACK);
  tft.setCursor(12, 14);
  tft.print("TOP LEFT");

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(w - 74, 14);
  tft.print("TOP RIGHT");

  tft.setCursor(12, h - 22);
  tft.print("BOTTOM LEFT");

  tft.setCursor(w - 88, h - 22);
  tft.print("BOTTOM RIGHT");

  // Центральная крупная надпись.
  tft.setTextSize(3);
  tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  tft.setCursor(28, h / 2 - 36);
  tft.print("ROT ");
  tft.print(rot);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(28, h / 2 + 4);
  tft.print(w);
  tft.print("x");
  tft.print(h);

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);
  tft.setCursor(28, h / 2 + 34);

  if (w > h) {
    tft.print("MODE: LANDSCAPE");
  } else {
    tft.print("MODE: PORTRAIT");
  }

  printBoth(String("# rotation=") + String(rot) +
            String(" screen=") + String(w) +
            String("x") + String(h) +
            String(" mode=") + String((w > h) ? "landscape" : "portrait"));
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  delay(1200);

  printBoth("");
  printBoth("# ESP32-S3 TFT ROTATION TEST");
  printBoth("# Upload USB2 / Monitor USB1");

  tft.begin();

  currentRotation = 0;
  drawRotationScreen(currentRotation);
  lastChangeMs = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - lastChangeMs >= 3000) {
    lastChangeMs = now;

    currentRotation++;
    if (currentRotation > 3) {
      currentRotation = 0;
    }

    drawRotationScreen(currentRotation);
  }
}
