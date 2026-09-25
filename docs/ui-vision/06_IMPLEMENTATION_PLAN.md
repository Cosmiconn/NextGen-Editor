# 06 – UI-Retrofit Implementation Plan

## Phase 0 – Vision Lock
- [x] UI-Vision im Repo
- [x] strukturelle SVG-Mockups geplant/versioniert
- [x] SVG-Icon-Pack geplant/versioniert
- [ ] freigegebene Raster-Referenzen zusätzlich archivieren
- [ ] Windows-ICO aus dem NG-Master neu erzeugen

## Phase 1 – Design Tokens
Zentrale Theme-Tokens, Radius, Padding, Button-/Headerhöhen, States.

**Abnahme:** kein Kernpanel nutzt ad-hoc Farben ohne semantischen Grund.

## Phase 2 – App Shell
Topbar → Toolbar → Statusbar → Dock Header.

**Abnahme:** Map, SHN, Quest, Skill und KFM teilen dieselbe Shell-Sprache.

## Phase 3 – Karteneditor
3D → Outliner → Inspector → Asset Browser → Terrain → Layer → 2D/Minimap → Block&Walk.

**Abnahme:** `mockups/editor-shell-target.svg` strukturell wiedererkennbar.

## Phase 4 – Data Workspaces
SHN → Quest → Skill → AI → Drops → Interface.

## Phase 5 – Animation
KFM Preview → Transport → Timeline → Trackliste → Skeleton/Mesh Playback.

## Phase 6 – Polish
Icon-Pass, Tooltips, Spacing, Tastaturfokus, HiDPI, 100/125/150/200 %, 1366×768 / 1920×1080 / 2560×1440.

## Harte Kriterien
1. 3D ist im Default-Layout größter Einzelbereich.
2. Kein dauerhaftes Hilfeoverlay verdeckt Viewports.
3. Aktive Werkzeuge sind sofort visuell eindeutig.
4. Eye/Lock/Dirty/Selection bedeuten überall dasselbe.
5. Icons bleiben bei 16 px verständlich.
6. Keine Kernaktion wird nur durch Farbe unterschieden.
7. Alle Workspaces verwenden dieselben Theme-Tokens.
