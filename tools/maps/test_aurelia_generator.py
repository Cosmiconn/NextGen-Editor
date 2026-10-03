#!/usr/bin/env python3
"""Fast CI/self-test for the Aurelia city generator; no Fiesta client required."""

from __future__ import annotations

import struct
import tempfile
from pathlib import Path

import generate_aurelia_75_125 as aurelia


def main() -> int:
    placements = aurelia.build_city()
    assert len(placements) == 96, f"unexpected placement count: {len(placements)}"
    assert len(aurelia.DISTRICTS) == 5
    assert len(aurelia.PORTAL_ANCHORS) == 4
    assert len(aurelia.NPC_ANCHORS) >= 8

    for p in placements:
        assert 0.0 <= p.x <= aurelia.WORLD_SIZE, p
        assert 0.0 <= p.y <= aurelia.WORLD_SIZE, p
        assert p.scale > 0.0, p
        assert p.model.lower().endswith(".nif"), p

    refs = set(aurelia.all_model_refs(placements))
    assert aurelia.SKY in refs
    assert aurelia.GUILD_HALL in refs
    assert aurelia.SMITH in refs
    assert aurelia.WATCH_TOWER in refs
    assert all(shop in refs for shop in aurelia.SHOPS)

    # Synthetic Rou-compatible walk grid: 256 header quads, 2048 rows,
    # 128 uint16 words/row => 2048 cells across => 6.25 world units/cell.
    rows = 2048
    words_per_row = 128
    raw = struct.pack("<II", 256, rows) + bytes(rows * words_per_row * 2)
    grid = aurelia.WalkGridTemplate(raw)
    assert grid.rows == rows
    assert grid.words_per_row == words_per_row
    assert grid.cols == 2048
    assert abs(grid.cell_size - 6.25) < 1e-9

    grid.set_world_rect(100.0, 100.0, 125.0, 125.0, True)
    cx = int(112.5 / grid.cell_size)
    cy = int(112.5 / grid.cell_size)
    index = cy * grid.words_per_row + cx // 16
    assert grid.words[index] & (1 << (cx % 16))
    grid.set_world_rect(100.0, 100.0, 125.0, 125.0, False)
    assert not (grid.words[index] & (1 << (cx % 16)))
    assert len(grid.to_bytes()) == len(raw)

    with tempfile.TemporaryDirectory(prefix="aurelia-selftest-") as temp:
        root = Path(temp)
        shmd = root / "Aurelia75.shmd"
        ini = root / "Aurelia75.ini"
        aurelia.write_shmd(shmd, placements)
        aurelia.write_ini(ini)
        shmd_text = shmd.read_text(encoding="utf-8")
        ini_text = ini.read_text(encoding="utf-8")
        assert shmd_text.startswith("shmd0_5")
        assert "GroundObject 0" in shmd_text
        assert "DataObjectLoadingEnd" in shmd_text
        assert aurelia.GUILD_HALL in shmd_text
        assert "#HEIGHTMAP_WIDTH : 257" in ini_text
        assert f"resmap\\field\\{aurelia.MAP_ID}\\{aurelia.MAP_ID}.HTD" in ini_text

    print(
        f"AURELIA SELF-TEST OK: {len(placements)} placements, "
        f"{len(refs)} unique NIF refs, {grid.cols}x{grid.rows} walk cells"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
