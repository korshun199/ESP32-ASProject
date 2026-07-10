#!/usr/bin/env bash
set -e
cd /home/work/ESP32-ASProject
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,UploadMode=default,FlashSize=16M,PSRAM=opi'
arduino-cli compile --fqbn "$FQBN" firmware/esp32s3_i2s_mic_test
arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" firmware/esp32s3_i2s_mic_test
