# NextGen Editor – verbindliche UI-Vision

**Status:** verbindlich für den visuellen Ausbau von `ui-upgrade`  
**Stand:** 25.09.2026

Diese Dateien sind die visuelle Source of Truth. Der Editor soll als **hochwertiger, dunkler AAA-World-/Level-Editor** wirken – nicht wie eine nur gestylte Desktop-Anwendung.

## Qualitätsregel

**Die hochauflösenden PNG-Boards unter `reference/` sind die primäre visuelle Referenz.**  
Sie dürfen nicht durch vereinfachte Wireframes, grobe SVG-Skizzen oder Platzhaltergrafiken ersetzt werden.

Programmatische Diagramme dürfen Struktur erklären, sind aber **niemals** die Qualitätsreferenz für das finale UI.

## Primäre visuelle Zielbilder

### UI Design Specification A
![NextGen UI Design Specification A](reference/06-generated-design-spec-a.png)

### UI Design Specification B
![NextGen UI Design Specification B](reference/07-generated-design-spec-b.png)

### UI Design Specification C
![NextGen UI Design Specification C](reference/08-generated-design-spec-c.png)

Diese drei Dateien sind byte-genaue Repo-Kopien der hochwertigen Zielbilder aus dem Design-Review.

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

## Referenzhierarchie

1. **High-resolution PNG boards** – Look, Materialwirkung, Dichte, Hierarchie.
2. **Vom Nutzer freigegebenes NG-Motiv** – Formreferenz für App-/EXE-Branding.
3. Markdown-Spezifikation – konkrete Tokens, Größen und Verhalten.
4. Implementierungsassets – SVGs / native DrawList-Icons / Rastergrößen.

Wenn sich Punkt 3 oder 4 optisch von Punkt 1 entfernt, wird die Implementierung angepasst – nicht das Zielbild.
