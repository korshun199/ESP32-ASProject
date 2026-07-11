#!/usr/bin/env bash
set -e

cd /home/work/ESP32-ASProject

arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200 -c dtr=off -c rts=off
