#!/usr/bin/env bash
set -e

PROJECT_DIR="/home/work/ESP32-ASProject"
WEB_DIR="$PROJECT_DIR/web/radar_virtual_mics"
PORT="${PORT:-8088}"

cd "$WEB_DIR"

IP="$(hostname -I | tr ' ' '\n' | grep '^192\.168\.20\.' | head -n1 || true)"

if [ -z "$IP" ]; then
  IP="$(hostname -I | awk '{print $1}')"
fi

echo
echo "=== Virtual Acoustic Radar ==="
echo
echo "T16 control:"
echo "  http://$IP:$PORT/control.html?v=7"
echo
echo "Tablet monitor:"
echo "  http://$IP:$PORT/monitor.html?v=7"
echo
echo "Stop:"
echo "  Ctrl+C"
echo

python3 server.py
