#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
RADAR MIC DASHBOARD

Русское диагностическое табло для микрофонов ESP32 Radar.

Назначение:
- читать JSON-строки из Serial-порта ESP32;
- показывать состояние 5 микрофонов в табличном виде;
- помогать при долгой диагностике и юстировке;
- понятно сообщать, если порт не найден или ESP32 не подключена.

Запуск:
  ./scripts/radar_mic_dashboard.py --port /dev/ttyUSB0
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from datetime import datetime
from typing import Any

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ОШИБКА: не найден Python-модуль serial.")
    print("Установи его командой:")
    print("  sudo apt install python3-serial")
    raise SystemExit(1)


RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
MAGENTA = "\033[35m"
CYAN = "\033[36m"
CLEAR = "\033[2J\033[H"


MIC_NAMES = {
    1: "ВЕРХ",
    2: "ПРАВО",
    3: "НИЗ",
    4: "ЛЕВО",
    5: "ЦЕНТР/ОПОРН",
}


def color(text: str, ansi: str, enabled: bool = True) -> str:
    if not enabled:
        return text
    return f"{ansi}{text}{RESET}"


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def bar(value: float, width: int = 12, enabled: bool = True) -> str:
    value = clamp(float(value or 0.0), 0.0, 1.0)
    filled = int(round(value * width))
    text = "█" * filled + "░" * (width - filled)

    if not enabled:
        return text

    if value >= 0.70:
        return color(text, RED)
    if value >= 0.25:
        return color(text, YELLOW)
    if value > 0.02:
        return color(text, GREEN)
    return color(text, DIM)


def fmt_float(value: Any, digits: int = 3) -> str:
    try:
        return f"{float(value):.{digits}f}"
    except Exception:
        return "0.000"


def fmt_int(value: Any) -> str:
    try:
        return str(int(value))
    except Exception:
        return "0"


def list_ports_text() -> str:
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return "serial-порты не найдены"

    chunks = []
    for p in ports:
        desc = p.description or "без описания"
        chunks.append(f"{p.device} ({desc})")
    return ", ".join(chunks)


def normalize_mics(data: dict[str, Any]) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}

    for i in range(1, 6):
        result[i] = {
            "id": i,
            "name": MIC_NAMES.get(i, f"MIC{i}"),
            "present": False,
            "raw": 0,
            "min": 0,
            "max": 0,
            "center": 0,
            "p2p": 0,
            "volume": 0.0,
            "freq": 0,
        }

    incoming = data.get("mics", [])
    if isinstance(incoming, list):
        for item in incoming:
            if not isinstance(item, dict):
                continue

            try:
                mic_id = int(item.get("id", 0))
            except Exception:
                continue

            if mic_id not in result:
                continue

            result[mic_id].update({
                "present": True,
                "name": item.get("name") or result[mic_id]["name"],
                "raw": item.get("raw", 0),
                "min": item.get("min", 0),
                "max": item.get("max", 0),
                "center": item.get("center", 0),
                "p2p": item.get("p2p", 0),
                "volume": item.get("volume", 0.0),
                "freq": item.get("freq", 0),
            })

    # ВАЖНО:
    # audio — это общая сводка по звуку, а не отдельный пятый микрофон.
    # MIC5 считается живым только если ESP32 реально прислала mics[].id == 5.
    return result


def parse_json_line(line: str) -> dict[str, Any] | None:
    line = line.strip()
    if not line:
        return None

    if line.startswith("#"):
        return None

    try:
        data = json.loads(line)
    except json.JSONDecodeError:
        return None

    if not isinstance(data, dict):
        return None

    return data


def render_dashboard(
    *,
    port: str,
    baud: int,
    connected: bool,
    last_data: dict[str, Any] | None,
    last_rx_time: float | None,
    packets_ok: int,
    packets_bad: int,
    last_bad_line: str,
    no_color: bool,
) -> None:
    now = time.time()
    now_text = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    if last_rx_time is None:
        age_ms_text = "нет данных"
        data_is_fresh = False
    else:
        age_ms = int((now - last_rx_time) * 1000)
        age_ms_text = f"{age_ms} мс"
        data_is_fresh = age_ms < 2000

    status_text = "ЕСТЬ" if connected else "НЕТ"
    if connected and data_is_fresh:
        status_text = color("ЕСТЬ", GREEN, not no_color)
    elif connected:
        status_text = color("ПОРТ ЕСТЬ, ДАННЫЕ МОЛЧАТ", YELLOW, not no_color)
    else:
        status_text = color("НЕТ", RED, not no_color)

    print(CLEAR, end="")
    print(color("ESP32 RADAR MIC DASHBOARD", BOLD + CYAN, not no_color))
    print(
        f"Порт: {color(port, BOLD, not no_color)} | "
        f"Скорость: {baud} | "
        f"Связь: {status_text} | "
        f"Возраст данных: {age_ms_text}"
    )

    if not connected:
        print()
        print(color("USB/Serial порт не открыт.", RED, not no_color))
        print("Проверь:")
        print("  1. ESP32 подключена к USB?")
        print("  2. Правильный порт указан?")
        print("  3. Не занят ли порт другим монитором?")
        print()
        print(f"Доступные порты: {list_ports_text()}")
        print()
        print(f"Время: {now_text}")
        return

    data = last_data or {}
    mics = normalize_mics(data)

    audio = data.get("audio", {})
    if not isinstance(audio, dict):
        audio = {}

    seq = data.get("seq", "-")
    source = data.get("source", "-")

    print(f"Источник: {source} | seq: {seq}")
    print()

    print("┌─────┬──────────────┬────────┬──────┬──────┬──────┬──────┬────────┬──────────────┬────────┐")
    print("│ MIC │ Имя          │ Статус │ RAW  │ MIN  │ MAX  │ P2P  │ Громк. │ Индикатор    │ Част.  │")
    print("├─────┼──────────────┼────────┼──────┼──────┼──────┼──────┼────────┼──────────────┼────────┤")

    for mic_id in range(1, 6):
        mic = mics[mic_id]
        present = bool(mic.get("present"))
        volume = float(mic.get("volume") or 0.0)

        raw_i = int(float(mic.get("raw") or 0))
        min_i = int(float(mic.get("min") or 0))
        max_i = int(float(mic.get("max") or 0))
        p2p_i = int(float(mic.get("p2p") or 0))

        looks_empty = (
            raw_i == 0
            and min_i == 0
            and max_i == 0
            and p2p_i == 0
            and volume <= 0.0
        )

        if not present or looks_empty:
            status = color("НЕТ", DIM, not no_color)
        elif volume > 0.02 or p2p_i > 20:
            status = color("ЖИВ", GREEN, not no_color)
        else:
            status = color("ТИХО", YELLOW, not no_color)

        name = str(mic.get("name") or MIC_NAMES.get(mic_id, f"MIC{mic_id}"))[:12]
        raw = fmt_int(mic.get("raw"))
        vmin = fmt_int(mic.get("min"))
        vmax = fmt_int(mic.get("max"))
        p2p = fmt_int(mic.get("p2p"))
        freq = fmt_int(mic.get("freq"))
        vol_bar = bar(volume, 12, not no_color)
        vol_num = fmt_float(volume, 2)

        print(
            f"│ {mic_id:<3} │ "
            f"{name:<12} │ "
            f"{status:<17}│ "
            f"{raw:>4} │ "
            f"{vmin:>4} │ "
            f"{vmax:>4} │ "
            f"{p2p:>4} │ "
            f"{vol_num:>6} │ "
            f"{vol_bar} │ "
            f"{freq:>6} │"
        )

    print("└─────┴──────────────┴────────┴──────┴──────┴──────┴──────┴────────┴──────────────┴────────┘")
    print()

    # Общая громкость: если audio.volume не пришёл или равен нулю,
    # берём максимум по реально присутствующим микрофонам.
    mic_volumes = [
        float(m.get("volume") or 0.0)
        for m in mics.values()
        if m.get("present")
    ]

    audio_volume = audio.get("volume", None)
    try:
        audio_volume = float(audio_volume)
    except Exception:
        audio_volume = 0.0

    if audio_volume <= 0.0 and mic_volumes:
        audio_volume = max(mic_volumes)

    audio_freq = audio.get("freq", 0)

    print(
        f"Аудио: громкость {fmt_float(audio_volume, 2)} "
        f"{bar(float(audio_volume or 0.0), 20, not no_color)} | "
        f"частота {fmt_int(audio_freq)} Гц"
    )

    print(
        f"Пакеты: принято {packets_ok} | "
        f"ошибки/мусор {packets_bad} | "
        f"время {now_text}"
    )

    if last_bad_line:
        trimmed = last_bad_line[:120]
        print(color(f"Последняя непонятная строка: {trimmed}", DIM, not no_color))


def open_serial(port: str, baud: int, timeout: float):
    return serial.Serial(port=port, baudrate=baud, timeout=timeout)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Русское табло диагностики микрофонов ESP32 Radar."
    )
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial-порт, по умолчанию /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200, help="Скорость порта, по умолчанию 115200")
    parser.add_argument("--refresh", type=float, default=0.25, help="Период обновления экрана в секундах")
    parser.add_argument("--no-color", action="store_true", help="Отключить цвета")
    args = parser.parse_args()

    ser = None
    connected = False
    last_data: dict[str, Any] | None = None
    last_rx_time: float | None = None
    packets_ok = 0
    packets_bad = 0
    last_bad_line = ""
    last_render = 0.0

    try:
        while True:
            if ser is None:
                try:
                    ser = open_serial(args.port, args.baud, timeout=0.05)
                    connected = True
                    last_bad_line = ""
                    time.sleep(0.2)
                except Exception as exc:
                    connected = False
                    now = time.time()
                    if now - last_render >= args.refresh:
                        render_dashboard(
                            port=args.port,
                            baud=args.baud,
                            connected=False,
                            last_data=last_data,
                            last_rx_time=last_rx_time,
                            packets_ok=packets_ok,
                            packets_bad=packets_bad,
                            last_bad_line=str(exc),
                            no_color=args.no_color,
                        )
                        last_render = now
                    time.sleep(1.0)
                    continue

            try:
                raw = ser.readline()
            except Exception as exc:
                connected = False
                last_bad_line = f"ошибка чтения порта: {exc}"
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
                continue

            if raw:
                try:
                    line = raw.decode("utf-8", errors="replace").strip()
                except Exception:
                    line = repr(raw)

                data = parse_json_line(line)
                if data is None:
                    if line and not line.startswith("#"):
                        packets_bad += 1
                        last_bad_line = line
                else:
                    packets_ok += 1
                    last_data = data
                    last_rx_time = time.time()

            now = time.time()
            if now - last_render >= args.refresh:
                render_dashboard(
                    port=args.port,
                    baud=args.baud,
                    connected=connected,
                    last_data=last_data,
                    last_rx_time=last_rx_time,
                    packets_ok=packets_ok,
                    packets_bad=packets_bad,
                    last_bad_line=last_bad_line,
                    no_color=args.no_color,
                )
                last_render = now

    except KeyboardInterrupt:
        print()
        print("Остановлено пользователем.")
        return 0
    finally:
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
