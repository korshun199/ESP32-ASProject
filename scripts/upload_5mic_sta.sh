#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

PORT="${1:-/dev/ttyUSB0}"
FQBN="${FQBN:-esp32:esp32:esp32}"
SKETCH="firmware/esp32_direct_wifi_5mic_sta"

echo "===== CHECK WIFI CONFIG ====="
if grep -q "ИМЯ_ТВОЕЙ_WIFI_СЕТИ" "$SKETCH/wifi_config.h"; then
  echo "ERROR: edit local Wi-Fi config first:"
  echo "$SKETCH/wifi_config.h"
  echo
  echo "Use command like:"
  echo "cat > $SKETCH/wifi_config.h <<'CFG'"
  echo "#pragma once"
  echo "#define WIFI_SSID \"your_wifi_name\""
  echo "#define WIFI_PASS \"your_wifi_password\""
  echo "CFG"
  exit 1
fi

echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "After upload, open Serial Monitor to see ESP32 IP:"
echo "arduino-cli monitor -p $PORT -c baudrate=115200"
