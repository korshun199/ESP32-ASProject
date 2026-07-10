#!/usr/bin/env bash
set -e
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
