#!/usr/bin/env python3
"""Vygeneruje ignorovanou hlavičku s jedinou verzí release buildu."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
LOCAL_DIR = ROOT / "WaveshareHodiny" / "local"
OUTPUT_FILE = LOCAL_DIR / "release_version.h"
VERSION_PATTERN = re.compile(
    r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"
    r"(?:-(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)"
    r"(?:\.(?:0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*))*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)


def main() -> None:
    if len(sys.argv) != 2 or not VERSION_PATTERN.fullmatch(sys.argv[1]):
        raise SystemExit("Použití: generate_release_version.py SEMVER")
    LOCAL_DIR.mkdir(parents=True, exist_ok=True)
    temporary_file = OUTPUT_FILE.with_suffix(".h.tmp")
    temporary_file.write_text(
        f'#pragma once\n\n#define FIRMWARE_VERSION "{sys.argv[1]}"\n',
        encoding="utf-8",
    )
    temporary_file.chmod(0o600)
    temporary_file.replace(OUTPUT_FILE)


if __name__ == "__main__":
    main()
