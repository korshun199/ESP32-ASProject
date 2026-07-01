#!/usr/bin/env python3
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import json
import time

HOST = "0.0.0.0"
PORT = 8088

BASE_DIR = Path(__file__).resolve().parent

RADAR_STATE = {
    "object": None,
    "display_object": None,
    "target": {"locked": False, "x": 0.5, "y": 0.5},
    "estimate": None,
    "source": None,
    "audio": {"volume": 0.0, "freq": 0},
    "mics": [],
    "updated_ms": 0,
}

ESP32_BUFFER = {
    "seq": 0,
    "source": "none",
    "updated_ms": 0,
    "age_ms": 0,
    "mics": [
        {"id": 1, "name": "MIC1 TOP", "volume": 0.0, "raw": 0},
        {"id": 2, "name": "MIC2 RIGHT", "volume": 0.0, "raw": 0},
        {"id": 3, "name": "MIC3 BOTTOM", "volume": 0.0, "raw": 0},
        {"id": 4, "name": "MIC4 LEFT", "volume": 0.0, "raw": 0},
    ],
    "audio": {"volume": 0.0, "freq": 0},
    "object": None,
}


def now_ms() -> int:
    return int(time.time() * 1000)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def read_json(handler):
    length = int(handler.headers.get("Content-Length", "0") or "0")
    if length <= 0:
        return {}
    raw = handler.rfile.read(length)
    return json.loads(raw.decode("utf-8"))


def write_json(handler, data, status=200):
    payload = json.dumps(data, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Cache-Control", "no-store")
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
    handler.send_header("Access-Control-Allow-Headers", "Content-Type")
    handler.send_header("Content-Length", str(len(payload)))
    handler.end_headers()
    handler.wfile.write(payload)


def normalize_esp32_payload(data):
    global ESP32_BUFFER

    seq = int(data.get("seq", ESP32_BUFFER.get("seq", 0) + 1))
    source = str(data.get("source", "esp32"))

    incoming_mics = data.get("mics", [])
    normalized_mics = []

    for idx in range(1, 5):
        found = None
        for m in incoming_mics:
            if int(m.get("id", -1)) == idx:
                found = m
                break

        if found is None:
            found = {}

        raw = int(found.get("raw", 0) or 0)

        if "volume" in found:
            volume = float(found.get("volume", 0.0) or 0.0)
        else:
            # 12-bit ADC ESP32: 0..4095
            volume = raw / 4095.0 if raw > 0 else 0.0

        volume = clamp(volume, 0.0, 1.0)

        default_names = {
            1: "MIC1 TOP",
            2: "MIC2 RIGHT",
            3: "MIC3 BOTTOM",
            4: "MIC4 LEFT",
        }

        normalized_mics.append({
            "id": idx,
            "name": str(found.get("name", default_names[idx])),
            "volume": volume,
            "raw": raw,
        })

    ESP32_BUFFER = {
        "seq": seq,
        "source": source,
        "updated_ms": now_ms(),
        "age_ms": 0,
        "mics": normalized_mics,
        "audio": data.get("audio", {"volume": 0.0, "freq": 0}),
        "object": data.get("object", None),
    }

    return ESP32_BUFFER


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BASE_DIR), **kwargs)

    def log_message(self, fmt, *args):
        print("[%s] %s" % (self.log_date_time_string(), fmt % args))

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    def do_GET(self):
        global RADAR_STATE, ESP32_BUFFER

        if self.path.startswith("/api/state"):
            write_json(self, RADAR_STATE)
            return

        if self.path.startswith("/api/esp32/buffer"):
            out = dict(ESP32_BUFFER)
            if out.get("updated_ms"):
                out["age_ms"] = now_ms() - int(out["updated_ms"])
            else:
                out["age_ms"] = 0
            write_json(self, {"ok": True, "buffer": out})
            return

        return super().do_GET()

    def do_POST(self):
        global RADAR_STATE

        try:
            data = read_json(self)

            if self.path.startswith("/api/state"):
                RADAR_STATE = data
                RADAR_STATE["updated_ms"] = now_ms()
                write_json(self, {"ok": True, "updated_ms": RADAR_STATE["updated_ms"]})
                return

            if self.path.startswith("/api/esp32/push"):
                buf = normalize_esp32_payload(data)
                write_json(self, {"ok": True, "buffer": buf})
                return

            write_json(self, {"ok": False, "error": "unknown endpoint"}, status=404)

        except Exception as e:
            write_json(self, {"ok": False, "error": str(e)}, status=500)


def main():
    print()
    print("=== Virtual Acoustic Radar Server ===")
    print(f"Directory: {BASE_DIR}")
    print(f"Listen:    http://{HOST}:{PORT}")
    print()
    print("Radar:")
    print("  GET  /api/state")
    print("  POST /api/state")
    print()
    print("ESP32 buffer:")
    print("  GET  /api/esp32/buffer")
    print("  POST /api/esp32/push")
    print()

    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.serve_forever()


if __name__ == "__main__":
    main()
