#!/usr/bin/env bash
set -e

PROJECT_DIR="/home/work/ESP32-ASProject"
WEB_DIR="$PROJECT_DIR/web_viewer"
VENV_DIR="$WEB_DIR/venv"

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"

cd "$PROJECT_DIR"

echo "===== PROJECT ====="
pwd

echo
echo "===== SERIAL PORT ====="
ls -l /dev/ttyUSB0 || true

echo
echo "===== PYTHON VENV ====="
python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/pip" install --upgrade pip
"$VENV_DIR/bin/pip" install -r "$WEB_DIR/requirements.txt"

echo
echo "===== START SERVER ====="
echo "Open in browser:"
echo "http://$HOST:$PORT"
echo
echo "IMPORTANT:"
echo "Do not run arduino-cli monitor at the same time."
echo "The server needs /dev/ttyUSB0."
echo

cd "$WEB_DIR"
exec "$VENV_DIR/bin/uvicorn" app:app --host "$HOST" --port "$PORT"
