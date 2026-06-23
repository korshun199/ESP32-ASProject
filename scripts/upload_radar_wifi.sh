#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="/home/work/ESP32-ASProject"
SKETCH="$PROJECT_DIR/firmware/radar_wifi/radar_wifi.ino"
PORT="${1:-/dev/ttyUSB0}"
FQBN="esp32:esp32:esp32"

echo "=== ESP32 RADAR WIFI UPLOAD ==="
echo "PROJECT: $PROJECT_DIR"
echo "SKETCH : $SKETCH"
echo "PORT   : $PORT"
echo

arduino-cli compile --fqbn "$FQBN" "$SKETCH"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "DONE"
echo "Connect phone/laptop to Wi-Fi:"
echo "SSID: ESP32-RADAR"
echo "PASS: 12345678"
echo "Open: http://192.168.4.1"
