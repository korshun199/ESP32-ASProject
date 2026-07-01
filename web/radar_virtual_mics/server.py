#!/usr/bin/env python3
import json
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent

STATE = {
    "source": {"x": 0.5, "y": 0.5, "auto": False, "speed": 25},
    "audio": {"volume": 0.0, "freq": 0},
    "mics": [
        {"id": 1, "name": "MIC1 LT", "x": 0.12, "y": 0.12, "gain": 1.0, "level": 0.0},
        {"id": 2, "name": "MIC2 RT", "x": 0.88, "y": 0.12, "gain": 1.0, "level": 0.0},
        {"id": 3, "name": "MIC3 LB", "x": 0.12, "y": 0.88, "gain": 1.0, "level": 0.0},
        {"id": 4, "name": "MIC4 RB", "x": 0.88, "y": 0.88, "gain": 1.0, "level": 0.0}
    ],
    "estimate": {"x": 0.5, "y": 0.5},
    "updated_ms": 0
}

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def send_json(self, obj, code=200):
        data = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/state":
            self.send_json(STATE)
            return
        if path == "/":
            self.path = "/control.html"
        return super().do_GET()

    def do_POST(self):
        path = urlparse(self.path).path
        if path != "/api/state":
            self.send_json({"error": "not found"}, 404)
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)

        try:
            incoming = json.loads(raw.decode("utf-8"))
        except Exception as e:
            self.send_json({"error": str(e)}, 400)
            return

        for key in ("source", "audio", "mics", "estimate", "updated_ms"):
            if key in incoming:
                STATE[key] = incoming[key]

        self.send_json({"ok": True, "state": STATE})

if __name__ == "__main__":
    host = "0.0.0.0"
    port = 8088
    print(f"Radar virtual mics server: http://{host}:{port}/")
    print("Control: http://YOUR_T16_IP:8088/control.html")
    print("Monitor: http://YOUR_T16_IP:8088/monitor.html")
    ThreadingHTTPServer((host, port), Handler).serve_forever()
