# Fiesta Minimap / Overview Format Investigation

**Status:** investigation gate – no exporter is allowed until real Fiesta examples are verified.

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

Before any writer/export path is implemented, at least several real client examples must establish:

1. map → minimap filename/path mapping;
2. file format/extension;
3. pixel dimensions;
4. alpha usage;
5. image origin/orientation;
6. world-axis mapping;
7. whether the stored image is square or follows map aspect ratio;
8. whether one image or multiple tiles are used;
9. whether metadata outside the image is required.

## Repository investigation

Repository code/search was checked for the terms `minimap`, `MapIcon`, `worldmap`, map-preview phrases, and direct Rou/Bera/SwaDn01/Mem_UA image-name guesses. No committed Fiesta minimap sample or verified mapping was found.

This absence is **not** evidence about the game format. It only means the repository itself currently does not provide a ground-truth minimap sample.

## Real-map corpus to inspect

The existing verified map corpus includes at least:

- Rou
- Bera
- SwaDn01
- Mem_UA

The client/resource archives used elsewhere in this project must be searched by:

- neighbouring map directory;
- matching map stem;
- likely image assets referenced by map/world UI data;
- dimensions and headers rather than filename guesses alone.

At least three independent maps should be compared before declaring a convention.

## Evidence table

Populate this table only from actual files.

| Map | Client path | Filename | Format | WxH | Alpha | Orientation | Mapping evidence |
|---|---|---|---|---:|---|---|---|
| Rou | pending | pending | pending | pending | pending | pending | pending |
| Bera | pending | pending | pending | pending | pending | pending | pending |
| SwaDn01 | pending | pending | pending | pending | pending | pending | pending |
| Mem_UA | pending | pending | pending | pending | pending | pending | pending |

## What is already safe to implement before export

The editor may implement a **non-exporting preview panel** using its own render data because that does not assert a Fiesta file format.

Safe preview scope:

- reuse current heightmap/terrain and texture render data;
- orthographic top-down camera;
- fit full map bounds;
- optional water/object overlays;
- current 2D/3D camera viewport rectangle;
- click-to-focus can be added later;
- preview remains an editor texture until target format verification is complete.

The preview UI must follow the same panel-header, toolbar, hover, active and disabled states as the 2D/3D viewports.

## Export implementation gate

Exporter work starts only after the evidence table contains enough real samples to establish all of:

- target file type;
- output path/name;
- image dimensions/aspect policy;
- orientation;
- alpha/channel expectations.

If examples disagree, the exporter must model the real variation rather than normalize it to a guessed universal rule.
