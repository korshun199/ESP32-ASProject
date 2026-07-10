ESP32-S3 + ICS-43434 MIC TEST

Пины:
MIC VDD/VCC -> 3V3
MIC GND     -> GND
MIC SCK     -> GPIO4
MIC WS      -> GPIO5
MIC SD      -> GPIO6
MIC L/R     -> GND

Файл прошивки:
firmware/esp32s3_i2s_mic_test/esp32s3_i2s_mic_test.ino

Запуск:
./scripts/run_mic.sh
./scripts/monitor.sh

Ожидаемый вывод:
mic=OK lvl=...% level=... p2p=... |####
