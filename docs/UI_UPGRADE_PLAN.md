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

Noch offen für spätere Ausbaustufen:
- spezialisierte NPC-/Mob-/Portal-Outliner statt nur der heutigen kontextuellen Listen;
- gleiche Panel-Sprache in allen tiefen Quest-/Skill-/Custom-Dialog-Unteransichten;
- optional gespeicherte Workspace-Presets;
- zukünftige Interface/Drop-Table/AI/NIF-Material-Editoren.
