# NextGen Editor – Icon Inventory

**Status:** verbindliche Inventar-/Mapping-Datei für `ui-upgrade`  
**Authoritative source package:** `NextGen_Icons_Complete_PNG_SVG.zip`  
**Supersedes:** `NextGen_Icons_True_Vector_Set_With_Sizes.zip`

## Source-of-truth rule

The newly uploaded `NextGen_Icons_Complete_PNG_SVG.zip` is the only approved source for new in-app icon artwork. It explicitly supersedes the earlier icon ZIP. Existing Lucide-based SVGs, old ImDrawList primitives, and SVGs from the superseded ZIP are implementation legacy and may only remain until the corresponding approved icon from the new package is wired in.

Rules:

- Never invent a substitute when the package already contains a matching icon.
- Do not mix unrelated icon styles in the final UI.
- App branding uses the metallic blue/cyan **NG** mark, not a generic “N” or a text-only placeholder.
- Missing-icon requests are created only after the package manifest has been checked.
- Every runtime mapping must point to a documented logical icon id.

## Package structure

The previous package documented **54 logical icons**. The exact contents, filenames and counts of the new `NextGen_Icons_Complete_PNG_SVG.zip` must be taken from the archive itself and must not be inferred from the superseded package. The new archive is expected to provide both PNG and improved SVG artwork; its manifest/inventory will replace the provisional rows below after extraction.

The superseded package shipped the following production/export material:

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

The user supplied `NextGen_Icons_Complete_PNG_SVG.zip` specifically because these SVG files are the improved/correct vector source. Therefore:

1. the SVG artwork from the **new** package is the preferred master for scalable editor icons;
2. matching PNGs remain the raster reference for visual parity and small-size QA;
3. SVGs from the superseded package must not be promoted as masters;
4. for 16/24 px use, compare the rendered SVG against the package PNG and use the provided small raster export if it is visibly crisper;
5. do not redraw or reinterpret the approved silhouette merely to make implementation easier.

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

The exact per-file rows must be imported from the new package inventory/manifest (or generated directly from its archive listing if it has no manifest). Until that import is committed, **do not rename, replace or delete package artwork by inference**.

Required columns:

| Source filename | Logical id | Depicted function | Editor use | Formats/sizes | Duplicate / conflict | Status |
|---|---|---|---|---|---|---|
| _from manifest_ | _mapped after inspection_ | _verified visually_ | _target module/action_ | _from package_ | _yes/no + note_ | pending / mapped / integrated |

## Integration checklist

- [ ] Extract and inventory `NextGen_Icons_Complete_PNG_SVG.zip` verbatim; import its manifest when present.
- [ ] Visually inspect all 54 logical icons.
- [ ] Record duplicates / near-duplicates and any misleading semantics.
- [ ] Freeze logical icon ids.
- [ ] Copy approved master assets into `src/app/resources/icons/` and branding into `src/app/resources/branding/`.
- [ ] Add runtime texture loading / caching for PNG/SVG-derived raster assets.
- [ ] Replace legacy DrawList icon primitives incrementally.
- [ ] Rebuild `nextgen.ico` from the approved NG source.
- [ ] Verify 16/24/32/48/64/128 and 256 app-icon output.
- [ ] Verify normal / hover / active / disabled rendering in the real editor.
