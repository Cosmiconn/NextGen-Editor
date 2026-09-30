#!/usr/bin/env python3
"""Strict full-corpus ResMap audit runner for directories and ZIP archives.

Linux/headless counterpart to tools/audit_resmap.ps1. Inputs remain separate scan
roots so the physical archive corpus is audited, while every discovered ResMap
directory is also supplied as an asset root for legacy/external texture lookup.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def add_unique(paths: list[Path], path: Path) -> None:
    resolved = path.resolve()
    key = str(resolved).casefold()
    if all(str(existing).casefold() != key for existing in paths):
        paths.append(resolved)


def safe_extract_zip(archive: Path, destination: Path) -> None:
    root = destination.resolve()
    with zipfile.ZipFile(archive) as handle:
        for info in handle.infolist():
            target = (destination / info.filename).resolve()
            try:
                target.relative_to(root)
            except ValueError as exc:
                raise RuntimeError(
                    f"Archive entry escapes extraction root: {archive} :: {info.filename}"
                ) from exc
        handle.extractall(destination)


def discover_asset_roots(root: Path, output: list[Path]) -> None:
    add_unique(output, root)
    for path in root.rglob("*"):
        if path.is_dir() and path.name.casefold() == "resmap":
            add_unique(output, path)


def run_logged(command: list[str], report: Path) -> tuple[int, str | None]:
    report.parent.mkdir(parents=True, exist_ok=True)
    summary: str | None = None
    with report.open("w", encoding="utf-8", newline="") as handle:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        assert process.stdout is not None
        for line in process.stdout:
            sys.stdout.write(line)
            handle.write(line)
            if line.startswith("SUMMARY"):
                summary = line.rstrip("\r\n")
        return process.wait(), summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the strict ResMap renderer audit over directories and/or ZIP archives."
    )
    parser.add_argument("--audit-exe", required=True, type=Path)
    parser.add_argument("--report", type=Path, default=Path("resmap-strict-audit.txt"))
    parser.add_argument("--defer-priority2-character-shaders", action="store_true")
    parser.add_argument("inputs", nargs="+", type=Path)
    args = parser.parse_args()

    audit = args.audit_exe.resolve(strict=True)
    if not audit.is_file():
        raise RuntimeError(f"Audit executable is not a file: {audit}")

    inputs = [path.resolve(strict=True) for path in args.inputs]
    scan_roots: list[Path] = []
    asset_roots: list[Path] = []

    with tempfile.TemporaryDirectory(prefix="nextgen-resmap-audit-") as temp_name:
        temp_root = Path(temp_name)

        archive_index = 0
        for input_path in inputs:
            if input_path.is_dir():
                add_unique(scan_roots, input_path)
                discover_asset_roots(input_path, asset_roots)
                continue

            if not input_path.is_file():
                raise RuntimeError(f"Input is neither a directory nor a file: {input_path}")
            if input_path.suffix.casefold() != ".zip":
                raise RuntimeError(f"Only directories and .zip archives are supported: {input_path}")

            archive_index += 1
            extract_root = temp_root / f"archive-{archive_index:02d}"
            extract_root.mkdir(parents=True)
            print(f"Extracting {input_path} -> {extract_root}", flush=True)
            safe_extract_zip(input_path, extract_root)
            add_unique(scan_roots, extract_root)
            discover_asset_roots(extract_root, asset_roots)

        if not scan_roots:
            raise RuntimeError("No ResMap scan roots were discovered.")

        command = [str(audit), "--strict-renderer"]
        if args.defer_priority2_character_shaders:
            command.append("--defer-priority2-character-shaders")
        for root in asset_roots:
            command += ["--asset-root", str(root)]
        command += [str(root) for root in scan_roots]

        report = args.report.resolve()
        print("Strict ResMap audit", flush=True)
        print(f"  executable : {audit}", flush=True)
        print(f"  scan roots : {len(scan_roots)}", flush=True)
        print(f"  asset roots: {len(asset_roots)}", flush=True)
        print(f"  report     : {report}", flush=True)

        exit_code, summary = run_logged(command, report)

        if summary is None:
            raise RuntimeError(f"Audit did not emit a SUMMARY line. See: {report}")
        if exit_code != 0:
            raise RuntimeError(
                f"Strict ResMap audit failed with exit code {exit_code}. See: {report}"
            )
        if "rendererGapFiles=0" not in summary.split():
            raise RuntimeError(
                "Strict ResMap audit did not finish at rendererGapFiles=0. "
                f"See: {report}"
            )

        print("", flush=True)
        print("STRICT RESMAP AUDIT PASSED", flush=True)
        print(summary, flush=True)
        return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
