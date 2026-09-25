# Fiesta Minimap / Overview Format Investigation

**Status:** metadata convention partially verified; image/export format gate remains closed.

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

## What is already safe to implement before export

The editor may implement a **non-exporting preview panel** using its own render data because that does not assert a Fiesta file format.

Safe preview scope:

- reuse current heightmap/terrain and texture render data;
- orthographic top-down camera;
- fit full map bounds;
- optional water/object overlays;
- current 2D/3D camera viewport rectangle;
- click-to-focus can be added later;
- preview remains an editor texture until target format verification is complete;
- when a loaded map has a matching `MapViewInfo` record, its scale/crop metadata may be displayed as **read-only diagnostic data**, but must not silently change export output before the image convention is verified.

The preview UI must follow the same shared panel-header, toolbar, hover, active and disabled states as the 2D/3D viewports.

## Export implementation gate

**Still closed.** Exporter work starts only after real client image assets establish all of:

- target file type;
- output path/name;
- image dimensions/aspect policy;
- orientation;
- alpha/channel expectations;
- how `MapFolderName`, `MiniMapScale` and Start/End coordinates relate to that image.

The 512-based metadata convention is useful evidence, but it is not enough to invent the missing file mapping. If later examples disagree, the exporter must model the real variation rather than normalize it to a guessed universal rule.
