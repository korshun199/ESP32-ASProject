#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

PORT="${1:-/dev/ttyUSB0}"
FQBN="${FQBN:-esp32:esp32:esp32}"
SKETCH="firmware/esp32_real_mic_selectable_sta"

echo "===== CHECK WIFI CONFIG ====="
if grep -q "ИМЯ_ТВОЕЙ_WIFI_СЕТИ" "$SKETCH/wifi_config.h"; then
  echo "ERROR: set Wi-Fi config first:"
  echo "$SKETCH/wifi_config.h"
  exit 1
fi

echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "Open:"
echo "  http://192.168.20.77/"
echo "  http://192.168.20.77/mobile"
echo "  http://192.168.20.77/api/latest"
