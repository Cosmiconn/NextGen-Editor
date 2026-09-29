#!/usr/bin/env python3
"""Cross-platform ResMap visual acceptance runner.

This is the Linux/native counterpart to verify_resmap_visual.ps1. It normalizes
split ResMap archives into a production-like <Client>/resmap tree, selects the
18 evidence-driven representatives, renders deterministic t=0/0.25/1.0 OpenGL
snapshots, and writes review evidence without changing renderer semantics.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

RESMAP_RE = re.compile(r"^resmap(?:[_.-].+)?$", re.IGNORECASE)
SNAPSHOT_STEM_RE = re.compile(r"[^A-Za-z0-9._-]")
DYNAMIC_CATEGORIES = {
    "texture_transform",
    "flip_controller",
    "particles_classic",
    "particles_mesh",
    "particles_world_space",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def snapshot_stem(value: str) -> str:
    return SNAPSHOT_STEM_RE.sub("_", value)


def is_ignored(relative: Path) -> bool:
    if relative.name.startswith("._"):
        return True
    return any(part.lower() == "__macosx" for part in relative.parts)


def is_nif(path: Path) -> bool:
    return path.suffix.lower() == ".nif" and not is_ignored(path)


def logical_resmap_roots(path: Path) -> list[Path]:
    path = path.resolve(strict=True)
    if RESMAP_RE.fullmatch(path.name):
        return [path]

    wrapped = sorted(
        (child for child in path.iterdir() if child.is_dir() and RESMAP_RE.fullmatch(child.name)),
        key=lambda item: item.name.lower(),
    )
    return wrapped if wrapped else [path]


def safe_extract_zip(archive: Path, destination: Path) -> None:
    destination = destination.resolve()
    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            # Zip entries always use POSIX separators. Reject traversal instead of relying
            # on platform-specific extraction sanitization.
            relative = Path(info.filename)
            if relative.is_absolute() or ".." in relative.parts:
                raise RuntimeError(f"Unsafe ZIP member in {archive}: {info.filename}")
            target = (destination / relative).resolve()
            try:
                target.relative_to(destination)
            except ValueError as exc:
                raise RuntimeError(f"Unsafe ZIP member in {archive}: {info.filename}") from exc
        zf.extractall(destination)


def merge_root(source: Path, destination: Path) -> tuple[int, int, int]:
    merged = 0
    duplicates = 0
    physical_nifs = 0

    for file in source.rglob("*"):
        if not file.is_file():
            continue
        relative = file.relative_to(source)
        if is_ignored(relative):
            continue

        if relative.suffix.lower() == ".nif":
            physical_nifs += 1

        target = destination / relative
        if target.is_file():
            same = target.stat().st_size == file.stat().st_size
            if same:
                same = sha256(target) == sha256(file)
            if not same:
                raise RuntimeError(
                    "Conflicting ResMap archive path: "
                    f"{relative}\n  existing: {target}\n  incoming: {file}"
                )
            duplicates += 1
            continue

        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
        merged += 1

    return merged, duplicates, physical_nifs


def run_logged(command: list[str], log_path: Path, *, append: bool = False, env: dict[str, str] | None = None) -> int:
    mode = "a" if append else "w"
    with log_path.open(mode, encoding="utf-8", newline="") as log:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            log.write(line)
        return process.wait()


def require_file(path: Path, label: str) -> Path:
    resolved = path.resolve(strict=True)
    if not resolved.is_file():
        raise RuntimeError(f"{label} is not a file: {resolved}")
    return resolved


def render_command(snapshot: Path, model_root: Path, category_dir: Path, runtime_map_dir: Path,
                   relative_path: str, use_xvfb: bool) -> tuple[list[str], dict[str, str] | None]:
    command = [
        str(snapshot),
        str(model_root),
        str(category_dir),
        "--runtime-map-dir",
        str(runtime_map_dir),
        relative_path,
    ]
    env = None
    if use_xvfb:
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            raise RuntimeError("--xvfb requested but xvfb-run is not available")
        command = [xvfb, "-a", *command]
        env = os.environ.copy()
        env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
    return command, env


def write_review_outputs(output: Path, rows: list[dict[str, str]]) -> None:
    review_path = output / "review.tsv"
    fields = [
        "Category", "Path", "Reason", "NifSha256",
        "T000Sha256", "T025Sha256", "T100Sha256",
        "T000", "T025", "T100",
    ]
    with review_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    checklist = output / "VISUAL_REVIEW_CHECKLIST.md"
    lines = [
        "# ResMap visual reference matrix",
        "",
        "Generated from real NIF parser properties by nif_visual_matrix and rendered through the normal NifMeshRenderer::LoadModelsForSet -> Draw runtime path.",
        "Split input archives are normalized into one production-like <Client>/resmap tree. Identical duplicate paths are deduplicated; conflicting duplicate paths fail the run.",
        "",
        "For every row compare: geometry, texture assignment, UVs, blend/alpha, depth, culling, material/color, environment, animation and particle position/motion where applicable.",
        "",
    ]
    for item in rows:
        lines.extend([
            f"## {item['Category']}",
            "",
            f"- NIF: {item['Path']}",
            f"- Selection evidence: {item['Reason']}",
            f"- NIF SHA-256: {item['NifSha256']}",
            f"- t=0.00 s: {item['T000']} (SHA-256 {item['T000Sha256']})",
            f"- t=0.25 s: {item['T025']} (SHA-256 {item['T025Sha256']})",
            f"- t=1.00 s: {item['T100']} (SHA-256 {item['T100Sha256']})",
            "- [ ] Geometry",
            "- [ ] Texture assignment / UVs",
            "- [ ] Alpha / blend / depth / culling",
            "- [ ] Material / color / brightness",
            "- [ ] Environment / shader-specific appearance",
            "- [ ] Animation / particles (when applicable)",
            "- [ ] No unexplained renderer deviation",
            "",
        ])
    checklist.write_text("\n".join(lines), encoding="utf-8")

    html_path = output / "index.html"
    sections: list[str] = []
    for item in rows:
        sections.append(
            "<section>"
            f"<h2>{html.escape(item['Category'])}</h2>"
            f"<p><code>{html.escape(item['Path'])}</code></p>"
            f"<p>{html.escape(item['Reason'])}</p>"
            f"<p><small>NIF SHA-256: {html.escape(item['NifSha256'])}</small></p>"
            "<div class='shots'>"
            f"<div class='shot'><h3>t = 0.00 s</h3><img src='{html.escape(item['T000'])}'></div>"
            f"<div class='shot'><h3>t = 0.25 s</h3><img src='{html.escape(item['T025'])}'></div>"
            f"<div class='shot'><h3>t = 1.00 s</h3><img src='{html.escape(item['T100'])}'></div>"
            "</div></section>"
        )
    html_path.write_text(
        "<!doctype html><html><head><meta charset='utf-8'><title>ResMap visual matrix</title>"
        "<style>body{font-family:Segoe UI,Arial,sans-serif;background:#111;color:#ddd;margin:24px}"
        "section{border:1px solid #333;border-radius:8px;padding:16px;margin:0 0 20px}"
        "h2{margin-top:0}.shots{display:flex;gap:16px;flex-wrap:wrap}.shot{min-width:300px;flex:1}"
        ".shot img{max-width:512px;width:100%;height:auto;background:#000;border:1px solid #444}"
        "code{color:#9de}</style></head><body><h1>ResMap visual reference matrix</h1>"
        + "".join(sections)
        + "</body></html>",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate the deterministic ResMap visual acceptance matrix.")
    parser.add_argument("--matrix-exe", required=True, type=Path)
    parser.add_argument("--snapshot-exe", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("resmap-visual-matrix"))
    parser.add_argument("--allow-incomplete-matrix", action="store_true")
    parser.add_argument("--xvfb", action="store_true", help="Run every OpenGL snapshot under xvfb-run; sets LIBGL_ALWAYS_SOFTWARE=1.")
    parser.add_argument("inputs", nargs="+", type=Path, help="ResMap directories and/or ZIP archives.")
    args = parser.parse_args()

    matrix = require_file(args.matrix_exe, "Matrix executable")
    snapshot = require_file(args.snapshot_exe, "Snapshot executable")
    inputs = [path.resolve(strict=True) for path in args.inputs]

    output = args.output.resolve()
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    with tempfile.TemporaryDirectory(prefix="nextgen-resmap-visual-") as temp_name:
        temp_root = Path(temp_name)
        source_roots: list[Path] = []
        seen_roots: set[str] = set()

        for index, input_path in enumerate(inputs, start=1):
            if input_path.is_dir():
                roots = logical_resmap_roots(input_path)
            elif input_path.is_file() and input_path.suffix.lower() == ".zip":
                extract_root = temp_root / f"archive-{index:02d}"
                extract_root.mkdir(parents=True)
                print(f"Extracting {input_path} -> {extract_root}")
                safe_extract_zip(input_path, extract_root)
                roots = logical_resmap_roots(extract_root)
            else:
                raise RuntimeError(f"Only directories and .zip archives are supported: {input_path}")

            for root in roots:
                key = os.path.normcase(str(root.resolve()))
                if key not in seen_roots:
                    seen_roots.add(key)
                    source_roots.append(root.resolve())

        if not source_roots:
            raise RuntimeError("No ResMap roots were discovered.")

        client_root = temp_root / "client"
        combined_resmap = client_root / "resmap"
        combined_resmap.mkdir(parents=True)

        merged_files = 0
        identical_duplicates = 0
        physical_nif_entries = 0
        for root in source_roots:
            print(f"Merging logical ResMap root: {root}")
            merged, duplicates, physical_nifs = merge_root(root, combined_resmap)
            merged_files += merged
            identical_duplicates += duplicates
            physical_nif_entries += physical_nifs

        logical_nif_paths = sum(
            1 for path in combined_resmap.rglob("*")
            if path.is_file() and path.suffix.lower() == ".nif" and not is_ignored(path.relative_to(combined_resmap))
        )

        runtime_map_dir = combined_resmap / "field" / "__NextGenVisualMatrixRuntime__"
        runtime_map_dir.mkdir(parents=True, exist_ok=True)

        provenance = {
            "sourceRoots": len(source_roots),
            "mergedFiles": merged_files,
            "identicalDuplicateFiles": identical_duplicates,
            "physicalNifEntries": physical_nif_entries,
            "logicalNifPaths": logical_nif_paths,
        }
        (output / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")

        print("Normalized ResMap client tree")
        print(f"  source roots          : {len(source_roots)}")
        print(f"  merged files          : {merged_files}")
        print(f"  identical duplicates  : {identical_duplicates}")
        print(f"  physical NIF entries  : {physical_nif_entries}")
        print(f"  logical NIF paths     : {logical_nif_paths}")
        print(f"  client root           : {client_root}")

        manifest = output / "matrix.tsv"
        matrix_log = output / "matrix.log"
        matrix_cmd = [str(matrix)]
        if not args.allow_incomplete_matrix:
            matrix_cmd.append("--strict")
        matrix_cmd += ["--manifest", str(manifest), str(combined_resmap)]
        print("\nSelecting ResMap visual matrix")
        if run_logged(matrix_cmd, matrix_log) != 0:
            raise RuntimeError(f"Visual matrix selection failed. See: {matrix_log}")

        with manifest.open("r", encoding="utf-8", newline="") as handle:
            matrix_rows = list(csv.DictReader(handle, delimiter="\t"))
        if not matrix_rows:
            raise RuntimeError(f"Visual matrix manifest is empty: {manifest}")

        snapshots = output / "snapshots"
        snapshots.mkdir()
        runtime_log = output / "runtime.log"
        review_rows: list[dict[str, str]] = []

        for row in matrix_rows:
            category = row.get("category", "").strip()
            root = Path(row.get("root", ""))
            relative_path = row.get("path", "").strip()
            reason = row.get("reason", "")
            if not category or not str(root) or not relative_path:
                raise RuntimeError(f"Malformed visual matrix row in {manifest}: {row}")

            category_dir = snapshots / category
            category_dir.mkdir(parents=True, exist_ok=True)
            print(f"\nRendering [{category}] {relative_path}")

            command, environment = render_command(
                snapshot, root, category_dir, runtime_map_dir, relative_path, args.xvfb
            )
            if run_logged(command, runtime_log, append=True, env=environment) != 0:
                raise RuntimeError(
                    f"OpenGL snapshot failed for [{category}] {relative_path}. See: {runtime_log}"
                )

            stem = snapshot_stem(relative_path)
            initial_bmp = category_dir / f"{stem}.bmp"
            t025_bmp = category_dir / f"{stem}__animated.bmp"
            t100_bmp = category_dir / f"{stem}__t1.bmp"
            for label, path in (("t=0.00", initial_bmp), ("t=0.25", t025_bmp), ("t=1.00", t100_bmp)):
                if not path.is_file():
                    raise RuntimeError(f"Missing {label} snapshot for [{category}]: {path}")

            nif_path = root / relative_path
            if not nif_path.is_file():
                raise RuntimeError(f"Selected NIF disappeared before review hashing: {nif_path}")

            hashes = (sha256(initial_bmp), sha256(t025_bmp), sha256(t100_bmp))
            if category in DYNAMIC_CATEGORIES and hashes[0] == hashes[1] == hashes[2]:
                raise RuntimeError(
                    f"Dynamic visual matrix category [{category}] produced identical snapshots "
                    f"at t=0.00/0.25/1.00: {relative_path}"
                )

            review_rows.append({
                "Category": category,
                "Path": relative_path,
                "Reason": reason,
                "NifSha256": sha256(nif_path),
                "T000Sha256": hashes[0],
                "T025Sha256": hashes[1],
                "T100Sha256": hashes[2],
                "T000": f"snapshots/{category}/{stem}.bmp",
                "T025": f"snapshots/{category}/{stem}__animated.bmp",
                "T100": f"snapshots/{category}/{stem}__t1.bmp",
            })

        write_review_outputs(output, review_rows)

        print("\nRESMAP VISUAL MATRIX GENERATED")
        print(f"  provenance : {output / 'provenance.json'}")
        print(f"  matrix     : {manifest}")
        print(f"  evidence   : {output / 'review.tsv'}")
        print(f"  snapshots  : {snapshots}")
        print(f"  review     : {output / 'VISUAL_REVIEW_CHECKLIST.md'}")
        print(f"  gallery    : {output / 'index.html'}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, zipfile.BadZipFile, subprocess.SubprocessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
