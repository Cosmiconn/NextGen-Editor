#!/usr/bin/env python3
"""
Fault-isolated NIF corpus scanner.

The scanner is intentionally independent from the editor implementation.
Pass a parser/probe command containing the literal token {file}. Each NIF is
extracted to a temporary file and executed in its own subprocess with a hard
timeout. One pathological file therefore cannot stall the complete corpus run.

Example:
    python tools/nif_fullscan.py \
        --command "./build/nif_probe {file}" \
        --timeout 12 \
        --output nif-fullscan.csv \
        Client.zip resmap.zip resmap__2_.zip resmap__4_.zip resitem.zip reseffect.zip ressystem.zip resmenu.zip fixtures.zip

The probe should return exit code 0 for a successfully parsed NIF. It may emit
one JSON object on stdout. Known fields are copied into the CSV when present:
parts, vertices, triangles, embedded_textures, skinned, status, error.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import time
import zipfile


FIELDNAMES = [
    "archive",
    "path",
    "status",
    "parts",
    "vertices",
    "triangles",
    "embedded_textures",
    "skinned",
    "duration_ms",
    "error",
]


def nif_entries(zf: zipfile.ZipFile):
    for info in zf.infolist():
        if info.is_dir():
            continue
        if info.filename.lower().endswith(".nif"):
            yield info


def parse_probe_stdout(stdout: str) -> dict:
    text = stdout.strip()
    if not text:
        return {}
    # Prefer the final non-empty line so diagnostic logging before JSON is safe.
    for line in reversed(text.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            value = json.loads(line)
            if isinstance(value, dict):
                return value
        except json.JSONDecodeError:
            pass
    return {}


def run_probe(command_template: str, nif_path: Path, timeout: float) -> dict:
    quoted_file = shlex.quote(str(nif_path))
    cmd_text = command_template.replace("{file}", quoted_file)
    cmd = shlex.split(cmd_text)
    started = time.monotonic()
    try:
        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout,
            check=False,
        )
        elapsed = int((time.monotonic() - started) * 1000)
    except subprocess.TimeoutExpired as exc:
        elapsed = int((time.monotonic() - started) * 1000)
        return {
            "status": "TIMEOUT",
            "duration_ms": elapsed,
            "error": f"probe exceeded {timeout:g}s",
        }

    data = parse_probe_stdout(proc.stdout)
    status = str(data.get("status", "")).upper()
    if not status:
        status = "OK" if proc.returncode == 0 else "ERROR"

    error = str(data.get("error", "") or "").strip()
    if proc.returncode != 0 and not error:
        error = (proc.stderr or proc.stdout or f"probe exit {proc.returncode}").strip()
        # Keep the CSV practical even when a tool emits a large stack trace.
        if len(error) > 4000:
            error = error[-4000:]

    return {
        "status": status,
        "parts": data.get("parts", ""),
        "vertices": data.get("vertices", ""),
        "triangles": data.get("triangles", ""),
        "embedded_textures": data.get("embedded_textures", ""),
        "skinned": data.get("skinned", ""),
        "duration_ms": elapsed,
        "error": error,
    }


def scan_zip(archive: Path, writer: csv.DictWriter, args) -> tuple[int, int]:
    total = 0
    failed = 0
    try:
        zf = zipfile.ZipFile(archive, "r")
        # Force central-directory / CRC metadata validation early.
        entries = list(nif_entries(zf))
    except (zipfile.BadZipFile, OSError) as exc:
        writer.writerow({
            "archive": str(archive),
            "path": "",
            "status": "ARCHIVE_ERROR",
            "error": str(exc),
        })
        return 0, 1

    with zf:
        for index, info in enumerate(entries, start=1):
            total += 1
            suffix = ".nif"
            with tempfile.NamedTemporaryFile(prefix="nextgen_nif_", suffix=suffix, delete=False) as tmp:
                tmp_path = Path(tmp.name)
                try:
                    with zf.open(info, "r") as src:
                        while True:
                            chunk = src.read(1024 * 1024)
                            if not chunk:
                                break
                            tmp.write(chunk)
                except Exception as exc:
                    writer.writerow({
                        "archive": str(archive),
                        "path": info.filename,
                        "status": "EXTRACT_ERROR",
                        "error": str(exc),
                    })
                    failed += 1
                    try:
                        tmp_path.unlink(missing_ok=True)
                    except OSError:
                        pass
                    continue

            try:
                result = run_probe(args.command, tmp_path, args.timeout)
            finally:
                try:
                    tmp_path.unlink(missing_ok=True)
                except OSError:
                    pass

            row = {
                "archive": str(archive),
                "path": info.filename,
                **result,
            }
            writer.writerow(row)
            args.output_handle.flush()

            if row["status"] not in ("OK", "NO_GEOMETRY"):
                failed += 1

            if not args.quiet:
                print(
                    f"[{archive.name}] {index}/{len(entries)} "
                    f"{row['status']:<12} {row['duration_ms']} ms  {info.filename}",
                    flush=True,
                )

    return total, failed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "archives",
        nargs="+",
        type=Path,
        help="ZIP archives containing NIF files",
    )
    parser.add_argument(
        "--command",
        required=True,
        help="probe command; must contain the literal token {file}",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=12.0,
        help="hard timeout per NIF in seconds (default: 12)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("nif-fullscan.csv"),
        help="CSV report path",
    )
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    if "{file}" not in args.command:
        parser.error("--command must contain {file}")
    if args.timeout <= 0:
        parser.error("--timeout must be > 0")

    args.output.parent.mkdir(parents=True, exist_ok=True)

    total = 0
    failed = 0
    with args.output.open("w", encoding="utf-8", newline="") as fh:
        args.output_handle = fh
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES)
        writer.writeheader()
        fh.flush()

        for archive in args.archives:
            if not archive.exists():
                writer.writerow({
                    "archive": str(archive),
                    "path": "",
                    "status": "ARCHIVE_MISSING",
                    "error": "archive does not exist",
                })
                failed += 1
                fh.flush()
                continue
            t, f = scan_zip(archive, writer, args)
            total += t
            failed += f

    print(f"Scanned NIFs: {total}", file=sys.stderr)
    print(f"Non-success results: {failed}", file=sys.stderr)
    print(f"Report: {args.output}", file=sys.stderr)
    return 0 if failed == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
