#!/usr/bin/env python3
"""Generate the Aurelia level 75-125 city as a non-destructive project override.

The generator deliberately reuses the verified Rou terrain/walk-grid as a *terrain template*
but creates a new SHMD object layout, city collision pass and design manifest. It never
writes into the source client. Source NIFs remain references and are validated before output.

Typical Windows usage:
    py tools/maps/generate_aurelia_75_125.py \
        --source-client "D:\\Fiesta\\Client" \
        --project-root "D:\\NextGenProjects\\Aurelia"

The generated map can then be opened from:
    <project>/Client/resmap/field/Aurelia75/Aurelia75.ini
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import struct
from collections import OrderedDict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

MAP_ID = "Aurelia75"
MAP_TITLE = "Aurelia - Bastion der Morgenwacht"
LEVEL_MIN = 75
LEVEL_MAX = 125
TEMPLATE_MAP = "Rou"

HEIGHTMAP_POINTS = 257
BLOCK_SIZE = 50.0
WORLD_SIZE = (HEIGHTMAP_POINTS - 1) * BLOCK_SIZE
EXPECTED_WALK_CELL_SIZE = 6.25

SKY = r"resmap\nifs\Common\field_sky_01.nif"
SEA = r"resmap\field\Rou\sea.nif"
WOOL = r"resmap\field\Rou\wool5.nif"
GUILD_HALL = r"resmap\field\Rou\GuildHall.nif"
ITEM_SHOP = r"resmap\field\Rou\ItemShop02.nif"
MARKET = r"resmap\field\Rou\rou_market.nif"
SMITH = r"resmap\field\Rou\smithsmith.nif"
WATCH_TOWER = r"resmap\field\Rou\rou_watchTower.nif"
LIGHTHOUSE = r"resmap\field\Rou\rou_lighthouse.nif"
BRIDGE = r"resmap\field\Rou\rou_Bridge01.nif"
WATER_HALL = r"resmap\field\Rou\waterhall.nif"
WATER_WELL = r"resmap\field\Rou\rou_waterwell.nif"
BANNER = r"resmap\field\Rou\Rou_M_Banner.nif"
BANNER_A = r"resmap\field\Rou\Rou_M_BannerA.nif"
BANNER_B = r"resmap\field\Rou\Rou_M_BannerB.nif"
BANNER_D = r"resmap\field\Rou\Rou_M_BannerD.nif"
FISHS = r"resmap\field\Rou\Rou_M_Fishs.nif"
TUBE = r"resmap\field\Rou\Rou_M_Tube.nif"
STORE = r"resmap\field\Rou\store.nif"
TICKER = r"resmap\field\Rou\ticker.nif"
TREE05 = r"resmap\field\Rou\tree05.nif"
TREE06 = r"resmap\field\Rou\tree06.nif"
WAGON = r"resmap\field\Rou\wagon2.nif"
WOODS = r"resmap\field\Rou\woods.nif"
SHOPS = [rf"resmap\field\Rou\Rou_M_Shop{i:02d}.nif" for i in range(8)]


@dataclass(frozen=True)
class Placement:
    model: str
    x: float
    y: float  # horizontal Fiesta/legacy Y coordinate
    elevation: float
    yaw_deg: float = 0.0
    scale: float = 1.0
    collision_x: float = 0.0
    collision_y: float = 0.0
    district: str = ""
    note: str = ""


def add(
    out: list[Placement],
    model: str,
    x: float,
    y: float,
    *,
    elevation: float = 483.609,
    yaw: float = 0.0,
    scale: float = 1.0,
    collision: tuple[float, float] | None = None,
    district: str = "",
    note: str = "",
) -> None:
    cx, cy = collision or (0.0, 0.0)
    out.append(Placement(model, x, y, elevation, yaw, scale, cx, cy, district, note))


def build_city() -> list[Placement]:
    """Return the deterministic city layout.

    Coordinates intentionally stay on the broad Rou city plateau (roughly X 8.1k-10.6k,
    legacy-Y 1.0k-4.3k), whose elevations are represented by the verified reference map.
    """
    p: list[Placement] = []

    # --- Central citadel / safe arrival hub (level 75 entry) ---
    add(p, GUILD_HALL, 9300, 2780, yaw=180, collision=(165, 135), district="citadel",
        note="Main guild/quest hall")
    add(p, WATER_WELL, 9300, 2525, collision=(38, 38), district="citadel",
        note="Central landmark")
    add(p, MARKET, 9300, 2225, collision=(135, 105), district="market",
        note="Covered central market")
    for x, y, yaw in [
        (9140, 2575, 0), (9460, 2575, 180), (9140, 3000, 0), (9460, 3000, 180),
    ]:
        add(p, BANNER_A, x, y, yaw=yaw, district="citadel")
    add(p, TICKER, 9225, 2460, district="citadel")
    add(p, TICKER, 9375, 2460, yaw=180, district="citadel")

    # --- Merchant ring (levels 75-95 services) ---
    shop_slots = [
        (SHOPS[0], 8620, 2420, 45),
        (SHOPS[1], 8790, 2180, 20),
        (SHOPS[2], 9040, 2030, 0),
        (SHOPS[3], 9560, 2030, 180),
        (SHOPS[4], 9810, 2180, 200),
        (SHOPS[5], 9980, 2420, 225),
        (SHOPS[6], 9980, 3090, 135),
        (SHOPS[7], 8620, 3090, 45),
    ]
    for model, x, y, yaw in shop_slots:
        add(p, model, x, y, yaw=yaw, collision=(72, 58), district="market")
    add(p, ITEM_SHOP, 8800, 3290, yaw=90, collision=(95, 80), district="market",
        note="High-tier item merchant building")
    add(p, ITEM_SHOP, 9800, 3290, yaw=-90, collision=(95, 80), district="market")
    add(p, STORE, 8460, 2740, yaw=90, scale=0.72, collision=(52, 42), district="market")
    add(p, STORE, 10140, 2740, yaw=-90, scale=0.72, collision=(52, 42), district="market")
    add(p, WAGON, 8850, 2380, yaw=25, collision=(38, 24), district="market")
    add(p, WAGON, 9750, 2380, yaw=155, collision=(38, 24), district="market")

    # --- Forge / crafting quarter (levels 95-105) ---
    add(p, SMITH, 9300, 3550, yaw=180, collision=(130, 105), district="forge",
        note="Blacksmith/crafting landmark")
    add(p, STORE, 9010, 3580, yaw=90, scale=0.80, collision=(58, 45), district="forge")
    add(p, STORE, 9590, 3580, yaw=-90, scale=0.80, collision=(58, 45), district="forge")
    add(p, WAGON, 9075, 3360, yaw=30, collision=(38, 24), district="forge")
    add(p, WAGON, 9525, 3360, yaw=150, collision=(38, 24), district="forge")
    for x in (8890, 9010, 9130, 9470, 9590, 9710):
        add(p, TUBE, x, 3820, yaw=90, district="forge")
    add(p, BANNER_D, 9090, 3310, yaw=0, district="forge")
    add(p, BANNER_D, 9510, 3310, yaw=180, district="forge")

    # --- South harbor / expedition quarter (levels 85-100) ---
    add(p, WATER_HALL, 9300, 1580, elevation=481.6, yaw=0, collision=(125, 105), district="harbor")
    add(p, LIGHTHOUSE, 10070, 1375, elevation=481.5, yaw=0, collision=(82, 82), district="harbor")
    add(p, BRIDGE, 9300, 1230, elevation=481.4, yaw=0, district="harbor",
        note="Walkable visual bridge - no rectangular collision stamp")
    for x, y, yaw in [(9000, 1740, 0), (9180, 1780, 20), (9420, 1780, 160), (9600, 1740, 180)]:
        add(p, FISHS, x, y, elevation=481.8, yaw=yaw, district="harbor")
    add(p, WAGON, 8740, 1690, elevation=481.8, yaw=20, collision=(38, 24), district="harbor")
    add(p, STORE, 8500, 1660, elevation=481.8, yaw=75, scale=0.65, collision=(50, 40), district="harbor")
    add(p, BANNER_B, 9000, 1460, elevation=481.6, yaw=0, district="harbor")
    add(p, BANNER_B, 9600, 1460, elevation=481.6, yaw=180, district="harbor")

    # --- East veteran bastion (levels 105-115) ---
    add(p, GUILD_HALL, 10020, 2760, yaw=-90, scale=0.92, collision=(150, 125), district="bastion",
        note="Veteran order hall")
    for x, y, yaw in [
        (10370, 2290, 90), (10370, 3230, 90), (10190, 2020, 45), (10190, 3500, 135),
    ]:
        add(p, WATCH_TOWER, x, y, yaw=yaw, collision=(58, 58), district="bastion")
    add(p, BANNER, 10150, 2520, yaw=-90, scale=1.30, district="bastion")
    add(p, BANNER, 10150, 3000, yaw=-90, scale=1.30, district="bastion")

    # --- Four progression gates: 75 / 90 / 105 / 120+ ---
    gate_towers = [
        (8210, 2520, 0, "west_gate"), (8210, 2980, 0, "west_gate"),
        (9000, 1080, 90, "south_gate"), (9600, 1080, 90, "south_gate"),
        (10520, 2520, 180, "east_gate"), (10520, 2980, 180, "east_gate"),
        (9000, 4210, -90, "north_gate"), (9600, 4210, -90, "north_gate"),
    ]
    for x, y, yaw, district in gate_towers:
        elev = 481.4 if district == "south_gate" else 483.609
        add(p, WATCH_TOWER, x, y, elevation=elev, yaw=yaw, collision=(58, 58), district=district)
    for model, x, y, yaw, district in [
        (BANNER_A, 8300, 2750, 90, "west_gate"),
        (BANNER_B, 9300, 1160, 0, "south_gate"),
        (BANNER_D, 10430, 2750, -90, "east_gate"),
        (BANNER, 9300, 4100, 180, "north_gate"),
    ]:
        add(p, model, x, y, elevation=481.5 if district == "south_gate" else 483.609,
            yaw=yaw, scale=1.25, district=district)

    # --- Parks and green corridors: fixed, deterministic positions ---
    tree_slots = [
        (8420, 2050), (8520, 1900), (8700, 1840), (9900, 1880), (10050, 1950),
        (10200, 2150), (8400, 3370), (8540, 3510), (8700, 3670), (9900, 3680),
        (10070, 3520), (10200, 3370), (8750, 3930), (8910, 3970), (9690, 3970),
        (9850, 3930), (8460, 2320), (8460, 3160), (10140, 2320), (10140, 3160),
        (8960, 2680), (9640, 2680), (8960, 2890), (9640, 2890),
    ]
    for i, (x, y) in enumerate(tree_slots):
        model = TREE05 if i % 3 else TREE06
        scale = 0.78 + (i % 5) * 0.07
        yaw = (i * 47) % 360
        add(p, model, x, y, yaw=yaw, scale=scale, collision=(16, 16), district="greenbelt")

    for i, (x, y) in enumerate([(8350, 1870), (10300, 1880), (8340, 3650), (10280, 3680),
                                  (8680, 4080), (9920, 4070)]):
        add(p, WOODS, x, y, yaw=(i * 63) % 360, scale=0.82 + 0.05 * (i % 3),
            collision=(32, 32), district="greenbelt")

    return p


DISTRICTS = [
    {
        "id": "west_arrival",
        "name": "Ankunftsviertel",
        "recommended_levels": [75, 85],
        "bounds": [8150, 2350, 8850, 3200],
        "purpose": "Entry hub, early quests and travel services",
    },
    {
        "id": "market",
        "name": "Kronmarkt",
        "recommended_levels": [85, 95],
        "bounds": [8500, 1950, 10050, 3350],
        "purpose": "Trading, consumables, storage and social hub",
    },
    {
        "id": "forge",
        "name": "Runenwerk",
        "recommended_levels": [95, 105],
        "bounds": [8800, 3250, 9800, 3950],
        "purpose": "Blacksmith, crafting and upgrade services",
    },
    {
        "id": "bastion",
        "name": "Veteranenbastion",
        "recommended_levels": [105, 115],
        "bounds": [9900, 1950, 10600, 3550],
        "purpose": "Veteran order, high-level quest handoff and east gate",
    },
    {
        "id": "north_citadel",
        "name": "Morgenwacht",
        "recommended_levels": [115, 125],
        "bounds": [8750, 3300, 9900, 4300],
        "purpose": "End-bracket services and level-120+ expedition gate",
    },
]

PORTAL_ANCHORS = [
    {"id": "west_75", "position": [8125, 2750, 483.609], "recommended_min_level": 75, "target_map": None},
    {"id": "south_90", "position": [9300, 1010, 481.4], "recommended_min_level": 90, "target_map": None},
    {"id": "east_105", "position": [10610, 2750, 483.609], "recommended_min_level": 105, "target_map": None},
    {"id": "north_120", "position": [9300, 4310, 483.609], "recommended_min_level": 120, "target_map": None},
]

NPC_ANCHORS = [
    {"role": "quest_master_75_95", "position": [9140, 2870, 483.609]},
    {"role": "quest_master_95_110", "position": [9460, 2870, 483.609]},
    {"role": "quest_master_110_125", "position": [9950, 2870, 483.609]},
    {"role": "blacksmith", "position": [9300, 3370, 483.609]},
    {"role": "item_merchant", "position": [8820, 3150, 483.609]},
    {"role": "storage", "position": [9790, 3150, 483.609]},
    {"role": "portal_master", "position": [9300, 3090, 483.609]},
    {"role": "guild_master", "position": [9300, 2925, 483.609]},
]


def normalize_client_root(path: Path) -> Path:
    path = path.expanduser().resolve()
    if (path / "resmap").is_dir():
        return path
    if (path / "Client" / "resmap").is_dir():
        return (path / "Client").resolve()
    raise SystemExit(f"Source client does not contain resmap: {path}")


def output_map_dir(project_root: Path) -> Path:
    root = project_root.expanduser().resolve()
    if root.name.lower() == "client":
        return root / "resmap" / "field" / MAP_ID
    return root / "Client" / "resmap" / "field" / MAP_ID


def overlaps(a: Path, b: Path) -> bool:
    try:
        common = Path(os.path.commonpath([str(a.resolve()), str(b.resolve())]))
    except ValueError:
        return False
    return common == a.resolve() or common == b.resolve()


def game_ref_to_path(client_root: Path, ref: str) -> Path:
    rel = ref.replace("\\", "/").replace("./", "").lstrip("/")
    return client_root / Path(rel)


def all_model_refs(placements: Iterable[Placement]) -> list[str]:
    refs = {SKY, SEA, WOOL}
    refs.update(p.model for p in placements)
    return sorted(refs, key=str.casefold)


def validate_dependencies(client_root: Path, placements: list[Placement]) -> list[str]:
    missing: list[str] = []
    for ref in all_model_refs(placements):
        if not game_ref_to_path(client_root, ref).is_file():
            missing.append(ref)
    for rel in [
        r"resmap\field\Rou\Rou.HTD",
        r"resmap\field\Rou\Rou.HTDG",
        r"resmap\field\Rou\Rou.shbd",
        r"resmap\field\Rou\Rouvertexcolor2.bmp",
        r"resmap\field\Rou\block.Bmp",
        r"resmap\field\Rou\rock.Bmp",
        r"resmap\field\Rou\grass.Bmp",
        r"resmap\field\Rou\grasst1.dds",
        r"resmap\fieldtexture\L1_A.BMP",
        r"resmap\fieldTexture\L3_RE.dds",
        r"resmap\fieldTexture\L7_D.dds",
        r"resmap\fieldTexture\grass_01.dds",
    ]:
        if not game_ref_to_path(client_root, rel).is_file():
            missing.append(rel)
    return sorted(set(missing), key=str.casefold)


def fmt(value: float) -> str:
    return f"{value:.6f}"


def write_shmd(path: Path, placements: list[Placement]) -> None:
    groups: "OrderedDict[str, list[Placement]]" = OrderedDict()
    for placement in placements:
        groups.setdefault(placement.model, []).append(placement)

    lines: list[str] = []

    def record(*tokens: object) -> None:
        lines.append(" ".join(str(t) for t in tokens) + " \r\n")

    record("shmd0_5")
    record("Sky", 1)
    record(SKY)
    record("Water", 2)
    record(SEA)
    record(WOOL)
    record("GroundObject", 0)
    record("GlobalLight", "0.820000", "0.820000", "0.820000")
    record("Fog", "0.570000", "0.110000", "0.420000", "0.680000")
    record("BackGroundColor", "0.090000", "0.180000", "0.310000")
    record("Frustum", "5000.000000")

    for model, instances in groups.items():
        record(model, len(instances))
        for p in instances:
            half = math.radians(p.yaw_deg) * 0.5
            qz = math.sin(half)
            qw = math.cos(half)
            record(
                fmt(p.x), fmt(p.y), fmt(p.elevation),
                "0.000000", "0.000000", fmt(qz), fmt(qw), fmt(p.scale),
            )

    lines.append("DataObjectLoadingEnd\r\n")
    record("DirectionLightAmbient", "0.620000", "0.620000", "0.680000")
    record("DirectionLightDiffuse", "0.920000", "0.900000", "0.860000")
    path.write_bytes("".join(lines).encode("utf-8"))


def write_ini(path: Path) -> None:
    text = f'''#PGFILE : HeightMap\r\n#FILE_VER : 0.01\r\n\r\n#HeightFileName : ".\\resmap\\field\\{MAP_ID}\\{MAP_ID}.HTD"\r\n#VerTexColorTexture : ".\\resmap\\field\\Rou\\Rouvertexcolor2.bmp"\r\n\r\n#HEIGHTMAP_WIDTH : 257\r\n#HEIGHTMAP_HEIGHT : 257\r\n\r\n#OneBlockWidth : 50.0f\r\n#OneBlockHeight : 50.0f\r\n\r\n#QuadsWide : 64\r\n#QuadsHigh : 64\r\n\r\n#Layer\r\n{{\r\n#Name : 01 ground\r\n#DiffuseFileName : ".\\resmap\\field\\Rou\\grasst1.dds"\r\n#BlendFileName : ".\\resmap\\fieldtexture\\L1_A.BMP"\r\n#StartPos_X : 0.0f\r\n#StartPos_Y : 0.0f\r\n#Width : 257.0f\r\n#Height : 257.0f\r\n#UVScaleDiffuse : 4f\r\n#UVScaleBlend : 1.0f\r\n}}\r\n#Layer\r\n{{\r\n#Name : 02 block\r\n#DiffuseFileName : ".\\resmap\\fieldTexture\\L3_RE.dds"\r\n#BlendFileName : ".\\resmap\\field\\Rou\\block.Bmp"\r\n#StartPos_X : 0.0f\r\n#StartPos_Y : 0.0f\r\n#Width : 257.0f\r\n#Height : 257.0f\r\n#UVScaleDiffuse : 5f\r\n#UVScaleBlend : 1.0f\r\n}}\r\n#Layer\r\n{{\r\n#Name : 03 rock\r\n#DiffuseFileName : ".\\resmap\\fieldTexture\\L7_D.dds"\r\n#BlendFileName : ".\\resmap\\field\\Rou\\rock.Bmp"\r\n#StartPos_X : 0.0f\r\n#StartPos_Y : 0.0f\r\n#Width : 257.0f\r\n#Height : 257.0f\r\n#UVScaleDiffuse : 5f\r\n#UVScaleBlend : 1.0f\r\n}}\r\n#Layer\r\n{{\r\n#Name : 04 grass\r\n#DiffuseFileName : ".\\resmap\\fieldTexture\\grass_01.dds"\r\n#BlendFileName : ".\\resmap\\field\\Rou\\grass.Bmp"\r\n#StartPos_X : 0.0f\r\n#StartPos_Y : 0.0f\r\n#Width : 257.0f\r\n#Height : 257.0f\r\n#UVScaleDiffuse : 5f\r\n#UVScaleBlend : 1.0f\r\n}}\r\n\r\n#END_FILE\r\n'''
    path.write_bytes(text.encode("utf-8"))


class WalkGridTemplate:
    def __init__(self, raw: bytes) -> None:
        if len(raw) < 10 or (len(raw) - 8) % 2:
            raise ValueError("Rou.shbd has an invalid byte length")
        self.header = raw[:8]
        self.header_quad_count, self.rows = struct.unpack_from("<II", raw, 0)
        word_count = (len(raw) - 8) // 2
        if not self.rows or word_count % self.rows:
            raise ValueError("Rou.shbd dimensions cannot be derived from its header")
        self.words_per_row = word_count // self.rows
        self.cols = self.words_per_row * 16
        self.words = list(struct.unpack(f"<{word_count}H", raw[8:]))
        self.cell_size = WORLD_SIZE / float(self.cols)
        if abs(self.cell_size - EXPECTED_WALK_CELL_SIZE) > 0.001:
            raise ValueError(
                f"Unexpected walk cell size {self.cell_size:.6f}; expected {EXPECTED_WALK_CELL_SIZE}"
            )

    def set_cell(self, cx: int, cy: int, blocked: bool) -> None:
        if not (0 <= cx < self.cols and 0 <= cy < self.rows):
            return
        index = cy * self.words_per_row + cx // 16
        mask = 1 << (cx % 16)
        if blocked:
            self.words[index] |= mask
        else:
            self.words[index] &= ~mask

    def set_world_rect(self, x0: float, y0: float, x1: float, y1: float, blocked: bool) -> None:
        lo_x, hi_x = sorted((x0, x1))
        lo_y, hi_y = sorted((y0, y1))
        c0x = max(0, int(math.floor(lo_x / self.cell_size)))
        c1x = min(self.cols - 1, int(math.ceil(hi_x / self.cell_size)))
        c0y = max(0, int(math.floor(lo_y / self.cell_size)))
        c1y = min(self.rows - 1, int(math.ceil(hi_y / self.cell_size)))
        for cy in range(c0y, c1y + 1):
            row = cy * self.words_per_row
            for cx in range(c0x, c1x + 1):
                index = row + cx // 16
                mask = 1 << (cx % 16)
                if blocked:
                    self.words[index] |= mask
                else:
                    self.words[index] &= ~mask

    def to_bytes(self) -> bytes:
        return self.header + struct.pack(f"<{len(self.words)}H", *self.words)


def write_walk_grid(source_client: Path, target: Path, placements: list[Placement]) -> dict[str, object]:
    source = game_ref_to_path(source_client, r"resmap\field\Rou\Rou.shbd")
    grid = WalkGridTemplate(source.read_bytes())

    # Keep Rou's verified outer terrain/water/cliff walk semantics, but clear the entire new
    # city plateau first so no obsolete Rou-building masks survive under the new layout.
    grid.set_world_rect(8000, 900, 10700, 4400, False)

    # Stamp conservative rectangular footprints for solid placed assets. Roads, bridge,
    # banners and decoration without collision extents stay walkable.
    for p in placements:
        if p.collision_x > 0 and p.collision_y > 0:
            grid.set_world_rect(
                p.x - p.collision_x,
                p.y - p.collision_y,
                p.x + p.collision_x,
                p.y + p.collision_y,
                True,
            )

    target.write_bytes(grid.to_bytes())
    return {
        "header_quad_count": grid.header_quad_count,
        "rows": grid.rows,
        "words_per_row": grid.words_per_row,
        "cols": grid.cols,
        "cell_size": grid.cell_size,
    }


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def write_manifest(
    target: Path,
    source_client: Path,
    placements: list[Placement],
    walk_info: dict[str, object],
) -> None:
    terrain_h = game_ref_to_path(source_client, r"resmap\field\Rou\Rou.HTD")
    terrain_g = game_ref_to_path(source_client, r"resmap\field\Rou\Rou.HTDG")
    manifest = {
        "map_id": MAP_ID,
        "title": MAP_TITLE,
        "level_range": [LEVEL_MIN, LEVEL_MAX],
        "map_type": "safe_city_hub",
        "source_policy": "read-only source assets; project override output only",
        "terrain_template": {
            "map": TEMPLATE_MAP,
            "heightmap_points": [HEIGHTMAP_POINTS, HEIGHTMAP_POINTS],
            "world_size": [WORLD_SIZE, WORLD_SIZE],
            "htd_sha256": sha256(terrain_h),
            "htdg_sha256": sha256(terrain_g),
        },
        "walk_grid": walk_info,
        "safe_spawn": [9300, 3140, 483.609],
        "districts": DISTRICTS,
        "portal_anchors": PORTAL_ANCHORS,
        "npc_anchors": NPC_ANCHORS,
        "server_binding": {
            "status": "not_written_by_generator",
            "reason": "NPC, mob and TownPortal SHN/server IDs must be bound to real project data; no IDs are invented.",
        },
        "placements": [asdict(p) for p in placements],
        "dependencies": all_model_refs(placements),
    }
    target.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_readme(target: Path, placements: list[Placement]) -> None:
    text = f"""{MAP_TITLE}\n{'=' * len(MAP_TITLE)}\n\nMap ID: {MAP_ID}\nLevel bracket: {LEVEL_MIN}-{LEVEL_MAX}\nPlaced objects: {len(placements)}\n\nOpen in NextGen-Editor:\n  {MAP_ID}.ini\n\nConcept:\n  - West: level 75-85 arrival and quest handoff\n  - Central market: level 85-95 services/social hub\n  - North forge: level 95-105 crafting/upgrades\n  - East veteran bastion: level 105-115\n  - North citadel gate: level 115-125 expedition handoff\n  - South harbor: level 85-100 expedition/travel quarter\n\nImportant:\n  NPC/Mob/TownPortal records are intentionally not fabricated. Exact anchor positions are\n  stored in {MAP_ID}.layout.json and can be bound in the editor to real SHN/server records.\n  The source Fiesta client stays read-only.\n"""
    target.write_text(text, encoding="utf-8")


def generate(source_client: Path, project_root: Path, *, force: bool, validate_only: bool) -> Path:
    client = normalize_client_root(source_client)
    out_dir = output_map_dir(project_root)

    if overlaps(client, out_dir):
        raise SystemExit(
            f"Refusing source/project overlap: source={client} output={out_dir}. "
            "Use a separate NextGen project directory."
        )

    placements = build_city()
    missing = validate_dependencies(client, placements)
    if missing:
        formatted = "\n".join(f"  - {item}" for item in missing)
        raise SystemExit(f"Missing required source assets ({len(missing)}):\n{formatted}")

    if validate_only:
        print(f"{MAP_ID}: dependencies OK, {len(placements)} placements, no files written")
        return out_dir

    if out_dir.exists():
        if not force:
            raise SystemExit(f"Output already exists: {out_dir}. Re-run with --force to replace it.")
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    source_field = client / "resmap" / "field" / TEMPLATE_MAP
    shutil.copyfile(source_field / f"{TEMPLATE_MAP}.HTD", out_dir / f"{MAP_ID}.HTD")
    shutil.copyfile(source_field / f"{TEMPLATE_MAP}.HTDG", out_dir / f"{MAP_ID}.HTDG")

    write_ini(out_dir / f"{MAP_ID}.ini")
    write_shmd(out_dir / f"{MAP_ID}.shmd", placements)
    walk_info = write_walk_grid(client, out_dir / f"{MAP_ID}.shbd", placements)
    write_manifest(out_dir / f"{MAP_ID}.layout.json", client, placements, walk_info)
    write_readme(out_dir / "README-Aurelia75.txt", placements)

    print(f"Generated {MAP_TITLE}")
    print(f"  Output: {out_dir}")
    print(f"  Objects: {len(placements)}")
    print(f"  Open: {out_dir / (MAP_ID + '.ini')}")
    return out_dir


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=f"Generate {MAP_TITLE}")
    parser.add_argument(
        "--source-client",
        type=Path,
        required=True,
        help="Read-only Fiesta Client root (or install root containing Client/resmap)",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        required=True,
        help="Writable NextGen project root; output goes below Client/resmap/field/Aurelia75",
    )
    parser.add_argument("--force", action="store_true", help="Replace an existing Aurelia75 project map")
    parser.add_argument("--validate-only", action="store_true", help="Validate dependencies without writing files")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generate(args.source_client, args.project_root, force=args.force, validate_only=args.validate_only)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
