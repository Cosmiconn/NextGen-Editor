# 02 – Layout Blueprint

## App Shell

1. Title/Menu Bar: 34–38 px
2. Primary Toolbar: 54–62 px
3. Dock Workspace
4. Status Bar: 24–28 px

## Primary Toolbar

Gruppen:
- Datei: Neu / Öffnen / Speichern
- Historie: Undo / Redo
- Transform: Select / Move / Rotate / Scale
- World: Brush / Terrain / Layer / Objects
- Gameplay: Block & Walk / Collision / Path
- Rendering: Light
- Runtime: Play/Test
- System: Settings / Help

Zwischen Gruppen dezente Separatoren.

## Karteneditor bei 1920×1080

- links: 250–290 px
- rechts: 300–360 px
- zentraler 3D-Bereich: mindestens 55 % der Breite
- untere Tool-Docks: 220–300 px
- Statusbar: 26 px

## 3D Viewport

Hero-Fläche. Eigener Header mit View Mode und Overlay-Buttons. Gizmo frei sichtbar. Statusdaten unten rechts. Keine permanente große Maus-Hilfe.

## 2D / Minimap

Gleiches Panel-System wie 3D, klare Layer-Toggles und Zoom/Scale-Anzeige.

## Links

Projekt/Map, Layer/World-Struktur, optional History.

## Rechts

Outliner oben, Inspector darunter. Preview als Inspector-Tab oder untere Card.

## Unten

Asset Browser, Terrain Tool, Minimap, Path/Block&Walk/Spezialwerkzeuge.

Das Default-Workspace erzeugt diese Gewichtung, bleibt danach aber vollständig dockbar.
