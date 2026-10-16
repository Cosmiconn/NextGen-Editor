# Fiesta Minimap / Overview Format Investigation

**Status:** verified against 180 original client minimaps (NA2016 `resmenu/minimap`); export is implemented (see "Verified image format" and "Export").

## Goal

Generate an editor minimap/overview from a loaded map while preserving Fiesta compatibility. The first implementation should be deliberately small:

- orthographic top-down capture;
- whole-map fit;
- correct aspect ratio and orientation;
- terrain/textures/water as the base image;
- important objects optional;
- NPC/Portal/Spawn/Walk overlays editor-preview-only unless a Fiesta format explicitly stores them;
- fixed quality presets only where the verified target format permits them.

## Hard rule

Do **not** introduce a new proprietary minimap format and do **not** infer a Fiesta filename, extension, axis convention, alpha rule or resolution from screenshots.

A Fiesta writer/export path remains blocked until real client assets establish:

1. map → minimap filename/path mapping;
2. file format/extension;
3. pixel dimensions;
4. alpha usage;
5. image origin/orientation;
6. world-axis mapping;
7. whether the stored image is square or follows map aspect ratio;
8. whether one image or multiple tiles are used;
9. whether metadata outside the image is required.

## Verified NA2016 metadata

The supplied `NA2016.zip` contains both:

- `Client/ressystem/MapViewInfo.shn`
- `Server/9Data/Shine/View/MapViewInfo.shn`

They are **byte-identical** in the supplied corpus (31,424 bytes; SHA-256 `f9839121b9a84191b341f8beed7802e041f8a68a5c990fb07d9673c6d264290f`) and parse successfully with the repository's verified SHN codec.

`MapViewInfo.shn` contains 138 rows and the following relevant fields:

- `MapName`
- `MiniMapScale`
- `MapFolderName`
- `MinimapView`
- `WorldMapView`
- `StartX`, `StartY`, `EndX`, `EndY`
- `ZoomMax`
- `MiniMapSort`

The coordinate fields use a strongly repeated **0..511 logical range**: 101 of 138 records use exactly `0,0 -> 511,511`; the maximum observed EndX is 511 and EndY is 512. This is evidence for a 512-based map-view coordinate/crop convention, **not yet proof that the stored image itself is 512×512**.

Observed `MiniMapScale` values are 0.0625, 0.125, 0.25, 0.5, 1.0 and two 1.855 values. Therefore `MiniMapScale` must be treated as data-driven metadata, not derived from one assumed map size.

### Verified rows

| Map | ID | MapFolderName | MiniMapScale | MinimapView | WorldMapView | StartX,Y | EndX,Y | MiniMapSort |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| Rou | 0 | Rou | 0.5 | 1 | 1 | 79,86 | 474,482 | 0 |
| Bera | 111 | Bera | 0.5 | 1 | 1 | 0,0 | 511,511 | 51 |
| SwaDn01 | 64 | SwaDn01 | 0.5 | 1 | 0 | 0,0 | 511,511 | 38 |
| Mem_UA | — | — | — | — | — | — | — | no MapViewInfo row found |

`WorldMapAvatarInfo.shn` is a separate table containing `MapName`, `LocalX`, `LocalY` and `NormalDn`. It establishes world-map marker placement metadata but does not identify a minimap image file. Example rows include Bera at 126,622 and UrgSwaDn01 at 74,240.

## Resource archive investigation

The supplied `resmap*.zip` archives were searched for filenames containing minimap/worldmap/map-view patterns and for image assets adjacent to Rou, Bera, SwaDn01 and Mem_UA.

No explicit minimap/worldmap image filename was found in these archives.

Several visually map-like 257×257 BMP files are **not** minimap evidence:

- `Rou/Rouvertexcolor2.bmp` is explicitly referenced by `Rou.ini` as `#VerTexColorTexture`.
- `Bera/bera_VertexLight.bmp` is explicitly referenced by `bera.ini` as `#VerTexColorTexture`.
- `Mem_UA/Mem_UA.BMP` is explicitly referenced by `Mem_UA.ini` as `#VerTexColorTexture`.

They may look like top-down maps because they encode terrain vertex colour/light information, but the INI semantics prove they must **not** be treated as Fiesta minimap files.

The supplied SwaDn01 directory contains scene/block files and no candidate raster minimap asset.

## Evidence table

| Map | Verified metadata | Candidate minimap file | Format / WxH / Alpha | Orientation | Current conclusion |
|---|---|---|---|---|---|
| Rou | MapViewInfo row ID 0; crop 79,86→474,482; scale 0.5 | none located | unresolved | unresolved | metadata verified, image mapping unresolved |
| Bera | MapViewInfo row ID 111; full 0→511 rectangle; scale 0.5 | none located | unresolved | unresolved | metadata verified, image mapping unresolved |
| SwaDn01 | MapViewInfo row ID 64; full 0→511 rectangle; scale 0.5 | none located | unresolved | unresolved | metadata verified, image mapping unresolved |
| Mem_UA | no MapViewInfo row | `Mem_UA.BMP` rejected as minimap candidate | 257×257 RGB vertex-color texture | n/a | no verified minimap metadata/image |

## Editor preview implementation

A **non-exporting preview panel is now implemented** using the editor's existing terrain/texturing render data. It does not assert a Fiesta file format.

Implemented preview scope:

- reuses the current heightmap/terrain and texture render data;
- independent orthographic top-down framebuffer, so the 2D editor and minimap cannot overwrite each other's texture in the same frame;
- fits the complete map bounds while preserving aspect ratio;
- optional normal-object markers;
- optional current 2D viewport rectangle;
- clicking the minimap recenters the existing 2D view without introducing a new coordinate model;
- dedicated `panel.minimap` icon/header in the dock UI;
- preview remains an editor texture; **no Fiesta image is written**.

Still pending inside the preview (not exporter): water/special-object overlays and optional gameplay overlays can be added only as editor visualization. When a loaded map has a matching `MapViewInfo` record, its scale/crop metadata may later be displayed as **read-only diagnostic data**, but must not silently change export output before the image convention is verified.

## Verified image format (original client minimaps)

The user supplied the client's `resmenu/minimap` folder (180 files). The files themselves are not committed. Together with `Fiesta.bin` they establish all points of the hard rule above:

1. **Path:** `Fiesta.bin` loads `%sminimap\%s.dds` relative to `ResMenu\`, with the name taken from `MapViewInfo.MapFolderName`. Several maps share one image this way, for example `Job2_Dn02` uses `EldFor01`.
2. **Format:** DDS, DXT1 or DXT3. Examples: `Rou.DDS` DXT3, `Bera.DDS` DXT1, `Adl.dds` DXT1 with mipmaps.
3. **Size:** 512×512 for regular maps, independent of the map size.
4. **Alpha:** not used visually. The images are opaque paintings, and DXT3 appears without any meaningful alpha.
5. **Orientation:** north up. Image row 0 is the north edge.
6. **Axis mapping:** `StartX/StartY/EndX/EndY` give a square crop in normalised map coordinates 0..511. The origin is the north-west corner, X runs east and Y runs south. The image shows exactly that crop.
7. **Shape:** always square. Every crop in `MapViewInfo` is square, for example Rou 79,86→474,482 and Eld 187,159→379,351.
8. **Tiles:** none; one image per map folder.
9. **Extra metadata:** only the `MapViewInfo` row (`MinimapView`, crop, `MiniMapScale`, `ZoomMax`).

### Evidence for the crop convention

Each original was compared against the walkability mask of the same map (`resmap/field/<map>/<map>.shbd` or `9Data/Shine/BlockInfo/<map>.shbd`), drawn north-up.

| Map | Crop | Result |
|---|---|---|
| Bera | 0,0→511,511 | The whole map, matching the walkable layout 1:1. |
| Rou | 79,86→474,482 | Edge correlation 0.087 with Y counted from the top, against ≤0.049 for every shifted variant and 0.015 with Y counted from the bottom. |
| RouTemDn01 | 0,106→405,511 | Visual check: the dungeon's south corridor reaches the lower image edge only with Y counted from the top. |
| Eld | 187,159→379,351 | Almost symmetric, so it does not discriminate between the two Y conventions. The full map (0.025) is clearly worse than the crop (0.097). |

The originals are hand-painted: parchment frame, dark outlines, painted rocks and water. They are not renders.

## Export (implemented)

- **Minimap panel:**
  - Shows the game minimap with the `MapViewInfo` crop applied. A project override takes precedence over the client file.
  - Can switch to the editor preview.
  - Object dots, the 2D view rectangle and clicks all use the crop convention.
- **"Generate minimap"** (`src/app/editor/MinimapTools.cpp`):
  - Renders an orthographic top-down view of terrain, objects and water into a 512×512 offscreen target. The sky is left out.
  - Optionally stylises it like the originals (`core::legacy::StylizeMinimap`: dark outlines along the walkable border, darker blocked areas, ragged parchment frame).
  - Writes `<Project>/Client/resmenu/minimap/<MapFolderName>.dds` as DXT1 without mipmaps, using `core::WriteDdsFile`.
  - Sets `MinimapView=1` and the crop `0,0-511,511` in both `MapViewInfo.shn` files, because the generated image shows the whole map.
- **New map wizard:** generates the minimap automatically once the new map is loaded.
- **Map check:** `MinimapMissing` is reported when `MinimapView=1` is set but no image exists.

`MiniMapScale` and `ZoomMax` are kept from the template row; their exact effect on the in-game zoom is not measured.
