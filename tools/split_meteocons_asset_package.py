#!/usr/bin/env python3
"""Split a verified complete Meteocons package into upload-sized style sets."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


STYLES = ("monochrome", "flat", "line")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    source = args.package.resolve()
    manifest = json.loads((source / "asset-manifest.json").read_text())
    for style in STYLES:
        version = f"{args.version}-{style}"
        destination = args.output.resolve() / style
        if destination.exists():
            raise SystemExit(f"Output directory already exists: {destination}")
        destination.mkdir(parents=True)
        shutil.copyfile(source / "LICENSE.txt", destination / "LICENSE.txt")
        prefix = f"{style}-"
        icons = {
            key: {"file": f"{key}.gif"}
            for key in manifest["icons"]
            if key.startswith(prefix)
        }
        if not icons:
            raise RuntimeError(f"No icons found for style {style}")
        for key in icons:
            filename = f"{key}.gif"
            shutil.copyfile(source / filename, destination / filename)
        style_manifest = {
            "schemaVersion": 1,
            "assetSet": manifest["assetSet"],
            "version": version,
            "format": manifest["format"],
            "width": manifest["width"],
            "height": manifest["height"],
            "license": {
                "name": manifest["license"]["name"],
                "source": manifest["license"]["source"],
            },
            "icons": icons,
        }
        (destination / "asset-manifest.json").write_text(
            json.dumps(style_manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(f"{style}: {len(icons)} icons, version {version}")


if __name__ == "__main__":
    main()
