#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"
BASE_DIR="${2:-/home/work/ESP32-ASProject/device_reports}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="$BASE_DIR/esp_probe_$STAMP"

mkdir -p "$OUT_DIR"

RAW_LOG="$OUT_DIR/raw.log"
HTML="$OUT_DIR/report.html"
CSV="$OUT_DIR/partitions.csv"
MAP="$OUT_DIR/flash_map.txt"

logrun() {
  echo
  echo "===== $* ====="
  "$@" 2>&1
}

{
  echo "ESP DEVICE PROBE"
  echo "DATE: $(date)"
  echo "PORT: $PORT"
  echo "OUT_DIR: $OUT_DIR"

  logrun esptool.py --port "$PORT" chip_id
  logrun esptool.py --port "$PORT" read_mac

  echo
  echo "===== FLASH ID ====="
  FLASH_ID_OUT="$(esptool.py --port "$PORT" flash_id 2>&1)"
  echo "$FLASH_ID_OUT" | tee "$OUT_DIR/flash_id.txt"

  FLASH_SIZE_HEX=""
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 2MB"; then FLASH_SIZE_HEX="0x200000"; fi
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 4MB"; then FLASH_SIZE_HEX="0x400000"; fi
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 8MB"; then FLASH_SIZE_HEX="0x800000"; fi
  if echo "$FLASH_ID_OUT" | grep -qi "Detected flash size: 16MB"; then FLASH_SIZE_HEX="0x1000000"; fi

  echo
  echo "===== READ PARTITION TABLE ====="
  esptool.py --port "$PORT" read_flash 0x8000 0x1000 "$OUT_DIR/partitions.bin"

  echo
  echo "===== FULL FLASH BACKUP ====="
  if [ -n "$FLASH_SIZE_HEX" ]; then
    esptool.py --port "$PORT" read_flash 0x000000 "$FLASH_SIZE_HEX" "$OUT_DIR/full_flash.bin"
    sha256sum "$OUT_DIR/full_flash.bin" | tee "$OUT_DIR/full_flash.sha256"
  else
    echo "FLASH SIZE UNKNOWN, full_flash.bin skipped"
  fi

  echo
  echo "===== DECODE PARTITIONS ====="
  python3 - "$OUT_DIR/partitions.bin" "$CSV" "$MAP" << 'PY'
import sys, struct, csv

part_bin, csv_path, map_path = sys.argv[1:4]
data = open(part_bin, "rb").read()

TYPE_NAMES = {
    0x00: "Прошивка",
    0x01: "Данные",
}

TYPE_RAW = {
    0x00: "app",
    0x01: "data",
}

SUBTYPE_APP = {
    0x00: "factory",
    0x10: "ota_0",
    0x11: "ota_1",
    0x12: "ota_2",
    0x13: "ota_3",
    0x14: "ota_4",
    0x15: "ota_5",
    0x16: "ota_6",
    0x17: "ota_7",
    0x18: "ota_8",
    0x19: "ota_9",
    0x1A: "ota_10",
    0x1B: "ota_11",
    0x1C: "ota_12",
    0x1D: "ota_13",
    0x1E: "ota_14",
    0x1F: "ota_15",
    0x20: "test",
}

SUBTYPE_DATA = {
    0x00: "ota",
    0x01: "phy",
    0x02: "nvs",
    0x03: "coredump",
    0x04: "nvs_keys",
    0x05: "efuse",
    0x80: "esphttpd",
    0x81: "fat",
    0x82: "spiffs",
    0x83: "littlefs",
}

SUBTYPE_RU = {
    "factory": "Заводская прошивка",
    "ota_0": "OTA слот 0",
    "ota_1": "OTA слот 1",
    "ota_2": "OTA слот 2",
    "ota_3": "OTA слот 3",
    "test": "Тестовая прошивка",
    "ota": "OTA данные",
    "phy": "PHY настройки",
    "nvs": "Настройки NVS",
    "coredump": "Дамп аварии",
    "nvs_keys": "Ключи NVS",
    "efuse": "eFuse данные",
    "fat": "Файловая система FAT",
    "spiffs": "Файловая система SPIFFS",
    "littlefs": "Файловая система LittleFS",
}

rows = []

for i in range(0, len(data), 32):
    entry = data[i:i+32]
    if len(entry) < 32:
        break

    magic = entry[0:2]

    if magic == b"\xff\xff":
        break

    if magic != b"\xaa\x50":
        continue

    p_type = entry[2]
    p_sub = entry[3]
    offset = struct.unpack("<I", entry[4:8])[0]
    size = struct.unpack("<I", entry[8:12])[0]
    label = entry[12:28].split(b"\x00", 1)[0].decode("ascii", errors="replace")
    flags = struct.unpack("<I", entry[28:32])[0]

    type_name = TYPE_NAMES.get(p_type, f"0x{p_type:02X}")
    type_raw = TYPE_RAW.get(p_type, f"0x{p_type:02X}")

    if p_type == 0x00:
        subtype_raw = SUBTYPE_APP.get(p_sub, f"0x{p_sub:02X}")
    elif p_type == 0x01:
        subtype_raw = SUBTYPE_DATA.get(p_sub, f"0x{p_sub:02X}")
    else:
        subtype_raw = f"0x{p_sub:02X}"

    subtype_ru = SUBTYPE_RU.get(subtype_raw, subtype_raw)

    end = offset + size

    rows.append({
        "Раздел": label,
        "Тип": type_name,
        "Подтип": subtype_ru,
        "Начало HEX": f"0x{offset:08X}",
        "Размер HEX": f"0x{size:08X}",
        "Конец HEX": f"0x{end:08X}",
        "Начало байт": offset,
        "Размер байт": size,
        "Конец байт": end,
        "Начало КБ": round(offset / 1024, 2),
        "Размер КБ": round(size / 1024, 2),
        "Конец КБ": round(end / 1024, 2),
        "Флаги": f"0x{flags:08X}",
        "_raw_type": type_raw,
        "_raw_subtype": subtype_raw,
    })

fieldnames = [
    "Раздел",
    "Тип",
    "Подтип",
    "Начало HEX",
    "Размер HEX",
    "Конец HEX",
    "Начало байт",
    "Размер байт",
    "Конец байт",
    "Начало КБ",
    "Размер КБ",
    "Конец КБ",
    "Флаги",
]

with open(csv_path, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fieldnames)
    w.writeheader()
    for r in rows:
        w.writerow({k: r[k] for k in fieldnames})

with open(map_path, "w", encoding="utf-8") as f:
    f.write("КАРТА FLASH ESP32\n")
    f.write("=================\n\n")

    if not rows:
        f.write("Разделы не найдены или таблица разделов не распознана.\n")
    else:
        f.write(
            f"{'Раздел':16} {'Тип':12} {'Подтип':26} "
            f"{'Начало HEX':12} {'Размер HEX':12} {'Конец HEX':12} {'Размер КБ':>10}\n"
        )
        f.write("-" * 106 + "\n")

        for r in rows:
            f.write(
                f"{r['Раздел']:16} {r['Тип']:12} {r['Подтип']:26} "
                f"{r['Начало HEX']:12} {r['Размер HEX']:12} {r['Конец HEX']:12} {r['Размер КБ']:10}\n"
            )

print(open(map_path, encoding="utf-8").read())

raw_csv = csv_path.replace(".csv", "_raw.csv")
raw_fields = [
    "name","type","subtype",
    "offset_hex","size_hex","end_hex",
    "offset_dec","size_dec","end_dec",
    "flags"
]

with open(raw_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=raw_fields)
    w.writeheader()
    for r in rows:
        w.writerow({
            "name": r["Раздел"],
            "type": r["_raw_type"],
            "subtype": r["_raw_subtype"],
            "offset_hex": r["Начало HEX"],
            "size_hex": r["Размер HEX"],
            "end_hex": r["Конец HEX"],
            "offset_dec": r["Начало байт"],
            "size_dec": r["Размер байт"],
            "end_dec": r["Конец байт"],
            "flags": r["Флаги"],
        })
PY

  echo
  echo "===== EXTRACT APP BINARIES ====="
  mkdir -p "$OUT_DIR/extracted"

  RAW_CSV="$OUT_DIR/partitions_raw.csv"

  if [ -f "$OUT_DIR/full_flash.bin" ] && [ -f "$RAW_CSV" ]; then
    tail -n +2 "$RAW_CSV" | while IFS=',' read -r name type subtype offset_hex size_hex end_hex offset_dec size_dec end_dec flags; do
      if [ "$type" = "app" ]; then
        safe_name="$(echo "${name}_${subtype}" | tr -cd 'A-Za-z0-9_.-')"
        out_bin="$OUT_DIR/extracted/${safe_name}_offset_${offset_hex}_size_${size_hex}.bin"

        dd if="$OUT_DIR/full_flash.bin" of="$out_bin" bs=1 skip="$offset_dec" count="$size_dec" status=none
        sha256sum "$out_bin" > "$out_bin.sha256"

        echo "APP EXTRACTED: $out_bin"
      fi
    done
  else
    echo "No full_flash.bin or partitions_raw.csv, app extraction skipped"
  fi

  echo
  echo "===== RESTORE COMMANDS ====="
  {
    echo "# Полное восстановление flash:"
    echo "esptool.py --port $PORT write_flash 0x000000 full_flash.bin"
    echo
    echo "# Восстановление только таблицы разделов:"
    echo "esptool.py --port $PORT write_flash 0x8000 partitions.bin"
    echo
    echo "# Извлечённые бинарники прошивок лежат в ./extracted/"
    echo "# Записывать их обратно только по исходному адресу offset."
  } | tee "$OUT_DIR/restore_commands.txt"

} | tee "$RAW_LOG"

{
  echo "<!doctype html><html><head><meta charset='utf-8'>"
  echo "<title>ESP Device Probe $STAMP</title>"
  echo "<style>"
  echo "body{background:#111;color:#eee;font-family:Arial,sans-serif;padding:20px}"
  echo "pre{background:#1b1b1b;border:1px solid #555;padding:15px;border-radius:10px;white-space:pre-wrap}"
  echo "table{border-collapse:collapse;background:#1b1b1b;margin:15px 0}"
  echo "td,th{border:1px solid #555;padding:6px 10px}"
  echo "th{background:#333}"
  echo "</style>"
  echo "</head><body>"
  echo "<h1>ESP Device Probe</h1>"
  echo "<p><b>Порт:</b> $PORT</p>"
  echo "<p><b>Каталог:</b> $OUT_DIR</p>"

  echo "<h2>Таблица разделов</h2>"
  echo "<table>"
  if [ -f "$CSV" ]; then
    python3 - "$CSV" << 'PY'
import sys, csv, html

csv_path = sys.argv[1]

with open(csv_path, newline="", encoding="utf-8") as f:
    reader = csv.reader(f)
    rows = list(reader)

if rows:
    print("<tr>")
    for h in rows[0]:
        print(f"<th>{html.escape(h)}</th>")
    print("</tr>")

    for row in rows[1:]:
        print("<tr>")
        for cell in row:
            print(f"<td>{html.escape(cell)}</td>")
        print("</tr>")
PY
  fi
  echo "</table>"

  echo "<h2>Карта flash</h2><pre>"
  sed 's/&/\&amp;/g;s/</\&lt;/g;s/>/\&gt;/g' "$MAP"
  echo "</pre>"

  echo "<h2>Лог</h2><pre>"
  sed 's/&/\&amp;/g;s/</\&lt;/g;s/>/\&gt;/g' "$RAW_LOG"
  echo "</pre>"
  echo "</body></html>"
} > "$HTML"

echo
echo "DONE"
echo "OUT_DIR: $OUT_DIR"
echo "CSV RU : $CSV"
echo "CSV RAW: $OUT_DIR/partitions_raw.csv"
echo "MAP    : $MAP"
echo "HTML   : $HTML"
