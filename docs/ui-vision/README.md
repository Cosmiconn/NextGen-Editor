# NextGen Editor – verbindliche UI-Vision

**Status:** verbindlich für den visuellen Ausbau von `ui-upgrade`  
**Stand:** 26.09.2026

Diese Dateien sind die visuelle Source of Truth. Der Editor soll als **hochwertiger, dunkler AAA-World-/Level-Editor** wirken – nicht wie eine nur gestylte Desktop-Anwendung.

## Qualitätsregel

Die vom Nutzer gelieferten Referenzbilder sind die primäre visuelle Referenz. Ihre bekannten Dateinamen, Rollen und der tatsächliche Git-Archivierungsstatus sind unter [references/README.md](references/README.md) dokumentiert.

Programmatische Wireframes oder vereinfachte Mockups dürfen Struktur erklären, sind aber **niemals** die Qualitätsreferenz für das finale UI.

## Primäre visuelle Zielbilder

1. NG-Monogramm / Branding
2. Icon-System / Navigation
3. Editor-Layout mit 3D + Outliner + Inspector + Asset Browser
4. Hero-Viewport / Premium-Shell
5. integrierte Gesamtvision

Die genaue Zuordnung steht in [references/README.md](references/README.md).

## Nicht verhandelbar

- Navy-/Graphit-Flächen statt Standard-ImGui-Grau.
- Cyan/Blau ist die Fokus-/Aktivfarbe.
- Der 3D-Viewport ist der Hero-Bereich.
- Toolbars verwenden klare, konsistente Icons.
- Outliner, Inspector, Asset Browser, Terrain, Layer, Minimap und Dateneditoren gehören sichtbar zum selben Designsystem.
- Hover, Active, Selected, Focus, Disabled, Dirty und Error haben überall dieselbe Bedeutung.
- Das metallisch-cyanfarbene **NG-Monogramm** ist die App-/EXE-Identität.
- Keine dauerhaft großen Hilfe-Popups über Viewports.
- Neue Screens werden erst als fertig betrachtet, wenn sie funktional **und** visuell mit den Zielbildern übereinstimmen.

## Spezifikation

- [01_BRAND_AND_THEME.md](01_BRAND_AND_THEME.md)
- [02_LAYOUT_BLUEPRINT.md](02_LAYOUT_BLUEPRINT.md)
- [03_COMPONENT_SYSTEM.md](03_COMPONENT_SYSTEM.md)
- [04_ICON_SYSTEM.md](04_ICON_SYSTEM.md)
- [05_MODULE_BLUEPRINTS.md](05_MODULE_BLUEPRINTS.md)
- [06_IMPLEMENTATION_PLAN.md](06_IMPLEMENTATION_PLAN.md)
- [references/README.md](references/README.md) – Originalreferenz-Manifest

## Aktiver Stand

- `NextGen_Icons_Final.zip` ist die verbindliche Artwork-Quelle des Icon-Systems: 68 geprüfte echte SVG-Vector-Master ohne eingebettete Rasterbilder sowie die freigegebenen PNG-Exporte 16 / 24 / 32 / 48 / 64 / 128 px.
- Der **aktuelle Branch archiviert bewusst nur den bereits migrierten Runtime-Subset** unter `assets/ui/icons/png/`; die vollständige Größenmatrix und die SVG-Master liegen noch nicht vollständig im Git-Checkout. `tools/ui/import_icon_pack.py` ist der reproduzierbare Importpfad, sobald das Final-ZIP lokal verfügbar ist.
- `assets/ui/icons/icon-map.json` stellt stabile semantische IDs bereit; `src/app/UiIconAssets.hpp/.cpp` sucht die gewünschte Größe und fällt auf den nächstgelegenen **tatsächlich vorhandenen** freigegebenen Rasterexport zurück. Fehlt für eine ID im Checkout derzeit jedes Raster, bleibt der bestehende DrawList-Fallback sichtbar.
- Die bereits eingecheckten Final-Paketicons sind an reale UI-Funktionen gebunden. Aktuelle QA migriert u. a. AI, Route/MobRoam, Spawn, Search/Filter und Transform-Aktionen auf ihre semantischen IDs, ohne unpassende Symbole umzudeuten.
- DrawList-Icons bleiben zulässig, wenn entweder das Final-Paket keine dedizierte Semantik besitzt oder der freigegebene Rasterexport dieser bereits gemappten ID im aktuellen Checkout noch nicht importiert wurde.
- `src/app/resources/nextgen.ico` enthält das NG-Markenicon in den nativen Windows-Größen; das Fenster setzt Ressource 101 zusätzlich für Titlebar, Alt-Tab und Taskleiste.
- `docs/ui-vision/ICON_INVENTORY.md` dokumentiert Inventar, Mapping, Runtime-Subset und bewusste Fallbacks des finalen 68-Icon-Systems.

## Referenzhierarchie

1. **Vom Nutzer gelieferte fünf Rasterreferenzen** – Look, Materialwirkung, Dichte, Hierarchie.
2. **NG-Monogramm aus Referenz 1** – Form- und Materialreferenz für Branding.
3. Markdown-Spezifikation – konkrete Tokens, Größen und Verhalten.
4. Implementierungsassets – SVGs / native DrawList-Icons / Rastergrößen.

Wenn sich Punkt 3 oder 4 optisch von Punkt 1 entfernt, wird die Implementierung angepasst – nicht das Zielbild.
