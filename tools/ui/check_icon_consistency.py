#!/usr/bin/env python3
"""Validate semantic UI icon mappings and the checked-in runtime PNG subset.

This deliberately does NOT require every approved icon size to be committed.
The complete source package lives in NextGen_Icons_Final.zip and is imported via
tools/ui/import_icon_pack.py. The current branch may contain only the runtime
subset needed by migrated UI surfaces.

The check guarantees that:
- icon-map.json and UiIconAssets.cpp expose the same 68 semantic IDs;
- every semantic ID points to the same approved group/file in both places;
- the runtime size set in C++ stays aligned with the approved 16/24/32/48/64/128 set;
- every checked-in runtime PNG belongs to a known semantic icon at a supported size.
"""

from __future__ import annotations

import json
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

EXPECTED_ICON_COUNT = 68
EXPECTED_SIZES = (16, 24, 32, 48, 64, 128)
GROUP_DIR = {
    "toolbar": "01_toolbar_icons",
    "navigation": "02_navigation_icons",
    "panel": "03_panel_icons",
    "extra": "04_extra_icons",
    "additional_ui": "05_additional_ui_icons",
}

ICON_ENTRY_RE = re.compile(
    r'IconPathEntry\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}'
)
SUPPORTED_SIZES_RE = re.compile(
    r"constexpr\s+std::array\s+kSupportedSizes\s*\{([^}]*)\};",
    re.MULTILINE,
)


def fail(errors: list[str]) -> int:
    print("UI icon consistency check FAILED:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    return 1


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    icon_root = repo / "assets" / "ui" / "icons"
    map_path = icon_root / "icon-map.json"
    cpp_path = repo / "src" / "app" / "UiIconAssets.cpp"
    png_root = icon_root / "png"

    errors: list[str] = []

    mapping_doc = json.loads(map_path.read_text(encoding="utf-8"))
    semantic_ids = mapping_doc.get("semantic_ids")
    if not isinstance(semantic_ids, dict):
        return fail(["icon-map.json: semantic_ids must be an object"])

    if len(semantic_ids) != EXPECTED_ICON_COUNT:
        errors.append(
            f"icon-map.json: expected {EXPECTED_ICON_COUNT} semantic IDs, got {len(semantic_ids)}"
        )

    values = list(semantic_ids.values())
    duplicate_values = sorted(value for value, count in Counter(values).items() if count > 1)
    if duplicate_values:
        errors.append(
            "icon-map.json: duplicate package mappings: " + ", ".join(duplicate_values)
        )

    expected_by_semantic: dict[str, tuple[str, str]] = {}
    expected_runtime_paths: dict[tuple[str, str], str] = {}
    for semantic, package_ref in semantic_ids.items():
        if not isinstance(semantic, str) or not isinstance(package_ref, str):
            errors.append("icon-map.json: semantic IDs and package mappings must be strings")
            continue
        try:
            group, stem = package_ref.split("/", 1)
        except ValueError:
            errors.append(f"{semantic}: invalid package mapping {package_ref!r}")
            continue
        group_dir = GROUP_DIR.get(group)
        if group_dir is None:
            errors.append(f"{semantic}: unknown package group {group!r}")
            continue
        file_name = f"{stem}.png"
        expected_by_semantic[semantic] = (group_dir, file_name)
        expected_runtime_paths[(group_dir, file_name)] = semantic

    cpp = cpp_path.read_text(encoding="utf-8")
    cpp_entries = ICON_ENTRY_RE.findall(cpp)
    if len(cpp_entries) != EXPECTED_ICON_COUNT:
        errors.append(
            f"UiIconAssets.cpp: expected {EXPECTED_ICON_COUNT} IconPathEntry rows, got {len(cpp_entries)}"
        )

    cpp_by_semantic: dict[str, tuple[str, str]] = {}
    for semantic, group_dir, file_name in cpp_entries:
        if semantic in cpp_by_semantic:
            errors.append(f"UiIconAssets.cpp: duplicate semantic ID {semantic!r}")
            continue
        cpp_by_semantic[semantic] = (group_dir, file_name)

    map_keys = set(expected_by_semantic)
    cpp_keys = set(cpp_by_semantic)
    for semantic in sorted(map_keys - cpp_keys):
        errors.append(f"UiIconAssets.cpp: missing semantic ID {semantic!r}")
    for semantic in sorted(cpp_keys - map_keys):
        errors.append(f"UiIconAssets.cpp: unknown semantic ID {semantic!r}")

    for semantic in sorted(map_keys & cpp_keys):
        expected = expected_by_semantic[semantic]
        actual = cpp_by_semantic[semantic]
        if actual != expected:
            errors.append(
                f"{semantic}: icon-map expects {expected[0]}/{expected[1]}, "
                f"UiIconAssets.cpp uses {actual[0]}/{actual[1]}"
            )

    size_match = SUPPORTED_SIZES_RE.search(cpp)
    if not size_match:
        errors.append("UiIconAssets.cpp: kSupportedSizes declaration not found")
        cpp_sizes: tuple[int, ...] = ()
    else:
        cpp_sizes = tuple(int(value) for value in re.findall(r"\d+", size_match.group(1)))
        if cpp_sizes != EXPECTED_SIZES:
            errors.append(
                f"UiIconAssets.cpp: supported sizes {cpp_sizes} do not match approved {EXPECTED_SIZES}"
            )

    runtime_by_semantic: dict[str, set[int]] = defaultdict(set)
    runtime_files = 0
    if not png_root.is_dir():
        errors.append(f"runtime PNG root is missing: {png_root.relative_to(repo)}")
    else:
        for path in sorted(png_root.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(png_root)
            if path.suffix.lower() != ".png":
                errors.append(f"runtime icon tree contains non-PNG file: {rel.as_posix()}")
                continue
            parts = rel.parts
            if len(parts) != 3:
                errors.append(f"unexpected runtime icon path shape: {rel.as_posix()}")
                continue
            size_text, group_dir, file_name = parts
            try:
                size = int(size_text)
            except ValueError:
                errors.append(f"runtime icon uses non-numeric size directory: {rel.as_posix()}")
                continue
            if size not in EXPECTED_SIZES:
                errors.append(f"runtime icon uses unsupported size {size}: {rel.as_posix()}")
            semantic = expected_runtime_paths.get((group_dir, file_name))
            if semantic is None:
                errors.append(f"orphan runtime PNG not present in icon-map.json: {rel.as_posix()}")
                continue
            runtime_by_semantic[semantic].add(size)
            runtime_files += 1

    if errors:
        return fail(errors)

    semantics_with_runtime = len(runtime_by_semantic)
    missing_runtime = sorted(set(expected_by_semantic) - set(runtime_by_semantic))
    size_counts = Counter(
        size for sizes in runtime_by_semantic.values() for size in sizes
    )

    print(
        f"UI icon consistency OK: {len(expected_by_semantic)} semantic IDs, "
        f"{runtime_files} committed runtime PNGs, "
        f"{semantics_with_runtime}/{len(expected_by_semantic)} semantics have a runtime raster."
    )
    print(
        "Committed runtime sizes: "
        + ", ".join(f"{size}px={size_counts.get(size, 0)}" for size in EXPECTED_SIZES)
    )
    if missing_runtime:
        print(
            f"Semantic IDs without a committed runtime raster ({len(missing_runtime)}): "
            + ", ".join(missing_runtime)
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
