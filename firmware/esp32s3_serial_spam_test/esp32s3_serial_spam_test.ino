/*
  ESP32-S3 Serial Spam Test

  Назначение:
    Проверить, что ESP32-S3 печатает в USB Serial без ожидания подключения.

  Почему так:
    На ESP32-S3 ожидание while (!Serial) иногда мешает диагностике.
    Поэтому этот тест просто печатает постоянно.

  Ожидаемый вывод:
    # ESP32-S3 SERIAL SPAM TEST
    tick 1
    tick 2
    tick 3
*/

#include <Arduino.h>

uint32_t counter = 0;
uint32_t lastPrintMs = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("# ESP32-S3 SERIAL SPAM TEST");
  Serial.println("# No waiting for Serial");
  Serial.println("# If you see this, USB CDC works");
}

void loop() {
  uint32_t now = millis();

  if (now - lastPrintMs >= 1000) {
    lastPrintMs = now;
    counter++;

    Serial.print("tick ");
    Serial.print(counter);
    Serial.print(" millis=");
    Serial.println(now);
  }
}
