#!/usr/bin/env bash
set -e
cd /home/work/ESP32-ASProject
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi'
arduino-cli compile --fqbn "$FQBN" firmware/radar
arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" --upload-property upload.speed=115200 firmware/radar
