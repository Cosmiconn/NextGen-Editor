#!/usr/bin/env python3
"""Generate original Aurelia landmark NIFs for the 75-125 city map.

The geometry is authored procedurally for NextGen-Editor and is not copied from
Fiesta meshes.  It uses the same verified Gamebryo 20.0.0.4 writer as the
original NextGen fantasy asset pack so the resulting files are real NIF assets
consumed by the normal NIF parser/renderer.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

import generate_original_fantasy_assets as ng

ng.COLORS.update({
    "aurelia_stone": (0.90, 0.89, 0.82),
    "aurelia_stone_shadow": (0.68, 0.69, 0.66),
    "aurelia_blue": (0.055, 0.17, 0.48),
    "aurelia_blue_light": (0.12, 0.38, 0.86),
    "aurelia_gold": (0.92, 0.61, 0.10),
    "aurelia_glass": (0.08, 0.64, 0.98),
    "aurelia_water": (0.03, 0.30, 0.58),
    "aurelia_water_light": (0.10, 0.56, 0.88),
    "aurelia_darkwood": (0.18, 0.085, 0.035),
    "aurelia_green": (0.08, 0.31, 0.13),
})


def crenellations(mesh, length: float, y: float, z: float, *, count: int = 9, depth: float = 95.0) -> None:
    step = length / count
    for i in range(count):
        x = -length / 2 + step * (i + 0.5)
        ng.box(mesh, x, y, z, step * 0.55, depth, 95)


def aurelia_wall_segment():
    p = {}
    ng.box(ng.part(p, "aurelia_stone_shadow"), 0, 0, 28, 620, 150, 56)
    ng.box(ng.part(p, "aurelia_stone"), 0, 0, 220, 600, 130, 360)
    ng.box(ng.part(p, "aurelia_stone_shadow"), 0, 0, 405, 620, 155, 35)
    crenellations(ng.part(p, "aurelia_stone"), 600, -48, 470, count=10, depth=52)
    crenellations(ng.part(p, "aurelia_stone"), 600, 48, 470, count=10, depth=52)
    for x in (-245, -125, 0, 125, 245):
        ng.box(ng.part(p, "aurelia_blue"), x, -68, 265, 22, 12, 130)
        ng.box(ng.part(p, "aurelia_gold"), x, -76, 325, 34, 10, 16)
    return p


def aurelia_watch_tower():
    p = {}
    ng.cylinder(ng.part(p, "aurelia_stone_shadow"), 0, 0, 0, 185, 55, 18)
    ng.cylinder(ng.part(p, "aurelia_stone"), 0, 0, 45, 155, 610, 18, 145)
    ng.cylinder(ng.part(p, "aurelia_stone_shadow"), 0, 0, 615, 190, 80, 18)
    for i in range(12):
        import math
        a = 2 * math.pi * i / 12
        ng.box(ng.part(p, "aurelia_stone"), 165 * math.cos(a), 165 * math.sin(a), 725, 55, 55, 120)
    ng.cylinder(ng.part(p, "aurelia_blue"), 0, 0, 700, 205, 290, 18, 18)
    ng.cylinder(ng.part(p, "aurelia_gold"), 0, 0, 978, 12, 90, 10, 5)
    for z in (230, 410, 560):
        ng.box(ng.part(p, "aurelia_glass"), 0, -158, z, 50, 16, 82)
    return p


def aurelia_main_gate():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    shadow = ng.part(p, "aurelia_stone_shadow")
    blue = ng.part(p, "aurelia_blue")
    gold = ng.part(p, "aurelia_gold")
    for x in (-390, 390):
        ng.box(shadow, x, 0, 35, 310, 270, 70)
        ng.box(stone, x, 0, 360, 270, 230, 650)
        ng.cylinder(blue, x, 0, 660, 175, 300, 16, 20)
        ng.cylinder(gold, x, 0, 948, 12, 75, 10, 5)
    ng.box(stone, 0, 0, 590, 520, 220, 220)
    ng.box(shadow, 0, 0, 710, 560, 245, 40)
    for x in (-220, -110, 0, 110, 220):
        ng.box(stone, x, -82, 790, 70, 75, 120)
    # The opening is represented by the intentional gap between the two towers.
    ng.box(gold, 0, -122, 630, 20, 16, 150)
    ng.box(blue, 0, -130, 610, 150, 12, 190)
    return p


def aurelia_grand_bridge():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    shadow = ng.part(p, "aurelia_stone_shadow")
    gold = ng.part(p, "aurelia_gold")
    # Local Y is bridge direction.
    ng.box(shadow, 0, 0, 80, 520, 1350, 100)
    ng.box(stone, 0, 0, 150, 470, 1320, 75)
    for y in (-520, -175, 175, 520):
        ng.box(stone, -190, y, -45, 95, 150, 310)
        ng.box(stone, 190, y, -45, 95, 150, 310)
    for side in (-1, 1):
        x = side * 232
        ng.box(stone, x, 0, 235, 35, 1320, 105)
        for y in range(-600, 601, 150):
            ng.box(gold, x, y, 310, 22, 22, 125)
    return p


def aurelia_terrace():
    p = {}
    ng.box(ng.part(p, "aurelia_stone_shadow"), 0, 0, 50, 1800, 1300, 100)
    ng.box(ng.part(p, "aurelia_stone"), 0, 0, 125, 1730, 1230, 55)
    for side in (-1, 1):
        x = side * 845
        ng.box(ng.part(p, "aurelia_stone"), x, 0, 220, 45, 1180, 185)
        for y in range(-500, 501, 200):
            ng.box(ng.part(p, "aurelia_gold"), x - side * 25, y, 315, 16, 16, 70)
    return p


def aurelia_grand_stairs():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    shadow = ng.part(p, "aurelia_stone_shadow")
    steps = 18
    for i in range(steps):
        # Ascends toward +Y.
        y = -720 + i * 82
        z = 22 + i * 24
        ng.box(stone if i % 2 == 0 else shadow, 0, y, z, 760, 95, 45)
    for x in (-405, 405):
        ng.box(stone, x, 0, 260, 70, 1500, 510)
        for y in range(-650, 651, 220):
            ng.box(ng.part(p, "aurelia_gold"), x, y, 545, 18, 18, 110)
    return p


def aurelia_grand_fountain():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    water = ng.part(p, "aurelia_water_light")
    gold = ng.part(p, "aurelia_gold")
    ng.cylinder(stone, 0, 0, 0, 430, 80, 28)
    ng.cylinder(water, 0, 0, 72, 370, 20, 28)
    ng.cylinder(stone, 0, 0, 80, 205, 95, 24)
    ng.cylinder(water, 0, 0, 165, 165, 18, 24)
    ng.cylinder(stone, 0, 0, 175, 48, 310, 16, 36)
    ng.sphere(gold, 0, 0, 520, 72, 12, 6)
    for x, y in ((250, 0), (-250, 0), (0, 250), (0, -250)):
        ng.cylinder(stone, x, y, 78, 32, 115, 10, 22)
        ng.sphere(gold, x, y, 210, 30, 8, 4)
    return p


def aurelia_cathedral():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    shadow = ng.part(p, "aurelia_stone_shadow")
    blue = ng.part(p, "aurelia_blue")
    blue2 = ng.part(p, "aurelia_blue_light")
    glass = ng.part(p, "aurelia_glass")
    gold = ng.part(p, "aurelia_gold")
    # Nave and transept.
    ng.box(shadow, 0, 0, 55, 1500, 1050, 110)
    ng.box(stone, 0, 40, 470, 1180, 900, 760)
    ng.roof(blue, 0, 60, 835, 1260, 960, 350)
    ng.box(stone, 0, -505, 520, 820, 120, 930)
    ng.box(stone, 0, 110, 510, 1500, 430, 700)
    ng.roof(blue2, 0, 110, 835, 1580, 500, 260)
    # Front tower pair and rear tower pair.
    for x, y in ((-520, -430), (520, -430), (-560, 365), (560, 365)):
        ng.cylinder(stone, x, y, 80, 155, 900, 16, 145)
        ng.cylinder(blue, x, y, 945, 185, 360, 16, 18)
        ng.cylinder(gold, x, y, 1292, 10, 95, 10, 4)
    # Central crossing tower.
    ng.cylinder(stone, 0, 90, 770, 250, 360, 16, 215)
    ng.cylinder(blue, 0, 90, 1095, 265, 430, 18, 18)
    ng.cylinder(gold, 0, 90, 1510, 14, 120, 10, 5)
    # Facade glass and gold tracery.
    ng.box(glass, 0, -571, 670, 250, 18, 310)
    ng.sphere(glass, 0, -584, 860, 125, 14, 7)
    for x in (-270, -135, 135, 270):
        ng.box(glass, x, -566, 470, 80, 16, 220)
        ng.box(gold, x, -580, 475, 15, 12, 235)
    ng.box(gold, 0, -585, 1020, 410, 12, 26)
    # Front staircase and portal frame.
    for i in range(12):
        ng.box(stone, 0, -665 - i * 35, 18 + i * 13, 640 + i * 18, 48, 35)
    ng.box(gold, 0, -575, 325, 265, 18, 28)
    # Flying-buttress-like supports.
    for x in (-690, 690):
        for y in (-250, 60, 360):
            ng.beam_xz(shadow, x * 0.72, 400, x, 120, y, 80, 70)
            ng.box(stone, x, y, 210, 75, 90, 420)
    return p


def aurelia_harbor_pier():
    p = {}
    wood = ng.part(p, "aurelia_darkwood")
    light = ng.part(p, "wood_light")
    ng.box(wood, 0, 0, 55, 420, 1500, 70)
    for x in (-175, 175):
        for y in range(-650, 651, 260):
            ng.box(wood, x, y, -90, 42, 42, 340)
            ng.box(light, x, y, 145, 35, 35, 140)
    for side in (-1, 1):
        ng.box(light, side * 195, 0, 115, 28, 1450, 35)
    return p


def aurelia_lighthouse():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    shadow = ng.part(p, "aurelia_stone_shadow")
    blue = ng.part(p, "aurelia_blue")
    glass = ng.part(p, "aurelia_glass")
    gold = ng.part(p, "aurelia_gold")
    ng.cylinder(shadow, 0, 0, 0, 260, 90, 20)
    ng.cylinder(stone, 0, 0, 75, 175, 780, 20, 125)
    ng.cylinder(shadow, 0, 0, 830, 210, 70, 20)
    ng.cylinder(glass, 0, 0, 900, 155, 180, 16)
    ng.cylinder(blue, 0, 0, 1060, 210, 250, 16, 20)
    ng.cylinder(gold, 0, 0, 1298, 10, 90, 10, 4)
    return p


def aurelia_water_plane():
    p = {}
    ng.box(ng.part(p, "aurelia_water"), 0, 0, -25, 6200, 3600, 50)
    return p


def aurelia_colonnade():
    p = {}
    stone = ng.part(p, "aurelia_stone")
    blue = ng.part(p, "aurelia_blue")
    gold = ng.part(p, "aurelia_gold")
    ng.box(stone, 0, 0, 40, 1200, 320, 80)
    for x in range(-500, 501, 200):
        ng.cylinder(stone, x, 0, 80, 42, 410, 12, 34)
        ng.box(gold, x, 0, 515, 75, 75, 28)
    ng.box(stone, 0, 0, 555, 1180, 280, 80)
    ng.roof(blue, 0, 0, 600, 1240, 360, 135)
    return p


AURELIA_ASSETS = {
    "NG_Aurelia_WallSegment": aurelia_wall_segment,
    "NG_Aurelia_WatchTower": aurelia_watch_tower,
    "NG_Aurelia_MainGate": aurelia_main_gate,
    "NG_Aurelia_GrandBridge": aurelia_grand_bridge,
    "NG_Aurelia_Terrace": aurelia_terrace,
    "NG_Aurelia_GrandStairs": aurelia_grand_stairs,
    "NG_Aurelia_GrandFountain": aurelia_grand_fountain,
    "NG_Aurelia_Cathedral": aurelia_cathedral,
    "NG_Aurelia_HarborPier": aurelia_harbor_pier,
    "NG_Aurelia_Lighthouse": aurelia_lighthouse,
    "NG_Aurelia_WaterPlane": aurelia_water_plane,
    "NG_Aurelia_Colonnade": aurelia_colonnade,
}

ALL_ASSETS = dict(ng.ASSETS)
ALL_ASSETS.update(AURELIA_ASSETS)


def generate(output: Path) -> list[dict[str, object]]:
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, builder in ALL_ASSETS.items():
        entry = ng.write_nif(output / f"{name}.nif", name, builder())
        ng.validate_header(output / entry["file"])
        manifest.append(entry)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / "README.txt").write_text(
        "Aurelia75 original NextGen NIF assets\n"
        "Gamebryo 20.0.0.4, static NiTriShape, Z-up, material-color baseline, no external textures.\n"
        "The landmark geometry is original and was authored specifically for Aurelia75.\n",
        encoding="utf-8",
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("build/aurelia-nifs"))
    args = parser.parse_args()
    manifest = generate(args.output)
    print(f"Generated {len(manifest)} Aurelia-compatible NIFs in {args.output}")
    for entry in manifest:
        print(f"{entry['file']}: {entry['parts']} parts, {entry['triangles']} triangles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
