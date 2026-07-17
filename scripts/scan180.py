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

FREQUENCY_HZ = 1000
SOURCE_DISTANCE_M = 2.0

QUIET_SECONDS = 10.0
SOURCE_HOLD_SECONDS = 20.0
TURN_SECONDS = 30.0
SIDE_HOLD_SECONDS = 20.0
TURN_DEGREES = 180.0
TURN_SPEED_DEG_S = TURN_DEGREES / TURN_SECONDS

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
    log_path = output_dir / f"radar_source_step_0_to_180_1000hz_{timestamp}.log"

    stop_reader = threading.Event()
    reader_errors = []
    log_lock = threading.Lock()
    state_lock = threading.Lock()

    state = {
        "phase": "WAIT",
        "phase_start": None,
        "angle_deg": 0.0,
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
            "SOURCE=FIXED",
            "SOURCE_AIMED_AT_NODE",
        ]
        for key, value in fields.items():
            parts.append(f"{key}={value}")
        write_log(" ".join(parts))

    def set_phase(name, angle_deg):
        with state_lock:
            state["phase"] = name
            state["phase_start"] = time.monotonic()
            state["angle_deg"] = angle_deg

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
            match = COMPARE_TEST_MS_RE.search(line)

            if match:
                firmware_test_ms = int(match.group(1))
                now = time.monotonic()

                with state_lock:
                    state["latest_test_ms"] = firmware_test_ms
                    phase = state["phase"]
                    phase_start = state["phase_start"]
                    angle_deg = state["angle_deg"]

                phase_elapsed = 0.0 if phase_start is None else max(0.0, now - phase_start)

                if phase == "TURN_0_TO_180":
                    angle_deg = min(TURN_DEGREES, phase_elapsed * TURN_SPEED_DEG_S)
                    with state_lock:
                        state["angle_deg"] = angle_deg

                write_log(
                    "### TEST_FRAME "
                    f"WALL_TIME={wall_time()} "
                    f"PHASE={phase} "
                    f"PHASE_ELAPSED={phase_elapsed:.6f} "
                    f"ANGLE_DEG={angle_deg:.3f} "
                    f"FIRMWARE_TEST_MS={firmware_test_ms}"
                )

            write_log(line)

    def wait_seconds(duration, label, color, fixed_angle=None):
        start = time.monotonic()
        last_whole = None

        while True:
            elapsed = time.monotonic() - start
            remaining = duration - elapsed
            if remaining <= 0:
                break

            if fixed_angle is not None:
                with state_lock:
                    state["angle_deg"] = fixed_angle

            whole = int(remaining + 0.999)
            if whole != last_whole:
                last_whole = whole
                print(
                    color + BOLD
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
        print(CYAN + BOLD + "\nТЕСТ НА 1000 ГЦ: 0° → ПЛАВНЫЙ ПОВОРОТ НА 180°" + RESET)
        print()
        print("Геометрия:")
        print("  • источник неподвижен на расстоянии 2 метра;")
        print("  • 0° — трубка направлена точно на источник;")
        print("  • 180° — трубка смотрит строго от источника;")
        print("  • меняется только угол микрофонного узла.")
        print()
        print("Сценарий:")
        print("  1. 10 секунд тишины, генератор выключен;")
        print("  2. 20 секунд источник 1000 Гц, трубка на 0°;")
        print("  3. плавный поворот 0° → 180° за 30 секунд;")
        print("  4. 20 секунд удержание на 180°.")
        print()
        print("В лог попадут все FRAME_A, FRAME_B, FRAME_TARGET и COMPARE.")
        print("Перед каждым COMPARE будет точная фаза, время и расчётный угол.")
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
            "MODE=SOURCE_HOLD_THEN_TURN_180 "
            f"QUIET_SECONDS={QUIET_SECONDS:.1f} "
            f"SOURCE_HOLD_SECONDS={SOURCE_HOLD_SECONDS:.1f} "
            f"TURN_SECONDS={TURN_SECONDS:.1f} "
            f"SIDE_HOLD_SECONDS={SIDE_HOLD_SECONDS:.1f} "
            f"TURN_SPEED_DEG_S={TURN_SPEED_DEG_S:.6f} "
            "PLANE=HORIZONTAL_ROTATION ROTATION_CENTER=MICROPHONE_NODE_AXIS "
            "ZERO_DEG=TUBE_AXIS_POINTS_TO_SOURCE "
            "SOURCE=FIXED SOURCE_AIMED_AT_NODE "
            "SOURCE_DISTANCE_REFERENCE=SOURCE_TO_ROTATION_AXIS"
        )

        reader_thread = threading.Thread(target=reader, daemon=True)
        reader_thread.start()

        marker("WAIT", NOTE="GENERATOR_OFF_NODE_AT_ZERO_DEG_PRESS_ENTER")
        input(
            YELLOW + BOLD
            + "\nГенератор выключен, трубка направлена на источник. Нажми Enter... "
            + RESET
        )

        for second in (3, 2, 1):
            print(
                MAGENTA + BOLD + f"\rНачало теста через {second}... " + RESET,
                end="",
                flush=True,
            )
            time.sleep(1.0)
        print("\r" + " " * 72 + "\r", end="", flush=True)

        ser.reset_input_buffer()
        ser.write(b"T")
        ser.flush()

        set_phase("QUIET", 0.0)
        marker("QUIET_START", ANGLE_DEG="0.000", NOTE="GENERATOR_OFF")
        banner("ТИШИНА, 10 СЕКУНД", "ГЕНЕРАТОР ВЫКЛЮЧЕН. НИЧЕГО НЕ ДВИГАТЬ.", CYAN)
        wait_seconds(QUIET_SECONDS, "ТИШИНА", CYAN, fixed_angle=0.0)
        marker("QUIET_END", ANGLE_DEG="0.000")

        set_phase("SOURCE_HOLD_0", 0.0)
        marker("SOURCE_HOLD_START", ANGLE_DEG="0.000", NOTE="TURN_ON_1000HZ")
        banner(
            "ИСТОЧНИК 0°, 20 СЕКУНД",
            "ВКЛЮЧИТЬ 1000 ГЦ. ТРУБКА ТОЧНО НА ИСТОЧНИК.",
            GREEN,
        )
        wait_seconds(SOURCE_HOLD_SECONDS, "ИСТОЧНИК 0°", GREEN, fixed_angle=0.0)
        marker("SOURCE_HOLD_END", ANGLE_DEG="0.000")

        set_phase("TURN_0_TO_180", 0.0)
        marker(
            "TURN_START",
            FROM_ANGLE_DEG="0.000",
            TO_ANGLE_DEG="180.000",
            TURN_SECONDS=f"{TURN_SECONDS:.1f}",
            TURN_SPEED_DEG_S=f"{TURN_SPEED_DEG_S:.6f}",
        )
        banner(
            "ПОВОРОТ 0° → 180°, 15 СЕКУНД",
            "ПЛАВНО ПОВОРАЧИВАТЬ УЗЕЛ. РАССТОЯНИЕ И НАКЛОН НЕ МЕНЯТЬ.",
            YELLOW,
        )

        turn_start = time.monotonic()
        last_second = -1

        while True:
            elapsed = time.monotonic() - turn_start
            if elapsed >= TURN_SECONDS:
                break

            angle = min(TURN_DEGREES, elapsed * TURN_SPEED_DEG_S)
            with state_lock:
                state["angle_deg"] = angle

            second = int(elapsed)
            if second != last_second:
                last_second = second
                print(
                    YELLOW + BOLD
                    + f"\rПоворот {elapsed:4.1f}/{TURN_SECONDS:.1f} с   "
                      f"угол ≈ {angle:5.1f}°   "
                    + RESET,
                    end="",
                    flush=True,
                )

            if reader_errors:
                raise RuntimeError(reader_errors[0])

            time.sleep(0.02)

        print("\r" + " " * 72 + "\r", end="", flush=True)
        marker("TURN_END", ANGLE_DEG="180.000")

        set_phase("SIDE_HOLD_180", 180.0)
        marker("SIDE_HOLD_START", ANGLE_DEG="180.000")
        banner(
            "УДЕРЖАНИЕ 180°, 20 СЕКУНД",
            "НЕ ДВИГАТЬ УЗЕЛ. ТРУБКА СМОТРИТ СТРОГО ОТ ИСТОЧНИКА.",
            MAGENTA,
        )
        wait_seconds(SIDE_HOLD_SECONDS, "УДЕРЖАНИЕ 180°", MAGENTA, fixed_angle=180.0)
        marker("SIDE_HOLD_END", ANGLE_DEG="180.000")

        set_phase("COMPLETE", 180.0)
        marker("TEST_COMPLETE", ANGLE_DEG="180.000", STATUS="COMPLETE")
        write_log(f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=COMPLETE")

        print(GREEN + BOLD + "\nТЕСТ ЗАВЕРШЁН." + RESET)
        print(f"Лог сохранён: {log_path}")

    except KeyboardInterrupt:
        if log_file is not None:
            marker("ABORTED", STATUS="KEYBOARD_INTERRUPT")
            write_log(f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=ABORTED")
        print(RED + BOLD + "\nТест остановлен пользователем." + RESET)

    except Exception:
        error_text = traceback.format_exc()
        if log_file is not None:
            write_log("### MONITOR_ERROR")
            for error_line in error_text.rstrip().splitlines():
                write_log(error_line)
            write_log(f"### MONITOR_FOOTER WALL_TIME={wall_time()} STATUS=ERROR")
        print(RED + BOLD + "\nОШИБКА:" + RESET)
        print(error_text)

    finally:
        stop_reader.set()

        if reader_thread is not None:
            reader_thread.join(timeout=1.0)

        if ser is not None and ser.is_open:
            ser.close()

        if log_file is not None:
            log_file.close()


if __name__ == "__main__":
    run_monitor()
