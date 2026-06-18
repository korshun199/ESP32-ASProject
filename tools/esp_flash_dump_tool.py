#!/usr/bin/env python3
import datetime
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

PROJECT_DIR = Path("/home/work/ESP32-ASProject")
DUMPS_DIR = PROJECT_DIR / "dumps"

DEFAULT_REGIONS = [
    ("bootloader", 0x00001000, 0x00007000),
    ("partition_table", 0x00008000, 0x00001000),
    ("nvs", 0x00009000, 0x00005000),
    ("boot_app0", 0x0000E000, 0x00002000),
    ("app0_ota0", 0x00010000, 0x00140000),
    ("app1_ota1", 0x00150000, 0x00140000),
    ("spiffs_or_littlefs", 0x00290000, 0x00170000),
]

FLASH_SIZES = {
    "1MB": 1 * 1024 * 1024,
    "2MB": 2 * 1024 * 1024,
    "4MB": 4 * 1024 * 1024,
    "8MB": 8 * 1024 * 1024,
    "16MB": 16 * 1024 * 1024,
    "32MB": 32 * 1024 * 1024,
}

def now_stamp():
    return datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

def clean_name(text):
    text = text.strip()
    text = re.sub(r"[^A-Za-z0-9_.-]+", "-", text)
    text = text.strip("-")
    return text or "unknown"

def find_esptool():
    for name in ("esptool.py", "esptool"):
        path = shutil.which(name)
        if path:
            return path
    print("ERROR: esptool not found")
    print("Install/check Arduino ESP32 core or esptool.")
    sys.exit(1)

def list_ports():
    ports = []
    ports.extend(glob.glob("/dev/ttyUSB*"))
    ports.extend(glob.glob("/dev/ttyACM*"))
    ports = sorted(set(ports))

    if not ports:
        try:
            import serial.tools.list_ports
            ports = [p.device for p in serial.tools.list_ports.comports()]
        except Exception:
            pass

    return ports

def choose_port():
    ports = list_ports()
    print()
    print("===== SERIAL PORTS =====")

    if not ports:
        print("No serial ports found.")
        print("Plug board via USB-UART and try again.")
        return None

    for i, port in enumerate(ports, 1):
        print(f"{i}) {port}")

    print()
    choice = input(f"Select port [1]: ").strip() or "1"

    try:
        index = int(choice) - 1
        return ports[index]
    except Exception:
        print("Bad port selection.")
        return None

def run_cmd(cmd, log_file=None, check=False):
    text_cmd = " ".join(str(x) for x in cmd)
    if log_file:
        with log_file.open("a", encoding="utf-8") as f:
            f.write("\n===== CMD =====\n")
            f.write(text_cmd + "\n")

    print()
    print("CMD:", text_cmd)

    proc = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )

    if log_file:
        with log_file.open("a", encoding="utf-8") as f:
            f.write(proc.stdout)
            f.write(f"\nEXIT={proc.returncode}\n")

    print(proc.stdout)

    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed: {text_cmd}")

    return proc.returncode, proc.stdout

def parse_chip_info(text):
    chip = "unknown-chip"
    mac = "unknown-mac"
    flash_size = None

    m = re.search(r"Chip type:\s*(.+)", text)
    if m:
        chip = m.group(1).strip()

    m = re.search(r"MAC:\s*([0-9a-fA-F:]+)", text)
    if m:
        mac = m.group(1).strip().replace(":", "-").lower()

    m = re.search(r"Detected flash size:\s*([0-9]+MB)", text)
    if m:
        flash_size = FLASH_SIZES.get(m.group(1).upper())

    return chip, mac, flash_size

def detect_chip(esptool, port, work_dir):
    log_file = work_dir / "command_log.txt"

    outputs = []

    for cmd_name in ("chip_id", "chip-id"):
        code, out = run_cmd([esptool, "--port", port, cmd_name], log_file=log_file)
        outputs.append(out)
        if code == 0:
            break

    for cmd_name in ("flash_id", "flash-id"):
        code, out = run_cmd([esptool, "--port", port, cmd_name], log_file=log_file)
        outputs.append(out)
        if code == 0:
            break

    all_text = "\n".join(outputs)
    chip, mac, flash_size = parse_chip_info(all_text)

    if not flash_size:
        flash_size = ask_flash_size()

    return chip, mac, flash_size, all_text

def ask_flash_size():
    print()
    print("Cannot auto-detect flash size.")
    print("Choose manually:")
    sizes = [("1MB", 1), ("2MB", 2), ("4MB", 4), ("8MB", 8), ("16MB", 16), ("32MB", 32)]
    for i, (label, _) in enumerate(sizes, 1):
        print(f"{i}) {label}")

    choice = input("Flash size [3 = 4MB]: ").strip() or "3"

    try:
        label = sizes[int(choice) - 1][0]
        return FLASH_SIZES[label]
    except Exception:
        print("Bad choice, using 4MB.")
        return 4 * 1024 * 1024

def make_session_dir(chip, mac):
    stamp = now_stamp()
    name = f"{clean_name(chip)}_{clean_name(mac)}_{stamp}"
    path = DUMPS_DIR / name
    path.mkdir(parents=True, exist_ok=True)
    return path

def read_flash(esptool, port, address, size, out_file, log_file):
    out_file.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        esptool,
        "--port", port,
        "read_flash",
        hex(address),
        hex(size),
        str(out_file),
    ]
    run_cmd(cmd, log_file=log_file, check=True)

def bin_to_hex_dump(bin_file, hex_file):
    data = bin_file.read_bytes()
    with hex_file.open("w", encoding="utf-8") as f:
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            hex_part = " ".join(f"{b:02X}" for b in chunk)
            ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
            f.write(f"{offset:08X}: {hex_part:<47}  {ascii_part}\n")

def intel_hex_record(address, record_type, data):
    length = len(data)
    hi = (address >> 8) & 0xFF
    lo = address & 0xFF
    body = bytes([length, hi, lo, record_type]) + data
    checksum = ((~sum(body) + 1) & 0xFF)
    return ":" + body.hex().upper() + f"{checksum:02X}"

def bin_to_intel_hex(bin_file, ihex_file, base_address=0):
    data = bin_file.read_bytes()
    with ihex_file.open("w", encoding="utf-8") as f:
        current_upper = None
        for offset in range(0, len(data), 16):
            absolute = base_address + offset
            upper = absolute >> 16
            low = absolute & 0xFFFF

            if current_upper != upper:
                current_upper = upper
                payload = struct.pack(">H", upper)
                f.write(intel_hex_record(0, 4, payload) + "\n")

            chunk = data[offset:offset + 16]
            f.write(intel_hex_record(low, 0, chunk) + "\n")

        f.write(":00000001FF\n")

def parse_partition_table(bin_file):
    data = bin_file.read_bytes()
    entries = []

    for offset in range(0, min(len(data), 0xC00), 32):
        item = data[offset:offset + 32]
        if len(item) < 32:
            break

        magic = item[0:2]

        if magic == b"\xFF\xFF":
            break

        if magic != b"\xAA\x50":
            continue

        part_type = item[2]
        subtype = item[3]
        part_offset = struct.unpack("<L", item[4:8])[0]
        part_size = struct.unpack("<L", item[8:12])[0]
        label = item[12:28].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        flags = struct.unpack("<L", item[28:32])[0]

        entries.append({
            "label": label or f"partition_{len(entries)}",
            "type": part_type,
            "subtype": subtype,
            "offset": part_offset,
            "size": part_size,
            "flags": flags,
        })

    return entries

def save_partition_csv(entries, csv_file):
    with csv_file.open("w", encoding="utf-8") as f:
        f.write("label,type,subtype,offset_hex,offset_dec,size_hex,size_dec,flags\n")
        for e in entries:
            f.write(
                f"{e['label']},"
                f"{e['type']},"
                f"{e['subtype']},"
                f"0x{e['offset']:X},"
                f"{e['offset']},"
                f"0x{e['size']:X},"
                f"{e['size']},"
                f"0x{e['flags']:X}\n"
            )

def save_info(work_dir, port, chip, mac, flash_size, detect_text):
    info = work_dir / "info.txt"
    with info.open("w", encoding="utf-8") as f:
        f.write("ESP FLASH DUMP SESSION\n")
        f.write(f"Date: {datetime.datetime.now().isoformat()}\n")
        f.write(f"Port: {port}\n")
        f.write(f"Chip: {chip}\n")
        f.write(f"MAC: {mac}\n")
        f.write(f"Flash size bytes: {flash_size}\n")
        f.write(f"Flash size hex: 0x{flash_size:X}\n")
        f.write("\nDETECT RAW OUTPUT\n")
        f.write(detect_text)
        f.write("\n")

def convert_bin_pair(bin_file, base_address=0):
    hex_file = bin_file.with_suffix(".hex")
    ihex_file = bin_file.with_suffix(".ihex")
    bin_to_hex_dump(bin_file, hex_file)
    bin_to_intel_hex(bin_file, ihex_file, base_address=base_address)

def dump_full_flash(esptool, port, flash_size, work_dir):
    log_file = work_dir / "command_log.txt"
    out = work_dir / "full_flash.bin"
    read_flash(esptool, port, 0, flash_size, out, log_file)
    convert_bin_pair(out, 0)

def dump_default_regions(esptool, port, flash_size, work_dir):
    log_file = work_dir / "command_log.txt"
    regions_dir = work_dir / "regions"

    for name, addr, size in DEFAULT_REGIONS:
        if addr >= flash_size:
            continue

        if addr + size > flash_size:
            size = flash_size - addr

        out = regions_dir / f"{name}_0x{addr:06X}_size_0x{size:X}.bin"
        read_flash(esptool, port, addr, size, out, log_file)
        convert_bin_pair(out, addr)

def dump_partition_regions(esptool, port, work_dir):
    log_file = work_dir / "command_log.txt"
    part_bin = work_dir / "partition_table.bin"

    if not part_bin.exists():
        read_flash(esptool, port, 0x8000, 0x1000, part_bin, log_file)
        convert_bin_pair(part_bin, 0x8000)

    entries = parse_partition_table(part_bin)
    save_partition_csv(entries, work_dir / "partition_table.csv")

    regions_dir = work_dir / "partitions"

    if not entries:
        print("No valid partition entries found.")
        return

    for e in entries:
        safe_label = clean_name(e["label"])
        out = regions_dir / f"{safe_label}_0x{e['offset']:06X}_size_0x{e['size']:X}.bin"
        read_flash(esptool, port, e["offset"], e["size"], out, log_file)
        convert_bin_pair(out, e["offset"])

def dump_custom_region(esptool, port, work_dir):
    log_file = work_dir / "command_log.txt"

    print()
    addr_text = input("Start address hex/dec, example 0x9000: ").strip()
    size_text = input("Size hex/dec, example 0x5000: ").strip()
    name = input("Name [custom]: ").strip() or "custom"

    try:
        address = int(addr_text, 0)
        size = int(size_text, 0)
    except Exception:
        print("Bad address or size.")
        return

    out = work_dir / "custom" / f"{clean_name(name)}_0x{address:06X}_size_0x{size:X}.bin"
    read_flash(esptool, port, address, size, out, log_file)
    convert_bin_pair(out, address)

def create_session():
    esptool = find_esptool()
    port = choose_port()

    if not port:
        return None

    temp_dir = DUMPS_DIR / f"detect_{now_stamp()}"
    temp_dir.mkdir(parents=True, exist_ok=True)

    chip, mac, flash_size, detect_text = detect_chip(esptool, port, temp_dir)
    work_dir = make_session_dir(chip, mac)

    for item in temp_dir.iterdir():
        shutil.move(str(item), str(work_dir / item.name))
    temp_dir.rmdir()

    save_info(work_dir, port, chip, mac, flash_size, detect_text)

    print()
    print("===== SESSION =====")
    print(f"Directory: {work_dir}")
    print(f"Chip     : {chip}")
    print(f"MAC      : {mac}")
    print(f"Flash    : {flash_size} bytes / 0x{flash_size:X}")

    return esptool, port, flash_size, work_dir

def menu():
    session = None

    while True:
        print()
        print("===== ESP FLASH DUMP TOOL =====")
        print("1) New session / detect chip")
        print("2) Dump full flash")
        print("3) Dump standard ESP32 regions")
        print("4) Dump partition table and all partitions")
        print("5) Dump custom region")
        print("6) Dump everything")
        print("7) Show current session")
        print("0) Exit")
        print()

        choice = input("Select: ").strip()

        try:
            if choice == "1":
                session = create_session()

            elif choice in ("2", "3", "4", "5", "6"):
                if not session:
                    session = create_session()

                if not session:
                    continue

                esptool, port, flash_size, work_dir = session

                if choice == "2":
                    dump_full_flash(esptool, port, flash_size, work_dir)
                elif choice == "3":
                    dump_default_regions(esptool, port, flash_size, work_dir)
                elif choice == "4":
                    dump_partition_regions(esptool, port, work_dir)
                elif choice == "5":
                    dump_custom_region(esptool, port, work_dir)
                elif choice == "6":
                    dump_full_flash(esptool, port, flash_size, work_dir)
                    dump_default_regions(esptool, port, flash_size, work_dir)
                    dump_partition_regions(esptool, port, work_dir)

                print()
                print("DONE")
                print(f"Saved to: {work_dir}")

            elif choice == "7":
                if not session:
                    print("No current session.")
                else:
                    _, port, flash_size, work_dir = session
                    print(f"Port: {port}")
                    print(f"Flash size: {flash_size}")
                    print(f"Directory: {work_dir}")

            elif choice == "0":
                return

            else:
                print("Bad choice.")

        except KeyboardInterrupt:
            print()
            print("Interrupted.")
        except Exception as e:
            print()
            print("ERROR:", e)

if __name__ == "__main__":
    menu()
