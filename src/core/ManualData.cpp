// GENERIERT (Handbuch-Daten, zweisprachig) - siehe docs/MANUAL_MAINTENANCE.md zum Erweitern.
#include "mapeditor/core/Manual.hpp"

namespace theseed::mapeditor::core::manual {
namespace {

const Chapter kChapters[] = {
    {"start", "Erste Schritte", "Getting started"},
    {"controls", "Steuerung und Tastenkürzel", "Controls and shortcuts"},
    {"map", "Map-Editor", "Map editor"},
    {"shn", "SHN-Editor (Spieldaten)", "SHN editor (game data)"},
    {"creators", "Erstellen: NPCs, Mobs, Skills, Shops", "Creating: NPCs, mobs, skills, shops"},
    {"reference", "Nachschlagen", "Reference"},
};

const Section kSections[] = {
    {"start.overview", "start",
     "Überblick", "Overview",
     R"MAN(Der Editor besteht aus zwei großen Bereichen:
- Map-Editor: Höhenkarte, Texturen, Begehbarkeit (Block & Walk), Objekte, NPCs, Mobs (Monster-Spawns) und Portale einer Karte. Alle Bereiche zeigen dieselbe Karte in einer 2D-Draufsicht (zum Bearbeiten) und einer 3D-Ansicht (zum Prüfen und Ausrichten).
- SHN-Editor: die Spieldaten-Tabellen (Items, Mobs, Skills, Quests, Shops ...) aus Client (ressystem) und Server (9Data/Shine).
Es wird NICHTS automatisch gespeichert. Jede Änderung liegt zunächst nur im Speicher; die jeweiligen Speichern-Knöpfe schreiben die Dateien. Angepasste SHN-Dateien sind in den Listen mit * markiert.
Lege vor dem ersten Speichern eine Sicherung Deiner Client- und Server-Ordner an.)MAN",
     R"MAN(The editor has two main areas:
- Map editor: heightmap, textures, walkability (Block & Walk), objects, NPCs, mobs (monster spawns) and portals of a map. All areas show the same map as a 2D top view (for editing) and a 3D view (for checking and aligning).
- SHN editor: the game data tables (items, mobs, skills, quests, shops ...) from client (ressystem) and server (9Data/Shine).
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
     R"MAN(Die Kamera arbeitet wie in einem Level-Editor:
- Rechte Maustaste halten + Maus bewegen: umsehen (die Kamera bleibt stehen, Du drehst Dich).
- W / A / S / D (oder Pfeiltasten): vorwärts, links, rückwärts, rechts laufen.
- Q / E (oder Leertaste): runter / hoch.
- Shift: 4-fach schneller. Strg: langsam (0,2-fach). Das Grundtempo wächst mit der Entfernung.
- Mausrad: Zoom, bis dicht an den Punkt heran. Die Kamera schneidet dabei nichts Nahes ab.
- Mittlere Maustaste ziehen: Ansicht schieben.
- Linke Maustaste ziehen: um das Ziel kreisen.
- Knöpfe + / - unten rechts: Zoom. 'Kamera zentrieren' (Werkzeugleiste): ganze Karte.
Die Tasten wirken nur, solange die Maus über dem 3D-Bild ist und kein Textfeld aktiv ist.)MAN",
     R"MAN(The camera works like in a level editor:
- Hold the right mouse button + move the mouse: look around (the camera stays in place, you turn).
- W / A / S / D (or arrow keys): walk forward, left, backward, right.
- Q / E (or space): down / up.
- Shift: 4 times faster. Ctrl: slow (0.2 times). The base speed grows with the distance.
- Mouse wheel: zoom, up to very close to the point. Nothing nearby is clipped.
- Drag with the middle mouse button: pan the view.
- Drag with the left mouse button: orbit around the target.
- Buttons + / - at the bottom right: zoom. 'Center camera' (toolbar): whole map.
The keys only work while the mouse is over the 3D image and no text field is active.)MAN",
     "3d kamera wasd rechte maustaste umsehen fly"},
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
- Mausrad: Zoom; Mitte ziehen: schieben; Links ziehen: kreisen
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
- Mouse wheel: zoom; drag middle: pan; drag left: orbit
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
- 'Erweitert (natives Format / Legacy Import-Export)': einzelne Dateien getrennt importieren/exportieren, außerdem das eigene Format (.tshm Höhe, .tstex Texturen, .tswalk Begehbarkeit, .tsobj Objekte).
# Große Karten
Bei großen Karten (z.B. Adelia 951x476 Blöcke) deckt jede Blend-Textur laut .ini nur eine Region ab. Der Editor zeichnet das korrekt (bis zu 24 Layer). Das Block&Walk-Gitter ist dabei immer quadratisch (Zelle = 6,25 Einheiten) und deckt die längere Kartenseite ab.)MAN",
     R"MAN(- Open a map: in the map editor start enter the path of the map .ini (e.g. resmap/field/Rou/Rou.ini) and 'Open map'. The editor reads heightmap (.HTD), textures (.ini + BMPs), Block&Walk (.shbd) and objects (.shmd/.idm/.aid) automatically.
- New map: 'New map' creates a map (e.g. 257x257 blocks). It starts flat and fully blocked.
- Save map: enter output directory and map name; 'Save map' writes all legacy files.
- 'Advanced (native format / legacy import-export)': import/export single files separately, plus the own format (.tshm height, .tstex textures, .tswalk walkability, .tsobj objects).
# Large maps
On large maps (e.g. Adelia 951x476 blocks) each blend texture covers only one region according to the .ini. The editor draws this correctly (up to 24 layers). The Block&Walk grid is always square (cell = 6.25 units) and covers the longer side of the map.)MAN",
     "karte map öffnen speichern neu ini htd shbd shmd"},
    {"map.heightmap", "map",
     "Tab Hightmap (Höhenkarte)", "Heightmap tab",
     R"MAN(Höhenkarte bearbeiten. Halte die linke Maustaste in der 2D-Ansicht und fahre über die Karte.
- Anheben / Absenken: Gelände hoch- oder herunterziehen.
- Glätten: gleicht Höhen an die Umgebung an.
- Einebnen: setzt das Gelände auf die 'Zielhöhe'.
- Radius: Pinselgröße in Welteinheiten. Stärke: wie stark pro Zug.
- Rückgängig / Wiederholen: nehmen ganze Pinselzüge zurück.
Unten stehen Gittergröße und Höhenbereich. Objekte und NPCs folgen der Höhe beim Neuberechnen ihrer Position; gespeicherte Objekthöhen ändern sich dadurch nicht automatisch.)MAN",
     R"MAN(Edit the heightmap. Hold the left mouse button in the 2D view and move over the map.
- Raise / Lower: pull the terrain up or down.
- Smooth: blends heights with the surroundings.
- Flatten: sets the terrain to the 'target height'.
- Radius: brush size in world units. Strength: how strong per stroke.
- Undo / Redo: revert whole brush strokes.
Below you find grid size and height range. Objects and NPCs follow the height when their position is recalculated; saved object heights do not change automatically.)MAN",
     "höhenkarte heightmap gelände anheben absenken glätten einebnen"},
    {"map.texturing", "map",
     "Tab Map Texturen", "Map textures tab",
     R"MAN(Jede Karte hat mehrere Texturschichten (Layer). Jeder Layer hat eine Diffuse-Textur (Bild), einen UV-Scale (Kachelgröße) und ein Gewicht je Kartenpunkt (Blend), das Du malst.
- Layer wählen (Liste), dann in der 2D-Ansicht malen: 'Erhöhen' verstärkt den Layer, 'Senken' schwächt ihn. Die Gewichte aller Layer derselben Region werden normalisiert.
- Radius / Stärke: Pinsel.
- Layer hinzufügen: Name, Diffuse-Datei ('Durchsuchen...') und UV-Scale angeben. 'Layer entfernen' löscht den gewählten Layer.
- Rückgängig / Wiederholen für Textur-Pinselzüge.
Einzelne Layer lassen sich im Bereich 'Sichtbarkeit' ein- und ausblenden, ohne die Daten zu ändern.)MAN",
     R"MAN(Every map has several texture layers. Each layer has a diffuse texture (image), a UV scale (tile size) and a weight per map point (blend) that you paint.
- Select a layer (list), then paint in the 2D view: 'Increase' strengthens the layer, 'Decrease' weakens it. The weights of all layers of the same region are normalized.
- Radius / Strength: brush.
- Add layer: enter name, diffuse file ('Browse...') and UV scale. 'Remove layer' deletes the selected layer.
- Undo / Redo for texture strokes.
Individual layers can be shown/hidden in the 'Visibility' area without changing the data.)MAN",
     "textur layer blend diffuse uv scale malen"},
    {"map.walk", "map",
     "Tab Walk & Block (Begehbarkeit)", "Walk & Block tab",
     R"MAN(Das Block&Walk-Gitter besteht aus Zellen von 6,25 Welteinheiten. Eine Zelle ist entweder blockiert (im Overlay rot) oder begehbar. Server-Punkte (NPCs, Wegpunkte) liegen immer auf begehbaren Zellen.
- Sperren / Freigeben: welche Art der Änderung der Pinsel macht.
- Radius: Pinselgröße; '1 Zelle' trifft nur die Zelle unter dem Zeiger.
- Aus Objekten: 'Grundflächen sichtbarer Objekte SPERREN/FREIGEBEN' stempelt die Grundfläche aller gerade sichtbaren Objekte (siehe Sichtbarkeit: z.B. nur 'Gebäude' einblenden) ins Gitter. Ein Klick lässt sich mit Rückgängig zurücknehmen.
- Rückgängig / Wiederholen (Walk).
Objekte erscheinen als Referenz mit ihrer Grundfläche; sie sind in diesem Werkzeug nicht anklickbar, damit man nichts versehentlich verschiebt.)MAN",
     R"MAN(The Block&Walk grid consists of cells of 6.25 world units. A cell is either blocked (red in the overlay) or walkable. Server points (NPCs, waypoints) always lie on walkable cells.
- Block / Release: which kind of change the brush makes.
- Radius: brush size; '1 cell' only hits the cell under the pointer.
- From objects: 'Block/Release footprints of visible objects' stamps the footprint of all currently visible objects (see Visibility: e.g. show only 'Buildings') into the grid. One click can be reverted with Undo.
- Undo / Redo (Walk).
Objects appear as a reference with their footprint; they cannot be clicked in this tool so nothing is moved by accident.)MAN",
     "walk block begehbar gitter zelle sperren freigeben grundfläche"},
    {"map.objects", "map",
     "Tab Objekt Platzierung", "Object placement tab",
     R"MAN(Objekte sind 3D-Modelle (Häuser, Bäume, Steine ...) mit Position, Drehung und Skalierung.
- Platzieren: Modellpfad eintragen oder 'Durchsuchen...', Rotation und Skalierung einstellen, dann in die 2D-Ansicht klicken.
- Auswählen: in der 2D-Ansicht auf ein Objekt klicken (nächstes innerhalb der Toleranz). Rotation, Skalierung und 'Objekt löschen' wirken auf das gewählte Objekt.
- Die Höhe wird beim Platzieren aus dem Gelände genommen.
- Im 3D-Bild siehst Du die echten Modelle. Objekte, deren Modell nicht geladen werden kann, erscheinen als Platzhalter-Pyramide.
Objekte lassen sich nach Kategorien (Bäume, Gebäude, Felsen ...) ein- und ausblenden (Bereich Sichtbarkeit) - hilfreich bei dichten Karten.)MAN",
     R"MAN(Objects are 3D models (houses, trees, stones ...) with position, rotation and scale.
- Place: enter a model path or 'Browse...', set rotation and scale, then click into the 2D view.
- Select: click an object in the 2D view (nearest within the tolerance). Rotation, scale and 'Delete object' act on the selected object.
- The height is taken from the terrain when placing.
- In the 3D image you see the real models. Objects whose model cannot be loaded appear as a placeholder pyramid.
Objects can be shown/hidden by category (trees, buildings, rocks ...) in the Visibility area - helpful on dense maps.)MAN",
     "objekte platzieren modell nif rotation skalierung löschen"},
    {"map.visibility", "map",
     "Bereich Sichtbarkeit", "Visibility area",
     R"MAN(Im Werkzeug-Panel steht der Bereich 'Sichtbarkeit'. Er ändert nur die Anzeige, nie die Daten.
- Terrain, Objekt-Modelle, Objekt-Platzhalter, NPC-Modelle, Objekte im 2D-View: je ein Schalter.
- Terrain-Layer: jede Texturschicht einzeln ausblenden ('Alle Layer an' setzt alles zurück).
- Objekt-Kategorien: Bäume & Büsche, Gras & Blumen, Felsen & Steine, Gebäude, Zäune/Mauern/Brücken, Dekoration & Möbel, Wasser & Schiffe, Tiere & Kreaturen, Effekte & Licht, Sonstiges. Mit der Anzahl je Kategorie; 'nur' zeigt allein diese Kategorie, 'Alle an'/'Alle aus' schalten alle.
- Die Kategorie wird aus dem Modellnamen abgeleitet (Schlüsselwörter) und kann daher selten danebenliegen.
- 'NPC-Namen und Blickpfeile': Namen und Blickrichtung der NPCs im 3D-Bild (NPC-Modus).
- Fehlt NPCs das Modell, werden sie hier namentlich aufgelistet.)MAN",
     R"MAN(The tool panel contains the 'Visibility' area. It only changes the display, never the data.
- Terrain, object models, object placeholders, NPC models, objects in the 2D view: one switch each.
- Terrain layers: hide each texture layer individually ('All layers on' resets).
- Object categories: trees & bushes, grass & flowers, rocks & stones, buildings, fences/walls/bridges, decoration & furniture, water & ships, animals & creatures, effects & light, other. With the count per category; 'only' shows just this category, 'All on'/'All off' switch all.
- The category is derived from the model name (keywords) and can therefore rarely be off.
- 'NPC names and facing arrows': names and facing direction of NPCs in the 3D image (NPC mode).
- If NPCs lack a model they are listed here by name.)MAN",
     "sichtbarkeit visibility ausblenden einblenden kategorie layer"},
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
     "Tabs NPC AI und Mob AI", "NPC AI and Mob AI tabs",
     R"MAN(Diese beiden Tabs haben noch keine eigenen Werkzeuge. Das Verhalten von NPCs und Mobs bearbeitest Du an ihrem jeweiligen Tab: 'KI-Skript (Lua) bearbeiten' und 'Patrouillenroute bearbeiten' im NPC-Tab, 'KI' und 'Route' je Monster im Mob-Tab.)MAN",
     R"MAN(These two tabs do not have their own tools yet. You edit the behaviour of NPCs and mobs in their respective tab: 'Edit AI script (Lua)' and 'Edit patrol route' in the NPC tab, 'AI' and 'Route' per monster in the mob tab.)MAN",
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
- 'TownPortal.shn speichern' und 'RecallCoord.txt speichern'.
Ein TownPortal-Ziel liegt höchstens 205 Einheiten vom zugehörigen Gate_Town-NPC entfernt.)MAN",
     R"MAN(Fast-travel targets: TownPortal.shn (town portal targets) and RecallCoord.txt (scroll targets).
- Markers: diamond = TownPortal, triangle = scroll, square = Gate_Town NPC, ring = regeneration point of the map.
- X / Y: target position. 'Set position by click in 2D view' picks the position with the mouse. 'Menu group' and 'Minimum level' control the menu.
- 'Add TownPortal target here', 'Remove', 'Discard and reload'.
- 'Save TownPortal.shn' and 'Save RecallCoord.txt'.
A TownPortal target is at most 205 units away from the associated Gate_Town NPC.)MAN",
     "portal townportal recall schriftrolle teleport"},
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
Die Prozent-Editoren ändern ALLE Zeilen der Spalte - vorher prüfen, dann speichern.)MAN",
     R"MAN(- Single SHN Editor: edit any table as a grid.
- Multi SHN Editor: choose a task (new item, new NPC, new mob, new skill, shop/price, new quest, XP/rate); the editor marks the relevant tables in CLIENT and SERVER yellow as candidates. This is an orientation aid, not a proven dependency.
- XP Rate Editor: change MonEXP and EXPRange of all mobs (MobInfoServer) by a percentage ('% change').
- Buy & Sell Editor: change BuyPrice and SellPrice of all items (ItemInfo) by a percentage.
- Quest Editor: quests (QuestData.shn) with texts from QuestDialog.
- Portals: TownPortal targets (also in the map editor).
- Custom NPC/Mob: create NPCs and monsters from templates.
- Skill Editor: modify skills and create new ones.
The percentage editors change ALL rows of the column - check first, then save.)MAN",
     "tabs multi xp rate buy sell quest single"},
    {"shn.quest", "shn",
     "Quest-Editor", "Quest editor",
     R"MAN(Links die Liste (Titel oder Beschreibung, Suche nach ID oder Text), rechts die Details:
- Quest-ID, Titel-Text-ID und Beschreibung-Text-ID (Texte aus QuestDialog.shn werden daneben aufgelöst), Mindest-/Maximal-Level, Start-NPC (Mob-ID), Aktiviert, Tägliche Quest, benötigtes Item, Vorgänger-Quest.
- Monster-Ziele (5 Plätze), Item-Ziele (10 Plätze): aktiv, ID, Anzahl; der Name wird aufgelöst (rot = nicht gefunden).
- Drops: Mob, Item, Menge, Rate.
- Skripte Start / Action / Finish: die Quest-Skriptsprache (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'SAY-Text-ID nachschlagen' zeigt den Text zu einer ID.
- Belohnungen (144 Byte) sind noch nicht entschlüsselt und werden unverändert gespeichert.
- 'QuestData.shn speichern'.)MAN",
     R"MAN(On the left the list (title or description, search by ID or text), on the right the details:
- Quest ID, title text ID and description text ID (texts from QuestDialog.shn are resolved next to them), minimum/maximum level, start NPC (mob ID), enabled, daily quest, required item, predecessor quest.
- Monster targets (5 slots), item targets (10 slots): active, ID, count; the name is resolved (red = not found).
- Drops: mob, item, amount, rate.
- Scripts Start / Action / Finish: the quest script language (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'Look up SAY text ID' shows the text for an ID.
- Rewards (144 bytes) are not decoded yet and are saved unchanged.
- 'Save QuestData.shn'.)MAN",
     "quest questdata skript say if goto"},
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
    {"Asset-Ordner wählen...", "Ordner mit den Modellen und Texturen (Client) wählen, aus dem NIF-Modelle und Texturen geladen werden.", "Choose the folder with the models and textures (client) from which NIF models and textures are loaded."},
    {"Karte-.ini", "Pfad der Karten-.ini (z.B. resmap/field/Rou/Rou.ini). Sie beschreibt Größe, Texturschichten und Dateien der Karte.", "Path of the map .ini (e.g. resmap/field/Rou/Rou.ini). It describes size, texture layers and files of the map."},
    {"Karte öffnen", "Liest die Karte (Höhe, Texturen, Begehbarkeit, Objekte) aus der angegebenen .ini.", "Reads the map (height, textures, walkability, objects) from the given .ini."},
    {"Ausgabeverzeichnis", "Ordner, in den 'Karte speichern' die Kartendateien schreibt.", "Folder into which 'Save map' writes the map files."},
    {"Wählen...", "Ordner über einen Dialog auswählen.", "Choose a folder via a dialog."},
    {"Kartenname", "Name der Karte = Dateiname der geschriebenen Dateien (ohne Endung).", "Name of the map = file name of the written files (without extension)."},
    {"Karte speichern", "Schreibt die Karte im Originalformat (Höhe, Texturen, Block&Walk, Objekte) in das Ausgabeverzeichnis.", "Writes the map in its original format (height, textures, Block&Walk, objects) to the output directory."},
    {"Neu (257x257)", "Legt eine neue, flache Karte mit 257x257 Höhenpunkten an.", "Creates a new flat map with 257x257 height points."},
    {"Laden (.tshm)", "Lädt die Höhenkarte im eigenen Format .tshm.", "Loads the heightmap in the own .tshm format."},
    {"Speichern (.tshm)", "Speichert die Höhenkarte im eigenen Format .tshm.", "Saves the heightmap in the own .tshm format."},
    {"Pfad##legacy", "Pfad der Original-Höhendatei (.HTD).", "Path of the original height file (.HTD)."},
    {"Breite (aus .ini)", "Anzahl der Höhenpunkte in X-Richtung (steht in der Karten-.ini).", "Number of height points in X direction (found in the map .ini)."},
    {"Höhe (aus .ini)", "Anzahl der Höhenpunkte in Z-Richtung (steht in der Karten-.ini).", "Number of height points in Z direction (found in the map .ini)."},
    {"Blockbreite", "Größe eines Höhenblocks in X (Welteinheiten, meist 50).", "Size of a height block in X (world units, usually 50)."},
    {"Blockhöhe", "Größe eines Höhenblocks in Z (Welteinheiten, meist 50).", "Size of a height block in Z (world units, usually 50)."},
    {"Importieren##htd", "Liest eine Höhendatei (.HTD) mit den obigen Maßen.", "Reads a height file (.HTD) with the dimensions above."},
    {"Exportieren##htd", "Schreibt die Höhenkarte als Original-.HTD.", "Writes the heightmap as an original .HTD."},
    {"Laden (.tstex)", "Lädt die Texturschichten im eigenen Format .tstex.", "Loads the texture layers in the own .tstex format."},
    {"Speichern (.tstex)", "Speichert die Texturschichten im eigenen Format .tstex.", "Saves the texture layers in the own .tstex format."},
    {"ini-Pfad##legacyTex", "Karten-.ini, aus der die Texturschichten (mit Regionen) importiert werden.", "Map .ini from which the texture layers (with regions) are imported."},
    {"Legacy-Set importieren", "Importiert alle Texturschichten samt Blend-BMPs aus der .ini.", "Imports all texture layers including blend BMPs from the .ini."},
    {"Export-Verzeichnis##legacyTex", "Ordner, in den die Textur-Dateien (.ini und BMPs) exportiert werden.", "Folder to which the texture files (.ini and BMPs) are exported."},
    {"Legacy-Set exportieren", "Schreibt Texturschichten als .ini und Blend-BMPs im Originalformat.", "Writes texture layers as .ini and blend BMPs in the original format."},
    {"Laden (.tswalk)", "Lädt die Begehbarkeit im eigenen Format .tswalk.", "Loads the walkability in the own .tswalk format."},
    {"Speichern (.tswalk)", "Speichert die Begehbarkeit im eigenen Format .tswalk.", "Saves the walkability in the own .tswalk format."},
    {"Pfad##walkLegacy", "Pfad der Original-Begehbarkeitsdatei (.shbd).", "Path of the original walkability file (.shbd)."},
    {"Breite##walkLegacy", "Breite des Gitters in 16-Bit-Wörtern (je 16 Zellen).", "Grid width in 16-bit words (16 cells each)."},
    {"Höhe##walkLegacy", "Höhe des Gitters in Zeilen (= Zellen).", "Grid height in rows (= cells)."},
    {"Importieren##shbd", "Liest eine .shbd-Datei mit den obigen Maßen.", "Reads a .shbd file with the dimensions above."},
    {"Exportieren##shbd", "Schreibt die Begehbarkeit als Original-.shbd.", "Writes the walkability as an original .shbd."},
    {"Laden (.tsobj)", "Lädt die Objekte im eigenen Format .tsobj.", "Loads the objects in the own .tsobj format."},
    {"Speichern (.tsobj)", "Speichert die Objekte im eigenen Format .tsobj.", "Saves the objects in the own .tsobj format."},
    {"Pfad##shmd", "Pfad der Objektdatei (.shmd).", "Path of the object file (.shmd)."},
    {"Importieren##shmd", "Liest die Objektliste aus einer .shmd.", "Reads the object list from a .shmd."},
    {"Exportieren##shmd", "Schreibt die Objektliste als .shmd.", "Writes the object list as a .shmd."},
    {"Pfad##idm", "Pfad der Objekt-Indexdatei (.idm).", "Path of the object index file (.idm)."},
    {"Importieren##idm", "Liest die Objektindexdatei (.idm).", "Reads the object index file (.idm)."},
    {"Exportieren##idm", "Schreibt die Objektindexdatei (.idm).", "Writes the object index file (.idm)."},
    {"Pfad##aid", "Pfad der Objekt-Attributdatei (.aid).", "Path of the object attribute file (.aid)."},
    {"Importieren##aid", "Liest die Objekt-Attributdatei (.aid).", "Reads the object attribute file (.aid)."},
    {"Exportieren##aid", "Schreibt die Objekt-Attributdatei (.aid).", "Writes the object attribute file (.aid)."},
    {"Erweitert (natives Format / Legacy Import-Export)", "Einzelne Dateien getrennt importieren/exportieren und das eigene Format nutzen.", "Import/export single files separately and use the own format."},
    {"Neu durchsuchen", "Liest die Modell-/Textur-Bibliothek des Asset-Ordners erneut ein.", "Rescans the model/texture library of the asset folder."},
    {"Übernehmen", "Wendet den eingegebenen Wert an. Ungültige Werte werden abgelehnt.", "Applies the entered value. Invalid values are rejected."},
    {"Abbrechen", "Verwirft die Eingabe.", "Discards the input."},
    {"Schließen", "Schließt dieses Fenster.", "Closes this window."},
    {"Entfernen", "Entfernt den gewählten Eintrag.", "Removes the selected entry."},
    {"← Zurück", "Zurück zur vorherigen Ebene.", "Back to the previous level."},
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
    {"TownPortal-Ziel hier hinzufügen", "Legt ein neues Stadtportal-Ziel an.", "Creates a new town portal target."},
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
    {"Auswählen", "Klick in die 2D-Ansicht wählt das nächste Objekt.", "Click in the 2D view selects the nearest object."},
    {"Modellpfad", "Pfad des Modells (NIF), das platziert wird.", "Path of the model (NIF) to place."},
    {"Durchsuchen...##nif", "Modell aus der Bibliothek wählen.", "Choose a model from the library."},
    {"Rotation um Hochachse (°)##new", "Drehung neuer Objekte um die senkrechte Achse in Grad.", "Rotation of new objects around the vertical axis in degrees."},
    {"Skalierung##new", "Größenfaktor neuer Objekte.", "Scale factor of new objects."},
    {"Rotation um Hochachse (°)", "Drehung des gewählten Objekts um die senkrechte Achse in Grad.", "Rotation of the selected object around the vertical axis in degrees."},
    {"Skalierung", "Größenfaktor des gewählten Objekts.", "Scale factor of the selected object."},
    {"Objekt löschen", "Löscht das gewählte Objekt.", "Deletes the selected object."},
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
    {"Objekt-Modelle (3D)", "Blendet die 3D-Modelle der Objekte ein/aus.", "Shows/hides the 3D models of objects."},
    {"Objekt-Platzhalter (3D)", "Blendet die Pyramiden für Objekte ohne ladbares Modell ein/aus.", "Shows/hides the pyramids for objects without a loadable model."},
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
    {"InputText:##tshmPath", "Pfad der eigenen Höhenkarten-Datei (.tshm).", "Path of the own heightmap file (.tshm)."},
    {"InputText:##tsobjPath", "Pfad der eigenen Objekt-Datei (.tsobj).", "Path of the own object file (.tsobj)."},
    {"InputText:##tstexPath", "Pfad der eigenen Textur-Datei (.tstex).", "Path of the own texture file (.tstex)."},
    {"InputText:##tswalkPath", "Pfad der eigenen Begehbarkeits-Datei (.tswalk).", "Path of the own walkability file (.tswalk)."},
    {"InputText:##n", "Name.", "Name."},
    {"InputText:##v", "Wert dieses Felds. Beschreibung: Mauszeiger über den Feldnamen links.", "Value of this field. Description: hover the field name on the left."},
    {"InputText:##newmob", "Name (MobIndex) des Monsters, das der Zone hinzugefügt wird.", "Name (MobIndex) of the monster added to the zone."},
    {"SliderFloat:##avyaw", "Dreht die Avatar-Vorschau.", "Rotates the avatar preview."},
    {"SliderInt:##scalepct", "Prozentwert der Skalierung (100 = unverändert).", "Percentage of the scaling (100 = unchanged)."},
    {"Versatz aus dieser Karte schaetzen (Block&Walk)", "Testet für alle NPCs dieser Karte, welcher Versatz sie im Schnitt am ehesten von der nächsten Wand weg blicken lässt (Annahme: Rücken zur Wand). Grobe Schätzung.", "For all NPCs on this map, tests which offset most often makes them face away from the nearest wall on average (assumption: back to the wall). A rough estimate."},
    {"Versatz bei Richtung 0 (Grad)", "Versatz der Zuordnung Richtung -> Blickwinkel bei Richtung 0 (gilt für alle NPCs). Per Block&Walk-Analyse auf 0 Grad voreingestellt, hier frei einstellbar.", "Offset of the direction -> facing-angle mapping at direction 0 (applies to all NPCs). Preset to 0 degrees from a Block&Walk analysis, freely adjustable here."},
    {"SmallButton:0##offreset", "Setzt den Versatz auf den Standardwert 0 Grad zurück.", "Resets the offset to the default of 0 degrees."},
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
    {"ItemInfo", "MarketIndex", false, "Markt-Kategorie; 'Skill' kennzeichnet Skillbücher.", "Market category; 'Skill' marks skill books."},
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
    {"ActiveSkillView", "TargetChange", false, "1 = Ziel darf während des Einsatzes gewechselt werden (vermutet).", "1 = target may change during use (assumed)."},
};

} // namespace

const Chapter* ChapterData(std::size_t& n) { n = sizeof(kChapters) / sizeof(kChapters[0]); return kChapters; }
const Section* SectionData(std::size_t& n) { n = sizeof(kSections) / sizeof(kSections[0]); return kSections; }
const ColumnDoc* ColumnData(std::size_t& n) { n = sizeof(kColumns) / sizeof(kColumns[0]); return kColumns; }
const char* const* TipData(std::size_t& n) { (void)n; return nullptr; }
void ForEachTip(void (*fn)(const char*, const char*, const char*, void*), void* user) { for (const auto& t : kTips) fn(t.key, t.de, t.en, user); }

} // namespace theseed::mapeditor::core::manual
