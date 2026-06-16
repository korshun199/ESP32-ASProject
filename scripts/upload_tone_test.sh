#!/usr/bin/env bash
set -e

PROJECT_DIR="/home/work/ESP32-ASProject"
SKETCH_DIR="$PROJECT_DIR/firmware/tone_test"
PORT="/dev/ttyUSB0"
FQBN="esp32:esp32:esp32"

cd "$PROJECT_DIR"

echo "===== PROJECT ====="
pwd

echo
echo "===== GIT ====="
git branch --show-current
git status --short

echo
echo "===== PORT ====="
ls -l "$PORT"

echo
echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"

echo
echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"

echo
echo "===== MONITOR ====="
echo "Run:"
echo "arduino-cli monitor -p $PORT -c baudrate=115200"
