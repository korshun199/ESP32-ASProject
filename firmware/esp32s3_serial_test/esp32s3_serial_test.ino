/*
  ESP32-S3 Serial Test

  Назначение:
    Проверить вывод Serial через USB CDC на ESP32-S3 N16R8.

  Что должно быть в мониторе:
    # ESP32-S3 SERIAL TEST START
    tick 1
    tick 2
    tick 3
*/

#include <Arduino.h>

uint32_t counter = 0;
uint32_t lastPrintMs = 0;

void setup() {
  Serial.begin(115200);

  // На ESP32-S3 USB Serial может появляться не сразу.
  unsigned long startMs = millis();
  while (!Serial && (millis() - startMs < 5000)) {
    delay(10);
  }

  delay(500);

  Serial.println();
  Serial.println("# ESP32-S3 SERIAL TEST START");
  Serial.println("# USB CDC enabled");
  Serial.println("# Port: /dev/ttyACM0");
}

void loop() {
  uint32_t now = millis();

  if (now - lastPrintMs >= 1000) {
    lastPrintMs = now;
    counter++;

    Serial.print("tick ");
    Serial.print(counter);
    Serial.print("  millis=");
    Serial.println(now);
  }
}
