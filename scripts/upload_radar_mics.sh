#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

PORT="${1:-/dev/ttyUSB0}"
FQBN="${FQBN:-esp32:esp32:esp32}"
SKETCH="firmware/esp32_radar_real_mics"

echo "===== CHECK WIFI CONFIG ====="
if [ ! -f "$SKETCH/wifi_config.h" ]; then
  echo "ERROR: missing Wi-Fi config:"
  echo "$SKETCH/wifi_config.h"
  echo
  echo "Run:"
  echo "  ./scripts/set_radar_wifi.sh"
  exit 1
fi

if grep -q "ИМЯ_ТВОЕЙ_WIFI_СЕТИ" "$SKETCH/wifi_config.h"; then
  echo "ERROR: set Wi-Fi config first:"
  echo "  ./scripts/set_radar_wifi.sh"
  exit 1
fi

echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "Open:"
echo "  http://192.168.4.77/"
echo "  http://192.168.4.77/mobile"
echo "  http://192.168.4.77/api/latest"
