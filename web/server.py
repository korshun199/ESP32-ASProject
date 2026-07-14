#!/usr/bin/env python3

"""
Веб-сервер живого звукового радара.

Сервер:
  - читает ESP32 через последовательный порт;
  - собирает FRAME, WAVE и SPECTRUM в один пакет;
  - отдаёт страницу и статические файлы;
  - передаёт данные браузеру через WebSocket.
"""

import argparse
import asyncio
import json
import logging
import threading
import time
from pathlib import Path
from typing import Any

import serial
from aiohttp import web


BASE_DIR = Path(__file__).resolve().parent

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)s | %(message)s",
)

LOGGER = logging.getLogger("audio-radar")


class RadarState:
    """Хранит последнее состояние радара."""

    def __init__(self) -> None:
        self.latest: dict[str, Any] | None = None
        self.serial_connected = False
        self.serial_error = ""
        self.received_frames = 0
        self.dropped_lines = 0
        self.started_at = time.time()
        self.lock = threading.Lock()

    def snapshot(self) -> dict[str, Any]:
        """Возвращает безопасную копию состояния."""

        with self.lock:
            return {
                "serial_connected": self.serial_connected,
                "serial_error": self.serial_error,
                "received_frames": self.received_frames,
                "dropped_lines": self.dropped_lines,
                "uptime_seconds": round(time.time() - self.started_at, 1),
                "latest": self.latest,
            }


class SerialReader:
    """Читает и разбирает текстовый поток ESP32."""

    def __init__(
        self,
        port: str,
        baudrate: int,
        state: RadarState,
        loop: asyncio.AbstractEventLoop,
        output_queue: asyncio.Queue,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.state = state
        self.loop = loop
        self.output_queue = output_queue

        self.stop_event = threading.Event()
        self.thread: threading.Thread | None = None

        self.pending: dict[int, dict[str, Any]] = {}

    def start(self) -> None:
        """Запускает поток чтения."""

        self.thread = threading.Thread(
            target=self._run,
            name="serial-reader",
            daemon=True,
        )
        self.thread.start()

    def stop(self) -> None:
        """Останавливает поток чтения."""

        self.stop_event.set()

        if self.thread is not None:
            self.thread.join(timeout=2)

    @staticmethod
    def _parse_key_values(text: str) -> dict[str, str]:
        """Разбирает пары КЛЮЧ=ЗНАЧЕНИЕ."""

        result: dict[str, str] = {}

        for item in text.split():
            if "=" not in item:
                continue

            key, value = item.split("=", 1)
            result[key] = value

        return result

    @staticmethod
    def _to_int(value: str | None, default: int = 0) -> int:
        """Преобразует строку в целое число."""

        try:
            return int(value or default)
        except ValueError:
            return default

    @staticmethod
    def _to_float(value: str | None, default: float = 0.0) -> float:
        """Преобразует строку в число с точкой."""

        try:
            return float(value or default)
        except ValueError:
            return default

    def _parse_frame(self, line: str) -> tuple[int, dict[str, Any]] | None:
        """Разбирает строку FRAME."""

        values = self._parse_key_values(line)

        if "T_MS" not in values:
            return None

        timestamp = self._to_int(values.get("T_MS"))

        frame = {
            "type": "audio_frame",
            "t_ms": timestamp,
            "id": self._to_int(values.get("ID")),
            "count": self._to_int(values.get("COUNT")),
            "mean": self._to_int(values.get("MEAN")),
            "min": self._to_int(values.get("MIN")),
            "max": self._to_int(values.get("MAX")),
            "rms": self._to_int(values.get("RMS")),
            "peak": self._to_int(values.get("PEAK")),
            "p2p": self._to_int(values.get("P2P")),
            "level": self._to_int(values.get("LEVEL")),
            "clip": self._to_int(values.get("CLIP")),
            "dominant_hz": self._to_float(values.get("DOM_HZ")),
            "dominant_amplitude": self._to_float(values.get("DOM_AMP")),
            "spectrum_maximum": self._to_float(values.get("SPEC_MAX")),
            "read_errors": self._to_int(values.get("READ_ERR")),
            "fps": self._to_float(values.get("FPS")),
            "wave": [],
            "spectrum": [],
            "spectrum_first_hz": 0.0,
            "spectrum_step_hz": 0.0,
        }

        return timestamp, frame

    def _parse_wave(self, line: str) -> tuple[int, list[int]] | None:
        """Разбирает строку WAVE."""

        if " DATA=" not in line:
            return None

        header, raw_data = line.split(" DATA=", 1)
        values = self._parse_key_values(header)

        if "T_MS" not in values:
            return None

        timestamp = self._to_int(values.get("T_MS"))

        try:
            data = [
                int(value)
                for value in raw_data.split(",")
                if value
            ]
        except ValueError:
            return None

        return timestamp, data

    def _parse_spectrum(
        self,
        line: str,
    ) -> tuple[int, float, float, list[int]] | None:
        """Разбирает строку SPECTRUM."""

        if " DATA=" not in line:
            return None

        header, raw_data = line.split(" DATA=", 1)
        values = self._parse_key_values(header)

        if "T_MS" not in values:
            return None

        timestamp = self._to_int(values.get("T_MS"))
        first_hz = self._to_float(values.get("FIRST_HZ"))
        step_hz = self._to_float(values.get("STEP_HZ"))

        try:
            data = [
                int(value)
                for value in raw_data.split(",")
                if value
            ]
        except ValueError:
            return None

        return timestamp, first_hz, step_hz, data

    def _publish_if_complete(self, timestamp: int) -> None:
        """Отправляет кадр, когда собраны все три части."""

        packet = self.pending.get(timestamp)

        if not packet:
            return

        if not all(
            key in packet
            for key in ("frame", "wave", "spectrum")
        ):
            return

        frame = packet["frame"]
        frame["wave"] = packet["wave"]
        frame["spectrum"] = packet["spectrum"]
        frame["spectrum_first_hz"] = packet["first_hz"]
        frame["spectrum_step_hz"] = packet["step_hz"]
        frame["server_time"] = time.time()

        with self.state.lock:
            self.state.latest = frame
            self.state.received_frames += 1

        asyncio.run_coroutine_threadsafe(
            self.output_queue.put(frame),
            self.loop,
        )

        del self.pending[timestamp]

        if len(self.pending) > 20:
            old_timestamps = sorted(self.pending)[:-10]

            for old_timestamp in old_timestamps:
                del self.pending[old_timestamp]

    def _process_line(self, line: str) -> None:
        """Обрабатывает одну строку ESP32."""

        if line.startswith("FRAME "):
            parsed = self._parse_frame(line)

            if parsed is None:
                self.state.dropped_lines += 1
                return

            timestamp, frame = parsed
            self.pending.setdefault(timestamp, {})["frame"] = frame
            self._publish_if_complete(timestamp)
            return

        if line.startswith("WAVE "):
            parsed = self._parse_wave(line)

            if parsed is None:
                self.state.dropped_lines += 1
                return

            timestamp, wave = parsed
            self.pending.setdefault(timestamp, {})["wave"] = wave
            self._publish_if_complete(timestamp)
            return

        if line.startswith("SPECTRUM "):
            parsed = self._parse_spectrum(line)

            if parsed is None:
                self.state.dropped_lines += 1
                return

            timestamp, first_hz, step_hz, spectrum = parsed
            packet = self.pending.setdefault(timestamp, {})
            packet["spectrum"] = spectrum
            packet["first_hz"] = first_hz
            packet["step_hz"] = step_hz
            self._publish_if_complete(timestamp)

    def _run(self) -> None:
        """Основной цикл последовательного порта."""

        while not self.stop_event.is_set():
            try:
                LOGGER.info(
                    "Подключение к %s, скорость %d",
                    self.port,
                    self.baudrate,
                )

                with serial.Serial(
                    port=self.port,
                    baudrate=self.baudrate,
                    timeout=0.5,
                    write_timeout=0.5,
                    rtscts=False,
                    dsrdtr=False,
                ) as device:
                    device.dtr = False
                    device.rts = False
                    device.reset_input_buffer()

                    with self.state.lock:
                        self.state.serial_connected = True
                        self.state.serial_error = ""

                    LOGGER.info("Последовательный порт подключён")

                    while not self.stop_event.is_set():
                        raw_line = device.readline()

                        if not raw_line:
                            continue

                        line = raw_line.decode(
                            "utf-8",
                            errors="ignore",
                        ).strip()

                        if not line:
                            continue

                        self._process_line(line)

            except serial.SerialException as error:
                with self.state.lock:
                    self.state.serial_connected = False
                    self.state.serial_error = str(error)

                LOGGER.error(
                    "Ошибка последовательного порта: %s",
                    error,
                )

                time.sleep(2)

            except Exception:
                LOGGER.exception("Ошибка чтения данных")

                with self.state.lock:
                    self.state.serial_connected = False
                    self.state.serial_error = "Внутренняя ошибка чтения"

                time.sleep(2)


async def index_handler(request: web.Request) -> web.FileResponse:
    """Отдаёт главную страницу."""

    return web.FileResponse(BASE_DIR / "index.html")


async def health_handler(request: web.Request) -> web.Response:
    """Возвращает состояние сервера."""

    state: RadarState = request.app["radar_state"]

    return web.json_response(
        {
            "status": "ok",
            **state.snapshot(),
        }
    )


async def latest_handler(request: web.Request) -> web.Response:
    """Возвращает последний звуковой кадр."""

    state: RadarState = request.app["radar_state"]

    return web.json_response(
        state.snapshot()
    )


async def websocket_handler(request: web.Request) -> web.WebSocketResponse:
    """Подключает браузер к живому потоку."""

    websocket = web.WebSocketResponse(
        heartbeat=15,
    )

    await websocket.prepare(request)

    clients: set[web.WebSocketResponse] = request.app["clients"]
    state: RadarState = request.app["radar_state"]

    clients.add(websocket)

    LOGGER.info(
        "WebSocket подключён, клиентов: %d",
        len(clients),
    )

    snapshot = state.snapshot()

    await websocket.send_json(
        {
            "type": "server_state",
            **snapshot,
        }
    )

    try:
        async for message in websocket:
            if message.type == web.WSMsgType.TEXT:
                if message.data == "ping":
                    await websocket.send_str("pong")

            if message.type == web.WSMsgType.ERROR:
                LOGGER.warning(
                    "Ошибка WebSocket: %s",
                    websocket.exception(),
                )

    finally:
        clients.discard(websocket)

        LOGGER.info(
            "WebSocket отключён, клиентов: %d",
            len(clients),
        )

    return websocket


async def broadcaster(app: web.Application) -> None:
    """Передаёт новые кадры всем браузерам."""

    queue: asyncio.Queue = app["output_queue"]
    clients: set[web.WebSocketResponse] = app["clients"]

    while True:
        frame = await queue.get()

        if not clients:
            continue

        disconnected: list[web.WebSocketResponse] = []

        for websocket in clients:
            try:
                await websocket.send_json(frame)
            except Exception:
                disconnected.append(websocket)

        for websocket in disconnected:
            clients.discard(websocket)


async def start_background_tasks(app: web.Application) -> None:
    """Запускает чтение Serial и рассылку."""

    loop = asyncio.get_running_loop()

    reader = SerialReader(
        port=app["serial_port"],
        baudrate=app["baudrate"],
        state=app["radar_state"],
        loop=loop,
        output_queue=app["output_queue"],
    )

    app["serial_reader"] = reader
    reader.start()

    app["broadcast_task"] = asyncio.create_task(
        broadcaster(app)
    )


async def stop_background_tasks(app: web.Application) -> None:
    """Останавливает фоновые задачи."""

    reader: SerialReader = app["serial_reader"]
    reader.stop()

    task: asyncio.Task = app["broadcast_task"]
    task.cancel()

    try:
        await task
    except asyncio.CancelledError:
        pass


def create_application(
    serial_port: str,
    baudrate: int,
) -> web.Application:
    """Создаёт веб-приложение."""

    application = web.Application()

    application["serial_port"] = serial_port
    application["baudrate"] = baudrate
    application["radar_state"] = RadarState()
    application["output_queue"] = asyncio.Queue(maxsize=20)
    application["clients"] = set()

    application.router.add_get("/", index_handler)
    application.router.add_get("/health", health_handler)
    application.router.add_get("/api/latest", latest_handler)
    application.router.add_get("/ws", websocket_handler)

    application.router.add_static(
        "/static",
        BASE_DIR,
        show_index=False,
    )

    application.on_startup.append(start_background_tasks)
    application.on_cleanup.append(stop_background_tasks)

    return application


def main() -> None:
    """Запускает сервер."""

    parser = argparse.ArgumentParser(
        description="Живой веб-монитор ESP32 Radar",
    )

    parser.add_argument(
        "--port",
        default="/dev/ttyACM0",
        help="Последовательный порт ESP32",
    )

    parser.add_argument(
        "--baudrate",
        type=int,
        default=115200,
        help="Скорость последовательного порта",
    )

    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="Адрес веб-сервера",
    )

    parser.add_argument(
        "--http-port",
        type=int,
        default=8080,
        help="Порт веб-сервера",
    )

    arguments = parser.parse_args()

    LOGGER.info(
        "Веб-панель: http://%s:%d",
        arguments.host,
        arguments.http_port,
    )

    application = create_application(
        serial_port=arguments.port,
        baudrate=arguments.baudrate,
    )

    web.run_app(
        application,
        host=arguments.host,
        port=arguments.http_port,
        print=None,
    )


if __name__ == "__main__":
    main()
