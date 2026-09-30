#!/usr/bin/env python3
"""Capture independent NifSkope reference views for a visual-matrix manifest.

This helper deliberately does not interpret NIF renderer semantics. It launches the
pinned/reference NifSkope GUI under an existing X11 display, asks NifSkope to center
the loaded model, and captures the resulting NifSkope window as PNG evidence.

Run it under Xvfb for deterministic headless automation. Required external tools:
  - xdotool
  - ImageMagick's import command
"""

from __future__ import annotations

import argparse
import csv
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time


def require_tool(name: str) -> str:
    resolved = shutil.which(name)
    if not resolved:
        raise RuntimeError(f"Required tool not found on PATH: {name}")
    return resolved


def parse_geometry(text: str) -> tuple[int, int]:
    values: dict[str, int] = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key in {"WIDTH", "HEIGHT"}:
            try:
                values[key] = int(value)
            except ValueError:
                pass
    return values.get("WIDTH", 0), values.get("HEIGHT", 0)


def visible_windows_for_pid(xdotool: str, pid: int) -> list[int]:
    result = subprocess.run(
        [xdotool, "search", "--onlyvisible", "--pid", str(pid)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    windows: list[int] = []
    for token in result.stdout.split():
        try:
            windows.append(int(token, 0))
        except ValueError:
            continue
    return windows


def largest_window(xdotool: str, windows: list[int]) -> tuple[int, int, int] | None:
    best: tuple[int, int, int] | None = None
    best_area = 0
    for window in windows:
        result = subprocess.run(
            [xdotool, "getwindowgeometry", "--shell", str(window)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if result.returncode != 0:
            continue
        width, height = parse_geometry(result.stdout)
        area = width * height
        if area > best_area:
            best_area = area
            best = (window, width, height)
    return best


def wait_for_main_window(
    xdotool: str,
    process: subprocess.Popen[bytes],
    timeout: float,
) -> tuple[int, int, int]:
    deadline = time.monotonic() + timeout
    last_windows: list[int] = []
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                f"NifSkope exited before a visible window appeared (exit {process.returncode})."
            )
        last_windows = visible_windows_for_pid(xdotool, process.pid)
        best = largest_window(xdotool, last_windows)
        if best is not None and best[1] >= 320 and best[2] >= 240:
            return best
        time.sleep(0.20)
    raise RuntimeError(
        f"Timed out waiting for NifSkope window for PID {process.pid}; "
        f"visible windows={last_windows}"
    )


def stop_process(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def capture_one(
    *,
    nifskope: Path,
    xdotool: str,
    import_tool: str,
    nif: Path,
    output_png: Path,
    timeout: float,
    settle: float,
    config_root: Path,
    window_width: int,
    window_height: int,
) -> tuple[int, int]:
    env = os.environ.copy()
    env.setdefault("LIBGL_ALWAYS_SOFTWARE", "1")
    env.setdefault("QT_X11_NO_MITSHM", "1")
    env["XDG_CONFIG_HOME"] = str(config_root)

    process = subprocess.Popen(
        [str(nifskope), str(nif)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env=env,
    )
    try:
        window, _, _ = wait_for_main_window(xdotool, process, timeout)

        # Xvfb has no window manager by default, so address the Qt top-level window
        # directly instead of depending on focus/activation semantics.
        subprocess.run(
            [xdotool, "windowsize", str(window), str(window_width), str(window_height)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        subprocess.run(
            [xdotool, "windowmove", str(window), "0", "0"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )

        time.sleep(settle)

        # NifSkope's documented Z action centers the viewport on the loaded object.
        subprocess.run(
            [xdotool, "key", "--window", str(window), "z"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
        time.sleep(settle)

        output_png.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            [import_tool, "-window", str(window), str(output_png)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(
                "ImageMagick import failed for NifSkope window "
                f"{window}: {result.stderr.decode(errors='replace').strip()}"
            )

        data = output_png.read_bytes()
        if len(data) < 1024 or not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError(
                f"Reference capture is not a valid non-trivial PNG: {output_png} "
                f"({len(data)} bytes)"
            )

        geometry = subprocess.run(
            [xdotool, "getwindowgeometry", "--shell", str(window)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        width, height = parse_geometry(geometry.stdout)
        return width, height
    finally:
        stop_process(process)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Capture NifSkope GUI reference PNGs for a ResMap visual matrix."
    )
    parser.add_argument("--nifskope", required=True, type=Path)
    parser.add_argument("--matrix", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--settle", type=float, default=1.5)
    parser.add_argument("--window-width", type=int, default=1200)
    parser.add_argument("--window-height", type=int, default=900)
    args = parser.parse_args()

    if not os.environ.get("DISPLAY"):
        raise RuntimeError("DISPLAY is not set; run this helper under Xvfb/X11.")
    if not args.nifskope.is_file():
        raise RuntimeError(f"NifSkope executable/wrapper not found: {args.nifskope}")
    if not args.matrix.is_file():
        raise RuntimeError(f"Matrix manifest not found: {args.matrix}")

    xdotool = require_tool("xdotool")
    import_tool = require_tool("import")

    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    config_root = output / "_nifskope_config"
    config_root.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, str]] = []
    with args.matrix.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {"category", "root", "path"}
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise RuntimeError(
                f"Matrix must contain columns {sorted(required)}; got {reader.fieldnames}"
            )
        rows = [row for row in reader if row.get("category") and row.get("path")]

    if not rows:
        raise RuntimeError("Matrix contains no reference rows.")

    manifest_path = output / "nifskope_reference.tsv"
    with manifest_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Category", "Path", "ReferencePng", "Width", "Height"],
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()

        for index, row in enumerate(rows, 1):
            category = row["category"].strip()
            root = Path(row["root"]).expanduser()
            relative = Path(row["path"])
            nif = relative if relative.is_absolute() else root / relative
            nif = nif.resolve()
            if not nif.is_file():
                raise RuntimeError(f"[{category}] selected NIF does not exist: {nif}")

            safe_category = "".join(
                ch if ch.isalnum() or ch in "._-" else "_" for ch in category
            )
            output_png = output / f"{safe_category}.png"
            print(
                f"[{index}/{len(rows)}] NifSkope reference: {category} -> {nif}",
                flush=True,
            )
            width, height = capture_one(
                nifskope=args.nifskope.resolve(),
                xdotool=xdotool,
                import_tool=import_tool,
                nif=nif,
                output_png=output_png,
                timeout=args.timeout,
                settle=args.settle,
                config_root=config_root,
                window_width=args.window_width,
                window_height=args.window_height,
            )
            writer.writerow(
                {
                    "Category": category,
                    "Path": str(nif),
                    "ReferencePng": output_png.name,
                    "Width": width,
                    "Height": height,
                }
            )
            handle.flush()

    print(f"NifSkope reference captures written to: {output}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
