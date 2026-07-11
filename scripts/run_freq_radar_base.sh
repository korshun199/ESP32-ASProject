#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi'

echo "===== FILE MARKER ====="
grep -n "FREQ_GOERTZEL_400_1000_START" firmware/freq_radar_base/freq_radar_base.ino

echo
echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" firmware/freq_radar_base

echo
echo "===== UPLOAD ====="
arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" --upload-property upload.speed=115200 firmware/freq_radar_base

echo
echo "===== MONITOR RTS/DTR OFF ====="
echo "Ждём ALIVE_WAITING. Потом введи s и нажми Enter."
echo "Сначала включи генератор на 400 Гц."
echo

arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200 -c dtr=off -c rts=off
