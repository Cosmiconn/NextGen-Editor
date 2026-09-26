# 06 – UI-Retrofit Implementation Plan

## Phase 0 – Vision Lock
- [x] UI-Vision im Repo
- [x] strukturelle SVG-Mockups geplant/versioniert
- [x] finales 68-Icon-Paket technisch geprüft, inventarisiert und semantisch gemappt
- [x] 68 echte SVG-Master und 6 freigegebene Small-Size-Rasterstufen im Final-Paket verifiziert
- [x] Windows-ICO/Resource 101 mit dem NG-Branding in die App integriert
- [ ] vollständige 68-SVG-/68×6-Runtime-Matrix in den Git-Checkout importieren; bis dahin bleibt nur der bereits benötigte Runtime-Subset eingecheckt
- [x] reproduzierbarer Importpfad über `tools/ui/import_icon_pack.py` dokumentiert

## Phase 1 – Design Tokens
Zentrale Theme-Tokens, Radius, Padding, Button-/Headerhöhen, States.

**Abnahme:** kein Kernpanel nutzt ad-hoc Farben ohne semantischen Grund.

## Phase 2 – App Shell
Topbar → Menüleiste → Primary Toolbar → Statusbar → Dock Header.

Die freigegebenen Paket-Icons ersetzen dabei schrittweise die bisherigen Lucide-/DrawList-Fallbacks; keine zweite Icon-Sprache parallel ausbauen.

**Abnahme:** Map, SHN, Quest, Skill und KFM teilen dieselbe Shell-Sprache.

## Phase 3 – Karteneditor
3D → Outliner → Inspector → Asset Browser → Terrain → Layer → 2D → Minimap → Block&Walk.

**Minimap-Gate:** zuerst `docs/MINIMAP_FORMAT.md` mit realen Fiesta-Beispielen vervollständigen. Eine editorinterne Preview darf vorher entstehen; ein Fiesta-Exporter erst nach verifiziertem Dateiformat, Pfad, Orientierung und Auflösung.

**Abnahme:** `mockups/editor-shell-target.svg` strukturell wiedererkennbar.

## Phase 4 – Data Workspaces
SHN → Quest → Skill → AI → Drops → Interface.

## Phase 5 – Animation
KFM Preview → Transport → Timeline → Trackliste → Skeleton/Mesh Playback.

## Phase 6 – Polish
Icon-Pass aus dem freigegebenen Paket, Tooltips, Spacing, Tastaturfokus, HiDPI, 100/125/150/200 %, 1366×768 / 1920×1080 / 2560×1440.

Der Icon-Pass wird durch `tools/ui/check_icon_consistency.py` abgesichert. CI verifiziert damit, dass `icon-map.json`, `UiIconAssets.cpp` und der tatsächlich eingecheckte Runtime-PNG-Subset semantisch übereinstimmen; ein noch unvollständiger Asset-Checkout bleibt ausdrücklich erlaubt.

## Harte Kriterien
1. 3D ist im Default-Layout größter Einzelbereich.
2. Kein dauerhaftes Hilfeoverlay verdeckt Viewports.
3. Aktive Werkzeuge sind sofort visuell eindeutig.
4. Eye/Lock/Dirty/Selection bedeuten überall dasselbe.
5. Icons bleiben bei 16 px verständlich.
6. Keine Kernaktion wird nur durch Farbe unterschieden.
7. Alle Workspaces verwenden dieselben Theme-Tokens.
