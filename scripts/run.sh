#!/usr/bin/env bash

# Останавливаем выполнение при любой ошибке.
set -euo pipefail

# Переходим в корень проекта.
cd /home/work/ESP32-ASProject

# USB CDC при старте отключён.
# Serial работает через UART0 и внешний USB-UART мост.
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=default,UploadMode=default,FlashSize=16M,PSRAM=opi'

# Собираем прошивку радара.
arduino-cli compile \
  --fqbn "$FQBN" \
  firmware/radar

# Загружаем прошивку в ESP32-S3.
arduino-cli upload \
  -p /dev/ttyACM0 \
  --fqbn "$FQBN" \
  --upload-property upload.speed=115200 \
  firmware/radar
