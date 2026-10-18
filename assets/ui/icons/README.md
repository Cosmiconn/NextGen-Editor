# NextGen UI icon assets

`NextGen_Icons_Final.zip` is the single authoritative source package for the editor icon system.

It contains:
- `svg/`: **68 true path-based vector masters**;
- `png_256/` and `png_1024/`: transparent raster renders;
- `icons_png/{16,24,32,48,64,128}/`: curated runtime exports;
- `svg_embedded_png_256/` and `svg_embedded_png_1024/`: SVG containers for exact PNG appearance;
- matching button-tile PNG/SVG variants.

The final package extends the original 54-icon set with 14 UI icons:
2D, 3D, KFM, AI, XP, Preise, Visibility Eye, Lock, Unlock, Copy,
Duplicate, Delete, Command Palette and Recent Projects.

## Current checkout state

The checkout contains the complete runtime PNG matrix from the Final ZIP
(SHA-256 `4057d096eb71cd9ef84bdcf5741afa66f85148ae5ffa55e82c87ff30e50e1b77`):
all 68 semantic IDs in all six sizes `16/24/32/48/64/128` (408 PNGs under `png/`).
The SVG masters and 1024 px raster masters stay in the ZIP; they are not needed at
runtime and are not committed.

- `UiIconAssets` loads the closest approved PNG for a semantic ID and size.
- The DrawList fallback of each caller remains only as a safety net when the
  `assets/ui` folder is missing next to the executable.
- Do not remap a semantic ID to a different icon file.

To refresh the runtime PNGs from a new approved package, run the importer into a
scratch folder and copy its `assets/ui/icons/png/` into the checkout:
Import:

```bash
python tools/ui/import_icon_pack.py \
  --pack /path/NextGen_Icons_Final.zip \
  --repo-root . --clean
```

Add `--include-embedded-svg` only when the exact-raster SVG containers are
needed for archival/export purposes. They are not runtime vector masters.

The importer verifies:
- exactly 68 manifest rows;
- group counts 18 / 15 / 12 / 9 / 14;
- every file under `svg/` contains vector paths and no embedded raster;
- every runtime size exists for every icon.

Runtime policy:
- 16 px: inline/outliner/state actions;
- 24 px: primary toolbar;
- 32 px: module launchers;
- 48/64/128: larger cards/branding.

Do not hand-edit generated exports. Update the approved final package and rerun
the importer.

Consistency check:

```bash
python tools/ui/check_icon_consistency.py
```

The checker validates the frozen 68 semantic IDs against `UiIconAssets.cpp`,
the approved size set and every PNG that is actually committed under
`assets/ui/icons/png/`. It intentionally permits semantic IDs with no committed
runtime raster yet; those remain on the documented DrawList fallback until the
approved Final ZIP is imported.

At runtime, `UiIconAssets::IsKnownSemantic(...)` provides the same semantic boundary.
`Texture(...)` logs an unknown semantic ID once, while a known ID whose approved PNG
has not been committed yet remains a silent, intentional DrawList fallback.
