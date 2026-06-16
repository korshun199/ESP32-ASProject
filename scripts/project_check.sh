#!/usr/bin/env bash
set -u

OK=0
FAIL=0

check_ok() {
  echo "[OK]   $1"
  OK=$((OK+1))
}

check_fail() {
  echo "[FAIL] $1"
  FAIL=$((FAIL+1))
}

check_file() {
  if [ -f "$1" ]; then
    check_ok "$1 exists"
  else
    check_fail "$1 missing"
  fi
}

echo "=== ESP32-ASProject CHECK ==="
echo

echo "=== SYSTEM ==="
uname -a || true
echo

echo "=== GIT ==="
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  check_ok "inside git repo"
else
  check_fail "not inside git repo"
fi

BRANCH="$(git branch --show-current 2>/dev/null || echo unknown)"
echo "branch: $BRANCH"

if git status --short | grep -q .; then
  echo "working tree: has changes"
else
  check_ok "working tree clean"
fi

if git remote -v | grep -q "github.com:korshun199/ESP32-ASProject.git"; then
  check_ok "GitHub remote OK"
else
  check_fail "GitHub remote not expected"
fi

echo
echo "=== ARDUINO CLI ==="
if command -v arduino-cli >/dev/null 2>&1; then
  check_ok "arduino-cli installed"
  arduino-cli version || true
else
  check_fail "arduino-cli not installed"
fi

echo
echo "=== ESP32 CORE ==="
if arduino-cli core list 2>/dev/null | grep -q "esp32:esp32"; then
  check_ok "ESP32 core installed"
  arduino-cli core list | grep "esp32:esp32" || true
else
  check_fail "ESP32 core not installed"
fi

echo
echo "=== SERIAL PORTS ==="
PORTS="$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)"
if [ -n "$PORTS" ]; then
  check_ok "serial port found"
  echo "$PORTS"
else
  check_fail "no serial port found"
fi

echo
echo "=== FILES ==="
check_file "scripts/project_check.sh"
check_file "docs/project-log.md"
check_file "firmware/tone_test/tone_test.ino"
check_file "scripts/upload_tone_test.sh"
check_file "firmware/sound_emulator/sound_emulator.ino"
check_file "scripts/upload_sound_emulator.sh"
check_file "firmware/three_mic_point/three_mic_point.ino"
check_file "scripts/upload_3mic_point.sh"

echo
echo "=== SUMMARY ==="
echo "OK=$OK"
echo "FAIL=$FAIL"

if [ "$FAIL" -eq 0 ]; then
  echo "STATUS=OK"
  exit 0
else
  echo "STATUS=FAIL"
  exit 1
fi
