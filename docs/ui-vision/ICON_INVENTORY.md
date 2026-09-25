# NextGen Editor – Icon Inventory

**Status:** verbindliche Inventar-/Mapping-Datei für `ui-upgrade`  
**Authoritative source package:** `NextGen_Icons_True_Vector_Set_With_Sizes.zip`

## Source-of-truth rule

The uploaded package above is the only approved source for the new in-app icon artwork. Existing Lucide-based SVGs and old ImDrawList primitives are implementation legacy and may only remain until the corresponding approved package icon is wired in.

Rules:

- Never invent a substitute when the package already contains a matching icon.
- Do not mix unrelated icon styles in the final UI.
- App branding uses the metallic blue/cyan **NG** mark, not a generic “N” or a text-only placeholder.
- Missing-icon requests are created only after the package manifest has been checked.
- Every runtime mapping must point to a documented logical icon id.

## Package structure

The approved package contains **54 logical icons** and ships the following production/export material:

- `manifest.csv`
- `svg/`
- `png_256/`
- `png_1024/`
- `icons_png/16/`
- `icons_png/24/`
- `icons_png/32/`
- `icons_png/48/`
- `icons_png/64/`
- `icons_png/128/`
- button-tile exports at the same small sizes
- overview/readme material

The 16/24/32/48/64/128 PNG exports are intended for actual UI size classes. The large PNG masters remain the visual reference for glow, highlights and edge treatment.

## Vector quality note

The package SVGs must be evaluated against the raster masters before runtime adoption. Previous automatic raster-to-vector reconstruction lost part of the original glow/gradient/highlight treatment. Therefore:

1. raster masters define the visual appearance;
2. SVGs are used only when they reproduce that appearance cleanly at target size;
3. PNG runtime icons are acceptable where they are visually superior and rendering cost is negligible;
4. a future hand-authored/vector-master pass must preserve the same silhouette and material language rather than redesigning the icons.

## Required logical coverage

The final mapping must cover at least these editor functions where present in the package:

- File/history: New, Open, Save, Undo, Redo
- Transform: Select, Move, Rotate, Scale
- View/world: 2D, 3D, Brush, Terrain, Layer, Objects, Minimap
- Gameplay: Block & Walk, Collision, Path, Light, Portal, Spawn, Trigger, Event
- Scene/navigation: Asset Browser, Outliner, Search, Filter, Validation, Export, Play/Test, Project, Materials, Biome, LOD
- Data/editor modules: Single SHN, Multi SHN, Quest, Skill, Interface, Drop Table, Custom NPC, Custom Mob
- Branding: NG app mark

## Runtime mapping policy

The runtime mapping will use stable semantic ids, independent of file extension or raster size, for example:

```text
file.save
transform.move
world.terrain
scene.outliner
module.quest
module.skill
brand.ng
```

Toolbar, panel header, outliner and module-launcher code consume these semantic ids and select the appropriate asset size. This keeps UI code independent from package filenames.

## Inventory table

The exact per-file rows are imported from the package `manifest.csv`. Until that import is committed, **do not rename, replace or delete package artwork by inference**.

Required columns:

| Source filename | Logical id | Depicted function | Editor use | Formats/sizes | Duplicate / conflict | Status |
|---|---|---|---|---|---|---|
| _from manifest_ | _mapped after inspection_ | _verified visually_ | _target module/action_ | _from package_ | _yes/no + note_ | pending / mapped / integrated |

## Integration checklist

- [ ] Import the package manifest verbatim.
- [ ] Visually inspect all 54 logical icons.
- [ ] Record duplicates / near-duplicates and any misleading semantics.
- [ ] Freeze logical icon ids.
- [ ] Copy approved master assets into `src/app/resources/icons/` and branding into `src/app/resources/branding/`.
- [ ] Add runtime texture loading / caching for PNG/SVG-derived raster assets.
- [ ] Replace legacy DrawList icon primitives incrementally.
- [ ] Rebuild `nextgen.ico` from the approved NG source.
- [ ] Verify 16/24/32/48/64/128 and 256 app-icon output.
- [ ] Verify normal / hover / active / disabled rendering in the real editor.
