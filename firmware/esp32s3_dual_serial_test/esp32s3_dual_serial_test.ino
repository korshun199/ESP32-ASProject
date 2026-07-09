/*
  ESP32-S3 Dual Serial Test

  Назначение:
    Проверить, куда реально выводится текст на ESP32-S3 N16R8.

  Что делает:
    - печатает в USB Serial
    - печатает в UART0 Serial0
    - мигает встроенным RGB, если он на GPIO48

  Почему:
    У платы два USB-порта.
    Один может быть USB CDC/JTAG, второй может быть UART-мостом.
    Поэтому печатаем сразу в оба направления.

  Ожидаемый вывод:
    # ESP32-S3 DUAL SERIAL TEST
    tick ...
*/

#include <Arduino.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48
#endif

uint32_t counter = 0;
uint32_t lastTickMs = 0;
uint8_t colorStep = 0;

void rgbSet(uint8_t r, uint8_t g, uint8_t b) {
#ifdef neopixelWrite
  neopixelWrite(RGB_BUILTIN, r, g, b);
#endif
}

void printBoth(const String &s) {
  Serial.println(s);
  Serial0.println(s);
}

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  delay(1500);

  printBoth("");
  printBoth("# ESP32-S3 DUAL SERIAL TEST");
  printBoth("# USB Serial + UART0 Serial0");
  printBoth("# Если это видно только на одном USB, значит второй порт и есть рабочий Serial.");
  printBoth("# RGB assumed GPIO48");

  rgbSet(30, 0, 0);
  delay(300);
  rgbSet(0, 30, 0);
  delay(300);
  rgbSet(0, 0, 30);
  delay(300);
  rgbSet(0, 0, 0);
}

void loop() {
  uint32_t now = millis();

  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    counter++;

    String line = "tick " + String(counter) + " millis=" + String(now);

    Serial.println(line);
    Serial0.println(line);

    if (colorStep == 0) {
      rgbSet(30, 0, 0);
    } else if (colorStep == 1) {
      rgbSet(0, 30, 0);
    } else {
      rgbSet(0, 0, 30);
    }

    colorStep++;
    if (colorStep >= 3) {
      colorStep = 0;
    }
  }
}
