#!/usr/bin/env python3
"""Stáhne RGB565 framebuffer z Waveshare displeje a uloží kruhové PNG."""

from __future__ import annotations

import argparse
import glob
import os
from pathlib import Path
import struct
import sys
import time
import zlib


PROJECT_ROOT = Path(__file__).resolve().parent.parent
PROJECT_PACKAGES = PROJECT_ROOT / ".arduino" / "python"
sys.path.insert(0, str(PROJECT_PACKAGES))

try:
    import serial
except ImportError as error:
    raise SystemExit("Chybí pyserial 3.5.") from error


WIDTH = 480
HEIGHT = 480
FRAMEBUFFER_BYTES = WIDTH * HEIGHT * 2
HEADER = b"WSFB1 480 480 RGB565LE 460800"


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum)
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def rgb565_to_circular_png(framebuffer: bytes, output_path: Path) -> None:
    rows = bytearray()
    center = (WIDTH - 1) / 2
    radius_squared = (WIDTH / 2) ** 2
    for y in range(HEIGHT):
        rows.append(0)
        dy_squared = (y - center) ** 2
        for x in range(WIDTH):
            offset = (y * WIDTH + x) * 2
            value = framebuffer[offset] | (framebuffer[offset + 1] << 8)
            red = ((value >> 11) & 0x1F) * 255 // 31
            green = ((value >> 5) & 0x3F) * 255 // 63
            blue = (value & 0x1F) * 255 // 31
            alpha = 255 if (x - center) ** 2 + dy_squared <= radius_squared else 0
            rows.extend((red, green, blue, alpha))
    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0)))
    png.extend(png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9)))
    png.extend(png_chunk(b"IEND", b""))
    output_path.write_bytes(png)


def find_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if len(ports) != 1:
        raise SystemExit("Nelze jednoznačně vybrat displej. Předejte port pomocí --port.")
    return ports[0]


def read_exact(connection: serial.Serial, byte_count: int) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + 30
    while len(data) < byte_count:
        if time.monotonic() > deadline:
            raise TimeoutError(f"Přijato pouze {len(data)} z {byte_count} bajtů")
        chunk = connection.read(min(16384, byte_count - len(data)))
        if chunk:
            data.extend(chunk)
    return bytes(data)


def request_framebuffer(connection: serial.Serial) -> bytes:
    connection.reset_input_buffer()
    connection.write(b"SCREENSHOT\n")
    connection.flush()
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if connection.readline().strip() == b"WSFB1_BEGIN":
            break
    else:
        raise TimeoutError("Displej nepotvrdil příkaz SCREENSHOT")
    metadata = connection.readline().strip()
    if metadata != HEADER:
        raise RuntimeError(f"Neočekávaná hlavička framebufferu: {metadata!r}")
    return read_exact(connection, FRAMEBUFFER_BYTES)


def open_settings(connection: serial.Serial, page: int = 1) -> None:
    connection.reset_input_buffer()
    command = "SETTINGS" if page == 1 else f"SETTINGS{page}"
    connection.write((command + "\n").encode("ascii"))
    connection.flush()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if connection.readline().strip() == b"SETTINGS_OPEN":
            return
    raise TimeoutError("Displej nepotvrdil otevření nastavení")


def enable_night_mode(connection: serial.Serial) -> None:
    connection.reset_input_buffer()
    connection.write(b"NIGHTTEST\n")
    connection.flush()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if connection.readline().strip() == b"NIGHT_TEST_ON":
            return
    raise TimeoutError("Displej nepotvrdil zapnutí nočního režimu")


def capture(port: str, settings_page: int = 0, night_mode: bool = False) -> bytes:
    connection = serial.Serial(baudrate=921600, timeout=1, dsrdtr=False, rtscts=False)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    try:
        # Otevření CH343 může desku resetovat. Počkáme na inicializaci panelu,
        # Wi-Fi, NTP a naplánovanou VSYNC resynchronizaci.
        time.sleep(20.0)
        if settings_page:
            open_settings(connection, settings_page)
            time.sleep(0.5)
        elif night_mode:
            enable_night_mode(connection)
            time.sleep(0.5)
        # První přenos sjednotí oba plné LVGL buffery po startu. Uložíme až
        # druhý, stabilní framebuffer; nevzniknou tak artefakty při sekundové
        # aktualizaci prstence.
        request_framebuffer(connection)
        time.sleep(0.5)
        return request_framebuffer(connection)
    finally:
        connection.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port")
    parser.add_argument(
        "--settings",
        action="store_true",
        help="Před screenshotem otevře první stránku nastavení",
    )
    parser.add_argument(
        "--settings-page",
        type=int,
        choices=(1, 2, 3, 4),
        help="Před screenshotem otevře vybranou stránku nastavení",
    )
    parser.add_argument(
        "--night",
        action="store_true",
        help="Před screenshotem zapne noční červený režim",
    )
    parser.add_argument("--output", type=Path, default=PROJECT_ROOT / "screenshots" / "latest.png")
    arguments = parser.parse_args()
    framebuffer = capture(
        find_port(arguments.port), arguments.settings_page or (1 if arguments.settings else 0), arguments.night
    )
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    rgb565_to_circular_png(framebuffer, arguments.output)
    print(os.fspath(arguments.output.resolve()))


if __name__ == "__main__":
    main()
