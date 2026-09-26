# 03 – Component System

## Panel Header
30–34 px, Icon 16–18 px, Titel semibold. Maximal drei Direktaktionen, Rest Kontextmenü.

Im Map-/KFM-Workspace wird dafür zentral `DrawPanelHeader(...)` verwendet: freigegebenes semantisches Icon links, Cyan-Titel, optionaler sekundärer Kontexttext und eine ruhige 1-px-Abschlusslinie. Outliner, Layer, Asset Browser, Visibility, Properties, 2D, 3D und KFM dürfen keine eigenen abweichenden Header-Chromes mehr erfinden.

## Global Menu Bar
Die App-Shell besitzt eine funktionale Menüleiste in derselben dunklen Navy-Sprache wie die Topbar: Datei / Bearbeiten / Ansicht / Map / Objekte / Terrain / Layer / Werkzeuge / Fenster / Hilfe. Menüeinträge dürfen nur vorhandene Aktionen auslösen; nicht verfügbare Befehle sind disabled statt Attrappen. Shortcuts werden am Menüeintrag angezeigt. DE/EN gilt auch hier vollständig.

## Toolbar Button
44×44 px, Icon 22–24 px. Aktiver Toolmodus blau gefüllt. Destructive Actions niemals blau.

## Compact Button
28–32 px hoch, Radius 5 px, Icon 16–20 px. Die App-Shell verwendet `DrawCompactIconTextButton(...)` für Hauptnavigation sowie Neu/Öffnen/Speichern. Active = Blue/Cyan-Fläche + Cyan-Unterstrich; Hover bleibt deutlich schwächer.

## Inputs
28–32 px, dunkle Fläche, 1 px Border. Fokus = Cyan/Blue. X/Y/Z-Farbe nur am Prefix.

## Tabs
Flach. Active = blauer Unterstrich oder leicht gefülltes Segment.

## Tabellen
Header 30 px, Zeile 26–30 px. Hover, Selection, Dirty und Error klar getrennt. Filterzeile direkt unter Header erlaubt.

## Outliner
Zeile: `[Eye] [Lock] [Semantic Icon] Name ........ [Type/Badge]`

Eye/Lock stehen immer gleich. Hierarchie: 16 px Einrückung. Gruppen zeigen Counts.

## Inspector
Reihenfolge: Transform → Modell/Daten → Anzeige → Gameplay/Spezial.  
2-spaltige Label/Value-Struktur. Referenzen werden klickbare Chips/Links.

## Toasts
Unten rechts, 3.5–4.5 s, max. 3 gleichzeitig. Statusfarbe nur als Accent-Balken.

## Dialoge
Titel klar, Primäraktion rechts, Abbrechen daneben, irreversible Aktion rot bestätigt.
