#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

CONFIG="firmware/esp32_real_mic_selectable_sta/wifi_config.h"

echo "===== ESP32 REAL MIC WIFI CONFIG ====="
read -rp "Wi-Fi SSID: " WIFI_SSID
read -rsp "Wi-Fi password: " WIFI_PASS
echo

if [ -z "$WIFI_SSID" ]; then
  echo "ERROR: Wi-Fi SSID is empty"
  exit 1
fi

mkdir -p "$(dirname "$CONFIG")"

cat > "$CONFIG" <<CFG
#pragma once

#define WIFI_SSID "$WIFI_SSID"
#define WIFI_PASS "$WIFI_PASS"
CFG

chmod 600 "$CONFIG"

echo
echo "Created:"
ls -lah "$CONFIG"
echo
echo "Now flash:"
echo "  ./scripts/upload_real_mic_selectable.sh /dev/ttyUSB0"
