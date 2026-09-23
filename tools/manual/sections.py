# -*- coding: utf-8 -*-
# Handbuch-Kapitel und -Abschnitte (DE/EN). Format im Text: "# " = Zwischenüberschrift, "- " = Aufzählung, sonst Absatz.
CHAPTERS = [
 ("start",    "Erste Schritte", "Getting started"),
 ("controls", "Steuerung und Tastenkürzel", "Controls and shortcuts"),
 ("map",      "Map-Editor", "Map editor"),
 ("shn",      "SHN-Editor (Spieldaten)", "SHN editor (game data)"),
 ("creators", "Erstellen: NPCs, Mobs, Skills, Shops", "Creating: NPCs, mobs, skills, shops"),
 ("reference","Nachschlagen", "Reference"),
]
SECTIONS = []
def S(id, ch, tde, ten, bde, ben, kw=""):
    SECTIONS.append((id, ch, tde, ten, bde.strip("\n"), ben.strip("\n"), kw))

S("start.overview","start","Überblick","Overview","""
Der Editor besteht aus zwei großen Bereichen:
- Map-Editor: Höhenkarte, Texturen, Begehbarkeit (Block & Walk), Objekte, NPCs, Mobs (Monster-Spawns) und Portale einer Karte. Alle Bereiche zeigen dieselbe Karte in einer 2D-Draufsicht (zum Bearbeiten) und einer 3D-Ansicht (zum Prüfen und Ausrichten).
- SHN-Editor: die Spieldaten-Tabellen (Items, Mobs, Skills, Quests, Shops ...) aus Client (ressystem) und Server (9Data/Shine).
Es wird NICHTS automatisch gespeichert. Jede Änderung liegt zunächst nur im Speicher; die jeweiligen Speichern-Knöpfe schreiben die Dateien. Angepasste SHN-Dateien sind in den Listen mit * markiert.
Lege vor dem ersten Speichern eine Sicherung Deiner Client- und Server-Ordner an.
""","""
The editor has two main areas:
- Map editor: heightmap, textures, walkability (Block & Walk), objects, NPCs, mobs (monster spawns) and portals of a map. All areas show the same map as a 2D top view (for editing) and a 3D view (for checking and aligning).
- SHN editor: the game data tables (items, mobs, skills, quests, shops ...) from client (ressystem) and server (9Data/Shine).
NOTHING is saved automatically. Every change first exists only in memory; the respective Save buttons write the files. Modified SHN files are marked with * in the lists.
Make a backup of your client and server folders before the first save.
""","overview start project")

S("start.project","start","Projekt einrichten (Client- und Server-Ordner)","Setting up the project (client and server folders)","""
Im Projekt-Hub gibst Du den Client-Ordner (enthält ressystem, resmap, reschar, resitem ...) und den Server-Ordner (enthält 9Data/Shine) an. Alles Weitere wird automatisch abgeleitet:
- ressystem (Client-SHN) und Server/9Data/Shine (Server-SHN und Textdateien wie World/NPC.txt, MobRegen, NPCItemList).
- reschar (Charaktermodelle inklusive NPC-Modelle) und resitem (Waffen und Item-Modelle).
- resmap (Karten, Objekt-Modelle).
Findet der Editor einen Ordner nicht, steht das im jeweiligen Bereich (z.B. 'Ordner reschar nicht gefunden'). Prüfe dann die Pfade im Projekt-Hub. Die Ordner können beliebig tief liegen; gesucht wird bis 3 Ebenen tief.
""","""
In the project hub you enter the client folder (containing ressystem, resmap, reschar, resitem ...) and the server folder (containing 9Data/Shine). Everything else is derived automatically:
- ressystem (client SHN) and Server/9Data/Shine (server SHN and text files such as World/NPC.txt, MobRegen, NPCItemList).
- reschar (character models including NPC models) and resitem (weapons and item models).
- resmap (maps, object models).
If the editor cannot find a folder, the respective area says so (e.g. 'folder reschar not found'). Then check the paths in the project hub. Folders may be nested; the search goes up to 3 levels deep.
""","project folder ordner client server pfad")

S("start.language","start","Sprache, Tooltips und Handbuch","Language, tooltips and manual","""
- Oben rechts wählst Du die Sprache (Deutsch/Englisch). Sie gilt für die Oberfläche, die Tooltips und dieses Handbuch. Die englischen Texte sind nicht von einem Muttersprachler geprüft.
- Tooltips: Bewegst Du die Maus kurz über einen Knopf, ein Eingabefeld oder eine Auswahl, erscheint eine Erklärung.
- Handbuch: Taste F1 oder der Knopf '?' oben rechts. Links stehen die Kapitel, oben die Suche. Die Suche findet Wörter in Titeln, Texten und Stichworten.
""","""
- At the top right you choose the language (German/English). It applies to the interface, the tooltips and this manual. The English texts have not been checked by a native speaker.
- Tooltips: hold the mouse over a button, input field or selection for a moment and an explanation appears.
- Manual: key F1 or the '?' button at the top right. Chapters are on the left, search on top. Search finds words in titles, texts and keywords.
""","language sprache tooltip hilfe handbuch f1")

S("start.saving","start","Speichern, Rückgängig und Sicherheit","Saving, undo and safety","""
- Karten: 'Karte speichern' schreibt die Karte im Originalformat (Legacy-Dateien: Heightmap, Texturen, Block&Walk, Objekte) in das Ausgabeverzeichnis. Die Kartendateien sind so gebaut, dass unveränderte Teile byte-identisch bleiben.
- NPCs, Mobs, Portale, Shops: eigene Speichern-Knöpfe im jeweiligen Bereich schreiben World/NPC.txt, MobRegen, RecallCoord.txt, NPCItemList/<NPC>.txt, TownPortal.shn.
- SHN-Tabellen: 'Speichern' (Single-Editor) bzw. 'Alle geänderten SHN speichern' (Assistenten) schreiben jede geänderte Tabelle an ihren Pfad zurück.
- Rückgängig/Wiederholen: gibt es für Höhenkarte, Texturen und Block&Walk (Strg+Z / Strg+Y, siehe Tastenkürzel). Löschen von Zeilen/Zonen/Tabs ist nur nach Freigabe-Haken möglich.
- Vor dem Überschreiben werden keine Sicherungen angelegt.
""","""
- Maps: 'Save map' writes the map in its original format (legacy files: heightmap, textures, Block&Walk, objects) to the output directory. Unchanged parts of the map files stay byte-identical.
- NPCs, mobs, portals, shops: their own Save buttons in the respective area write World/NPC.txt, MobRegen, RecallCoord.txt, NPCItemList/<NPC>.txt, TownPortal.shn.
- SHN tables: 'Save' (single editor) or 'Save all changed SHN' (wizards) write every changed table back to its path.
- Undo/redo: available for heightmap, textures and Block&Walk (Ctrl+Z / Ctrl+Y, see shortcuts). Deleting rows/zones/tabs is only possible after ticking the release checkbox.
- No backups are created before overwriting.
""","save speichern undo rückgängig backup sicherung")

S("controls.view2d","controls","2D-Ansicht (Draufsicht)","2D view (top view)","""
Die 2D-Ansicht zeigt die Karte von oben, Norden ist oben.
- Mausrad: Zoom um den Mauszeiger (1x bis 40x). Der Kartenpunkt unter dem Zeiger bleibt stehen.
- Mittlere oder rechte Maustaste ziehen: Ausschnitt verschieben.
- Knöpfe oben rechts: + / - zoomen, 1:1 zeigt die ganze Karte. Unten links steht der Zoomfaktor.
- Linke Maustaste: je nach Werkzeug malen (Höhe, Textur, Block&Walk) oder platzieren/auswählen (Objekte, NPCs, Mobs, Portale).
- Marker: Objekte = Punkte mit Grundfläche, NPCs = Quadrate mit gelber Blicklinie, Mob-Zonen = Symbol mit Radius, Portale = Raute/Dreieck/Quadrat/Ring.
Die Auswahl-Toleranz schrumpft mit dem Zoom - für genaues Treffen einfach näher heranzoomen.
""","""
The 2D view shows the map from above, north is up.
- Mouse wheel: zoom around the mouse pointer (1x to 40x). The map point under the pointer stays in place.
- Drag with middle or right mouse button: pan the view.
- Buttons at the top right: + / - zoom, 1:1 shows the whole map. The zoom factor is at the bottom left.
- Left mouse button: paint (height, texture, Block&Walk) or place/select (objects, NPCs, mobs, portals), depending on the tool.
- Markers: objects = dots with footprint, NPCs = squares with a yellow facing line, mob zones = symbol with radius, portals = diamond/triangle/square/ring.
The selection tolerance shrinks with the zoom - to hit precisely just zoom in closer.
""","2d zoom pan draufsicht mausrad")

S("controls.view3d","controls","3D-Ansicht (Kamera)","3D view (camera)","""
Die Kamera arbeitet wie in einem Level-Editor:
- Rechte Maustaste halten + Maus bewegen: umsehen (die Kamera bleibt stehen, Du drehst Dich).
- W / A / S / D (oder Pfeiltasten): vorwärts, links, rückwärts, rechts laufen.
- Q / E (oder Leertaste): runter / hoch.
- Shift: 4-fach schneller. Strg: langsam (0,2-fach). Das Grundtempo wächst mit der Entfernung.
- Mausrad: Zoom, bis dicht an den Punkt heran. Die Kamera schneidet dabei nichts Nahes ab.
- Mittlere Maustaste ziehen: Ansicht schieben.
- Linke Maustaste ziehen: um das Ziel kreisen.
- Knöpfe + / - unten rechts: Zoom. 'Kamera zentrieren' (Werkzeugleiste): ganze Karte.
Die Tasten wirken nur, solange die Maus über dem 3D-Bild ist und kein Textfeld aktiv ist.
""","""
The camera works like in a level editor:
- Hold the right mouse button + move the mouse: look around (the camera stays in place, you turn).
- W / A / S / D (or arrow keys): walk forward, left, backward, right.
- Q / E (or space): down / up.
- Shift: 4 times faster. Ctrl: slow (0.2 times). The base speed grows with the distance.
- Mouse wheel: zoom, up to very close to the point. Nothing nearby is clipped.
- Drag with the middle mouse button: pan the view.
- Drag with the left mouse button: orbit around the target.
- Buttons + / - at the bottom right: zoom. 'Center camera' (toolbar): whole map.
The keys only work while the mouse is over the 3D image and no text field is active.
""","3d kamera wasd rechte maustaste umsehen fly")

S("controls.keys","controls","Tastenkürzel im Überblick","Shortcut overview","""
# Allgemein
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
- Enter im Zellfeld oder 'Übernehmen': Wert anwenden
""","""
# General
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
- Enter in the cell field or 'Apply': apply the value
""","tastenkürzel shortcuts tasten keys hotkeys strg ctrl")

S("map.open","map","Karte öffnen, neu anlegen, speichern","Open, create and save a map","""
- Karte öffnen: im Map-Editor-Start den Pfad der Karten-.ini angeben (z.B. resmap/field/Rou/Rou.ini) und 'Karte öffnen'. Der Editor liest Heightmap (.HTD), Texturen (.ini + BMPs), Block&Walk (.shbd) und Objekte (.shmd/.idm/.aid) automatisch.
- Neue Karte: 'Neue Karte' legt eine Karte an (z.B. 257x257 Blöcke). Sie startet flach und ganz gesperrt.
- Karte speichern: Ausgabeverzeichnis und Kartenname angeben; 'Karte speichern' schreibt alle Legacy-Dateien.
- 'Erweitert (natives Format / Legacy Import-Export)': einzelne Dateien getrennt importieren/exportieren, außerdem das eigene Format (.tshm Höhe, .tstex Texturen, .tswalk Begehbarkeit, .tsobj Objekte).
# Große Karten
Bei großen Karten (z.B. Adelia 951x476 Blöcke) deckt jede Blend-Textur laut .ini nur eine Region ab. Der Editor zeichnet das korrekt (bis zu 24 Layer). Das Block&Walk-Gitter ist dabei immer quadratisch (Zelle = 6,25 Einheiten) und deckt die längere Kartenseite ab.
""","""
- Open a map: in the map editor start enter the path of the map .ini (e.g. resmap/field/Rou/Rou.ini) and 'Open map'. The editor reads heightmap (.HTD), textures (.ini + BMPs), Block&Walk (.shbd) and objects (.shmd/.idm/.aid) automatically.
- New map: 'New map' creates a map (e.g. 257x257 blocks). It starts flat and fully blocked.
- Save map: enter output directory and map name; 'Save map' writes all legacy files.
- 'Advanced (native format / legacy import-export)': import/export single files separately, plus the own format (.tshm height, .tstex textures, .tswalk walkability, .tsobj objects).
# Large maps
On large maps (e.g. Adelia 951x476 blocks) each blend texture covers only one region according to the .ini. The editor draws this correctly (up to 24 layers). The Block&Walk grid is always square (cell = 6.25 units) and covers the longer side of the map.
""","karte map öffnen speichern neu ini htd shbd shmd")

S("map.heightmap","map","Tab Hightmap (Höhenkarte)","Heightmap tab","""
Höhenkarte bearbeiten. Halte die linke Maustaste in der 2D-Ansicht und fahre über die Karte.
- Anheben / Absenken: Gelände hoch- oder herunterziehen.
- Glätten: gleicht Höhen an die Umgebung an.
- Einebnen: setzt das Gelände auf die 'Zielhöhe'.
- Radius: Pinselgröße in Welteinheiten. Stärke: wie stark pro Zug.
- Rückgängig / Wiederholen: nehmen ganze Pinselzüge zurück.
Unten stehen Gittergröße und Höhenbereich. Objekte und NPCs folgen der Höhe beim Neuberechnen ihrer Position; gespeicherte Objekthöhen ändern sich dadurch nicht automatisch.
""","""
Edit the heightmap. Hold the left mouse button in the 2D view and move over the map.
- Raise / Lower: pull the terrain up or down.
- Smooth: blends heights with the surroundings.
- Flatten: sets the terrain to the 'target height'.
- Radius: brush size in world units. Strength: how strong per stroke.
- Undo / Redo: revert whole brush strokes.
Below you find grid size and height range. Objects and NPCs follow the height when their position is recalculated; saved object heights do not change automatically.
""","höhenkarte heightmap gelände anheben absenken glätten einebnen")

S("map.texturing","map","Tab Map Texturen","Map textures tab","""
Jede Karte hat mehrere Texturschichten (Layer). Jeder Layer hat eine Diffuse-Textur (Bild), einen UV-Scale (Kachelgröße) und ein Gewicht je Kartenpunkt (Blend), das Du malst.
- Layer wählen (Liste), dann in der 2D-Ansicht malen: 'Erhöhen' verstärkt den Layer, 'Senken' schwächt ihn. Die Gewichte aller Layer derselben Region werden normalisiert.
- Radius / Stärke: Pinsel.
- Layer hinzufügen: Name, Diffuse-Datei ('Durchsuchen...') und UV-Scale angeben. 'Layer entfernen' löscht den gewählten Layer.
- Rückgängig / Wiederholen für Textur-Pinselzüge.
Einzelne Layer lassen sich im Bereich 'Sichtbarkeit' ein- und ausblenden, ohne die Daten zu ändern.
""","""
Every map has several texture layers. Each layer has a diffuse texture (image), a UV scale (tile size) and a weight per map point (blend) that you paint.
- Select a layer (list), then paint in the 2D view: 'Increase' strengthens the layer, 'Decrease' weakens it. The weights of all layers of the same region are normalized.
- Radius / Strength: brush.
- Add layer: enter name, diffuse file ('Browse...') and UV scale. 'Remove layer' deletes the selected layer.
- Undo / Redo for texture strokes.
Individual layers can be shown/hidden in the 'Visibility' area without changing the data.
""","textur layer blend diffuse uv scale malen")

S("map.walk","map","Tab Walk & Block (Begehbarkeit)","Walk & Block tab","""
Das Block&Walk-Gitter besteht aus Zellen von 6,25 Welteinheiten. Eine Zelle ist entweder blockiert (im Overlay rot) oder begehbar. Server-Punkte (NPCs, Wegpunkte) liegen immer auf begehbaren Zellen.
- Sperren / Freigeben: welche Art der Änderung der Pinsel macht.
- Radius: Pinselgröße; '1 Zelle' trifft nur die Zelle unter dem Zeiger.
- Aus Objekten: 'Grundflächen sichtbarer Objekte SPERREN/FREIGEBEN' stempelt die Grundfläche aller gerade sichtbaren Objekte (siehe Sichtbarkeit: z.B. nur 'Gebäude' einblenden) ins Gitter. Ein Klick lässt sich mit Rückgängig zurücknehmen.
- Rückgängig / Wiederholen (Walk).
Objekte erscheinen als Referenz mit ihrer Grundfläche; sie sind in diesem Werkzeug nicht anklickbar, damit man nichts versehentlich verschiebt.
""","""
The Block&Walk grid consists of cells of 6.25 world units. A cell is either blocked (red in the overlay) or walkable. Server points (NPCs, waypoints) always lie on walkable cells.
- Block / Release: which kind of change the brush makes.
- Radius: brush size; '1 cell' only hits the cell under the pointer.
- From objects: 'Block/Release footprints of visible objects' stamps the footprint of all currently visible objects (see Visibility: e.g. show only 'Buildings') into the grid. One click can be reverted with Undo.
- Undo / Redo (Walk).
Objects appear as a reference with their footprint; they cannot be clicked in this tool so nothing is moved by accident.
""","walk block begehbar gitter zelle sperren freigeben grundfläche")

S("map.objects","map","Tab Objekt Platzierung","Object placement tab","""
Objekte sind 3D-Modelle (Häuser, Bäume, Steine ...) mit Position, Drehung und Skalierung.
- Platzieren: Modellpfad eintragen oder 'Durchsuchen...', Rotation und Skalierung einstellen, dann in die 2D-Ansicht klicken.
- Auswählen: in der 2D-Ansicht auf ein Objekt klicken (nächstes innerhalb der Toleranz). Rotation, Skalierung und 'Objekt löschen' wirken auf das gewählte Objekt.
- Die Höhe wird beim Platzieren aus dem Gelände genommen.
- Im 3D-Bild siehst Du die echten Modelle. Objekte, deren Modell nicht geladen werden kann, erscheinen als Platzhalter-Pyramide.
Objekte lassen sich nach Kategorien (Bäume, Gebäude, Felsen ...) ein- und ausblenden (Bereich Sichtbarkeit) - hilfreich bei dichten Karten.
""","""
Objects are 3D models (houses, trees, stones ...) with position, rotation and scale.
- Place: enter a model path or 'Browse...', set rotation and scale, then click into the 2D view.
- Select: click an object in the 2D view (nearest within the tolerance). Rotation, scale and 'Delete object' act on the selected object.
- The height is taken from the terrain when placing.
- In the 3D image you see the real models. Objects whose model cannot be loaded appear as a placeholder pyramid.
Objects can be shown/hidden by category (trees, buildings, rocks ...) in the Visibility area - helpful on dense maps.
""","objekte platzieren modell nif rotation skalierung löschen")

S("map.visibility","map","Bereich Sichtbarkeit","Visibility area","""
Im Werkzeug-Panel steht der Bereich 'Sichtbarkeit'. Er ändert nur die Anzeige, nie die Daten.
- Terrain, Objekt-Modelle, Objekt-Platzhalter, NPC-Modelle, Objekte im 2D-View: je ein Schalter.
- Terrain-Layer: jede Texturschicht einzeln ausblenden ('Alle Layer an' setzt alles zurück).
- Objekt-Kategorien: Bäume & Büsche, Gras & Blumen, Felsen & Steine, Gebäude, Zäune/Mauern/Brücken, Dekoration & Möbel, Wasser & Schiffe, Tiere & Kreaturen, Effekte & Licht, Sonstiges. Mit der Anzahl je Kategorie; 'nur' zeigt allein diese Kategorie, 'Alle an'/'Alle aus' schalten alle.
- Die Kategorie wird aus dem Modellnamen abgeleitet (Schlüsselwörter) und kann daher selten danebenliegen.
- 'NPC-Namen und Blickpfeile': Namen und Blickrichtung der NPCs im 3D-Bild (NPC-Modus).
- Fehlt NPCs das Modell, werden sie hier namentlich aufgelistet.
""","""
The tool panel contains the 'Visibility' area. It only changes the display, never the data.
- Terrain, object models, object placeholders, NPC models, objects in the 2D view: one switch each.
- Terrain layers: hide each texture layer individually ('All layers on' resets).
- Object categories: trees & bushes, grass & flowers, rocks & stones, buildings, fences/walls/bridges, decoration & furniture, water & ships, animals & creatures, effects & light, other. With the count per category; 'only' shows just this category, 'All on'/'All off' switch all.
- The category is derived from the model name (keywords) and can therefore rarely be off.
- 'NPC names and facing arrows': names and facing direction of NPCs in the 3D image (NPC mode).
- If NPCs lack a model they are listed here by name.
""","sichtbarkeit visibility ausblenden einblenden kategorie layer")

S("map.npcs","map","Tab NPC Platzierung","NPC placement tab","""
NPCs kommen aus World/NPC.txt (Server-Ordner) und gehören zu einer Karte. Wähle einen NPC durch Klick auf sein Quadrat in der 2D-Ansicht.
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
Die Zuordnung 'Richtung -> Blickwinkel' ist nicht aus Daten belegt. Stimmt der Pfeil bei einem bekannten NPC nicht mit dem Spiel überein, stelle 'Drehsinn' und 'Blick bei Richtung 0' um (gilt für alle NPCs).
""","""
NPCs come from World/NPC.txt (server folder) and belong to a map. Select an NPC by clicking its square in the 2D view.
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
The mapping 'direction -> facing angle' is not proven by data. If the arrow does not match the game for a known NPC, change 'Rotation sense' and 'Facing at direction 0' (applies to all NPCs).
""","npc platzierung richtung rolle merchant dialog händler blickrichtung")

S("map.ai","map","Tabs NPC AI und Mob AI","NPC AI and Mob AI tabs","""
Diese beiden Tabs haben noch keine eigenen Werkzeuge. Das Verhalten von NPCs und Mobs bearbeitest Du an ihrem jeweiligen Tab: 'KI-Skript (Lua) bearbeiten' und 'Patrouillenroute bearbeiten' im NPC-Tab, 'KI' und 'Route' je Monster im Mob-Tab.
""","""
These two tabs do not have their own tools yet. You edit the behaviour of NPCs and mobs in their respective tab: 'Edit AI script (Lua)' and 'Edit patrol route' in the NPC tab, 'AI' and 'Route' per monster in the mob tab.
""","ai ki lua patrouille")

S("map.mobs","map","Tab Mobs (Monster-Spawns)","Mobs tab (monster spawns)","""
Spawn-Zonen kommen aus MobRegen/<Karte>.txt. Eine Zone hat Position, Radius und eine oder mehrere Monstergruppen.
- Zone wählen: Klick auf das Zonen-Symbol in der 2D-Ansicht. Der Umriss-Kreis zeigt den Spawn-Radius.
- Alle Zonenfelder und Monsterfelder sind editierbar (Anzahl, Respawn, Bereich ...).
- '+ Zone (Kopie der gewählten)': dupliziert die Zone samt Monstern. 'Zone samt Monstern löschen': nur nach Freigabe-Haken.
- '+ Monster hinzufügen': neuer Monstereintrag in der Zone (Name aus MobInfo). 'Monster entfernen'.
- 'KI' und 'Route' je Monster: Verhaltensskript und Wegpunkte (MobRoam).
- 'MobRegen speichern' schreibt die Datei.
Ob der Server per #recordin eingefügte Zeilen wie erwartet verarbeitet, ist ungeprüft.
""","""
Spawn zones come from MobRegen/<map>.txt. A zone has a position, radius and one or more monster groups.
- Select a zone: click the zone symbol in the 2D view. The outline circle shows the spawn radius.
- All zone fields and monster fields are editable (count, respawn, range ...).
- '+ Zone (copy of the selected)': duplicates the zone including monsters. 'Delete zone with monsters': only after the release checkbox.
- '+ Add monster': new monster entry in the zone (name from MobInfo). 'Remove monster'.
- 'AI' and 'Route' per monster: behaviour script and waypoints (MobRoam).
- 'Save MobRegen' writes the file.
Whether the server processes lines inserted via #recordin as expected has not been verified.
""","mob monster spawn zone mobregen respawn")

S("map.portals","map","Tab Portale","Portals tab","""
Schnellreise-Ziele: TownPortal.shn (Stadtportal-Ziele) und RecallCoord.txt (Schriftrollen-Ziele).
- Marker: Raute = TownPortal, Dreieck = Schriftrolle, Quadrat = Gate_Town-NPC, Ring = Regenerationspunkt der Karte.
- X / Y: Zielposition. 'Position per Klick im 2D-View setzen' wählt die Position mit der Maus. 'Menü-Gruppe' und 'Mindestlevel' steuern das Menü.
- 'TownPortal-Ziel hier hinzufügen', 'Entfernen', 'Verwerfen und neu laden'.
- 'TownPortal.shn speichern' und 'RecallCoord.txt speichern'.
Ein TownPortal-Ziel liegt höchstens 205 Einheiten vom zugehörigen Gate_Town-NPC entfernt.
""","""
Fast-travel targets: TownPortal.shn (town portal targets) and RecallCoord.txt (scroll targets).
- Markers: diamond = TownPortal, triangle = scroll, square = Gate_Town NPC, ring = regeneration point of the map.
- X / Y: target position. 'Set position by click in 2D view' picks the position with the mouse. 'Menu group' and 'Minimum level' control the menu.
- 'Add TownPortal target here', 'Remove', 'Discard and reload'.
- 'Save TownPortal.shn' and 'Save RecallCoord.txt'.
A TownPortal target is at most 205 units away from the associated Gate_Town NPC.
""","portal townportal recall schriftrolle teleport")

S("shn.overview","shn","SHN-Editor: Grundlagen","SHN editor: basics","""
SHN sind die Tabellen-Dateien des Spiels (Zeilen und typisierte Spalten). CLIENT-Dateien liegen in ressystem, SERVER-Dateien in 9Data/Shine.
- Dateien einlesen: 'CLIENT: SHN-Ordner einlesen' / 'SERVER: SHN-Ordner einlesen' (geschieht meist automatisch beim ersten Öffnen), 'Einzelne SHN öffnen...' für eine Datei.
- Listen: CLIENT und SERVER getrennt. '*' = geändert. Ein gelbes Warnzeichen zeigt Dateien mit möglicherweise abhängigen Tabellen (Tooltip nennt sie: gleiche Tabelle in Client/Server oder verwandte Tabellen mit gleicher ID).
- Suche: filtert Zeilen; 'Spalten durchsuchen' / 'Werte durchsuchen' legen fest, wo gesucht wird.
- Zelle bearbeiten: Einzelklick wählt, Doppelklick oder 'Zelle bearbeiten' öffnet das Feld. Tippen, dann Enter oder 'Übernehmen'. Ungültige Werte (falscher Typ, zu groß) werden abgelehnt.
- '+ Neue Zeile (in Familie propagieren)': legt eine Zeile mit automatisch freier ID an - in der Datei und in allen zugehörigen Tabellen (z.B. ItemInfo + ItemViewInfo). Die ID ist die erste ID im größten freien Block (nicht Maximum+1).
- 'Speichern' schreibt die Datei.
Beziehungen zwischen Tabellen laufen über ID und InxName (Text-Schlüssel). Ein InxName wird von anderen Tabellen referenziert - vorhandene sollte man nicht umbenennen.
""","""
SHN files are the tables of the game (rows and typed columns). CLIENT files are in ressystem, SERVER files in 9Data/Shine.
- Loading files: 'CLIENT: read SHN folder' / 'SERVER: read SHN folder' (usually happens automatically on first open), 'Open single SHN...' for one file.
- Lists: CLIENT and SERVER separate. '*' = modified. A yellow warning sign marks files with possibly dependent tables (the tooltip names them: same table in client/server or related tables with the same ID).
- Search: filters rows; 'Search columns' / 'Search values' set where to search.
- Edit a cell: single click selects, double click or 'Edit cell' opens the field. Type, then Enter or 'Apply'. Invalid values (wrong type, too large) are rejected.
- '+ New row (propagate to family)': creates a row with an automatically free ID - in the file and in all related tables (e.g. ItemInfo + ItemViewInfo). The ID is the first ID in the largest free block (not maximum+1).
- 'Save' writes the file.
Relations between tables use ID and InxName (text key). An InxName is referenced by other tables - do not rename existing ones.
""","shn tabelle zeile spalte zelle bearbeiten id inxname client server")

S("shn.tabs","shn","Die Tabs des SHN-Editors","The tabs of the SHN editor","""
- Single SHN Editor: beliebige Tabelle als Raster bearbeiten.
- Multi SHN Editor: wähle eine Aufgabe (Neues Item, Neuer NPC, Neuer Mob, Neuer Skill, Shop/Preis, Neue Quest, XP/Rate); der Editor markiert die dafür relevanten Tabellen in CLIENT und SERVER gelb als Kandidaten. Das ist eine Orientierungshilfe, keine bewiesene Abhängigkeit.
- XP Rate Editor: MonEXP und EXPRange aller Mobs (MobInfoServer) um einen Prozentwert ändern ('% Änderung').
- Buy & Sell Editor: BuyPrice und SellPrice aller Items (ItemInfo) um einen Prozentwert ändern.
- Quest Editor: Quests (QuestData.shn) mit Texten aus QuestDialog.
- Portale: TownPortal-Ziele (auch im Map-Editor).
- Custom NPC/Mob: NPCs und Monster aus Vorlagen erzeugen.
- Skill Editor: Skills ändern und neue erstellen.
Die Prozent-Editoren ändern ALLE Zeilen der Spalte - vorher prüfen, dann speichern.
""","""
- Single SHN Editor: edit any table as a grid.
- Multi SHN Editor: choose a task (new item, new NPC, new mob, new skill, shop/price, new quest, XP/rate); the editor marks the relevant tables in CLIENT and SERVER yellow as candidates. This is an orientation aid, not a proven dependency.
- XP Rate Editor: change MonEXP and EXPRange of all mobs (MobInfoServer) by a percentage ('% change').
- Buy & Sell Editor: change BuyPrice and SellPrice of all items (ItemInfo) by a percentage.
- Quest Editor: quests (QuestData.shn) with texts from QuestDialog.
- Portals: TownPortal targets (also in the map editor).
- Custom NPC/Mob: create NPCs and monsters from templates.
- Skill Editor: modify skills and create new ones.
The percentage editors change ALL rows of the column - check first, then save.
""","tabs multi xp rate buy sell quest single")

S("shn.quest","shn","Quest-Editor","Quest editor","""
Links die Liste (Titel oder Beschreibung, Suche nach ID oder Text), rechts die Details:
- Quest-ID, Titel-Text-ID und Beschreibung-Text-ID (Texte aus QuestDialog.shn werden daneben aufgelöst), Mindest-/Maximal-Level, Start-NPC (Mob-ID), Aktiviert, Tägliche Quest, benötigtes Item, Vorgänger-Quest.
- Monster-Ziele (5 Plätze), Item-Ziele (10 Plätze): aktiv, ID, Anzahl; der Name wird aufgelöst (rot = nicht gefunden).
- Drops: Mob, Item, Menge, Rate.
- Skripte Start / Action / Finish: die Quest-Skriptsprache (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'SAY-Text-ID nachschlagen' zeigt den Text zu einer ID.
- Belohnungen (144 Byte) sind noch nicht entschlüsselt und werden unverändert gespeichert.
- 'QuestData.shn speichern'.
""","""
On the left the list (title or description, search by ID or text), on the right the details:
- Quest ID, title text ID and description text ID (texts from QuestDialog.shn are resolved next to them), minimum/maximum level, start NPC (mob ID), enabled, daily quest, required item, predecessor quest.
- Monster targets (5 slots), item targets (10 slots): active, ID, count; the name is resolved (red = not found).
- Drops: mob, item, amount, rate.
- Scripts Start / Action / Finish: the quest script language (SAY, IF, GOTO, ACCEPT, CREATE_ITEM ...). 'Look up SAY text ID' shows the text for an ID.
- Rewards (144 bytes) are not decoded yet and are saved unchanged.
- 'Save QuestData.shn'.
""","quest questdata skript say if goto")

S("creators.npcmob","creators","Custom NPC / Mob erstellen","Creating a custom NPC / mob","""
Tab 'Custom NPC/Mob' im SHN-Editor. Der Assistent klont eine Vorlage in alle Tabellen (MobInfo, MobInfoServer, MobViewInfo, MobSpecies, QuestSpecies, MobWeapon) mit einer überall freien ID.
1. Vorlage: NPC oder Monster wählen (Suche).
2. Bezeichnung und Werte: InxName (eindeutig), Anzeigename, ID automatisch; bei Monstern Level, HP, Tempo, Größe.
3. Aussehen: 'Wie Vorlage', 'Anderes Modell' (Modell aus vorhandenen wählen) oder 'Spieler-Avatar mit Rüstung' (Klasse, Geschlecht, Gesicht, Frisur, Haarfarbe, Ausrüstung je Slot - mit 3D-ähnlicher Vorschau, drehbar).
4. NPC: Dialog der Vorlage kopieren; auf der offenen Karte platzieren (Position, Richtung, Rolle, Argument).
'Anlegen' erzeugt alle Zeilen; 'Alle geänderten SHN speichern' schreibt sie. Händler bekommen danach über den NPC-Tab ihr Inventar.
Klassen der Avatare: 0 Fighter, 1 Archer (keine Modelle), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.
Ob das Spiel selbst angelegte Einträge akzeptiert, ist nicht geprüft.
""","""
Tab 'Custom NPC/Mob' in the SHN editor. The wizard clones a template into all tables (MobInfo, MobInfoServer, MobViewInfo, MobSpecies, QuestSpecies, MobWeapon) with an ID that is free everywhere.
1. Template: choose an NPC or monster (search).
2. Name and values: InxName (unique), display name, ID automatic; for monsters level, HP, speed, size.
3. Appearance: 'Like template', 'Other model' (choose from existing) or 'Player avatar with armor' (class, gender, face, hair, hair color, equipment per slot - with a rotatable preview).
4. NPC: copy the template's dialog; place on the open map (position, direction, role, argument).
'Create' generates all rows; 'Save all changed SHN' writes them. Merchants then get their inventory via the NPC tab.
Avatar classes: 0 Fighter, 1 Archer (no models), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel.
Whether the game accepts self-created entries has not been verified.
""","custom npc mob erstellen klonen avatar vorlage")

S("creators.skills","creators","Skill-Editor","Skill editor","""
Ein Skill ist eine STUFE einer Skillreihe (TripleHit14 = Reihe TripleHit, Stufe 14). Er steckt in ActiveSkill (Client+Server), ActiveSkillInfoServer und ActiveSkillView (Client+Server); lernbare Skills haben ein Skillbuch-Item mit gleichem InxName.
# Ändern
Liste links (Suche nach Name, InxName oder ID), Formular rechts in Bereichen: Grunddaten, Kosten und Zeiten, Schaden, Bewegung/Ziele, Server-Werte, Darstellung, Zustände (Buffs/Debuffs) und 'Alle übrigen Spalten (roh)'. Jede Änderung wird sofort in ALLE Kopien (Client und Server) geschrieben. Der InxName eines vorhandenen Skills bleibt fest.
# Neu aus Vorhandenem
- 'Neuen Skill aus diesem anlegen': klont alle Zeilen (Werte, Server-Werte, Darstellung) mit neuer ID und neuem InxName; optional mit Skillbuch-Item (neue Item-ID).
- 'Nächste Stufe anlegen': legt <Reihe><Stufe+1> an, Voraussetzungs-Skill = dieser Skill.
- Animationen (Zauber-Bereitschaft, Zaubern, Ausführung), Effekte (Geschoss, Treffer, Fläche, Dauerschaden), Sounds und Icon sind NAMEN. Über 'Auswahl...' wählst Du sie aus den bereits verwendeten Werten (Anzahl der Verwendungen in Klammern) und kombinierst so neue Skills aus vorhandenen Animationen und Effekten. Neue Animations-/Effekt-DATEIEN kann der Editor nicht erzeugen.
# Serie skalieren
'Ganze Reihe skalieren': Schaden, Kosten, Abklingzeit oder Zauberzeit aller Stufen einer Reihe um einen Prozentwert ändern.
# Wichtig
Spalten, deren Bedeutung nur aus dem Namen abgeleitet ist, tragen im Tooltip den Zusatz '(vermutet)'. 'Alle geänderten SHN speichern' schreibt alles.
""","""
A skill is one STEP of a skill series (TripleHit14 = series TripleHit, step 14). It lives in ActiveSkill (client+server), ActiveSkillInfoServer and ActiveSkillView (client+server); learnable skills have a skill book item with the same InxName.
# Modify
List on the left (search by name, InxName or ID), form on the right in areas: basics, costs and timing, damage, movement/targets, server values, presentation, states (buffs/debuffs) and 'All remaining columns (raw)'. Every change is written immediately to ALL copies (client and server). The InxName of an existing skill stays fixed.
# New from existing
- 'Create new skill from this one': clones all rows (values, server values, presentation) with a new ID and new InxName; optionally with a skill book item (new item ID).
- 'Create next step': creates <series><step+1>, required skill = this skill.
- Animations (cast ready, casting, execution), effects (projectile, impact, area, damage over time), sounds and icon are NAMES. With 'Choose...' you pick them from the values already in use (number of uses in brackets) and so combine new skills from existing animations and effects. The editor cannot create new animation/effect FILES.
# Scale a series
'Scale the whole series': change damage, costs, cooldown or cast time of all steps of a series by a percentage.
# Important
Columns whose meaning is only derived from the name carry '(assumed)' in the tooltip. 'Save all changed SHN' writes everything.
""","skill editor skills animation effekt buff neu klonen serie stufe")

S("creators.shop","creators","Shop-Editor (Händler-Inventar)","Shop editor (merchant inventory)","""
Im NPC-Tab bei einem Händler: 'Händler-Inventar bearbeiten'. Die Daten liegen in NPCItemList/<NPC-Name>.txt.
- Ein Shop hat Tabs (Kategorien im Shop-Fenster: Tab00, Tab01 ...). Jeder Tab hat Zeilen (Regale) mit bis zu 6 Slots.
- Slot anklicken: Item aus der Liste wählen (Suche nach Name, Bezeichnung, ID). Rechtsklick leert den Slot. Rot = Item nicht in ItemInfo.
- '+ Zeile', '^' 'v' (verschieben), 'x' (löschen, nur mit Freigabe-Haken). '+ Tab' und 'Diesen Tab löschen'.
- Hat der NPC noch keine Shop-Datei: 'Shop-Datei neu anlegen'.
- 'Speichern' schreibt die Datei.
Alle Händlerarten außer SoulStone benutzen diese Dateien; Skill-Händler verkaufen Skillbücher als Items.
""","""
In the NPC tab for a merchant: 'Edit merchant inventory'. The data lives in NPCItemList/<NPC name>.txt.
- A shop has tabs (categories in the shop window: Tab00, Tab01 ...). Each tab has rows (shelves) with up to 6 slots.
- Click a slot: choose an item from the list (search by name, identifier, ID). Right click empties the slot. Red = item not in ItemInfo.
- '+ Row', '^' 'v' (move), 'x' (delete, only with the release checkbox). '+ Tab' and 'Delete this tab'.
- If the NPC has no shop file yet: 'Create shop file'.
- 'Save' writes the file.
All merchant kinds except SoulStone use these files; skill merchants sell skill books as items.
""","shop händler npcitemlist inventar item slot tab")

S("reference.formats","reference","Dateiformate und Zuordnung","File formats and mapping","""
# Karten (Legacy)
- .HTD: Höhenkarte (Blöcke 50 Einheiten). .ini: Kartenbeschreibung mit Layern (Region, Diffuse, UV).
- Texturen: BMP je Layer als Blend-Gewicht.
- .shbd: Block&Walk. Wort = 16 Zellen, Bit gesetzt = blockiert, Zelle = 6,25 Einheiten, Gitter quadratisch.
- .shmd/.idm/.aid: Objekte. Drehung: Quaternion; Editor-Achsen entstehen aus dem Legacy-Format durch Vertauschen (Spiegelung).
# Koordinaten
Welt X = Ost, Welt Z = Nord, Y = Höhe. In NPC.txt sind Coord-X / Coord-Y die Welt-X und Welt-Z. Das 2D-Bild zeigt Norden oben.
# Textdateien (Server)
- World/NPC.txt (MobName, Map, Coord-X, Coord-Y, Direct, NPCMenu, Role, RoleArg0), MobRegen/<Karte>.txt, MobRoam, NPCItemList/<NPC>.txt, Script (Lua).
# Modelle
- NIF (Gamebryo 10.x/20.0.0.4): Objekte in resmap, Charaktere in reschar, Waffen/Items in resitem. Ein kleiner Teil (unter 10 %) von Sonderdateien - Partikel/Effekte - lädt nicht und erscheint als Platzhalter.
""","""
# Maps (legacy)
- .HTD: heightmap (blocks of 50 units). .ini: map description with layers (region, diffuse, UV).
- Textures: BMP per layer as blend weight.
- .shbd: Block&Walk. Word = 16 cells, bit set = blocked, cell = 6.25 units, grid square.
- .shmd/.idm/.aid: objects. Rotation: quaternion; editor axes arise from the legacy format by swapping (mirroring).
# Coordinates
World X = east, world Z = north, Y = height. In NPC.txt Coord-X / Coord-Y are world X and world Z. The 2D image shows north up.
# Text files (server)
- World/NPC.txt (MobName, Map, Coord-X, Coord-Y, Direct, NPCMenu, Role, RoleArg0), MobRegen/<map>.txt, MobRoam, NPCItemList/<NPC>.txt, Script (Lua).
# Models
- NIF (Gamebryo 10.x/20.0.0.4): objects in resmap, characters in reschar, weapons/items in resitem. A small part (under 10 %) of special files - particles/effects - does not load and appears as a placeholder.
""","format htd shbd shmd nif koordinaten datei")

S("reference.trouble","reference","Fehlersuche","Troubleshooting","""
- 'Ordner ... nicht gefunden': Client-/Server-Ordner im Projekt-Hub prüfen; die Ordner müssen ressystem/reschar/resitem bzw. 9Data/Shine enthalten.
- NPC ohne Modell im 3D-Bild: im Bereich Sichtbarkeit steht die Liste. Ursachen: Modell liegt nicht in reschar/res*, Name in MobViewInfo.FileName weicht ab, Datei ist ein nicht ladbares Effekt-NIF.
- Objekt als Pyramide statt Modell: die NIF-Datei fehlt oder ist nicht ladbar.
- Wert lässt sich nicht übernehmen: der Typ der Spalte passt nicht (Zahl statt Text, Wert zu groß). Die Statusmeldung nennt den Grund.
- Skill/NPC/Mob wird im Spiel nicht angezeigt: alle geänderten SHN speichern (Client UND Server) und den Client neu starten; der Client zwischenspeichert Tabellen.
- 3D-Kamera reagiert nicht auf Tasten: Maus über das 3D-Bild bewegen und kein Textfeld aktiv haben.
""","""
- 'Folder ... not found': check client/server folders in the project hub; the folders must contain ressystem/reschar/resitem or 9Data/Shine.
- NPC without a model in the 3D image: the Visibility area lists them. Causes: the model is not in reschar/res*, the name in MobViewInfo.FileName differs, the file is a non-loadable effect NIF.
- Object shown as a pyramid instead of a model: the NIF file is missing or cannot be loaded.
- A value cannot be applied: the column type does not fit (number instead of text, value too large). The status message names the reason.
- Skill/NPC/mob is not shown in game: save all changed SHN (client AND server) and restart the client; the client caches tables.
- 3D camera does not react to keys: move the mouse over the 3D image and make sure no text field is active.
""","fehler problem troubleshooting hilfe nicht gefunden")
