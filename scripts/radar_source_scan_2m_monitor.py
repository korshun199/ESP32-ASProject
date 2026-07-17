#!/usr/bin/env python3

import re
import threading
import time
import traceback
from datetime import datetime
from pathlib import Path

import serial

PORT = "/dev/ttyACM0"
BAUD = 115200

FREQUENCY_HZ = 2000
SOURCE_DISTANCE_M = 2.0

BACKGROUND_SECONDS = 10.0
PREP_SECONDS = 5.0
SWEEP_SECONDS = 60.0
TAIL_SECONDS = 1.0
DEGREES_PER_SECOND = 360.0 / SWEEP_SECONDS

RESET = "\033[0m"
BOLD = "\033[1m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
MAGENTA = "\033[35m"

COMPARE_TEST_MS_RE = re.compile(r"\bTEST_MS=(\d+)\b")


def wall_time():
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def banner(title, instruction, color):
    print()
    print(color + BOLD + "=" * 78 + RESET, flush=True)
    print(color + BOLD + title.center(78) + RESET, flush=True)
    print(color + BOLD + instruction.center(78) + RESET, flush=True)
    print(color + BOLD + "=" * 78 + RESET, flush=True)
    print("\a", end="", flush=True)


def run_monitor():
    output_dir = Path("/home/work/ESP32-ASProject/device_reports")
    output_dir.mkdir(parents=True, exist_ok=True)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_path = output_dir / f"radar_source_scan_2m_2000hz_{timestamp}.log"

    stop_reader = threading.Event()
    reader_errors = []
    log_lock = threading.Lock()

    state_lock = threading.Lock()
    state = {
        "program_start": None,
        "sweep_start": None,
        "sweep_active": False,
        "latest_test_ms": None,
    }

    ser = None
    log_file = None
    reader_thread = None

    def write_log(text):
        with log_lock:
            log_file.write(text.rstrip("\r\n") + "\n")
            log_file.flush()

    def marker(name, **fields):
        parts = [
            "### MONITOR_MARKER",
            f"NAME={name}",
            f"WALL_TIME={wall_time()}",
            f"FREQ_HZ={FREQUENCY_HZ}",
            f"SOURCE_DISTANCE_M={SOURCE_DISTANCE_M:.1f}",
            "PLANE=HORIZONTAL_ROTATION",
            "ROTATION_CENTER=MICROPHONE_NODE_AXIS",
            "ZERO_DEG=TUBE_AXIS_POINTS_TO_SOURCE",
            "ROTATION=CLOCKWISE_TOP_VIEW",
            "SOURCE=FIXED",
            "SOURCE_AIMED_AT_NODE",
        ]
        for key, value in fields.items():
            parts.append(f"{key}={value}")
        write_log(" ".join(parts))

    def reader():
        while not stop_reader.is_set():
            try:
                raw = ser.readline()
            except Exception:
                reader_errors.append(traceback.format_exc())
                stop_reader.set()
                return

            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            now = time.monotonic()

            match = COMPARE_TEST_MS_RE.search(line)
            if match:
                firmware_test_ms = int(match.group(1))

                with state_lock:
                    state["latest_test_ms"] = firmware_test_ms
                    sweep_start = state["sweep_start"]
                    sweep_active = state["sweep_active"]

                if sweep_active and sweep_start is not None:
                    sweep_elapsed = max(0.0, now - sweep_start)
                    angle = min(360.0, sweep_elapsed * DEGREES_PER_SECOND)

                    write_log(
                        "### SWEEP_FRAME "
                        f"WALL_TIME={wall_time()} "
                        f"SWEEP_ELAPSED={sweep_elapsed:.6f} "
                        f"ANGLE_DEG={angle:.3f} "
                        f"ANGULAR_SPEED_DEG_S={DEGREES_PER_SECOND:.6f} "
                        f"FIRMWARE_TEST_MS={firmware_test_ms}"
                    )

            write_log(line)

    def wait_seconds(duration, label, color):
        start = time.monotonic()
        last_whole = None

        while True:
            elapsed = time.monotonic() - start
            remaining = duration - elapsed
            if remaining <= 0:
                break

            whole = int(remaining + 0.999)
            if whole != last_whole:
                last_whole = whole
                print(
                    color
                    + BOLD
                    + f"\r{label}: осталось {remaining:4.1f} с   "
                    + RESET,
                    end="",
                    flush=True,
                )

            if reader_errors:
                raise RuntimeError(reader_errors[0])

            time.sleep(0.03)

        print("\r" + " " * 72 + "\r", end="", flush=True)

    try:
        print(CYAN + BOLD + "\nПОИСК ИСТОЧНИКА: ВРАЩЕНИЕ УЗЛА 360° ЗА 60 СЕКУНД, 2000 ГЦ" + RESET)
        print()
        print("Геометрия опыта:")
        print("  • источник звука неподвижен в точке A;")
        print("  • микрофонный узел находится в точке B и вращается вокруг своей оси;")
        print("  • расстояние A–B равно 2 метрам;")
        print("  • источник и ось микрофонного узла находятся примерно на одной высоте;")
        print("  • источник всё время направлен на микрофонный узел;")
        print("  • стартовая точка 0° — трубка направлена точно на источник;")
        print("  • вращать узел по часовой стрелке при взгляде сверху.")
        print()
        print("Скорость:")
        print("  • один полный круг ровно за 60 секунд;")
        print("  • это 6 градусов в секунду;")
        print("  • каждые 10 секунд будет звуковая отметка очередных 60°.")
        print()
        print("Сценарий:")
        print("  1. 10 секунд естественного фона, генератор выключен;")
        print("  2. 5 секунд на включение 2000 Гц и установку узла в 0°;")
        print("  3. по команде ПОЕХАЛИ равномерно вращать узел полный круг за минуту;")
        print("  4. скрипт сам остановит измерение через 60 секунд.")
        print()
        print("В точке 30 секунд трубка должна смотреть строго от источника.")
        print("В точке 60 секунд она должна вернуться к источнику.")
        print("Ctrl+C можно нажать только для аварийной остановки.")
        print(GREEN + f"\nЛог: {log_path}" + RESET)

        ser = serial.Serial(
            port=PORT,
            baudrate=BAUD,
            timeout=0.12,
            write_timeout=1.0,
            dsrdtr=False,
            rtscts=False,
        )
        ser.dtr = False
        ser.rts = False

        log_file = log_path.open("w", encoding="utf-8", buffering=1)

        write_log(
            "### MONITOR_HEADER "
            f"WALL_TIME={wall_time()} PORT={PORT} BAUD={BAUD} "
            f"FREQ_HZ={FREQUENCY_HZ} SOURCE_DISTANCE_M={SOURCE_DISTANCE_M:.1f} "
            "MODE=FIXED_SOURCE_ROTATING_NODE_360 "
            f"BACKGROUND_SECONDS={BACKGROUND_SECONDS:.1f} "
            f"PREP_SECONDS={PREP_SECONDS:.1f} "
            f"SWEEP_SECONDS={SWEEP_SECONDS:.1f} "
            f"ANGULAR_SPEED_DEG_S={DEGREES_PER_SECOND:.6f} "
            "PLANE=HORIZONTAL_ROTATION ROTATION_CENTER=MICROPHONE_NODE_AXIS "
            "ZERO_DEG=TUBE_AXIS_POINTS_TO_SOURCE "
            "ROTATION=CLOCKWISE_TOP_VIEW SOURCE=FIXED "
            "SOURCE_AIMED_AT_NODE "
            "SOURCE_DISTANCE_REFERENCE=SOURCE_TO_ROTATION_AXIS "
            "GAIN=BACKGROUND_ONLY_FROZEN_AFTER_10S"
        )

        reader_thread = threading.Thread(target=reader, daemon=True)
        reader_thread.start()

        marker(
            "WAIT",
            NOTE="GENERATOR_OFF_NODE_AT_ZERO_DEG_TUBE_POINTS_TO_SOURCE_PRESS_ENTER",
        )

        input(
            YELLOW
            + BOLD
            + "\nГенератор ВЫКЛЮЧЕН, трубка направлена на источник. Нажми Enter... "
            + RESET
        )

        for second in (3, 2, 1):
            print(
                MAGENTA + BOLD + f"\rНачало фонового этапа через {second}... " + RESET,
                end="",
                flush=True,
            )
            time.sleep(1.0)
        print("\r" + " " * 72 + "\r", end="", flush=True)

        ser.reset_input_buffer()
        ser.write(b"T")
        ser.flush()

        program_start = time.monotonic()
        with state_lock:
            state["program_start"] = program_start

        marker("BACKGROUND_START", PROGRAM_ELAPSED="0.000")
        banner(
            "ФОН, 10 СЕКУНД",
            "ГЕНЕРАТОР ВЫКЛЮЧЕН. УЗЕЛ И ИСТОЧНИК НЕ ДВИГАТЬ.",
            CYAN,
        )
        wait_seconds(BACKGROUND_SECONDS, "ФОН", CYAN)

        background_end = time.monotonic()
        with state_lock:
            latest_test_ms = state["latest_test_ms"]

        marker(
            "BACKGROUND_END",
            PROGRAM_ELAPSED=f"{background_end - program_start:.6f}",
            FIRMWARE_TEST_MS=latest_test_ms if latest_test_ms is not None else "UNKNOWN",
        )

        marker(
            "PREP_START",
            PROGRAM_ELAPSED=f"{background_end - program_start:.6f}",
            NOTE="TURN_ON_2000HZ_KEEP_NODE_AT_ZERO_DEG",
        )
        banner(
            "ПОДГОТОВКА, 5 СЕКУНД",
            "ВКЛЮЧИТЬ 2000 ГЦ. ТРУБКА ОСТАЁТСЯ НАПРАВЛЕНА НА ИСТОЧНИК.",
            YELLOW,
        )
        wait_seconds(PREP_SECONDS, "ПОДГОТОВКА", YELLOW)

        sweep_start = time.monotonic()
        with state_lock:
            state["sweep_start"] = sweep_start
            state["sweep_active"] = True
            latest_test_ms = state["latest_test_ms"]

        marker(
            "SWEEP_START",
            PROGRAM_ELAPSED=f"{sweep_start - program_start:.6f}",
            SWEEP_ELAPSED="0.000000",
            ANGLE_DEG="0.000",
            FIRMWARE_TEST_MS=latest_test_ms if latest_test_ms is not None else "UNKNOWN",
        )

        banner(
            "ПОЕХАЛИ: 0°",
            "РАВНОМЕРНО ВРАЩАТЬ УЗЕЛ ПО ЧАСОВОЙ СТРЕЛКЕ. КРУГ ЗА 60 СЕКУНД.",
            GREEN,
        )

        last_second = -1
        last_major = -1

        while True:
            now = time.monotonic()
            sweep_elapsed = now - sweep_start

            if sweep_elapsed >= SWEEP_SECONDS:
                break

            angle = sweep_elapsed * DEGREES_PER_SECOND
            second = int(sweep_elapsed)

            if second != last_second:
                last_second = second
                marker(
                    "SWEEP_TICK",
                    SWEEP_ELAPSED=f"{sweep_elapsed:.6f}",
                    ANGLE_DEG=f"{angle:.3f}",
                )

                print(
                    GREEN
                    + BOLD
                    + f"\rВремя {sweep_elapsed:5.1f}/60.0 с   "
                      f"угол ≈ {angle:6.1f}°   "
                      f"до конца {SWEEP_SECONDS - sweep_elapsed:4.1f} с   "
                    + RESET,
                    end="",
                    flush=True,
                )

            major = int(sweep_elapsed // 10)
            if major != last_major:
                last_major = major
                if major > 0:
                    major_angle = major * 60
                    print("\a", end="", flush=True)
                    marker(
                        "MAJOR_ANGLE",
                        SWEEP_ELAPSED=f"{major * 10.0:.3f}",
                        ANGLE_DEG=f"{major_angle:.1f}",
                    )

            if reader_errors:
                raise RuntimeError(reader_errors[0])

            time.sleep(0.02)

        print("\r" + " " * 78 + "\r", end="", flush=True)

        sweep_end = time.monotonic()
        with state_lock:
            state["sweep_active"] = False
            latest_test_ms = state["latest_test_ms"]

        marker(
            "SWEEP_END",
            PROGRAM_ELAPSED=f"{sweep_end - program_start:.6f}",
            SWEEP_ELAPSED=f"{sweep_end - sweep_start:.6f}",
            ANGLE_DEG="360.000",
            FIRMWARE_TEST_MS=latest_test_ms if latest_test_ms is not None else "UNKNOWN",
        )

        banner(
            "КРУГ ЗАВЕРШЁН: 360°",
            "ОСТАНОВИТЬ УЗЕЛ В ТОЧКЕ 0° И ВЫКЛЮЧИТЬ ГЕНЕРАТОР.",
            MAGENTA,
        )

        time.sleep(TAIL_SECONDS)

        marker(
            "TEST_COMPLETE",
            PROGRAM_ELAPSED=f"{time.monotonic() - program_start:.6f}",
            STATUS="COMPLETE",
        )
        write_log(
            f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=COMPLETE "
            f"LOG={log_path}"
        )

        print(GREEN + BOLD + "\nЗапись завершена автоматически." + RESET)
        print(GREEN + str(log_path) + RESET)

    except KeyboardInterrupt:
        with state_lock:
            sweep_start = state["sweep_start"]
            state["sweep_active"] = False

        if sweep_start is None:
            sweep_elapsed = 0.0
            angle = 0.0
        else:
            sweep_elapsed = max(0.0, time.monotonic() - sweep_start)
            angle = min(360.0, sweep_elapsed * DEGREES_PER_SECOND)

        print(RED + BOLD + "\n\nТест остановлен через Ctrl+C." + RESET)

        if log_file:
            marker(
                "USER_ABORT",
                SWEEP_ELAPSED=f"{sweep_elapsed:.6f}",
                ANGLE_DEG=f"{angle:.3f}",
            )
            write_log(
                f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=ABORTED "
                f"SWEEP_ELAPSED={sweep_elapsed:.6f} ANGLE_DEG={angle:.3f}"
            )

    except Exception:
        print(RED + BOLD + "\n\nОШИБКА:" + RESET)
        traceback.print_exc()

        if log_file:
            write_log(
                f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=ERROR"
            )

    finally:
        stop_reader.set()

        if reader_thread:
            reader_thread.join(timeout=1.0)

        if ser:
            try:
                ser.close()
            except Exception:
                traceback.print_exc()

        if log_file:
            try:
                log_file.close()
            except Exception:
                traceback.print_exc()

        input("\nНажми Enter после того, как прочитаешь вывод...")


run_monitor()
