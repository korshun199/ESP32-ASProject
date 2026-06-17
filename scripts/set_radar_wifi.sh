#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

CONFIG="firmware/esp32_radar_real_mics/wifi_config.h"

echo "===== ESP32 RADAR WIFI CONFIG ====="
echo "This creates local Wi-Fi config:"
echo "$CONFIG"
echo
echo "This file is ignored by Git and must NOT be committed."
echo

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
echo "===== CREATED ====="
ls -lah "$CONFIG"
echo
echo "Now flash with:"
echo "  ./scripts/upload_radar_mics.sh /dev/ttyUSB0"
