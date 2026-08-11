#!/usr/bin/env python3
"""Build the complete LVGL-compatible Meteocons GIF asset library from SVG."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import shutil
import subprocess
import tarfile
from pathlib import Path
from urllib.parse import quote

from PIL import Image


STYLES = ("monochrome", "flat", "line")
FRAME_DURATION_MS = 70
FRAME_COUNT = 90
OUTPUT_SIZE = (84, 84)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract_package(archive: Path, root: Path) -> tuple[str, list[str]]:
    svg_root = root / "svg"
    svg_root.mkdir(parents=True)
    with tarfile.open(archive, "r:gz") as package:
        package_json = json.load(package.extractfile("package/package.json"))
        version = package_json["version"]
        style_icons: dict[str, set[str]] = {}
        for style in STYLES:
            destination = svg_root / style
            destination.mkdir()
            icons: set[str] = set()
            prefix = f"package/{style}/"
            for member in package.getmembers():
                if not member.isfile() or not member.name.startswith(prefix):
                    continue
                if not member.name.endswith(".svg"):
                    continue
                slug = Path(member.name).stem
                icons.add(slug)
                with package.extractfile(member) as source:
                    (destination / f"{slug}.svg").write_bytes(source.read())
            style_icons[style] = icons
    reference = style_icons[STYLES[0]]
    for style in STYLES[1:]:
        if style_icons[style] != reference:
            raise RuntimeError(f"Style {style} does not contain the same icons")
    return version, sorted(reference)


def chunks(values: list[str], count: int) -> list[list[str]]:
    result = [[] for _ in range(count)]
    for index, value in enumerate(values):
        result[index % count].append(value)
    return [chunk for chunk in result if chunk]


def render_chunk(
    root: Path,
    style: str,
    icons: list[str],
    renderer: Path,
    playwright_module: Path,
) -> None:
    subprocess.run(
        [
            "node",
            str(renderer),
            str(root),
            style,
            str(playwright_module),
            *icons,
        ],
        check=True,
    )


def frame_to_indexed(path: Path, monochrome: bool) -> Image.Image:
    rgba = Image.open(path).convert("RGBA")
    if rgba.size != OUTPUT_SIZE:
        raise RuntimeError(f"Unexpected rendered frame size: {rgba.size}")
    if monochrome:
        alpha = rgba.getchannel("A")
        rgba = Image.new("RGBA", rgba.size, (255, 255, 255, 0))
        rgba.putalpha(alpha)
    alpha = rgba.getchannel("A")
    rgb = Image.new("RGB", rgba.size, (0, 0, 0))
    rgb.paste(rgba.convert("RGB"), mask=alpha)
    indexed = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    indexed.putpalette(indexed.getpalette() + [0, 0, 0])
    indexed.info["transparency"] = 255
    indexed.paste(255, mask=alpha.point(lambda value: 255 if value < 96 else 0))
    return indexed


def make_gif(root: Path, style: str, icon: str) -> Path:
    frame_directory = root / "frames" / style / icon
    frames = [
        frame_to_indexed(path, style == "monochrome")
        for path in sorted(frame_directory.glob("*.png"))
    ]
    if len(frames) != FRAME_COUNT:
        raise RuntimeError(f"{style}/{icon} has {len(frames)} frames")
    output = root / "package" / f"{style}-{icon}.gif"
    frames[0].save(
        output,
        save_all=True,
        append_images=frames[1:],
        duration=FRAME_DURATION_MS,
        loop=0,
        disposal=2,
        transparency=255,
        optimize=True,
    )
    shutil.rmtree(frame_directory)
    return output


def build_manifest(
    package_directory: Path,
    base_url: str,
    project: str,
    asset_set: str,
    asset_version: str,
    upstream_version: str,
) -> dict:
    base_url = (
        f"{base_url.rstrip('/')}/{quote(project)}/assets/"
        f"{quote(asset_set)}/{quote(asset_version)}"
    )
    icons = {}
    for path in sorted(package_directory.glob("*.gif")):
        with Image.open(path) as image:
            frame_count = getattr(image, "n_frames", 1)
        icons[path.stem] = {
            "url": f"{base_url}/{quote(path.name)}",
            "size": path.stat().st_size,
            "sha256": sha256(path),
            "contentType": "image/gif",
            "frameCount": frame_count,
            "animated": frame_count > 1,
        }
    return {
        "schemaVersion": 1,
        "project": project,
        "assetSet": asset_set,
        "version": asset_version,
        "upstream": {
            "package": "@meteocons/svg",
            "version": upstream_version,
            "source": "https://github.com/basmilius/meteocons",
        },
        "renderer": {"engine": "Chromium SVG/SMIL"},
        "format": "gif",
        "width": OUTPUT_SIZE[0],
        "height": OUTPUT_SIZE[1],
        "frames": FRAME_COUNT,
        "frameDurationMs": FRAME_DURATION_MS,
        "styles": list(STYLES),
        "license": {
            "name": "MIT",
            "source": "Meteocons by Bas Milius",
            "url": f"{base_url}/LICENSE.txt",
        },
        "icons": icons,
    }


def verify_package(package_directory: Path, manifest: dict) -> None:
    paths = sorted(package_directory.glob("*.gif"))
    if not paths or len(paths) != len(manifest["icons"]):
        raise RuntimeError("GIF file count does not match the manifest")
    style_counts = {
        style: sum(path.name.startswith(f"{style}-") for path in paths)
        for style in STYLES
    }
    if len(set(style_counts.values())) != 1:
        raise RuntimeError(f"Styles have different file counts: {style_counts}")
    for path in paths:
        metadata = manifest["icons"].get(path.stem)
        if metadata is None:
            raise RuntimeError(f"Missing manifest entry for {path.name}")
        if metadata["size"] != path.stat().st_size:
            raise RuntimeError(f"Invalid size in manifest for {path.name}")
        if metadata["sha256"] != sha256(path):
            raise RuntimeError(f"Invalid SHA-256 in manifest for {path.name}")
        with Image.open(path) as image:
            if image.size != OUTPUT_SIZE or image.format != "GIF":
                raise RuntimeError(f"Invalid GIF format for {path.name}")
            if metadata["frameCount"] != getattr(image, "n_frames", 1):
                raise RuntimeError(f"Invalid frame count for {path.name}")
            if metadata["animated"] != (metadata["frameCount"] > 1):
                raise RuntimeError(f"Invalid animation flag for {path.name}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--playwright", required=True, type=Path)
    parser.add_argument("--license", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--asset-version", required=True)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--project", default="waveshare-hodiny")
    parser.add_argument("--asset-set", default="weather-icons")
    parser.add_argument("--workers", type=int, default=6)
    args = parser.parse_args()

    root = args.output.resolve()
    if root.exists():
        raise SystemExit(f"Output directory already exists: {root}")
    (root / "package").mkdir(parents=True)
    upstream_version, icons = extract_package(args.package, root)
    print(
        f"Meteocons {upstream_version}: {len(icons)} icons x {len(STYLES)} styles"
    )

    renderer = Path(__file__).with_name("render_meteocons_svg.mjs")
    jobs = []
    for style in STYLES:
        for chunk in chunks(icons, args.workers):
            jobs.append((style, chunk))
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [
            pool.submit(
                render_chunk,
                root,
                style,
                chunk,
                renderer,
                args.playwright,
            )
            for style, chunk in jobs
        ]
        for future in concurrent.futures.as_completed(futures):
            future.result()

    gif_jobs = [(style, icon) for style in STYLES for icon in icons]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        list(pool.map(lambda item: make_gif(root, *item), gif_jobs))

    shutil.copyfile(args.license, root / "package" / "LICENSE.txt")
    manifest = build_manifest(
        root / "package",
        args.base_url,
        args.project,
        args.asset_set,
        args.asset_version,
        upstream_version,
    )
    verify_package(root / "package", manifest)
    manifest_path = root / "package" / "asset-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Created {len(manifest['icons'])} GIF files in {root / 'package'}")


if __name__ == "__main__":
    main()
