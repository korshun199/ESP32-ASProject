/*
  ESP32-S3 Alive Test

  Назначение:
    Проверить, что пользовательская прошивка реально стартует на ESP32-S3 N16R8.

  Что делает:
    - печатает сообщения в USB Serial
    - мигает встроенным RGB-светодиодом, если он висит на GPIO48

  Почему это нужно:
    Если после boot-лога нет наших сообщений, надо понять:
      1. программа не стартует
      2. или программа стартует, но Serial не виден

  Примечание:
    На многих ESP32-S3 DevKit RGB-светодиод WS2812 подключён к GPIO48.
*/

#include <Arduino.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

uint32_t counter = 0;
uint32_t lastTickMs = 0;
uint8_t colorStep = 0;

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef neopixelWrite
  neopixelWrite(RGB_BUILTIN, r, g, b);
#endif
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("# ESP32-S3 ALIVE TEST START");
  Serial.println("# Если видишь tick, USB Serial работает.");
  Serial.println("# Если tick не видишь, но RGB мигает, программа работает, а Serial молчит.");
  Serial.println("# RGB pin assumed: GPIO48");

  setRgb(20, 0, 0);
  delay(300);
  setRgb(0, 20, 0);
  delay(300);
  setRgb(0, 0, 20);
  delay(300);
  setRgb(0, 0, 0);
}

void loop() {
  uint32_t now = millis();

  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    counter++;

    Serial.print("tick ");
    Serial.print(counter);
    Serial.print(" millis=");
    Serial.println(now);

    if (colorStep == 0) {
      setRgb(30, 0, 0);
    } else if (colorStep == 1) {
      setRgb(0, 30, 0);
    } else {
      setRgb(0, 0, 30);
    }

    colorStep++;
    if (colorStep >= 3) {
      colorStep = 0;
    }
  }
}
