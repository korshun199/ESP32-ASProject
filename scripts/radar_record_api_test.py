#!/usr/bin/env python3
import csv
import json
import time
import urllib.request
from datetime import datetime
from pathlib import Path

URL = "http://192.168.4.1/api/latest"
OUT_DIR = Path("logs")
OUT_DIR.mkdir(exist_ok=True)

STAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
OUT_FILE = OUT_DIR / f"radar_manual_freq_test_{STAMP}.csv"

INTERVAL = 0.5
DURATION = 30

MENU = {
    "0": ("silence_before", 0, "30 сек тишина ДО теста"),
    "1": ("generator_400hz", 400, "30 сек генератор 400 Гц"),
    "2": ("silence_middle", 0, "30 сек тишина МЕЖДУ частотами"),
    "3": ("generator_1000hz", 1000, "30 сек генератор 1000 Гц"),
    "4": ("silence_after", 0, "30 сек финальная тишина"),
}

FIELDS = [
    "timestamp",
    "phase",
    "target_freq_hz",
    "elapsed_s",
    "seq",
    "mic_id",
    "raw",
    "min",
    "max",
    "center",
    "p2p",
    "volume",
    "samples",
    "crossings",
    "freq",
    "audio_volume",
    "audio_freq",
]

def fetch_json():
    with urllib.request.urlopen(URL, timeout=2) as r:
        raw = r.read().decode("utf-8", errors="replace").strip()

    if not raw:
        raise RuntimeError("пустой ответ от ESP32")

    return json.loads(raw)

def read_sample(phase, target_freq_hz, elapsed):
    data = fetch_json()
    mic = data.get("mics", [{}])[0]
    audio = data.get("audio", {})

    return {
        "timestamp": datetime.now().isoformat(timespec="milliseconds"),
        "phase": phase,
        "target_freq_hz": target_freq_hz,
        "elapsed_s": round(elapsed, 3),
        "seq": data.get("seq"),
        "mic_id": mic.get("id"),
        "raw": mic.get("raw"),
        "min": mic.get("min"),
        "max": mic.get("max"),
        "center": mic.get("center"),
        "p2p": mic.get("p2p"),
        "volume": mic.get("volume"),
        "samples": mic.get("samples"),
        "crossings": mic.get("crossings"),
        "freq": mic.get("freq"),
        "audio_volume": audio.get("volume"),
        "audio_freq": audio.get("freq"),
    }

def record_phase(writer, csv_file, phase, target_freq_hz, label):
    print()
    print(f"=== СТАРТ: {label} ===")
    print(f"Фаза: {phase}")
    print(f"Целевая частота: {target_freq_hz} Гц")
    print(f"Длительность: {DURATION} сек")
    print()

    phase_start = time.monotonic()
    next_tick = phase_start

    ok = 0
    fail = 0

    while True:
        now = time.monotonic()
        elapsed = now - phase_start

        if elapsed >= DURATION:
            break

        try:
            row = read_sample(phase, target_freq_hz, elapsed)
            writer.writerow(row)
            csv_file.flush()
            ok += 1

            print(
                f"\r{elapsed:5.1f}/{DURATION}s "
                f"phase={phase:16s} "
                f"target={target_freq_hz:4d}Hz "
                f"vol={row.get('volume')} "
                f"p2p={row.get('p2p')} "
                f"freq={row.get('freq')}Hz      ",
                end="",
                flush=True
            )

        except Exception as e:
            fail += 1
            print(
                f"\r{elapsed:5.1f}/{DURATION}s "
                f"ERROR: {e}      ",
                end="",
                flush=True
            )

        next_tick += INTERVAL
        sleep_for = next_tick - time.monotonic()
        if sleep_for > 0:
            time.sleep(sleep_for)

    print()
    print(f"=== ГОТОВО: {label} | ok={ok}, fail={fail} ===")
    print()

def print_menu(done):
    print()
    print("========================================")
    print(" ESP32 Radar ручной замер частоты")
    print("========================================")
    print(f"Файл: {OUT_FILE}")
    print()
    print("Команды:")
    for key, (_, _, label) in MENU.items():
        mark = "✅" if key in done else "  "
        print(f"  {key} -> {label} {mark}")
    print("  q -> выход")
    print()
    print("Подсказка:")
    print("  перед 0: тишина")
    print("  перед 1: включи 400 Гц")
    print("  перед 2: выключи генератор")
    print("  перед 3: включи 1000 Гц")
    print("  перед 4: выключи генератор")
    print("========================================")
    print()

def main():
    print("ESP32 Radar manual API recorder")
    print(f"URL:  {URL}")
    print(f"CSV:  {OUT_FILE}")
    print()
    print("Проверь, что ноут подключён к Wi-Fi ESP32-RADAR.")
    print("Каждый пункт меню пишет ровно 30 секунд.")
    print()

    done = set()

    with OUT_FILE.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=FIELDS)
        writer.writeheader()
        csv_file.flush()

        while True:
            print_menu(done)
            cmd = input("Выбор > ").strip().lower()

            if cmd == "q":
                print("Выход.")
                break

            if cmd not in MENU:
                print("Нет такой команды. Да, цифры всего пять, но техника всё равно требует дисциплины.")
                continue

            phase, target_freq_hz, label = MENU[cmd]

            print()
            print(f"Вы выбрали: {label}")
            confirm = input("Нажми Enter для старта или n + Enter для отмены > ").strip().lower()

            if confirm == "n":
                print("Отменено.")
                continue

            record_phase(writer, csv_file, phase, target_freq_hz, label)
            done.add(cmd)

    print()
    print("Готово.")
    print(f"CSV сохранён: {OUT_FILE}")
    print()
    print("Посмотреть файл:")
    print(f"  column -s, -t < {OUT_FILE} | less -S")
    print()
    print("Последние строки:")
    print(f"  tail -30 {OUT_FILE}")

if __name__ == "__main__":
    main()
