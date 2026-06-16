#!/usr/bin/env bash
set -e

PROJECT_DIR="/home/work/ESP32-ASProject"
SKETCH_DIR="$PROJECT_DIR/firmware/three_mic_point"
PORT="${1:-/dev/ttyUSB0}"
FQBN="${2:-esp32:esp32:esp32}"

cd "$PROJECT_DIR"

echo "===== PROJECT ====="
pwd

echo
echo "===== BRANCH ====="
git branch --show-current

echo
echo "===== PORT ====="
ls -l "$PORT"

echo
echo "===== SKETCH ====="
ls -l "$SKETCH_DIR"

echo
echo "===== COMPILE ====="
arduino-cli compile --fqbn "$FQBN" "$SKETCH_DIR"

echo
echo "===== UPLOAD ====="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_DIR"

echo
echo "===== MONITOR COMMAND ====="
echo "arduino-cli monitor -p $PORT -c baudrate=115200"
