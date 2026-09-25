# NextGen Editor – production SVG icon sources

Dieser Ordner enthält die **Implementierungsquellen** für das neue Icon-System.

## Qualitätsstandard
- konsistente 24×24-Geometrie
- klare Silhouette bei 16–24 px
- einheitliche Linienführung
- keine Emojis, keine ad-hoc gezeichneten Platzhalter
- Zustandsfarbe wird vom NextGen-Theme gesteuert

## Quelle
Die neutralen Basisglyphen stammen aus **Lucide Icons** (ISC License, siehe `LICENSE-LUCIDE.txt`) und werden im NextGen Editor semantisch benannt und bei Bedarf projektbezogen angepasst.

Die hochauflösenden visuellen Zielbilder liegen unter:
`docs/ui-vision/reference/`

Diese SVGs sind die technische Icon-Basis; **die PNG-Zielbilder bleiben die visuelle Qualitätsreferenz**.

## Vorhandene Kernicons
Datei/History: new, open, save, undo, redo  
Transform: select, move, rotate, scale  
World: brush, terrain, layers, object, water, sky, weather, light  
Scene: outliner, asset-browser, visibility, search, filter  
Gameplay: npc, spawn, portal, trigger, event, collision, block-walk, path  
System: minimap, project, settings, help, validate, playtest, export, copy, duplicate, lock, unlock

Noch projektindividuell zu zeichnen: NG-App-Small-Mark, spezielle Fiesta-/Map-Semantik, einige gefüllte Active-Varianten.
