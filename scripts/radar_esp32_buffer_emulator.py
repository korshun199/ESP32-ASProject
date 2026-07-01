#!/usr/bin/env python3
import json
import math
import time
import urllib.request
import urllib.error
import argparse
import random

MICS = [
    {"id": 1, "name": "MIC1 TOP",    "x": 0.50, "y": 0.08},
    {"id": 2, "name": "MIC2 RIGHT",  "x": 0.92, "y": 0.50},
    {"id": 3, "name": "MIC3 BOTTOM", "x": 0.50, "y": 0.92},
    {"id": 4, "name": "MIC4 LEFT",   "x": 0.08, "y": 0.50},
]


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def object_position(t):
    # Медленное широкое движение по экрану.
    x = 0.50 + 0.38 * math.sin(t * 0.43)
    y = 0.50 + 0.34 * math.cos(t * 0.31 + 1.2)

    # Маленькое живое смещение, чтобы объект не ходил как циркуль из канцелярии.
    x += 0.05 * math.sin(t * 1.11)
    y += 0.04 * math.cos(t * 0.97)

    return clamp(x, 0.06, 0.94), clamp(y, 0.06, 0.94)


def mic_level_from_object(mic, ox, oy):
    dx = ox - mic["x"]
    dy = oy - mic["y"]
    d2 = dx * dx + dy * dy

    # Чем ближе объект к направлению/позиции микрофона, тем выше уровень.
    level = 1.0 / (0.10 + d2 * 3.2)
    level = clamp(level, 0.0, 1.0)

    # Небольшая дрожь измерений, как у реального АЦП.
    level += random.uniform(-0.015, 0.015)

    return clamp(level, 0.0, 1.0)


def post_json(url, data):
    payload = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    with urllib.request.urlopen(req, timeout=2.0) as resp:
        return resp.read().decode("utf-8", errors="replace")


def main():
    ap = argparse.ArgumentParser(description="ESP32 radar controller emulator")
    ap.add_argument("--server", default="http://127.0.0.1:8088", help="Radar server base URL")
    ap.add_argument("--hz", type=float, default=20.0, help="Send rate")
    args = ap.parse_args()

    url = args.server.rstrip("/") + "/api/esp32/push"
    delay = 1.0 / max(args.hz, 1.0)
    seq = 0

    print()
    print("=== ESP32 Radar Buffer Emulator ===")
    print("POST:", url)
    print("Hz:  ", args.hz)
    print("Stop: Ctrl+C")
    print()

    while True:
        t = time.time()
        ox, oy = object_position(t)

        mics = []
        for mic in MICS:
            volume = mic_level_from_object(mic, ox, oy)
            raw = int(volume * 4095)

            mics.append({
                "id": mic["id"],
                "name": mic["name"],
                "volume": round(volume, 4),
                "raw": raw,
            })

        seq += 1

        msg = {
            "seq": seq,
            "source": "esp32_emulator",
            "object": {
                "visible": True,
                "x": round(ox, 4),
                "y": round(oy, 4),
            },
            "audio": {
                "volume": round(sum(m["volume"] for m in mics) / len(mics), 4),
                "freq": int(90 + 25 * math.sin(t * 0.8)),
            },
            "mics": mics,
        }

        try:
            post_json(url, msg)
            line = "seq=%06d obj=(%.3f %.3f) " % (seq, ox, oy)
            line += " ".join("M%d=%03d" % (m["id"], int(m["volume"] * 100)) for m in mics)
            print(line, flush=True)
        except urllib.error.URLError as e:
            print("send error:", e, flush=True)

        time.sleep(delay)


if __name__ == "__main__":
    main()
