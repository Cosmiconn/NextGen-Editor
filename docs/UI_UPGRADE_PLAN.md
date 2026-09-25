# NextGen Editor – UI Upgrade Plan

> Branch: `ui-upgrade`  
> Ziel: moderne, übersichtliche und erweiterbare Oberfläche, ohne bestehende Fiesta-Editor-Funktionalität zu verlieren.

## 1. Leitprinzipien

1. **Funktion vor Dekoration** – jede vorhandene Funktion bleibt erreichbar; keine Mockup-Funktion wird als fertig dargestellt, wenn sie technisch noch nicht existiert.
2. **Kontext statt Tab-Flut** – häufige Werkzeuge liegen in der Hauptnavigation, Detailfunktionen erscheinen im Inspector des ausgewählten Werkzeugs/Objekts.
3. **2D + 3D als Zentrum** – Kartenbearbeitung bleibt visuell im Mittelpunkt. Panels sind dockbar und können vom Nutzer umgeordnet werden.
4. **Ein konsistentes Icon-System** – klare Silhouetten, identische Strichstärke, Blau als aktiver Akzent, neutrale Symbole im Ruhezustand.
5. **Erweiterbar ohne Umbau** – neue Editoren werden als Workspace/Tool registriert statt neue Sondernavigation zu erfinden.

## 2. Tatsächlicher Funktionsstand, der in die neue Navigation übernommen wird

### Karte
- Heightmap: Anheben, Absenken, Glätten, Einebnen, Radius/Stärke, Undo/Redo.
- Texturen: Layer, Diffuse, UV-Scale, Malen, Layer hinzufügen/entfernen, Sichtbarkeit.
- Walk & Block: Sperren/Freigeben, Pinsel, sichtbare Objektgrundflächen stempeln.
- Objekte: NIF-Bibliothek, Platzieren, Mehrfachauswahl, Gruppenbewegung, Koordinaten, Rotation, Skalierung, Löschen.
- SHMD-Szenenmodelle: Sky, Water, GroundObject, einzeln sichtbar und editierbar.
- NPCs: Platzierung, Richtung, Rollen, Dialog, Lua-KI, Patrouillenroute, Händler/Shop, 3D-Modelle.
- Mobs: Spawn-Zonen, Monstergruppen, Respawn-/Zonenfelder, Lua-KI, MobRoam.
- Portale: TownPortal + RecallCoord, 2D-Positionierung, Level-/Menüparameter.
- Sichtbarkeit: Terrain, Layer, Objekte, Kategorien, NPCs, Sky/Water/GroundObject.

### Spieldaten
- Single SHN Editor.
- Multi SHN Editor.
- XP Rate Editor.
- Buy & Sell Editor.
- Quest Editor.
- Portal Editor.
- Custom NPC/Mob Wizard.
- Skill Editor.
- Händler-/Shop-Editor kontextuell am NPC.

### Animationen
- KFM-Katalog.
- Filter nach KF/Name/Event-ID.
- Übergänge und Detailarrays.
- Dateiverweisprüfung.
- Verlustfreier Kopie-Export.
- Noch **kein** Skelettanimations-Playback und keine KFM-Feldbearbeitung.

### Projekt / Hilfe
- Projekt mit Client-/Serverpfad.
- Map öffnen / neu / speichern.
- Fiesta Import/Export.
- Deutsch/Englisch.
- Tooltips + Handbuch.

## 3. Informationsarchitektur

### Primäre Navigation
- **Karte**
- **Spieldaten**
- **Animationen**
- **Projekt**

### Map Workspace
- obere Command-Bar: Neu / Öffnen / Speichern / Undo / Redo.
- Werkzeugleiste: Heightmap / Textur / Walk & Block / Objekte / NPCs / Mobs / Portale.
- linkes Navigationspanel: Karteninfo, aktueller Modus, schnelle Sichtbarkeit.
- Zentrum: große 3D-Ansicht.
- unten/zweites Dock: 2D-Draufsicht.
- rechts: kontextsensitiver Inspector mit exakt den vorhandenen Werkzeugparametern.
- Statusleiste: Karte, Auswahl, Koordinaten, Lade-/Statusmeldungen.

### Spieldaten Workspace
- linker Bereich: Datenquelle / SHN-Dateien.
- obere Workspace-Tabs: Single, Multi, XP, Preise, Quest, Portale, Custom NPC/Mob, Skill.
- Hauptbereich: Tabelle/Formular.
- rechts optional: Details/Validierung.

### Animationen Workspace
- KFM-Datei + Referenzstatus links.
- Animationen in der Mitte.
- Übergänge/Details rechts/unten.
- spätere Erweiterung: Playback-Dock, sobald technisch vorhanden.

## 4. Icon-System

Icons werden als vektorähnliche ImGui-Primitives gezeichnet, damit keine externe Icon-Font-Abhängigkeit nötig ist.

Pflichtsymbole:
- App/EXE: **NG-Monogramm**.
- Neu, Öffnen, Speichern, Undo, Redo.
- Heightmap/Terrain.
- Pinsel.
- Layer.
- Walk & Block.
- Objekt/Würfel.
- NPC.
- Mob/Spawn.
- Portal.
- Sichtbarkeit/Auge.
- Eigenschaften/Sliders.
- SHN/Tabelle.
- Quest/Buch.
- Skill/Blitz.
- KFM/Animation.
- Projekt/Ordner.
- Einstellungen/Zahnrad.
- Hilfe.

## 5. EXE-Icon

- Windows-Ressource `src/app/resources/nextgen.ico`.
- `src/app/resources/nextgen.rc` bindet das Icon in `map_editor.exe` ein.
- CMake nimmt die Ressource nur unter Windows in das App-Target auf.
- Das Icon bleibt unabhängig von Runtime-Assets verfügbar.

## 6. Umsetzungsphasen

### Phase A – UI-Shell (dieser Branch)
- modernes Theme.
- echte Hauptnavigation nach vorhandenen Funktionen.
- moderner Map-Toolbar mit Icons.
- dockbares Default-Layout 3D-zentriert, 2D ergänzend, Inspector rechts, Navigator links.
- vorhandene Tools unverändert im Inspector weiterverwenden.
- Projekt-Hub korrigieren: Quest/Skill nicht mehr als „Coming Soon“ darstellen.
- NG-EXE-Icon integrieren.
- Dokumentation dieses Plans im Repo.

### Phase B – Panels spezialisieren
- Objektliste als eigenes Dock mit Suche/Kategorie/Sichtbarkeit.
- Properties als eigener Objekt-Inspector.
- Layer-Liste als eigenes Dock.
- **bereits vorgezogen:** permanenter, kontextsensitiver Asset Browser für NIF/DDS.
- **bereits begonnen:** Statusbar mit Karte, aktivem Werkzeug, Auswahl und Statusmeldungen.
- später ergänzen: Koordinaten/FPS und feinere Performance-Anzeigen in der Statusbar.

### Phase C – Dateneditoren angleichen
- SHN/Quest/Skill/Custom NPC/Mob erhalten dieselbe Shell, Toolbar und Inspector-Sprache.
- Shop/Dialog/Lua/Route als kontextuelle Untereditoren statt isolierter Modals, wo sinnvoll.

### Phase D – Erweiterungspunkte
- Interface Editor.
- Drop Table Editor.
- eigener AI Workspace.
- Material/NIF Editing.
- KFM Playback.
- Plugins/Tool-Registry.

## 7. Abnahmekriterien Phase A

- bestehende Core-Tests bleiben grün.
- Windows-GUI kompiliert.
- keine Map-/SHN-/KFM-Dateilogik wird durch das Redesign verändert.
- jeder bisher erreichbare Editor bleibt erreichbar.
- Quest und Skill führen auf ihre bereits vorhandenen Editoren.
- NPC AI/Mob AI werden nicht länger als leere Hauptwerkzeuge beworben; KI/Route bleiben kontextuell bei NPC/Mob.
- NG-Icon ist im Windows-Binary eingebettet.


## 8. Implementierungsstand im Branch `ui-upgrade`

Bereits umgesetzt:
- modernes Dark/Cyan-Theme;
- primäre Navigation Karte / Spieldaten / Animationen / Projekt;
- Project Hub nach realem Funktionsumfang;
- Quest/Skill direkt auf vorhandene Editoren verdrahtet;
- Icon-Command-Bar für Map-Werkzeuge;
- Navigator / 3D / 2D / Inspector als neues Default-Docking;
- kontextsensitiver Asset Browser für NIF-Modelle und DDS-Texturen;
- Statuszeile;
- NG-Icon als native Windows-EXE-Ressource;
- Fenstertitel `NextGen-Editor`.

Zusätzlich in der laufenden zweiten Ausbaustufe umgesetzt:
- Objekt-Outliner als eigenes, filterbares Dock mit Mehrfachauswahl;
- Layer-Manager als permanentes eigenes Dock;
- Sichtbarkeit + Wireframe/Kamera als separates Dock;
- Properties-Inspector dadurch von Listen- und View-Steuerung entlastet;
- kontextuelles Fokussieren von Objekt- bzw. Layer-Dock beim Werkzeugwechsel;
- Statusbar mit Auswahlkoordinaten und FPS;
- Spieldaten-Workspace mit Icon-Toolbar für Single/Multi SHN, XP, Preise, Quest, Portale, NPC/Mob und Skills;
- KFM-Katalog in Animation-Liste + Detail/Transitions-Bereich umgebaut.
- spezialisierte Spieldaten-Editoren (Quest, Portale, Custom NPC/Mob, Skill) nutzen die volle Workspace-Breite statt zusätzlich die generische SHN-Dateileiste einzublenden;
- Quest und Skill in klarer Liste/Eigenschaften-Struktur nachgezogen; Portal- und Custom-NPC/Mob-Kopfbereiche vereinheitlicht;
- Projektkonfiguration sowie Neue-Karte/Karte-öffnen-Flows in die gleiche Dark/Cyan-Designsprache überführt;
- doppelte schwebende Hilfe-/Sprachleiste entfernt; Hilfe und Sprache sitzen zentral in der Hauptnavigation;
- NG-Icon auf native Windows-Größen 16/24/32/48/64/128/256 px erweitert.
- Level-Editor-Werkzeuge aus dem aktuellen Ziel umgesetzt: 3D-Transform-Gizmo mit Move/Rotate/Scale und Snap, Fokus/Boden, Copy/Paste/Duplicate, Lock/Hide, Gruppen/Labels, Kontextmenüs, Rechteck-/Lasso-Auswahl und NIF-Drag&Drop in 3D.
- Terrain-/Walk-Brush-Overlays und Presets sowie Layer-DnD, DDS-Drop, Duplizieren und größere Thumbnails umgesetzt.
- Single-SHN modernisiert: sortierbare/fixierte Tabelle, Spaltenfilter, Dirty-/Fehler-Markierungen, Inline-Editing, Copy/Paste, Undo/Redo und Client/Server-Diff.
- Multi-SHN zu einer Client/Server-Vergleichsansicht mit Schema-/Zeilen-/Zell-Diffs ausgebaut.
- KFM-Playback-Grundlage ergänzt: ausgewählte KF-Dateien können geladen, auf einer Timeline abgespielt und pro Transform-Track live gesampelt werden; Play/Pause, Loop und Geschwindigkeit sind vorhanden. Komprimierte Fiesta-B-Spline-Tracks bleiben bewusst als noch nicht verifiziert markiert, statt falsches Skelett-Playback vorzutäuschen.
- Globales UX nachgezogen: persistente Recent Projects/Recent Maps, getrennter Unsaved-Status für Karte und SHN, Toast-Meldungen, Strg+P-Command-Palette und Strg+S für Map-Speichern.
- Dock-/Workspace-Layout wird dauerhaft im NextGen-Benutzerordner gespeichert und beim nächsten Start wiederhergestellt; ein expliziter Reset stellt das Standardlayout wieder her.

Noch offen für spätere Ausbaustufen:
- spezialisierte NPC-/Mob-/Portal-Outliner statt nur der heutigen kontextuellen Listen;
- gleiche Panel-Sprache in allen tiefen Quest-/Skill-/Custom-Dialog-Unteransichten;
- optional gespeicherte Workspace-Presets;
- zukünftige Interface/Drop-Table/AI/NIF-Material-Editoren.

## 9. Aktuelles Arbeitsziel

Diese Roadmap ist ab jetzt die verbindliche Reihenfolge für den weiteren Ausbau des bestehenden Editors.

### Priorität 1 – Karten-/Level-Editor
1. **3D Transform-Gizmo**
   - Move / Rotate / Scale direkt im 3D-Viewport.
   - X/Y/Z-Achsen.
   - World/Local.
   - Grid-, Winkel- und Scale-Snapping.
   - Mehrfachauswahl bewegt/rotiert/skaliert als Gruppe.
   - Auswahl fokussieren und auf Terrain/Boden setzen.
2. **Asset Drag & Drop**
   - NIF aus dem Asset Browser direkt in 2D/3D platzieren.
   - DDS auf Layer bzw. als neuen Layer ziehen.
3. **Objekt-Produktivität**
   - Duplicate, Copy/Paste.
   - Rechteck-/Lasso-Auswahl.
   - Eye/Lock pro Objekt.
   - Rechtsklick-Kontextmenüs.
   - Frame Selected.
   - später Gruppen/Ordner und frei benennbare Editor-Labels.
4. **Terrain UX**
   - Brush-Kreis und Falloff-Vorschau im 2D-/3D-Viewport.
   - Radius/Stärke-Presets.
   - deutlicher aktiver Sculpt-Modus.
5. **Textur/Layer UX**
   - Drag&Drop-Reihenfolge.
   - größere Thumbnails.
   - Doppelklick-Umbenennen.
   - Kontextmenü und Layer duplizieren.
   - DDS-Drag&Drop.
6. **Walk & Block**
   - klare Legende.
   - Brush-Overlay.
   - Rechteckfüllung.
   - Objekt-/Auswahlflächen als prominente Aktion.
   - Undo/Redo-Zustand sichtbar.

### Priorität 2 – Szeneobjekte
- **NPC:** Inspector-Karten Transform / Rolle / Dialog / Händler / AI-Lua / Route / Darstellung; Rollen-/Quest-/Shop-Markierungen im Outliner.
- **Mobs:** Zone / Monstergruppe / Spawn / MobRoam / Lua; Spawn-/Roam-Flächen in 2D/3D.
- **Portale:** Typ-Icons, gruppierte Eigenschaften, Zielkarte öffnen bzw. Ziel anzeigen.

### Priorität 3 – Spieldaten
- **Single/Multi SHN:** Sortierung, eingefrorene Header/Spalten, Spaltenfilter, Dirty-Markierung, Inline-Editing, Validierung, Copy/Paste, Undo/Redo und Client/Server-Diff.
- **Quest:** Allgemein / Voraussetzungen / Ziele / Belohnungen / Dialoge / Scripts; klickbare Mob/Item/NPC-Referenzen; später optional Quest-Flow.
- **Skill:** Allgemein / Kosten-Cooldown / Schaden / Voraussetzungen / Zustände / Animation-VFX / Serverwerte; Such-Picker für Animationen/Effekte; Skill-Reihen gruppieren.
- **Custom NPC/Mob:** echter 5-Schritt-Assistent mit permanenter Vorschau.

### Priorität 4 – Animationen
- echtes KFM-/KF-Playback.
- Preview-Viewport und Timeline.
- KFM-/Transition-Bearbeitung erst dort, wo der Codec nachweislich verlustfrei und sicher schreiben kann.

### Priorität 5 – Globales UX
- Workspace-Layouts speichern.
- Unsaved-Changes-Anzeige.
- Recent Projects / Recent Maps.
- Toasts.
- globale Suche / Command Palette.
- konfigurierbare Shortcuts.
- konsistente Kontextmenüs.
- vollständiger 16–20px-In-App-Icon-Satz: Terrain, Brush, Layers, Walk, Cube, NPC, Mob, Portal, Eye, Lock, Duplicate, Delete, Transform, Grid/Snap, Quest, Item, Skill, Shop, Dialog, Lua, Route, KFM, Play/Pause, Project, Settings.

### Umsetzungsstand – Level-Editor Meilenstein 1

Bereits umgesetzt:
- ImGuizmo als native Editor-Abhängigkeit eingebunden.
- 3D-Gizmo: Move / Rotate / Scale, X/Y/Z, World/Local und einstellbares Snapping.
- Mehrfachauswahl transformiert um einen gemeinsamen Pivot; Lock wird respektiert.
- vollständige XYZ-Rotation auch numerisch im Eigenschaften-Inspector.
- Auswahl fokussieren und Auswahl auf Terrain setzen.
- 3D-Picking für sichtbare, entsperrte Objekte.
- Rechteckauswahl in 2D mit Shift; Strg+Shift erweitert die bestehende Auswahl.
- Lasso-Auswahl in 2D mit Alt+Shift; Strg+Alt+Shift erweitert die bestehende Auswahl.
- freie Editor-Labels und einklappbare Editor-Gruppen/Ordner im Szene-Outliner; Metadaten folgen Löschen, Duplizieren, Copy/Paste und SHMD-Promotion.
- Copy / Paste / Duplicate inklusive Shortcuts.
- Eye / Lock und Rechtsklick-Kontextmenüs im Szene-Outliner.
- Eye / Lock gilt auch für Sky, Water und GroundObject.
- NIF-Drag&Drop aus dem Asset Browser direkt in 2D und 3D.
- Layer-Drag&Drop-Reihenfolge, größere Thumbnails, Duplizieren, Kontextmenü und Doppelklick-Umbenennen.
- DDS-Drag&Drop auf bestehende Layer oder als neuer Layer.
- Terrain-/Textur-/Walk-Brush-Overlay in 2D und 3D, inklusive Falloff-Anzeige und Presets.
- Walk & Block: rot/grün-Legende und Shift-Drag-Rechteckfüllung als einzelner Undo-Schritt.
- NPC- und Mob-Inspector in fachliche Bereiche gegliedert.
- NPC-/MobRoam-Routen als 2D-/3D-Overlay, wahlweise nur für die aktuelle Auswahl oder dauerhaft für alle relevanten NPCs/Mobs der Karte.
- Mob-Spawn-Zonen zusätzlich als 3D-Overlay.
- Portal-Inspector in Auswahl / Position / Bedingungen / Positionieren / Aktionen gegliedert.
- TownPortal und RecallCoord zusätzlich mit unterschiedlichen 3D-Markern.
- Regressionstests für Gruppenbewegung, Lock, Pivot-Rotation und Gruppenskalierung ergänzt.

Noch offen innerhalb von Priorität 1/2:
- freie Editor-Labels sowie einklappbare Gruppen/Ordner im Objekt-Outliner (reine Editor-Metadaten).
- optional echtes Geometrie-Ray-Picking statt des derzeitigen projizierten Objektursprungs.
- noch tiefere NPC-/Mob-Rollenicons; Route/Roam-Overlays sind inzwischen für Auswahl oder gesamten Kartenkontext verfügbar.
- belastbare Zielkarten-Verknüpfung für Portale erst, sobald die konkrete Outbound-Relation aus den Fiesta-Daten eindeutig belegt ist.



## 9. Aktuelles Ziel

Dieser Abschnitt ist der verbindliche Ausbau-Fahrplan für den laufenden Branch `ui-upgrade`.

### Karten-/Level-Editor
1. 3D Transform-Gizmo für Move / Rotate / Scale mit X/Y/Z-Achsen.
2. Grid-, Winkel- und Scale-Snapping, World/Local, „Auf Boden setzen“ und „Auswahl fokussieren“.
3. Drag & Drop von NIF-Assets direkt in 2D/3D.
4. Copy / Paste / Duplizieren.
5. Eye / Lock und einheitliche Kontextmenüs im Szene-Outliner.
6. Rechteck- und Lasso-Mehrfachauswahl.
7. Objektgruppen/Ordner + freie Editor-Labels als reine Editor-Organisation, ohne das Fiesta-Dateiformat zu verändern. **Umgesetzt.**

### Terrain
- sichtbarer Brush-Kreis in 2D/3D;
- Falloff-Vorschau und kompakte Radius-/Stärke-Presets;
- klare Modi Anheben / Absenken / Glätten / Einebnen.

### Texturen / Layer
- Drag&Drop-Reihenfolge;
- größere Thumbnails und aktive Layer-Hervorhebung;
- Rename, Duplicate und Kontextmenüs;
- DDS-Drag&Drop auf bestehenden oder neuen Layer.

### Walk & Block
- klare Farblegende;
- Brush-Overlay;
- Rechteckfüllung;
- prominente Aktion aus sichtbaren Objektgrundflächen;
- Undo/Redo-Zustand sichtbar.

### Objekte / NPC / Mob / Portale
- moderner Szene-Outliner mit Icons, Sichtbarkeit und Lock;
- kontextuelle Properties statt langer Formularlisten;
- NPC: Transform / Rolle / Dialog / Händler / AI-Lua / Route / Darstellung;
- Mob: Zone / Monstergruppe / Spawnzeiten / MobRoam / Lua;
- Portal: Typ-Icons, Zielkarte, Zielposition und direkte Ziel-Navigation.

### SHN / Spieldaten
- sortierbare, fixierte Tabellen;
- Inline-Editing für einfache Zelltypen;
- Dirty-Indikatoren pro Datei/Zelle;
- Copy/Paste und Undo/Redo;
- anschließend Spaltenfilter, Referenz-/Fehlermarkierung und Client-/Server-Vergleich.

### Quest
- Allgemein / Voraussetzungen / Ziele / Belohnungen / Dialoge / Scripts;
- klickbare Referenzen auf Mob / Item / NPC;
- später optional Quest-Flow/Graph.

### Skill
- Allgemein / Kosten-Cooldown / Schaden / Voraussetzungen / Zustände / Animation-VFX / Serverwerte;
- Picker für Animationen/Effekte;
- Skill-Reihen gruppiert nach Stufen.

### Custom NPC / Mob
- echter 5-Schritt-Assistent:
  1. Vorlage
  2. Identität/Werte
  3. Aussehen
  4. Rolle/Platzierung
  5. Zusammenfassung
- permanente Modell-/Avatar-Vorschau.

### KFM / Animation
- echtes Playback mit Preview-Viewport und Timeline;
- erst danach schreibende Transition-/KFM-Feldbearbeitung, wenn der Codec dafür ausreichend verifiziert ist.

### Globales UX
- gespeicherte Workspace-Layouts;
- Unsaved-Changes-Anzeige;
- Recent Projects / Recent Maps;
- Toasts;
- globale Command-Palette;
- konfigurierbare Shortcuts;
- vollständiger konsistenter Icon-Satz in 16–20 px für Panel-Aktionen.

### Priorität
Die unmittelbare Reihenfolge ist:
**Level-Editor-Interaktion → SHN → Quest/Skill → Custom NPC/Mob → KFM → globale UX-Politur.**
