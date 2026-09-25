#!/usr/bin/env python3
"""Import the approved NextGen icon packs into the repository.

The pipeline intentionally uses both user-approved archives:
- True_Vector_Set: real path-based SVG masters + curated small-size PNGs.
- Complete_PNG_SVG: high-resolution raster appearance reference.

No icon artwork is generated or reinterpreted here; this script only verifies and
copies the approved assets into stable repository locations.
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
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def read_csv_from_zip(zf: zipfile.ZipFile, name: str, delimiter: str = ";"):
    text = zf.read(name).decode("utf-8-sig").splitlines()
    return list(csv.DictReader(text, delimiter=delimiter))


def ensure_true_vector(svg: bytes, name: str) -> None:
    text = svg.decode("utf-8")
    if re.search(r"<(?:svg:)?image\b|data:image|base64", text, re.IGNORECASE):
        raise RuntimeError(f"{name}: embedded raster image found in vector master")
    if not re.search(r"<(?:svg:)?path\b", text, re.IGNORECASE):
        raise RuntimeError(f"{name}: no SVG path geometry found")


def write_bytes(root: Path, relative: Path, data: bytes) -> None:
    target = root / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vector-pack", type=Path, required=True)
    parser.add_argument("--raster-pack", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--clean", action="store_true", help="remove generated icon asset dirs first")
    args = parser.parse_args()

    repo = args.repo_root.resolve()
    vector_zip = args.vector_pack.resolve()
    raster_zip = args.raster_pack.resolve()
    icon_root = repo / "assets/ui/icons"
    brand_root = repo / "assets/ui/branding/ng"

    if args.clean:
        for p in (icon_root / "svg-master", icon_root / "png", icon_root / "png-master", brand_root):
            if p.exists():
                shutil.rmtree(p)

    with zipfile.ZipFile(vector_zip) as vz, zipfile.ZipFile(raster_zip) as rz:
        vector_rows = read_csv_from_zip(vz, "manifest.csv")
        raster_rows = read_csv_from_zip(rz, "manifest_combined.csv")

        if len(vector_rows) != 54:
            raise RuntimeError(f"vector manifest: expected 54 icons, got {len(vector_rows)}")

        raster_keys = {
            (r["Gruppe"], str(r["Nr"]), r["Name"])
            for r in raster_rows
            if r["Variante"] == "normalized_256"
        }
        vector_keys = {(r["Gruppe"], str(r["Nr"]), r["Name"]) for r in vector_rows}
        if len(raster_keys) != 54 or vector_keys != raster_keys:
            missing = sorted(vector_keys - raster_keys)
            extra = sorted(raster_keys - vector_keys)
            raise RuntimeError(f"pack manifests differ; missing={missing}, extra={extra}")

        manifest = {
            "schema": 1,
            "vector_pack": {
                "filename": vector_zip.name,
                "sha256": sha256_file(vector_zip),
                "role": "true vector masters and curated small-size PNG exports",
            },
            "raster_pack": {
                "filename": raster_zip.name,
                "sha256": sha256_file(raster_zip),
                "role": "high-resolution raster appearance reference",
            },
            "sizes": list(SIZES),
            "icons": [],
        }

        raster_by_key = {
            (r["Gruppe"], str(r["Nr"]), r["Name"]): r
            for r in raster_rows
            if r["Variante"] == "master_1024"
        }

        for row in vector_rows:
            group = row["Gruppe"]
            group_dir = GROUP_DIR[group]
            source_svg = row["SVG"]
            svg_name = Path(source_svg).name
            svg_data = vz.read(source_svg)
            ensure_true_vector(svg_data, source_svg)
            write_bytes(icon_root, Path("svg-master") / group_dir / svg_name, svg_data)

            runtime_paths = {}
            for size in SIZES:
                source_png = f"icons_png/{size}/{group_dir}/{Path(svg_name).with_suffix('.png').name}"
                rel = Path("png") / str(size) / group_dir / Path(svg_name).with_suffix(".png").name
                write_bytes(icon_root, rel, vz.read(source_png))
                runtime_paths[str(size)] = str(rel).replace("\\", "/")

            key = (group, str(row["Nr"]), row["Name"])
            rr = raster_by_key[key]
            raster_source = rr["PNG B"]  # transparent 1024 master
            master_rel = Path("png-master") / group_dir / Path(svg_name).with_suffix(".png").name
            write_bytes(icon_root, master_rel, rz.read(raster_source))

            manifest["icons"].append({
                "group": group,
                "number": int(row["Nr"]),
                "name": row["Name"],
                "vector_master": str((Path("svg-master") / group_dir / svg_name)).replace("\\", "/"),
                "raster_master": str(master_rel).replace("\\", "/"),
                "runtime_png": runtime_paths,
            })

        ng_svg = vz.read("svg/04_extra_icons/09_ng_icon.svg")
        ensure_true_vector(ng_svg, "svg/04_extra_icons/09_ng_icon.svg")
        write_bytes(brand_root, Path("ng-icon-master.svg"), ng_svg)
        for size in SIZES:
            write_bytes(
                brand_root,
                Path(str(size)) / "ng-icon.png",
                vz.read(f"icons_png/{size}/04_extra_icons/09_ng_icon.png"),
            )

    (icon_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Imported {len(manifest['icons'])} approved icons into {icon_root}")
    print(f"Vector pack SHA-256: {manifest['vector_pack']['sha256']}")
    print(f"Raster pack SHA-256: {manifest['raster_pack']['sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
