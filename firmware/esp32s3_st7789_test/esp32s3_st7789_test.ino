#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12

Adafruit_ST7789 tft(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(1200);

  tft.init(240, 320);
  tft.setRotation(0);

  int w = tft.width();
  int h = tft.height();

  Serial.println("# ST7789 TEST");
  Serial.print("# width=");
  Serial.println(w);
  Serial.print("# height=");
  Serial.println(h);

  Serial0.println("# ST7789 TEST");
  Serial0.print("# width=");
  Serial0.println(w);
  Serial0.print("# height=");
  Serial0.println(h);

  tft.fillScreen(ST77XX_BLACK);

  tft.fillRect(0, 0, 10, 10, ST77XX_RED);
  tft.fillRect(w - 10, 0, 10, 10, ST77XX_GREEN);
  tft.fillRect(0, h - 10, 10, 10, ST77XX_BLUE);
  tft.fillRect(w - 10, h - 10, 10, 10, ST77XX_YELLOW);
}

void loop() {
}
