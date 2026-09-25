# NextGen Editor – runtime icon resources

This directory contains the icon assets used by the editor runtime.

## Authoritative artwork

The approved source package for the UI upgrade is:

`NextGen_Icons_True_Vector_Set_With_Sizes.zip`

The package inventory and semantic mapping are tracked in:
`docs/ui-vision/ICON_INVENTORY.md`.

Do not add newly invented replacement icons when the approved package already contains the required function.

## Current migration state

The SVG files currently in this directory originated from the earlier Lucide-based implementation pass. They are **legacy migration assets**, not the final visual source of truth. The older native ImDrawList icon primitives in `main.cpp` are legacy as well.

They may remain temporarily while the approved package is imported and mapped, but a finished module must not visibly mix both styles.

## Runtime quality rules

- Prefer package-native 16/24/32 px exports for dense editor chrome.
- Preserve larger PNG masters for visual comparison and future regeneration.
- Use SVG only where it reproduces the approved raster appearance cleanly.
- If an automatically reconstructed SVG visibly loses glow, gradients, bevels or edge quality, use the approved PNG export instead.
- State color is controlled by the NextGen UI theme; destructive actions remain red and active tools blue/cyan.
- Keep one stable semantic id per editor action; UI code must not depend on arbitrary source filenames.

## Migration order

1. import/package inventory;
2. NG branding and executable/window icon;
3. app shell / primary toolbar;
4. viewport and map-editor panels;
5. data-editor module launchers;
6. secondary actions / context menus;
7. remove obsolete Lucide/native DrawList fallbacks once their replacements are verified.

See `docs/ui-vision/04_ICON_SYSTEM.md` and `docs/ui-vision/ICON_INVENTORY.md`.
