#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="/home/work/ESP32-ASProject"
SKETCH="$PROJECT_DIR/firmware/esp32_single_mic_serial"
PORT="${1:-/dev/ttyUSB0}"

# Универсальная плата для ESP32-WROOM / ESP32 Dev Module
FQBN="esp32:esp32:esp32"

echo "=== Прошивка ESP32: один реальный микрофон по USB Serial ==="
echo "Проект:  $PROJECT_DIR"
echo "Скетч:   $SKETCH"
echo "Порт:    $PORT"
echo "Плата:   $FQBN"
echo

echo "=== Список плат Arduino CLI ==="
arduino-cli board list || true
echo

echo "=== Компиляция ==="
arduino-cli compile --fqbn "$FQBN" "$SKETCH"

echo
echo "=== Загрузка в ESP32 ==="
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"

echo
echo "=== Готово ==="
echo "Проверка Serial:"
echo "  arduino-cli monitor -p $PORT -c baudrate=115200"
echo
echo "Ожидаемые строки JSON:"
echo '  {"seq":1,"source":"esp32_single_real_mic_serial",...}'
