#!/usr/bin/env python3
"""Ověří 1:1 shodu vestavěných ikon s animovanými podmínkami."""

from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "assets" / "meteocons" / "monochrome-static"
ANIMATION_SERVICE = ROOT / "WaveshareHodiny" / "WeatherAnimationService.cpp"
GENERATED_ICONS = ROOT / "WaveshareHodiny" / "OpenWeatherIcons.c"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
TARGET_SIZE = (84, 84)


def png_size(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise SystemExit(f"Neplatný PNG soubor: {path}")
    return struct.unpack(">II", header[16:24])


animation_source = ANIMATION_SERVICE.read_text(encoding="utf-8")
generated_source = GENERATED_ICONS.read_text(encoding="utf-8")

animated = {
    key.removeprefix("monochrome-")
    for key in re.findall(r'\{"(monochrome-[^"]+)"', animation_source)
}
embedded = {
    name.replace("_", "-")
    for name in re.findall(
        r"const lv_img_dsc_t meteocons_static_([a-z_]+)", generated_source
    )
}
assets = {path.stem for path in ASSET_DIR.glob("*.png")}

if not animated:
    raise SystemExit("Nebyla nalezena žádná Monochrome animovaná podmínka")
if embedded != animated:
    raise SystemExit(
        f"Vestavěné ikony se liší od animací: navíc={sorted(embedded - animated)}, "
        f"chybí={sorted(animated - embedded)}"
    )
if assets != animated:
    raise SystemExit(
        f"PNG podklady se liší od animací: navíc={sorted(assets - animated)}, "
        f"chybí={sorted(animated - assets)}"
    )

for asset in sorted(ASSET_DIR.glob("*.png")):
    dimensions = png_size(asset)
    if dimensions != TARGET_SIZE:
        raise SystemExit(
            f"{asset} má {dimensions[0]} × {dimensions[1]} px, očekáváno 84 × 84 px"
        )

print(
    f"Ověřeno: {len(animated)} statických Monochrome ikon přesně odpovídá "
    "animovaným podmínkám a všechny PNG mají 84 × 84 px."
)
