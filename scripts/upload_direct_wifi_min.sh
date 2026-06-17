#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

PORT="${1:-/dev/ttyUSB0}"
FQBN="${FQBN:-esp32:esp32:esp32}"

echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" firmware/esp32_direct_wifi_min

echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" firmware/esp32_direct_wifi_min

echo
echo "Android Wi-Fi: ESP32-RADAR"
echo "Password    : 12345678"
echo "Open        : http://192.168.4.1"
