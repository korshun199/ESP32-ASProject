#!/usr/bin/env python3
"""
Мост USB Serial -> буфер радара.

Назначение:
  - читать JSON-строки от ESP32 по USB Serial;
  - пропускать служебные строки, которые начинаются с #;
  - отправлять корректный JSON в сервер радара:
      POST /api/esp32/push

Используется для первого реального микрофона:
  микрофон -> ESP32 GPIO34 -> USB Serial -> этот скрипт -> server.py -> control.html
"""

import argparse
import json
import sys
import time
import urllib.request
import urllib.error

try:
    import serial
except ImportError:
    print("ОШИБКА: не найден Python-модуль 'serial'")
    print("Установить можно так:")
    print("  sudo apt install python3-serial")
    sys.exit(1)


def post_json(url, data, timeout=2.0):
    """Отправить JSON в сервер радара."""
    payload = json.dumps(data, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.status, resp.read().decode("utf-8", errors="replace")


def main():
    ap = argparse.ArgumentParser(
        description="Мост: ESP32 USB Serial с реальным микрофоном -> буфер радара"
    )
    ap.add_argument("--port", default="/dev/ttyUSB0", help="Serial-порт ESP32")
    ap.add_argument("--baud", type=int, default=115200, help="Скорость Serial")
    ap.add_argument("--server", default="http://127.0.0.1:8088", help="Адрес сервера радара")
    ap.add_argument("--print-json", action="store_true", help="Печатать полный JSON")
    args = ap.parse_args()

    url = args.server.rstrip("/") + "/api/esp32/push"

    print()
    print("=== Мост ESP32 Serial -> буфер радара ===")
    print("Порт:       ", args.port)
    print("Скорость:   ", args.baud)
    print("POST:       ", url)
    print("Остановить: Ctrl+C")
    print()

    ser = serial.Serial(args.port, args.baud, timeout=1)

    good = 0
    bad = 0

    while True:
        raw = ser.readline()
        if not raw:
            continue

        line = raw.decode("utf-8", errors="replace").strip()

        if not line:
            continue

        # Служебные строки прошивки начинаются с #.
        if line.startswith("#"):
            print(line)
            continue

        if not line.startswith("{"):
            bad += 1
            print(f"пропуск не-JSON bad={bad}: {line[:100]}")
            continue

        try:
            data = json.loads(line)
        except json.JSONDecodeError as e:
            bad += 1
            print(f"битый JSON bad={bad}: {e}: {line[:120]}")
            continue

        try:
            code, _ = post_json(url, data)
            good += 1

            mic = data.get("mic", {})
            seq = data.get("seq", "?")
            src = data.get("source", "?")
            raw_v = mic.get("raw", "?")
            p2p = mic.get("p2p", "?")
            vol = mic.get("volume", data.get("audio", {}).get("volume", "?"))

            print(
                f"post={code} seq={seq} src={src} raw={raw_v} p2p={p2p} vol={vol} good={good} bad={bad}",
                flush=True,
            )

            if args.print_json:
                print(json.dumps(data, ensure_ascii=False))

        except urllib.error.URLError as e:
            print(f"ошибка POST: {e}. Сервер радара запущен?", flush=True)
            time.sleep(0.5)


if __name__ == "__main__":
    main()
