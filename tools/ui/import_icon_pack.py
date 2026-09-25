#!/usr/bin/env python3
"""Import the final approved NextGen icon pack into the repository.

Source of truth:
    NextGen_Icons_Final.zip

The archive contains:
- svg/: true path-based SVG masters
- png_256/ and png_1024/: transparent raster renders
- icons_png/{16,24,32,48,64,128}/: curated small-size runtime exports
- svg_embedded_png_* and button_tiles_svg_embedded_png_*: exact-raster SVG containers

No artwork is generated or reinterpreted here. The importer verifies the archive
structure and copies only approved assets into stable repository locations.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path

SIZES = (16, 24, 32, 48, 64, 128)
GROUP_DIR = {
    "toolbar": "01_toolbar_icons",
    "navigation": "02_navigation_icons",
    "panel": "03_panel_icons",
    "extra": "04_extra_icons",
    "additional_ui": "05_additional_ui_icons",
}
EXPECTED_COUNTS = {
    "toolbar": 18,
    "navigation": 15,
    "panel": 12,
    "extra": 9,
    "additional_ui": 14,
}
EXPECTED_TOTAL = sum(EXPECTED_COUNTS.values())


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_manifest(zf: zipfile.ZipFile):
    text = zf.read("manifest.csv").decode("utf-8-sig").splitlines()
    return list(csv.DictReader(text, delimiter=";"))


def ensure_true_vector(svg: bytes, name: str) -> None:
    text = svg.decode("utf-8")
    if re.search(r"<(?:svg:)?image\b|data:image|base64", text, re.IGNORECASE):
        raise RuntimeError(f"{name}: embedded raster found in true-vector directory")
    if not re.search(r"<(?:svg:)?path\b", text, re.IGNORECASE):
        raise RuntimeError(f"{name}: no SVG path geometry found")


def ensure_embedded_svg(svg: bytes, name: str) -> None:
    text = svg.decode("utf-8")
    if not re.search(r"<(?:svg:)?image\b|data:image|base64", text, re.IGNORECASE):
        raise RuntimeError(f"{name}: expected embedded PNG payload")


def write_bytes(root: Path, relative: Path, data: bytes) -> None:
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", type=Path, required=True,
                        help="Path to NextGen_Icons_Final.zip")
    parser.add_argument("--repo-root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--clean", action="store_true",
                        help="remove generated icon asset directories first")
    parser.add_argument("--include-embedded-svg", action="store_true",
                        help="also archive exact-raster SVG containers")
    args = parser.parse_args()

    repo = args.repo_root.resolve()
    pack = args.pack.resolve()
    icon_root = repo / "assets/ui/icons"
    brand_root = repo / "assets/ui/branding/ng"

    if args.clean:
        for p in (
            icon_root / "svg-master",
            icon_root / "png",
            icon_root / "png-master",
            icon_root / "svg-embedded-png",
            brand_root,
        ):
            if p.exists():
                shutil.rmtree(p)

    with zipfile.ZipFile(pack) as zf:
        rows = read_manifest(zf)
        if len(rows) != EXPECTED_TOTAL:
            raise RuntimeError(f"expected {EXPECTED_TOTAL} manifest rows, got {len(rows)}")

        counts = {group: 0 for group in EXPECTED_COUNTS}
        for row in rows:
            group = row["Gruppe"]
            if group not in GROUP_DIR:
                raise RuntimeError(f"unexpected icon group: {group}")
            counts[group] += 1
        if counts != EXPECTED_COUNTS:
            raise RuntimeError(f"unexpected group counts: {counts}")

        names = set(zf.namelist())
        manifest = {
            "schema": 2,
            "source": {
                "filename": pack.name,
                "sha256": sha256_file(pack),
                "role": "final approved icon source",
            },
            "sizes": list(SIZES),
            "icons": [],
        }

        for row in rows:
            group = row["Gruppe"]
            group_dir = GROUP_DIR[group]
            source_svg = row["SVG"]
            file_name = Path(source_svg).name

            svg_data = zf.read(source_svg)
            ensure_true_vector(svg_data, source_svg)
            vector_rel = Path("svg-master") / group_dir / file_name
            write_bytes(icon_root, vector_rel, svg_data)

            runtime_paths = {}
            png_name = Path(file_name).with_suffix(".png").name
            for size in SIZES:
                source_png = f"icons_png/{size}/{group_dir}/{png_name}"
                if source_png not in names:
                    raise RuntimeError(f"missing runtime PNG: {source_png}")
                rel = Path("png") / str(size) / group_dir / png_name
                write_bytes(icon_root, rel, zf.read(source_png))
                runtime_paths[str(size)] = rel.as_posix()

            source_master = row["PNG 1024"]
            master_rel = Path("png-master") / group_dir / png_name
            write_bytes(icon_root, master_rel, zf.read(source_master))

            embedded_paths = {}
            if args.include_embedded_svg:
                for label, source_prefix in (
                    ("256", "svg_embedded_png_256"),
                    ("1024", "svg_embedded_png_1024"),
                ):
                    source = f"{source_prefix}/{group_dir}/{file_name}"
                    data = zf.read(source)
                    ensure_embedded_svg(data, source)
                    rel = Path("svg-embedded-png") / label / group_dir / file_name
                    write_bytes(icon_root, rel, data)
                    embedded_paths[label] = rel.as_posix()

            manifest["icons"].append({
                "group": group,
                "number": int(row["Nr"]),
                "name": row["Name"],
                "vector_master": vector_rel.as_posix(),
                "raster_master": master_rel.as_posix(),
                "runtime_png": runtime_paths,
                "embedded_svg": embedded_paths,
            })

        ng_svg = zf.read("svg/04_extra_icons/09_ng_icon.svg")
        ensure_true_vector(ng_svg, "svg/04_extra_icons/09_ng_icon.svg")
        write_bytes(brand_root, Path("ng-icon-master.svg"), ng_svg)
        for size in SIZES:
            write_bytes(
                brand_root,
                Path(str(size)) / "ng-icon.png",
                zf.read(f"icons_png/{size}/04_extra_icons/09_ng_icon.png"),
            )
        write_bytes(
            brand_root,
            Path("1024") / "ng-icon.png",
            zf.read("png_1024/04_extra_icons/09_ng_icon.png"),
        )

    (icon_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"Imported {len(manifest['icons'])} approved icons into {icon_root}")
    print(f"Pack SHA-256: {manifest['source']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
