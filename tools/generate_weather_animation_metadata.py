#!/usr/bin/env python3
"""Generate the firmware allowlist from published Meteocons manifests."""

import argparse
import json
import urllib.request


VERSION_PREFIX = "3.0.0-next.10-lvgl.2"
STYLES = ("flat", "line", "monochrome")
CONDITIONS = (
    "clear-day",
    "clear-night",
    "drizzle",
    "mist",
    "mostly-clear-day",
    "mostly-clear-night",
    "overcast",
    "overcast-day",
    "overcast-night",
    "partly-cloudy-day",
    "partly-cloudy-night",
    "rain",
    "sleet",
    "snow",
    "thunderstorms",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--base-url",
        required=True,
        help="Public base URL containing immutable per-style asset versions.",
    )
    args = parser.parse_args()
    base_url = args.base_url.rstrip("/")

    for style in STYLES:
        version = f"{VERSION_PREFIX}-{style}"
        with urllib.request.urlopen(
            f"{base_url}/{version}/manifest.json"
        ) as response:
            manifest = json.load(response)
        for condition in CONDITIONS:
            key = f"{style}-{condition}"
            metadata = manifest["icons"][key]
            print(
                f'    {{"{key}", {metadata["size"]}, '
                f'"{metadata["sha256"]}"}},'
            )


if __name__ == "__main__":
    main()
