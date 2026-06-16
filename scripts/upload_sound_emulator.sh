#!/usr/bin/env bash
set -e

PORT="${1:-/dev/ttyUSB0}"
FQBN="esp32:esp32:esp32"
SKETCH="/home/work/ESP32-ASProject/firmware/sound_emulator"

echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo
echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "===== MONITOR ====="
echo "arduino-cli monitor -p $PORT -c baudrate=115200"
