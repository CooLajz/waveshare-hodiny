#!/usr/bin/env python3
"""Publikuje pouze animované Meteocons uvedené ve firmwarovém allowlistu."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
FIRMWARE_SOURCE = ROOT / "WaveshareHodiny" / "WeatherAnimationService.cpp"
ENTRY_PATTERN = re.compile(r'\{"([^"]+)", (\d+), "([0-9a-f]{64})"\}')
VERSION_PATTERN = re.compile(
    r'constexpr char (MONOCHROME|FLAT|LINE)_ASSET_VERSION\[\] =\s*"([^"]+)";'
)


def write_json(path: Path, value: object) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "docs" / "assets" / "weather-icons",
    )
    args = parser.parse_args()

    source = args.package.resolve()
    output = args.output.resolve()
    firmware = FIRMWARE_SOURCE.read_text(encoding="utf-8")
    entries = ENTRY_PATTERN.findall(firmware)
    versions = {
        style.lower(): version for style, version in VERSION_PATTERN.findall(firmware)
    }
    if len(entries) != 42 or set(versions) != {"monochrome", "flat", "line"}:
        raise SystemExit("Firmwarový allowlist nebo verze assetů mají nečekaný tvar.")
    if not (source / "LICENSE.txt").is_file():
        raise SystemExit("Ve zdrojovém balíčku chybí LICENSE.txt.")

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    shutil.copy2(source / "LICENSE.txt", output / "LICENSE.txt")

    published_versions: dict[str, dict[str, object]] = {}
    for style, version in versions.items():
        style_entries = [entry for entry in entries if entry[0].startswith(f"{style}-")]
        destination = output / version
        destination.mkdir()
        icons: dict[str, dict[str, object]] = {}
        for key, expected_size, expected_sha256 in style_entries:
            asset = source / f"{key}.gif"
            if not asset.is_file():
                raise SystemExit(f"Chybí používaný asset: {asset}")
            data = asset.read_bytes()
            actual_sha256 = hashlib.sha256(data).hexdigest()
            if len(data) != int(expected_size) or actual_sha256 != expected_sha256:
                raise SystemExit(f"Asset neodpovídá firmwarovému allowlistu: {key}")
            shutil.copy2(asset, destination / asset.name)
            icons[key] = {
                "file": asset.name,
                "size": len(data),
                "sha256": actual_sha256,
            }
        manifest = {
            "schemaVersion": 1,
            "assetSet": "weather-icons",
            "version": version,
            "format": "gif",
            "width": 84,
            "height": 84,
            "license": {"name": "MIT", "file": "../LICENSE.txt"},
            "icons": icons,
        }
        write_json(destination / "manifest.json", manifest)
        published_versions[style] = {
            "version": version,
            "manifest": f"{version}/manifest.json",
            "icons": len(icons),
        }

    write_json(
        output / "manifest.json",
        {
            "schemaVersion": 1,
            "assetSet": "weather-icons",
            "versions": published_versions,
        },
    )
    print(f"Publikováno {len(entries)} používaných GIFů do {output}.")


if __name__ == "__main__":
    main()
