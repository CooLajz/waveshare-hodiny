#!/usr/bin/env python3
"""Ověří statický Pages web a případný čtyřdílný instalační balíček."""

from __future__ import annotations

import json
import hashlib
import re
import sys
from pathlib import Path


EXPECTED_PARTS = {
    "waveshare-hodiny.bootloader.bin": 0x0,
    "waveshare-hodiny.partitions.bin": 0x8000,
    "waveshare-hodiny.boot-app0.bin": 0xE000,
    "waveshare-hodiny.app.bin": 0x10000,
}
ROOT = Path(__file__).resolve().parent.parent
ASSET_ENTRY_PATTERN = re.compile(
    r'\{"([^"]+)", (\d+), "([0-9a-f]{64})"\}'
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_weather_assets(site: Path) -> None:
    source = (ROOT / "WaveshareHodiny" / "WeatherAnimationService.cpp").read_text(
        encoding="utf-8"
    )
    expected = {
        key: {"size": int(size), "sha256": digest}
        for key, size, digest in ASSET_ENTRY_PATTERN.findall(source)
    }
    root = site / "assets" / "weather-icons"
    catalog = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    actual: dict[str, dict[str, object]] = {}
    referenced_files: set[Path] = set()
    for style in ("monochrome", "flat", "line"):
        version_info = catalog.get("versions", {}).get(style)
        if not isinstance(version_info, dict):
            raise SystemExit(f"V asset katalogu chybí styl {style}.")
        relative_manifest = version_info.get("manifest")
        if not isinstance(relative_manifest, str) or ".." in Path(relative_manifest).parts:
            raise SystemExit(f"Styl {style} má neplatnou cestu manifestu.")
        manifest_path = root / relative_manifest
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("version") != version_info.get("version"):
            raise SystemExit(f"Verze asset manifestu {style} nesouhlasí s katalogem.")
        icons = manifest.get("icons")
        if not isinstance(icons, dict) or len(icons) != version_info.get("icons"):
            raise SystemExit(f"Asset manifest {style} má neplatný počet ikon.")
        for key, metadata in icons.items():
            if key in actual or not isinstance(metadata, dict):
                raise SystemExit(f"Duplicitní nebo neplatný asset: {key}")
            filename = metadata.get("file")
            if not isinstance(filename, str) or Path(filename).name != filename:
                raise SystemExit(f"Asset {key} má nebezpečnou cestu.")
            path = manifest_path.parent / filename
            if not path.is_file():
                raise SystemExit(f"Asset {key} chybí.")
            actual[key] = {"size": path.stat().st_size, "sha256": sha256(path)}
            referenced_files.add(path.resolve())
    published_files = {path.resolve() for path in root.glob("*/*.gif")}
    if actual != expected or published_files != referenced_files:
        raise SystemExit("Veřejné Meteocons neodpovídají firmwarovému allowlistu.")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Použití: validate_pages_bundle.py ADRESAR_WEBU")

    site = Path(sys.argv[1]).resolve()
    for relative in ("index.html", "styles.css", "app.js"):
        path = site / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise SystemExit(f"Povinný soubor webu chybí nebo je prázdný: {path}")
    index_html = (site / "index.html").read_text(encoding="utf-8")
    expected_css_url = f"styles.css?v={sha256(site / 'styles.css')[:12]}"
    expected_js_url = f"app.js?v={sha256(site / 'app.js')[:12]}"
    if expected_css_url not in index_html or expected_js_url not in index_html:
        raise SystemExit("HTML nepoužívá obsahové verze aktuálního CSS a JavaScriptu.")
    validate_weather_assets(site)

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
    ota_path = firmware / "waveshare-hodiny.ota.bin"
    ota_metadata_path = firmware / "ota.json"
    if not ota_path.is_file() or not ota_metadata_path.is_file():
        raise SystemExit("Ve veřejném release chybí OTA obraz nebo metadata.")
    ota = json.loads(ota_metadata_path.read_text(encoding="utf-8"))
    expected_ota_keys = {"version", "chipFamily", "size", "sha256", "url"}
    if set(ota) != expected_ota_keys:
        raise SystemExit("OTA metadata mají neplatnou strukturu.")
    if (
        ota["version"] != manifest.get("version")
        or ota["chipFamily"] != "ESP32-S3"
        or ota["size"] != ota_path.stat().st_size
        or ota["sha256"] != sha256(ota_path)
        or ota["url"] != "/waveshare-hodiny/firmware/waveshare-hodiny.ota.bin"
    ):
        raise SystemExit("OTA metadata neodpovídají veřejnému OTA obrazu.")

    catalog_path = firmware / "releases.json"
    if catalog_path.exists():
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
        releases = catalog.get("releases")
        if not isinstance(releases, list) or not 1 <= len(releases) <= 5:
            raise SystemExit("Katalog musí obsahovat jednu až pět stabilních verzí.")
        versions: set[str] = set()
        for index, release in enumerate(releases):
            if not isinstance(release, dict) or set(release) != {"version", "manifest"}:
                raise SystemExit("Položka katalogu verzí má neplatnou strukturu.")
            version = release["version"]
            relative_manifest = release["manifest"]
            expected_manifest = f"firmware/releases/{version}/manifest.json"
            if (
                not isinstance(version, str)
                or not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version)
                or version in versions
                or relative_manifest != expected_manifest
            ):
                raise SystemExit("Katalog obsahuje neplatnou nebo duplicitní verzi.")
            versions.add(version)
            archived_manifest_path = site / relative_manifest
            archived_manifest = json.loads(archived_manifest_path.read_text(encoding="utf-8"))
            if archived_manifest.get("version") != version:
                raise SystemExit(f"Manifest archivní verze {version} nesouhlasí s katalogem.")
            archived_builds = archived_manifest.get("builds")
            if not isinstance(archived_builds, list) or len(archived_builds) != 1:
                raise SystemExit(f"Archivní verze {version} nemá právě jeden build.")
            archived_build = archived_builds[0]
            archived_parts = archived_build.get("parts")
            if (
                archived_build.get("chipFamily") != "ESP32-S3"
                or archived_build.get("improv") is not True
                or not isinstance(archived_parts, list)
                or len(archived_parts) != len(EXPECTED_PARTS)
                or any(
                    not isinstance(part, dict)
                    or set(part) != {"path", "offset"}
                    or not isinstance(part["path"], str)
                    or not isinstance(part["offset"], int)
                    or isinstance(part["offset"], bool)
                    for part in archived_parts
                )
                or {part.get("path"): part.get("offset") for part in archived_parts if isinstance(part, dict)} != EXPECTED_PARTS
            ):
                raise SystemExit(f"Archivní manifest {version} nemá očekávaný instalační build.")
            for name in EXPECTED_PARTS:
                archived_part = archived_manifest_path.parent / name
                if not archived_part.is_file() or archived_part.stat().st_size == 0:
                    raise SystemExit(f"Archivní verzi {version} chybí část {name}.")
            if index == 0 and archived_manifest != manifest:
                raise SystemExit("První verze katalogu neodpovídá aktuálnímu manifestu.")
    print(f"Web i instalační balíček verze {manifest.get('version')} jsou platné.")


if __name__ == "__main__":
    main()
