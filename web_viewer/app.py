import json
import threading
import time
from pathlib import Path
from typing import Any

import serial
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles

BASE_DIR = Path(__file__).resolve().parent
STATIC_DIR = BASE_DIR / "static"

SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_BAUD = 115200

latest_data: dict[str, Any] = {
    "freq": [0, 0, 0],
    "vol": [0, 0, 0],
    "x": 0,
    "y": 0,
    "status": "waiting",
    "updated_at": time.time(),
}

latest_lock = threading.Lock()

app = FastAPI(title="ESP32-ASProject Web Viewer")
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")


@app.get("/")
def index() -> FileResponse:
    return FileResponse(STATIC_DIR / "index.html")


@app.get("/api/latest")
def api_latest() -> JSONResponse:
    with latest_lock:
        return JSONResponse(latest_data)


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket) -> None:
    await websocket.accept()

    try:
        while True:
            with latest_lock:
                payload = dict(latest_data)

            await websocket.send_text(json.dumps(payload, ensure_ascii=False))
            time.sleep(0.05)

    except WebSocketDisconnect:
        return


def update_latest(data: dict[str, Any]) -> None:
    with latest_lock:
        latest_data.update(data)
        latest_data["status"] = "online"
        latest_data["updated_at"] = time.time()


def set_status(status: str) -> None:
    with latest_lock:
        latest_data["status"] = status
        latest_data["updated_at"] = time.time()


def parse_line(line: str) -> dict[str, Any] | None:
    line = line.strip()

    if not line:
        return None

    if line.startswith("DATA:"):
        line = line[5:].strip()
    elif line.startswith("{"):
        pass
    else:
        return None

    try:
        data = json.loads(line)
    except json.JSONDecodeError:
        return None

    if not isinstance(data, dict):
        return None

    freq = data.get("freq")
    vol = data.get("vol")
    x = data.get("x")
    y = data.get("y")

    if not isinstance(freq, list) or len(freq) != 3:
        return None

    if not isinstance(vol, list) or len(vol) != 3:
        return None

    if not isinstance(x, int) or not isinstance(y, int):
        return None

    return {
        "freq": [int(freq[0]), int(freq[1]), int(freq[2])],
        "vol": [int(vol[0]), int(vol[1]), int(vol[2])],
        "x": int(x),
        "y": int(y),
    }


def serial_reader_loop() -> None:
    while True:
        try:
            set_status(f"connecting {SERIAL_PORT}")

            with serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1) as ser:
                set_status("online")

                while True:
                    raw = ser.readline()

                    if not raw:
                        continue

                    line = raw.decode("utf-8", errors="ignore").strip()
                    data = parse_line(line)

                    if data is not None:
                        update_latest(data)

        except Exception as exc:
            set_status(f"serial error: {exc}")
            time.sleep(1)


@app.on_event("startup")
def startup() -> None:
    thread = threading.Thread(target=serial_reader_loop, daemon=True)
    thread.start()
