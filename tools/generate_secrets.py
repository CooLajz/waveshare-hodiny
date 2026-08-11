#!/usr/bin/env python3
"""Bezpečně převede lokální .env do ignorované Arduino hlavičky."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent
ENV_FILE = ROOT / ".env"
LOCAL_DIR = ROOT / "WaveshareHodiny" / "local"
OUTPUT_FILE = LOCAL_DIR / "secrets.h"
FIRMWARE_CONFIG_FILE = LOCAL_DIR / "firmware_config.h"
REQUIRED_KEYS = ("WIFI_SSID", "WIFI_PASSWORD")
FIRMWARE_KEYS = ("FIRMWARE_SERVER_URL", "FIRMWARE_PROJECT_SLUG")
PUBLIC_FIRMWARE_CONFIG = {
    "FIRMWARE_SERVER_URL": "https://coolajz.github.io",
    "FIRMWARE_PROJECT_SLUG": "waveshare-hodiny",
    "FIRMWARE_OTA_METADATA_PATH": "/waveshare-hodiny/firmware/ota.json",
    "FIRMWARE_WEATHER_ASSET_PATH": "/waveshare-hodiny/assets/weather-icons",
}
HOME_ASSISTANT_KEYS = (
    "HOME_ASSISTANT_URL",
    "HOME_ASSISTANT_TOKEN",
    "HA_ENTITY_WEATHER_CODE",
    "HA_ENTITY_OUTSIDE_TEMPERATURE",
    "HA_ENTITY_ROOM_TEMPERATURE",
    "HA_ENTITY_ROOM_CO2",
    "HA_ENTITY_ROOM_HUMIDITY",
    "HA_ENTITY_SUN",
)
def parse_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
            value = value[1:-1]
        values[key.strip()] = value
    return values


def cpp_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def main() -> None:
    LOCAL_DIR.mkdir(parents=True, exist_ok=True)
    release_build = "--release" in sys.argv[1:]
    public_release = "--public-release" in sys.argv[1:]
    if public_release:
        firmware_lines = ["#pragma once", ""]
        firmware_lines.extend(
            f"#define {key} {cpp_string(value)}"
            for key, value in PUBLIC_FIRMWARE_CONFIG.items()
        )
        firmware_lines.append("")
        temporary_firmware_file = FIRMWARE_CONFIG_FILE.with_suffix(".h.tmp")
        temporary_firmware_file.write_text(
            "\n".join(firmware_lines), encoding="utf-8"
        )
        temporary_firmware_file.chmod(0o600)
        temporary_firmware_file.replace(FIRMWARE_CONFIG_FILE)
        OUTPUT_FILE.unlink(missing_ok=True)
        print("Veřejný release používá GitHub Pages bez lokálních secrets.")
        return
    if not ENV_FILE.exists():
        OUTPUT_FILE.unlink(missing_ok=True)
        FIRMWARE_CONFIG_FILE.unlink(missing_ok=True)
        print("Lokální .env chybí; Wi-Fi a NTP zůstávají vypnuté.")
        return

    values = parse_env(ENV_FILE)
    missing_firmware = [key for key in FIRMWARE_KEYS if not values.get(key)]
    if missing_firmware:
        FIRMWARE_CONFIG_FILE.unlink(missing_ok=True)
    else:
        firmware_lines = [
            "#pragma once",
            "",
            f"#define FIRMWARE_SERVER_URL {cpp_string(values['FIRMWARE_SERVER_URL'].rstrip('/'))}",
            f"#define FIRMWARE_PROJECT_SLUG {cpp_string(values['FIRMWARE_PROJECT_SLUG'])}",
            "",
        ]
        temporary_firmware_file = FIRMWARE_CONFIG_FILE.with_suffix(".h.tmp")
        temporary_firmware_file.write_text(
            "\n".join(firmware_lines), encoding="utf-8"
        )
        temporary_firmware_file.chmod(0o600)
        temporary_firmware_file.replace(FIRMWARE_CONFIG_FILE)
    if release_build:
        OUTPUT_FILE.unlink(missing_ok=True)
        print("Release konfigurace neobsahuje lokální Wi-Fi ani aplikační údaje.")
        if missing_firmware:
            print("Konfigurace Firmware Hubu chybí; OTA zůstává vypnuté.")
        else:
            print("Veřejná konfigurace Firmware Hubu byla připravena pro sestavení.")
        return
    missing = [key for key in REQUIRED_KEYS if not values.get(key)]
    if missing:
        OUTPUT_FILE.unlink(missing_ok=True)
        print("V .env chybí Wi-Fi údaje; Wi-Fi a NTP zůstávají vypnuté.")
        return

    lines = [
        "#pragma once",
        "",
        f"#define WIFI_SSID {cpp_string(values['WIFI_SSID'])}",
        f"#define WIFI_PASSWORD {cpp_string(values['WIFI_PASSWORD'])}",
    ]
    missing_home_assistant = [
        key for key in HOME_ASSISTANT_KEYS if not values.get(key)
    ]
    if not missing_home_assistant:
        lines.extend(
            [
                "",
                f"#define HOME_ASSISTANT_URL {cpp_string(values['HOME_ASSISTANT_URL'].rstrip('/'))}",
                f"#define HOME_ASSISTANT_TOKEN {cpp_string(values['HOME_ASSISTANT_TOKEN'])}",
                f"#define HA_ENTITY_WEATHER_CODE {cpp_string(values['HA_ENTITY_WEATHER_CODE'])}",
                f"#define HA_ENTITY_OUTSIDE_TEMPERATURE {cpp_string(values['HA_ENTITY_OUTSIDE_TEMPERATURE'])}",
                f"#define HA_ENTITY_ROOM_TEMPERATURE {cpp_string(values['HA_ENTITY_ROOM_TEMPERATURE'])}",
                f"#define HA_ENTITY_ROOM_CO2 {cpp_string(values['HA_ENTITY_ROOM_CO2'])}",
                f"#define HA_ENTITY_ROOM_HUMIDITY {cpp_string(values['HA_ENTITY_ROOM_HUMIDITY'])}",
                f"#define HA_ENTITY_SUN {cpp_string(values['HA_ENTITY_SUN'])}",
            ]
        )
    lines.append("")
    temporary_file = OUTPUT_FILE.with_suffix(".h.tmp")
    temporary_file.write_text("\n".join(lines), encoding="utf-8")
    temporary_file.chmod(0o600)
    temporary_file.replace(OUTPUT_FILE)
    if missing_home_assistant:
        print("Wi-Fi konfigurace byla připravena; Home Assistant zůstává vypnutý.")
    else:
        print("Wi-Fi a Home Assistant konfigurace byly připraveny pro sestavení.")
    if missing_firmware:
        print("Konfigurace Firmware Hubu chybí; OTA zůstává vypnuté.")
    else:
        print("Veřejná konfigurace Firmware Hubu byla připravena pro sestavení.")


if __name__ == "__main__":
    main()
