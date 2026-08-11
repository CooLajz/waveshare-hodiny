#!/usr/bin/env python3
"""Ověří statický Pages web a případný čtyřdílný instalační balíček."""

from __future__ import annotations

import json
import sys
from pathlib import Path


EXPECTED_PARTS = {
    "waveshare-hodiny.bootloader.bin": 0x0,
    "waveshare-hodiny.partitions.bin": 0x8000,
    "waveshare-hodiny.boot-app0.bin": 0xE000,
    "waveshare-hodiny.app.bin": 0x10000,
}


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Použití: validate_pages_bundle.py ADRESAR_WEBU")

    site = Path(sys.argv[1]).resolve()
    for relative in ("index.html", "styles.css", "app.js"):
        path = site / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"Povinný soubor webu chybí nebo je prázdný: {path}")

    firmware = site / "firmware"
    manifest_path = firmware / "manifest.json"
    if not manifest_path.exists():
        unexpected = [path for path in firmware.glob("*") if path.is_file()]
        if unexpected:
            raise SystemExit("Firmware adresář obsahuje soubory bez manifestu.")
        print("Web je platný; firmware zatím není publikovaný.")
        return

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    builds = manifest.get("builds")
    if not isinstance(builds, list) or len(builds) != 1:
        raise SystemExit("Manifest musí obsahovat právě jeden build.")
    build = builds[0]
    if build.get("chipFamily") != "ESP32-S3" or build.get("improv") is not True:
        raise SystemExit("Manifest nemá očekávaný ESP32-S3 Improv build.")

    parts = build.get("parts")
    if not isinstance(parts, list) or len(parts) != len(EXPECTED_PARTS):
        raise SystemExit("Manifest musí obsahovat právě čtyři instalační části.")
    if any(
        not isinstance(part, dict)
        or set(part) != {"path", "offset"}
        or not isinstance(part["path"], str)
        or not isinstance(part["offset"], int)
        or isinstance(part["offset"], bool)
        for part in parts
    ):
        raise SystemExit("Instalační část má neplatnou strukturu nebo typy.")
    paths = [part["path"] for part in parts]
    if len(paths) != len(set(paths)):
        raise SystemExit("Manifest obsahuje duplicitní instalační část.")
    actual = {part["path"]: part["offset"] for part in parts}
    if actual != EXPECTED_PARTS:
        raise SystemExit(f"Instalační části nebo offsety nesouhlasí: {actual}")

    for name in EXPECTED_PARTS:
        path = firmware / name
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"Instalační část chybí nebo je prázdná: {path}")
    print(f"Web i instalační balíček verze {manifest.get('version')} jsou platné.")


if __name__ == "__main__":
    main()
