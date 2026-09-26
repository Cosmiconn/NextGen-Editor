# NextGen Editor – UI Upgrade Plan

> **VISUAL SOURCE OF TRUTH:** Für alle neuen UI-Arbeiten gilt verbindlich [docs/ui-vision/README.md](ui-vision/README.md). Mockups, Tokens, Icon-Quellen und Abnahmekriterien definieren das Zielbild. `NextGen_Icons_Final.zip` ist die technisch verifizierte, alleinige Icon-Quelle: 68 echte SVG-Master, hochwertige PNGs, Small-Size-Exports sowie optionale SVG-mit-PNG-Container; Mapping und Migrationsstatus stehen in [docs/ui-vision/ICON_INVENTORY.md](ui-vision/ICON_INVENTORY.md).


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
- AI Workspace für `LuaScript/AIScript/*.lua` und `MobBehaviorDescript/*.ps` mit Suche, Dirty-State und Textbearbeitung.
- Händler-/Shop-Editor kontextuell am NPC.

### Animationen
- KFM-Katalog.
- Filter nach KF/Name/Event-ID.
- Übergänge und Detailarrays.
- Dateiverweisprüfung.
- Verlustfreier Kopie-Export.
- KF-Transport/Timeline mit Text-Key-Markern und Track-Sampling.
- echter Skeleton-Viewport aus expliziter KFM-NIF-Hierarchie + verifizierten KF-Local-Transforms.
- echte CPU-Skinned-Mesh-Deformation aus bewahrten NiSkin-Weights/Bind-Matrizen ist im KFM-Preview umgesetzt; material-/texturiertes Character-Rendering sowie nicht verifizierte B-Spline/TBC-Semantik bleiben offen.

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
- obere Workspace-Tabs: Single, Multi, XP, Preise, Quest, Portale, Custom NPC/Mob, Skill, AI Scripts.
- Hauptbereich: Tabelle/Formular.
- rechts optional: Details/Validierung.

### Animationen Workspace
- KFM-Datei + Referenzstatus links.
- Animationen in der Mitte.
- Übergänge/Details rechts/unten.
- spätere Erweiterung: Playback-Dock, sobald technisch vorhanden.

## 4. Icon-System

Finale Editor-Icons stammen aus `NextGen_Icons_Final.zip`: 68 verifizierte SVG-Master plus vorbereitete PNG-Runtime-Größen. Der aktuelle Branch enthält davon bewusst nur den bereits migrierten Runtime-Subset; `UiIconAssets` fällt auf den nächstgelegenen eingecheckten freigegebenen Export zurück. ImGui-Primitives bleiben ausschließlich als dokumentierter Fallback für echte Semantik-Lücken oder noch nicht importierte Runtime-Raster; vorhandene Paketsemantik wird nicht auf ein anderes Symbol umgedeutet.

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
- Interface Editor: `resmenu`-Browser mit TGA/DDS/PNG/JPG/BMP-Vorschau und UI-NIF-/Materialinspektor ist umgesetzt. Zusätzlich können ausgewählte Assets non-destruktiv als Projekt-Override nach `<Projekt>/Client/resmenu/...` gespiegelt und wieder entfernt werden. Overrides werden beim Scan bytegenau gegen den Clientbestand als identisch, geändert oder projekt-only klassifiziert, separat gezählt und über „Nur Abweichungen“ filterbar gemacht; echte Layout-/Asset-Inhaltsbearbeitung bleibt offen.
- Drop Table Editor.
- eigener AI Workspace. **Umgesetzt.**
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
- primäre Navigation Karte / Spieldaten / Animationen / Projekt als aktive Icon+Text-Shell;
- globale funktionale Menüleiste `Datei / Bearbeiten / Ansicht / Map / Objekte / Terrain / Layer / Werkzeuge / Fenster / Hilfe`; Einträge sind an reale bestehende Aktionen gebunden und kontextabhängig disabled;
- Project Hub nach realem Funktionsumfang;
- Quest/Skill direkt auf vorhandene Editoren verdrahtet;
- Icon-Command-Bar für Map-Werkzeuge; App-Shell-Dateiaktionen und Project-Hub nutzen ebenfalls die finalen semantischen Paketicons;
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
- zentrale Panel-Header für Outliner, Layer, Asset Browser, Sichtbarkeit, Properties, 2D, 3D und KFM auf eine gemeinsame Icon-/Titel-/Kontext-Komponente vereinheitlicht;
- kontextuelles Fokussieren von Objekt- bzw. Layer-Dock beim Werkzeugwechsel;
- Statusbar mit Auswahlkoordinaten und FPS;
- Spieldaten-Workspace mit Icon-Toolbar für Single/Multi SHN, XP, Preise, Quest, Portale, getrenntes Custom NPC/Custom Mob, Skills, AI, Interface und Drops;
- KFM-Katalog in Animation-Liste + Detail/Transitions-Bereich umgebaut.
- spezialisierte Spieldaten-Editoren (Quest, Portale, Custom NPC/Mob, Skill) nutzen die volle Workspace-Breite statt zusätzlich die generische SHN-Dateileiste einzublenden;
- zentrale Workspace-Header von Spieldaten, Multi SHN, Quest, AI, Custom NPC/Mob, Skill, Drop Table und Interface nutzen denselben Panel-Header-Component und die finalen semantischen Icons; Portal verwendet mangels freigegebenem Portal-Haupticon bewusst den dokumentierten DrawList-Fallback;
- Quest und Skill in klarer Liste/Eigenschaften-Struktur nachgezogen; Portal- und Custom-NPC/Mob-Kopfbereiche vereinheitlicht;
- Projektkonfiguration sowie Neue-Karte/Karte-öffnen-Flows in die gleiche Dark/Cyan-Designsprache überführt;
- doppelte schwebende Hilfe-/Sprachleiste entfernt; Hilfe und Sprache sitzen zentral in der Hauptnavigation;
- NG-Icon auf native Windows-Größen 16/24/32/48/64/128/256 px erweitert.
- Level-Editor-Werkzeuge aus dem aktuellen Ziel umgesetzt: 3D-Transform-Gizmo mit Move/Rotate/Scale und Snap, Fokus/Boden, Copy/Paste/Duplicate, Lock/Hide, Gruppen/Labels, Kontextmenüs, Rechteck-/Lasso-Auswahl und NIF-Drag&Drop in 3D.
- Terrain-/Walk-Brush-Overlays und Presets sowie Layer-DnD, DDS-Drop, Duplizieren und größere Thumbnails umgesetzt.
- Single-SHN modernisiert: sortierbare/fixierte Tabelle, Spaltenfilter, Dirty-/Fehler-Markierungen, Inline-Editing, Copy/Paste, Undo/Redo und Client/Server-Diff.
- Multi-SHN zu einer Client/Server-Vergleichsansicht mit Schema-/Zeilen-/Zell-Diffs ausgebaut.
- KFM-Playback ausgebaut: ausgewählte KF-Dateien können geladen und mit Play/Pause, Loop, Geschwindigkeit und Scrub abgespielt werden; die Timeline zeigt echte KF-Text-Key-Marker. Der NIF-/Skeleton-Viewport verwendet die explizite KFM-NIF-Hierarchie, verifizierte KF-Local-Transforms und die bewahrten NiSkin-Weights/Bind-Matrizen. Samplebare Tracks deformieren die echten NIF-Dreiecke CPU-seitig synchron zur Timeline; das Mesh wird performant als Wireframe hinter dem Skeleton angezeigt. Nicht samplebare B-Spline/TBC-/Quadratic-Tracks und mehrdeutige Namen bleiben bewusst in Bind-Pose.
- Globales UX nachgezogen: persistente Recent Projects/Recent Maps, getrennter Unsaved-Status für Karte und SHN, Toast-Meldungen, Command-Palette und Map-Speichern per Shortcut.
- konfigurierbare Shortcuts für Command Palette, Map-Speichern, Gizmo Move/Rotate/Scale, Fokus, Auf-Terrain, Duplizieren und Löschen werden im NextGen-Benutzerordner persistiert; sichtbare Shortcut-Hinweise in Command Palette, Inspector, Szene-Outliner und Objekt-Kontextmenüs leiten sich aus der aktuellen Belegung ab statt feste Default-Tasten vorzutäuschen.
- Dock-/Workspace-Layout wird dauerhaft im NextGen-Benutzerordner gespeichert und beim nächsten Start wiederhergestellt; ein expliziter Reset stellt das Standardlayout wieder her.
- QA/Politur: der 3D-Gizmo-Overlay nutzt für Move/Rotate/Scale dieselben finalen semantischen Paketicons wie Command-Bar und Inspector; der Navigator folgt jetzt ebenfalls dem gemeinsamen Panel-Header-System.
- QA/Politur: AI-Aktionen sind auf `module.ai`, MobRegen-Einträge auf `nav.spawns`, Portal-Positionierung auf `transform.move` und Route/MobRoam auf `gameplay.path` verdrahtet; fehlende Route-Raster im Checkout bleiben korrekt funktionaler DrawList-Fallback.
- QA/Politur: die gemeinsame Search-Chrome mit `panel.search` deckt jetzt Asset Browser, Szene-Outliner, AI Workspace, NIF Inspector, Interface, SHN-Datei-/Zeilensuche, Quest, Skill, Drop Table, Asset-/String-/Item-/Template-Picker, Handbuch und Command Palette ab; NIF-/Interface-Filter verwenden zusätzlich `panel.filter`. Popup-/Palette-Autofokus wird gezielt auf das Eingabefeld gesetzt, nicht auf das vorgeschaltete Icon.
- Icon-Checkout-Audit: das Final-ZIP selbst ist vollständig, Git enthält aktuell aber nur einen Runtime-Subset. Vollimport von SVG-Mastern + 68×6 Runtime-Rastern bleibt reproduzierbar über `tools/ui/import_icon_pack.py` offen.
- Icon-Konsistenzcheck: `tools/ui/check_icon_consistency.py` läuft in CI und prüft die 68 semantischen IDs, `UiIconAssets.cpp`, die freigegebenen Größen sowie jeden eingecheckten Runtime-PNG-Pfad; ein partieller Runtime-Subset bleibt dabei ausdrücklich zulässig.
- QA/Politur: Map-Launcher, Quest-/Skill-Unterpanels und Active Tool verwenden jetzt die gemeinsame Header-Chrome; AI-Bibliothek/Editor und NIF-Material behalten ihre kompakten Inline-Aktionen, zeigen dort aber die passenden finalen Paketicons.
- QA/Politur: auch der linke Raw-SHN-Workspace nutzt den gemeinsamen Panel-Header; Single SHN und Multi SHN wechseln dabei zwischen `module.shn.single` und `module.shn.multi`.
- 3D-Viewport-Interaktion: Gizmo-Toolbar und Zoom-Overlay besitzen explizite Capture-Flächen; Klicks/Drags auf diese Controls gelangen nicht mehr zusätzlich in Objekt-Picking, Orbit/Pan oder Keyboard-Kamerasteuerung.
- QA/Politur: SHN-, NPC-, Mob-, Portal-, Objekt- und Layer-Kontextmenüs verwenden eine gemeinsame Kopfzeilen-Chrome mit semantischem Paketicon/Fallback und klar getrennten Fokus-, Bearbeitungs- und destructive Aktionsgruppen.
- Layer-Kontextmenü: Rename/Duplicate/Remove sind direkt erreichbar; Rechtsklick selektiert den Layer und der bestehende Inline-Rename erhält über einen expliziten Pending-Fokus zuverlässig Tastaturfokus.
- NIF-Textur-QA: eingebettete `NiPixelData` ist ein First-Class-Texturpfad (`Use External = 0` darf einen leeren Dateinamen haben). Alle UV-Sets werden vor dem Renderer plausibilisiert; ungültige sekundäre Sets können deterministisch auf UV0 zurückfallen. Materialslots und Flipbook-Frames behalten Embedded-Herkunft + PixelData-Ref bis in Renderer/Inspector; fehlgeschlagene Embedded-Dekodierung wird nicht als externer Pfad umgedeutet und im NIF-Inspector sichtbar gemeldet.
- NIF-Embedded-Regression: `test_nif_model` läuft in CI mit `tree05.nif` sowie den älteren 10.2.0.0-Dateien `wagon2.nif` und `ship_post.nif`; alle drei müssen Embedded-PixelData an Materialslots binden und sämtliche NiPixelData-Blöcke dekodieren. Ubuntu-CI #1097 bestätigt diese Bedingungen. Fixture-Binäraudit: alle 296 direkt lesbaren Textur-Dateinamensfelder haben `Use External = 0`; ein Dateiname ist damit häufig nur Metadatum.

Noch offen für spätere Ausbaustufen:
- NPC-/Mob-/Portal-Outliner sind inzwischen spezialisiert: semantische Rollen-/Gruppenicons, Auswahl-Details, direkte Kontextaktionen, Gate-Zielnavigation und Schnellfilter (NPC-Rolle, Mob-Belegung, Portal-Typ); zusätzlich strukturieren einklappbare semantische Gruppen mit Trefferzählern die Listen nach `Role + RoleArg0`, Mob-Belegung und Portal-Typ;
- Custom NPC/Mob Wizard einschließlich Vorschau, Schrittleiste, Ausrüstung, Rollenplatzierung und Zusammenfassung vollständig an die DE/EN-Sprachumschaltung angebunden.
- tiefer Quest-Editor (Allgemein, Voraussetzungen, Ziele, Drops, Belohnungen, Dialoge und Scripts) vollständig an die DE/EN-Sprachumschaltung angebunden.
- persistente Map-Workspace-Presets ergänzt: Standard, 3D-Fokus, Terrain/2D und Daten/Szene; im Navigator, in den Einstellungen und über Strg+P/Command-Palette erreichbar.
- eigenständiger AI Workspace im Spieldatenbereich: rekursiver Lua-/PineScript-Katalog, Suche, Volltexteditor, Dirty-State, Reload/Verwerfen und sicheres Speichern; bestehende NPC-/Mob-Kontextaktionen verwenden denselben Editor-Unterbau.
- Skill-, Quest- und Custom NPC/Mob-Editor sowie Händler-/Shop-, NPC-Dialog- und Patrouillen-/MobRoam-Untereditor verwenden inzwischen durchgängig dieselbe DE/EN-Panel-Sprache. Auch der gesamte Szene-Outliner ist an die Sprachumschaltung angebunden: NPC-/Mob-/Portal-Gruppen, Filter, Tooltips und Kontextaktionen ebenso wie Objektgruppen, Labels, Eye/Lock und Auswahlaktionen; Layer-Manager und Sichtbarkeits-Dock einschließlich dynamischer Objektkategorien folgen ebenfalls DE/EN. Der Karten-Asset-Browser, der read-only NIF-/Materialinspektor sowie Navigator, Properties-Chrome, 2D-/3D-Überschriften, Presets und Statuszeile des Map-Workspace sind ebenfalls zweisprachig. Auch die primären Properties-Workflows für Terrain, Textur, Walk & Block, Objekt-Transform, NPC, Mob und Portale sind an DE/EN angebunden; verbleibende Spezialdialoge können schrittweise nachgezogen werden;
- gespeicherte Map-Workspace-Presets sind umgesetzt: Standard, 3D-Fokus, Terrain/2D und Daten/Szene; die Auswahl bleibt über `workspace.txt` erhalten und das danach frei angepasste Dock-Layout weiterhin über `layout.ini`;
- NIF-Materialinspektor im Asset Browser ist read-only umgesetzt: Mesh-/Materialdaten, Textur-Slots, DDS/TGA-/Embedded-Vorschauen, gecachte Pfadauflösung sowie Filter/Diagnose für fehlende Texturdateien. Fehlende Referenzen aus Flipbook-Animationen werden ebenfalls in Suche, Gesamtzähler und „Nur fehlende Texturen“ einbezogen; eine deduplizierte Referenzliste lässt sich zur Reparatur kopieren. Schreibende NIF-Materialbearbeitung bleibt bewusst zurückgestellt, bis ein verlustfreier Writer belegt ist.
- Interface-Ausbaustufe 1 ist umgesetzt: automatischer `Client/resmenu`-Katalog, Suche, TGA/DDS- sowie PNG/JPG/BMP-Vorschau (WIC unter Windows) und read-only NIF-/Materialanalyse für UI-NIFs. Als erste schreibende, formatsichere Stufe gibt es non-destruktive Projekt-Overrides: ein ausgewähltes Asset wird unverändert nach `<Projekt>/Client/resmenu/...` kopiert, aktive Overrides sind im Katalog markiert, die Vorschau bevorzugt die Projektkopie und der Override kann wieder entfernt werden. Unter Windows kann ein Asset außerdem durch eine externe Datei gleicher Endung direkt in dieser Projektkopie ersetzt werden; beim Rescan werden auch Projekt-only Assets aus `<Projekt>/Client/resmenu/...` in den Katalog aufgenommen. Der Override-Katalog validiert den Projektbestand bytegenau gegen die Originale und unterscheidet `identisch`, `geändert` und `nur im Projekt`; dafür gibt es eigene Badges, Zähler und einen Filter nur für tatsächliche Abweichungen. Byte-identische Projektkopien lassen sich gesammelt bereinigen, ohne geänderte oder projekt-only Assets anzutasten. Die Ergebnisse werden nach Scan/Änderung gecacht, sodass der Dateivergleich nicht pro UI-Frame läuft. Bei UI-NIFs gilt derselbe Overlay-Mechanismus jetzt auch für externe Textur- und Flipbook-Referenzen sowie für „Neu laden“: vorhandene Dateien aus `<Projekt>/Client/resmenu/...` werden vor den read-only Originalen aufgelöst. Der Original-Clientbestand bleibt unangetastet. Inhaltliche Layout-Bearbeitung und schreibendes NIF-Material-Editing bleiben offen; AI Workspace ist umgesetzt.
- Drop-Table-Ausbaustufe 1 ist umgesetzt: der ShineText-Parser normalisiert das reale `ItemDropTable.txt`-Schema mit 290 Datenspalten plus trailing `;`-Sentinel formatgetreu und besitzt dafür einen >270-Spalten-Roundtrip-/Editier-Regressionstest. Im Spieldaten-Workspace gibt es eine semantische `ItemGroup`-Ansicht mit Mob-/Map-/Drop-Item-Suche, Basiswerten, 45 Drop-Slots, Rate/Anzahl/Upgrade/Rule, ItemInfo-Auflösung und Ausschlussitems. Basis- und Slotwerte sind über den minimal-invasiven ShineText-Writer editierbar; Dirty-State, Strg+S und Neu-laden/Verwerfen sind integriert. Die Validierung umfasst MobId gegen `MobViewInfo.shn`, Drop- und Ausschlussitems gegen `ItemInfo.shn`, `MinLevel <= MaxLevel`, `MinCen <= MaxCen`, Upgrade-Min/Max pro aktivem Slot sowie die in allen 1.485 bereitgestellten NA2016-Records belegte Regel `CheckSum = MaxLevel + 1`. Problem-Records lassen sich gezielt filtern; die Detailansicht nennt die konkrete Fehlerart, eine vollständige Problemliste kann für Audits in die Zwischenablage kopiert werden, und CheckSum wird beim Ändern von MaxLevel automatisch nachgeführt.

## Minimap / Overview – verifizierungsgebundener Ausbau

- eigener Roadmap-Punkt, nicht nur Dekoration des 2D-Views;
- **Editor-Preview umgesetzt:** eigener Minimap-Dock mit unabhängigem Top-Down-FBO, Whole-Map-Fit, optionalem Objekt-Overlay, 2D-Viewport-Rahmen und Klick-zum-Zentrieren;
- zuerst echte Fiesta-Beispiele aus Client-/Map-Daten vergleichen;
- Dateiname, Pfad, Format, Auflösung, Alpha und Orientierung werden in [MINIMAP_FORMAT.md](MINIMAP_FORMAT.md) dokumentiert;
- vor Formatverifikation ist nur eine editorinterne Top-Down-Preview zulässig;
- Fiesta-Export erst nach belegter Zuordnung Map → Minimap;
- erste Preview: orthografisch, ganze Map, korrekte Aspect Ratio, Terrain/Texturen/Wasser, wichtige Objekte optional;
- NPC/Portal/Spawn/Walk-Overlays bleiben standardmäßig Editor-Preview;
- Kamera-/Viewport-Rahmen und Click-to-Focus sind anschließende UX-Schritte.

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
- **Quest:** Allgemein / Voraussetzungen / Ziele / Belohnungen / Dialoge / Scripts; klickbare Mob/Item/NPC-Referenzen. Optionaler Quest-Flow ist jetzt read-only umgesetzt: predecessor-Kette, aktuelle Quest und direkte Folgequests stammen ausschließlich aus den verifizierten `needPred/predecessor`-Feldern; Zyklen, fehlende und doppelte IDs werden sichtbar markiert.
- **Skill:** Allgemein / Kosten-Cooldown / Schaden / Voraussetzungen / Zustände / Animation-VFX / Serverwerte; Skill-Reihen gruppiert. Animation/VFX-Picker aggregieren jetzt reale Werte über alle gleichartigen View-Felder, bieten Suche sowie eine Referenz-Vorschau mit den Skills/Spalten, die den Wert tatsächlich verwenden. Physische KF/NIF/VFX-Asset-Existenz wird dabei bewusst noch nicht behauptet.
- **Custom NPC/Mob:** echter 5-Schritt-Assistent mit permanenter Vorschau.

### Priorität 4 – Animationen
- echtes KFM-/KF-Playback; **Timeline/Transport umgesetzt**
- Preview-Viewport; **Skeleton-Viewport aus echter NIF-Hierarchie + verifizierten KF-Tracks umgesetzt**
- animierte Mesh-Deformation/Skinned-Mesh-Playback; **umgesetzt als echte CPU-Deformation der NIF-Dreiecke mit bewahrten NiSkin-Weights/Bind-Matrizen, synchron zur Timeline**
- material-/texturiertes Character-Preview auf Basis derselben Pose; **offen / Politur**
- komprimierte Fiesta-B-Spline-/TBC-/Quadratic-Sampler nur nach Verifikation; **offen**
- KFM-/Transition-Bearbeitung nur dort, wo der Codec nachweislich verlustfrei und die Runtime-Semantik belegt ist.

### Priorität 5 – Globales UX
- Workspace-Layouts speichern.
- Unsaved-Changes-Anzeige.
- Recent Projects / Recent Maps.
- Toasts.
- globale Suche / Command Palette.
- konfigurierbare Shortcuts. **Umgesetzt** – persistiert, konfliktmarkiert und mit dynamischen Shortcut-Hinweisen in den relevanten Map-UI-Flächen.
- konsistente Kontextmenüs. **Umgesetzt** – Objekt/NPC/Mob/Portal zeigen denselben konfigurierten Fokus-Shortcut; SHN/Objekt/Layer laufen über den gemeinsamen MenuItem-Wrapper, SHN Copy/Paste ist DE/EN-konsistent.
- vollständiger 16–20px-In-App-Icon-Satz: Terrain, Brush, Layers, Walk, Cube, NPC, Mob, Portal, Eye, Lock, Duplicate, Delete, Transform, Grid/Snap, Quest, Item, Skill, Shop, Dialog, Lua, Route, KFM, Play/Pause, Project, Settings.

### Umsetzungsstand – Level-Editor Meilenstein 1

Bereits umgesetzt:
- ImGuizmo als native Editor-Abhängigkeit eingebunden.
- 3D-Gizmo: Move / Rotate / Scale, X/Y/Z, World/Local und einstellbares Snapping.
- Mehrfachauswahl transformiert um einen gemeinsamen Pivot; Lock wird respektiert.
- vollständige XYZ-Rotation auch numerisch im Eigenschaften-Inspector.
- Auswahl fokussieren und Auswahl auf Terrain setzen.
- 3D-Picking für sichtbare, entsperrte Objekte; geladene NIFs werden exakt gegen ihre Dreiecksgeometrie geraycastet.
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
- szenenweites Frame Selected: F bzw. Doppelklick im Szene-Outliner fokussiert Objekte, NPCs, Mob-Zonen und Portale im 3D-Viewport.
- spezialisierte NPC-/Mob-/Portal-Outliner mit semantischen Icons, Schnellfiltern, einklappbaren semantischen Gruppen samt Zählern, Rechtsklick-Aktionen und ausgewählten Inline-Details/Aktionen (Dialog/AI/Route/Shop, Mob-Einträge, Portal-Ziele).
- Mob-Spawn-Zonen zusätzlich als 3D-Overlay.
- Portal-Inspector in Auswahl / Position / Bedingungen / Positionieren / Aktionen gegliedert.
- TownPortal und RecallCoord zusätzlich mit unterschiedlichen 3D-Markern.
- ausgehende Gate-Ziele aus `World/NPC.txt` datenbasiert aufgelöst (`ShineNPC.RoleArg0 -> LinkTable.argument`), inklusive Zielkarte/-koordinate, Richtung, Party-Flag und sicherer direkter Zielnavigation.
- Regressionstests für Gruppenbewegung, Lock, Pivot-Rotation und Gruppenskalierung ergänzt.

Noch offen innerhalb von Priorität 1/2:
- freie Editor-Labels sowie einklappbare Gruppen/Ordner im Objekt-Outliner (reine Editor-Metadaten).
- echtes Geometrie-Ray-Picking für geladene NIF-Dreiecke ist umgesetzt; nur nicht ladbare NIFs verwenden noch den projizierten Marker-Fallback.
- tiefere NPC-/Mob-Semantik ist umgesetzt: NPC-Icons werten `Role + RoleArg0` aus (Quest/GBDice, Waffen, Skill, SoulStone, Guild, Coin/RandomOption usw.); Mob-Zonen unterscheiden leer/eine Art/gemischte Gruppe und zeigen Art-/Mobzahlen. Route/Roam-Overlays sind für Auswahl oder gesamten Kartenkontext verfügbar.
- belastbare Zielkarten-Verknüpfung ist für NPC-Gates belegt und umgesetzt (`RoleArg0 -> LinkTable.argument`); TownPortal/RecallCoord bleiben korrekt als Ziel-/Ankunftsdaten behandelt und werden nicht fälschlich als Outbound-Link interpretiert.



## 9. Aktuelles Ziel

Dieser Abschnitt ist der verbindliche Ausbau-Fahrplan für den laufenden Branch `ui-upgrade`.

### Karten-/Level-Editor
1. 3D Transform-Gizmo für Move / Rotate / Scale mit X/Y/Z-Achsen.
2. Grid-, Winkel- und Scale-Snapping, World/Local, „Auf Boden setzen“ und „Auswahl fokussieren“.
3. Drag & Drop von NIF-Assets direkt in 2D/3D.
4. Copy / Paste / Duplizieren.
5. Eye / Lock und einheitliche Kontextmenüs im Szene-Outliner. **Umgesetzt.**
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
- klare Farblegende; **umgesetzt**
- Brush-Overlay; **umgesetzt**
- Rechteckfüllung; **umgesetzt**
- sichtbare Objektgrundflächen werden aus NIF-Hull/Bounding-Fallback abgeleitet; **umgesetzt**
- Footprint-Workflow ist jetzt **Preview → Apply/Cancel**: rote/grüne Polygone zeigen vor dem Schreiben exakt die aktuell sichtbaren Grundflächen; Apply aggregiert alle Polygone zu genau einem Undo-Step; **umgesetzt**
- Undo/Redo-Zustand als explizite Icon-Aktionen mit Disabled-State sichtbar; **umgesetzt**.

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
- Allgemein / Voraussetzungen / Ziele / Belohnungen / Dialoge / Scripts; **umgesetzt**
- klickbare Referenzen auf Mob / Item / NPC; **umgesetzt**
- read-only Quest-Flow/Graph für verifizierte `needPred/predecessor`-Kanten mit Vorgänger-Kette, direkter Folgequest-Navigation, Zyklus-/Missing-/Duplicate-ID-Hinweisen; **umgesetzt**
- Script-`GOTO`/`ACCEPT` und unbekannte Reward-/Raw-Felder werden bewusst nicht als Graph-Semantik interpretiert.

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
- echtes KF-Playback mit Text-Key-Timeline; **umgesetzt**
- interaktiver Skeleton-Viewport über die echte KFM-NIF-Hierarchie; **umgesetzt für verifizierte, samplebare Transformtracks**
- Skinned-Mesh-Deformation; **umgesetzt** – Source-Vertices, Skin-Weights, Bone-Refs und Bind-Transforms werden im NIF-Modell bewahrt und pro Timeline-Zeit neu ausgewertet
- NIF-Mesh im Preview als performantes Wireframe; **umgesetzt**, material-/texturierter Preview bleibt Politur
- B-Spline/TBC/Quadratic nur nach Datenverifikation; **offen**
- weitergehende schreibende Transition-/KFM-Feldbearbeitung nur, wenn zusätzlich zur Codec-Erhaltung auch die Runtime-Semantik ausreichend verifiziert ist.

### Globales UX
- gespeicherte Workspace-Layouts; **umgesetzt**
- Unsaved-Changes-Anzeige; **umgesetzt**
- Recent Projects / Recent Maps; **umgesetzt**
- Toasts unten rechts, 4 s, maximal 3 gleichzeitig; **umgesetzt**
- globale Command-Palette; **umgesetzt**
- konfigurierbare Shortcuts; **umgesetzt**
- vollständiger konsistenter Icon-Satz in 16–20 px für Panel-Aktionen; **laufende QA / Vollimport des Final-Pakets noch offen**.

### Priorität
Die unmittelbare Reihenfolge ist:
**Icon-/UI-Vision festziehen → App-Shell/Map-UI-Retrofit → Minimap-Metadaten + Editor-Preview (erreicht; Exportformat weiter gesperrt) → Walk/Block-Footprints (Preview/Apply erreicht) → SHN-Referenzen (verifizierte Kernfamilien erreicht) → Skill Animation/VFX (datenbelegte Picker/Referenz-Vorschau erreicht) → KFM-Playback (Timeline + Skeleton + echte CPU-Skinned-Mesh-Deformation erreicht; B-Splines/materialisiertes Preview offen) → Quest-Flow (verifizierte predecessor-Sicht erreicht) → globale QA/Politur.**
