#!/usr/bin/env bash
set -u

PORT="${1:-/dev/ttyUSB0}"
BASE_DIR="${2:-/home/work/ESP32-ASProject/device_reports}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="$BASE_DIR/esp_probe_$STAMP"
mkdir -p "$OUT_DIR"

RAW_LOG="$OUT_DIR/raw.log"
REPORT="$OUT_DIR/report.txt"
HTML="$OUT_DIR/report.html"

run() {
  echo
  echo "===== $* ====="
  "$@" 2>&1
}

{
  echo "ESP DEVICE PROBE"
  echo "DATE: $(date)"
  echo "PORT: $PORT"
  echo "OUT_DIR: $OUT_DIR"

  run esptool.py --port "$PORT" chip_id
  run esptool.py --port "$PORT" flash_id
  run esptool.py --port "$PORT" read_mac

  echo
  echo "===== READ PARTITION TABLE ====="
  esptool.py --port "$PORT" read_flash 0x8000 0x1000 "$OUT_DIR/partitions.bin"

  echo
  echo "===== DECODE PARTITION TABLE ====="
  GEN_PART="$(find ~/.arduino15/packages/esp32/hardware/esp32 -name gen_esp32part.py | sort -V | tail -1)"
  echo "GEN_PART: $GEN_PART"

  if [ -n "$GEN_PART" ] && [ -f "$GEN_PART" ]; then
    python "$GEN_PART" "$OUT_DIR/partitions.bin" | tee "$OUT_DIR/partitions.csv"
  else
    echo "gen_esp32part.py not found"
  fi

  echo
  echo "===== FLASH SIZE DETECT ====="
  FLASH_ID_OUT="$(esptool.py --port "$PORT" flash_id 2>&1)"
  echo "$FLASH_ID_OUT" > "$OUT_DIR/flash_id.txt"
  echo "$FLASH_ID_OUT"

  FLASH_SIZE_HEX=""
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 4MB"; then FLASH_SIZE_HEX="0x400000"; fi
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 8MB"; then FLASH_SIZE_HEX="0x800000"; fi
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 16MB"; then FLASH_SIZE_HEX="0x1000000"; fi

  if [ -n "$FLASH_SIZE_HEX" ]; then
    echo
    echo "===== FULL FLASH BACKUP ====="
    echo "FLASH_SIZE: $FLASH_SIZE_HEX"
    esptool.py --port "$PORT" read_flash 0x000000 "$FLASH_SIZE_HEX" "$OUT_DIR/full_flash.bin"
    sha256sum "$OUT_DIR/full_flash.bin" | tee "$OUT_DIR/full_flash.sha256"
  else
    echo "FLASH SIZE UNKNOWN, full dump skipped"
  fi

} | tee "$RAW_LOG"

cp "$RAW_LOG" "$REPORT"

{
  echo "<!doctype html><html><head><meta charset='utf-8'>"
  echo "<title>ESP Device Probe $STAMP</title>"
  echo "<style>body{background:#111;color:#eee;font-family:monospace;padding:20px}pre{background:#1b1b1b;border:1px solid #555;padding:15px;border-radius:10px;white-space:pre-wrap}</style>"
  echo "</head><body>"
  echo "<h1>ESP Device Probe</h1>"
  echo "<p><b>Port:</b> $PORT</p>"
  echo "<p><b>Output:</b> $OUT_DIR</p>"
  echo "<pre>"
  sed 's/&/\&amp;/g;s/</\&lt;/g;s/>/\&gt;/g' "$RAW_LOG"
  echo "</pre>"
  echo "</body></html>"
} > "$HTML"

echo
echo "DONE"
echo "OUT_DIR: $OUT_DIR"
echo "REPORT : $REPORT"
echo "HTML   : $HTML"
