// GENERIERT (Handbuch-Daten, zweisprachig) - siehe docs/MANUAL_MAINTENANCE.md zum Erweitern.
#include "mapeditor/core/Manual.hpp"

namespace theseed::mapeditor::core::manual {
namespace {

const Chapter kChapters[] = {
    {"start", "Erste Schritte", "Getting started"},
    {"controls", "Steuerung und Tastenkürzel", "Controls and shortcuts"},
    {"map", "Map-Editor", "Map editor"},
    {"shn", "Datei Editor (SHN-Tabellen, Interface, Animationen)", "File Editor (SHN tables, interface, animations)"},
    {"creators", "Erstellen: NPCs, Mobs, Skills, Shops", "Creating: NPCs, mobs, skills, shops"},
    {"reference", "Nachschlagen", "Reference"},
};

const Section kSections[] = {
    {"start.overview", "start",
     "Überblick", "Overview",
     R"MAN(Der Editor besteht aus zwei großen Bereichen; die Startseite zeigt genau diese zwei Felder:
- Map Editor (links): alles, was auf einer Karte liegt. Karten anlegen (Assistent) und öffnen, Höhenkarte und Texturen, Foliage, Begehbarkeit (Block & Walk), Objekte und Prefabs, Sky/Water/GroundObject, NPCs, Mobs (Monster-Spawns), Routen und Portale, Minimap und Kartenprüfung. Alle Bereiche zeigen dieselbe Karte in einer 2D-Draufsicht (zum Bearbeiten) und einer 3D-Ansicht (zum Prüfen und Ausrichten).
- Datei Editor (rechts): alle Spieldaten-Dateien unabhängig von einer Karte, aus Client (ressystem, resmenu) und Server (9Data/Shine). SHN-Tabellen einzeln oder mehrere zugleich, Items, Mobs, Skills, Passive, Zustände, Item-Systeme (Sets, Aufwertung, Mix, Zufallsoptionen, Pakete), Angriffsfolgen der Mobs, Quests, Drops, Preise, XP, KI-Skripte, Texte und Lokalisierung, Interface (Ladebilder, Icons, Porträts, Fenster-Layouts) und Animationen (KFM).
Unter jeder Beschreibung führen Schnellknöpfe direkt in einen Unterbereich (z. B. Neue Karte, Items, Interface, Animationen).
Es wird NICHTS automatisch gespeichert. Jede Änderung liegt zunächst nur im Speicher; die jeweiligen Speichern-Knöpfe schreiben die Dateien. Angepasste SHN-Dateien sind in den Listen mit * markiert.
Lege vor dem ersten Speichern eine Sicherung Deiner Client- und Server-Ordner an.)MAN",
     R"MAN(The editor has two main areas; the start page shows exactly these two fields:
- Map Editor (left): everything that lives on a map. Create maps (wizard) and open them, heightmap and textures, foliage, walkability (Block & Walk), objects and prefabs, Sky/Water/GroundObject, NPCs, mobs (monster spawns), routes and portals, minimap and map check. All areas show the same map as a 2D top view (for editing) and a 3D view (for checking and aligning).
- File Editor (right): all game data files independent of a map, from client (ressystem, resmenu) and server (9Data/Shine). SHN tables one at a time or several at once, items, mobs, skills, passives, states, item systems (sets, upgrade, mix, random options, packages), mob attack sequences, quests, drops, prices, XP, AI scripts, texts and localization, interface (loading screens, icons, portraits, window layouts) and animations (KFM).
Below each description, quick buttons lead straight into a sub-area (e.g. New map, Items, Interface, Animations).
NOTHING is saved automatically. Every change first exists only in memory; the respective Save buttons write the files. Modified SHN files are marked with * in the lists.
Make a backup of your client and server folders before the first save.)MAN",
     "overview start project"},
    {"start.project", "start",
     "Projekt einrichten (Client- und Server-Ordner)", "Setting up the project (client and server folders)",
     R"MAN(Im Projekt-Hub gibst Du den Client-Ordner (enthält ressystem, resmap, reschar, resitem ...) und den Server-Ordner (enthält 9Data/Shine) an. Alles Weitere wird automatisch abgeleitet:
- ressystem (Client-SHN) und Server/9Data/Shine (Server-SHN und Textdateien wie World/NPC.txt, MobRegen, NPCItemList).
- reschar (Charaktermodelle inklusive NPC-Modelle) und resitem (Waffen und Item-Modelle).
- resmap (Karten, Objekt-Modelle).
Findet der Editor einen Ordner nicht, steht das im jeweiligen Bereich (z.B. 'Ordner reschar nicht gefunden'). Prüfe dann die Pfade im Projekt-Hub. Die Ordner können beliebig tief liegen; gesucht wird bis 3 Ebenen tief.)MAN",
     R"MAN(In the project hub you enter the client folder (containing ressystem, resmap, reschar, resitem ...) and the server folder (containing 9Data/Shine). Everything else is derived automatically:
- ressystem (client SHN) and Server/9Data/Shine (server SHN and text files such as World/NPC.txt, MobRegen, NPCItemList).
- reschar (character models including NPC models) and resitem (weapons and item models).
- resmap (maps, object models).
If the editor cannot find a folder, the respective area says so (e.g. 'folder reschar not found'). Then check the paths in the project hub. Folders may be nested; the search goes up to 3 levels deep.)MAN",
     "project folder ordner client server pfad"},
    {"start.language", "start",
     "Sprache, Tooltips und Handbuch", "Language, tooltips and manual",
     R"MAN(- Oben rechts wählst Du die Sprache (Deutsch/Englisch). Sie gilt für die Oberfläche, die Tooltips und dieses Handbuch. Die englischen Texte sind nicht von einem Muttersprachler geprüft.
- Tooltips: Bewegst Du die Maus kurz über einen Knopf, ein Eingabefeld oder eine Auswahl, erscheint eine Erklärung.
- Handbuch: Taste F1 oder der Knopf '?' oben rechts. Links stehen die Kapitel, oben die Suche. Die Suche findet Wörter in Titeln, Texten und Stichworten.)MAN",
     R"MAN(- At the top right you choose the language (German/English). It applies to the interface, the tooltips and this manual. The English texts have not been checked by a native speaker.
- Tooltips: hold the mouse over a button, input field or selection for a moment and an explanation appears.
- Manual: key F1 or the '?' button at the top right. Chapters are on the left, search on top. Search finds words in titles, texts and keywords.)MAN",
     "language sprache tooltip hilfe handbuch f1"},
    {"start.saving", "start",
     "Speichern, Rückgängig und Sicherheit", "Saving, undo and safety",
     R"MAN(- Karten: 'Karte speichern' schreibt die Karte im Originalformat (Legacy-Dateien: Heightmap, Texturen, Block&Walk, Objekte) in das Ausgabeverzeichnis. Die Kartendateien sind so gebaut, dass unveränderte Teile byte-identisch bleiben.
- NPCs, Mobs, Portale, Shops: eigene Speichern-Knöpfe im jeweiligen Bereich schreiben World/NPC.txt, MobRegen, RecallCoord.txt, NPCItemList/<NPC>.txt, TownPortal.shn.
- SHN-Tabellen: 'Speichern' (Single-Editor) bzw. 'Alle geänderten SHN speichern' (Assistenten) schreiben jede geänderte Tabelle an ihren Pfad zurück.
- Rückgängig/Wiederholen: gibt es für Höhenkarte, Texturen und Block&Walk (Strg+Z / Strg+Y, siehe Tastenkürzel). Löschen von Zeilen/Zonen/Tabs ist nur nach Freigabe-Haken möglich.
- Vor dem Überschreiben werden keine Sicherungen angelegt.)MAN",
     R"MAN(- Maps: 'Save map' writes the map in its original format (legacy files: heightmap, textures, Block&Walk, objects) to the output directory. Unchanged parts of the map files stay byte-identical.
- NPCs, mobs, portals, shops: their own Save buttons in the respective area write World/NPC.txt, MobRegen, RecallCoord.txt, NPCItemList/<NPC>.txt, TownPortal.shn.
- SHN tables: 'Save' (single editor) or 'Save all changed SHN' (wizards) write every changed table back to its path.
- Undo/redo: available for heightmap, textures and Block&Walk (Ctrl+Z / Ctrl+Y, see shortcuts). Deleting rows/zones/tabs is only possible after ticking the release checkbox.
- No backups are created before overwriting.)MAN",
     "save speichern undo rückgängig backup sicherung"},
    {"controls.view2d", "controls",
     "2D-Ansicht (Draufsicht)", "2D view (top view)",
     R"MAN(Die 2D-Ansicht zeigt die Karte von oben, Norden ist oben.
- Mausrad: Zoom um den Mauszeiger (1x bis 40x). Der Kartenpunkt unter dem Zeiger bleibt stehen.
- Mittlere oder rechte Maustaste ziehen: Ausschnitt verschieben.
- Knöpfe oben rechts: + / - zoomen, 1:1 zeigt die ganze Karte. Unten links steht der Zoomfaktor.
- Linke Maustaste: je nach Werkzeug malen (Höhe, Textur, Block&Walk) oder platzieren/auswählen (Objekte, NPCs, Mobs, Portale).
- Marker: Objekte = Punkte mit Grundfläche, NPCs = Quadrate mit gelber Blicklinie, Mob-Zonen = Symbol mit Radius, Portale = Raute/Dreieck/Quadrat/Ring.
Die Auswahl-Toleranz schrumpft mit dem Zoom - für genaues Treffen einfach näher heranzoomen.)MAN",
     R"MAN(The 2D view shows the map from above, north is up.
- Mouse wheel: zoom around the mouse pointer (1x to 40x). The map point under the pointer stays in place.
- Drag with middle or right mouse button: pan the view.
- Buttons at the top right: + / - zoom, 1:1 shows the whole map. The zoom factor is at the bottom left.
- Left mouse button: paint (height, texture, Block&Walk) or place/select (objects, NPCs, mobs, portals), depending on the tool.
- Markers: objects = dots with footprint, NPCs = squares with a yellow facing line, mob zones = symbol with radius, portals = diamond/triangle/square/ring.
The selection tolerance shrinks with the zoom - to hit precisely just zoom in closer.)MAN",
     "2d zoom pan draufsicht mausrad"},
    {"controls.view3d", "controls",
     "3D-Ansicht (Kamera)", "3D view (camera)",
     R"MAN(Die Kamera arbeitet wie in einem Level-Editor. Standard ist die Unreal-Navigation (umschaltbar unter Einstellungen, im Menü Ansicht oder im Ansicht-Menü der Viewport-Leiste):
- Rechte Maustaste halten + Maus bewegen: umsehen (die Kamera bleibt stehen, Du drehst Dich).
- Rechte Maustaste halten + W / A / S / D: fliegen; Q / E: runter / hoch. Mausrad bei gehaltener rechter Taste: Kameratempo 1..8.
- Pfeiltasten: bewegen, auch ohne Maustaste.
- Linke Maustaste ziehen: vor/zurück und drehen. Alt + linke Maustaste: um das Ziel kreisen. Alt + rechte Maustaste: Dolly (näher/weiter).
- Mittlere Maustaste ziehen: Ansicht schieben. Mausrad: Zoom, bis dicht an den Punkt heran. Die Kamera schneidet dabei nichts Nahes ab.
- Shift: 4-fach schneller. Strg: langsam (0,2-fach). Das Grundtempo wächst mit der Entfernung und mit der Tempo-Stufe.
- Knöpfe + / - unten rechts: Zoom. 'Kamera zentrieren' (Werkzeugleiste): ganze Karte.
Klassische Navigation: W / A / S / D wirken, sobald die Maus über dem 3D-Bild ist; linke Maustaste ziehen kreist um das Ziel.
Die Tasten wirken nur, solange kein Textfeld aktiv ist.)MAN",
     R"MAN(The camera works like in a level editor. The default is Unreal navigation (switchable in Settings, in the View menu or in the view menu of the viewport toolbar):
- Hold the right mouse button + move the mouse: look around (the camera stays in place, you turn).
- Hold the right mouse button + W / A / S / D: fly; Q / E: down / up. Mouse wheel while holding the right button: camera speed 1..8.
- Arrow keys: move, also without a mouse button.
- Drag with the left mouse button: forward/back and turn. Alt + left mouse button: orbit around the target. Alt + right mouse button: dolly (closer/farther).
- Drag with the middle mouse button: pan the view. Mouse wheel: zoom, up to very close to the point. Nothing nearby is clipped.
- Shift: 4 times faster. Ctrl: slow (0.2 times). The base speed grows with the distance and the speed level.
- Buttons + / - at the bottom right: zoom. 'Center camera' (toolbar): whole map.
Classic navigation: W / A / S / D work whenever the mouse is over the 3D image; dragging with the left mouse button orbits the target.
The keys only work while no text field is active.)MAN",
     "3d kamera wasd rechte maustaste umsehen fly unreal navigation tempo speed orbit dolly"},
    {"controls.keys", "controls",
     "Tastenkürzel im Überblick", "Shortcut overview",
     R"MAN(# Allgemein
- F1: Handbuch öffnen/schließen
- Strg+Z: Rückgängig (Höhenkarte, Texturen oder Block&Walk - je nach aktivem Werkzeug)
- Strg+Y oder Strg+Shift+Z: Wiederholen
# 3D-Ansicht
- Rechte Maustaste + Maus: umsehen
- W A S D / Pfeile: laufen; Q E / Leertaste: runter/hoch
- Shift: schnell; Strg: langsam
- Mausrad: Zoom; Mitte ziehen: schieben; Alt+Links ziehen: kreisen
- W / E / R oder Leertaste: Gizmo Verschieben / Rotieren / Skalieren (Unreal-Navigation)
- Strg+0..9: Kamera-Lesezeichen setzen; 0..9: anspringen
- F: Auswahl fokussieren; Ende: auf den Boden setzen; G: Spielansicht
- H: Auswahl ausblenden; Shift+H: Auswahl isolieren; Strg+H: alle einblenden
- Strg+Alt+Links ziehen: Rahmenauswahl (Shift ergänzt); Alt+Ziehen am Gizmo: duplizieren
- V beim Ziehen des Verschieben-Gizmos: Vertex-Snap (Mesh-Ecke bzw. HTD-Vertex unter dem Zeiger)
- Alt + mittlere Maustaste: Pivot frei setzen; Alt + mittlerer Doppelklick: Pivot zurücksetzen
- Alt+P: Spieltest starten/beenden; Esc: Spieltest beenden bzw. Auswahl aufheben
# 2D-Ansicht
- Mausrad: Zoom um den Zeiger; Mitte oder Rechts ziehen: verschieben
# SHN-Tabellen
- Einzelklick: Zelle wählen; Doppelklick: Zelle bearbeiten
- Enter im Zellfeld oder 'Übernehmen': Wert anwenden)MAN",
     R"MAN(# General
- F1: open/close the manual
- Ctrl+Z: undo (heightmap, textures or Block&Walk - depending on the active tool)
- Ctrl+Y or Ctrl+Shift+Z: redo
# 3D view
- Right mouse button + mouse: look around
- W A S D / arrows: walk; Q E / space: down/up
- Shift: fast; Ctrl: slow
- Mouse wheel: zoom; drag middle: pan; Alt+drag left: orbit
- W / E / R or space: gizmo move / rotate / scale (Unreal navigation)
- Ctrl+0..9: set camera bookmark; 0..9: jump to it
- F: focus selection; End: drop to ground; G: game view
- H: hide selection; Shift+H: isolate selection; Ctrl+H: show all
- Ctrl+Alt+drag left: marquee selection (Shift adds); Alt+drag on the gizmo: duplicate
- V while dragging the move gizmo: vertex snap (mesh corner or HTD vertex under the pointer)
- Alt + middle mouse button: place the pivot freely; Alt + middle double-click: reset the pivot
- Alt+P: start/stop playtest; Esc: stop playtest or clear selection
# 2D view
- Mouse wheel: zoom around the pointer; drag middle or right: pan
# SHN tables
- Single click: select cell; double click: edit cell
- Enter in the cell field or 'Apply': apply the value)MAN",
     "tastenkürzel shortcuts tasten keys hotkeys strg ctrl"},
    {"map.open", "map",
     "Karte öffnen, neu anlegen, speichern", "Open, create and save a map",
     R"MAN(- Karte öffnen: im Map-Editor-Start den Pfad der Karten-.ini angeben (z.B. resmap/field/Rou/Rou.ini) und 'Karte öffnen'. Der Editor liest Heightmap (.HTD), Texturen (.ini + BMPs), Block&Walk (.shbd) und Objekte (.shmd/.idm/.aid) automatisch.
- Neue Karte: 'Neue Karte' legt eine Karte an (z.B. 257x257 Blöcke). Sie startet flach und ganz gesperrt.
- Karte speichern: Ausgabeverzeichnis und Kartenname angeben; 'Karte speichern' schreibt alle Legacy-Dateien.
- 'Erweitert (Fiesta Import/Export)': einzelne Fiesta-Dateien getrennt importieren/exportieren.
# Große Karten
Bei großen Karten (z.B. Adelia 951x476 Blöcke) deckt jede Blend-Textur laut .ini nur eine Region ab. Der Editor zeichnet das korrekt (bis zu 24 Layer). Das Block&Walk-Gitter ist dabei immer quadratisch (Zelle = 6,25 Einheiten) und deckt die längere Kartenseite ab.)MAN",
     R"MAN(- Open a map: in the map editor start enter the path of the map .ini (e.g. resmap/field/Rou/Rou.ini) and 'Open map'. The editor reads heightmap (.HTD), textures (.ini + BMPs), Block&Walk (.shbd) and objects (.shmd/.idm/.aid) automatically.
- New map: 'New map' creates a map (e.g. 257x257 blocks). It starts flat and fully blocked.
- Save map: enter output directory and map name; 'Save map' writes all legacy files.
- 'Advanced (Fiesta import/export)': import/export individual Fiesta files.
# Large maps
On large maps (e.g. Adelia 951x476 blocks) each blend texture covers only one region according to the .ini. The editor draws this correctly (up to 24 layers). The Block&Walk grid is always square (cell = 6.25 units) and covers the longer side of the map.)MAN",
     "karte map öffnen speichern neu ini htd shbd shmd"},
    {"map.heightmap", "map",
     "Tab Hightmap (Höhenkarte)", "Heightmap tab",
     R"MAN(Höhenkarte bearbeiten. Halte die linke Maustaste in der 2D-Ansicht oder im 3D-Viewport und fahre über das Terrain (im 3D-Viewport dreht Alt+LMB weiterhin die Kamera).
- Anheben / Absenken: Gelände hoch- oder herunterziehen.
- Glätten: gleicht Höhen an die Umgebung an. Schärfen: Gegenteil, betont Kanten und Spitzen.
- Einebnen: setzt das Gelände auf die 'Zielhöhe' - standardmäßig die Höhe beim Strichbeginn; wahlweise nur anheben oder nur absenken.
- Rauschen: natürliche Unebenheiten (Wellenlänge, Seed; Stärke = Amplitude).
- Terrassen: zieht Höhen auf Stufen (Stufenhöhe).
- Erosion: Material rutscht von zu steilen Stellen bergab, das Volumen bleibt gleich.
- Rampe: erster Klick Start, zweiter Klick Ende - ebene Steigung, Breite = Radius.
- Falloff: Weich, Linear, Kugel, Spitz oder Konstant.
- Radius: Pinselgröße in Welteinheiten. Stärke: wie stark pro Zug.
- Rückgängig / Wiederholen: nehmen ganze Pinselzüge zurück.
Unten stehen Gittergröße und Höhenbereich. Objekte und NPCs folgen der Höhe beim Neuberechnen ihrer Position; gespeicherte Objekthöhen ändern sich dadurch nicht automatisch.
Höhendaten austauschen (Datei & Fiesta Import/Export): 'Exportieren' schreibt die Heightmap als 16-Bit-Graustufen-PNG oder RAW R16 (.r16/.raw, little endian, quadratisch) für World Machine, Gaea oder Photoshop; der Höhenbereich wird auf 0..65535 abgebildet und im PNG mitgeschrieben. 'Importieren' liest PNG (8/16 Bit) oder R16 mit dem angegebenen Bereich Min..Max ('Bereich aus Datei' übernimmt ihn aus einer vom Editor exportierten PNG), rechnet bei anderer Größe bilinear um und ist ein Undo-Schritt. Bildoberkante = Norden.
Weg (Spline): 'Punkte setzen' anhaken und in 2D/3D klicken - der Weg erscheint gelb im 2D-View. 'Terrain entlang ebnen' legt einen Weg der eingestellten Breite mit weichem Rand an; die Weghöhe folgt geglättet dem Gelände. 'Layer malen' malt den gewählten Layer entlang des Wegs, 'Objekte entlang setzen' setzt ein Modell im gewählten Abstand (Mitte, links, rechts oder beidseitig, entlang der Wegrichtung gedreht) - z.B. Zäune oder Laternen. Jede Aktion ist ein Undo-Schritt.)MAN",
     R"MAN(Edit the heightmap. Hold the left mouse button in the 2D view or the 3D viewport and move over the terrain (in the 3D viewport Alt+LMB still orbits the camera).
- Raise / Lower: pull the terrain up or down.
- Smooth: blends heights with the surroundings. Sharpen: the opposite, emphasizes edges and peaks.
- Flatten: sets the terrain to the 'target height' - by default the height at the start of the stroke; optionally raise only or lower only.
- Noise: natural unevenness (wavelength, seed; strength = amplitude).
- Terrace: pulls heights to steps (step height).
- Erode: material slides down from spots that are too steep; volume is preserved.
- Ramp: first click start, second click end - even incline, width = radius.
- Falloff: smooth, linear, spherical, tip or constant.
- Radius: brush size in world units. Strength: how strong per stroke.
- Undo / Redo: revert whole brush strokes.
Below you find grid size and height range. Objects and NPCs follow the height when their position is recalculated; saved object heights do not change automatically.
Exchange height data (File & Fiesta import/export): 'Export' writes the heightmap as a 16-bit grayscale PNG or RAW R16 (.r16/.raw, little endian, square) for World Machine, Gaea or Photoshop; the height range is mapped to 0..65535 and stored in the PNG. 'Import' reads PNG (8/16 bit) or R16 with the given range Min..Max ('Range from file' takes it from a PNG the editor exported), resamples bilinearly if the size differs and is one undo step. Top edge of the image = north.
Path (spline): tick 'Place points' and click in 2D/3D - the path appears in yellow in the 2D view. 'Flatten terrain along path' builds a path of the set width with a soft edge; the path height follows the terrain, smoothed. 'Paint layer' paints the chosen layer along the path, 'Place objects along path' places a model at the chosen spacing (centre, left, right or both, rotated along the path) - e.g. fences or lanterns. Each action is one undo step.)MAN",
     "höhenkarte heightmap gelände anheben absenken glätten einebnen rauschen noise terrassen erosion rampe schärfen falloff"},
    {"map.texturing", "map",
     "Tab Map Texturen", "Map textures tab",
     R"MAN(Jede Karte hat mehrere Texturschichten (Layer). Jeder Layer hat eine Diffuse-Textur (Bild), einen UV-Scale (Kachelgröße) und ein Gewicht je Kartenpunkt (Blend), das Du malst.
- Layer wählen (Liste), dann in der 2D-Ansicht oder im 3D-Viewport malen: 'Erhöhen' verstärkt den Layer, 'Senken' schwächt ihn. Jeder Layer hat seine eigene Maske; Malen ändert nur den gewählten Layer. Die Layer liegen in Listenreihenfolge übereinander (spätere über früheren), wie im Spiel.
- Radius / Stärke: Pinsel.
- Layer hinzufügen: Name, Diffuse-Datei ('Durchsuchen...') und UV-Scale angeben. 'Layer entfernen' löscht den gewählten Layer.
- Rückgängig / Wiederholen für Textur-Pinselzüge.
Einzelne Layer lassen sich im Bereich 'Sichtbarkeit' ein- und ausblenden, ohne die Daten zu ändern.
- Gewählten Layer sperren (auch im Kontextmenü des Layer-Docks): Pinsel und Regeln ändern ihn dann nicht.
- Nach Regeln texturieren: der gewählte Layer bekommt Gewicht, wo Neigung (z.B. 35-90° für Fels) und/oder Höhe im Bereich liegen, mit weichem Übergang. 'Nur hinzufügen' malt dazu, 'Ersetzen' überschreibt den Layer. 'Auf ganze Karte anwenden' ist ein Undo-Schritt; mit 'Pinsel malt nach dieser Regel' wirkt die Regel nur unter dem Pinsel.
- Pinselform aus Bild (auch beim Terrain): ein Graustufen-PNG (1-16 Bit, auch Palette) oder R16 ersetzt den Falloff; weiß = volle Wirkung, Bildoberkante = Norden.)MAN",
     R"MAN(Every map has several texture layers. Each layer has a diffuse texture (image), a UV scale (tile size) and a weight per map point (blend) that you paint.
- Select a layer (list), then paint in the 2D view or the 3D viewport: 'Increase' strengthens the layer, 'Decrease' weakens it. Every layer has its own mask; painting changes only the selected layer. Layers lie on top of each other in list order (later over earlier), as in the game.
- Radius / Strength: brush.
- Add layer: enter name, diffuse file ('Browse...') and UV scale. 'Remove layer' deletes the selected layer.
- Undo / Redo for texture strokes.
Individual layers can be shown/hidden in the 'Visibility' area without changing the data.
- Lock selected layer (also in the Layer dock context menu): brushes and rules then leave it unchanged.
- Texture by rules: the selected layer gets weight where slope (e.g. 35-90° for rock) and/or height are in range, with a soft transition. 'Add only' paints on top, 'Replace' overwrites the layer. 'Apply to whole map' is one undo step; with 'Brush paints by this rule' the rule only acts under the brush.
- Brush shape from image (also for terrain): a grayscale PNG (1-16 bit, palette too) or R16 replaces the falloff; white = full effect, top edge = north.)MAN",
     "textur layer blend diffuse uv scale malen"},
    {"map.walk", "map",
     "Tab Walk & Block (Begehbarkeit)", "Walk & Block tab",
     R"MAN(Das Block&Walk-Gitter besteht aus Zellen von 6,25 Welteinheiten. Eine Zelle ist entweder blockiert (im Overlay rot) oder begehbar. Server-Punkte (NPCs, Wegpunkte) liegen immer auf begehbaren Zellen.
- Sperren / Freigeben: welche Art der Änderung der Pinsel macht.
- Radius: Pinselgröße; '1 Zelle' trifft nur die Zelle unter dem Zeiger.
- Aus Objekten: 'Grundflächen sichtbarer Objekte SPERREN/FREIGEBEN' stempelt die Grundfläche aller gerade sichtbaren Objekte (siehe Sichtbarkeit: z.B. nur 'Gebäude' einblenden) ins Gitter. Ein Klick lässt sich mit Rückgängig zurücknehmen.
- Aus Terrain und Objekten neu berechnen: sperrt Hänge steiler als 'Max. Neigung' (Standard 30°; auf Roumen sind 98 % solcher Zellen im Original blockiert), optional alles über 'Max. Höhe' oder unter 'Min. Höhe' (z.B. Wasser), und die Grundflächen blockierender Objekte. 'Nur Geändertes' rechnet nur dort neu, wo sich Terrain oder Objekte seit dem Laden geändert haben, und gibt nur Zellen frei, die wegen Hang oder Objekt gesperrt waren - von Hand gesperrtes Meer oder Wege bleiben. 'Ganze Karte' ersetzt das Gitter (für neue Karten). 'Vorschau berechnen' zeigt im 2D-View gelb = neu blockiert, grün = neu begehbar; 'Übernehmen' ist ein Undo-Schritt.
- Rückgängig / Wiederholen (Walk).
Objekte erscheinen als Referenz mit ihrer Grundfläche; sie sind in diesem Werkzeug nicht anklickbar, damit man nichts versehentlich verschiebt.)MAN",
     R"MAN(The Block&Walk grid consists of cells of 6.25 world units. A cell is either blocked (red in the overlay) or walkable. Server points (NPCs, waypoints) always lie on walkable cells.
- Block / Release: which kind of change the brush makes.
- Radius: brush size; '1 cell' only hits the cell under the pointer.
- From objects: 'Block/Release footprints of visible objects' stamps the footprint of all currently visible objects (see Visibility: e.g. show only 'Buildings') into the grid. One click can be reverted with Undo.
- Recompute from terrain and objects: blocks slopes steeper than 'Max. slope' (default 30°; on Roumen 98 % of such cells are blocked in the original), optionally everything above 'Max. height' or below 'Min. height' (e.g. water), and the footprints of blocking objects. 'Changes only' recomputes only where terrain or objects changed since loading and frees only cells that were blocked by slope or an object - hand-made blocks such as sea or paths stay. 'Whole map' replaces the grid (for new maps). 'Compute preview' shows yellow = newly blocked, green = newly walkable in the 2D view; 'Apply' is one undo step.
- Undo / Redo (Walk).
Objects appear as a reference with their footprint; they cannot be clicked in this tool so nothing is moved by accident.)MAN",
     "walk block begehbar gitter zelle sperren freigeben grundfläche neu berechnen hang neigung slope recompute shbd"},
    {"map.objects", "map",
     "Tab Objekt Platzierung", "Object placement tab",
     R"MAN(Objekte sind 3D-Modelle (Häuser, Bäume, Steine ...) mit Position, Drehung und Skalierung.
- Platzieren: Modellpfad eintragen oder 'Durchsuchen...', Rotation und Skalierung einstellen, dann in die 2D-Ansicht klicken.
- Mehrfachauswahl: Strg+Klick fügt einzelne Objekte hinzu/entfernt sie, Shift+Klick wählt einen Bereich in der Liste. In der 2D-Ansicht wählt Shift+Ziehen per Rechteck, Alt+Shift+Ziehen per Lasso; mit Strg wird die bestehende Auswahl erweitert. 'Alle normalen auswählen' bzw. Strg+A markiert alle normalen Placements.
- Bewegen: Ein bereits ausgewähltes Objekt in der 2D-Ansicht mit links ziehen. Bei Mehrfachauswahl bewegt sich die ganze Gruppe gemeinsam; relative Abstände und Y-Höhen bleiben erhalten.
- Koordinaten: X/Y/Z des aktiven Objekts direkt ändern. Bei Mehrfachauswahl wird dieselbe Positionsdifferenz auf alle ausgewählten Objekte angewandt. Rotation und Skalierung wirken ebenfalls relativ auf die Gruppe.
- Szene-Outliner: Rechtsklick auf ein Objekt erlaubt ein freies Editor-Label und eine Gruppe/Ordner-Zuordnung. Gruppen erscheinen einklappbar; diese Organisation ist reine Editor-Metadaten und verändert die Fiesta-Dateien nicht.
- 'Ausgewählte Objekte löschen' löscht die Auswahl. 'Alle normalen Objekte entfernen' entfernt nach Bestätigung alle normalen Placements, lässt Sky/Water/GroundObject aber bestehen.
- Die Höhe wird beim Platzieren aus dem Gelände genommen.
- Im 3D-Bild siehst Du die echten Modelle. Ausgewählte normale Objekte erhalten zusätzlich einen weißen Auswahl-Marker.
Objekte lassen sich nach Kategorien (Bäume, Gebäude, Felsen ...) ein- und ausblenden (Bereich Sichtbarkeit) - hilfreich bei dichten Karten.
Malen (Foliage): Modelle über 'Modellpfad oben hinzufügen' in die Liste aufnehmen, dann im 2D- oder 3D-View ziehen. Jede Anwendung versucht 'Dichte' Objekte je 100x100 Einheiten und hält den Mindestabstand zu Objekten der Liste ein - wiederholtes Malen füllt die Fläche, aber nicht darüber hinaus. Zufällige Drehung und Skalierung, 'Max. Neigung' (steilere Stellen bleiben frei), 'Am Hang ausrichten', Höhenversatz. Strg oder 'Löschen statt malen' entfernt Objekte der Liste unter dem Pinsel. Ein Strich ist ein Undo-Schritt.
Prefabs: ausgewählte Objekte mit Namen als Prefab speichern (<Projekt>/Prefabs/<Name>.prefab, nur für den Editor). Ein Prefab in der Liste anklicken und in 2D oder 3D klicken setzt die Gruppe ein - mit 'Drehung', die Höhen folgen dem Terrain (gespeichert wird die Höhe über dem Boden). Prefabs stehen auf jeder Karte des Projekts zur Verfügung; Einsetzen ist ein Undo-Schritt.
Animationen: Bewegungen, die im NIF selbst stecken (Windmühlenflügel, schaukelnde Boote, Schilder, Leuchtturm-Licht), laufen im 3D-Bild mit - wie im Spiel als Endlosschleife. Abschalten unter 'Anzeigen > Objekt-Animationen'; dann stehen alle Objekte in der Ruhepose. Auswahl und Klick-Treffer verwenden immer die Ruhepose.)MAN",
     R"MAN(Objects are 3D models (houses, trees, stones ...) with position, rotation and scale.
- Place: enter a model path or 'Browse...', set rotation and scale, then click into the 2D view.
- Multi-select: Ctrl+click adds/removes individual objects, Shift+click selects a range in the list. In the 2D view, Shift+drag selects by rectangle and Alt+Shift+drag selects by lasso; hold Ctrl to extend the existing selection. 'Select all normal' or Ctrl+A selects all normal placements.
- Move: left-drag an already selected object in the 2D view. With multiple objects selected, the whole group moves together while relative spacing and Y heights are preserved.
- Coordinates: edit X/Y/Z of the active object directly. With multiple objects selected, the same position delta is applied to every selected object. Rotation and scale are also applied relatively to the group.
- Scene Outliner: right-click an object to assign a free editor label and a group/folder. Groups are collapsible; this organization is editor-only metadata and does not change Fiesta files.
- 'Delete selected objects' deletes the selection. 'Remove all normal objects' removes all normal placements after confirmation while keeping Sky/Water/GroundObject.
- Height is taken from the terrain when placing.
- In the 3D image you see the real models. Selected normal objects also get a white selection marker.
Objects can be shown/hidden by category (trees, buildings, rocks ...) in the Visibility area - helpful on dense maps.
Paint (foliage): add models to the list with 'Add model path above', then drag in the 2D or 3D view. Each application tries 'Density' objects per 100x100 units and keeps the minimum distance to objects of the list - repeated painting fills the area but not beyond. Random rotation and scale, 'Max. slope' (steeper spots stay empty), 'Align to slope', height offset. Ctrl or 'Erase instead of paint' removes objects of the list under the brush. One stroke is one undo step.
Prefabs: save selected objects under a name as a prefab (<Project>/Prefabs/<name>.prefab, editor only). Click a prefab in the list and then in 2D or 3D to place the group - with 'Rotation'; heights follow the terrain (the height above ground is stored). Prefabs are available on every map of the project; placing is one undo step.
Animations: motion stored in the NIF itself (windmill blades, rocking boats, signs, lighthouse light) plays in the 3D view, looping as in the game. Turn it off under 'Show > Object animations'; all objects then stay in their rest pose. Selection and click hits always use the rest pose.)MAN",
     "objekte platzieren modell nif rotation skalierung löschen"},
    {"map.visibility", "map",
     "Bereich Sichtbarkeit", "Visibility area",
     R"MAN(Im Werkzeug-Panel steht der Bereich 'Sichtbarkeit'. Er ändert nur die Anzeige, nie die Daten.
- Terrain, Objekt-Modelle, Objekt-Platzhalter, NPC-Modelle, Objekte im 2D-View: je ein Schalter.
- SHMD-Szenenmodelle: Sky, Water und GroundObject lassen sich getrennt ein-/ausblenden und erscheinen zusätzlich in der normalen Objektliste. Modellpfad und Löschen bearbeiten den Kategorieeintrag direkt. Sobald Position, Rotation oder Skalierung geändert wird, wandelt der Editor den Eintrag automatisch in ein normales Placement um, weil das SHMD-Kategoriefeld selbst keine Transform-Daten besitzt.
- Terrain-Layer: jede Texturschicht einzeln ausblenden ('Alle Layer an' setzt alles zurück).
- Objekt-Kategorien: Bäume & Büsche, Gras & Blumen, Felsen & Steine, Gebäude, Zäune/Mauern/Brücken, Dekoration & Möbel, Wasser & Schiffe, Tiere & Kreaturen, Effekte & Licht, Sonstiges. Mit der Anzahl je Kategorie; 'nur' zeigt allein diese Kategorie, 'Alle an'/'Alle aus' schalten alle.
- Die Kategorie wird aus dem Modellnamen abgeleitet (Schlüsselwörter) und kann daher selten danebenliegen.
- 'NPC-Namen und Blickpfeile': Namen und Blickrichtung der NPCs im 3D-Bild (NPC-Modus).
- Fehlt NPCs das Modell, werden sie hier namentlich aufgelistet.)MAN",
     R"MAN(The tool panel contains the 'Visibility' area. It only changes the display, never the data.
- Terrain, object models, object placeholders, NPC models, objects in the 2D view: one switch each.
- SHMD scene models: Sky, Water and GroundObject can be shown/hidden independently and also appear in the normal object list. Model-path changes and deletion edit the category entry directly. As soon as position, rotation or scale changes, the editor automatically converts the entry to a normal placement because the SHMD category field itself has no transform data.
- Terrain layers: hide each texture layer individually ('All layers on' resets).
- Object categories: trees & bushes, grass & flowers, rocks & stones, buildings, fences/walls/bridges, decoration & furniture, water & ships, animals & creatures, effects & light, other. With the count per category; 'only' shows just this category, 'All on'/'All off' switch all.
- The category is derived from the model name (keywords) and can therefore rarely be off.
- 'NPC names and facing arrows': names and facing direction of NPCs in the 3D image (NPC mode).
- If NPCs lack a model they are listed here by name.)MAN",
     "sichtbarkeit visibility ausblenden einblenden kategorie layer"},
    {"map.environment", "map",
     "Umgebung: Licht, Nebel, Himmel", "Environment: light, fog, sky",
     R"MAN(Der Tab 'Umgebung' (links neben Szene, Layer und Sichtbarkeit) bearbeitet die Szene-Werte der <Karte>.shmd:
- GlobalLight: Umgebungslicht, das jede Fläche erhält. DirectionLightAmbient kommt im Editor-Renderer hinzu.
- DirectionLightDiffuse: Farbe des gerichteten Lichts (Sonne). Die Richtung steht in keiner Kartendatei; der Editor nimmt waagerechtes Licht an (siehe docs/RENDERER.md).
- Nebeltiefe, Nebelfarbe und Sichtweite (Fog und Frustum): Der Nebel beginnt bei Sichtweite × (1 − Tiefe) und ist an der Sichtweite voll. Diese Lesart ist eine dokumentierte Näherung, nicht aus dem Client belegt. In vielen Originalen ist die Nebelfarbe gleich der Hintergrundfarbe; 'Nebelfarbe = Hintergrund' übernimmt sie.
- Hintergrundfarbe (BackGroundColor): Himmelsfarbe hinter allen Modellen.
- Sky / Water / GroundObject: die Modelllisten der SHMD. X entfernt einen Eintrag, 'Modell hinzufügen' hängt den Pfad aus dem Eingabefeld an (z. B. resmap\field\Rou\Rou_Sky.nif).
Die 3D-Ansicht zeigt jede Änderung sofort, sofern unter 'Vorschau' Kartenlicht, Hintergrund bzw. Nebel eingeschaltet sind. Ein Reglerzug ist ein Undo-Schritt (Strg+Z). 'Werte beim Öffnen wiederherstellen' setzt alle Szene-Werte auf den Stand beim Laden zurück (ebenfalls rückgängig machbar). Gespeichert wird mit der Karte; in der SHMD ändern sich nur die betroffenen Zeilen. SHMD-Dateien, die nicht in der kanonischen Form vorliegen, werden beim ersten geänderten Speichern kanonisch geschrieben (gleicher Inhalt, 6 Nachkommastellen).)MAN",
     R"MAN(The 'Environment' tab (on the left next to Scene, Layers and Visibility) edits the scene values of <map>.shmd:
- GlobalLight: ambient light every surface receives. DirectionLightAmbient is added in the editor renderer.
- DirectionLightDiffuse: color of the directional light (sun). The direction is not stored in any map file; the editor assumes horizontal light (see docs/RENDERER.md).
- Fog depth, fog color and view distance (Fog and Frustum): fog starts at view distance × (1 − depth) and is full at the view distance. This reading is a documented approximation, not confirmed from the client. In many originals the fog color equals the background color; 'Fog color = background' copies it.
- Background color (BackGroundColor): sky color behind all models.
- Sky / Water / GroundObject: the model lists of the SHMD. X removes an entry, 'Add model' appends the path from the input field (e.g. resmap\field\Rou\Rou_Sky.nif).
The 3D view shows every change immediately if map lighting, background or fog are switched on under 'Preview'. One slider drag is one undo step (Ctrl+Z). 'Restore values at open' resets all scene values to the state at load (also undoable). Saving happens with the map; in the SHMD only the affected lines change. SHMD files that are not in canonical form are written canonically on the first changed save (same content, 6 decimals).)MAN",
     "umgebung environment licht light nebel fog himmel sky hintergrund background frustum sichtweite globallight directionlight water groundobject"},
    {"map.leveltools", "map",
     "Level-Editor-Werkzeuge (Viewport-Leiste, Spieltest, Ausgabe)", "Level editor tools (viewport toolbar, playtest, output log)",
     R"MAN(Die Leiste oben im 3D-Bild bündelt die Level-Editor-Werkzeuge. Alles davon ist reiner Editor-Zustand und verändert keine Fiesta-Datei.
# Viewport-Leiste
- Ansicht: Perspektive oder Achsenansichten (Oben mit Norden oben, von Süden/Norden/Westen/Osten), Viewport-Layout (einzeln, 2 nebeneinander, 4 als 2x2 wie in Unreal: Perspektive/Oben/Süden/Osten), Kamera-Lesezeichen 0..9 und Navigationsart. Jeder Viewport hat eigene Kamera, Ansicht und Anzeigen; ein Klick macht ihn zum aktiven (gelber Rahmen), Kürzel und Gizmo wirken im aktiven Viewport. Die Achsenansichten nutzen die Perspektivkamera.
- Anzeigen: Show Flags für Terrain, NIF-Meshes, Platzhalter, SHMD Sky/Water/GroundObject, NPCs, Routen, Drahtgitter sowie Raster, Achsen-Anzeige, Statistik und SHBD-Kollision um den Mauszeiger.
- Objekte-Modus: Verschieben/Rotieren/Skalieren, World/Local, Pivot (Auswahlmitte oder aktives = zuletzt gewähltes Objekt; Alt+MMB setzt ihn frei), 'Boden' (Surface Snap), 'Objekt' (Actor Snap: rastet am Pivot anderer Objekte im Raster-Snap-Radius ein), Raster-, Winkel- und Skalierungs-Snap. Die Raster-Presets sind Fiesta-Einheiten: 6,25 = eine SHBD-Walk-Zelle, 50 = ein HTD-Block.
- Surface Snap und 'Auf den Boden setzen' im Spieltest beachten das HTD-Terrain UND die SHMD-Kategorie GroundObject (z.B. das Pflaster von Roumen liegt über dem HTD).
- Tempo 1..8, G (Spielansicht: alle Editor-Helfer aus) und Spieltest.
- Anzeigen -> Darstellung: Ansichtsmodus (beleuchtet, unbeleuchtet, nur Licht/Vertexfarbe, Normalen), Terrain-Vertexfarben der Karte (#VerTexColorTexture), Glow aus <Karte>.conf und Kantenglättung. Die Achsenansichten sind orthografisch: Ziehen verschiebt, Mausrad zoomt, Drehen kehrt zur Perspektive zurück. Details und Datenherkunft: docs/RENDERER.md.
# Statistik
FPS, Objekt-/Mesh-/NPC-Anzahl, ausgeblendete Objekte, gezeichnete Modellteile und solche außerhalb des Bildes (werden gar nicht erst gezeichnet) samt CPU-Zeit der Objekte, Kameraziel sowie Weltposition unter dem Mauszeiger mit Höhe und SHBD-Zelle (begehbar/blockiert).
# Spieltest
Startet eine Spielfigur direkt im Viewport (Alt+P oder Knopf 'Spieltest'). Spawn: MapInfo.shn RegenX/RegenY der offenen Karte, sonst das Kameraziel; danach die nächste begehbare SHBD-Zelle. W/A/S/D laufen relativ zur Kamera, Shift geht langsam, rechte/linke Maustaste dreht, Mausrad zoomt, C schaltet die Kollision, Esc beendet und stellt die Editor-Kamera wieder her. Die Kollision nutzt das geladene Block&Walk-Gitter (gesetztes Bit = blockiert, 6,25 Einheiten je Zelle) mit 1,5 Zellen Radius und gleitet an Wänden entlang. Laufgeschwindigkeit und Radius sind Editorwerte, keine aus dem Client belegten Konstanten. Ist kein begehbares SHBD geladen, läuft der Spieltest ohne Kollision und meldet das.
# Auswahl und Sichtbarkeit
Strg+Alt+linke Maustaste ziehen wählt per Rahmen; Shift ergänzt. 'Überschneidung' wählt alles, dessen projizierte NIF-Bounds den Rahmen berühren, 'vollständig innen' nur vollständig enthaltene Objekte. H blendet die Auswahl aus, Shift+H isoliert sie, Strg+H blendet alles wieder ein (nur Editor-Sichtbarkeit). Alt beim Ziehen des Verschieben-Gizmos dupliziert die Auswahl an Ort und Stelle; Duplikat und Verschiebung sind ein Undo-Schritt. V beim Ziehen setzt den Pivot auf die nächste Ecke des NIF-Dreiecks unter dem Mauszeiger (andere Objekte, z.B. das Pflaster-GroundObject) oder auf den nächsten HTD-Vertex - ein gelber Kreis zeigt das Ziel.
# Objekte-Menü
Ausrichten (X/Y/Z auf Minimum, Mitte, Maximum oder das aktive = zuletzt gewählte Objekt), Verteilen (gleichmäßige Abstände, Reihenfolge bleibt) und Rotation/Skalierung vom aktiven Objekt übernehmen. Alles ist rückgängig machbar.
# Ausgabe
Das Fenster 'Ausgabe' sammelt alle Statusmeldungen mit Zeit, Kategorie und Schweregrad (Info/Warnung/Fehler), filterbar und kopierbar.)MAN",
     R"MAN(The bar at the top of the 3D image groups the level editor tools. All of it is editor state only and never changes a Fiesta file.
# Viewport toolbar
- View: perspective or axis views (top with north up, from south/north/west/east), viewport layout (single, 2 side by side, 4 as 2x2 like Unreal: perspective/top/south/east), camera bookmarks 0..9 and the navigation scheme. Each viewport has its own camera, view and show flags; a click makes it active (yellow frame), shortcuts and the gizmo act in the active viewport. Axis views use the perspective camera.
- Show: show flags for terrain, NIF meshes, placeholders, SHMD Sky/Water/GroundObject, NPCs, routes, wireframe as well as grid, axis indicator, stats and SHBD collision around the mouse pointer.
- Objects mode: move/rotate/scale, world/local, pivot (selection center or active = last selected object; Alt+MMB places it freely), 'Surface' (surface snap), 'Actor' (actor snap: snaps to other objects' pivots within the grid snap radius), grid, angle and scale snap. Grid presets are Fiesta units: 6.25 = one SHBD walk cell, 50 = one HTD block.
- Surface snap and ground placement in the playtest respect the HTD terrain AND the SHMD GroundObject category (e.g. the Roumen pavement lies above the HTD).
- Speed 1..8, G (game view: all editor helpers off) and playtest.
- Show -> Rendering: view mode (lit, unlit, lighting/vertex color only, normals), the map's terrain vertex colors (#VerTexColorTexture), glow from <map>.conf and anti-aliasing. Axis views are orthographic: dragging pans, the mouse wheel zooms, rotating returns to perspective. Details and data sources: docs/RENDERER.md.
# Stats
FPS, object/mesh/NPC counts, hidden objects, model parts drawn and those outside the view (skipped entirely) with the CPU time spent on objects, camera target and the world position under the mouse pointer with height and SHBD cell (walkable/blocked).
# Playtest
Starts a player character directly in the viewport (Alt+P or the 'Playtest' button). Spawn: MapInfo.shn RegenX/RegenY of the open map, otherwise the camera target; then the nearest walkable SHBD cell. W/A/S/D run relative to the camera, Shift walks, right/left mouse button turns, mouse wheel zooms, C toggles collision, Esc ends and restores the editor camera. Collision uses the loaded block & walk grid (set bit = blocked, 6.25 units per cell) with a 1.5 cell radius and slides along walls. Run speed and radius are editor values, not constants verified from the client. If no walkable SHBD is loaded, the playtest runs without collision and says so.
# Selection and visibility
Ctrl+Alt+left mouse drag selects by marquee; Shift adds. 'Crossing' selects everything whose projected NIF bounds touch the rectangle, 'fully inside' only fully contained objects. H hides the selection, Shift+H isolates it, Ctrl+H shows everything again (editor visibility only). Holding Alt while dragging the move gizmo duplicates the selection in place; duplicate and move are one undo step. Holding V while dragging puts the pivot on the nearest corner of the NIF triangle under the mouse pointer (other objects, e.g. the pavement GroundObject) or on the nearest HTD vertex - a yellow circle shows the target.
# Objects menu
Align (X/Y/Z to minimum, center, maximum or the active = last selected object), distribute (even spacing, order is kept) and match rotation/scale to the active object. Everything can be undone.
# Output log
The 'Output' window collects all status messages with time, category and severity (info/warning/error), filterable and copyable.)MAN",
     "spieltest playtest pie unreal viewport leiste toolbar snap raster grid lesezeichen bookmark ausgabe output log rahmen marquee isolieren isolate ausrichten align verteilen distribute shbd kollision"},
    {"map.npcs", "map",
     "Tab NPC Platzierung", "NPC placement tab",
     R"MAN(NPCs kommen aus World/NPC.txt (Server-Ordner) und gehören zu einer Karte. Wähle einen NPC durch Klick auf sein Quadrat in der 2D-Ansicht.
- Coord-X / Coord-Y: Position in Welteinheiten. Richtung: Blickrichtung in Grad. -15/+15/+90/180 drehen schnell.
- Rolle / Argument: Aufgabe des NPCs (QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate) mit Argument (z.B. Merchant + Item/Weapon/Skill). Händler benutzen NPCItemList/<NPC>.txt.
- Dialog bearbeiten: Begrüßung und Knöpfe des Gesprächs (NpcDialogData).
- KI-Skript (Lua) bearbeiten und Patrouillenroute bearbeiten: Verhalten des NPCs.
- Händler-Inventar bearbeiten: Shop-Editor (Kapitel Shops).
- Kamera zu diesem NPC: die 3D-Kamera springt zum NPC.
- 'World/NPC.txt speichern' schreibt alle NPC-Änderungen.
# NPCs im 3D-Bild
NPC-Modelle werden aus dem Client geladen: reschar/<Name>/<Name>.nif (oder ein anderer res*-Ordner). Spieler-artige NPCs (mit Rüstung) werden aus NPCViewInfo zusammengesetzt. Ein Pfeil zeigt die Blickrichtung.
# Blickrichtung einstellen
Die Zuordnung 'Richtung -> Blickwinkel' ist nicht aus Daten belegt. Stimmt der Pfeil bei einem bekannten NPC nicht mit dem Spiel überein, stelle 'Drehsinn' und 'Blick bei Richtung 0' um (gilt für alle NPCs).)MAN",
     R"MAN(NPCs come from World/NPC.txt (server folder) and belong to a map. Select an NPC by clicking its square in the 2D view.
- Coord-X / Coord-Y: position in world units. Direction: facing in degrees. -15/+15/+90/180 rotate quickly.
- Role / argument: the task of the NPC (QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate) with argument (e.g. Merchant + Item/Weapon/Skill). Merchants use NPCItemList/<NPC>.txt.
- Edit dialog: greeting and buttons of the conversation (NpcDialogData).
- Edit AI script (Lua) and edit patrol route: behaviour of the NPC.
- Edit merchant inventory: shop editor (chapter Shops).
- Camera to this NPC: the 3D camera jumps to the NPC.
- 'Save World/NPC.txt' writes all NPC changes.
# NPCs in the 3D image
NPC models are loaded from the client: reschar/<name>/<name>.nif (or another res* folder). Player-like NPCs (with armor) are assembled from NPCViewInfo. An arrow shows the facing direction.
# Setting the facing direction
The mapping 'direction -> facing angle' is not proven by data. If the arrow does not match the game for a known NPC, change 'Rotation sense' and 'Facing at direction 0' (applies to all NPCs).)MAN",
     "npc platzierung richtung rolle merchant dialog händler blickrichtung"},
    {"map.ai", "map",
     "KI von NPCs und Mobs", "AI of NPCs and mobs",
     R"MAN(Das Verhalten von NPCs und Mobs bearbeitest Du direkt an ihrem Werkzeug: 'KI-Skript (Lua) bearbeiten' und 'Patrouillenroute bearbeiten' im NPC-Werkzeug, 'KI' und 'Route' je Monster im Mob-Werkzeug. Alle Skripte einer Installation zeigt der Datei Editor im Unterreiter 'AI Scripts' (Lua-AIScripts und PineScript-Routen).)MAN",
     R"MAN(You edit the behaviour of NPCs and mobs directly at their tool: 'Edit AI script (Lua)' and 'Edit patrol route' in the NPC tool, 'AI' and 'Route' per monster in the mob tool. All scripts of an installation are shown by the File Editor in the 'AI Scripts' sub-tab (Lua AIScripts and PineScript routes).)MAN",
     "ai ki lua patrouille"},
    {"map.mobs", "map",
     "Tab Mobs (Monster-Spawns)", "Mobs tab (monster spawns)",
     R"MAN(Spawn-Zonen kommen aus MobRegen/<Karte>.txt. Eine Zone hat Position, Radius und eine oder mehrere Monstergruppen.
- Zone wählen: Klick auf das Zonen-Symbol in der 2D-Ansicht. Der Umriss-Kreis zeigt den Spawn-Radius.
- Alle Zonenfelder und Monsterfelder sind editierbar (Anzahl, Respawn, Bereich ...).
- '+ Zone (Kopie der gewählten)': dupliziert die Zone samt Monstern. 'Zone samt Monstern löschen': nur nach Freigabe-Haken.
- '+ Monster hinzufügen': neuer Monstereintrag in der Zone (Name aus MobInfo). 'Monster entfernen'.
- 'KI' und 'Route' je Monster: Verhaltensskript und Wegpunkte (MobRoam).
- 'MobRegen speichern' schreibt die Datei.
Ob der Server per #recordin eingefügte Zeilen wie erwartet verarbeitet, ist ungeprüft.)MAN",
     R"MAN(Spawn zones come from MobRegen/<map>.txt. A zone has a position, radius and one or more monster groups.
- Select a zone: click the zone symbol in the 2D view. The outline circle shows the spawn radius.
- All zone fields and monster fields are editable (count, respawn, range ...).
- '+ Zone (copy of the selected)': duplicates the zone including monsters. 'Delete zone with monsters': only after the release checkbox.
- '+ Add monster': new monster entry in the zone (name from MobInfo). 'Remove monster'.
- 'AI' and 'Route' per monster: behaviour script and waypoints (MobRoam).
- 'Save MobRegen' writes the file.
Whether the server processes lines inserted via #recordin as expected has not been verified.)MAN",
     "mob monster spawn zone mobregen respawn"},
    {"map.portals", "map",
     "Tab Portale", "Portals tab",
     R"MAN(Schnellreise-Ziele: TownPortal.shn (Stadtportal-Ziele) und RecallCoord.txt (Schriftrollen-Ziele).
- Marker: Raute = TownPortal, Dreieck = Schriftrolle, Quadrat = Gate_Town-NPC, Ring = Regenerationspunkt der Karte.
- X / Y: Zielposition. 'Position per Klick im 2D-View setzen' wählt die Position mit der Maus. 'Menü-Gruppe' und 'Mindestlevel' steuern das Menü.
- 'TownPortal-Ziel hier hinzufügen', 'Entfernen', 'Verwerfen und neu laden'.
- Portalbilder: Der Client zeigt in der Stadtportal-Auswahl resmenu/TownPortal/<Karte>.dds (und <Karte>_disable.dds für gesperrte Ziele). Der Bereich 'Portalbilder' zeigt beide (Projekt vor Client). 'Portalbilder erzeugen' nimmt den Ankunftsort aus der eingestellten 'Blickrichtung' auf und schreibt beide Dateien wie die Originale (256x256 DXT5 mit Mipmaps, abgerundete Ecken, graue Variante). Beim Hinzufügen eines Ziels werden fehlende Bilder automatisch erzeugt; die Kartenprüfung warnt, wenn sie fehlen.
- 'TownPortal.shn speichern' und 'RecallCoord.txt speichern'.
Ein TownPortal-Ziel liegt höchstens 205 Einheiten vom zugehörigen Gate_Town-NPC entfernt.)MAN",
     R"MAN(Fast-travel targets: TownPortal.shn (town portal targets) and RecallCoord.txt (scroll targets).
- Markers: diamond = TownPortal, triangle = scroll, square = Gate_Town NPC, ring = regeneration point of the map.
- X / Y: target position. 'Set position by click in 2D view' picks the position with the mouse. 'Menu group' and 'Minimum level' control the menu.
- 'Add TownPortal target here', 'Remove', 'Discard and reload'.
- Portal images: the client's town portal list shows resmenu/TownPortal/<map>.dds (and <map>_disable.dds for locked targets). The 'Portal images' area shows both (project before client). 'Generate portal images' captures the arrival point from the chosen 'View direction' and writes both files like the originals (256x256 DXT5 with mipmaps, rounded corners, grey variant). Adding a target generates missing images automatically; the map check warns when they are missing.
- 'Save TownPortal.shn' and 'Save RecallCoord.txt'.
A TownPortal target is at most 205 units away from the associated Gate_Town NPC.)MAN",
     "portal townportal recall schriftrolle teleport"},
    {"map.newmap", "map",
     "Neue Karte bis ins Spiel", "New map, ready for the game",
     R"MAN(Karte > Neue Karte legt eine Karte an, die alle Einträge für Client und Server mitbringt, und schreibt sie sofort in den Projektordner (die Quellordner bleiben unberührt).
- Kartenname: Ordner-, Datei- und Servername (Buchstabe am Anfang, dann Buchstaben/Ziffern/_, höchstens 15 Zeichen). Anzeigename: der Name im Spiel.
- Vorlage: eine vorhandene Karte, deren Textur-Layer, Licht (.conf, SHMD-Szenenlicht), Himmel und .aid übernommen werden - so zeigen alle Verweise auf Dateien, die im Client existieren. Neu angelegt werden flaches Terrain, frische Masken (Layer 1 voll), ein freies Block&Walk-Gitter, eine weiße Vertex-Color-Bitmap und eine leere Objektliste.
- Größe 129, 257 (12800 x 12800 Einheiten wie Roumen) oder 513 Höhenpunkte; Grundhöhe.
- Spieldaten: MapInfo.shn und MapViewInfo.shn (Client und Server, ID = nächste freie unter 1000, Wiederbelebung in der Kartenmitte), World/Field.txt (Serial = CheckSum = nächste freie Nummer, wie bei allen 153 Originaleinträgen), Server-Begehbarkeit 9Data/Shine/BlockInfo/<Karte>.shbd/.aid.
- Optional ein Gate von einer vorhandenen Karte (Servername und Position) mit Rückweg: zwei ShineNPC-Gates und zwei LinkTable-Einträge in World/NPC.txt; Ankunft je 150 Einheiten neben dem Gate.
- Textdateien (Field.txt, NPC.txt): alle vorhandenen Zeilen bleiben byte-gleich. Neue Zeilen haben genau die Form der Originalzeilen ihrer Tabelle - in Field.txt '#Record' mit Leerzeichen hinter den Zahlen wie in der Vorlagenzeile, in NPC.txt das führende Tab und '#recordin <Tabelle>', jeweils mit derselben Zahl an Auffüll-Tabs.
- Ladebild: Die Karte bekommt ein eigenes Ladebild resmenu/loading/<Karte>.png (bzw. .tga); MapViewInfo.Loading zeigt darauf. Ist im Feld 'Ladebild' ein Bild gewählt (PNG, JPG, BMP, TGA oder DDS), wird es je Variante der Vorlage auf deren Größe und Format umgerechnet (füllend, Überstand mittig abgeschnitten); sonst ist es eine unveränderte Kopie des Vorlagen-Ladebilds. Ist das Bild nicht verwendbar, nennt der Bericht den Grund und kopiert die Vorlage. Ersetzen lässt es sich später im Interface-Tab unter 'Ladebildschirme' - andere Karten bleiben unberührt.
Die Liste unter dem Knopf zeigt jede geschriebene Datei. Danach mit der Kartenprüfung kontrollieren.
Minimap: Nach dem Laden der neuen Karte erzeugt der Editor resmenu\minimap\<Karte>.dds (512x512 DXT1, Draufsicht im Stil der Originale) und schaltet MinimapView ein.
Minimap-Panel: zeigt die Spiel-Minimap der offenen Karte (Projekt vor Client) mit ihrem MapViewInfo-Ausschnitt oder die Editor-Vorschau. 'Minimap erzeugen' rendert die Karte von oben (wahlweise mit Objekten und im Stil der Originale: dunkle Konturen an der Grenze begehbar/blockiert, Pergament-Rand), schreibt sie ins Projekt und setzt in MapViewInfo MinimapView 1 und den Ausschnitt 0,0-511,511 (ganze Karte). Die Kartenprüfung warnt, wenn MinimapView gesetzt ist, aber das Bild fehlt.)MAN",
     R"MAN(Map > New map creates a map with all client and server entries and writes it to the project folder right away (the source folders stay untouched).
- Map name: folder, file and server name (letter first, then letters/digits/_, at most 15 characters). Display name: the name in the game.
- Template: an existing map whose texture layers, light (.conf, SHMD scene light), sky and .aid are taken over - so every reference points to files that exist in the client. Newly created are flat terrain, fresh masks (layer 1 full), a free Block&Walk grid, a white vertex colour bitmap and an empty object list.
- Size 129, 257 (12800 x 12800 units like Roumen) or 513 height points; base height.
- Game data: MapInfo.shn and MapViewInfo.shn (client and server, ID = next free one below 1000, respawn in the map centre), World/Field.txt (Serial = CheckSum = next free number, as in all 153 original entries), server walkability 9Data/Shine/BlockInfo/<map>.shbd/.aid.
- Optionally a gate from an existing map (server name and position) with a way back: two ShineNPC gates and two LinkTable entries in World/NPC.txt; arrival 150 units next to each gate.
- Text files (Field.txt, NPC.txt): every existing line stays byte-identical. New lines have exactly the form of the original lines of their table - in Field.txt '#Record' with spaces after the numbers as in the template line, in NPC.txt the leading tab and '#recordin <table>', each with the same number of padding tabs.
- Loading image: the map gets its own loading image resmenu/loading/<map>.png (or .tga); MapViewInfo.Loading points to it. If an image is chosen in the 'Loading image' field (PNG, JPG, BMP, TGA or DDS), it is converted to the size and format of each template variant (filling, overhang cut off centred); otherwise it is an unchanged copy of the template's loading image. If the image cannot be used, the report names the reason and copies the template. Replace it later in the interface tab under 'Loading screens' - other maps stay untouched.
The list under the button shows every written file. Check the result with the map check afterwards.
Minimap: once the new map is loaded the editor generates resmenu\minimap\<map>.dds (512x512 DXT1, top-down view in the style of the originals) and turns MinimapView on.
Minimap panel: shows the open map's game minimap (project before client) with its MapViewInfo crop, or the editor preview. 'Generate minimap' renders the map from above (optionally with objects and in the style of the originals: dark outlines along the walkable border, parchment frame), writes it to the project and sets MinimapView 1 and the crop 0,0-511,511 (whole map) in MapViewInfo. The map check warns when MinimapView is set but the image is missing.)MAN",
     "neue karte new map vorlage template mapinfo field gate assistent wizard"},
    {"map.mapcheck", "map",
     "Kartenprüfung", "Map check",
     R"MAN(Werkzeuge > Kartenprüfung prüft die offene Karte wie Unreals "Map Check" und listet alles, was im Spiel nicht funktionieren würde. Doppelklick springt zur Stelle (Objekte werden ausgewählt, Quests öffnen den Quest-Editor). 'Prüfen' wiederholt die Prüfung nach Änderungen.
Geprüft wird:
- NPCs und Gates auf blockierten Zellen (SHBD). Die Schwere richtet sich nach dem Abstand zur nächsten freien Zelle: bis 4 Zellen Hinweis (kommt im Original vor, z.B. Händler hinter einer Theke), bis 16 Zellen Warnung, darüber Fehler.
- Gates ohne LinkTable-Eintrag, Gate-Ziele auf der Zielkarte (falls deren SHBD im Client-Ordner liegt) und Ankunftspunkte anderer Gates auf dieser Karte.
- TownPortal- und Rückruf-Ziele sowie der Wiederbelebungspunkt (MapInfo RegenX/RegenY) auf blockierten Zellen.
- Spawn-Zonen in Wänden (ab 90 % blockiert), außerhalb der Karte, ohne aktiven MobRegen-Eintrag; Monster, die nicht in MobInfo.shn stehen.
- Objekte außerhalb der Karte, NIFs, die fehlen oder nicht lesbar sind, fehlende Objekt- und Terrain-Texturen.
- Aktivierte Quests, deren Start-NPC nicht existiert oder auf keiner Karte steht ('Quests prüfen').
Hinweise sind ausgeblendet, bis 'Hinweise' angehakt wird. In NA2016 findet die Prüfung z.B. den LinkTable-Eintrag 'Eld' mit Ziel-Y 1344 (blockiert; gemeint ist vermutlich 13445) und sieben Gates ohne LinkTable-Eintrag.)MAN",
     R"MAN(Tools > Map check checks the open map like Unreal's "Map Check" and lists everything that would not work in the game. Double-click jumps to the spot (objects are selected, quests open the quest editor). 'Check' runs it again after changes.
Checked are:
- NPCs and gates on blocked cells (SHBD). Severity follows the distance to the nearest free cell: up to 4 cells note (happens in the original, e.g. a merchant behind a counter), up to 16 cells warning, beyond that error.
- Gates without a LinkTable entry, gate targets on the target map (if its SHBD is in the client folder) and arrival points of other gates on this map.
- TownPortal and recall targets and the respawn point (MapInfo RegenX/RegenY) on blocked cells.
- Spawn zones inside walls (90 % blocked or more), outside the map, without an active MobRegen entry; monsters that are not in MobInfo.shn.
- Objects outside the map, NIFs that are missing or unreadable, missing object and terrain textures.
- Enabled quests whose starting NPC does not exist or is not placed on any map ('Check quests').
Notes stay hidden until 'Notes' is ticked. In NA2016 the check finds e.g. the LinkTable entry 'Eld' with target Y 1344 (blocked; 13445 was probably meant) and seven gates without a LinkTable entry.)MAN",
     "kartenprüfung map check fehler error prüfen gate npc spawn quest nif textur"},
    {"map.autosave", "map",
     "Automatisches Speichern", "Autosave",
     R"MAN(Bei Änderungen sichert der Editor die offene Karte alle 5 Minuten (Werkzeuge > Einstellungen: an/aus, 1-60 Minuten, 'Jetzt sichern'). Die Sicherung liegt in <Projekt>/Autosave/Client/resmap/field/<Karte> - nicht in der Projektausgabe, sie wird also nie ausgeliefert. Geschrieben wird im Hintergrund, nicht während eines Pinselstrichs.
Beim nächsten Öffnen der Karte (z.B. nach einem Absturz) fragt der Editor: 'Wiederherstellen' übernimmt Höhen, Layer-Masken, Block&Walk, Objekte und die passende IDM-Zuordnung aus der Sicherung; die Karte ist danach geändert und wird mit Speichern ins Projekt übernommen. 'Verwerfen' löscht die Sicherung, 'Später' fragt beim nächsten Öffnen erneut. Speichern ins Projekt löscht die Sicherung.)MAN",
     R"MAN(When something changed, the editor saves the open map every 5 minutes (Tools > Settings: on/off, 1-60 minutes, 'Autosave now'). The autosave lives in <Project>/Autosave/Client/resmap/field/<map> - not in the project output, so it is never shipped. It is written in the background, never during a brush stroke.
The next time the map is opened (e.g. after a crash) the editor asks: 'Restore' takes heights, layer masks, Block&Walk, objects and the matching IDM mapping from the autosave; the map is then modified and goes to the project with Save. 'Discard' deletes the autosave, 'Later' asks again next time. Saving to the project deletes the autosave.)MAN",
     "autosave automatisch speichern sicherung absturz crash wiederherstellen restore"},
    {"shn.overview", "shn",
     "SHN-Editor: Grundlagen", "SHN editor: basics",
     R"MAN(SHN sind die Tabellen-Dateien des Spiels (Zeilen und typisierte Spalten). CLIENT-Dateien liegen in ressystem, SERVER-Dateien in 9Data/Shine.
- Dateien einlesen: 'CLIENT: SHN-Ordner einlesen' / 'SERVER: SHN-Ordner einlesen' (geschieht meist automatisch beim ersten Öffnen), 'Einzelne SHN öffnen...' für eine Datei.
- Listen: CLIENT und SERVER getrennt. '*' = geändert. Ein gelbes Warnzeichen zeigt Dateien mit möglicherweise abhängigen Tabellen (Tooltip nennt sie: gleiche Tabelle in Client/Server oder verwandte Tabellen mit gleicher ID).
- Suche: filtert Zeilen; 'Spalten durchsuchen' / 'Werte durchsuchen' legen fest, wo gesucht wird.
- Zelle bearbeiten: Einzelklick wählt, Doppelklick oder 'Zelle bearbeiten' öffnet das Feld. Tippen, dann Enter oder 'Übernehmen'. Ungültige Werte (falscher Typ, zu groß) werden abgelehnt.
- '+ Neue Zeile (in Familie propagieren)': legt eine Zeile mit automatisch freier ID an - in der Datei und in allen zugehörigen Tabellen (z.B. ItemInfo + ItemViewInfo). Die ID ist die erste ID im größten freien Block (nicht Maximum+1).
- 'Speichern' schreibt die Datei.
Beziehungen zwischen Tabellen laufen über ID und InxName (Text-Schlüssel). Ein InxName wird von anderen Tabellen referenziert - vorhandene sollte man nicht umbenennen.)MAN",
     R"MAN(SHN files are the tables of the game (rows and typed columns). CLIENT files are in ressystem, SERVER files in 9Data/Shine.
- Loading files: 'CLIENT: read SHN folder' / 'SERVER: read SHN folder' (usually happens automatically on first open), 'Open single SHN...' for one file.
- Lists: CLIENT and SERVER separate. '*' = modified. A yellow warning sign marks files with possibly dependent tables (the tooltip names them: same table in client/server or related tables with the same ID).
- Search: filters rows; 'Search columns' / 'Search values' set where to search.
- Edit a cell: single click selects, double click or 'Edit cell' opens the field. Type, then Enter or 'Apply'. Invalid values (wrong type, too large) are rejected.
- '+ New row (propagate to family)': creates a row with an automatically free ID - in the file and in all related tables (e.g. ItemInfo + ItemViewInfo). The ID is the first ID in the largest free block (not maximum+1).
- 'Save' writes the file.
Relations between tables use ID and InxName (text key). An InxName is referenced by other tables - do not rename existing ones.)MAN",
     "shn tabelle zeile spalte zelle bearbeiten id inxname client server"},
    {"shn.tabs", "shn",
     "Die Tabs des SHN-Editors", "The tabs of the SHN editor",
     R"MAN(- Single SHN Editor: beliebige Tabelle als Raster bearbeiten.
- Multi SHN Editor: wähle eine Aufgabe (Neues Item, Neuer NPC, Neuer Mob, Neuer Skill, Shop/Preis, Neue Quest, XP/Rate); der Editor markiert die dafür relevanten Tabellen in CLIENT und SERVER gelb als Kandidaten. Das ist eine Orientierungshilfe, keine bewiesene Abhängigkeit.
- XP Rate Editor: MonEXP und EXPRange aller Mobs (MobInfoServer) um einen Prozentwert ändern ('% Änderung').
- Buy & Sell Editor: BuyPrice und SellPrice aller Items (ItemInfo) um einen Prozentwert ändern.
- Quest Editor: Quests (QuestData.shn) mit Texten aus QuestDialog.
- Portale: TownPortal-Ziele (auch im Map-Editor).
- Custom NPC/Mob: NPCs und Monster aus Vorlagen erzeugen.
- Skill Editor: Skills ändern und neue erstellen.
- Drops: Mob-Drop-Tabelle und Drop-Gruppen.
- Items / Mobs: Formular-Editoren für Items (ItemInfo, ItemInfoServer, ItemViewInfo) und Mobs (MobInfo, MobInfoServer, MobViewInfo).
- Passiv / Zustände: Passiv-Skills (PassiveSkill, PassiveSkillView) und Zustände (AbState, AbStateView, SubAbState).
- Texte: alle Text- und Lokalisierungstabellen (TextData, ScriptMsg, QuestDialog, AnnounceData ...) mit Suche und CSV-Austausch.
- Item-Systeme: Sets, Set-Effekte, Aufwertung, Aufwertungs-Chancen, Item-Mix, Zufallsoptionen und Item-Pakete.
- Angriffsfolgen: Skill-Reihenfolge und Regeln der Mobs (MobAttackSequence).
Die Prozent-Editoren ändern ALLE Zeilen der Spalte - vorher prüfen, dann speichern.)MAN",
     R"MAN(- Single SHN Editor: edit any table as a grid.
- Multi SHN Editor: choose a task (new item, new NPC, new mob, new skill, shop/price, new quest, XP/rate); the editor marks the relevant tables in CLIENT and SERVER yellow as candidates. This is an orientation aid, not a proven dependency.
- XP Rate Editor: change MonEXP and EXPRange of all mobs (MobInfoServer) by a percentage ('% change').
- Buy & Sell Editor: change BuyPrice and SellPrice of all items (ItemInfo) by a percentage.
- Quest Editor: quests (QuestData.shn) with texts from QuestDialog.
- Portals: TownPortal targets (also in the map editor).
- Custom NPC/Mob: create NPCs and monsters from templates.
- Skill Editor: modify skills and create new ones.
- Drops: mob drop table and drop groups.
- Items / Mobs: form editors for items (ItemInfo, ItemInfoServer, ItemViewInfo) and mobs (MobInfo, MobInfoServer, MobViewInfo).
- Passive / States: passive skills (PassiveSkill, PassiveSkillView) and states (AbState, AbStateView, SubAbState).
- Texts: all text and localization tables (TextData, ScriptMsg, QuestDialog, AnnounceData ...) with search and CSV exchange.
- Item systems: sets, set effects, upgrade, upgrade chances, item mix, random options and item packages.
- Attack sequences: skill order and rules of mobs (MobAttackSequence).
The percentage editors change ALL rows of the column - check first, then save.)MAN",
     "tabs multi xp rate buy sell quest single"},
    {"shn.quest", "shn",
     "Quest-Editor", "Quest editor",
     R"MAN(Links die Liste (Titel oder Beschreibung, Suche nach ID oder Text), rechts die Details:
- Quest-ID, Titel-Text-ID und Beschreibung-Text-ID; Titel und Beschreibung stehen daneben als Eingabefeld und lassen sich direkt ändern (QuestDialog.shn, übernommen beim Verlassen des Felds, Strg+Z macht es rückgängig). Mindest-/Maximal-Level, Start-NPC (Mob-ID), Aktiviert, Tägliche Quest, benötigtes Item, Vorgänger-Quest.
- Start-NPC: 'Auswählen...' öffnet eine Liste aller NPCs aus MobInfo.shn mit ihrer Platzierung aus World/NPC.txt (Karte, Position, Rolle); Suche nach Name, InxName oder ID. Die Auswahl setzt die ID und schaltet 'Start-NPC erforderlich' ein. Steht der NPC auf keiner Karte, wird das rot angezeigt - die Quest kann dann im Spiel nicht angenommen werden.
- Monster-/NPC-Ziele (5 Plätze, Art 'NPC aufsuchen' oder 'Monster besiegen'), Item-Ziele (10 Plätze): aktiv, ID, Anzahl; der Name wird aufgelöst (rot = nicht gefunden). 'Auswählen...' wählt auch hier aus der Liste; 'NPC aufsuchen' hat im Original Anzahl 0.
- Drops (bis 10): Mob, Item, Menge, Rate, Min?/Max? (vermutlich Mindest-/Höchstanzahl, nicht belegt).
- Skripte Start / Action / Finish: die Quest-Skriptsprache (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'SAY-Text-ID nachschlagen' zeigt den Text zu einer ID.
- Dialoge: alle Texte der Quest in einer Liste - Titel, Beschreibung und jede SAY-Zeile (mit Skript und Sprecher NPC/ME), jeweils bearbeitbar. Fehlt eine Text-ID in QuestDialog, wird die Zeile beim Bearbeiten angelegt. Nutzen mehrere Quests denselben Text (z. B. die Anfänger-Quest 1 und ihre Kopien 944-946), steht die Anzahl daneben; 'Eigene ID' kopiert den Text auf eine freie ID und stellt nur diese Quest um.
- Klonen: 'Aus Auswahl klonen' übernimmt die Text-IDs der Vorlage (Klon und Vorlage teilen dann ihre Texte). Mit 'neue Text-IDs' werden alle Texte kopiert und die Quest bekommt einen freien Text-ID-Block hinter der höchsten belegten ID; Titel, Beschreibung und alle SAY-Verweise werden umgestellt.
- Belohnungen (12 Einträge): Verwendung (fest/Auswahl), Typ (EXP, Geld, Item, Typ 4) und Wert bzw. Item-ID × Anzahl. Die Struktur ist an allen 2304 NA2016-Quests belegt; mit * markierte Bedeutungen sind aus den Wertebereichen abgeleitet.
- 'QuestData speichern' schreibt QuestData.shn (Client und Server) und geänderte Texte in QuestDialog.shn; die bytegleiche Server-Kopie von QuestDialog wird mitgeschrieben.)MAN",
     R"MAN(On the left the list (title or description, search by ID or text), on the right the details:
- Quest ID, title text ID and description text ID; title and description are shown next to them as input fields and can be changed directly (QuestDialog.shn, applied when leaving the field, Ctrl+Z undoes it). Minimum/maximum level, start NPC (mob ID), enabled, daily quest, required item, predecessor quest.
- Start NPC: 'Choose...' opens a list of all NPCs from MobInfo.shn with their placement from World/NPC.txt (map, position, role); search by name, InxName or ID. Choosing one sets the ID and turns on 'Starting NPC required'. An NPC that stands on no map is shown in red - the quest cannot be accepted in the game then.
- Monster/NPC targets (5 slots, kind 'visit NPC' or 'defeat monster'), item targets (10 slots): active, ID, count; the name is resolved (red = not found). 'Choose...' picks from the list here too; 'visit NPC' has count 0 in the original.
- Drops (up to 10): mob, item, amount, rate, Min?/Max? (probably minimum/maximum count, unproven).
- Scripts Start / Action / Finish: the quest script language (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'Look up SAY text ID' shows the text for an ID.
- Dialogs: all texts of the quest in one list - title, description and every SAY line (with script and speaker NPC/ME), each editable. If a text ID is missing in QuestDialog, the row is created when you edit it. If several quests use the same text (e.g. beginner quest 1 and its copies 944-946), the count is shown; 'Own ID' copies the text to a free ID and switches only this quest to it.
- Cloning: 'Clone selected' keeps the template's text IDs (clone and template then share their texts). With 'new text IDs' all texts are copied and the quest gets a free block of text IDs after the highest used ID; title, description and all SAY references are switched over.
- Rewards (12 entries): usage (fixed/choice), type (EXP, money, item, type 4) and value or item ID × count. The structure is verified on all 2304 NA2016 quests; meanings marked with * are inferred from value ranges.
- 'Save QuestData' writes QuestData.shn (client and server) and changed texts in QuestDialog.shn; the byte-identical server copy of QuestDialog is written along.)MAN",
     "quest questdata skript say if goto belohnung reward drop exp start npc auswählen choose platzierung text dialog questdialog klonen clone"},
    {"shn.drops", "shn",
     "Drop-Tabelle und Drop-Gruppen", "Drop table and drop groups",
     R"MAN(Der Drops-Tab hat zwei Reiter.
- Mob-Drops (World/ItemDropTable.txt): je Mob bis zu 45 Slots. Ein Slot nennt keine Items, sondern eine Drop-Gruppe; daneben stehen die Items, auf die sie auflöst. 'Öffnen' springt in den Gruppen-Editor. '+ Drop-Zeile' / 'Drop-Zeile löschen' mit Undo.
- Drop-Gruppen (World/ItemDropGroup.txt): links alle Gruppen (Suche nach Gruppe, ItemID oder Item; 'Nur Probleme'), rechts die Zeilen der gewählten Gruppe. Jede Zeile nennt per ItemID entweder einen DropGroup-Namen aus ItemInfoServer (eine Menge von Items) oder direkt einen InxName; 'Wählen...' öffnet die Item-Auswahl mit Icons. Menge (Min-Max) und die Verbesserungsstufe (Promille je +0 bis +15, zusammen 1000) sind bearbeitbar.
- CheckSum wird automatisch nachgeführt: MinQtty + MaxQtty + alle Upgrade-Werte (an allen 970 NA2016-Zeilen belegt).
- '+ Gruppe' legt eine Gruppe an, '+ Zeile' eine weitere Zeile, 'x' löscht eine Zeile, 'Gruppe löschen' die ganze Gruppe (mit Hinweis, welche Mobs sie noch nennen). Umbenennen im Namensfeld ändert auch die DrItem-Slots der Mob-Drop-Tabelle - als ein Schritt.
- 'Verwendet von' listet die Mobs, die die Gruppe droppen; ein Klick öffnet ihre Zeile in der Drop-Tabelle.
- Beide Dateien werden getrennt gespeichert (Projekt-Override im Server-Ordner). Alles lässt sich mit Strg+Z rückgängig machen.)MAN",
     R"MAN(The Drops tab has two sub-tabs.
- Mob drops (World/ItemDropTable.txt): up to 45 slots per mob. A slot names a drop group, not items; the items it resolves to are shown next to it. 'Open' jumps to the group editor. '+ Drop row' / 'Delete drop row' with undo.
- Drop groups (World/ItemDropGroup.txt): all groups on the left (search by group, ItemID or item; 'Problems only'), the rows of the selected group on the right. Each row names via ItemID either a DropGroup name from ItemInfoServer (a set of items) or an InxName directly; 'Pick...' opens the item picker with icons. Quantity (min-max) and the upgrade level (per mille for +0 to +15, 1000 in total) are editable.
- CheckSum is kept up to date automatically: MinQtty + MaxQtty + all upgrade values (verified on all 970 NA2016 rows).
- '+ Group' creates a group, '+ Row' another row, 'x' deletes a row, 'Delete group' the whole group (with a note which mobs still name it). Renaming in the name field also changes the DrItem slots of the mob drop table - as one step.
- 'Used by' lists the mobs that drop the group; a click opens their row in the drop table.
- Both files are saved separately (project override in the server folder). Everything can be undone with Ctrl+Z.)MAN",
     "drop droptable itemdroptable itemdropgroup gruppe group checksum upgrade menge quantity mob item"},
    {"shn.itemmob", "shn",
     "Item- und Mob-Editor", "Item and mob editor",
     R"MAN(Beide Editoren zeigen links alle Einträge (Suche nach Name, InxName oder ID; Filter Alle / Geändert / Client/Server / Probleme) und rechts ein Formular über alle Tabellen der Familie.
- Item: ItemInfo (Client und Server), ItemInfoServer, ItemViewInfo. Mob: MobInfo (Client und Server), MobInfoServer, MobViewInfo.
- Eingaben werden beim Verlassen des Felds in ALLE Kopien geschrieben (Client und Server) - ein Schritt für Strg+Z. Ein orangefarbenes Label mit '!' heißt: Client und Server haben verschiedene Werte (Tooltip zeigt beide).
- 'Wählen...' bei Verweisfeldern: Items mit Icon, Mobs, Skills, Zustände oder die in der Spalte schon verwendeten Werte. Drop-Gruppe A/B zeigt, in wie vielen Drop-Gruppen-Zeilen der Name vorkommt, 'Öffnen' springt in den Drop-Gruppen-Editor.
- Item-Kopf: Icon mit Atlas-Auswahl, Modell einer Waffe/eines Schilds (resitem), 'Verwendung suchen' (Verweise in allen geladenen Tabellen und in den Drop-Gruppen).
- Mob-Kopf: Modell-Vorschau (reschar), Drop-Zeilen mit Sprung in den Drop-Editor, Platzierung aus World/NPC.txt, Gespräch, 'Spawns suchen' (alle MobRegen-Dateien).
- Prüfung: doppelter InxName, fehlende Zeile in einer Tabelle, Verkaufspreis über Kaufpreis, Min-Schaden über Max-Schaden, Mob-Modell nicht gefunden.
- 'Neu aus diesem anlegen': klont alle Zeilen unter einer freien ID mit neuem InxName (und Namen) - ein Schritt. 'Löschen' entfernt den Eintrag in allen Tabellen der Familie, nach einer Verweisliste.
- 'Alle übrigen Spalten (roh)' zeigt jede Spalte ohne eigenes Feld. Labels mit '(vermutet)' im Tooltip sind aus Namen und Wertebereichen abgeleitet, nicht belegt.)MAN",
     R"MAN(Both editors list all entries on the left (search by name, InxName or ID; filters All / Modified / Client/server / Problems) and show a form across all tables of the family on the right.
- Item: ItemInfo (client and server), ItemInfoServer, ItemViewInfo. Mob: MobInfo (client and server), MobInfoServer, MobViewInfo.
- Input is written to ALL copies (client and server) when you leave the field - one step for Ctrl+Z. An orange label with '!' means client and server have different values (the tooltip shows both).
- 'Pick...' on reference fields: items with icons, mobs, skills, states or the values already used in the column. Drop group A/B shows in how many drop group rows the name occurs; 'Open' jumps to the drop group editor.
- Item header: icon with atlas picker, model of a weapon/shield (resitem), 'Find usage' (references in all loaded tables and in the drop groups).
- Mob header: model preview (reschar), drop rows with a jump to the drop editor, placement from World/NPC.txt, conversation, 'Find spawns' (all MobRegen files).
- Checks: duplicate InxName, row missing in a table, sell price above buy price, minimum damage above maximum, mob model not found.
- 'Create new from this one': clones every row under a free ID with a new InxName (and name) - one step. 'Delete' removes the entry from every table of the family, after a list of references.
- 'All remaining columns (raw)' shows every column without its own field. Labels marked '(assumed)' in the tooltip are derived from names and value ranges, not proven.)MAN",
     "item mob editor iteminfo mobinfo formular form klonen clone icon modell model drop spawn prüfung check"},
    {"shn.states", "shn",
     "Passiv-Skills und Zustände", "Passive skills and states",
     R"MAN(Beide Tabs nutzen denselben Formular-Editor wie Items und Mobs (Liste, Filter, Klonen, Löschen, Strg+Z, Client und Server gemeinsam).
- Passiv: PassiveSkill (Client und Server) und PassiveSkillView. 'Voraussetzung' (DemandSk) ist die Vorstufe; 'Wählen...' listet alle Passiv-Skills. Der Abschnitt Waffenbeherrschung zeigt Rate (MstRt) und Bonus (MstPl) je Waffenart.
- Zustände: AbState (Client und Server) und AbStateView. Der Kopf zeigt Icon, BUFF/DEBUFF, Beschreibung und die aktiven Skills, die den Zustand auslösen (Klick öffnet den Skill).
- Stufen: die SubAbState-Zeilen des Zustands (Stärke 1..n) als Tabelle - Stärke, Typ, Dauer (KeepTime in ms, 4000000000 = dauerhaft) und die Aktionen A-D. Jede Zelle wird in Client und Server geschrieben; '+ Stufe' kopiert die letzte Stufe mit Stärke + 1, 'x' löscht eine Stufe.
- Im Skill-Editor steht bei jedem Zustand (StaNameA-D) ein 'Öffnen'-Knopf, der in diesen Tab springt.
- Icons: die Zustands-Atlanten AbState00-04 sind 128x128 mit 16-px-Zellen (8x8); der Editor erkennt das an der Atlasgröße.)MAN",
     R"MAN(Both tabs use the same form editor as items and mobs (list, filters, clone, delete, Ctrl+Z, client and server together).
- Passive: PassiveSkill (client and server) and PassiveSkillView. 'Requirement' (DemandSk) is the previous step; 'Pick...' lists all passive skills. The weapon mastery section shows the rate (MstRt) and bonus (MstPl) per weapon kind.
- States: AbState (client and server) and AbStateView. The header shows icon, BUFF/DEBUFF, description and the active skills that apply the state (a click opens the skill).
- Levels: the SubAbState rows of the state (strength 1..n) as a table - strength, type, duration (KeepTime in ms, 4000000000 = permanent) and the actions A-D. Each cell is written to client and server; '+ Level' copies the last level with strength + 1, 'x' deletes a level.
- In the skill editor every state (StaNameA-D) has an 'Open' button that jumps to this tab.
- Icons: the state atlases AbState00-04 are 128x128 with 16 px cells (8x8); the editor detects this from the atlas size.)MAN",
     "passiv passive zustand state abstate subabstate buff debuff stufe level mastery waffe icon"},
    {"shn.texts", "shn",
     "Texte und Lokalisierung", "Texts and localization",
     R"MAN(Der Tab 'Texte' sammelt alle Texttabellen von Client und Server: TextData, TextData2, TextData3, ScriptMsg, QuestDialog, MobKillAnnounceText, AbStateMsg, BasicInfoTip, ItemActionEffectDesc, SetItemName, KingdomQuestDesc, KQVoteDesc, CollectCardGroupDesc (Client) sowie AnnounceData, MsgWorldManager, ErrorCodeTable und QuestDialog (Server). [C], [S] bzw. [C+S] zeigt die Seite.
- Suche: Das Feld oben durchsucht alle Tabellen nach Schlüssel und Inhalt (ab 2 Zeichen, höchstens 500 Treffer). Ein Klick springt zur Zeile.
- Bearbeiten: Textspalten (variable Zeichenketten und feste ab 64 Byte) als mehrzeiliges Feld; übernommen wird beim Verlassen des Feldes, ein Undo-Schritt (Strg+Z). Feste Spalten zeigen 'belegt / erlaubt' in Byte; zu lange Texte werden abgelehnt. Die übrigen Spalten (z. B. SM_Inx, Farbe, Dauer bei ScriptMsg) stehen darunter. Tabellen auf beiden Seiten (QuestDialog, CollectCardGroupDesc) werden in Client und Server gleich geändert.
- Schlüssel: der Wert der ersten Spalte; doppelte Werte heißen beim zweiten Vorkommen <Wert>#2 usw., Tabellen ohne Schlüsselspalte werden nach Zeile adressiert (#0, #1 ...).
- Verwendet von: ScriptMsg zeigt die Lua-Skripte (LuaScript), die den SM_Inx aufrufen, und FaceCutMsg-Zeilen; MobKillAnnounceText zeigt die Mobs aus MobKillAnnounce.shn; AbStateMsg den Zustand aus AbState.shn.
- Neue Zeile: kopiert die gewählte Zeile mit der nächsten freien ID und leeren Texten (bei ScriptMsg SM_Inx = NewMsg_<ID>). Bei TextData* ist das gesperrt, weil die Text-ID ein Hash aus dem Client-Code ist - neue Zeilen würden nie angezeigt. Ebenso bei AnnounceData (Typ fest im Server) und den nach Zeile adressierten Tabellen.
- Zeile löschen: mit Rückfrage und Liste der Verwendungen; Client und Server, Strg+Z.
- CSV für Übersetzungen: 'Diese Tabelle exportieren' bzw. 'Alle Tabellen exportieren' schreibt table,key,column,text (RFC 4180, Kommas und Zeilenumbrüche in Anführungszeichen). Übersetzen, dann 'CSV importieren': nur Zellen, deren Text sich unterscheidet, werden übernommen - alles als ein Undo-Schritt. Unbekannte Schlüssel, Nicht-Textspalten und zu lange Texte werden aufgelistet und übersprungen. Der Text wird bytegenau übernommen (keine Umkodierung); die NA-Daten sind ASCII/Latin-1.
- Platzhalter wie %s, %d, [NAME], [LINE] müssen beim Übersetzen erhalten bleiben.
Gespeichert wird mit 'Alle geänderten SHN speichern' (Strg+S) als Projektkopie.)MAN",
     R"MAN(The 'Texts' tab collects all text tables of client and server: TextData, TextData2, TextData3, ScriptMsg, QuestDialog, MobKillAnnounceText, AbStateMsg, BasicInfoTip, ItemActionEffectDesc, SetItemName, KingdomQuestDesc, KQVoteDesc, CollectCardGroupDesc (client) and AnnounceData, MsgWorldManager, ErrorCodeTable and QuestDialog (server). [C], [S] or [C+S] shows the side.
- Search: the field at the top searches all tables by key and content (from 2 characters, at most 500 hits). A click jumps to the row.
- Editing: text columns (variable strings and fixed ones from 64 bytes) as a multi-line field; applied when leaving the field, one undo step (Ctrl+Z). Fixed columns show 'used / allowed' in bytes; texts that are too long are refused. The other columns (e.g. SM_Inx, color, duration for ScriptMsg) are listed below. Tables on both sides (QuestDialog, CollectCardGroupDesc) are changed identically on client and server.
- Key: the value of the first column; repeated values are called <value>#2 etc. from the second occurrence, tables without a key column are addressed by row (#0, #1 ...).
- Used by: ScriptMsg shows the Lua scripts (LuaScript) that call the SM_Inx, and FaceCutMsg rows; MobKillAnnounceText shows the mobs from MobKillAnnounce.shn; AbStateMsg the state from AbState.shn.
- New row: copies the selected row with the next free ID and empty texts (for ScriptMsg SM_Inx = NewMsg_<ID>). Disabled for TextData* because the text ID is a hash from the client code - new rows would never be shown. Also for AnnounceData (type fixed in the server) and the tables addressed by row.
- Delete row: with confirmation and list of usages; client and server, Ctrl+Z.
- CSV for translations: 'Export this table' or 'Export all tables' writes table,key,column,text (RFC 4180, commas and line breaks in quotes). Translate, then 'Import CSV': only cells whose text differs are applied - all as one undo step. Unknown keys, non-text columns and texts that are too long are listed and skipped. Text is taken byte for byte (no re-encoding); the NA data is ASCII/Latin-1.
- Placeholders such as %s, %d, [NAME], [LINE] must be kept when translating.
Saving happens with 'Save all changed SHN' (Ctrl+S) as a project copy.)MAN",
     "text texte lokalisierung localization übersetzung translation csv textdata scriptmsg questdialog announce errorcode suche search"},
    {"shn.korean", "shn",
     "Koreanische Texte", "Korean texts",
     R"MAN(NA2016 enthält rund 3.700 Zellen mit koreanischem Text (CP949/EUC-KR), vor allem Mob-Skillnamen (ActiveSkill.Name), Item-Namen und -Titel, Set-Namen, Sammelkarten-Gruppen und Kommentare in QuestScript. Der Editor zeigt sie überall übersetzt an - im SHN-Raster, in Listen und Auswahlfeldern der Editoren, in Angriffsfolgen und im Text-Editor. Die Sprache folgt der Oberfläche (Deutsch oder Englisch).
- Herkunft der Übersetzung, in dieser Reihenfolge: 1. Projekt-Wörterbuch <Projekt>/Texte/ko_user.tsv (eigene Korrekturen und Online-Ergebnisse), 2. mitgeliefertes Wörterbuch assets/i18n/ko_dictionary.tsv (alle 1.214 verschiedenen Texte aus NA2016, Englisch und Deutsch), 3. Automatik: bekannte Wörter aus dem Glossar assets/i18n/ko_glossary.tsv, der Rest als Umschrift - markiert mit '≈'. Der Tooltip einer Zelle zeigt die Umschrift des Originals und die Herkunft.
- Nur Anzeige: Die Daten bleiben koreanisch, bis Du die Übersetzung übernimmst. Einzelne hohe Bytes aus Windows-1252 (z. B. '…' in ScriptMsg) gelten nicht als Koreanisch.
- 'Koreanische Texte übersetzen (N)' im SHN-Raster und im Text-Editor ersetzt alle koreanischen Zellen der Tabelle (Client und Server zusammen) durch die englische Übersetzung - ein Undo-Schritt (Strg+Z). Texte, die nur automatisch übersetzt sind, Übersetzungen, die länger als die feste Spalte sind (Länge minus 1 Byte), und Schlüsselspalten (Verweise wie *IDX/*Inx/InxName und die Admin-Befehle AdminLvSet.ALS_Cmd - dort sind koreanische Befehlsnamen Aliase, eine Übersetzung ergäbe doppelte Befehle) werden übersprungen und im Bericht aufgelistet. Englisch, weil der Client nur ASCII/Latin-1 sicher darstellt.
- Einzelne Zelle: Rechtsklick im SHN-Raster -> 'Übersetzung übernehmen (Englisch)', im Text-Editor der Knopf über dem Eingabefeld.
- Eigene Übersetzung: ko_user.tsv mit einem Texteditor bearbeiten (UTF-8, drei Spalten korean, english, german, getrennt durch Tab; '#' am Zeilenanfang ist ein Kommentar). Der Editor liest die Datei beim Öffnen des Projekts.
- Online übersetzen (optional): 'Unbekannte koreanische Texte online übersetzen (N)' erscheint, wenn Texte nur automatisch übersetzt sind (z. B. in anderen Datenständen). Die Texte gehen in Blöcken zu 50 an die Claude-API von Anthropic und werden mit Deinem eigenen API-Schlüssel abgerechnet; vorher fragt der Editor nach. Den Schlüssel trägst Du unter Einstellungen ein (oder setzt ANTHROPIC_API_KEY). Er wird in den Editor-Einstellungen gespeichert (unter Windows verschlüsselt), nie im Projekt. Die Ergebnisse landen in ko_user.tsv und sind sofort sichtbar; in die Daten kommen sie erst über 'Koreanische Texte übersetzen'. Ausweichmodelle sind eingeschaltet: Lehnt das Modell eine Anfrage ab, wiederholt der Server sie mit einem anderen Modell.
Bitte die Übersetzungen von Namen (Mobs, Orte) im Spiel prüfen - sie sind teils sinngemäß, teils umschrieben.)MAN",
     R"MAN(NA2016 contains about 3,700 cells with Korean text (CP949/EUC-KR), mostly mob skill names (ActiveSkill.Name), item names and titles, set names, collect card groups and comments in QuestScript. The editor shows them translated everywhere - in the SHN grid, in lists and pickers of the editors, in attack sequences and in the text editor. The language follows the interface (German or English).
- Source of the translation, in this order: 1. project dictionary <project>/Texte/ko_user.tsv (your corrections and online results), 2. shipped dictionary assets/i18n/ko_dictionary.tsv (all 1,214 distinct texts from NA2016, English and German), 3. automatic: known words from the glossary assets/i18n/ko_glossary.tsv, the rest romanized - marked with '≈'. The tooltip of a cell shows the romanized original and the source.
- Display only: the data stays Korean until you apply the translation. Single high bytes from Windows-1252 (e.g. '…' in ScriptMsg) do not count as Korean.
- 'Translate Korean texts (N)' in the SHN grid and in the text editor replaces all Korean cells of the table (client and server together) with the English translation - one undo step (Ctrl+Z). Texts that are only translated automatically, translations longer than the fixed column (length minus 1 byte) and key columns (references such as *IDX/*Inx/InxName and the admin commands AdminLvSet.ALS_Cmd - Korean command names there are aliases, translating them would create duplicate commands) are skipped and listed in the report. English, because the client only displays ASCII/Latin-1 reliably.
- Single cell: right-click in the SHN grid -> 'Apply translation (English)', in the text editor the button above the input field.
- Your own translation: edit ko_user.tsv with a text editor (UTF-8, three columns korean, english, german separated by tab; '#' at the start of a line is a comment). The editor reads the file when the project is opened.
- Translate online (optional): 'Translate unknown Korean texts online (N)' appears when texts are only translated automatically (e.g. in other data sets). The texts are sent in batches of 50 to Anthropic's Claude API and billed to your own API key; the editor asks first. Enter the key under Settings (or set ANTHROPIC_API_KEY). It is stored in the editor settings (encrypted on Windows), never in the project. Results go to ko_user.tsv and are visible at once; they reach the data only via 'Translate Korean texts'. Fallback models are enabled: if the model declines a request, the server retries it with another model.
Please check translated names (mobs, places) in game - some are by meaning, some transliterated.)MAN",
     "koreanisch korean übersetzung translation wörterbuch dictionary glossar glossary euc-kr cp949 umschrift romanization hangul api online claude schlüssel key"},
    {"shn.itemsystems", "shn",
     "Item-Systeme", "Item systems",
     R"MAN(Der Tab 'Item-Systeme' bearbeitet Tabellen, in denen mehrere Zeilen mit demselben Schlüssel einen Eintrag bilden. Oben wählst Du das System:
- Sets: SetItem (Server: Teilezahl -> Effektname aus SetItemEffect), SetEffect (Client und Server: Teilezahl -> Item-Aktion, Beschreibung aus ItemActionEffectDesc) und SetItemName (Client: Anzeigename). Unten stehen die Items mit ItemInfo.SetItemIndex = Set (Klick öffnet den Item-Editor).
- Set-Effekte: SetItemEffect (Server) - Beschreibung, Skill-Gruppe, Stufenbereich From/To, Index und Argument.
- Aufwertung: UpgradeInfo (Client und Server) - Werte je Aufwertungsstufe einer Gruppe. Dass Items über ItemInfo.BasicUpInx auf die ID verweisen, ist vermutet.
- Aufwertungs-Chancen: ItemUpgrade (Server) - je ID eine Zeile pro Stufe (vermutet) mit den Fehlschlag- und Glückswerten.
- Item-Mix: ItemMix (Client und Server) - Ausgangs-Item -> Ergebnis-Item.
- Zufallsoptionen: RandomOption und RandomOptionCount (Server); Items verweisen über ItemInfoServer.RandomOptionDropGroup.
- Item-Pakete: ItemPackage (Server) - Paket-Item, Inhalt und Anzahl.
Bedienung:
- Links die Einträge (Filter, 'Nur mit Problemen' zeigt Einträge mit Verweisen auf unbekannte Items oder Effekte). Rechts alle Zeilen des Eintrags in jeder beteiligten Tabelle; jede Zelle wird beim Verlassen übernommen. Neben verweisenden Zellen steht der Name bzw. die Beschreibung des Ziels, unbekannte Ziele sind markiert.
- '+ Zeile' kopiert die letzte Zeile des Eintrags und fügt sie direkt dahinter ein, 'X' löscht eine Zeile.
- 'Als Kopie anlegen' legt den Eintrag unter dem neuen Schlüssel als Kopie des gewählten in allen Tabellen an, 'Gewählten umbenennen' ändert den Schlüssel überall, 'Gewählten löschen' entfernt alle Zeilen (mit Hinweis auf verweisende Items).
- Tabellen auf beiden Seiten werden gemeinsam geändert: die n-te Zeile eines Eintrags im Client entspricht der n-ten im Server. Alles geht über den Daten-Verlauf (Strg+Z) und wird mit 'Alle geänderten SHN speichern' als Projektkopie geschrieben.)MAN",
     R"MAN(The 'Item systems' tab edits tables in which several rows with the same key form one entry. Pick the system at the top:
- Sets: SetItem (server: piece count -> effect name from SetItemEffect), SetEffect (client and server: piece count -> item action, description from ItemActionEffectDesc) and SetItemName (client: display name). Below are the items with ItemInfo.SetItemIndex = set (a click opens the item editor).
- Set effects: SetItemEffect (server) - description, skill group, level range From/To, index and argument.
- Upgrade: UpgradeInfo (client and server) - values per upgrade level of a group. That items reference the ID via ItemInfo.BasicUpInx is assumed.
- Upgrade chances: ItemUpgrade (server) - per ID one row per level (assumed) with the failure and luck values.
- Item mix: ItemMix (client and server) - source item -> result item.
- Random options: RandomOption and RandomOptionCount (server); items reference them via ItemInfoServer.RandomOptionDropGroup.
- Item packages: ItemPackage (server) - package item, content and count.
Usage:
- Entries on the left (filter, 'Problems only' shows entries referencing unknown items or effects). On the right all rows of the entry in every table involved; each cell is applied when leaving it. Next to referencing cells the name or description of the target is shown, unknown targets are marked.
- '+ Row' copies the last row of the entry and inserts it right behind it, 'X' deletes a row.
- 'Create as copy' creates the entry under the new key as a copy of the selected one in all tables, 'Rename selected' changes the key everywhere, 'Delete selected' removes all rows (with a note about referencing items).
- Tables on both sides are changed together: the n-th row of an entry on the client matches the n-th on the server. Everything goes through the data history (Ctrl+Z) and is written with 'Save all changed SHN' as a project copy.)MAN",
     "item system sets set effekt upgrade aufwertung mix random zufall option paket package itemupgrade upgradeinfo randomoption"},
    {"shn.mounts", "shn",
     "Reittiere und Haustiere", "Mounts and pets",
     R"MAN(Der Tab 'Reittiere' bearbeitet Mover (Reittiere/Fahrzeuge) und Pups (Haustiere). Er funktioniert wie die Item-Systeme: links die Einträge, rechts alle Zeilen des Eintrags in jeder beteiligten Tabelle, Client und Server gemeinsam, Strg+Z.
- Mover (Schlüssel MoverIDX, 461 in NA2016): MoverMain (Nummer MoverID, Zauberzeit CastingTime und Abklingzeit CoolTime in ms, RunSpeed/WalkSpeed, DurationHour = Dauer in Stunden, 0 = dauerhaft, MaxCharSlot = Sitzplätze), MoverItem (Item, das den Mover ruft), MoverHG (Hunger: FeedType = Futter-Item, RestoreAmount, MaxHG, CreateHG, Tick in ms, ConsumeHG - Bedeutung teils vermutet), MoverView (Name, Modellordner FolderName, Bilder Normalmage/HungryImage/EatImage, MoverTypeIDX), MoverSlotView (Sitzplätze, ob der Charakter sichtbar ist). MoverAbility und MoverUseSkill sind in NA2016 leer. Oben im Eintrag steht das Bild aus resmenu/game/MobPortrait/<Normalmage>.
- Mover-Typen (Schlüssel MoverTypeIDX): MoverSlotCharAni - Reit-Animationen je Sitzplatz und Charakter-Animation (RideLegECode/RideAniECode, vermutet). Darunter alle Mover dieses Typs; ein Klick wählt den Mover.
- Mover-Aufwertung: MoverUpgradeEffect (eine Tabelle ohne Schlüssel, alle Zeilen = Stufen 0..5, vermutet) mit Tempo, Erholung, Zauberzeiten, Effekt und Zustand (AbStateIDX muss in AbState existieren).
- Pups (Schlüssel PupIDX): PupMain (Nummer PupID, Item, Tempo), PupView (Name, Modell, Größe), PupServer (Laune- und Stress-Spanne), PupCaseDesc (Reaktionen mit Meldung SM_Inx aus ScriptMsg, Effekt und Sound). Der Eintrag '-' enthält die Reaktionen für alle Pups (vermutet).
- Pup-Verhalten: PupCase, PupMind, PupPriority, PupFactorCondition (Server, ohne Schlüssel; Wirkung vermutet).
- 'Als Kopie anlegen' kopiert den Eintrag in allen Tabellen unter dem neuen Schlüssel; MoverMain.MoverID und PupMain.PupID bekommen automatisch die nächste freie Nummer (in Client und Server gleich). Danach bitte das rufende Item ändern: Die Prüfung meldet ein Item, das schon ein anderer Mover/Pup benutzt. Das neue Item legst Du im Item-Editor an ('Neu aus diesem anlegen').
- Modelle der Mover und Pups (Ordner FolderName/FileName) sind in den NA2016-Daten nicht enthalten; eine 3D-Vorschau gibt es deshalb nicht.
Gespeichert wird mit 'Alle geänderten SHN speichern' als Projektkopie.)MAN",
     R"MAN(The 'Mounts' tab edits movers (mounts/vehicles) and pups (pets). It works like the item systems: entries on the left, all rows of the entry in every table involved on the right, client and server together, Ctrl+Z.
- Movers (key MoverIDX, 461 in NA2016): MoverMain (number MoverID, cast time CastingTime and cooldown CoolTime in ms, RunSpeed/WalkSpeed, DurationHour = duration in hours, 0 = permanent, MaxCharSlot = seats), MoverItem (item that summons the mover), MoverHG (hunger: FeedType = feed item, RestoreAmount, MaxHG, CreateHG, Tick in ms, ConsumeHG - meaning partly assumed), MoverView (name, model folder FolderName, images Normalmage/HungryImage/EatImage, MoverTypeIDX), MoverSlotView (seats, whether the character is visible). MoverAbility and MoverUseSkill are empty in NA2016. The entry shows the image from resmenu/game/MobPortrait/<Normalmage> at the top.
- Mover types (key MoverTypeIDX): MoverSlotCharAni - ride animations per seat and character animation (RideLegECode/RideAniECode, assumed). Below are all movers of this type; a click selects the mover.
- Mover upgrade: MoverUpgradeEffect (a table without key, all rows = levels 0..5, assumed) with speed, recovery, cast times, effect and state (AbStateIDX must exist in AbState).
- Pups (key PupIDX): PupMain (number PupID, item, speed), PupView (name, model, size), PupServer (mood and stress range), PupCaseDesc (reactions with message SM_Inx from ScriptMsg, effect and sound). The entry '-' holds the reactions for all pups (assumed).
- Pup behavior: PupCase, PupMind, PupPriority, PupFactorCondition (server, without key; effect assumed).
- 'Create as copy' copies the entry in all tables under the new key; MoverMain.MoverID and PupMain.PupID automatically get the next free number (the same on client and server). Then change the summoning item: the check reports an item that another mover/pup already uses. Create the new item in the item editor ('Create new from this one').
- Mover and pup models (folders FolderName/FileName) are not part of the NA2016 data, so there is no 3D preview.
Saving happens with 'Save all changed SHN' as a project copy.)MAN",
     "reittier mount mover haustier pet pup moverview movermain hunger futter slot sitzplatz aufwertung upgrade pupcase"},
    {"shn.attackseq", "shn",
     "Angriffsfolgen der Mobs und Effekt-Prüfung", "Mob attack sequences and effect check",
     R"MAN(Der Tab 'Angriffsfolgen' bearbeitet Server/9Data/Shine/MobAttackSequence/<Mob>.txt (297 Dateien in NA2016, der Dateiname ist der InxName des Mobs). Im Mob-Editor öffnet 'Angriffsfolge öffnen' die passende Datei.
- Schritte (Tabelle AttSeq): der Mob benutzt die Skills der Reihe nach. Ein Klick auf den Skill öffnet die Auswahl (Server-ActiveSkill, Suche nach InxName oder Name). '-' ist ein leerer Schritt, '*RepeatStart' eine Sondermarke (vermutet: ab hier wiederholen). ^ und v verschieben, + fügt eine Kopie dahinter ein, X löscht. Die Order-Nummern werden immer 0, 1, 2 ... geschrieben, damit Dateireihenfolge und Order gleich bleiben.
- Mob-Skills tragen in NA2016 koreanische Namen (EUC-KR); angezeigt wird die Übersetzung (siehe 'Koreanische Texte'). Unbekannte Skills sind farbig markiert.
- Regeln (Tabelle SkillChange): Bedingung (HPLow_ChangeOrder, OutOfRange, HPLow, NextSkill), Wert, From und To (neuer Skill). Die genaue Bedeutung von Wert und From ist nicht aus dem Server belegt (vermutet: HP-Grenze in Promille bzw. Schritt). Fehlt die Tabelle, legt '+ Regel' sie an.
- 'Als Kopie für diesen Mob anlegen' kopiert die geladene Folge für den eingegebenen InxName.
- Jede Änderung ist ein Undo-Schritt (Strg+Z). 'Speichern' schreibt die Datei als Projektkopie; unveränderte Zeilen und Kommentare bleiben bytegleich.
Effekt-Prüfung: Im Skill-Editor zeigt 'Effekt-Dateien prüfen', welche Effekte aus ActiveSkillView (ShoEffect, LastEffectA, LastAreaEf, DOTRageEft, DOTRageEftLoop) und AbStateView (effName, LoopEffect, LastEffect, DOTEffect) keine Datei in reseffect haben; ein Klick springt zum Skill bzw. Zustand. 'MyWeapon' gilt als Sonderwert (vermutet: die geworfene Waffe selbst). Fehlende Effekte eines Zustands stehen auch in seinen Problemen.)MAN",
     R"MAN(The 'Attack sequences' tab edits Server/9Data/Shine/MobAttackSequence/<mob>.txt (297 files in NA2016, the file name is the mob's InxName). In the mob editor, 'Open attack sequence' opens the matching file.
- Steps (table AttSeq): the mob uses the skills in order. A click on the skill opens the picker (server ActiveSkill, search by InxName or name). '-' is an empty step, '*RepeatStart' a special marker (assumed: repeat from here). ^ and v move, + inserts a copy behind it, X deletes. Order numbers are always written as 0, 1, 2 ... so that file order and Order stay equal.
- Mob skills carry Korean names (EUC-KR) in NA2016; the translation is shown (see 'Korean texts'). Unknown skills are highlighted.
- Rules (table SkillChange): condition (HPLow_ChangeOrder, OutOfRange, HPLow, NextSkill), value, From and To (new skill). The exact meaning of value and From is not confirmed from the server (assumed: HP limit in per mille and step). If the table is missing, '+ Rule' creates it.
- 'Create as copy for this mob' copies the loaded sequence for the entered InxName.
- Every change is one undo step (Ctrl+Z). 'Save' writes the file as a project copy; unchanged lines and comments stay byte-identical.
Effect check: in the skill editor, 'Check effect files' shows which effects from ActiveSkillView (ShoEffect, LastEffectA, LastAreaEf, DOTRageEft, DOTRageEftLoop) and AbStateView (effName, LoopEffect, LastEffect, DOTEffect) have no file in reseffect; a click jumps to the skill or state. 'MyWeapon' counts as a special value (assumed: the thrown weapon itself). Missing effects of a state also appear among its problems.)MAN",
     "angriffsfolge attack sequence mob ki ai skill reihenfolge skillchange repeatstart effekt effect reseffect prüfung check"},
    {"shn.interface", "shn",
     "Interface-Bilder ersetzen", "Replacing interface images",
     R"MAN(Tab 'Interface' im SHN-Editor: alle Dateien unter Client/resmenu (Bilder und Fenster-NIFs). Originale werden nie verändert; jede Änderung ist eine Projektkopie unter <Projekt>/Client/resmenu/... (Markierung OVR* = geändert, OVR= = gleich, NEW = nur im Projekt).
- 'Ersetzen...' öffnet den Ersetzen-Dialog: Quelle als Pfad eintragen (unter Windows auch 'Durchsuchen...'). Erlaubt sind PNG, JPG, BMP, TGA und DDS in beliebiger Größe.
- Das Ergebnis bekommt automatisch Format und Größe des Originals: gleicher Dateityp, bei DDS gleiche Kompression (DXT1/DXT3/DXT5/unkomprimiert) und Mipmaps, bei TGA gleiche Farbtiefe (16/24/32 Bit), bei PNG mit oder ohne Alpha. So lädt der Client die Datei genauso wie vorher.
- Anpassung: 'Strecken' verzerrt auf die Zielgröße, 'Einpassen' zeigt das ganze Bild mit transparentem Rand, 'Füllen' füllt alles und schneidet den Überstand mittig ab. Ohne 'Größe des Originals behalten' bleibt die Größe der Quelle (DDS auf Vielfache von 4 gerundet).
- Vorher/Nachher zeigt beide Bilder auf einem Schachbrett (transparente Stellen sichtbar). 'Nachher' ist bereits die fertig kodierte Datei - Kompressionsverluste sind also zu sehen. Hat die Quelle schon genau Format und Größe des Originals, wird sie unverändert übernommen.
- 'Übernehmen' schreibt die Projektkopie; Strg+Z stellt den vorherigen Stand wieder her (auch nach 'Projektkopie entfernen').
- 'Als PNG exportieren' legt das Bild unter <Projekt>/Export/resmenu/... ab, z. B. als Vorlage für ein Bildprogramm.)MAN",
     R"MAN('Interface' tab in the SHN editor: every file below Client/resmenu (images and window NIFs). Originals are never changed; every change is a project copy below <Project>/Client/resmenu/... (badge OVR* = changed, OVR= = identical, NEW = project only).
- 'Replace...' opens the replace dialog: enter the source as a path (on Windows also 'Browse...'). PNG, JPG, BMP, TGA and DDS of any size are accepted.
- The result automatically gets the format and size of the original: same file type, for DDS the same compression (DXT1/DXT3/DXT5/uncompressed) and mipmaps, for TGA the same colour depth (16/24/32 bit), for PNG with or without alpha. The client therefore loads the file exactly as before.
- Fitting: 'Stretch' distorts to the target size, 'Fit' shows the whole image with a transparent border, 'Fill' fills everything and crops the overhang in the centre. Without 'Keep original size' the source size is kept (DDS rounded to multiples of 4).
- Before/after shows both images on a checkerboard (transparent areas visible). 'After' is already the finished encoded file, so compression losses are visible. If the source already has exactly the original's format and size, it is taken over unchanged.
- 'Apply' writes the project copy; Ctrl+Z restores the previous state (also after 'Remove project copy').
- 'Export as PNG' stores the image below <Project>/Export/resmenu/..., e.g. as a template for an image editor.)MAN",
     "interface resmenu bild image textur texture ersetzen replace konvertieren convert dds tga png schachbrett export"},
    {"shn.loading", "shn",
     "Ladebildschirme", "Loading screens",
     R"MAN(Interface-Tab, Umschalter 'Ladebildschirme'. Beim Betreten einer Karte zeigt der Client resmenu/loading/<Name>.png (bzw. .tga); <Name> steht in MapViewInfo.Loading. Mehrere Karten teilen sich oft ein Bild (NA2016: 138 Karten, 129 Bilder, BFGate01 viermal).
- Liste: Karte, Anzeigename (MapInfo) und Ladebild; 'x4' = von 4 Karten genutzt, rot = Datei fehlt. Oben stehen Karten ohne Ladebild.
- 'Anderes Bild zuweisen...': vorhandenes Ladebild mit Vorschau wählen; MapViewInfo.Loading wird in Client und Server gesetzt (ein Undo-Schritt). Speichern mit 'Alle geänderten SHN speichern'.
- 'Bild ersetzen...': ersetzt das Bild für ALLE Karten, die es nutzen (Hinweis 'betrifft N Karten'). Gibt es das Bild als PNG und TGA, werden beide aus derselben Quelle geschrieben, jeweils in ihrem Format.
- 'Eigenes Bild für diese Karte...': legt loading/<Name>.png als Projektkopie an (Format der bisherigen PNG, meist 1024x768) und setzt Loading dieser Karte auf den neuen Namen - so wird ein geteiltes Bild nur für diese Karte abgespalten. Ein Strg+Z nimmt Datei und Zuweisung zurück.
- Neue Karten aus dem Assistenten bekommen ein eigenes Ladebild (das dort gewählte Bild oder eine Kopie der Vorlage); hier lässt es sich ersetzen.
- LoadLocation (0/1) wird angezeigt und in beide Seiten geschrieben; die Bedeutung ist nicht belegt.
- 'Gemeinsame Ladebild-Dateien': NowLoading.tga (1920x1080), ProgressBar.tga, NowLoadingTextColor.tga und die Tipp-Bilder loading/Tips - jeweils ersetzbar. Wo der Client sie platziert, ist nicht belegt; die Vorschau zeigt sie einzeln.)MAN",
     R"MAN(Interface tab, switch 'Loading screens'. When entering a map the client shows resmenu/loading/<name>.png (or .tga); <name> is MapViewInfo.Loading. Several maps often share one image (NA2016: 138 maps, 129 images, BFGate01 four times).
- List: map, display name (MapInfo) and loading image; 'x4' = used by 4 maps, red = file missing. Maps without a loading image are listed at the top.
- 'Assign other image...': choose an existing loading image with preview; MapViewInfo.Loading is set on client and server (one undo step). Save with 'Save all changed SHN'.
- 'Replace image...': replaces the image for ALL maps using it (note 'affects N maps'). If the image exists as PNG and TGA, both are written from the same source, each in its own format.
- 'Own image for this map...': creates loading/<name>.png as a project copy (format of the previous PNG, usually 1024x768) and points this map's Loading to the new name - this splits a shared image off for this map only. One Ctrl+Z reverts file and assignment.
- New maps from the wizard get their own loading image (the image chosen there or a copy of the template's); replace it here.
- LoadLocation (0/1) is shown and written to both sides; its meaning is not proven.
- 'Shared loading files': NowLoading.tga (1920x1080), ProgressBar.tga, NowLoadingTextColor.tga and the tip images loading/Tips - each replaceable. Where the client places them is not proven; the preview shows them one by one.)MAN",
     "ladebild loading screen ladebildschirm mapviewinfo karte map nowloading progressbar tipps tips abspalten split"},
    {"shn.icons", "shn",
     "Icon-Atlanten und Porträts", "Icon atlases and portraits",
     R"MAN(Interface-Tab, Umschalter 'Icons' und 'Porträts'.
# Icons
Alle Atlanten unter resmenu/Icon. Jeder ist ein 8x8-Raster (Zelle = Atlasbreite / 8: 32 px bei 256er, 16 px bei den Zustands-Atlanten AbState00-04). Die Liste zeigt, wie viele der 64 Zellen benutzt werden.
- Benutzt heißt: eine Zeile in ItemViewInfo (IconFile/IconIndex, SubIcon, PeriodIcon), ActiveSkillView, PassiveSkillView oder AbStateView zeigt auf die Zelle. Eine grüne Ecke markiert freie Zellen; der Tooltip sagt, ob sie leer (ganz transparent) ist. Andere Stellen des Clients können Atlanten ebenfalls nutzen - frei heißt nur 'in keiner dieser Tabellen'.
- Klick auf eine Zelle: Vorschau, wer sie benutzt (Klick springt in den Item-, Skill-, Passiv- oder Zustands-Editor) und 'Zelle ersetzen...'. Die Quelle wird auf die Zellgröße gebracht (Standard: Einpassen), der Atlas im Originalformat geschrieben. Bei DDS ändern sich dabei nur die Blöcke der Zelle in jeder Mipmap-Stufe - alle anderen Icons bleiben bitgleich.
- 'Atlas ersetzen...', 'Als PNG exportieren' und 'Neuer leerer Atlas' (transparent, im Format und in der Größe des gewählten Atlas; der Name zählt die Nummer hoch). Den neuen Atlas trägst Du danach im Item-/Skill-Editor als Icon-Datei ein.
- Aus dem Item-, Skill-, Passiv- und Zustands-Editor führt 'Im Atlas-Editor' direkt zur Zelle des Icons.
# Porträts
Mob-Porträts: resmenu/game/MobPortrait/<MobViewInfo.MobPortrait>.png; NPC-Gesichter im Dialog: resmenu/game/NPCFaceCut/<NpcDialogData.FaceCutFile>.png.
- Die Liste vereint Dateien und Verweise: 'fehlt' = Verweis ohne Datei (NA2016: 126 Mob-Zeilen, 64 NPC-Dialoge), 'unbenutzt' = Datei ohne Verweis, 'x3' = drei Einträge nutzen das Porträt. 'Nur fehlende Dateien' filtert.
- 'Ersetzen...' schreibt alle vorhandenen Varianten (PNG/TGA) aus derselben Quelle; 'Datei anlegen...' erzeugt eine fehlende Datei im Format der vorhandenen Porträts.
- 'Neues Porträt': Dateiname und optional ein Mob (InxName) bzw. NPC (MobIDX). 'Anlegen...' erzeugt die Datei und setzt die Porträt-Spalte dieses Eintrags (MobViewInfo in Client und Server) - ein Undo-Schritt. Danach 'Alle geänderten SHN speichern'.)MAN",
     R"MAN(Interface tab, switches 'Icons' and 'Portraits'.
# Icons
Every atlas below resmenu/Icon. Each is an 8x8 grid (cell = atlas width / 8: 32 px for 256 atlases, 16 px for the state atlases AbState00-04). The list shows how many of the 64 cells are used.
- Used means: a row in ItemViewInfo (IconFile/IconIndex, SubIcon, PeriodIcon), ActiveSkillView, PassiveSkillView or AbStateView points to the cell. A green corner marks free cells; the tooltip says whether it is empty (fully transparent). Other parts of the client may use atlases too - free only means 'in none of these tables'.
- Click a cell: preview, who uses it (a click jumps into the item, skill, passive or state editor) and 'Replace cell...'. The source is brought to the cell size (default: fit) and the atlas is written in its original format. For DDS only the cell's blocks change in every mipmap level - all other icons stay bit-identical.
- 'Replace atlas...', 'Export as PNG' and 'New empty atlas' (transparent, in the format and size of the selected atlas; the name counts the number up). Enter the new atlas as icon file in the item/skill editor afterwards.
- From the item, skill, passive and state editors, 'In atlas editor' leads straight to the icon's cell.
# Portraits
Mob portraits: resmenu/game/MobPortrait/<MobViewInfo.MobPortrait>.png; NPC faces in dialogs: resmenu/game/NPCFaceCut/<NpcDialogData.FaceCutFile>.png.
- The list combines files and references: 'missing' = reference without file (NA2016: 126 mob rows, 64 NPC dialogs), 'unused' = file without reference, 'x3' = three entries use the portrait. 'Missing files only' filters.
- 'Replace...' writes every existing variant (PNG/TGA) from the same source; 'Create file...' creates a missing file in the format of the existing portraits.
- 'New portrait': file name and optionally a mob (InxName) or NPC (MobIDX). 'Create...' creates the file and sets that entry's portrait column (MobViewInfo on client and server) - one undo step. Then 'Save all changed SHN'.)MAN",
     "icon atlas zelle cell frei free porträt portrait mobportrait npcfacecut gesicht face dds mipmap"},
    {"shn.windows", "shn",
     "Fenster-Layouts", "Window layouts",
     R"MAN(Interface-Tab, Umschalter 'Fenster'. Die Fenster des Clients sind NIF-Dateien unter resmenu (NA2016: 310). Jedes Element ist eine texturierte Fläche in Bildschirmpixeln; die Ansicht zeichnet sie wie im Spiel übereinander.
- Liste links: alle Interface-NIFs. In der Mitte das Fenster; Maus darüber zeigt das Element, Klick wählt es. 'Platzhalter zeigen' macht Flächen ohne Textur sichtbar - dort zeichnet der Client selbst (Text, Item-Slots). 'Umrisse' rahmt alle Elemente.
- Rechts: die Texturen des Fensters mit 'Ersetzen...' und die Elementliste (Größe, Textur). Beim gewählten Element: Position, Größe, sein Ausschnitt in der Textur (Pixel) und 'Ausschnitt ersetzen...'.
- 'Ausschnitt ersetzen...' ersetzt nur die Grafik dieses Elements in der gemeinsamen Textur (Quelle wird auf die Ausschnittgröße gestreckt). Bei TGA bleiben Kopf, Footer und alle übrigen Pixel byte-gleich, bei DDS alle anderen Blöcke. Dieselbe Textur nutzen oft mehrere Fenster (z. B. Maininterface.TGA) - die Änderung gilt dann überall.
- Elemente an derselben Stelle (z. B. die Zustände eines Knopfs: normal, Maus darüber, gedrückt) liegen übereinander; das gewählte Element wird obenauf gezeichnet, die Elementliste wählt jedes einzeln.
- Position und Größe: Element ziehen verschiebt es, die Eck-Griffe des gewählten Elements ändern die Größe (auf ganze Pixel gerundet, Esc bricht ab); Pfeiltasten verschieben um 1 Pixel, mit Umschalt um 10. Rechts stehen X, Y, Breite und Höhe zum Eintippen mit 'Übernehmen'. 'Elemente an derselben Stelle mitnehmen' bewegt Elemente mit genau demselben Rechteck mit - meist die Zustände eines Knopfs (normal, Maus darüber, gedrückt).
- Geschrieben werden nur die Eckpunkte (Vertex-Floats) und die Bounding-Sphere des Datenblocks dieses Elements; Dateigröße und alle übrigen Bytes bleiben gleich. Vor dem Speichern wird die geänderte Datei streng neu geladen und geprüft (Element im Zielrechteck, alle anderen unverändert) - sonst wird nichts geschrieben. Ergebnis als Projektkopie, Strg+Z stellt die vorherige Datei bytegleich wieder her.
- Nicht verschiebbar (Hinweis rechts): Elemente, deren Geometrie mit anderen geteilt ist (NA2016: 47), NiTriStrips- und geskinnte Elemente sowie Dateien, die nur über Wiederherstellungs-Heuristiken laden. NA2016: 11.806 von 12.548 Elementen in allen 310 Fenstern sind verschiebbar.
- Ob der Client Klickflächen genauso aus den Eckpunkten bestimmt wie die Darstellung, ist im Spiel zu prüfen.)MAN",
     R"MAN(Interface tab, switch 'Windows'. The client's windows are NIF files below resmenu (NA2016: 310). Every element is a textured area in screen pixels; the view draws them on top of each other like the game.
- List on the left: every interface NIF. In the middle the window; hovering shows the element, a click selects it. 'Show placeholders' makes untextured areas visible - the client draws there itself (text, item slots). 'Outlines' frames every element.
- On the right: the window's textures with 'Replace...' and the element list (size, texture). For the selected element: position, size, its region in the texture (pixels) and 'Replace region...'.
- 'Replace region...' replaces only this element's graphic in the shared texture (the source is stretched to the region size). For TGA the header, footer and all other pixels stay byte-identical, for DDS all other blocks. Several windows often share a texture (e.g. Maininterface.TGA) - the change then applies everywhere.
- Elements at the same place (e.g. the states of a button: normal, hovered, pressed) lie on top of each other; the selected element is drawn on top, and the element list selects each one.
- Position and size: dragging an element moves it, the corner handles of the selected element resize it (rounded to whole pixels, Esc cancels); arrow keys move by 1 pixel, with Shift by 10. On the right, X, Y, width and height can be typed in and applied with 'Apply'. 'Move elements at the same place along' also moves elements with exactly the same rectangle - usually the states of a button (normal, hovered, pressed).
- Only the corner points (vertex floats) and the bounding sphere of this element's data block are written; file size and all other bytes stay the same. Before saving, the changed file is reloaded strictly and checked (element in the target rectangle, all others unchanged) - otherwise nothing is written. The result is a project copy; Ctrl+Z restores the previous file byte-identically.
- Not movable (note on the right): elements whose geometry is shared with others (NA2016: 47), NiTriStrips and skinned elements, and files that only load through recovery heuristics. NA2016: 11,806 of 12,548 elements in all 310 windows are movable.
- Whether the client derives click areas from the corner points the same way as the display has to be checked in the game.)MAN",
     "fenster window layout nif interface element textur texture ausschnitt region maininterface verschieben move position groesse size"},
    {"creators.npcmob", "creators",
     "Custom NPC / Mob erstellen", "Creating a custom NPC / mob",
     R"MAN(Tab 'Custom NPC/Mob' im SHN-Editor. Der Assistent klont eine Vorlage in alle Tabellen (MobInfo, MobInfoServer, MobViewInfo, MobSpecies, QuestSpecies, MobWeapon) mit einer überall freien ID.
1. Vorlage: NPC oder Monster wählen (Suche).
2. Bezeichnung und Werte: InxName (eindeutig), Anzeigename, ID automatisch; bei Monstern Level, HP, Tempo, Größe.
3. Aussehen: 'Wie Vorlage', 'Anderes Modell' (Modell aus vorhandenen wählen) oder 'Spieler-Avatar mit Rüstung' (Klasse, Geschlecht, Gesicht, Frisur, Haarfarbe, Ausrüstung je Slot - mit 3D-ähnlicher Vorschau, drehbar).
4. NPC: Dialog der Vorlage kopieren; auf der offenen Karte platzieren (Position, Richtung, Rolle, Argument).
'Anlegen' erzeugt alle Zeilen; 'Alle geänderten SHN speichern' schreibt sie. Händler bekommen danach über den NPC-Tab ihr Inventar.
Klassen der Avatare: 0 Fighter, 1 Archer (keine Modelle), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.
Ob das Spiel selbst angelegte Einträge akzeptiert, ist nicht geprüft.)MAN",
     R"MAN(Tab 'Custom NPC/Mob' in the SHN editor. The wizard clones a template into all tables (MobInfo, MobInfoServer, MobViewInfo, MobSpecies, QuestSpecies, MobWeapon) with an ID that is free everywhere.
1. Template: choose an NPC or monster (search).
2. Name and values: InxName (unique), display name, ID automatic; for monsters level, HP, speed, size.
3. Appearance: 'Like template', 'Other model' (choose from existing) or 'Player avatar with armor' (class, gender, face, hair, hair color, equipment per slot - with a rotatable preview).
4. NPC: copy the template's dialog; place on the open map (position, direction, role, argument).
'Create' generates all rows; 'Save all changed SHN' writes them. Merchants then get their inventory via the NPC tab.
Avatar classes: 0 Fighter, 1 Archer (no models), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.
Whether the game accepts self-created entries has not been verified.)MAN",
     "custom npc mob erstellen klonen avatar vorlage"},
    {"creators.skills", "creators",
     "Skill-Editor", "Skill editor",
     R"MAN(Ein Skill ist eine STUFE einer Skillreihe (TripleHit14 = Reihe TripleHit, Stufe 14). Er steckt in ActiveSkill (Client+Server), ActiveSkillInfoServer und ActiveSkillView (Client+Server); lernbare Skills haben ein Skillbuch-Item mit gleichem InxName.
# Ändern
Liste links (Suche nach Name, InxName oder ID), Formular rechts in Bereichen: Grunddaten, Kosten und Zeiten, Schaden, Bewegung/Ziele, Server-Werte, Darstellung, Zustände (Buffs/Debuffs) und 'Alle übrigen Spalten (roh)'. Jede Änderung wird sofort in ALLE Kopien (Client und Server) geschrieben. Der InxName eines vorhandenen Skills bleibt fest.
Icon: Über dem Formular steht das Icon des Skills (ActiveSkillView IconFile/IconIndex, Bild aus resmenu/Icon/<IconFile>.dds, Projekt vor Client). 'Icon wählen...' zeigt den ganzen Atlas; ein Klick setzt IconIndex in Client und Server. Die Datenblätter zeigen Item-Icons ebenso: Quest-Belohnungen, Drop-Gruppen und die Händler-Slots (aus ItemViewInfo.shn).
# Neu aus Vorhandenem
- 'Neuen Skill aus diesem anlegen': klont alle Zeilen (Werte, Server-Werte, Darstellung) mit neuer ID und neuem InxName; optional mit Skillbuch-Item (neue Item-ID).
- 'Nächste Stufe anlegen': legt <Reihe><Stufe+1> an, Voraussetzungs-Skill = dieser Skill.
- Animationen (Zauber-Bereitschaft, Zaubern, Ausführung), Effekte (Geschoss, Treffer, Fläche, Dauerschaden), Sounds und Icon sind NAMEN. Über 'Auswahl...' wählst Du sie aus den bereits verwendeten Werten (Anzahl der Verwendungen in Klammern) und kombinierst so neue Skills aus vorhandenen Animationen und Effekten. Neue Animations-/Effekt-DATEIEN kann der Editor nicht erzeugen.
# Serie skalieren
'Ganze Reihe skalieren': Schaden, Kosten, Abklingzeit oder Zauberzeit aller Stufen einer Reihe um einen Prozentwert ändern.
# Wichtig
Spalten, deren Bedeutung nur aus dem Namen abgeleitet ist, tragen im Tooltip den Zusatz '(vermutet)'. 'Alle geänderten SHN speichern' schreibt alles.)MAN",
     R"MAN(A skill is one STEP of a skill series (TripleHit14 = series TripleHit, step 14). It lives in ActiveSkill (client+server), ActiveSkillInfoServer and ActiveSkillView (client+server); learnable skills have a skill book item with the same InxName.
# Modify
List on the left (search by name, InxName or ID), form on the right in areas: basics, costs and timing, damage, movement/targets, server values, presentation, states (buffs/debuffs) and 'All remaining columns (raw)'. Every change is written immediately to ALL copies (client and server). The InxName of an existing skill stays fixed.
Icon: above the form is the skill's icon (ActiveSkillView IconFile/IconIndex, image from resmenu/Icon/<IconFile>.dds, project before client). 'Choose icon...' shows the whole atlas; a click sets IconIndex on client and server. The data editors show item icons the same way: quest rewards, drop groups and merchant slots (from ItemViewInfo.shn).
# New from existing
- 'Create new skill from this one': clones all rows (values, server values, presentation) with a new ID and new InxName; optionally with a skill book item (new item ID).
- 'Create next step': creates <series><step+1>, required skill = this skill.
- Animations (cast ready, casting, execution), effects (projectile, impact, area, damage over time), sounds and icon are NAMES. With 'Choose...' you pick them from the values already in use (number of uses in brackets) and so combine new skills from existing animations and effects. The editor cannot create new animation/effect FILES.
# Scale a series
'Scale the whole series': change damage, costs, cooldown or cast time of all steps of a series by a percentage.
# Important
Columns whose meaning is only derived from the name carry '(assumed)' in the tooltip. 'Save all changed SHN' writes everything.)MAN",
     "skill editor skills animation effekt buff neu klonen serie stufe"},
    {"creators.shop", "creators",
     "Shop-Editor (Händler-Inventar)", "Shop editor (merchant inventory)",
     R"MAN(Im NPC-Tab bei einem Händler: 'Händler-Inventar bearbeiten'. Die Daten liegen in NPCItemList/<NPC-Name>.txt.
- Ein Shop hat Tabs (Kategorien im Shop-Fenster: Tab00, Tab01 ...). Jeder Tab hat Zeilen (Regale) mit bis zu 6 Slots.
- Slot anklicken: Item aus der Liste wählen (Suche nach Name, Bezeichnung, ID). Rechtsklick leert den Slot. Rot = Item nicht in ItemInfo.
- '+ Zeile', '^' 'v' (verschieben), 'x' (löschen, nur mit Freigabe-Haken). '+ Tab' und 'Diesen Tab löschen'.
- Hat der NPC noch keine Shop-Datei: 'Shop-Datei neu anlegen'.
- 'Speichern' schreibt die Datei.
Alle Händlerarten außer SoulStone benutzen diese Dateien; Skill-Händler verkaufen Skillbücher als Items.)MAN",
     R"MAN(In the NPC tab for a merchant: 'Edit merchant inventory'. The data lives in NPCItemList/<NPC name>.txt.
- A shop has tabs (categories in the shop window: Tab00, Tab01 ...). Each tab has rows (shelves) with up to 6 slots.
- Click a slot: choose an item from the list (search by name, identifier, ID). Right click empties the slot. Red = item not in ItemInfo.
- '+ Row', '^' 'v' (move), 'x' (delete, only with the release checkbox). '+ Tab' and 'Delete this tab'.
- If the NPC has no shop file yet: 'Create shop file'.
- 'Save' writes the file.
All merchant kinds except SoulStone use these files; skill merchants sell skill books as items.)MAN",
     "shop händler npcitemlist inventar item slot tab"},
    {"reference.formats", "reference",
     "Dateiformate und Zuordnung", "File formats and mapping",
     R"MAN(# Karten (Legacy)
- .HTD: Höhenkarte (Blöcke 50 Einheiten). .ini: Kartenbeschreibung mit Layern (Region, Diffuse, UV).
- Texturen: BMP je Layer als Blend-Gewicht.
- .shbd: Block&Walk. Wort = 16 Zellen, Bit gesetzt = blockiert, Zelle = 6,25 Einheiten, Gitter quadratisch.
- .shmd/.idm/.aid: Objekte. Drehung: Quaternion; Editor-Achsen entstehen aus dem Legacy-Format durch Vertauschen (Spiegelung).
# Koordinaten
Welt X = Ost, Welt Z = Nord, Y = Höhe. In NPC.txt sind Coord-X / Coord-Y die Welt-X und Welt-Z. Das 2D-Bild zeigt Norden oben.
# Textdateien (Server)
- World/NPC.txt (MobName, Map, Coord-X, Coord-Y, Direct, NPCMenu, Role, RoleArg0), MobRegen/<Karte>.txt, MobRoam, NPCItemList/<NPC>.txt, Script (Lua).
- Speichern ist minimal: nur geänderte Zeilen werden neu geschrieben, alles andere bleibt byte-gleich (geprüft an allen 680 Textdateien von NA2016).
- ';' leitet auch mitten in einer Zeile einen Kommentar ein. Er ist weder Spalte noch Wert und bleibt beim Bearbeiten der Zeile stehen. Werte jenseits der Spaltenzahl bleiben ebenso unverändert stehen.
- Unter '#delimiter \x20' werden Zeilen ohne Tab an Leerzeichen getrennt (NPC.txt: Xiaoming, Oluming, Toryming in Eld) und beim Bearbeiten mit Leerzeichen geschrieben.
- Texte über mehrere Zeilen (Script/Scenario.txt) werden vollständig gelesen, lassen sich aber nur unverändert speichern; eine Änderung daran meldet einen Fehler.
- Dateien ohne '#Table' (DefaultCharacterData.txt, World/QuestParser.txt) sind keine Tabellen und bleiben unverändert.
# Modelle
- NIF (Gamebryo 10.x/20.0.0.4): Objekte in resmap, Charaktere in reschar, Waffen/Items in resitem. Ein kleiner Teil (unter 10 %) von Sonderdateien - Partikel/Effekte - lädt nicht und erscheint als Platzhalter.)MAN",
     R"MAN(# Maps (legacy)
- .HTD: heightmap (blocks of 50 units). .ini: map description with layers (region, diffuse, UV).
- Textures: BMP per layer as blend weight.
- .shbd: Block&Walk. Word = 16 cells, bit set = blocked, cell = 6.25 units, grid square.
- .shmd/.idm/.aid: objects. Rotation: quaternion; editor axes arise from the legacy format by swapping (mirroring).
# Coordinates
World X = east, world Z = north, Y = height. In NPC.txt Coord-X / Coord-Y are world X and world Z. The 2D image shows north up.
# Text files (server)
- World/NPC.txt (MobName, Map, Coord-X, Coord-Y, Direct, NPCMenu, Role, RoleArg0), MobRegen/<map>.txt, MobRoam, NPCItemList/<NPC>.txt, Script (Lua).
- Saving is minimal: only changed lines are rewritten, everything else stays byte-identical (checked on all 680 NA2016 text files).
- ';' also starts a comment in the middle of a line. It is neither a column nor a value and stays in place when the line is edited. Values beyond the column count also stay unchanged.
- Under '#delimiter \x20', lines without tabs are split at spaces (NPC.txt: Xiaoming, Oluming, Toryming in Eld) and written back with spaces when edited.
- Texts spanning several lines (Script/Scenario.txt) are read in full but can only be saved unchanged; changing one reports an error.
- Files without '#Table' (DefaultCharacterData.txt, World/QuestParser.txt) are not tables and stay unchanged.
# Models
- NIF (Gamebryo 10.x/20.0.0.4): objects in resmap, characters in reschar, weapons/items in resitem. A small part (under 10 %) of special files - particles/effects - does not load and appears as a placeholder.)MAN",
     "format htd shbd shmd nif koordinaten datei"},
    {"reference.trouble", "reference",
     "Fehlersuche", "Troubleshooting",
     R"MAN(- 'Ordner ... nicht gefunden': Client-/Server-Ordner im Projekt-Hub prüfen; die Ordner müssen ressystem/reschar/resitem bzw. 9Data/Shine enthalten.
- NPC ohne Modell im 3D-Bild: im Bereich Sichtbarkeit steht die Liste. Ursachen: Modell liegt nicht in reschar/res*, Name in MobViewInfo.FileName weicht ab, Datei ist ein nicht ladbares Effekt-NIF.
- Objekt als Pyramide statt Modell: die NIF-Datei fehlt oder ist nicht ladbar.
- Wert lässt sich nicht übernehmen: der Typ der Spalte passt nicht (Zahl statt Text, Wert zu groß). Die Statusmeldung nennt den Grund.
- Skill/NPC/Mob wird im Spiel nicht angezeigt: alle geänderten SHN speichern (Client UND Server) und den Client neu starten; der Client zwischenspeichert Tabellen.
- 3D-Kamera reagiert nicht auf Tasten: Maus über das 3D-Bild bewegen und kein Textfeld aktiv haben.)MAN",
     R"MAN(- 'Folder ... not found': check client/server folders in the project hub; the folders must contain ressystem/reschar/resitem or 9Data/Shine.
- NPC without a model in the 3D image: the Visibility area lists them. Causes: the model is not in reschar/res*, the name in MobViewInfo.FileName differs, the file is a non-loadable effect NIF.
- Object shown as a pyramid instead of a model: the NIF file is missing or cannot be loaded.
- A value cannot be applied: the column type does not fit (number instead of text, value too large). The status message names the reason.
- Skill/NPC/mob is not shown in game: save all changed SHN (client AND server) and restart the client; the client caches tables.
- 3D camera does not react to keys: move the mouse over the 3D image and make sure no text field is active.)MAN",
     "fehler problem troubleshooting hilfe nicht gefunden"},
};

struct TipRow { const char* key; const char* de; const char* en; };
const TipRow kTips[] = {
    {"Fenster", "Fenster-NIFs als 2D-Ansicht: Elemente, Texturen, Ausschnitt eines Elements ersetzen.", "Window NIFs as a 2D view: elements, textures, replace an element's region."},
    {"Windows", "Fenster-NIFs als 2D-Ansicht: Elemente, Texturen, Ausschnitt eines Elements ersetzen.", "Window NIFs as a 2D view: elements, textures, replace an element's region."},
    {"Platzhalter zeigen", "Zeigt Flächen ohne Textur (dort zeichnet der Client Text, Slots usw.).", "Shows untextured areas (the client draws text, slots etc. there)."},
    {"Show placeholders", "Zeigt Flächen ohne Textur (dort zeichnet der Client Text, Slots usw.).", "Shows untextured areas (the client draws text, slots etc. there)."},
    {"Umrisse", "Rahmt alle texturierten Elemente.", "Frames every textured element."},
    {"Outlines", "Rahmt alle texturierten Elemente.", "Frames every textured element."},
    {"Ausschnitt ersetzen...", "Ersetzt nur die Grafik dieses Elements in der Textur; alle übrigen Pixel bleiben byte-gleich.", "Replaces only this element's graphic in the texture; all other pixels stay byte-identical."},
    {"Replace region...", "Ersetzt nur die Grafik dieses Elements in der Textur; alle übrigen Pixel bleiben byte-gleich.", "Replaces only this element's graphic in the texture; all other pixels stay byte-identical."},
    {"InputText:##layoutFilter", "Fenster-NIFs nach Namen filtern.", "Filter window NIFs by name."},
    {"InputFloat:##layoutRect", "X, Y, Breite und Höhe des gewählten Elements in Bildschirmpixeln; mit 'Übernehmen' schreiben.", "X, Y, width and height of the selected element in screen pixels; write them with 'Apply'."},
    {"Button:Übernehmen##layoutRect", "Verschiebt/skaliert das Element auf die eingegebenen Werte (Projektkopie, Strg+Z).", "Moves/resizes the element to the entered values (project copy, Ctrl+Z)."},
    {"Elemente an derselben Stelle mitnehmen", "Elemente mit genau demselben Rechteck (Zustände eines Knopfs) wandern mit.", "Elements with exactly the same rectangle (states of a button) move along."},
    {"Icons", "Icon-Atlanten unter resmenu/Icon: Belegung, freie Zellen, Zelle ersetzen, neuer Atlas.", "Icon atlases below resmenu/Icon: usage, free cells, replace a cell, new atlas."},
    {"Porträts", "Mob-Porträts und NPC-Gesichter: Verwendung, fehlende Dateien, ersetzen, neu anlegen.", "Mob portraits and NPC faces: usage, missing files, replace, create."},
    {"Portraits", "Mob-Porträts und NPC-Gesichter: Verwendung, fehlende Dateien, ersetzen, neu anlegen.", "Mob portraits and NPC faces: usage, missing files, replace, create."},
    {"Atlas ersetzen...", "Ersetzt den ganzen Atlas (umgerechnet auf Format und Größe des Originals).", "Replaces the whole atlas (converted to the original's format and size)."},
    {"Replace atlas...", "Ersetzt den ganzen Atlas (umgerechnet auf Format und Größe des Originals).", "Replaces the whole atlas (converted to the original's format and size)."},
    {"Neuer leerer Atlas", "Legt einen transparenten Atlas mit dem eingetragenen Namen an - Format und Größe wie der gewählte.", "Creates a transparent atlas with the entered name - format and size like the selected one."},
    {"New empty atlas", "Legt einen transparenten Atlas mit dem eingetragenen Namen an - Format und Größe wie der gewählte.", "Creates a transparent atlas with the entered name - format and size like the selected one."},
    {"Zelle ersetzen...", "Ersetzt nur diese Zelle; bei DDS bleiben alle anderen Icons bitgleich.", "Replaces only this cell; for DDS all other icons stay bit-identical."},
    {"Replace cell...", "Ersetzt nur diese Zelle; bei DDS bleiben alle anderen Icons bitgleich.", "Replaces only this cell; for DDS all other icons stay bit-identical."},
    {"Im Atlas-Editor", "Öffnet den Icon-Atlas-Editor bei dieser Zelle.", "Opens the icon atlas editor at this cell."},
    {"In atlas editor", "Öffnet den Icon-Atlas-Editor bei dieser Zelle.", "Opens the icon atlas editor at this cell."},
    {"InputText:##iconAtlasFilter", "Atlanten nach Namen filtern.", "Filter atlases by name."},
    {"InputText:##iconAtlasNewName", "Name des neuen Atlas (Buchstaben, Ziffern, _).", "Name of the new atlas (letters, digits, _)."},
    {"Mob-Porträts", "Porträts aus MobViewInfo.MobPortrait (game/MobPortrait).", "Portraits from MobViewInfo.MobPortrait (game/MobPortrait)."},
    {"Mob portraits", "Porträts aus MobViewInfo.MobPortrait (game/MobPortrait).", "Portraits from MobViewInfo.MobPortrait (game/MobPortrait)."},
    {"NPC-Gesichter (Dialog)", "Gesichter aus NpcDialogData.FaceCutFile (game/NPCFaceCut).", "Faces from NpcDialogData.FaceCutFile (game/NPCFaceCut)."},
    {"NPC faces (dialog)", "Gesichter aus NpcDialogData.FaceCutFile (game/NPCFaceCut).", "Faces from NpcDialogData.FaceCutFile (game/NPCFaceCut)."},
    {"Nur fehlende Dateien", "Zeigt nur Porträts, auf die verwiesen wird, deren Datei aber fehlt.", "Shows only referenced portraits whose file is missing."},
    {"Missing files only", "Zeigt nur Porträts, auf die verwiesen wird, deren Datei aber fehlt.", "Shows only referenced portraits whose file is missing."},
    {"Datei anlegen...", "Erzeugt die fehlende Porträt-Datei aus einer Quelle (Format der vorhandenen Porträts).", "Creates the missing portrait file from a source (format of the existing portraits)."},
    {"Create file...", "Erzeugt die fehlende Porträt-Datei aus einer Quelle (Format der vorhandenen Porträts).", "Creates the missing portrait file from a source (format of the existing portraits)."},
    {"Anlegen...", "Erzeugt das neue Porträt und weist es optional dem eingetragenen Mob/NPC zu (ein Undo-Schritt).", "Creates the new portrait and optionally assigns it to the entered mob/NPC (one undo step)."},
    {"Create...", "Erzeugt das neue Porträt und weist es optional dem eingetragenen Mob/NPC zu (ein Undo-Schritt).", "Creates the new portrait and optionally assigns it to the entered mob/NPC (one undo step)."},
    {"InputText:##portraitFilter", "Porträts nach Name oder nach Mob/NPC filtern.", "Filter portraits by name or by mob/NPC."},
    {"InputText:##portraitNewName", "Dateiname des neuen Porträts ohne Endung.", "File name of the new portrait without extension."},
    {"InputText:##portraitAssignTo", "Optional: InxName des Mobs bzw. MobIDX des NPCs, der das neue Porträt bekommt.", "Optional: InxName of the mob or MobIDX of the NPC that gets the new portrait."},
    {"Dateien", "Alle Dateien unter Client/resmenu (Bilder und Fenster-NIFs).", "All files below Client/resmenu (images and window NIFs)."},
    {"Files", "Alle Dateien unter Client/resmenu (Bilder und Fenster-NIFs).", "All files below Client/resmenu (images and window NIFs)."},
    {"Ladebildschirme", "Ladebilder der Karten (MapViewInfo.Loading) zuweisen, ersetzen und abspalten.", "Assign, replace and split the maps' loading images (MapViewInfo.Loading)."},
    {"Loading screens", "Ladebilder der Karten (MapViewInfo.Loading) zuweisen, ersetzen und abspalten.", "Assign, replace and split the maps' loading images (MapViewInfo.Loading)."},
    {"Anderes Bild zuweisen...", "Vorhandenes Ladebild wählen; setzt MapViewInfo.Loading dieser Karte in Client und Server.", "Choose an existing loading image; sets this map's MapViewInfo.Loading on client and server."},
    {"Assign other image...", "Vorhandenes Ladebild wählen; setzt MapViewInfo.Loading dieser Karte in Client und Server.", "Choose an existing loading image; sets this map's MapViewInfo.Loading on client and server."},
    {"Button:Zuweisen##loadingPick", "Weist das gewählte Ladebild der Karte zu (Strg+Z nimmt es zurück).", "Assigns the chosen loading image to the map (Ctrl+Z reverts it)."},
    {"Button:Assign##loadingPick", "Weist das gewählte Ladebild der Karte zu (Strg+Z nimmt es zurück).", "Assigns the chosen loading image to the map (Ctrl+Z reverts it)."},
    {"Bild ersetzen...", "Ersetzt das Ladebild (alle Varianten PNG/TGA) - gilt für jede Karte, die es nutzt.", "Replaces the loading image (all PNG/TGA variants) - applies to every map that uses it."},
    {"Replace image...", "Ersetzt das Ladebild (alle Varianten PNG/TGA) - gilt für jede Karte, die es nutzt.", "Replaces the loading image (all PNG/TGA variants) - applies to every map that uses it."},
    {"Eigenes Bild für diese Karte...", "Legt loading/<Name>.png aus einer Quelle an und weist es nur dieser Karte zu (ein Undo-Schritt).", "Creates loading/<name>.png from a source and assigns it to this map only (one undo step)."},
    {"Own image for this map...", "Legt loading/<Name>.png aus einer Quelle an und weist es nur dieser Karte zu (ein Undo-Schritt).", "Creates loading/<name>.png from a source and assigns it to this map only (one undo step)."},
    {"InputText:##loadingNewName", "Name des neuen Ladebilds (Buchstaben, Ziffern, _ und -); vorgeschlagen wird der Kartenname.", "Name of the new loading image (letters, digits, _ and -); the map name is suggested."},
    {"InputText:##loadingFilter", "Karten nach Name, Anzeigename oder Ladebild filtern.", "Filter maps by name, display name or loading image."},
    {"InputText:##loadingPickFilter", "Ladebilder nach Namen filtern.", "Filter loading images by name."},
    {"Gemeinsame Ladebild-Dateien", "NowLoading, ProgressBar, NowLoadingTextColor und die Tipp-Bilder - gelten für alle Karten.", "NowLoading, ProgressBar, NowLoadingTextColor and the tip images - shared by all maps."},
    {"Shared loading files", "NowLoading, ProgressBar, NowLoadingTextColor und die Tipp-Bilder - gelten für alle Karten.", "NowLoading, ProgressBar, NowLoadingTextColor and the tip images - shared by all maps."},
    {"Save all changed SHN", "Schreibt alle geänderten SHN-Tabellen (Client und Server) auf die Platte.", "Writes all modified SHN tables (client and server) to disk."},
    {"Ersetzen...", "Ersetzt das Bild durch eine beliebige Datei (PNG/JPG/BMP/TGA/DDS); sie wird auf Format und Größe des Originals umgerechnet und als Projektkopie gespeichert.", "Replaces the image with any file (PNG/JPG/BMP/TGA/DDS); it is converted to the original's format and size and stored as a project copy."},
    {"Replace...", "Ersetzt das Bild durch eine beliebige Datei (PNG/JPG/BMP/TGA/DDS); sie wird auf Format und Größe des Originals umgerechnet und als Projektkopie gespeichert.", "Replaces the image with any file (PNG/JPG/BMP/TGA/DDS); it is converted to the original's format and size and stored as a project copy."},
    {"Als PNG exportieren", "Speichert das Bild als PNG unter <Projekt>/Export/resmenu/... (zum Bearbeiten in einem Bildprogramm).", "Saves the image as PNG below <Project>/Export/resmenu/... (for editing in an image editor)."},
    {"Export as PNG", "Speichert das Bild als PNG unter <Projekt>/Export/resmenu/... (zum Bearbeiten in einem Bildprogramm).", "Saves the image as PNG below <Project>/Export/resmenu/... (for editing in an image editor)."},
    {"Projektkopie entfernen", "Löscht die Projektkopie; danach gilt wieder das Original. Strg+Z stellt sie wieder her.", "Deletes the project copy; the original applies again. Ctrl+Z restores it."},
    {"Remove project copy", "Löscht die Projektkopie; danach gilt wieder das Original. Strg+Z stellt sie wieder her.", "Deletes the project copy; the original applies again. Ctrl+Z restores it."},
    {"Durchsuchen...", "Quelldatei im Windows-Dialog wählen.", "Choose the source file in the Windows dialog."},
    {"Browse...", "Quelldatei im Windows-Dialog wählen.", "Choose the source file in the Windows dialog."},
    {"InputText:##imageReplaceSource", "Pfad der Quelldatei (PNG, JPG, BMP, TGA oder DDS). Sobald die Datei existiert, wird sie geladen und umgerechnet.", "Path of the source file (PNG, JPG, BMP, TGA or DDS). As soon as the file exists it is loaded and converted."},
    {"Keep original size", "An: Ergebnis bekommt die Größe des Originals. Aus: Größe der Quelle (DDS auf Vielfache von 4 gerundet).", "On: the result gets the original's size. Off: the source size (DDS rounded to multiples of 4)."},
    {"Anpassung", "Strecken: auf Zielgröße verzerren. Einpassen: ganzes Bild, Ränder transparent. Füllen: alles füllen, Überstand mittig abschneiden.", "Stretch: distort to the target size. Fit: whole image, transparent borders. Fill: fill everything, crop the overhang in the centre."},
    {"Fitting", "Strecken: auf Zielgröße verzerren. Einpassen: ganzes Bild, Ränder transparent. Füllen: alles füllen, Überstand mittig abschneiden.", "Stretch: distort to the target size. Fit: whole image, transparent borders. Fill: fill everything, crop the overhang in the centre."},
    {"Größe des Originals behalten", "An: Ergebnis bekommt die Größe des Originals. Aus: Größe der Quelle (DDS auf Vielfache von 4 gerundet).", "On: the result gets the original's size. Off: the source size (DDS rounded to multiples of 4)."},
    {"Button:Übernehmen##imageReplace", "Schreibt das Ergebnis als Projektkopie (Strg+Z nimmt es zurück).", "Writes the result as a project copy (Ctrl+Z reverts it)."},
    {"Button:Apply##imageReplace", "Schreibt das Ergebnis als Projektkopie (Strg+Z nimmt es zurück).", "Writes the result as a project copy (Ctrl+Z reverts it)."},
    {"Asset-Ordner wählen...", "Ordner mit den Modellen und Texturen (Client) wählen, aus dem NIF-Modelle und Texturen geladen werden.", "Choose the folder with the models and textures (client) from which NIF models and textures are loaded."},
    {"Karte-.ini", "Pfad der Karten-.ini (z.B. resmap/field/Rou/Rou.ini). Sie beschreibt Größe, Texturschichten und Dateien der Karte.", "Path of the map .ini (e.g. resmap/field/Rou/Rou.ini). It describes size, texture layers and files of the map."},
    {"Karte öffnen", "Liest die Karte (Höhe, Texturen, Begehbarkeit, Objekte) aus der angegebenen .ini.", "Reads the map (height, textures, walkability, objects) from the given .ini."},
    {"Ausgabeverzeichnis", "Ordner, in den 'Karte speichern' die Kartendateien schreibt.", "Folder into which 'Save map' writes the map files."},
    {"Wählen...", "Ordner über einen Dialog auswählen.", "Choose a folder via a dialog."},
    {"Kartenname", "Name der Karte = Dateiname der geschriebenen Dateien (ohne Endung).", "Name of the map = file name of the written files (without extension)."},
    {"Karte speichern", "Schreibt die Karte im Originalformat (Höhe, Texturen, Block&Walk, Objekte) in das Ausgabeverzeichnis.", "Writes the map in its original format (height, textures, Block&Walk, objects) to the output directory."},
    {"Neu (257x257)", "Legt eine neue, flache Karte mit 257x257 Höhenpunkten an.", "Creates a new flat map with 257x257 height points."},
    {"Pfad##legacy", "Pfad der Original-Höhendatei (.HTD).", "Path of the original height file (.HTD)."},
    {"Breite (aus .ini)", "Anzahl der Höhenpunkte in X-Richtung (steht in der Karten-.ini).", "Number of height points in X direction (found in the map .ini)."},
    {"Höhe (aus .ini)", "Anzahl der Höhenpunkte in Z-Richtung (steht in der Karten-.ini).", "Number of height points in Z direction (found in the map .ini)."},
    {"Blockbreite", "Größe eines Höhenblocks in X (Welteinheiten, meist 50).", "Size of a height block in X (world units, usually 50)."},
    {"Blockhöhe", "Größe eines Höhenblocks in Z (Welteinheiten, meist 50).", "Size of a height block in Z (world units, usually 50)."},
    {"Importieren##htd", "Liest eine Höhendatei (.HTD) mit den obigen Maßen.", "Reads a height file (.HTD) with the dimensions above."},
    {"Exportieren##htd", "Schreibt die Höhenkarte als Original-.HTD.", "Writes the heightmap as an original .HTD."},
    {"ini-Pfad##legacyTex", "Karten-.ini, aus der die Texturschichten (mit Regionen) importiert werden.", "Map .ini from which the texture layers (with regions) are imported."},
    {"Legacy-Set importieren", "Importiert alle Texturschichten samt Blend-BMPs aus der .ini.", "Imports all texture layers including blend BMPs from the .ini."},
    {"Export-Verzeichnis##legacyTex", "Ordner, in den die Textur-Dateien (.ini und BMPs) exportiert werden.", "Folder to which the texture files (.ini and BMPs) are exported."},
    {"Legacy-Set exportieren", "Schreibt Texturschichten als .ini und Blend-BMPs im Originalformat.", "Writes texture layers as .ini and blend BMPs in the original format."},
    {"Pfad##walkLegacy", "Pfad der Original-Begehbarkeitsdatei (.shbd).", "Path of the original walkability file (.shbd)."},
    {"Breite##walkLegacy", "Breite des Gitters in 16-Bit-Wörtern (je 16 Zellen).", "Grid width in 16-bit words (16 cells each)."},
    {"Höhe##walkLegacy", "Höhe des Gitters in Zeilen (= Zellen).", "Grid height in rows (= cells)."},
    {"Importieren##shbd", "Liest eine .shbd-Datei mit den obigen Maßen.", "Reads a .shbd file with the dimensions above."},
    {"Exportieren##shbd", "Schreibt die Begehbarkeit als Original-.shbd.", "Writes the walkability as an original .shbd."},
    {"Pfad##shmd", "Pfad der Objektdatei (.shmd).", "Path of the object file (.shmd)."},
    {"Importieren##shmd", "Liest die Objektliste aus einer .shmd.", "Reads the object list from a .shmd."},
    {"Exportieren##shmd", "Schreibt die Objektliste als .shmd.", "Writes the object list as a .shmd."},
    {"Pfad##idm", "Pfad der Objekt-Indexdatei (.idm).", "Path of the object index file (.idm)."},
    {"Importieren##idm", "Liest die Objektindexdatei (.idm).", "Reads the object index file (.idm)."},
    {"Exportieren##idm", "Schreibt die Objektindexdatei (.idm).", "Writes the object index file (.idm)."},
    {"Pfad##aid", "Pfad der Objekt-Attributdatei (.aid).", "Path of the object attribute file (.aid)."},
    {"Importieren##aid", "Liest die Objekt-Attributdatei (.aid).", "Reads the object attribute file (.aid)."},
    {"Exportieren##aid", "Schreibt die Objekt-Attributdatei (.aid).", "Writes the object attribute file (.aid)."},
    {"Erweitert (Fiesta Import/Export)", "Einzelne Dateien getrennt importieren/exportieren.", "Import/export single files separately."},
    {"Neu durchsuchen", "Liest die Modell-/Textur-Bibliothek des Asset-Ordners erneut ein.", "Rescans the model/texture library of the asset folder."},
    {"Übernehmen", "Wendet den eingegebenen Wert an. Ungültige Werte werden abgelehnt.", "Applies the entered value. Invalid values are rejected."},
    {"Abbrechen", "Verwirft die Eingabe.", "Discards the input."},
    {"Schließen", "Schließt dieses Fenster.", "Closes this window."},
    {"Entfernen", "Entfernt den gewählten Eintrag.", "Removes the selected entry."},
    {"< Zurück", "Zurück zur vorherigen Ebene.", "Back to the previous level."},
    {"Speichern", "Schreibt die geänderte Datei auf die Platte.", "Writes the modified file to disk."},
    {"CLIENT: SHN-Ordner einlesen", "Liest alle SHN-Tabellen des Client-Ordners (ressystem) ein.", "Reads all SHN tables of the client folder (ressystem)."},
    {"SERVER: SHN-Ordner einlesen", "Liest alle SHN-Tabellen des Server-Ordners (9Data/Shine) ein.", "Reads all SHN tables of the server folder (9Data/Shine)."},
    {"Einzelne SHN öffnen...", "Öffnet eine einzelne SHN-Datei über einen Dateidialog.", "Opens a single SHN file via a file dialog."},
    {"Datei", "Dateiname oder Pfad einer SHN-Datei.", "File name or path of an SHN file."},
    {"Pfad öffnen", "Öffnet die SHN-Datei mit dem angegebenen Pfad.", "Opens the SHN file with the given path."},
    {"Suche", "Filtert die Tabellenzeilen nach dem eingegebenen Text.", "Filters the table rows by the entered text."},
    {"Spalten durchsuchen", "Die Suche prüft auch die Spaltennamen.", "The search also checks the column names."},
    {"Werte durchsuchen", "Die Suche prüft auch die Zellwerte.", "The search also checks the cell values."},
    {"Zelle bearbeiten", "Öffnet das Eingabefeld für die gewählte Zelle (oder Doppelklick auf die Zelle).", "Opens the input field for the selected cell (or double-click the cell)."},
    {"+ Neue Zeile (in Familie propagieren)", "Legt eine Zeile mit automatisch freier ID an - in dieser Tabelle und allen zugehörigen (z.B. ItemInfo + ItemViewInfo).", "Creates a row with an automatically free ID - in this table and all related ones (e.g. ItemInfo + ItemViewInfo)."},
    {"Aufgabe", "Wählt die Aufgabe; relevante Tabellen werden als Kandidaten gelb markiert.", "Chooses the task; relevant tables are marked yellow as candidates."},
    {"% Änderung", "Prozentwert, um den ALLE Zeilen der Spalte verändert werden (z.B. 10 = +10 %, -20 = -20 %).", "Percentage by which ALL rows of the column are changed (e.g. 10 = +10 %, -20 = -20 %)."},
    {"Monster-Ziele", "Zu tötende Monster: aktiv, Mob-ID, Anzahl.", "Monsters to kill: active, mob ID, count."},
    {"Item-Ziele", "Zu sammelnde Items: aktiv, Item-ID, Anzahl.", "Items to collect: active, item ID, count."},
    {"Drops", "Items, die bestimmte Monster für diese Quest fallen lassen: Mob, Item, Menge, Rate.", "Items that certain monsters drop for this quest: mob, item, amount, rate."},
    {"+ Drop hinzufügen", "Fügt einen Drop-Eintrag hinzu (maximal 11).", "Adds a drop entry (maximum 11)."},
    {"SAY-Text-ID nachschlagen", "Gibt eine Text-ID aus einem SAY-Befehl ein und zeigt den Text aus QuestDialog.", "Enter a text ID from a SAY command and show the text from QuestDialog."},
    {"QuestData.shn speichern", "Schreibt alle Quests zurück in QuestData.shn (Server).", "Writes all quests back to QuestData.shn (server)."},
    {"SmallButton:X", "Entfernt diese Zeile.", "Removes this row."},
    {"InputInt:X", "X-Position in Welteinheiten (Ost).", "X position in world units (east)."},
    {"InputInt:Y", "Y-Position in Welteinheiten (Welt-Z, Nord).", "Y position in world units (world Z, north)."},
    {"Zonenname", "Name der Spawn-Zone (Gruppenname in MobRegen).", "Name of the spawn zone (group name in MobRegen)."},
    {"TownPortal.shn speichern", "Schreibt die Stadtportal-Ziele (Client-Datei TownPortal.shn).", "Writes the town portal targets (client file TownPortal.shn)."},
    {"RecallCoord.txt speichern", "Schreibt die Schriftrollen-Ziele (Server-Datei RecallCoord.txt).", "Writes the scroll targets (server file RecallCoord.txt)."},
    {"Mindestlevel", "Charakterlevel, ab dem das Ziel im Menü erscheint.", "Character level from which the target appears in the menu."},
    {"Menü-Gruppe", "Gruppe/Sortierung des Ziels im Portal-Menü.", "Group/sorting of the target in the portal menu."},
    {"Position per Klick im 2D-View setzen", "Danach in die 2D-Ansicht klicken, um die Position zu setzen.", "Then click in the 2D view to set the position."},
    {"TownPortal-Ziel hier hinzufügen", "Legt ein neues Stadtportal-Ziel an und erzeugt fehlende Portalbilder.", "Creates a new town portal target and generates missing portal images."},
    {"Verwerfen und neu laden", "Verwirft alle ungespeicherten Portal-Änderungen und liest neu.", "Discards all unsaved portal changes and rereads."},
    {"Event", "Name des Ereignisses/Skripts, das der NPC ausführt.", "Name of the event/script the NPC runs."},
    {"+ Wegpunkt hinzufügen", "Fügt einen Punkt zur Patrouillenroute hinzu.", "Adds a point to the patrol route."},
    {"+ Button hinzufügen", "Fügt dem Dialog einen weiteren Antwort-Knopf hinzu.", "Adds another answer button to the dialog."},
    {"Ordner wählen...##ressystem", "Ordner ressystem des Clients manuell wählen (enthält NpcDialogData.shn).", "Choose the client's ressystem folder manually (contains NpcDialogData.shn)."},
    {"Ordner wählen...##shinetextnpc", "Ordner 9Data/Shine des Servers manuell wählen (enthält World/NPC.txt).", "Choose the server's 9Data/Shine folder manually (contains World/NPC.txt)."},
    {"Ordner wählen...##shinetextmob", "Ordner 9Data/Shine des Servers manuell wählen (enthält MobRegen).", "Choose the server's 9Data/Shine folder manually (contains MobRegen)."},
    {"Shop-Datei neu anlegen", "Legt für diesen NPC eine neue NPCItemList-Datei mit einem leeren Tab an.", "Creates a new NPCItemList file with an empty tab for this NPC."},
    {"^", "Verschiebt diese Zeile nach oben.", "Moves this row up."},
    {"v", "Verschiebt diese Zeile nach unten.", "Moves this row down."},
    {"x", "Löscht diese Zeile (nur sichtbar, wenn 'Löschen freigeben' gesetzt ist).", "Deletes this row (only visible when 'Allow delete' is ticked)."},
    {"+ Zeile", "Fügt eine neue Zeile (Regal mit 6 Slots) hinzu.", "Adds a new row (shelf with 6 slots)."},
    {"Diesen Tab löschen", "Löscht den ganzen Tab samt Zeilen (nur mit 'Löschen freigeben').", "Deletes the whole tab with its rows (only with 'Allow delete')."},
    {"+ Tab", "Fügt einen neuen Tab (Kategorie im Shop-Fenster) hinzu.", "Adds a new tab (category in the shop window)."},
    {"(leer) -", "Leert den Slot.", "Empties the slot."},
    {"Löschen freigeben", "Sicherung: erst wenn gesetzt, erscheinen Löschen-Knöpfe.", "Safety: delete buttons only appear when this is ticked."},
    {"NPC##kind", "Einen NPC (Händler, Questgeber ...) erstellen.", "Create an NPC (merchant, quest giver ...)."},
    {"Monster##kind", "Ein Monster erstellen.", "Create a monster."},
    {"InxName (eindeutig)", "Interner Schlüsselname; muss in MobInfo eindeutig sein, keine Leerzeichen.", "Internal key name; must be unique in MobInfo, no spaces."},
    {"Anzeigename", "Name, der im Spiel angezeigt wird.", "Name shown in game."},
    {"ID automatisch (erste freie im größten freien Block, in allen Tabellen frei)", "Vergibt die erste in ALLEN beteiligten Tabellen freie ID im größten freien Bereich.", "Assigns the first ID free in ALL involved tables in the largest free block."},
    {"ID", "ID von Hand festlegen (muss in allen Tabellen frei sein).", "Set the ID by hand (must be free in all tables)."},
    {"Level", "Level des Monsters.", "Level of the monster."},
    {"Max. HP", "Maximale Lebenspunkte.", "Maximum hit points."},
    {"Gehtempo", "Gehgeschwindigkeit.", "Walking speed."},
    {"Lauftempo", "Laufgeschwindigkeit.", "Running speed."},
    {"Größe", "Größe/Skalierung des Modells (1000 = normal).", "Size/scaling of the model (1000 = normal)."},
    {"Wie Vorlage", "Aussehen (Modell/Avatar) der Vorlage übernehmen.", "Take the appearance (model/avatar) from the template."},
    {"Anderes Modell", "Ein anderes vorhandenes Modell (MobViewInfo.FileName) verwenden.", "Use another existing model (MobViewInfo.FileName)."},
    {"Spieler-Avatar mit Rüstung", "NPC sieht wie ein Spieler aus (NPCViewInfo): Klasse, Gesicht, Frisur, Rüstung und Waffen.", "NPC looks like a player (NPCViewInfo): class, face, hair, armor and weapons."},
    {"Modell wählen...", "Modell aus den in MobViewInfo verwendeten wählen.", "Choose a model from those used in MobViewInfo."},
    {"Klasse (0-5)", "0 Fighter, 1 Archer (keine Modelle), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.", "0 Fighter, 1 Archer (no models), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel."},
    {"Geschlecht (1=männl., 0=weibl.)", "Geschlecht des Avatars (1 männlich, 0 weiblich; nach ChrCreateEquip.IsMale vermutet).", "Gender of the avatar (1 male, 0 female; assumed from ChrCreateEquip.IsMale)."},
    {"Gesicht", "Gesichtsform (Face00N.nif der Klasse).", "Face shape (Face00N.nif of the class)."},
    {"Frisur", "Frisur (HairInfo-ID).", "Hairstyle (HairInfo ID)."},
    {"Haarfarbe", "Haarfarbe (Nummer).", "Hair color (number)."},
    {"Dialog der Vorlage kopieren (NpcDialogData)", "Kopiert das Gespräch der Vorlage für den neuen NPC.", "Copies the template's conversation for the new NPC."},
    {"Auf der offenen Karte platzieren (World/NPC.txt)", "Trägt den NPC in World/NPC.txt für die geöffnete Karte ein.", "Enters the NPC in World/NPC.txt for the open map."},
    {"Richtung", "Blickrichtung in Grad.", "Facing direction in degrees."},
    {"Kartenmitte", "Setzt die Position auf die Mitte der Karte.", "Sets the position to the center of the map."},
    {"Rolle", "Aufgabe des NPCs: QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate.", "Task of the NPC: QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate."},
    {"Rollenargument (z.B. Quest, Item, Weapon, Skill)", "Zusatz zur Rolle, z.B. Merchant + Item/Weapon/WeaponTitle/Skill/Guild.", "Addition to the role, e.g. Merchant + Item/Weapon/WeaponTitle/Skill/Guild."},
    {"Anlegen", "Erzeugt alle Zeilen in allen Tabellen. Danach 'Alle geänderten SHN speichern'.", "Creates all rows in all tables. Then 'Save all changed SHN'."},
    {"Alle geänderten SHN speichern", "Schreibt alle geänderten SHN-Tabellen (Client und Server) auf die Platte.", "Writes all modified SHN tables (client and server) to disk."},
    {"...", "Wert aus der Liste wählen.", "Choose a value from the list."},
    {"Anheben", "Pinsel hebt das Gelände an.", "Brush raises the terrain."},
    {"Absenken", "Pinsel senkt das Gelände ab.", "Brush lowers the terrain."},
    {"Glätten", "Gleicht die Höhen an die Umgebung an.", "Blends the heights with the surroundings."},
    {"Einebnen", "Setzt das Gelände auf die 'Zielhöhe'.", "Sets the terrain to the 'target height'."},
    {"Radius", "Pinselgröße in Welteinheiten.", "Brush size in world units."},
    {"Stärke", "Wie stark der Pinsel pro Zug wirkt.", "How strongly the brush acts per stroke."},
    {"Zielhöhe", "Höhe, auf die 'Einebnen' das Gelände setzt.", "Height to which 'Flatten' sets the terrain."},
    {"Rückgängig (Strg+Z)", "Nimmt den letzten Höhen-Pinselzug zurück (Strg+Z).", "Reverts the last height brush stroke (Ctrl+Z)."},
    {"Wiederholen (Strg+Y)", "Stellt den zurückgenommenen Pinselzug wieder her (Strg+Y).", "Restores the reverted brush stroke (Ctrl+Y)."},
    {"Name##newLayer", "Name des neuen Layers.", "Name of the new layer."},
    {"Diffuse##newLayer", "Bilddatei (Diffuse-Textur) des neuen Layers.", "Image file (diffuse texture) of the new layer."},
    {"Durchsuchen...##tex", "Bilddatei per Dialog wählen.", "Choose the image file via a dialog."},
    {"UV-Scale##newLayer", "Wie oft sich die Textur über die Karte wiederholt (größer = kleinere Kacheln).", "How often the texture repeats over the map (larger = smaller tiles)."},
    {"Layer hinzufügen", "Legt einen neuen Layer mit diesen Angaben an.", "Creates a new layer with these settings."},
    {"Layer entfernen", "Löscht den gewählten Layer samt seinen Gewichten.", "Deletes the selected layer with its weights."},
    {"Erhöhen", "Pinsel verstärkt den gewählten Layer.", "Brush strengthens the selected layer."},
    {"Senken", "Pinsel schwächt den gewählten Layer.", "Brush weakens the selected layer."},
    {"Radius##tex", "Pinselgröße in Welteinheiten (Textur).", "Brush size in world units (texture)."},
    {"Stärke##tex", "Pinselstärke pro Zug (Textur).", "Brush strength per stroke (texture)."},
    {"Rückgängig (Textur)", "Nimmt den letzten Textur-Pinselzug zurück.", "Reverts the last texture stroke."},
    {"Wiederholen (Textur)", "Stellt den zurückgenommenen Textur-Pinselzug wieder her.", "Restores the reverted texture stroke."},
    {"Sperren (blockiert)", "Pinsel macht Zellen blockiert (nicht begehbar).", "Brush makes cells blocked (not walkable)."},
    {"Freigeben (begehbar)", "Pinsel macht Zellen begehbar.", "Brush makes cells walkable."},
    {"Radius##walk", "Pinselgröße in Welteinheiten (eine Zelle = 6,25).", "Brush size in world units (one cell = 6.25)."},
    {"1 Zelle##walk", "Pinsel trifft nur die Zelle unter dem Zeiger.", "Brush only hits the cell under the pointer."},
    {"Grundflächen sichtbarer Objekte SPERREN", "Sperrt die Zellen unter den Grundflächen aller sichtbaren Objekte (Sichtbarkeit beachten).", "Blocks the cells under the footprints of all visible objects (mind the visibility)."},
    {"Grundflächen sichtbarer Objekte FREIGEBEN", "Gibt die Zellen unter den Grundflächen aller sichtbaren Objekte frei.", "Releases the cells under the footprints of all visible objects."},
    {"Rückgängig (Walk)", "Nimmt die letzte Begehbarkeits-Änderung zurück.", "Reverts the last walkability change."},
    {"Wiederholen (Walk)", "Stellt die zurückgenommene Begehbarkeits-Änderung wieder her.", "Restores the reverted walkability change."},
    {"Platzieren", "Klick in die 2D-Ansicht setzt ein neues Objekt.", "Click in the 2D view places a new object."},
    {"Auswählen", "Klick wählt ein Objekt; Strg erweitert die Auswahl, Ziehen eines bereits ausgewählten Objekts bewegt die ganze Auswahl.", "Click selects an object; Ctrl extends the selection, dragging an already selected object moves the whole selection."},
    {"Alle normalen auswählen", "Wählt alle normalen Placement-Objekte aus (Sky/Water/GroundObject ausgenommen).", "Selects all normal placement objects (excluding Sky/Water/GroundObject)."},
    {"Auswahl aufheben", "Hebt die komplette Objektauswahl auf.", "Clears the complete object selection."},
    {"Modellpfad", "Pfad des Modells (NIF), das platziert wird.", "Path of the model (NIF) to place."},
    {"Modellpfad##selected", "Ändert das Modell des ausgewählten Placement- oder SHMD-Szenenobjekts.", "Changes the model of the selected placement or SHMD scene object."},
    {"Durchsuchen...##nif", "Modell aus der Bibliothek wählen.", "Choose a model from the library."},
    {"Durchsuchen...##selectedNif", "Ersetzt das Modell des ausgewählten Objekts aus der NIF-Bibliothek.", "Replaces the selected object's model from the NIF library."},
    {"Rotation um Hochachse (°)##new", "Drehung neuer Objekte um die senkrechte Achse in Grad.", "Rotation of new objects around the vertical axis in degrees."},
    {"Skalierung##new", "Größenfaktor neuer Objekte.", "Scale factor of new objects."},
    {"Rotation um Hochachse (°)", "Drehung des gewählten Objekts um die senkrechte Achse in Grad.", "Rotation of the selected object around the vertical axis in degrees."},
    {"Skalierung", "Größenfaktor des gewählten Objekts.", "Scale factor of the selected object."},
    {"Objekt löschen", "Löscht das gewählte Objekt.", "Deletes the selected object."},
    {"Ausgewählte Objekte löschen", "Löscht alle ausgewählten Placement- und SHMD-Szenenobjekte.", "Deletes all selected placement and SHMD scene objects."},
    {"Alle normalen Objekte entfernen", "Entfernt nach Bestätigung alle normalen Placements; Sky/Water/GroundObject bleiben erhalten.", "After confirmation removes all normal placements; Sky/Water/GroundObject are kept."},
    {"Coord-X", "Welt-X-Position (Ost) des NPCs.", "World X position (east) of the NPC."},
    {"Coord-Y", "Welt-Z-Position (Nord) des NPCs (in NPC.txt 'Coord-Y').", "World Z position (north) of the NPC (called 'Coord-Y' in NPC.txt)."},
    {"-15", "Dreht die Blickrichtung um -15 Grad.", "Rotates the facing by -15 degrees."},
    {"+15", "Dreht die Blickrichtung um +15 Grad.", "Rotates the facing by +15 degrees."},
    {"+90", "Dreht die Blickrichtung um +90 Grad.", "Rotates the facing by +90 degrees."},
    {"180", "Dreht die Blickrichtung um 180 Grad.", "Rotates the facing by 180 degrees."},
    {"Kamera zu diesem NPC", "Setzt die 3D-Kamera auf den gewählten NPC.", "Puts the 3D camera on the selected NPC."},
    {"Drehsinn", "Zuordnung Richtung -> Blickwinkel: wie angegeben oder gespiegelt (gilt für alle NPCs).", "Mapping direction -> facing angle: as given or mirrored (applies to all NPCs)."},
    {"Blick bei Richtung 0", "Versatz der Blickrichtung bei Richtung 0 (gilt für alle NPCs). Passt den Pfeil an das Spiel an.", "Offset of the facing at direction 0 (applies to all NPCs). Adjusts the arrow to the game."},
    {"Dialog bearbeiten", "Öffnet den Gesprächseditor (Begrüßung und Knöpfe, NpcDialogData).", "Opens the conversation editor (greeting and buttons, NpcDialogData)."},
    {"KI-Skript (Lua) bearbeiten", "Öffnet das Lua-Verhaltensskript des NPCs.", "Opens the NPC's Lua behaviour script."},
    {"Patrouillenroute bearbeiten", "Öffnet die Wegpunkte, die der NPC abläuft.", "Opens the waypoints the NPC walks along."},
    {"Händler-Inventar bearbeiten", "Öffnet den Shop-Editor (NPCItemList) dieses Händlers.", "Opens the shop editor (NPCItemList) of this merchant."},
    {"World/NPC.txt speichern", "Schreibt alle NPC-Änderungen in World/NPC.txt.", "Writes all NPC changes to World/NPC.txt."},
    {"+ Zone (Kopie der gewählten)", "Dupliziert die gewählte Spawn-Zone samt ihren Monstern.", "Duplicates the selected spawn zone including its monsters."},
    {"Zone samt Monstern löschen", "Löscht die Zone und alle ihre Monster (nur mit 'Löschen freigeben').", "Deletes the zone and all its monsters (only with 'Allow delete')."},
    {"KI", "Öffnet das Verhaltensskript dieses Monsters.", "Opens the behaviour script of this monster."},
    {"Route", "Öffnet die Wegpunkte (MobRoam) dieses Monsters.", "Opens the waypoints (MobRoam) of this monster."},
    {"Monster entfernen", "Entfernt dieses Monster aus der Zone.", "Removes this monster from the zone."},
    {"+ Monster hinzufügen", "Fügt der Zone ein neues Monster hinzu.", "Adds a new monster to the zone."},
    {"MobRegen speichern", "Schreibt die Spawn-Zonen in die MobRegen-Datei der Karte.", "Writes the spawn zones to the map's MobRegen file."},
    {"Sichtbarkeit", "Blendet Teile der Karte nur in der Anzeige ein/aus - die Daten bleiben unverändert.", "Shows/hides parts of the map in the display only - the data stays unchanged."},
    {"Terrain", "Blendet das Gelände ein/aus.", "Shows/hides the terrain."},
    {"Objekt-Modelle (3D)", "Blendet alle 3D-Objektmodelle ein/aus, einschließlich der SHMD-Szenenmodelle.", "Shows/hides all 3D object models, including SHMD scene models."},
    {"Objekt-Platzhalter (3D)", "Blendet die Pyramiden für Objekte ohne ladbares Modell ein/aus.", "Shows/hides the pyramids for objects without a loadable model."},
    {"Sky (SHMD)", "Blendet die Modelle aus der SHMD-Kategorie Sky ein/aus.", "Shows/hides models from the SHMD Sky category."},
    {"Water (SHMD)", "Blendet die Modelle aus der SHMD-Kategorie Water ein/aus.", "Shows/hides models from the SHMD Water category."},
    {"GroundObject (SHMD)", "Blendet die Modelle aus der SHMD-Kategorie GroundObject ein/aus.", "Shows/hides models from the SHMD GroundObject category."},
    {"NPC-Modelle (3D)", "Blendet die NPC-Modelle im 3D-Bild ein/aus.", "Shows/hides the NPC models in the 3D image."},
    {"NPC-Namen und Blickpfeile (3D, NPC-Modus)", "Zeigt Namen und Blickpfeile der NPCs im 3D-Bild (nur im NPC-Modus).", "Shows names and facing arrows of NPCs in the 3D image (NPC mode only)."},
    {"Objekte im 2D-View", "Blendet die Objektpunkte in der 2D-Ansicht ein/aus.", "Shows/hides the object dots in the 2D view."},
    {"Alle Layer an", "Zeigt alle Terrain-Layer wieder.", "Shows all terrain layers again."},
    {"nur", "Zeigt nur diese Objekt-Kategorie.", "Shows only this object category."},
    {"Alle an", "Zeigt alle Objekt-Kategorien.", "Shows all object categories."},
    {"Alle aus", "Blendet alle Objekt-Kategorien aus.", "Hides all object categories."},
    {"+##zoom2dIn", "2D-Ansicht vergrößern.", "Zoom the 2D view in."},
    {"-##zoom2dOut", "2D-Ansicht verkleinern.", "Zoom the 2D view out."},
    {"1:1##zoom2dFit", "Ganze Karte anzeigen.", "Show the whole map."},
    {"+##zoomIn", "3D-Ansicht näher heran.", "Move the 3D view closer."},
    {"-##zoomOut", "3D-Ansicht weiter weg.", "Move the 3D view away."},
    {"Ordner", "Ausgabeordner.", "Output folder."},
    {"Name", "Name.", "Name."},
    {"T:workspace.wireframe", "Zeigt das Gelände als Gitternetz (Drahtmodell).", "Shows the terrain as a wire mesh."},
    {"T:workspace.centercamera", "Setzt die 3D-Kamera so, dass die ganze Karte zu sehen ist.", "Puts the 3D camera so that the whole map is visible."},
    {"T:workspace.save", "Speichert die Karte.", "Saves the map."},
    {"T:workspace.saveas", "Speichert die Karte unter neuem Namen/Ordner.", "Saves the map under a new name/folder."},
    {"T:workspace.undo", "Nimmt die letzte Änderung zurück (Strg+Z).", "Reverts the last change (Ctrl+Z)."},
    {"T:workspace.redo", "Stellt die zurückgenommene Änderung wieder her (Strg+Y).", "Restores the reverted change (Ctrl+Y)."},
    {"T:nav.back", "Zurück zur vorherigen Ebene.", "Back to the previous level."},
    {"T:mapeditor.newmap", "Legt eine neue Karte an.", "Creates a new map."},
    {"T:mapeditor.openmap", "Öffnet eine vorhandene Karte.", "Opens an existing map."},
    {"T:mapeditor.createmap", "Erzeugt die neue Karte mit den Angaben.", "Creates the new map with the given settings."},
    {"T:mapeditor.cancel", "Bricht ab.", "Cancels."},
    {"T:mapeditor.open", "Öffnet die gewählte Karte.", "Opens the chosen map."},
    {"T:card.start", "Startet diesen Editor.", "Starts this editor."},
    {"Button:Reittiere & Pets##hubMounts", "Reittiere (Mover) und Haustiere (Pups): alle Tabellen eines Eintrags, Kopieren mit freier Nummer.", "Mounts (movers) and pets (pups): all tables of an entry, copy with a free number."},
    {"Button:Mounts & pets##hubMounts", "Reittiere (Mover) und Haustiere (Pups): alle Tabellen eines Eintrags, Kopieren mit freier Nummer.", "Mounts (movers) and pets (pups): all tables of an entry, copy with a free number."},
    {"Button:Angriffsfolgen##hubAttackSeq", "Angriffsfolgen der Mobs: Skill-Reihenfolge und Regeln.", "Mob attack sequences: skill order and rules."},
    {"Button:Attack sequences##hubAttackSeq", "Angriffsfolgen der Mobs: Skill-Reihenfolge und Regeln.", "Mob attack sequences: skill order and rules."},
    {"Button:Speichern##attackSeq", "Schreibt die Angriffsfolge als Projektkopie (Strg+S).", "Writes the attack sequence as a project copy (Ctrl+S)."},
    {"Button:Save##attackSeq", "Schreibt die Angriffsfolge als Projektkopie (Strg+S).", "Writes the attack sequence as a project copy (Ctrl+S)."},
    {"InputText:##attackSeqNewMob", "InxName des Mobs, für den eine Kopie der geladenen Folge angelegt wird.", "InxName of the mob that gets a copy of the loaded sequence."},
    {"Button:Als Kopie für diesen Mob anlegen##attackSeq", "Kopiert die geladene Angriffsfolge für den eingegebenen Mob (noch nicht gespeichert).", "Copies the loaded attack sequence for the entered mob (not saved yet)."},
    {"Button:Create as copy for this mob##attackSeq", "Kopiert die geladene Angriffsfolge für den eingegebenen Mob (noch nicht gespeichert).", "Copies the loaded attack sequence for the entered mob (not saved yet)."},
    {"InputText:##attackSkillFilter", "Skills nach InxName oder Name filtern.", "Filter skills by InxName or name."},
    {"SmallButton:^##stepUp", "Schritt nach oben verschieben.", "Move the step up."},
    {"SmallButton:v##stepDown", "Schritt nach unten verschieben.", "Move the step down."},
    {"SmallButton:+##stepInsert", "Kopie dieses Schritts direkt dahinter einfügen.", "Insert a copy of this step right behind it."},
    {"SmallButton:X##stepDelete", "Schritt löschen (der letzte Schritt bleibt).", "Delete the step (the last step stays)."},
    {"SmallButton:+ Schritt am Ende##attackSeq", "Hängt eine Kopie des letzten Schritts an.", "Appends a copy of the last step."},
    {"SmallButton:+ Step at the end##attackSeq", "Hängt eine Kopie des letzten Schritts an.", "Appends a copy of the last step."},
    {"SmallButton:X##ruleDelete", "Regel löschen.", "Delete the rule."},
    {"SmallButton:+ Regel##attackSeq", "Neue Regel (legt die Tabelle SkillChange an, falls sie fehlt).", "New rule (creates the SkillChange table if it is missing)."},
    {"SmallButton:+ Rule##attackSeq", "Neue Regel (legt die Tabelle SkillChange an, falls sie fehlt).", "New rule (creates the SkillChange table if it is missing)."},
    {"SmallButton:Angriffsfolge öffnen##mobAttackSeq", "Öffnet die Angriffsfolge dieses Mobs (MobAttackSequence).", "Opens this mob's attack sequence (MobAttackSequence)."},
    {"SmallButton:Open attack sequence##mobAttackSeq", "Öffnet die Angriffsfolge dieses Mobs (MobAttackSequence).", "Opens this mob's attack sequence (MobAttackSequence)."},
    {"Button:Item-Systeme##hubItemSystems", "Item-Systeme: Sets, Aufwertung, Mix, Zufallsoptionen, Pakete.", "Item systems: sets, upgrade, mix, random options, packages."},
    {"Button:Item systems##hubItemSystems", "Item-Systeme: Sets, Aufwertung, Mix, Zufallsoptionen, Pakete.", "Item systems: sets, upgrade, mix, random options, packages."},
    {"Button:Alle geänderten SHN speichern##itemSystems", "Speichert alle geänderten SHN-Dateien als Projektkopie (Strg+S).", "Saves all changed SHN files as a project copy (Ctrl+S)."},
    {"Button:Save all changed SHN##itemSystems", "Speichert alle geänderten SHN-Dateien als Projektkopie (Strg+S).", "Saves all changed SHN files as a project copy (Ctrl+S)."},
    {"Checkbox:Nur mit Problemen##itemSystems", "Nur Einträge mit Verweisen auf unbekannte Items oder Effekte zeigen.", "Show only entries referencing unknown items or effects."},
    {"Checkbox:Problems only##itemSystems", "Nur Einträge mit Verweisen auf unbekannte Items oder Effekte zeigen.", "Show only entries referencing unknown items or effects."},
    {"InputText:##itemSystemNewKey", "Schlüssel für 'Als Kopie anlegen' bzw. 'Gewählten umbenennen'.", "Key for 'Create as copy' or 'Rename selected'."},
    {"Button:Als Kopie anlegen##itemSystems", "Legt den neuen Schlüssel als Kopie des gewählten Eintrags in allen beteiligten Tabellen an (Client und Server, Strg+Z).", "Creates the new key as a copy of the selected entry in all tables involved (client and server, Ctrl+Z)."},
    {"Button:Create as copy##itemSystems", "Legt den neuen Schlüssel als Kopie des gewählten Eintrags in allen beteiligten Tabellen an (Client und Server, Strg+Z).", "Creates the new key as a copy of the selected entry in all tables involved (client and server, Ctrl+Z)."},
    {"Button:Gewählten umbenennen##itemSystems", "Ändert den Schlüssel des gewählten Eintrags in allen Tabellen. Verweise in Items werden nicht angepasst.", "Changes the key of the selected entry in all tables. References in items are not adjusted."},
    {"Button:Rename selected##itemSystems", "Ändert den Schlüssel des gewählten Eintrags in allen Tabellen. Verweise in Items werden nicht angepasst.", "Changes the key of the selected entry in all tables. References in items are not adjusted."},
    {"Button:Gewählten löschen##itemSystems", "Löscht alle Zeilen des gewählten Eintrags in allen Tabellen - mit Rückfrage.", "Deletes all rows of the selected entry in all tables - with confirmation."},
    {"Button:Delete selected##itemSystems", "Löscht alle Zeilen des gewählten Eintrags in allen Tabellen - mit Rückfrage.", "Deletes all rows of the selected entry in all tables - with confirmation."},
    {"Button:Löschen##itemSystemDelete", "Jetzt löschen (Strg+Z nimmt es zurück).", "Delete now (Ctrl+Z reverts it)."},
    {"Button:Delete##itemSystemDelete", "Jetzt löschen (Strg+Z nimmt es zurück).", "Delete now (Ctrl+Z reverts it)."},
    {"Button:Abbrechen##itemSystemDelete", "Nichts löschen.", "Delete nothing."},
    {"Button:Cancel##itemSystemDelete", "Nichts löschen.", "Delete nothing."},
    {"SmallButton:X##groupRow", "Löscht diese Zeile (Client und Server, Strg+Z).", "Deletes this row (client and server, Ctrl+Z)."},
    {"SmallButton:+ Zeile##groupRow", "Kopiert die letzte Zeile des Eintrags und fügt sie direkt dahinter ein.", "Copies the last row of the entry and inserts it right behind it."},
    {"SmallButton:+ Row##groupRow", "Kopiert die letzte Zeile des Eintrags und fügt sie direkt dahinter ein.", "Copies the last row of the entry and inserts it right behind it."},
    {"Button:Texte##hubTexts", "Texte und Lokalisierung: alle Texttabellen, Suche, CSV für Übersetzungen.", "Texts and localization: all text tables, search, CSV for translations."},
    {"Button:Texts##hubTexts", "Texte und Lokalisierung: alle Texttabellen, Suche, CSV für Übersetzungen.", "Texts and localization: all text tables, search, CSV for translations."},
    {"Button:Alle geänderten SHN speichern##texts", "Speichert alle geänderten SHN-Dateien als Projektkopie (Strg+S).", "Saves all changed SHN files as a project copy (Ctrl+S)."},
    {"Button:Save all changed SHN##texts", "Speichert alle geänderten SHN-Dateien als Projektkopie (Strg+S).", "Saves all changed SHN files as a project copy (Ctrl+S)."},
    {"Button:Diese Tabelle exportieren##textCsv", "Schreibt alle Textzellen dieser Tabelle als CSV (table,key,column,text) in die Datei darüber.", "Writes all text cells of this table as CSV (table,key,column,text) to the file above."},
    {"Button:Export this table##textCsv", "Schreibt alle Textzellen dieser Tabelle als CSV (table,key,column,text) in die Datei darüber.", "Writes all text cells of this table as CSV (table,key,column,text) to the file above."},
    {"Button:Alle Tabellen exportieren##textCsv", "Schreibt die Textzellen aller Texttabellen in eine CSV-Datei.", "Writes the text cells of all text tables to one CSV file."},
    {"Button:Export all tables##textCsv", "Schreibt die Textzellen aller Texttabellen in eine CSV-Datei.", "Writes the text cells of all text tables to one CSV file."},
    {"Button:CSV importieren (nur Änderungen)##textCsv", "Liest die CSV-Datei und übernimmt nur Zellen mit anderem Text (Client und Server, ein Undo-Schritt). Probleme werden aufgelistet.", "Reads the CSV file and applies only cells with different text (client and server, one undo step). Problems are listed."},
    {"Button:Import CSV (changes only)##textCsv", "Liest die CSV-Datei und übernimmt nur Zellen mit anderem Text (Client und Server, ein Undo-Schritt). Probleme werden aufgelistet.", "Reads the CSV file and applies only cells with different text (client and server, one undo step). Problems are listed."},
    {"Button:Koreanische Texte übersetzen##koreanApply", "Ersetzt alle koreanischen Zellen dieser Tabelle (Client und Server) durch die englische Übersetzung aus dem Wörterbuch; ein Undo-Schritt. Nur automatisch übersetzte und zu lange Texte werden übersprungen und gemeldet.", "Replaces all Korean cells of this table (client and server) with the English translation from the dictionary; one undo step. Texts translated only automatically and texts that are too long are skipped and reported."},
    {"Button:Translate Korean texts##koreanApply", "Ersetzt alle koreanischen Zellen dieser Tabelle (Client und Server) durch die englische Übersetzung aus dem Wörterbuch; ein Undo-Schritt. Nur automatisch übersetzte und zu lange Texte werden übersprungen und gemeldet.", "Replaces all Korean cells of this table (client and server) with the English translation from the dictionary; one undo step. Texts translated only automatically and texts that are too long are skipped and reported."},
    {"Button:Unbekannte koreanische Texte online übersetzen##koreanOnline", "Sendet die koreanischen Texte ohne Wörterbuch-Eintrag an die Claude-API (eigener API-Schlüssel, Einstellungen) und speichert die Ergebnisse in <Projekt>/Texte/ko_user.tsv. Fragt vorher nach.", "Sends the Korean texts without dictionary entry to the Claude API (your own API key, settings) and saves the results to <project>/Texte/ko_user.tsv. Asks first."},
    {"Button:Translate unknown Korean texts online##koreanOnline", "Sendet die koreanischen Texte ohne Wörterbuch-Eintrag an die Claude-API (eigener API-Schlüssel, Einstellungen) und speichert die Ergebnisse in <Projekt>/Texte/ko_user.tsv. Fragt vorher nach.", "Sends the Korean texts without dictionary entry to the Claude API (your own API key, settings) and saves the results to <project>/Texte/ko_user.tsv. Asks first."},
    {"Button:Senden##koreanOnline", "Startet die Online-Übersetzung im Hintergrund (Blöcke zu 50 Texten).", "Starts the online translation in the background (batches of 50 texts)."},
    {"Button:Send##koreanOnline", "Startet die Online-Übersetzung im Hintergrund (Blöcke zu 50 Texten).", "Starts the online translation in the background (batches of 50 texts)."},
    {"Button:Abbrechen##koreanOnlineConfirm", "Nichts senden.", "Send nothing."},
    {"Button:Cancel##koreanOnlineConfirm", "Nichts senden.", "Send nothing."},
    {"Button:Abbrechen##koreanOnline", "Bricht die Online-Übersetzung nach dem laufenden Block ab; fertige Blöcke bleiben gespeichert.", "Cancels the online translation after the current batch; finished batches stay saved."},
    {"Button:Cancel##koreanOnline", "Bricht die Online-Übersetzung nach dem laufenden Block ab; fertige Blöcke bleiben gespeichert.", "Cancels the online translation after the current batch; finished batches stay saved."},
    {"Button:Übersetzung übernehmen (Englisch)##koreanField", "Schreibt die englische Übersetzung in dieses Feld (Client und Server, Strg+Z). Gesperrt, wenn es nur eine automatische Übersetzung gibt oder sie zu lang ist.", "Writes the English translation into this field (client and server, Ctrl+Z). Disabled when there is only an automatic translation or it is too long."},
    {"Button:Apply translation (English)##koreanField", "Schreibt die englische Übersetzung in dieses Feld (Client und Server, Strg+Z). Gesperrt, wenn es nur eine automatische Übersetzung gibt oder sie zu lang ist.", "Writes the English translation into this field (client and server, Ctrl+Z). Disabled when there is only an automatic translation or it is too long."},
    {"MenuItem:Übersetzung übernehmen (Englisch)", "Schreibt die englische Übersetzung in diese Zelle (bei gleichem Wert auch ins Gegenstück der anderen Seite), Strg+Z.", "Writes the English translation into this cell (also into the counterpart of the other side if its value is identical), Ctrl+Z."},
    {"MenuItem:Apply translation (English)", "Schreibt die englische Übersetzung in diese Zelle (bei gleichem Wert auch ins Gegenstück der anderen Seite), Strg+Z.", "Writes the English translation into this cell (also into the counterpart of the other side if its value is identical), Ctrl+Z."},
    {"InputText:##koreanApiKey", "Eigener API-Schlüssel für die Claude-API (console.anthropic.com), nur für die optionale Online-Übersetzung koreanischer Texte. Wird verdeckt angezeigt.", "Your own API key for the Claude API (console.anthropic.com), only for the optional online translation of Korean texts. Shown masked."},
    {"Button:Speichern##koreanApiKey", "Speichert den Schlüssel in den Editor-Einstellungen (unter Windows verschlüsselt), nie im Projekt.", "Stores the key in the editor settings (encrypted on Windows), never in the project."},
    {"Button:Save##koreanApiKey", "Speichert den Schlüssel in den Editor-Einstellungen (unter Windows verschlüsselt), nie im Projekt.", "Stores the key in the editor settings (encrypted on Windows), never in the project."},
    {"Button:Löschen##koreanApiKey", "Entfernt den gespeicherten Schlüssel.", "Removes the stored key."},
    {"Button:Delete##koreanApiKey", "Entfernt den gespeicherten Schlüssel.", "Removes the stored key."},
    {"InputText:##textCsvPath", "Pfad der CSV-Datei für Export und Import.", "Path of the CSV file for export and import."},
    {"Button:Neue Zeile##texts", "Kopiert die gewählte Zeile mit der nächsten freien ID und leeren Texten (Client und Server).", "Copies the selected row with the next free ID and empty texts (client and server)."},
    {"Button:New row##texts", "Kopiert die gewählte Zeile mit der nächsten freien ID und leeren Texten (Client und Server).", "Copies the selected row with the next free ID and empty texts (client and server)."},
    {"Button:Zeile löschen##texts", "Löscht die gewählte Zeile in Client und Server - mit Rückfrage und Liste der Verwendungen.", "Deletes the selected row on client and server - with confirmation and list of usages."},
    {"Button:Delete row##texts", "Löscht die gewählte Zeile in Client und Server - mit Rückfrage und Liste der Verwendungen.", "Deletes the selected row on client and server - with confirmation and list of usages."},
    {"Button:Löschen##textDelete", "Zeile jetzt löschen (Strg+Z nimmt es zurück).", "Delete the row now (Ctrl+Z reverts it)."},
    {"Button:Delete##textDelete", "Zeile jetzt löschen (Strg+Z nimmt es zurück).", "Delete the row now (Ctrl+Z reverts it)."},
    {"Button:Abbrechen##textDelete", "Nichts löschen.", "Delete nothing."},
    {"Button:Cancel##textDelete", "Nichts löschen.", "Delete nothing."},
    {"ColorEdit3:##envGlobalLight", "GlobalLight der SHMD: Umgebungslicht für alle Flächen (Werte über 1 erlaubt).", "SHMD GlobalLight: ambient light for all surfaces (values above 1 allowed)."},
    {"ColorEdit3:##envDirAmbient", "DirectionLightAmbient der SHMD: zusätzliches Umgebungslicht.", "SHMD DirectionLightAmbient: additional ambient light."},
    {"ColorEdit3:##envDirDiffuse", "DirectionLightDiffuse der SHMD: Farbe des gerichteten Lichts (Sonne).", "SHMD DirectionLightDiffuse: color of the directional light (sun)."},
    {"ColorEdit3:##envFogColor", "Nebelfarbe (Fog, Werte 2–4).", "Fog color (Fog, values 2–4)."},
    {"ColorEdit3:##envBackground", "BackGroundColor der SHMD: Himmelsfarbe hinter allen Modellen.", "SHMD BackGroundColor: sky color behind all models."},
    {"SliderFloat:##envFogDepth", "Fog-Tiefe: Anteil der Sichtweite, über den sich der Nebel aufbaut (0 = kein Nebel).", "Fog depth: share of the view distance over which fog builds up (0 = no fog)."},
    {"DragFloat:##envFrustum", "Frustum: Sichtweite der Karte; der Nebel ist hier voll.", "Frustum: view distance of the map; fog is full here."},
    {"Button:Werte beim Öffnen wiederherstellen##env", "Setzt Licht, Nebel, Hintergrund und Sichtweite auf den Stand beim Öffnen (Strg+Z nimmt es zurück).", "Resets light, fog, background and view distance to the state at open (Ctrl+Z reverts it)."},
    {"Button:Restore values at open##env", "Setzt Licht, Nebel, Hintergrund und Sichtweite auf den Stand beim Öffnen (Strg+Z nimmt es zurück).", "Resets light, fog, background and view distance to the state at open (Ctrl+Z reverts it)."},
    {"SmallButton:Nebelfarbe = Hintergrund##env", "Übernimmt die Hintergrundfarbe als Nebelfarbe (wie in vielen Originalkarten).", "Copies the background color into the fog color (as in many original maps)."},
    {"SmallButton:Fog color = background##env", "Übernimmt die Hintergrundfarbe als Nebelfarbe (wie in vielen Originalkarten).", "Copies the background color into the fog color (as in many original maps)."},
    {"SmallButton:X##envModel", "Entfernt diesen Eintrag aus der Modellliste (Strg+Z nimmt es zurück).", "Removes this entry from the model list (Ctrl+Z reverts it)."},
    {"SmallButton:Modell hinzufügen##envModel", "Hängt den Pfad aus dem Eingabefeld unten an diese Liste an.", "Appends the path from the input field below to this list."},
    {"SmallButton:Add model##envModel", "Hängt den Pfad aus dem Eingabefeld unten an diese Liste an.", "Appends the path from the input field below to this list."},
    {"InputText:##envModelPath", "Modellpfad wie in der SHMD, z. B. resmap\\field\\Rou\\Rou_Sky.nif.", "Model path as in the SHMD, e.g. resmap\\field\\Rou\\Rou_Sky.nif."},
    {"Checkbox:Kartenlicht##env", "3D-Ansicht mit dem Licht aus der SHMD statt Editor-Standardlicht.", "3D view with the light from the SHMD instead of the editor default light."},
    {"Checkbox:Map lighting##env", "3D-Ansicht mit dem Licht aus der SHMD statt Editor-Standardlicht.", "3D view with the light from the SHMD instead of the editor default light."},
    {"Checkbox:Hintergrundfarbe##envPreview", "BackGroundColor als Hintergrund der 3D-Ansicht.", "BackGroundColor as background of the 3D view."},
    {"Checkbox:Background color##envPreview", "BackGroundColor als Hintergrund der 3D-Ansicht.", "BackGroundColor as background of the 3D view."},
    {"Checkbox:Nebel##env", "Nebel in der 3D-Ansicht zeigen (im Spieltest immer an).", "Show fog in the 3D view (always on in playtest)."},
    {"Checkbox:Fog##env", "Nebel in der 3D-Ansicht zeigen (im Spieltest immer an).", "Show fog in the 3D view (always on in playtest)."},
    {"Button:Neue Karte##hubNewMap", "Öffnet den Assistenten für eine neue Karte (Terrain, SHN-Zeilen, Field.txt, Minimap, Ladebild).", "Opens the new map wizard (terrain, SHN rows, Field.txt, minimap, loading image)."},
    {"Button:New map##hubNewMap", "Öffnet den Assistenten für eine neue Karte (Terrain, SHN-Zeilen, Field.txt, Minimap, Ladebild).", "Opens the new map wizard (terrain, SHN rows, Field.txt, minimap, loading image)."},
    {"Button:Karte öffnen##hubOpenMap", "Zeigt die Kartenliste des Clients und die zuletzt geöffneten Karten.", "Shows the client's map list and recently opened maps."},
    {"Button:Open map##hubOpenMap", "Zeigt die Kartenliste des Clients und die zuletzt geöffneten Karten.", "Shows the client's map list and recently opened maps."},
    {"Button:Tabellen##hubShn", "Öffnet eine beliebige SHN-Tabelle als Raster (Client oder Server).", "Opens any SHN table as a grid (client or server)."},
    {"Button:Tables##hubShn", "Öffnet eine beliebige SHN-Tabelle als Raster (Client oder Server).", "Opens any SHN table as a grid (client or server)."},
    {"Button:Items##hubItems", "Item-Editor: ItemInfo, ItemInfoServer und ItemViewInfo gemeinsam.", "Item editor: ItemInfo, ItemInfoServer and ItemViewInfo together."},
    {"Button:Mobs##hubMobs", "Mob-Editor: Werte, Modell und Verwendungen eines Mobs.", "Mob editor: stats, model and usages of a mob."},
    {"Button:Skills##hubSkills", "Skill-Editor: aktive Skills anlegen, klonen und skalieren.", "Skill editor: create, clone and scale active skills."},
    {"Button:Quests##hubQuests", "Quest-Editor: QuestData, Texte, Ziele und Belohnungen.", "Quest editor: QuestData, texts, objectives and rewards."},
    {"Button:Drops##hubDrops", "Drop-Tabellen und Drop-Gruppen der Mobs.", "Mob drop tables and drop groups."},
    {"Button:Interface##hubInterface", "Interface: Ladebilder, Icons, Porträts, Fenster-Layouts (resmenu).", "Interface: loading screens, icons, portraits, window layouts (resmenu)."},
    {"Button:Animationen##hubKfm", "KFM-Animationen: Katalog, Übergänge, Dateiverweise.", "KFM animations: catalog, transitions, file references."},
    {"Button:Animations##hubKfm", "KFM-Animationen: Katalog, Übergänge, Dateiverweise.", "KFM animations: catalog, transitions, file references."},
    {"T:newproject.createsave", "Legt das Projekt an und speichert die Einstellungen.", "Creates the project and saves the settings."},
    {"T:workspace.tab.heightmap", "Höhenkarte bearbeiten (Gelände anheben, senken, glätten).", "Edit the heightmap (raise, lower, smooth terrain)."},
    {"T:workspace.tab.texturing", "Texturschichten bemalen.", "Paint texture layers."},
    {"T:workspace.tab.blockwalk", "Begehbarkeit (Zellen sperren/freigeben).", "Walkability (block/release cells)."},
    {"T:workspace.tab.objects", "Objekte (Modelle) platzieren und ausrichten.", "Place and align objects (models)."},
    {"T:workspace.tab.npcs", "NPCs platzieren, ausrichten und konfigurieren.", "Place, align and configure NPCs."},
    {"T:workspace.tab.npcai", "Noch ohne eigene Werkzeuge: KI im NPC-Tab bearbeiten.", "No tools of its own yet: edit AI in the NPC tab."},
    {"T:workspace.tab.mobs", "Monster-Spawn-Zonen bearbeiten.", "Edit monster spawn zones."},
    {"T:workspace.tab.mobai", "Noch ohne eigene Werkzeuge: KI im Mob-Tab bearbeiten.", "No tools of its own yet: edit AI in the mob tab."},
    {"T:workspace.tab.portals", "Schnellreise-Ziele (TownPortal, Schriftrollen) bearbeiten.", "Edit fast-travel targets (TownPortal, scrolls)."},
    {"Checkbox:##a", "Dieses Ziel ist aktiv (wird von der Quest verwendet).", "This target is active (used by the quest)."},
    {"Checkbox:##daily", "Tägliche Quest: kann jeden Tag wiederholt werden.", "Daily quest: can be repeated every day."},
    {"Checkbox:##enable", "Quest ist aktiviert.", "Quest is enabled."},
    {"Checkbox:##v", "Schalter (an = 1). Erklärung: Mauszeiger über den Feldnamen links.", "Switch (on = 1). Explanation: hover the field name on the left."},
    {"Combo:##lang", "Sprache der Oberfläche, der Tooltips und des Handbuchs.", "Language of the interface, tooltips and manual."},
    {"InputInt:##a", "Zahlenwert.", "Numeric value."},
    {"InputInt:##i", "Item-ID; der Name wird daneben aufgelöst (rot = nicht gefunden).", "Item ID; the name is resolved next to it (red = not found)."},
    {"InputInt:##id", "ID (Mob bzw. Item); der Name wird daneben aufgelöst (rot = nicht gefunden).", "ID (mob or item); the name is resolved next to it (red = not found)."},
    {"InputInt:##m", "Mob-ID (Monster, das den Drop fallen lässt).", "Mob ID (monster that drops the item)."},
    {"InputInt:##maxlv", "Höchstes Charakterlevel für diese Quest.", "Highest character level for this quest."},
    {"InputInt:##minlv", "Niedrigstes Charakterlevel für diese Quest.", "Lowest character level for this quest."},
    {"InputInt:##n", "Anzahl.", "Count."},
    {"InputInt:##newmapx", "Breite der neuen Karte in Höhenpunkten.", "Width of the new map in height points."},
    {"InputInt:##newmapy", "Höhe (Tiefe) der neuen Karte in Höhenpunkten.", "Height (depth) of the new map in height points."},
    {"InputInt:##r", "Rate/Chance (Einheit wie in den Spieldaten).", "Rate/chance (unit as in the game data)."},
    {"InputInt:##v", "Zahlenwert dieses Felds. Beschreibung: Mauszeiger über den Feldnamen links.", "Numeric value of this field. Description: hover the field name on the left."},
    {"InputText:##action", "Aktion bzw. Skript, das beim Klick auf den Knopf ausgeführt wird.", "Action or script executed when the button is clicked."},
    {"InputText:##label", "Beschriftung des Knopfes im Gespräch.", "Label of the button in the conversation."},
    {"InputText:##clientfolder", "Client-Ordner (enthält ressystem, resmap, reschar, resitem ...).", "Client folder (contains ressystem, resmap, reschar, resitem ...)."},
    {"InputText:##serverfolder", "Server-Ordner (enthält 9Data/Shine).", "Server folder (contains 9Data/Shine)."},
    {"InputText:##projfolder", "Ordner, in dem die Projektdatei gespeichert wird.", "Folder in which the project file is saved."},
    {"InputText:##projname", "Name des Projekts.", "Name of the project."},
    {"InputText:##newmaplayer", "Name der ersten Texturschicht der neuen Karte.", "Name of the first texture layer of the new map."},
    {"InputText:##newmapname", "Name der neuen Karte.", "Name of the new map."},
    {"InputText:##newmapdisplay", "Anzeigename der neuen Karte im Spiel (MapInfo.Name); leer = Kartenname.", "Display name of the new map in the game (MapInfo.Name); empty = map name."},
    {"InputText:##newmaploading", "Eigenes Ladebild der neuen Karte (PNG, JPG, BMP, TGA, DDS). Wird auf Größe und Format des Vorlagen-Ladebilds umgerechnet; leer = Kopie des Vorlagen-Ladebilds.", "Own loading image for the new map (PNG, JPG, BMP, TGA, DDS). Converted to the size and format of the template's loading image; empty = copy of the template's image."},
    {"InputText:##filter", "Filtert die Liste nach dem eingegebenen Text.", "Filters the list by the entered text."},
    {"InputText:##itemfilter", "Sucht Items nach Name oder Item-Bezeichnung (InxName).", "Searches items by name or item identifier (InxName)."},
    {"InputText:##pickfilter", "Filtert die Auswahlliste.", "Filters the choice list."},
    {"InputText:##questsearch", "Sucht Quests nach ID oder Text.", "Searches quests by ID or text."},
    {"InputText:##ressystemroot", "Pfad zum Client-Ordner ressystem (enthält NpcDialogData.shn).", "Path to the client folder ressystem (contains NpcDialogData.shn)."},
    {"InputText:##shinetextrootmob", "Pfad zum Server-Ordner 9Data/Shine (enthält MobRegen).", "Path to the server folder 9Data/Shine (contains MobRegen)."},
    {"InputText:##shinetextrootnpc", "Pfad zum Server-Ordner 9Data/Shine (enthält World/NPC.txt).", "Path to the server folder 9Data/Shine (contains World/NPC.txt)."},
    {"InputText:##shncell", "Neuer Wert der gewählten Zelle. Enter oder 'Übernehmen' wendet ihn an.", "New value of the selected cell. Enter or 'Apply' applies it."},
    {"InputText:##skillfilter", "Sucht Skills nach Name, InxName oder ID.", "Searches skills by name, InxName or ID."},
    {"InputText:##tplfilter", "Sucht die Vorlage nach Name oder InxName.", "Searches the template by name or InxName."},
    {"InputText:##n", "Name.", "Name."},
    {"InputText:##v", "Wert dieses Felds. Beschreibung: Mauszeiger über den Feldnamen links.", "Value of this field. Description: hover the field name on the left."},
    {"InputText:##newmob", "Name (MobIndex) des Monsters, das der Zone hinzugefügt wird.", "Name (MobIndex) of the monster added to the zone."},
    {"SliderFloat:##avyaw", "Dreht die Avatar-Vorschau.", "Rotates the avatar preview."},
    {"SliderInt:##scalepct", "Prozentwert der Skalierung (100 = unverändert).", "Percentage of the scaling (100 = unchanged)."},
    {"Versatz aus dieser Karte schaetzen (Block&Walk)", "Testet für alle NPCs dieser Karte, welcher Versatz sie im Schnitt am ehesten von der nächsten Wand weg blicken lässt (Annahme: Rücken zur Wand). Grobe Schätzung.", "For all NPCs on this map, tests which offset most often makes them face away from the nearest wall on average (assumption: back to the wall). A rough estimate."},
    {"Versatz bei Richtung 0 (Grad)", "Versatz der Zuordnung Richtung -> Blickwinkel bei Richtung 0 (gilt für alle NPCs). Per Block&Walk-Analyse auf 0 Grad voreingestellt, hier frei einstellbar.", "Offset of the direction -> facing-angle mapping at direction 0 (applies to all NPCs). Preset to 0 degrees from a Block&Walk analysis, freely adjustable here."},
    {"SmallButton:0##offreset", "Setzt den Versatz auf den Standardwert 0 Grad zurück.", "Resets the offset to the default of 0 degrees."},
    {"RadioButton:Spiel", "Zeigt die Spiel-Minimap (resmenu/minimap/<Karte>.dds, Projekt vor Client) mit ihrem MapViewInfo-Ausschnitt.", "Shows the game minimap (resmenu/minimap/<map>.dds, project before client) with its MapViewInfo crop."},
    {"RadioButton:Vorschau", "Zeigt die Editor-Draufsicht der ganzen Karte statt der Spiel-Minimap.", "Shows the editor's top-down view of the whole map instead of the game minimap."},
    {"Minimap erzeugen", "Rendert die Karte von oben, schreibt resmenu/minimap/<Karte>.dds (512x512 DXT1) ins Projekt und setzt in MapViewInfo MinimapView 1 und den Ausschnitt 0,0-511,511.", "Renders the map from above, writes resmenu/minimap/<map>.dds (512x512 DXT1) to the project and sets MinimapView 1 and the crop 0,0-511,511 in MapViewInfo."},
    {"Stil der Originale", "Erzeugte Minimap im Stil der handgemalten Originale: dunkle Konturen an der Grenze begehbar/blockiert, dunklere blockierte Flächen, Pergament-Rand.", "Generated minimap in the style of the hand-painted originals: dark outlines along the walkable border, darker blocked areas, parchment frame."},
    {"mit Objekten", "Objekte und Wasser in die erzeugte Minimap mitzeichnen (Himmel nie).", "Draw objects and water into the generated minimap (never the sky)."},
    {"Portalbilder erzeugen", "Nimmt den Ankunftsort des Stadtportals auf und schreibt resmenu/TownPortal/<Karte>.dds und <Karte>_disable.dds (256x256 DXT5 mit Mipmaps, abgerundete Ecken, graue Variante) ins Projekt.", "Captures the town portal's arrival point and writes resmenu/TownPortal/<map>.dds and <map>_disable.dds (256x256 DXT5 with mipmaps, rounded corners, grey variant) to the project."},
    {"Blickrichtung", "Richtung, in die die Portal-Aufnahme schaut (0 = nach Norden).", "Direction the portal capture looks in (0 = north)."},
    {"Icon wählen...", "Zeigt den Icon-Atlas (resmenu/Icon/<Icon-Datei>.dds); ein Klick auf ein Icon setzt IconIndex.", "Shows the icon atlas (resmenu/Icon/<icon file>.dds); clicking an icon sets IconIndex."},
    {"Icon-Auswahl schließen", "Blendet den Icon-Atlas wieder aus.", "Hides the icon atlas again."},
};

const ColumnDoc kColumns[] = {
    {"*", "ID", true, "Nummer der Zeile (eindeutig). Andere Tabellen verweisen darüber.", "Number of the row (unique). Other tables refer to it."},
    {"*", "InxName", true, "Interner Textschlüssel. Andere Tabellen verweisen darüber; vorhandene nicht umbenennen.", "Internal text key. Other tables refer to it; do not rename existing ones."},
    {"*", "Name", true, "Anzeigename im Spiel.", "Display name in game."},
    {"MobInfo", "Level", true, "Level des Mobs/NPCs.", "Level of the mob/NPC."},
    {"MobInfo", "MaxHP", true, "Maximale Lebenspunkte.", "Maximum hit points."},
    {"MobInfo", "WalkSpeed", true, "Gehgeschwindigkeit.", "Walking speed."},
    {"MobInfo", "RunSpeed", true, "Laufgeschwindigkeit.", "Running speed."},
    {"MobInfo", "IsNPC", true, "1 = NPC, 0 = Monster.", "1 = NPC, 0 = monster."},
    {"MobInfo", "Size", false, "Größe des Modells (1000 = normal).", "Size of the model (1000 = normal)."},
    {"MobInfo", "WeaponType", false, "Art der geführten Waffe.", "Kind of wielded weapon."},
    {"MobInfo", "ArmorType", false, "Art der Rüstung.", "Kind of armor."},
    {"MobInfo", "GradeType", false, "Rang/Klasse des Mobs (normal, Elite, Boss ...).", "Rank/class of the mob (normal, elite, boss ...)."},
    {"MobInfo", "IsPlayerSide", false, "Steht auf der Seite der Spieler.", "Is on the players' side."},
    {"MobInfo", "AbsoluteSize", false, "Absolute Größe (ohne Skalierung).", "Absolute size (without scaling)."},
    {"MobInfoServer", "MonEXP", true, "Erfahrungspunkte beim Töten (XP Rate Editor ändert diese Spalte).", "Experience for a kill (the XP Rate Editor changes this column)."},
    {"MobInfoServer", "EXPRange", true, "Streubereich der Erfahrung (wird vom XP Rate Editor mitskaliert).", "Spread of the experience (scaled together by the XP Rate Editor)."},
    {"MobInfoServer", "AC", false, "Rüstungswert.", "Armor value."},
    {"MobInfoServer", "MR", false, "Magieresistenz.", "Magic resistance."},
    {"MobInfoServer", "TB", false, "Blockchance/Ausweichwert (TB).", "Block/evade value (TB)."},
    {"MobInfoServer", "MB", false, "Magischer Block (MB).", "Magic block (MB)."},
    {"MobInfoServer", "Visible", false, "Sichtbar für Spieler.", "Visible to players."},
    {"MobInfoServer", "EnemyDetectType", false, "Art der Gegnererkennung.", "Kind of enemy detection."},
    {"MobInfoServer", "DetectCha", false, "Erkennungs-/Aggro-Reichweite.", "Detection/aggro range."},
    {"MobInfoServer", "IsRoaming", false, "1 = wandert umher.", "1 = roams around."},
    {"MobInfoServer", "RoamingNumber", false, "Anzahl der Wanderziele.", "Number of roaming targets."},
    {"MobInfoServer", "RoamingDistance", false, "Wanderentfernung.", "Roaming distance."},
    {"MobInfoServer", "RoamingRestTime", false, "Ruhezeit zwischen Wanderungen.", "Rest time between roams."},
    {"MobInfoServer", "Str", false, "Attribut Stärke.", "Attribute strength."},
    {"MobInfoServer", "Dex", false, "Attribut Geschick.", "Attribute dexterity."},
    {"MobInfoServer", "Con", false, "Attribut Konstitution.", "Attribute constitution."},
    {"MobInfoServer", "Int", false, "Attribut Intelligenz.", "Attribute intelligence."},
    {"MobInfoServer", "Men", false, "Attribut Mentalität.", "Attribute mentality."},
    {"MobInfoServer", "MobKillInx", false, "Zähl-/Drop-Gruppe beim Töten.", "Kill counter/drop group."},
    {"MobInfoServer", "Rank", false, "Rang des Mobs.", "Rank of the mob."},
    {"MobInfoServer", "MaxSP", false, "Maximale SP.", "Maximum SP."},
    {"MobViewInfo", "FileName", true, "Modell des Mobs/NPCs: reschar/<FileName>/<FileName>.nif (oder ein anderer res*-Ordner).", "Model of the mob/NPC: reschar/<FileName>/<FileName>.nif (or another res* folder)."},
    {"MobViewInfo", "Texture", false, "Alternative Textur des Modells.", "Alternative texture of the model."},
    {"MobViewInfo", "NpcViewIndex", true, "0 = normales Modell (FileName). Sonst Verweis auf NPCViewInfo.TypeIndex (Spieler-Aussehen).", "0 = normal model (FileName). Otherwise reference to NPCViewInfo.TypeIndex (player look)."},
    {"MobViewInfo", "MobPortrait", false, "Porträtbild im Spiel.", "Portrait image in game."},
    {"MobViewInfo", "MiniMapIcon", false, "Symbol auf der Minikarte.", "Icon on the minimap."},
    {"MobViewInfo", "BoundingBox", false, "Größe des Auswahlkörpers.", "Size of the selection volume."},
    {"MobViewInfo", "AttackType", false, "Art des Angriffs (Nahkampf/Fernkampf).", "Kind of attack (melee/ranged)."},
    {"MobViewInfo", "ShotEffect", false, "Geschoss-Effekt bei Fernangriff.", "Projectile effect for ranged attacks."},
    {"NPCViewInfo", "TypeIndex", true, "Nummer des Spieler-Aussehens (Ziel von MobViewInfo.NpcViewIndex).", "Number of the player look (target of MobViewInfo.NpcViewIndex)."},
    {"NPCViewInfo", "Class", true, "Klasse 0-5: 0 Fighter, 1 Archer, 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.", "Class 0-5: 0 fighter, 1 archer, 2 cleric, 3 mage, 4 joker, 5 sentinel."},
    {"NPCViewInfo", "Gender", false, "Geschlecht (1 männlich, 0 weiblich - aus ChrCreateEquip abgeleitet).", "Gender (1 male, 0 female - derived from ChrCreateEquip)."},
    {"NPCViewInfo", "FaceShape", true, "Gesichtsform (Face00N.nif).", "Face shape (Face00N.nif)."},
    {"NPCViewInfo", "HairType", true, "Frisur (HairInfo-ID).", "Hairstyle (HairInfo ID)."},
    {"NPCViewInfo", "HairColor", false, "Haarfarbe.", "Hair color."},
    {"NPCViewInfo", "BaseActionCode", false, "Standard-Animation im Stand.", "Standard idle animation."},
    {"NPCViewInfo", "PeriodActionCode", false, "Gelegentliche Zusatz-Animation.", "Occasional extra animation."},
    {"NPCViewInfo", "ActionDelayTime", false, "Pause zwischen Zusatz-Animationen.", "Pause between extra animations."},
    {"NPCViewInfo", "bUseEventAction", false, "Ereignis-Animationen verwenden.", "Use event animations."},
    {"NPCViewInfo", "Equ_RightHand", true, "Item (InxName) in der rechten Hand.", "Item (InxName) in the right hand."},
    {"NPCViewInfo", "Equ_LeftHand", true, "Item (InxName) in der linken Hand (Schild).", "Item (InxName) in the left hand (shield)."},
    {"NPCViewInfo", "Equ_Body", true, "Rüstungsteil Körper (Item-InxName).", "Body armor piece (item InxName)."},
    {"NPCViewInfo", "Equ_Leg", true, "Rüstungsteil Beine (Item-InxName).", "Leg armor piece (item InxName)."},
    {"NPCViewInfo", "Equ_Shoes", true, "Rüstungsteil Schuhe (Item-InxName).", "Shoe armor piece (item InxName)."},
    {"NpcDialogData", "MobIDX", true, "InxName des NPCs, zu dem das Gespräch gehört.", "InxName of the NPC the conversation belongs to."},
    {"NpcDialogData", "FaceCutFile", false, "Porträtbild im Gesprächsfenster.", "Portrait image in the conversation window."},
    {"NpcDialogData", "Dialog", true, "Gesprächstext: Begrüßung und Antwort-Knöpfe (im Dialog-Editor bearbeitbar).", "Conversation text: greeting and answer buttons (editable in the dialog editor)."},
    {"ItemInfo", "BuyPrice", true, "Kaufpreis beim Händler (Buy & Sell Editor ändert diese Spalte).", "Buy price at the merchant (the Buy & Sell Editor changes this column)."},
    {"ItemInfo", "SellPrice", true, "Verkaufspreis an den Händler (Buy & Sell Editor).", "Sell price to the merchant (Buy & Sell Editor)."},
    {"ItemInfo", "MinWC", false, "Minimaler Waffenschaden.", "Minimum weapon damage."},
    {"ItemInfo", "MaxWC", false, "Maximaler Waffenschaden.", "Maximum weapon damage."},
    {"ItemInfo", "MinMA", false, "Minimaler magischer Schaden.", "Minimum magic damage."},
    {"ItemInfo", "MaxMA", false, "Maximaler magischer Schaden.", "Maximum magic damage."},
    {"ItemInfo", "AC", false, "Rüstungswert.", "Armor value."},
    {"ItemInfo", "MR", false, "Magieresistenz.", "Magic resistance."},
    {"ItemInfo", "DemandLv", false, "Benötigtes Charakterlevel.", "Required character level."},
    {"ItemInfo", "MaxLot", false, "Maximale Stapelgröße.", "Maximum stack size."},
    {"ItemInfo", "Equip", false, "Ausrüstungs-Slot (Aufzählung).", "Equipment slot (enum)."},
    {"ItemInfo", "UseClass", false, "Klassen, die das Item nutzen dürfen (Bitmaske).", "Classes allowed to use the item (bitmask)."},
    {"PassiveSkill", "DemandSk", true, "Passiv-Skill (InxName), der vorher gelernt sein muss - in 470 von 503 Zeilen die Vorstufe, sonst '-'.", "Passive skill (InxName) that must be learned first - the previous step in 470 of 503 rows, otherwise '-'."},
    {"AbState", "SubAbState", true, "InxName der SubAbState-Zeilen mit den Werten je Stärkestufe (775 von 777 Zeilen lösen auf).", "InxName of the SubAbState rows with the values per strength level (775 of 777 rows resolve)."},
    {"AbStateView", "IconSort", true, "BUFF oder DEBUFF.", "BUFF or DEBUFF."},
    {"AbStateView", "iconFile", true, "Icon-Atlas (resmenu/Icon, AbState00-04: 128x128, 16-px-Zellen).", "Icon atlas (resmenu/Icon, AbState00-04: 128x128, 16 px cells)."},
    {"AbStateView", "icon", true, "Position im Icon-Atlas (zeilenweise, 8 Spalten).", "Position in the icon atlas (row by row, 8 columns)."},
    {"SubAbState", "Strength", true, "Stärkestufe dieser Zeile (1..n innerhalb desselben InxName).", "Strength level of this row (1..n within the same InxName)."},
    {"SubAbState", "KeepTime", false, "Dauer in ms; 4000000000 = dauerhaft.", "Duration in ms; 4000000000 = permanent."},
    {"ItemInfoServer", "MarketIndex", true, "Markt-Kategorie: 'Skill' = Skillbuch (2259 Items), 'Weapon', 'Item', 'WeaponTitle', leer.", "Market category: 'Skill' = skill book (2259 items), 'Weapon', 'Item', 'WeaponTitle', empty."},
    {"ItemInfoServer", "DropGroupA", true, "Name einer Item-Menge; ItemDropGroup-Zeilen mit diesem Namen als ItemID droppen eines dieser Items.", "Name of an item set; ItemDropGroup rows with this name as ItemID drop one of these items."},
    {"ItemInfoServer", "DropGroupB", true, "Zweite Item-Menge (wie DropGroupA).", "Second item set (like DropGroupA)."},
    {"ItemInfo", "AtkSpeed", false, "Angriffsgeschwindigkeit der Waffe.", "Attack speed of the weapon."},
    {"ItemInfo", "TwoHand", false, "1 = Zweihandwaffe.", "1 = two-handed weapon."},
    {"ItemViewInfo", "IconFile", true, "Icon-Bilddatei des Items.", "Icon image file of the item."},
    {"ItemViewInfo", "IconIndex", true, "Position des Icons in der Bilddatei.", "Position of the icon in the image file."},
    {"ItemViewInfo", "LinkFile", true, "Modell einer Waffe/eines Schilds: resitem/<LinkFile>.nif.", "Model of a weapon/shield: resitem/<LinkFile>.nif."},
    {"ItemViewInfo", "TextureFile", true, "Rüstungstextur im Klassenordner (reschar/<Klasse>-<m|f>/<TextureFile>.dds).", "Armor texture in the class folder (reschar/<class>-<m|f>/<TextureFile>.dds)."},
    {"ItemViewInfo", "MSetNo", true, "Nummer des Rüstungs-Modells für männliche Charaktere (setNNN.nif).", "Number of the armor model for male characters (setNNN.nif)."},
    {"ItemViewInfo", "FSetNo", true, "Nummer des Rüstungs-Modells für weibliche Charaktere (setNNN.nif).", "Number of the armor model for female characters (setNNN.nif)."},
    {"ItemViewInfo", "EquipType", false, "Art der Ausrüstung für die Darstellung (Aufzählung).", "Kind of equipment for the display (enum)."},
    {"ItemViewInfo", "Descript", false, "Beschreibungstext.", "Description text."},
    {"MapInfo", "MapName", false, "Interner Kartenname (Ordner-/Dateiname).", "Internal map name (folder/file name)."},
    {"MapInfo", "RegenX", false, "Start-/Wiederbelebungspunkt X.", "Start/respawn point X."},
    {"MapInfo", "RegenY", false, "Start-/Wiederbelebungspunkt Y (Welt-Z).", "Start/respawn point Y (world Z)."},
    {"MapInfo", "MapFolderName", false, "Ordnername der Karte im Client.", "Folder name of the map in the client."},
    {"MapInfo", "InSide", false, "1 = Innenraum-Karte.", "1 = indoor map."},
    {"MapInfo", "Sight", false, "Sichtweite.", "View distance."},
    {"MobWeapon", "Skill", false, "Skill, den diese Waffenzeile einsetzt.", "Skill this weapon row uses."},
    {"MobWeapon", "MinWC", false, "Minimaler Schaden.", "Minimum damage."},
    {"MobWeapon", "MaxWC", false, "Maximaler Schaden.", "Maximum damage."},
    {"MobWeapon", "Range", false, "Reichweite.", "Range."},
    {"MobWeapon", "AtkSpd", false, "Angriffsgeschwindigkeit.", "Attack speed."},
    {"MobWeapon", "SwingTime", false, "Dauer der Angriffsbewegung (ms).", "Duration of the attack motion (ms)."},
    {"MobWeapon", "HitTime", false, "Trefferzeitpunkt in der Animation (ms).", "Hit moment within the animation (ms)."},
    {"ShineNPC", "MobName", true, "InxName des NPCs (Verweis auf MobInfo).", "InxName of the NPC (reference to MobInfo)."},
    {"ShineNPC", "Map", true, "Karte, auf der der NPC steht.", "Map the NPC stands on."},
    {"ShineNPC", "Coord-X", true, "Welt-X (Ost).", "World X (east)."},
    {"ShineNPC", "Coord-Y", true, "Welt-Z (Nord).", "World Z (north)."},
    {"ShineNPC", "Direct", true, "Blickrichtung in Grad.", "Facing direction in degrees."},
    {"ShineNPC", "NPCMenu", false, "Menü-Kennzeichen (meist 1).", "Menu flag (usually 1)."},
    {"ShineNPC", "Role", true, "Rolle: QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate.", "Role: QuestNpc, Merchant, Guard, NPCMenu, StoreManager, Gate."},
    {"ShineNPC", "RoleArg0", true, "Rollenargument (z.B. Merchant + Item/Weapon/WeaponTitle/Skill/Guild).", "Role argument (e.g. Merchant + Item/Weapon/WeaponTitle/Skill/Guild)."},
    {"ActiveSkill", "Name", true, "Anzeigename des Skills im Spiel (z.B. 'Slice and Dice [14]').", "Display name of the skill in game (e.g. 'Slice and Dice [14]')."},
    {"ActiveSkill", "Grade", false, "Rang der Skillreihe (vermutet).", "Rank of the skill series (assumed)."},
    {"ActiveSkill", "Step", true, "Stufe dieses Skills innerhalb seiner Reihe (TripleHit14 = Stufe 14).", "Level of this skill within its series (TripleHit14 = step 14)."},
    {"ActiveSkill", "MaxStep", true, "Höchste Stufe der Reihe.", "Highest step of the series."},
    {"ActiveSkill", "DemandType", false, "Art der Lernvoraussetzung (Aufzählung, Wert aus vorhandenen Skills übernehmen).", "Kind of learning requirement (enum; copy the value from existing skills)."},
    {"ActiveSkill", "DemandSk", true, "Skill (InxName), der vorher gelernt sein muss - meist die Vorstufe.", "Skill (InxName) that must be learned first - usually the previous step."},
    {"ActiveSkill", "UseClass", true, "Welche Klassen den Skill nutzen dürfen (Bitmaske; 7 = mehrere Klassen, Werte aus vorhandenen Skills übernehmen).", "Which classes may use the skill (bitmask; copy values from existing skills)."},
    {"ActiveSkill", "Range", true, "Reichweite in Spieleinheiten; 0 = Nahkampf/Standard.", "Range in game units; 0 = melee/default."},
    {"ActiveSkill", "Area", true, "Radius bei Flächenskills; 0 = Einzelziel.", "Radius for area skills; 0 = single target."},
    {"ActiveSkill", "TargetNumber", true, "Wie viele Ziele der Skill trifft.", "How many targets the skill hits."},
    {"ActiveSkill", "SP", true, "Verbrauch von SP (Skill-/Manapunkte).", "SP consumption (skill/mana points)."},
    {"ActiveSkill", "SPRate", false, "Zusätzlicher prozentualer SP-Verbrauch (vermutet).", "Additional percentage SP consumption (assumed)."},
    {"ActiveSkill", "HP", true, "Verbrauch von HP.", "HP consumption."},
    {"ActiveSkill", "HPRate", false, "Prozentualer HP-Verbrauch (vermutet).", "Percentage HP consumption (assumed)."},
    {"ActiveSkill", "LP", false, "Verbrauch von LP (vermutet: Licht-/Sonderpunkte).", "LP consumption (assumed: special points)."},
    {"ActiveSkill", "CastTime", true, "Zeit bis der Skill auslöst, in Millisekunden.", "Time until the skill triggers, in milliseconds."},
    {"ActiveSkill", "DlyTime", true, "Abklingzeit dieses Skills in Millisekunden.", "Cooldown of this skill in milliseconds."},
    {"ActiveSkill", "DlyGroupNum", true, "Nummer der gemeinsamen Abklingzeit-Gruppe: Skills mit gleicher Nummer sperren sich gegenseitig.", "Number of the shared cooldown group: skills with the same number block each other."},
    {"ActiveSkill", "DlyTimeGroup", true, "Abklingzeit, die die Gruppe nach Nutzung bekommt.", "Cooldown applied to the group after use."},
    {"ActiveSkill", "UseItem", true, "Item-ID, die beim Einsatz verbraucht wird; 0 = keins.", "Item ID consumed on use; 0 = none."},
    {"ActiveSkill", "ItemNumber", true, "Anzahl der verbrauchten Items.", "Number of consumed items."},
    {"ActiveSkill", "DemandItem1", true, "Item, das im Inventar sein muss (z.B. Waffe/Munition).", "Item that must be in the inventory (e.g. weapon/ammo)."},
    {"ActiveSkill", "DemandItem2", true, "Zweites benötigtes Item.", "Second required item."},
    {"ActiveSkill", "DemandSoul", false, "Benötigte Seelenpunkte (vermutet).", "Required soul points (assumed)."},
    {"ActiveSkill", "MinWC", true, "Minimaler physischer Schaden (WC).", "Minimum physical damage (WC)."},
    {"ActiveSkill", "MinWCRate", false, "Prozentualer Anteil an der Waffenkraft (vermutet).", "Percentage share of weapon power (assumed)."},
    {"ActiveSkill", "MaxWC", true, "Maximaler physischer Schaden (WC).", "Maximum physical damage (WC)."},
    {"ActiveSkill", "MaxWCRate", false, "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {"ActiveSkill", "MinMA", true, "Minimaler magischer Schaden (MA).", "Minimum magic damage (MA)."},
    {"ActiveSkill", "MinMARate", false, "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {"ActiveSkill", "MaxMA", true, "Maximaler magischer Schaden (MA).", "Maximum magic damage (MA)."},
    {"ActiveSkill", "MaxMARate", false, "Prozentualer Anteil (vermutet).", "Percentage share (assumed)."},
    {"ActiveSkill", "AC", false, "Verteidigungswert-Änderung durch den Skill (vermutet).", "Defense change by the skill (assumed)."},
    {"ActiveSkill", "MR", false, "Magieresistenz-Änderung durch den Skill (vermutet).", "Magic resistance change by the skill (assumed)."},
    {"ActiveSkill", "IsMovingSkill", false, "1 = Skill bewegt die Figur (Sprung/Ansturm o.ä.) (vermutet).", "1 = skill moves the character (jump/charge) (assumed)."},
    {"ActiveSkill", "UsableDegree", true, "Öffnungswinkel vor dem Charakter, in dem das Ziel liegen muss (Grad).", "Opening angle in front of the character in which the target must be (degrees)."},
    {"ActiveSkill", "SkillDegree", false, "Winkel des Trefferbereichs (360 = rundum) (vermutet).", "Angle of the hit area (360 = all around) (assumed)."},
    {"ActiveSkill", "DirectionRotate", false, "Drehung der Blickrichtung beim Einsatz (vermutet).", "Rotation of facing on use (assumed)."},
    {"ActiveSkill", "First", false, "Aufzählung: Art des ersten Ziels (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of the first target (copy from a similar skill)."},
    {"ActiveSkill", "Last", false, "Aufzählung: Art des letzten Ziels.", "Enum: kind of the last target."},
    {"ActiveSkill", "SkillTargetState", false, "Aufzählung: Zustand, den das Ziel haben muss (vermutet).", "Enum: state the target must have (assumed)."},
    {"ActiveSkill", "CannotInside", false, "1 = in Innenräumen nicht nutzbar (vermutet).", "1 = not usable indoors (assumed)."},
    {"ActiveSkill", "EffectType", false, "Aufzählung: Art der Zusatzwirkung (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of additional effect (copy from a similar skill)."},
    {"ActiveSkill", "HitID", false, "Verweis auf die Trefferdefinition (vermutet).", "Reference to the hit definition (assumed)."},
    {"ActiveSkillInfoServer", "UsualAttack", false, "1 = wird wie ein normaler Angriff behandelt (vermutet).", "1 = treated like a normal attack (assumed)."},
    {"ActiveSkillInfoServer", "SkilPyHitRate", false, "Grundtrefferchance für physische Skills (950 = 95 %) (vermutet).", "Base hit chance for physical skills (950 = 95 %) (assumed)."},
    {"ActiveSkillInfoServer", "SkilMaHitRate", true, "Grundtrefferchance für magische Skills.", "Base hit chance for magic skills."},
    {"ActiveSkillInfoServer", "PsySucRate", false, "Erfolgschance der Zusatzwirkung (physisch) (vermutet).", "Success chance of the additional effect (physical) (assumed)."},
    {"ActiveSkillInfoServer", "MagSucRate", false, "Erfolgschance der Zusatzwirkung (magisch) (vermutet).", "Success chance of the additional effect (magic) (assumed)."},
    {"ActiveSkillInfoServer", "StaLevel", false, "Stufe der ausgelösten Zustände (vermutet).", "Level of the applied states (assumed)."},
    {"ActiveSkillInfoServer", "DmgIncRate", false, "Prozentualer Schadenszuwachs (vermutet).", "Percentage damage increase (assumed)."},
    {"ActiveSkillInfoServer", "DmgIncValue", false, "Fester Schadenszuwachs (vermutet).", "Flat damage increase (assumed)."},
    {"ActiveSkillInfoServer", "SkillHitType", false, "Aufzählung: Art des Treffers (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of hit (copy from a similar skill)."},
    {"ActiveSkillInfoServer", "AggroPerDamage", false, "Wie viel Bedrohung pro Schadenspunkt entsteht (vermutet).", "How much threat per damage point (assumed)."},
    {"ActiveSkillInfoServer", "AbsoluteAggro", false, "Feste Bedrohung unabhängig vom Schaden (vermutet).", "Fixed threat independent of damage (assumed)."},
    {"ActiveSkillInfoServer", "AttackStart", false, "1 = der Einsatz beginnt den Kampf (vermutet).", "1 = use starts combat (assumed)."},
    {"ActiveSkillInfoServer", "AttackEnd", false, "1 = beendet den Kampf (vermutet).", "1 = ends combat (assumed)."},
    {"ActiveSkillInfoServer", "SwingTime", true, "Dauer der Angriffsbewegung in ms - sollte zur Animation passen.", "Duration of the attack motion in ms - should match the animation."},
    {"ActiveSkillInfoServer", "HitTime", false, "Zeitpunkt des Treffers innerhalb der Animation (vermutet).", "Moment of the hit within the animation (assumed)."},
    {"ActiveSkillInfoServer", "AddSoul", false, "Seelenpunkte, die der Skill gibt (vermutet).", "Soul points granted by the skill (assumed)."},
    {"ActiveSkillView", "IconFile", true, "Name der Icon-Bilddatei (z.B. FighterSk00) - Werte aus vorhandenen Skills.", "Name of the icon image file (e.g. FighterSk00) - values from existing skills."},
    {"ActiveSkillView", "IconIndex", true, "Position des Icons in der Bilddatei.", "Position of the icon within the image file."},
    {"ActiveSkillView", "R", true, "Rotanteil der Skillfarbe (0-255).", "Red part of the skill colour (0-255)."},
    {"ActiveSkillView", "G", true, "Grünanteil der Skillfarbe.", "Green part of the skill colour."},
    {"ActiveSkillView", "B", true, "Blauanteil der Skillfarbe.", "Blue part of the skill colour."},
    {"ActiveSkillView", "CastingType", false, "Aufzählung: Art der Zauberdarstellung (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of cast presentation (copy from a similar skill)."},
    {"ActiveSkillView", "ActionType", false, "Aufzählung: Art der Aktion (Wert aus ähnlichem Skill übernehmen).", "Enum: kind of action (copy from a similar skill)."},
    {"ActiveSkillView", "CasRdyAction", true, "Animation beim Beginn des Zauberns (Zaubern-Skills: '..Rd'); '-' = keine.", "Animation when casting begins (cast skills: '..Rd'); '-' = none."},
    {"ActiveSkillView", "CasAction", true, "Animation während des Zauberns ('..Cas'); '-' = keine.", "Animation while casting ('..Cas'); '-' = none."},
    {"ActiveSkillView", "SwingAction", true, "Animation der eigentlichen Ausführung (Schlag/Wurf/Zauberabschluss).", "Animation of the actual execution (strike/throw/cast finish)."},
    {"ActiveSkillView", "LastAction", true, "Abschlussanimation (in den Daten nie benutzt).", "Finishing animation (never used in the data)."},
    {"ActiveSkillView", "ShoEffect", true, "Effekt, der vom Wirker zum Ziel fliegt (z.B. MagicBallSho).", "Effect flying from caster to target (e.g. MagicBallSho)."},
    {"ActiveSkillView", "ShoEfSpd", true, "Fluggeschwindigkeit des Geschosses.", "Flight speed of the projectile."},
    {"ActiveSkillView", "LastEffectA", true, "Effekt am Ziel bei Treffer (z.B. IceBlastDem).", "Effect at the target on impact (e.g. IceBlastDem)."},
    {"ActiveSkillView", "eLastEffPos", false, "Aufzählung: wo der Treffereffekt erscheint (Wert aus ähnlichem Skill übernehmen).", "Enum: where the impact effect appears (copy from a similar skill)."},
    {"ActiveSkillView", "LastAreaEf", true, "Flächeneffekt am Zielpunkt (Flächenskills).", "Area effect at the target point (area skills)."},
    {"ActiveSkillView", "LastAEfWhe", false, "Aufzählung/Angabe, wo der Flächeneffekt erscheint (vermutet).", "Enum/value where the area effect appears (assumed)."},
    {"ActiveSkillView", "DOTRageEft", true, "Effekt für Dauerschaden/-zustände (Anfang).", "Effect for damage over time/states (start)."},
    {"ActiveSkillView", "DOTRageEftLoop", true, "Schleifen-Effekt während des Dauerzustands.", "Looping effect during the ongoing state."},
    {"ActiveSkillView", "ShoSnd", true, "Sound beim Geschoss.", "Sound for the projectile."},
    {"ActiveSkillView", "LastEfASnd", true, "Sound beim Trefferefekt.", "Sound for the impact effect."},
    {"ActiveSkillView", "LastAESnd", true, "Sound beim Flächeneffekt.", "Sound for the area effect."},
    {"ActiveSkillView", "DOTRageEftSnd", true, "Sound beim Dauerschaden-Effekt.", "Sound for the DOT effect."},
    {"ActiveSkillView", "DOTRageEftLoopSnd", true, "Sound der Schleife.", "Sound of the loop."},
    {"ActiveSkillView", "Descript", true, "Beschreibungstext im Skillfenster.", "Description text in the skill window."},
    {"ActiveSkillView", "Function", true, "Kurztext zur Wirkung (z.B. 'additional Damage.').", "Short text about the effect (e.g. 'additional Damage.')."},
    {"ActiveSkillView", "uiDemandLv", true, "Charakterlevel, ab dem der Skill lernbar ist (Anzeige).", "Character level from which the skill can be learned (display)."},
    {"ActiveSkillView", "HideHandItem", true, "1 = Waffe in der Hand während der Animation ausblenden.", "1 = hide the hand weapon during the animation."},
    {"ActiveSkillView", "CancelCasting", false, "Verhalten beim Abbrechen des Zauberns (vermutet).", "Behaviour when casting is cancelled (assumed)."},
    {"MapViewInfo", "Loading", true, "Ladebild der Karte: Name ohne Endung unter resmenu/loading (.png, sonst .tga); mehrere Karten dürfen dasselbe Bild nutzen.", "Loading image of the map: name without extension below resmenu/loading (.png, otherwise .tga); several maps may share an image."},
    {"MapViewInfo", "LoadLocation", false, "0 oder 1 (29 von 138 Karten haben 1); Bedeutung nicht belegt.", "0 or 1 (29 of 138 maps use 1); meaning not proven."},
    {"ActiveSkillView", "TargetChange", false, "1 = Ziel darf während des Einsatzes gewechselt werden (vermutet).", "1 = target may change during use (assumed)."},
};

} // namespace

const Chapter* ChapterData(std::size_t& n) { n = sizeof(kChapters) / sizeof(kChapters[0]); return kChapters; }
const Section* SectionData(std::size_t& n) { n = sizeof(kSections) / sizeof(kSections[0]); return kSections; }
const ColumnDoc* ColumnData(std::size_t& n) { n = sizeof(kColumns) / sizeof(kColumns[0]); return kColumns; }
const char* const* TipData(std::size_t& n) { (void)n; return nullptr; }
void ForEachTip(void (*fn)(const char*, const char*, const char*, void*), void* user) { for (const auto& t : kTips) fn(t.key, t.de, t.en, user); }

} // namespace theseed::mapeditor::core::manual
