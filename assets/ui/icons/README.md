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

The source package is complete, but the Git checkout currently contains only the
runtime subset already needed by the migrated UI. This is intentional while the
branch remains under visual QA.

- `UiIconAssets` accepts the full approved size set `16/24/32/48/64/128`.
- At runtime it loads the closest **committed** approved PNG for a semantic ID.
- If no PNG for that semantic ID is committed yet, the caller keeps its existing
  functional DrawList fallback.
- Do not remap a missing runtime asset to a different semantic icon merely to
  avoid the fallback.
- Running the importer below with the approved Final ZIP materializes the full
  SVG masters, PNG size matrix, raster masters and manifest reproducibly.
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
