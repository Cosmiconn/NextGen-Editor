# 04 – Icon System

## Verbindliche Quelle

Für den UI-Retrofit ist das vom Nutzer gelieferte Paket **`NextGen_Icons_Complete_PNG_SVG.zip`** die verbindliche Artwork-Quelle.

- vollständiges Inventar/Mapping: [ICON_INVENTORY.md](ICON_INVENTORY.md)
- bestehende Lucide-SVGs und native ImDrawList-Symbole sind Legacy-Implementierung und werden schrittweise ersetzt
- kein neues Ersatzicon zeichnen, solange ein passendes Paket-Icon existiert
- die verbesserten SVG-Dateien aus diesem neuen Paket sind die bevorzugten Vektor-Master; die PNGs dienen als Rasterreferenz und für Small-Size-QA
- das frühere Paket `NextGen_Icons_True_Vector_Set_With_Sizes.zip` ist superseded und darf nicht mehr als Masterquelle verwendet werden

## Grundstil

Die freigegebenen Icons definieren die Form. Runtime-Darstellung folgt diesen Regeln:

- klare Silhouette bei 16–24 px
- konsistente optische Box / Baseline
- neutraler Zustand ruhig und kontrastarm
- Hover hebt Kontur/Foreground an
- Active nutzt Blue/Cyan, ohne die gesamte Toolbar grell zu machen
- Disabled 42–50 % Deckkraft, ohne Glow
- destructive Actions rot, nicht blau
- keine Mischung aus Emojis, generischen Font-Glyphs und freigegebenem Set

## Größenklassen

| Einsatz | Zielgröße |
|---|---:|
| Outliner / Inline | 14–16 px |
| Panel Header | 16–18 px |
| Compact Controls | 16–20 px |
| Primary Toolbar | 22–24 px |
| Module Launcher | 24–32 px |
| App Small Mark | 16 / 24 / 32 px |
| App / Windows | 16 / 24 / 32 / 48 / 64 / 128 / 256 px |

Die Paketexports 16/24/32/48/64/128 werden bevorzugt direkt verwendet, wenn sie bei der Zielgröße sauberer als eine zur Laufzeit skalierte große Quelle aussehen.

## Semantik

Neutral = secondary text · Hover = primary text · Active = cyan/blue · Collision = purple · Warning = amber · Delete = red · Playtest/Success = green.

Farbe ändert nicht die Bedeutung des Glyphs; Kernaktionen müssen auch ohne Farbe unterscheidbar bleiben.

## Stabile logische IDs

UI-Code referenziert semantische IDs statt konkrete Dateinamen:

- `file.*`
- `transform.*`
- `world.*`
- `scene.*`
- `gameplay.*`
- `module.*`
- `system.*`
- `brand.*`

Beispiel: `module.quest` kann je nach UI-Größe auf 16-, 24- oder 32-px-PNG zeigen, ohne dass Quest-UI-Code den Dateipfad kennen muss.

## Pflichtabdeckung

Datei/History: New, Open, Save, Undo, Redo  
Transform: Select, Move, Rotate, Scale, Focus, Ground, Local/World, Snap  
World/View: 2D, 3D, Terrain, Brush, Layers, Materials, Water, Sky, Weather, Light, Biome, Minimap  
Scene: Object, Outliner, Asset Browser, Visibility, Lock, Unlock, Search, Filter  
Gameplay: NPC, Spawn/Mob, Portal, Trigger, Event, Collision, Block & Walk, Path  
System: Play/Test, Project, Settings, Help, Validate, Export, LOD  
Module launcher: Single SHN, Multi SHN, Quest, Skill, Interface, Drop Table, Custom NPC, Custom Mob  
Brand: metallic blue/cyan NG mark

## Branding

Das metallisch blau/cyan leuchtende NG-Monogramm wird konsistent für EXE, Window Icon, Taskbar und Produktbranding eingesetzt. Kleine Windows-Größen dürfen Details vereinfachen, aber nicht die NG-Silhouette verändern.

## Abnahme

Ein Icon gilt erst als integriert, wenn:

1. Funktion und Symbol semantisch passen.
2. 16/24-px-Darstellung lesbar ist.
3. Normal/Hover/Active/Disabled stimmen.
4. Tooltip und Shortcut-Hinweis stimmen.
5. keine Legacy-Ersatzgrafik parallel sichtbar bleibt.
6. Mapping in [ICON_INVENTORY.md](ICON_INVENTORY.md) aktualisiert ist.
