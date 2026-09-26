# HANDOFF — Stand für Fortsetzung in neuem Chat

## Aktueller Stand: v12, 23.09.2026

Maßgeblich ist [docs/V12_VALIDATION.md](docs/V12_VALIDATION.md), einschließlich des vollständigen
NIF-Inventars und der getrennten Standard-/Recovery-Ergebnisse in `docs/v12-results/`.
Core und GUI wurden unter MSVC gebaut, 13/13 CTest bestanden; zusätzlich wurde der echte
OpenGL-Renderer mit fünf Particle-Fixtures auf einer RTX 3060 getestet. Keine vollständige
Partikelsimulation und kein visueller Gleichheitstest mit NifSkope.

Der Standardparser liest jetzt die dokumentierten Blockgrenzen und den Footer exakt.
Die alten 14/15-Float-, Trailer- und Namens-Heuristiken sind nur im Recovery-Pfad aktiv.
`LoadNifMesh(path, false)` deaktiviert Recovery. `NifModel::recovered` und `partial` kennzeichnen
Fallback-Ergebnisse. `tools/scan_nifs.py` trennt beide Phasen und beendet langsame Einzelprozesse
über einen harten OS-Timeout. Nicht auf interne Zeitbudgets als einzige Absicherung verlassen.

Die nachfolgenden Abschnitte dokumentieren ältere Entwicklungsstände. Frühere Aussagen zu
fehlenden Build-/OpenGL-Möglichkeiten und alten NIF-Erfolgsquoten sind durch v12 überholt.

Dieses Dokument ist der schnelle Einstieg. Für Details siehe `docs/MAP_FORMAT.md` (vollständige
Reverse-Engineering-Historie mit Herleitungen) und `CHANGELOG.md` (chronologische Änderungen).

## Projekt
Standalone C++23 Map-Editor für TheSeed (MMORPG-Engine, Cosmiconn/David) — Bildungsprojekt.
GLFW + Dear ImGui (Docking) + OpenGL 3.3 über vcpkg. Windows + Linux. Reverse-engineert ein
Legacy-Kartenformat eines koreanischen MMORPGs (vermutlich DirectX-basiert), um Karten/Objekte
in einem modernen, eigenständigen Editor zu öffnen und zu bearbeiten.

## Arbeitsweise (bitte beibehalten)
- **Immer an echten Referenzdateien verifizieren**, nie nur an Byte-Anzahl - eine Byte-LÄNGEN-
  Prüfung (landet exakt auf dem Dateiende) kann zwei kompensierende Fehler NICHT erkennen, wenn
  beide dieselbe Gesamtlänge ergeben (siehe das gelöste UV-Rätsel, docs/MAP_FORMAT.md
  Abschnitt 6: ein fehlendes Feld hier, ein überzähliges dort - Länge stimmte trotzdem). Wo
  möglich IMMER zusätzlich eine Inhalts-Plausibilitätsprüfung machen (Werte im erwarteten
  Bereich, nicht nur "Datei endet exakt").
- **Mehrere unterschiedliche Dateien vergleichen**, nicht nur zwei ähnliche.
- **Öffentliche, unabhängige Referenzimplementierungen sind extrem wertvoll und erlaubt**
  (z.B. NifTools/NifSkope, oder der öffentliche, quelloffene Rust-NIF-Parser "nif" von docs.rs,
  der explizit Version 20.0.0.4 - unsere Version - als Ziel angibt) - unabhängig vom Fiesta-
  Server-Cleanroom-Constraint, der nur proprietäre Server-Binärdateien betrifft. Diese Session
  löste ZWEI seit mehreren Sessions offene Rätsel (UV-Koordinaten, NiTextureTransformController)
  erst NACHDEM der Nutzer auf NifTools-Produkte als funktionierende Referenz hinwies - siehe
  docs/MAP_FORMAT.md Abschnitte 6+7 für die vollständige Herleitung. **Bei zukünftigen
  hartnäckigen Rätseln: aktiv nach einer solchen Referenz suchen (web_search), nicht nur an den
  eigenen Dateien weiterraten.**
- Bei jedem Format-Fix: Regressionstest über ALLE 7 Test-Suiten (`tests/test_*.cpp`) VOR dem
  Verpacken. Massentest über alle `.nif`-Dateien nach jeder .nif-Struktur-Änderung.
- Ich (Claude) habe hier KEIN GLFW/ImGui/glad zum echten Kompilieren - nur ein handgeschriebener
  GL-Stub für Syntax-Checks. Echte Build-Fehler kommen erst vom Nutzer via Windows-Build.
  Deshalb: konservativ vorgehen, jede Änderung syntaktisch prüfen, aber ehrlich sagen, dass ein
  echter Build noch aussteht.
- Bei Unsicherheit lieber eine kurze, gezielte Rückfrage stellen (oder um Screenshot bitten) als
  blind zu raten und einen Build-Zyklus zu verschwenden.

## Was funktioniert (verifiziert)
- **Heightmap** (.tshm nativ, .HTD/.HTDG Legacy) - byte-exakt, inkl. Trailing-Bytes-Sonderfälle
- **Texturing** (.tstex nativ, ini+BMP Legacy) - 24-bit-RGB-BMPs, Resampling bei abweichender
  Layer-Auflösung
- **Block&Walk** (.tswalk nativ, .shbd Legacy) - Auflösung NICHT quadratisch (Breite=Quads/2,
  Höhe=Quads×8), aus Datei-Header lesbar
- **Objekt-Placement** (.tsobj nativ, .shmd/.idm/.aid Legacy) - byte-exakt
- **DDS-Texturen** (BC1/BC2/BC3) - gegen Pillow verifiziert
- **`.nif`-Parser** (`core/NifModel.hpp`): Header, NiNode-Szenengraph, Material, komplette
  Textur-Kette (NiTexturingProperty→NiSourceTexture→NiPixelData mit Mipmap-Struktur),
  Mehrfach-Mesh-Objekte, Kollisionsdaten (Box/Sphere/Capsule), Extra-Daten (String/Integer),
  Transform-, Alpha- und Materialfarb-Keyframe-Animation (Controller→Interpolator→Data-Ketten),
  `NiTextureTransformController`, `NiLODNode`/`NiRangeLODData`, die komplette
  Partikelsystem-Familie (`NiParticleSystem`, `NiPSysData`, Emitter/Modifier-Ketten),
  `NiTextureEffect`, `NiDirectionalLight`/`NiAmbientLight`/`NiLight`, `NiSpecularProperty`,
  `NiPathInterpolator`, geskinnte Meshes (`NiSkinInstance`/`NiSkinData`/`NiSkinPartition`,
  ohne Animation - nur Bindungspose), Dreiecksgeometrie sowohl als Streifen (NiTriStrips) als
  auch als flache Liste (NiTriShape), sowohl eingebettete als auch rein externe Texturen,
  `NiLookAtInterpolator`, UND korrekte UV-Koordinaten. **Massentest: 2630 von 3436 echten
  Dateien ladbar (76,4%)** (Stand 18.09.2026, von Claude per selbst gebautem Massentest
  bestätigt).
- **Eingebettete NiPixelData-Texturen werden jetzt dekodiert und gerendert** (v0.44.8, siehe
  "Diese Chat-Sitzung" oben) - 4935/4935 Dekodier-Versuche im Massentest erfolgreich, außer
  Pixelformat 3 (unkomprimiert, ~12 Dateien, noch nicht unterstützt).
- **3D-Rendering**: Terrain mit echter Multi-Layer-Textur (bis 8 Layer), Objekt-Marker
  (Platzhalter-Pyramiden, GPU-instanced), echte `.nif`-Meshes (`NifMeshRenderer`) - Objekt-
  Texturierung sollte jetzt mit korrekten UV-Koordinaten UND eingebetteten/externen Texturen
  funktionieren, aber NOCH NICHT in einem echten Build visuell verifiziert (nächster
  naheliegender Schritt).
- **Asset-Picker (Textur-/NIF-Auswahl) zeigen jetzt Vorschaubilder, Textur-Layer-Liste zeigt
  Icons** (v0.44.9, siehe "Diese Chat-Sitzung" oben) - NUR syntaktisch geprüft, noch nicht in
  einem echten Build visuell verifiziert.
- **2D-Editor**: zeigt echte texturierte Draufsicht statt Graustufen (Heightmap/Texturing/
  Objekt-Placement), Block&Walk als halbtransparentes Rot-Overlay
- Windows-Build funktioniert (mehrere reale Fixes durch den Nutzer bestätigt: NOMINMAX,
  Ordner-Dialog, etc. - siehe CHANGELOG ab v0.7.2)

## Diese Chat-Sitzung (18.09.2026) - Fortsetzung: Theme, DockSpace, SHN-Abhängigkeiten
0. **NIF-Nachtrag (wichtige Methodik-Korrektur):** die "163 neue, fixbare Fälle"-Schätzung aus
   der vorherigen Zusammenfassung war zu optimistisch. Tiefe Byte-für-Byte-Verifikation von
   `rou_waterwell.nif` (NiNode-Cluster) zeigte: obwohl ALLE Felder bis 25 Blöcke zurück exakt
   mit der offiziellen nif.xml übereinstimmen, geht der eigentliche Fehler vermutlich auf ein
   `NiTriStripsData` GANZ am Anfang der Kette zurück (tabuisiert). Eine Vollversion der
   Rückverfolgung (nicht nur letzte 3 Blöcke) zeigt: 799 von 806 verbleibenden "Block-N-EOF"-
   Fehlern haben IRGENDWO in der Datei ein `NiTriStrips`/`NiMaterialProperty`-Vorkommen - das
   ist aber auch zu grob (fast jede Mesh-Datei hat sowas). Echte Ursache von Koinzidenz zu
   trennen braucht ein Werkzeug, das blockweise Plausibilität prüft, nicht nur Ja/Nein-Suche.
   **Für die nächste Sitzung:** kein Fix ohne diese Art tiefer Verifikation (~20 Min/Datei)
   versuchen - der `NiPalette`-Fix aus [0.44.11] war ein sauberer Treffer, aber eher Glück
   (lokale Drift) als Regel.
3. **v0.44.12**: Theme auf Schwarz/Blau/Grau/Weiß (Projekt-Hub-Kacheln, Objekt-Marker 2D+3D).
   Map-Editor-Arbeitsfläche (Werkzeuge/2D/3D) läuft jetzt über ein echtes ImGui-DockSpace -
   skalierbar UND per Tab verschiebbar/anordenbar. Äußerer App-Rahmen bewusst unverändert.
4. **v0.44.13 + v0.44.14**: SHN-Abhängigkeits-Analyse gegen alle 350 NA2016-Dateien (siehe
   docs/SHN_DEPENDENCIES.md für die vollständige Liste) - Namensstamm-Erkennung
   (`FindNameStemPeers`) deutlich zuverlässiger als reine Zeilenanzahl. Grün/Rot-
   Feldmarkierung MIT Propagation umgesetzt (`AddRowWithPropagation`, `cellStatus`) - neue
   Zeile in einer Datei legt automatisch passende Zeilen in geladenen Familienmitgliedern an.
   Echte XP-/Preis-Editoren (`MobInfoServer.MonEXP`/`EXPRange` bzw. `ItemInfo.BuyPrice`/
   `SellPrice`) mit Skalier-Aktion, ersetzen die alten Platzhaltertexte.
5. **Verdachtsfund, NICHT umgesetzt**: die Z-up→Y-up-Achsenumrechnung für Objekt-Placement
   (`posX=legacyX, posY=legacyZ, posZ=legacyY`, `ObjectPlacementIO.cpp`) ist ein reiner
   Achsentausch ohne Vorzeichenwechsel - mathematisch eine Spiegelung (Determinante -1), keine
   reine Rotation. Könnte die gemeldete Links/Rechts-Vertauschung bei Objekten erklären,
   eventuell dieselbe Ursache wie der offene Textur-Spiegel-Bug (siehe unten). NICHT blind
   gefixt - diese Transformation wird von Heightmap, Walk-Grid, Objekt-Placement UND NIF-
   Meshes gemeinsam genutzt, ein falscher Vorzeichen-Rat ist ohne echten Build/Screenshot
   riskant. Nächster Schritt: nach einem echten Build gezielt mit Screenshot verifizieren.
6. **v0.44.15 (WICHTIG für nächste Sitzung): NPC/Mob/Händler jetzt auf echten Daten.**
   `Server/9Data/Shine` enthält eine dritte Datenformat-Familie (Klartext `#Table`/`#record`,
   TAB-getrennt trotz anderslautender Selbstbeschreibung) - neuer Parser `ShineText.hpp/cpp`.
   `World/NPC.txt` = echte NPC-Platzierung (527 Einträge), `MobRegen/<Karte>.txt` = echte
   Mob-Spawn-Zonen, `NPCItemList/<NPC>.txt` = echtes Händler-Inventar. `EditMode::Npcs`/`Mobs`
   (vorher reine Platzhalter) sind jetzt funktionsfähig: Liste, Bearbeiten, 2D-Marker (nur
   NPCs, quadratisch), Klick-Auswahl, Speichern. Kartenname zur Filterung = `legacySaveStem`.
   **Noch offen:** Dialog-Editor für `NpcDialogData.shn`s `[BUTTON_NPC]=[Label][Aktion]`-Skript
   (Format schon verstanden, UI noch nicht gebaut), 3D-NPC-Marker, Mob-Zonen-Marker im 2D-View.
   Ein paar sehr breite Text-Tabellen (`ItemDropTable.txt` 270+ Spalten, `Quest.txt`) haben noch
   Parser-Kanten - für Quest-/Loot-Editor in Zukunft relevant, für NPC/Mob/Händler nicht nötig.
7. **v0.44.16: Dialog-Editor fertig - damit sind alle 4 ursprünglich angefragten Teile
   umgesetzt** (NPC-Platzierung/Mob-Spawn/Händler/Dialog). "Dialog bearbeiten"-Knopf im
   NPC-Tab, Begrüßungstext + Button-Liste (Label+Aktion) editierbar, baut auf der schon
   vorhandenen `ShnFile`-Infrastruktur auf (Typ-26-Variable-Länge-String war schon
   unterstützt). Byte-exakt gegen alle 223 echten Dialog-Einträge geprüft, dabei einen
   Randfall gefunden: manche Buttons haben nur eine Klammer (nur Aktion, kein Label, z.B.
   `RouGaianMaria`) - jetzt korrekt behandelt.
8. **v0.44.17: NPCs vollständig in 3D + Mob-Zonen-Icon/Radius im 2D-View.** Eigener zweiter
   `NifMeshRenderer` (`npcMeshRenderer`). Auflösungskette: `NPC.txt`-Name →
   `MobViewInfo.shn.InxName` → `FileName` = `.nif`-Dateiname, gesucht in
   `state.availableNifFiles`. **Wichtiger Fund:** `MobViewInfo.shn.NpcViewIndex` - bei `!=0`
   ist der NPC aus Ausrüstungsteilen zusammengesetzt (`NPCViewInfo.shn`, wie ein
   Spielercharakter) - NICHT unterstützt, bleibt bei 2D-Markierung. Gegen Karte "RouN" (29
   NPCs) geprüft: 29/29 haben `NpcViewIndex==0`, der Ausrüstungsfall scheint bei benannten
   NPCs die Ausnahme. Mob-Zonen: Icon + Radius-Kreis, Radius = `RangeDegree` direkt (wenn
   Width/Height=0, häufigster Fall - trotz Namens KEIN Winkel) sonst halbe längere Seite.
   **Bitte nach dem nächsten Build besonders sorgfältig visuell prüfen** (Position/Ausrichtung
   der NPC-Modelle, Performance bei vielen NPCs) - GL-Rendering war in der Sandbox nicht
   visuell testbar, nur die Auflösungslogik selbst (29/29 gegen echte Daten verifiziert).
9. **v0.44.18 (KORREKTUR zu v0.44.17, WICHTIG): Charaktermodelle liegen in `Client/reschar/
   <Name>/<Name>.nif`, NICHT in `resmap/nif(s)`.** Vom Nutzer per Screenshot des echten
   Client-Ordners aufgedeckt - `state.availableNifFiles` (aus [0.44.17] genutzt) deckt
   `reschar` gar nicht ab, wäre also für Charaktere still leergelaufen. Jetzt: `reschar/<Name>/
   <Name>.nif` zuerst probiert, Fallback auf `resmap/nif(s)` für Nicht-Charakter-"NPCs" (Tore,
   Ambosse). Auch geprüft: NIF-Parser überspringt Skelett-/Skinning-Daten byte-korrekt, wendet
   sie aber nicht an - Charaktere rendern in ihrer Ruhepose ohne `.kf`-Animation (akzeptabel
   für eine Platzierungs-Vorschau, volle Animation wäre ein eigenes größeres Vorhaben).
10. **v0.44.19: Quest-Editor + Gate/Portal-Bestandsaufnahme.** Nutzer stellte einen
    vollständigen, selbst verifizierten Python-Referenzparser für `QuestData.shn` bereit
    (drittes eigenständiges Format, uint16-Header 0x0006 + variable Records) - nach C++
    portiert, byte-identisches Round-Trip bestätigt, neue 9. Test-Suite `test_questdata`.
    `QuestDialog.shn` (25222 Zeilen) löst die Zahlen-Text-IDs zu lesbarem Text auf - Quest-
    Editor-UI im bisherigen Platzhalter-Tab gebaut (Liste, Level/NPC/Ziele/Drops/Skripte).
    **Gates/Portale geklärt:** `TownPortal.shn` (8 Zeilen) = Schriftrollen-Schnellreise,
    "normale Portale" = die schon geladenen `Role=Gate`-Einträge in `NPC.txt`, laufen
    automatisch über dieselbe NPC-3D-Pipeline. Von 24 eindeutigen Gate-Namen im ganzen Spiel
    hat die LOKALE (nur teilweise vorhandene) Nif-Bibliothek 8 Modelle, **16 fehlen** (u.a.
    `LevelGuard00`-`LevelGuardH7` - vermutlich Level-Sperren, keine echten Portale;
    `KarenGate`, `ID_LinkGate02`, `EldSpeGuard01`) - Nutzer hat angeboten, fehlende .nif-
    Dateien in seinem echten Client zu suchen, sobald ihm die Namen genannt werden.
11. **v0.44.20: Portal-Taxonomie vom Nutzer klargestellt, 5 Kategorien:** 1. TownPortal
    (Skill, auswählbares Ziel), 2. normale Portale (`NPC.txt` Role=Gate, Map-A→B), 3. NPC-
    förmige Teleporter (dieselbe Gate-Mechanik, nur Wächter-Optik statt Torbogen), 4.
    Schriftrollen mit festem Ziel (`RecallCoord.txt`) bzw. Items mit wählbarem Ziel
    (vermutlich TownPortal-Mechanik, nicht verifiziert), 5. Instanz-Tore - beim Durchsuchen
    ALLER `NPC.txt`-Rollen gefunden: eigene Rollen `IDGate`/`ModeIDGate` (Ziele wie `WarBL01`/
    `IDGate01`). Auch gefunden: `RandomGate` (→ Gilden-Häuser), `NPCMenu`/`ClientMenu` (reine
    Menü-NPCs, keine Teleporter - Gilde, Münz-Automat). Rollen 2/3/5 liefen schon automatisch
    über die bestehende NPC-3D-Pipeline mit. Neuer 6. SHN-Editor-Tab "Portale" für die zwei
    bisher fehlenden eigenständigen Tabellen: `TownPortal.shn` + `RecallCoord.txt`, beide klein
    genug für direkte Tabellen-Bearbeitung.
12. **v0.44.21: Nutzer nannte 7 neue wichtige offene Punkte. Stand + Roadmap für die nächste
    Sitzung:**
    1. **SHN-Validierung** (keine falschen Änderungen/Abstürze): TEILWEISE - Item-/Mob-ID-
       Referenzen im Quest-Editor zeigen jetzt grün/rot (siehe unten). Eine umfassendere,
       eigenständige "Validieren"-Funktion (z.B. vor dem Speichern, projektweit) fehlt noch.
    2. **Quest-Editor: Belohnung + Item-ID mit echtem Namen** TEILWEISE - Item-/Mob-Namen
       jetzt aufgelöst (Start-NPC, Ziele, Drops, benötigtes Item, Vorgänger-Quest, alle grün/
       rot nach Gültigkeit). Die 144-Byte-Belohnungsstruktur selbst bleibt ungeklärt - ein
       Abgleich gegen `CREATE_ITEM`-Aufrufe in Quest-Skripten ergab kein Muster, weitere
       Versuche bräuchten einen systematischeren Ansatz (z.B. viele Quests mit extern bekannten
       Belohnungen gegenprüfen).
    3. **TownPortal im Map-Editor platzieren/anzeigen/anpassen** NOCH OFFEN. Zu klären: sind
       `TownPortal.shn`s X/Y das ZIEL (kein Ort zum Platzieren) oder gibt es doch einen
       Aufstellungsort? Eventuell eher eine reine Ziel-Auswahl-UI als eine Karten-Platzierung.
    4. **NPC-KI** NOCH OFFEN, aber Datengrundlage gefunden: `MobRoam/<Name>.txt`
       (Patrouillenroute: Koordinatenliste + `return`-Marker, ShineText-Format, lädt schon mit
       dem bestehenden Parser). Naheliegend: "Route bearbeiten"-Knopf im NPC-Tab analog zu
       "Dialog bearbeiten", mit Wegpunkten als Liste + 2D-Marker/Linie.
    5. **MapWayPoint für Auto-Move** NOCH OFFEN. `MapWayPoint.shn` (14886 Zeilen: `MapID`, zwei
       unbenannte Zahlenfelder, `MWP_Gate`) gefunden, `MapID`→Kartenname über `MapInfo.shn`
       (`ID`/`MapFolderName`/`RegenX`/`RegenY`) - Bedeutung der unbenannten Felder und
       `MWP_Gate`-Werte noch NICHT verifiziert, vor jeder UI erst byte-genau klären.
    6. **Custom NPCs/Mobs anlegen** NOCH OFFEN, aber technisch machbar: `AddRowWithPropagation`
       (SHN-Editor) ist bereits das Vorbild für einen koordinierten Multi-Datei-"Neu anlegen"-
       Workflow - für NPCs bräuchte es einen neuen Eintrag in `World/NPC.txt` PLUS
       `MobViewInfo.shn` (fürs Modell), für Mobs in `MobCoordinate.shn`/`MobRegen` PLUS
       `MobInfo.shn`/`MobViewInfo.shn`.
    7. **Mob-KI** NOCH OFFEN, Datengrundlage gefunden: `MobAttackSequence/<Boss>.txt`
       (nummerierte Angriffs-Skill-Sequenzen, nur für Bosse gefunden) und `MobSetting/
       Action/<Name>.txt` (Trigger-Bedingung→Aktion, auch für Gate-Objekte genutzt) - beide
       laden schon mit dem bestehenden ShineText-Parser, noch keine UI.
13. **v0.44.22 (WICHTIG für Punkte 4+7): Nutzer wies auf Lua UND PineScript hin - beides
    ECHTE, vollständige Skriptsprachen für Mob-/NPC-KI, nicht nur Tabellen.**
    `LuaScript/AIScript/<Name>.lua` (404 Dateien, echter Lua-Code) + `MobBehaviorDescript/
    **/*.ps` (54 Dateien, eigene Zustandsmaschinen-Sprache: `open`/`close`/`if`/`chat`/
    `whoistarget`/`permillage`). `World/PineScript.txt` = Register aller beim Serverstart
    geladenen `.ps`-Dateien (ShineText, lädt schon). Bewusst KEIN Lua-Interpreter/PineScript-
    Parser gebaut - reiner Text-Editor (`DrawAiScriptEditorPopup`, byte-exaktes Laden/
    Speichern, keine Syntaxprüfung). Namensauflösung gegen echte Daten verifiziert ("Chimera"
    existiert als Mob-Name UND als `Chimera.lua`). Neue Buttons: "KI-Skript (Lua) bearbeiten"
    im NPC-Tab, jedes Monster in einer Mob-Spawn-Zone ist jetzt anklickbar (öffnet dessen
    Lua-Skript, falls vorhanden - die meisten normalen Feldmonster haben keins, nur Bosse,
    das ist normal). **Für die nächste Sitzung bei Punkt 4/7 relevant:** PineScript-Editor
    fehlt noch (nur Lua ist verknüpft) - `OpenPineScriptEditor` existiert schon als Funktion,
    aber noch keine UI-Anbindung (fehlt: wie findet man den richtigen PineScript-Pfad zu einem
    Mob/einer Zone - vermutlich über `World/PineScript.txt`s Registry plus Kontext, welche KQ/
    Instanz gerade bearbeitet wird, noch nicht geklärt).
14. **v0.44.23: Objekte im Block/Walk-2D-View sichtbar (Nutzerwunsch, "nicht durch Häuser
    laufen") + Patrouillenrouten-Editor.** Objekte jetzt auch im BlockWalk-Modus als dezente
    Referenz-Punkte sichtbar (nicht anklickbar dort, reine Orientierung). Bekannte Grenze:
    nur Position als Punkt, keine echte Gebäude-Grundfläche (NIF-Bounding-Box wird noch nicht
    an die 2D-UI durchgereicht - wäre der nächste Schritt für noch genaueres Blocken).
    Patrouillenrouten-Editor (`MobRoam/<Name>.txt`) fertig: Buttons im NPC-Tab und pro Monster
    in der Mob-Spawn-Zonen-Liste, Wegpunkt-Liste bearbeitbar (X/Y/Event, hinzufügen/entfernen).
    Byte-exakt gegen `BerValeDw04.txt` (9 Wegpunkte) verifiziert. **Noch offen:** die Route
    NICHT als Linie im 2D-View gezeichnet (nur die Liste im Popup) - naheliegende Ergänzung.
15. **v0.44.24: Echte Objekt-Grundfläche im 2D-View** (löst die in Punkt 14 genannte Grenze).
    `GetOrComputeFootprint`/`DrawObjectFootprint2D`: Bounding-Box aus den echten NIF-Vertex-
    Positionen, Y-rotiertes/skaliertes Rechteck an der Weltposition, sichtbar in Objekt- UND
    Block/Walk-Modus. Gegen `tree05.nif` (plausible ~985×991 Einheiten) und ein bekannt
    defektes NIF (liefert sauber `valid=false`, kein Absturz) verifiziert.

16. **v0.44.25: TownPortal geklärt + Portale-Tab, zwei Datenverlust-/Exaktheits-Bugs behoben.**
    Siehe CHANGELOG [0.44.25] für alle Details. Kernaussagen: (a) `TownPortal.shn` X/Y = Ort auf
    der Karte (Weltkoordinaten), liegt nahe am `Gate_Town`-NPC derselben Karte in `World/NPC.txt`
    - neuer Tab "Portale" zeigt/bearbeitet TownPortal + Schriftrollen (`RecallCoord.txt`) im
    2D-View. (b) **SHN-Loader hat 1-Zeichen-Spaltennamen (`X`, `Y`) zu "Undefined N"
    umbenannt und so gespeichert** - behoben, SHN-Round-Trip 348/348 (vorher 299/348).
    `MapWayPoint.shn`-Spalten sind `MapID`/`X`/`Y`/`MWP_Gate`. (c) `SaveShineTextFile` schrieb
    alles als CRLF und verwarf Auffüll-Tabs - jetzt 677/677 Textdateien byte-identisch beim
    unveränderten Speichern. (d) Drei Windows-Compile-Fehler (`p->string()`) und ein
    Heightmap-Pinsel-Leck in Tabs ohne Klick-Logik behoben.
    **Methodik-Nachtrag:** für Syntaxprüfung von `main.cpp` echte Header holen:
    `codeload.github.com` (ImGui `docking`-Branch, GLFW 3.4) und `pip install glad2` +
    `python3 -m glad --api gl:core=3.3 --out-path X c` (glad2 erzeugt `glad/gl.h`; für
    `#include <glad/glad.h>` einen Shim mit `GLADloadproc`/`gladLoadGLLoader` anlegen). Portal-
    Logik ist ohne GL testbar: `main.cpp` mit `-Dmain=xyz` in eine Test-Datei includen, gegen
    die core-Objekte + `Camera/Renderer/ObjectMarker/NifMesh`-Objekte linken
    (`-Wl,--unresolved-symbols=ignore-all`) und die freien Funktionen direkt aufrufen.
    **v0.44.26:** erster echter MSVC-Build kompilierte fehlerfrei, scheiterte aber am Linken -
    `QuestData.cpp` fehlte in `CMakeLists.txt` (seit 0.44.19). Behoben; Zip heißt jetzt v0_44_26.
    Lehre: nach jeder neuen `.cpp` die CMake-Liste prüfen (Skript: für jede `src/**/*.cpp`
    `grep` in `CMakeLists.txt`).

17. **v0.44.27: gemeinsame Spiegel-Ursache, NIF-Texturen, Pfade, Mob-Spawns.** Details in CHANGELOG
    [0.44.27]. Wichtigste Erkenntnisse für die nächste Sitzung: (a) Die Legacy->Editor-Achsen-
    vertauschung (x,y,z)->(x,z,y) ist eine Spiegelung; sie erklärt "Textur oben/unten" UND "Objekte
    links/rechts". Korrigiert über BMP-Rohreihenfolge, Quaternion-Konjugation und Ansicht (3D-
    Kamera spiegelt Z, 2D Norden oben). Nur die ANNAHME "im Spiel +X rechts, +Y oben" ist offen -
    bitte mit einer bekannten Stelle im Spiel abgleichen; falls falsch, ist es EIN Vorzeichen in
    `OrbitCamera::ViewMatrix` (Z- vs. X-Spiegelung) plus die 2D-Formel v = 1 - z/spanZ.
    (b) Eingebettete NiPixelData: "Num Faces" (4 Byte) vor den Pixeln + "Bytes Per Pixel" 3/4 =
    Rohpixel. (c) `SaveShineTextFile` konnte löschen nicht und verschob Zeilen bei Einfügen.
    **Methodik-Nachtrag (sehr nützlich):** Orientierungsfragen NICHT per Screenshot raten, sondern
    an echten Daten prüfen: Objekt-Höhen vs. Heightmap unter 8 Achsenabbildungen, Blend-Layer-
    Korrelation mit Höhe, Serverpunkte (NPC/MapWayPoint) auf Walk-Bits. Werkzeuge liegen nicht im
    Repo, sind aber kurz (C++ gegen `OpenLegacyMap`, Python/PIL/numpy für Bilder). Textur-
    Dekodierfehler: Rohdaten mit PIL (DDS-Header selbst bauen) gegenprüfen.

18. **v0.44.28: SHN-Bearbeiten, große Karten, NIF + Höhen, Shops, Custom NPC/Mob.** Details CHANGELOG
    [0.44.28]. Für die nächste Sitzung wichtig: (a) `NifModel.cpp` lädt jetzt in `LoadNifMeshData` (Kern)
    + `LoadNifMesh` (Varianten-Suche: Material-Maske, Pixel-Trailer-Variante, Positions-Verschiebungen);
    Szenengraph-Transformationen werden NACH dem Parsen angewendet (Header-Parser unverändert!).
    (b) Layer-Regionen: `TextureLayer::region*`, Shader `uLayerRegion`, Renderer mehrpassig (24 Layer).
    (c) Neue Shop-/Assistent-Funktionen sitzen in `main.cpp` (`DrawShopEditorPopup`,
    `DrawCustomCreatureEditor`, `RunCreateCreature`). Server-MobViewInfo/ItemViewInfo liegen unter
    `Server/9Data/Shine/View/` - `FindOrLoadShnDoc` sucht rekursiv. (d) Datenmodell NPC/Mob: gleiche ID
    in MobInfo(C+S)/MobInfoServer/MobViewInfo(C+S)/MobSpecies/QuestSpecies/MobWeapon; Avatar-NPCs über
    `NPCViewInfo` (Klasse 0-5, Gender 1=männl. ANNAHME nach ChrCreateEquip.IsMale).
    **Offen:** 3D-Vorschau Avatar+Rüstung (Körpermodelle unbekannt, Item-NIFs `resitem/`, Rüstungen in
    `resitem/male|Female/`), ob das Spiel Custom-Einträge akzeptiert, verbleibende ~5 % NIFs, MobRegen
    per `#recordin` (Server?), Objekt-Höhen sind nur statistisch geprüft (Objekte auf Objekten).

19. **v0.44.29: UI-Fehler, Sichtbarkeit/Kategorien, Block&Walk auf Zell-Ebene, mehr Objekte.** Details
    CHANGELOG [0.44.29]. **Sehr nützliche Methodik:** ImGui-Oberflächen lassen sich OHNE GPU ansehen - einen
    Software-Rasterizer für `ImDrawData` (Dreiecke mit Vertexfarbe + Font-Atlas, ~40 Zeilen) in ein
    Headless-Harness einbauen (`main.cpp` mit `-Dmain=xyz` includen, Kontext + Font aufbauen, Frames
    treiben, PNG speichern). Damit wurden Quest-Editor, Assistent und Shop-Editor gesehen und repariert.
    Klicks lassen sich per `io.AddMousePosEvent`/`AddMouseButtonEvent` simulieren; Popup-Position über
    `GetCurrentContext()->OpenPopupStack.back().Window`. **Merkregeln:** (a) `OpenPopup` und `BeginPopup`
    auf DERSELBEN ID-Ebene; (b) Popups (`BeginPopup`) sind AlwaysAutoResize - keine relativen Kindgrößen;
    (c) `InputText` mit `EnterReturnsTrue` schreibt den Text nicht in den Zustand - nach dem Aufruf immer
    kopieren; (d) `SameLine()` nur direkt vor dem Widget, das wirklich folgt.
    **Block&Walk:** Wort = 16 Zellen (LSB-zuerst), Bit gesetzt = blockiert, Zelle = 6.25 Einheiten, Gitter
    quadratisch (Kantenlänge = längere Kartenseite) - siehe MAP_FORMAT Abschnitt 55.
    **Offen:** Sichtbarkeit/Kategorien nicht im echten Build gesehen; Kategorien sind Schlüsselwort-Heuristik
    (Modellname) - ggf. erweitern (`ClassifyObjectModel`); animierte Item-NIFs; Rou_M_Banner/wool2.

20. **v0.44.30: Charakter-Vorschau (reschar) + NIF-Neuaufbau.** Details CHANGELOG [0.44.30]. Merkregeln:
    (a) NIF-UVs zählen von OBEN, `LoadDdsImage` liefert unten zuerst -> Zeile = (1-v)*Höhe. (b) Gesicht/Haare der
    Charakter-NIFs sind schon im Weltrahmen modelliert - NICHT mit der Knochenrotation drehen. (c) Bei jeder
    Änderung am NIF-Parser IMMER `loadset`-Vergleich gegen den Vorgängerstand über ALLE Sätze (Feld, Items, reschar,
    Fixtures): geladen, verloren, Dreieckszahl, Konsistenz (Index < Vertices). (d) Parts entstehen jetzt aus den
    Geometrie-Knoten, wenn das alte Material-getriebene Verfahren inkonsistent ist. **Offen:** Archer-Modelle fehlen
    (kein Ordner), Zusatzteile/Kopfbedeckung/Waffenposition der Vorschau, GL-Anzeige ungesehen, 7 weitere reschar-
    Ordner nur teilweise geprüft (Mage, Joker, Sentinel, Cleric-m fehlten im Test), Haarfarbe/-textur je Klasse.

21. **v0.44.31: NIF-Abdeckung aller Karten-Modelle.** Details CHANGELOG [0.44.31]. **Methodik (wichtig, mehrfach
    fast schiefgegangen):** (a) IMMER gegen ALLE NIFs vergleichen (`resmap_all/`: alle `resmap*.zip` mit
    `unzip -o "*.nif"`), nicht gegen eine Stichprobe. (b) Vergleich pro Datei: `loadone <nif>` -> "geladen Indizes Vertices",
    Läufe mit `xargs -P` und `timeout` in EIGENEM Hintergrundprozess (`setsid nohup ... &`) - lange Läufe im Vordergrund
    brechen das Werkzeug-Limit (300 s), und ein `pkill -f <name>` tötet die eigene Shell. Kennzahlen: geladen, verloren (MUSS 0
    sein), andere Dreieckszahl, Laufzeit. (c) Änderungen an der Header-Logik von `ParseNiTriStripsHeader` haben schon
    zweimal Hunderte Dateien verloren - jede Änderung sofort im Vollvergleich prüfen. (d) Die Such-Stufen (Varianten,
    Verschiebungen, Namens-Resync, Teilmodell) laufen NUR nach einem gescheiterten Standardlauf.
    **Offen:** siehe CHANGELOG (NiNode/NiBillboardNode/NiTriStrips-Reste, blocktypspezifisch), Adl 24 / Cypian 89 Objekte mit
    fehlenden Dateien (`AiDn01_plant00.nif` heißt in den Zips evtl. `AlDn01...` - Namensabgleich prüfen).

22. **v0.44.32: Kamera, 2D-Zoom, NPCs im 3D-Editor.** Details CHANGELOG [0.44.32]. Merkregeln: (a) 2D-Ansicht: `imageSize`/
    `cursorScreenPos` sind das VIRTUELLE gezoomte Bild - neue Overlays einfach weiter mit `cursorScreenPos + u*imageSize`
    zeichnen; ImGui-Elemente, die "letztes Item" ändern, erst NACH der Hover-Auswertung (`hoveredView`). (b) Die Kamera
    rechnet im gespiegelten Anzeigeraum (Z -> -Z), Ziel-Z über `SetTarget`/`TargetZ()` in Weltkoordinaten. (c) NPC-Rendering:
    `CollectNpcRenderData` (GL-frei, testbar) + `EnsureNpcModelsLoaded` (lädt in den Renderer); Position/Richtung ändern nur
    über `RefreshNpcTransforms`. **Offen:** NPC-Blickrichtung (Drehsinn/Versatz) am echten Spiel verifizieren, Modellgröße
    der NPCs, Auswahl von NPCs per Klick im 3D-View, Kamera-Feintuning im echten Fenster.

23. **v0.44.33: Handbuch, Tooltips, Skill-Editor.** Details CHANGELOG [0.44.33]. Merkregeln: (a) Neue Bedienelemente IMMER mit `UI::...`
    statt `ImGui::...` (Tooltips) und den Tooltip in `tools/manual/tooltips.py` eintragen, dann `python3 tools/manual/gen.py`;
    `test_manual <main.cpp>` erzwingt das. (b) Handbuchtexte/Spaltenbeschreibungen: nur Belegtes als sicher, Abgeleitetes "(vermutet)".
    (c) Skill-Editor: `skilled::`-Feldlisten in main.cpp sind die einzige Quelle der Skill-Beschreibungen - der Generator übernimmt sie auch in die
    Spalten-Referenz. (d) `L(de, en)` liefert Text je Sprache (für Texte ohne T()-Schlüssel). **Offen:** Bedeutung der (vermutet)-Spalten
    belegen, Skill im echten Spiel testen, weitere Spaltenbeschreibungen (nur 195 von mehreren Tausend), Handbuch-Inhalt vom Nutzer prüfen lassen.

24. **v0.44.34: NPC-Ausrichtung (Vorzeichen korrigiert), Assert-Absturz (NaN in Heightmap), weitere NIFs.**
    Details CHANGELOG [0.44.34]. Merkregeln: (a) Für "wie sollte X aussehen"-Fragen ohne Ground-Truth-Doku: nach
    überprüfbaren INDIREKTEN Signalen in den eigenen Daten suchen (hier: Block&Walk "NPC steht mit dem Rücken zur
    Wand") statt zu raten - Methodik in `EstimateNpcOrientation`/CHANGELOG [0.44.34] als Vorlage. (b) `assert()`-
    Ausfälle, die nur "manchmal am Mauszeiger" auftreten, sind fast immer NaN/UB aus einer stillen 0/0-Division
    irgendwo im Aufrufpfad - IMMER die Kette bis zur Eingabequelle zurückverfolgen (hier: `.ini`-Parser lieferte 0.0
    bei Parse-Fehlern), nicht nur den Assert-Ort selbst absichern. (c) NIF-Blocktyp-Strukturen: docs.rs der Rust-
    "nif"-Crate (binrw, von nif.xml generiert) ist eine gute, schnell durchsuchbare Zweitquelle neben nif.xml/pyffi-
    Doku. **Offen:** ~191 Karten-NIFs (siehe CHANGELOG für Gruppen/Beispiele), NPC-Ausrichtung und Assert-Fix am
    echten Spiel/Fenster verifizieren.

25. **v0.44.35: NIF-Untersuchung fortgesetzt (auf Nutzerbitte "löse die letzten 191").** Details CHANGELOG [0.44.35].
    Ein reeller, referenzbelegter Fix (Tangentenraum-Bitmaske), aber neutral auf dem aktuellen Testkorpus - 184 Dateien
    bleiben offen trotz gezielter Prüfung von 5 weiteren Blocktyp-Gruppen gegen die "nif" Rust-Crate (docs.rs, binrw,
    direkt aus nif.xml generiert - guter, schnell durchsuchbarer Zweitabgleich neben pyffi-Doku). Wichtigste Lektion
    dieser Runde: mehrere naheliegende Verdächtige (NiPSysBoxEmitter-Kette, NiCollisionData, NiAvObject) stimmten bei
    genauer Prüfung BEREITS mit der Referenz überein - die verbleibenden Fehlschläge sitzen tiefer (vermutlich in
    `ParseObjectNetBase`s Peek-Heuristik oder in `NiTexturingProperty`, beide seit mehreren Sitzungen mehrfach revidiert
    und deshalb bewusst NICHT ohne vollständige Korpus-Verifikation angefasst - das Risiko für die bestehenden 94.6%
    wiegt schwerer als der mögliche Gewinn bei einer Einzelfall-Änderung ohne Beleg). **Für die nächste Sitzung:** falls
    weiter an NiPixelData/NiSourceTexture-Kette gearbeitet wird (15 Dateien, größte einzelne verbleibende Gruppe mit
    klarem gemeinsamen Vorgänger), zuerst `NiTexturingProperty`s "Num Shader Textures"-Bedingung (aktuell: immer lesen
    ab Version 10.0.1.0) gegen ALLE 15 Dateien einzeln durchgehen, nicht nur eine Stichprobe - diese Bedingung wurde in
    früheren Sitzungen bereits zweimal widersprüchlich revidiert (santuary.nif vs. Eff_2.nif).

## ⭐ START HIER — konsolidierte Roadmap für die nächste Sitzung (Stand 22.09.2026, v0.44.35)

Die Nutzer-Anfragen der letzten Runden in Kurzform, nach Priorität grob geordnet (Details in
den nummerierten Einträgen oben nachschlagen, insb. 12/13/14/15):

0. **ZUERST im echten Build prüfen (v0.44.28, nichts davon gerendert gesehen):** Adl-Textur (Regionen, 10 Layer), Objekte auf dem Gelände (Szenengraph), Grasobjekte sichtbar, Shop-Editor (Item-Auswahl), Custom-NPC-Assistent, SHN-Zelle mehrstellig + neue-Zeile-ID.
   Danach (Stand v0.44.27): Textur passt jetzt
   zu Objekten/Gelände? Norden oben in 2D, 3D nicht spiegelverkehrt (Annahme siehe Punkt 17)?
   Objekt-Texturen bunt? Portale im 3D, Grundflächen-Hüllen im Block/Walk-Modus, Mob-Zonen bearbeiten,
   SHN-Zellen per Doppelklick/Knopf, Quest-Editor lädt. Offen bleibt: Custom Mobs/NPCs (= Spieler-
   Modell + Rüstung, braucht Spezifikation), Server-Verhalten bei per `#recordin` eingefügten MobRegen-
   Zeilen, 3 verbleibend verrauschte NIF-Texturen (unbekanntes Format), NPC-Platzhalter-Drehung.
1. **TownPortal im Map-Editor** - ERLEDIGT in v0.44.25 (Tab "Portale"), aber NOCH NICHT in
   einem echten Build visuell verifiziert (Raute/Dreieck/Quadrat/Ring im 2D-View, Klick-Auswahl,
   "Position per Klick setzen"). Offen: 3D-Marker, TownPortal-Einträge löschen, ob der Client
   für NEU angelegte TownPortal-Ziele weitere Daten braucht (Menütext o.ä.) - vor Nutzung im
   Spiel prüfen.
2. **MapWayPoint für Auto-Move** - `MapWayPoint.shn` (14886 Zeilen) hat die Spalten `MapID`, `X`,
   `Y`, `MWP_Gate` (die "unbenannten Felder" waren der SHN-Namensbug aus v0.44.25, jetzt behoben).
   `MapID`→Kartenname über `MapInfo.shn`. Vermutlich Weltkoordinaten wie bei TownPortal - vor
   jeder UI gegen NPC.txt/Gates prüfen; Bedeutung der `MWP_Gate`-Werte (1/2) NICHT verifiziert.
3. **Custom NPCs/Mobs anlegen** - technisch machbar (siehe `AddRowWithPropagation`-Vorbild),
   noch kein koordinierter "Neu anlegen"-Workflow (NPC: `World/NPC.txt` + `MobViewInfo.shn`;
   Mob: `MobCoordinate.shn`/`MobRegen` + `MobInfo.shn`/`MobViewInfo.shn`).
4. **PineScript-Editor-Anbindung** - `OpenPineScriptEditor` existiert schon, aber ungenutzt.
   Fehlt: wie kommt man vom Mob/der Zone zum richtigen `.ps`-Pfad? Vermutlich über `World/
   PineScript.txt`s Registry (ShineText, lädt schon) plus Kontext, welche KQ/Instanz gerade
   bearbeitet wird - noch nicht geklärt.
5. **MobAttackSequence/MobSetting-Action-Anbindung** - beide Formate laden schon mit dem
   ShineText-Parser (siehe Punkt 13 oben), noch keine UI/Verknüpfung zu Mobs gebaut.
6. **Patrouillenroute als Linie im 2D-View zeichnen** (naheliegende Ergänzung zu Punkt 14).
7. **SHN-Validierung erweitern** - bisher nur Item-/Mob-ID-Auflösung im Quest-Editor (grün/rot).
   Eine eigenständige, projektweite "Validieren"-Funktion vor dem Speichern fehlt noch.
8. **Belohnungsstruktur in QuestData** (144 Byte) bleibt ungeklärt - ein Abgleich gegen
   `CREATE_ITEM`-Aufrufe in Quest-Skripten ergab kein Muster. Nicht weiter raten ohne neue
   Evidenz (z.B. eine Liste bekannter Quest-Belohnungen von außen zum Gegenprüfen).

**Für einen neuen Chat:** dieses `HANDOFF.md` (im mitgelieferten Repo-Zip) ist die
Kontinuitätsbrücke - im neuen Chat das aktuelle Zip hochladen und bitten, zuerst `HANDOFF.md`
und `docs/MAP_FORMAT.md` zu lesen (Enden zuerst, dort steht der genaueste Stand). Die
Projekt-Arbeitsweise (`ways-of-working.md`) und der Projekt-Überblick liegen bereits in
Claudes Projekt-Gedächtnis und werden automatisch geladen, MÜSSEN also nicht erneut erklärt
werden - nur der Code-/Doku-Stand selbst lebt im Zip, nicht im Gedächtnis.

## Diese Chat-Sitzung (18.09.2026) - Kurzfassung
Zwei getrennte Dinge, siehe CHANGELOG.md für Details:
1. **v0.44.8 war beim Sitzungsstart bereits vom Nutzer selbst umgesetzt** (eingebettete
   NIF-Texturen werden jetzt wirklich dekodiert + gerendert, nicht mehr nur strukturell
   übersprungen) - hier aber noch nicht dokumentiert gewesen. Von Claude diese Sitzung
   NACHTRÄGLICH verifiziert: echter g++-Build (kein `-fsyntax-only`) aller 8 Test-Suiten
   gegen echte Referenzdaten (0 Fehler) + selbst gebauter Massentest über alle 3436 echten
   `.nif`-Dateien (Skript lag nicht im Repo, war Claude-lokal aus früheren Sitzungen) -
   **2625/3436 (76,4%)**, unverändert stabil. Eingebettete Texturen: **4935 von 4935**
   Dekodier-Versuchen erfolgreich, bis auf Pixelformat 3 (unkomprimiert/palettiert, ~12
   Dateien) - noch nicht unterstützt (nur DXT1/3/5 = Format 4/5/6). Die tabuisierte
   `SkipNiPixelData` wurde dabei NICHT verändert (nur toter Code jetzt) - die neue
   `ParseNiPixelData` dupliziert ihre exakte Byte-Konsum-Logik, liest aber statt zu
   überspringen.
2. **v0.44.9 (Claude, diese Sitzung): Vorschaubilder im Asset-Picker + Layer-Icons**
   (Nutzerwunsch aus einer früheren Sitzung, siehe unten "Was NICHT funktioniert" alt).
   `DrawAssetPickerPopup` nutzt jetzt `ImGuiListClipper` (Vorbedingung für performantes
   Nachladen bei tausenden Dateien) und lädt Thumbnails NUR für sichtbare Zeilen, gecacht in
   `EditorState::assetThumbnails`. Texturen direkt über `LoadDdsImage`, `.nif`-Modelle über
   die erste Diffuse-Textur (eingebettet ODER extern über `fieldTexture` aufgelöst). Layer-
   Liste im Texturing-Panel zeigt jetzt ein 20x20-Icon pro Layer. NUR syntaktisch geprüft -
   aber diesmal gegen die ECHTEN Dear-ImGui-Docking-Header + echten GLFW-Header + einen neu
   geschriebenen `glad.h`-Stub (nicht nur ein Minimal-Stub wie zuvor), `-fsyntax-only` über
   alle `src/app/*.cpp` sauber, keine Warnungen. Bekannter Trade-off: das erste Sichtbarwerden
   eines `.nif`-Eintrags im Picker kostet einen vollen `LoadNifMesh`-Aufruf (~4ms im Schnitt)
   - bei sehr schnellem Scrollen durch viele neue Einträge ggf. ein kurzer Ruckler, danach
   dauerhaft gecacht. **Bitte nach dem nächsten Build visuell verifizieren** (Icons sichtbar,
   keine Ruckler beim Scrollen, korrekte Bilder).

## Diese Session gelöst (sechs Funde, einer davon der größte dieser Codebasis bisher)
1. **UV-Koordinaten waren komplett unbrauchbar** - ein direkt VOR den UV-Daten gelesenes
   "uv_flags"-Feld existiert dort gar nicht; gehört NACH die UV-Daten. Siehe Abschnitt 6.
2. **`NiTextureTransformController` schien inkonsistent lang** - tatsächlich IMMER fest 39
   Byte; wahre Ursache war ein übersehenes, konditionales u32-Feld in `NiTexturingProperty`.
   Siehe Abschnitt 7.
3. **Folgefehler aus Punkt 2**: `NiSourceTexture` hatte DASSELBE konditionale Muster. Siehe
   Abschnitt 8.
4. **`NiLODNode`/`NiRangeLODData` implementiert** - Letzteres weicht von der öffentlichen
   Referenz ab (kein Vector3-Center in diesem Fork). Siehe Abschnitt 9.
5. **Komplette Partikelsystem-Familie implementiert**, dabei drei eigenständige Bugs gefunden
   (konditionales `has_shader`-Byte, fehlender `KeyType`-Wert 5/CONST, `currentMeshHasTexturing`
   nie für Partikelsysteme aktualisiert). Siehe Abschnitt 10.
6. **GRÖSSTER FUND: Der konditionale 8-Byte-Trailer bei `NiTriStripsData`/`NiTriShapeData` war
   zu eng gefasst** - fehlt nicht nur vor einem zweiten Mesh-Teil, sondern vor JEDEM
   Geschwister-Block (Licht, Effekt, weiterer Node-Ast). Löste allein +382 Dateien im
   Massentest. Siehe Abschnitt 11.
7. **ZWEITER GROSSER FUND: `NiSourceTexture` bei rein externen Texturen** (`use_external=1`,
   kein eingebettetes `NiPixelData`) braucht 18 weitere Byte (`pixel_layout`, `mipmap_format`,
   `alpha_format`, `is_static`, `direct_render`, ein Abschlussfeld), die bei eingebetteten
   Texturen fehlen. Löste +164 weitere Dateien. Siehe Abschnitt 12.
8. `NiAmbientLight`, `NiSpecularProperty` (viel einfacher als vermutet: nur `NiObjectNET` +
   `flags`), `NiPathInterpolator` ergänzt.
9. `NiSkinInstance`/`NiSkinData`/`NiSkinPartition` (geskinnte Meshes) ergänzt - dabei EINEN
   weiteren, analogen Trailer-Ausnahmefall gefunden (NiSkinInstance beginnt ohne Namensfeld,
   der LooksLikeFreshName-Peek konnte ihn nicht selbst erkennen) UND einen Bug im
   Peek-Override selbst behoben (er konnte fälschlich einen expliziten Ausschluss des
   Aufrufers überstimmen). Siehe Abschnitt 13.
10. **Header-Unterstützung für ältere NIF-Versionen (10.1.0.0/10.2.0.0, 177 Dateien)**
    ergänzt - diesen Dateien fehlt ein Endian-Byte im Header, das 20.0.0.4 hat. Die
    Blockinhalte selbst weichen aber TIEFER ab (schon `NiSourceTexture` ist strukturell
    anders) - eine vollständige Unterstützung dieser Ära bleibt ein eigenständiges,
    größeres Projekt für eine Folgesession. Siehe Abschnitt 15.
11. Zwei weitere Verdachtsstellen (`ParseNiTriStripsHeader`s Freitextfeld bei einer
    Ausreißer-Datei, `NiMaterialProperty`s 14-vs-15-Float-Frage) systematisch untersucht und
    als NICHT robust fixbar verworfen - beide Peek-Versuche verursachten Rückschritte im
    Massentest und wurden sofort zurückgenommen. Siehe Abschnitt 14.
12. **GROSSER FUND: `NiSourceTexture`s `num_extra_data_refs`-Feld fehlt, wenn im Blockindex
    direkt eine weitere `NiSourceTexture` folgt (+39 Dateien!)** - byte-exakt durch
    Rückwärtsrekonstruktion von einer eindeutig lesbaren Dateiendung bewiesen, risikoarmer
    Peek-Fix (ändert nichts an bereits funktionierenden Dateien, da das Feld dort immer 0
    war). Massentest 1676 → 1715/3436. Siehe Abschnitt 16.
13. **GROSSER FUND #2: dasselbe `num_extra_data_refs`-Problem auch in der gemeinsamen
    `ParseObjectNetBase` (nicht nur `NiSourceTexture`) (+26 Dateien!)** - enger gefasster Fix
    (nur bei exakt `0xFFFFFFFF`), da diese Funktion praktisch von jedem Blocktyp verwendet
    wird und kein nachgelagerter Peek zur Absicherung existiert. Massentest 1715 →
    1741/3436, erstmals über 50%. Siehe Abschnitt 17.
14. **Dieselbe Ursache noch zweimal gefunden: `SkipNiStencilProperty` und
    `ParseNiMaterialProperty` hatten die "kurze Basis" hartkodiert statt
    `ParseObjectNetBase()` zu nutzen (+57 Dateien zusammen!)** - danach gezielt nach weiteren
    Fundstellen dieser Art gesucht, keine mehr gefunden. Massentest 1741 → 1798/3436, 52.3%.
    **Gesamtbilanz dieser Fund-Serie (12-14, eine Sitzung): 1676 → 1798, +122 Dateien.**
    Siehe Abschnitt 18.
15. **`NiPSysMeshEmitter` per empirisch bestem Kompromiss (NICHT byte-exakt bewiesen!) statt
    "nicht unterstützt" behandelt (+20 Dateien)** - Massentest-Scan über Kandidaten-Skip-
    Längen zeigte ein Plateau statt eines Optimums (Indiz für pro-Instanz variierende Länge).
    Dokumentiertes Restrisiko unbemerkt leicht verschobener Nachbargeometrie. Massentest 1798
    → 1818/3436, 52.9%. Siehe Abschnitt 19.
16. `NiMeshPSysData` byte-exakt um 17 Byte gegenüber `NiPSysData` korrigiert (netto keine
    neuen Dateien wegen Abhängigkeit von Fund 15, aber eine echte, bewiesene Korrektur).
    Siehe Abschnitt 21.
17. **`ParseObjectNetBase` weiter generalisiert: `num_extra_data_refs` fehlt auch bei
    KLEINEN, gültigen Controller-Werten (nicht nur bei -1) (+18 Dateien)** - Peek jetzt mit
    echter Plausibilitätsprüfung der Werte (nicht nur der Struktur-Form, siehe Abschnitt 20
    für den gegenteiligen, gescheiterten Ansatz). Massentest 1818 → 1836/3436, 53.4%. Siehe
    Abschnitt 23.
18. `NiPSysMeshUpdateModifier` ergänzt (einfache, unzweideutige Struktur, netto keine neuen
    Dateien wegen unabhängiger Folgefehler). Siehe Abschnitt 24.
19. Ein weiterer Gegenbeweis zur `NiTexturingProperty`-`num_shader_textures`-Regel aus
    Abschnitt 7/8 gefunden und dokumentiert (keine Funktionsänderung - ein
    `LooksLikeFreshName`-Peek hätte hier keine Unterscheidungskraft gehabt). Siehe Abschnitt
    25.
20. **Vier weitere einfache `NiExtraData`-Varianten ergänzt: `NiTextKeyExtraData`,
    `NiFloatExtraData`, `NiColorExtraData`, `NiBooleanExtraData` (+6 Dateien)** - drei davon
    byte-exakt verifiziert (lesbare Animationskommandos, plausible Namen/Werte). Massentest
    1836 → 1842/3436, 53.6%. Siehe Abschnitt 26.
21. `NiPSysModifierActiveCtlr` und `NiFlipController` ergänzt (beide byte-exakt verifiziert:
    target-Refs zeigen exakt auf die erwarteten Nachbarblöcke). Massentest 1842 →
    1844/3436, 53.7%. Siehe Abschnitt 27.
22. **DURCHBRUCH: autoritative `nif.xml`-Referenz (niftools/nifxml, 8563 Zeilen) direkt von
    `raw.githubusercontent.com` heruntergeladen** (steht auf der bash-Tool-Netzwerk-Allowlist,
    anders als `github.com` selbst, das für `web_fetch` per robots.txt blockiert ist) - siehe
    /home/claude/work/nif.xml, falls die Datei noch im Container liegt, sonst per
    `curl -sL -o nif.xml https://raw.githubusercontent.com/niftools/nifxml/master/nif.xml`
    neu laden. FÜR FOLGESESSIONEN: dies ist jetzt der ERSTE Anlaufpunkt für jede neue
    Blockstruktur-Frage - lokales `grep`/`sed` statt einzelner Web-Suchen. Damit direkt:
    - Widerlegt die `NiTexturingProperty`-`num_shader_textures`-Regel aus Abschnitt 7/8
      (Feld ist unbedingt vorhanden seit Version 10.0.1.0, keine controller_ref-Bedingung)
      - **+5 Dateien**.
    - `NiPSysMeshEmitter` (Abschnitt 19) durch die exakte Struktur ersetzt statt des
      empirischen 244-Byte-Kompromisses (der zudem durch zwischenzeitliche andere Fixes
      bereits veraltet war).
    - Zwölf weitere Blocktypen ergänzt: `NiPointLight`, `NiSortAdjustNode`, `NiRoomGroup`,
      `NiPalette` (KEINE NiObjectNET-Basis!), `NiVisController`, `NiPSysColliderManager`,
      `NiIntegersExtraData`, `NiMultiTargetTransformController`, `NiPSysGravityStrengthCtlr`,
      `NiFogProperty`, `NiDitherProperty`, `NiSourceCubeMap` - **+4 Dateien**.
    **Gesamt: Massentest 1844 → 1854/3436, 54.0%.** Siehe Abschnitt 28.
23. **WICHTIG: `NiPointLight` aus Punkt 22 war implementiert, aber NIE in die Dispatch-Weiche
    eingebunden** - blieb dadurch wirkungslos, bis in der Folgerunde entdeckt (Lehre: nach
    Einführung mehrerer neuer Typen sofort per `grep -c` auf korrekte Einbindung prüfen, nicht
    nur auf den Gesamt-Massentest verlassen). Dazu `NiBoolTimelineInterpolator`, `NiRoom`,
    `NiPSysPlanarCollider` (neue Basis `NiPSysCollider` entdeckt), `NiPSysEmitterLifeSpanCtlr`
    ergänzt. Massentest 1854 → 1859/3436, 54.1%. Siehe Abschnitt 29.
24. `NiPSysDragModifier` und `NiPortal` ergänzt. Dazu `NiTexturingProperty` korrigiert (hatte
    eine eigene, veraltete Kopie der `num_extra_data_refs`-Peek-Logik statt
    `ParseObjectNetBase()`) und der Bump-Map-Texturslot (Index 5, +24 Byte) ergänzt. Ein
    Rückfallversuch bei `texture_count==0` wurde getestet, verursachte aber einen
    Netto-Rückschritt und wurde verworfen (dokumentiert, nicht implementiert). Massentest
    1859 → 1868/3436, 54.4%. Siehe Abschnitt 30.
25. **`ShaderTexDesc` implementiert statt sauber zu scheitern (+34 Dateien, größter
    Einzelfund seit dem `nif.xml`-Durchbruch)** - `NiTexturingProperty`s `num_shader_
    textures` führte bisher bei jedem Nicht-Null-Wert zu Abbruch. Byte-exakt an
    `bossroom_wall.nif` verifiziert (3 Shader-Texturen, plausible source_refs/map_ids).
    Dazu `NiGeomMorpherController` ergänzt. Massentest 1868 → 1902/3436, 55.4%. Siehe
    Abschnitt 31.
29. **DURCHBRUCH bei älteren NIF-Versionen (10.1.0.0/10.2.0.0): drei zusammenhängende Funde
    (+31 Dateien)** - ausgehend von `skeleton_monolith_blood.nif` (Version 10.2.0.0):
    (a) `TexDesc` hat zwei zusätzliche PS2-Felder vor Version 10.4.0.1, (b) `NiPixelData`s
    Kopf ist bei älteren Versionen 50 statt 72 Byte lang (weicht auch von der Referenz ab -
    noch eine Custom-Engine-Eigenheit, per Mipmap-Ketten-Suche gefunden), (c) der 4-vs-8-Byte-
    Trailer-Peek nach `NiPixelData` versagt strukturell, wenn der nächste Block kein
    Namensfeld hat (`NiTriStripsData`) - **dieser dritte Fund allein brachte +30 Dateien**.
    Massentest 1902 → 1933/3436, 56.3%. Siehe Abschnitt 35.
30. **`Additional Data`-Feld in `NiGeometryData` entfällt bei älteren Versionen komplett
    (+41 Dateien, größter Einzelfund seit `ShaderTexDesc`)** - Feld ist laut Referenz erst
    seit 20.0.0.4 vorhanden. Byte-exakt an `skeleton_monolith_blood.nif` verifiziert (klare
    Streifenlänge, klassische Dreiecksstreifen-Indexfolge nach der Korrektur).
    `ParseNiTriStripsData`/`ParseNiTriShapeData`/`SkipNiGeometryDataHeader` erhalten
    `isOlderVersion`-Parameter. Massentest 1933 → 1974/3436, 57.5%. Siehe Abschnitt 36.
31. Dritter `NiPixelData`-Trailer-Fall gefunden: manchmal GAR KEIN Trailer nötig (0 statt
    4/8 Byte) - byte-exakt verifiziert, ergänzt (+2 Dateien). Siehe Abschnitt 37.
    OFFENER FADEN für Folgesession: `NiZBufferProperty`→`NiTriStripsData` scheint in einem
    Fall 4 weitere Byte zu brauchen - nur ein Beleg, nicht implementiert.
32. **Faden aus Punkt 31 gelöst**: an einer zweiten, unabhängigen Datei (`field_sky_01.nif`,
    identische Werte) bestätigt. Ein unbedingter Test verursachte eine Regression - eng auf
    die Nachbarschaft zu `NiTriStripsData`/`NiTriShapeData` begrenzt (+3 Dateien). Massentest
    1976 → 1979/3436, 57.6%. Siehe Abschnitt 38.
33. **`NiAlphaProperty` braucht dasselbe +4-Byte-Muster wie `NiZBufferProperty`, aber
    VERSIONSUNABHÄNGIG bestätigt (+62 Dateien, größter Einzelfund dieser Session!)** -
    generalisierte Hilfsfunktion `SkipExtraBytesIfFollowedByTriData` ergänzt. Ein Versuch,
    dies auch auf `NiVertexColorProperty`/`NiStencilProperty`/`NiSpecularProperty`/
    `NiFogProperty`/`NiDitherProperty` anzuwenden, verursachte einen Rückschritt (-8) und
    wurde zurückgenommen - OFFEN für eine Folgesession, jeden Typ EINZELN zu testen.
    Massentest 1979 → 2041/3436, **59.4%**. Siehe Abschnitt 39.
34. Offener Faden aus Punkt 33 einzeln durchgetestet: `NiVertexColorProperty` (+14),
    `NiSpecularProperty` (+2), `NiFogProperty`/`NiDitherProperty` (je neutral) sicher
    übernommen; `NiStencilProperty` (-24, eigene interne Freitextfeld-Mehrdeutigkeit
    kollidiert) sofort zurückgenommen. Massentest 2041 → 2057/3436, 59.9%. Siehe
    Abschnitt 40.
35. **🎉 DURCHBRUCH, MIT ABSTAND GRÖSSTER EINZELFUND DER SITZUNG: das "+4-Byte-Muster" aus
    Punkt 33/34 wurde GENERALISIERT (+411 Dateien!).** Bestätigt an `NiFloatData` als
    weiterem Vorgänger - das Problem hängt NICHT vom spezifischen vorherigen Blocktyp ab.
    Statt weiter einzelne Typen aufzuzählen, prüft eine neue Hilfsfunktion
    (`LooksLikeTriDataHeader`) jetzt DIREKT am Zielblock (`NiTriStripsData`/
    `NiTriShapeData`), ob die aktuelle Position plausibel aussieht - unabhängig vom
    Vorgänger. Massentest 2057 → **2468/3436 (71.8%!)**. Siehe Abschnitt 42.
36. `NiTriShapeData` (anders als `NiTriStripsData`) verliert bei älteren Versionen das
    "mysteriöse" u16-Feld nach den UV-Daten KOMPLETT, nicht nur `additional_data_ref` -
    byte-exakt an `horse2.nif`/`horse3.nif` verifiziert (num_triangles=700,
    num_triangle_points=2100=700*3). Massentest 2468 → 2490/3436, 72.5%. Siehe Abschnitt 43.
37. **🎉 ZWEITGRÖSSTER EINZELFUND DER SESSION: vollständige ObjectNetBase-Kettenvalidierung
    erkennt benannte Objekte sicher (+128 Dateien!).** Anders als der in Punkt "Abschnitt 20"
    gescheiterte Versuch (nur das Namensfeld geprüft, katastrophaler Rückschritt) validiert
    die neue Prüfung Name UND numExtra UND controller GLEICHZEITIG als zusammenhängende
    Kette - eine Kombination, die weit seltener zufällig erfüllt ist. Byte-exakt an
    `Tree01.nif` verifiziert, stichprobenartig auf konsistente Geometrie geprüft. Massentest
    2490 → **2618/3436 (76.2%!)**. Siehe Abschnitt 45.
26. `NiGeometryData`-Kernstruktur (Basis für JEDES Mesh) gegen die autoritative Referenz
    bestätigt (keine Code-Änderung an der Feldreihenfolge nötig - gute Nachricht nach
    mehreren Runden Unsicherheit). Dabei einen latenten Bug behoben: das "Data Flags"-Feld
    hatte einen zu engen Plausibilitäts-Cap, der Dateien mit Tangenten-Bit gesetzt fälschlich
    abgelehnt hätte - Tangenten/Binormalen-Handling ergänzt. Netto keine neuen Dateien im
    aktuellen Korpus, aber wichtige Absicherung. Auch bestätigt: `NiPosData`s `key_type=0`
    ist laut autoritativer `KeyType`-Enum-Definition (nur Werte 1-5) genuinely ungültig -
    betrifft 38 Kopien derselben `EnvSet.nif`. Siehe Abschnitt 32.
27. **`NiPixelData`-Kopf weicht von BEIDEN nif.xml-Versionsvarianten ab** (bestätigt an
    `BerFrz01_Ice02.nif`, keine Version ergibt plausible Werte) - bereits funktionierende
    Eigenimplementierung bewusst nicht verändert. Siehe Abschnitt 33.
28. **⚠️ WICHTIGSTE WARNUNG DIESER SESSION: `ParseNiTriStripsHeader` bleibt ENDGÜLTIG OFF
    LIMITS.** Ein ZWEITER, für sich genommen sehr überzeugend aussehender Versuch (exakte
    autoritative Referenzstruktur + byte-exakter Einzelbeleg an `Leviathan_deco1.nif`)
    verursachte einen KATASTROPHALEN Rückschritt (1902 → 1486!). Dieser custom Engine-Fork
    weicht hier (wie auch bei `NiPixelData`, Punkt 27) von der Vanilla-Spezifikation ab.
    NICHT ERNEUT VERSUCHEN, auch nicht mit scheinbar wasserdichter Evidenz. Siehe Abschnitt
    34 für die vollständige Analyse.

Bei den Funden 1-3 war der Schlüssel eine unabhängige, öffentliche Referenzimplementierung
(Hinweis des Nutzers auf NifTools-Produkte). Bei Fund 6 half stattdessen eine NEUE, generische
Heuristik (`LooksLikeFreshName()`: prüft, ob eine Position plausibel der Anfang eines
Namensfelds ist) - nützlich für konditionale Felder, wo keine erschöpfende externe Referenz
vorliegt. Wichtige Lehre aus Fund 3: auch eine gute Referenz kann für einzelne Felder eines
Forks danebenliegen - jede übernommene Korrektur einzeln byte-exakt verifizieren.

**Wichtige Vorsicht:** ein Versuch, den bei `NiParticleSystem` erfolgreichen `has_shader`-Fix
auch auf `ParseNiTriStripsHeader` (die gemeinsame, extrem häufig genutzte Kopf-Funktion für
NiTriStrips/NiTriShape) anzuwenden, verursachte einen KATASTROPHALEN Rückschritt (1664 → 4 im
Massentest) und wurde sofort zurückgenommen. Diese Funktion NICHT ohne sehr sorgfältige,
schrittweise Verifikation anfassen - siehe docs/MAP_FORMAT.md Abschnitt 12 für Details.

29. **Folgesitzung (kein Massentest-Zuwachs, aber wichtige strukturelle Diagnose):** eigenes
    `mass_test`-Tool gebaut (lag nicht im Repo - für Folgesessions mitbringen/wiederverwenden,
    spart Zeit; walkt ein Verzeichnis, bucketed Fehler nach `Block N (Typ)`, zeigt zusätzlich
    eine nach Typ AGGREGIERTE Verteilung). Sechs Buckets (`NiCollisionData`,
    `NiBillboardNode`, `NiNode`, `NiIntegerExtraData`, `NiPSysSpawnModifier`/
    `NiPSysGrowFadeModifier`, `NiFloatData`/`NiBoolData`) systematisch untersucht:
    - **NiCollisionData/NiBillboardNode/NiNode/NiIntegerExtraData sind größtenteils reine
      Symptom-Buckets** einer `ParseNiTriStripsHeader`-Kaskade (direkter Vorgänger in allen
      Stichproben NiTriStrips/NiTriShape/NiTriStripsData) - kein eigener Fixbedarf.
    - **DREI weitere, an mehreren Dateien byte-exakt aussehende Fixes durch vollen
      Massentest widerlegt** (alle NICHT ERNEUT VERSUCHEN ohne neue Evidenz):
      - `NiStencilProperty`s letztes Feld (String vs. reines Skalar) ist NICHT zuverlässig per
        Druckbarkeits-Peek unterscheidbar (2618→2606, netto -12).
      - `NiPSysEmitter`s "Unknown QQSpeed Floats" aus der autoritativen nif.xml existieren in
        diesem Fork nicht (2618→2594, netto -24).
      - `NiPSysModifierBase`/`NiPSysEmitterBase` brauchen KEINE universellen 2 Byte zwischen
        `order` und `target_ref` (an 3 "Leviathan"-Partikeldateien byte-exakt aussehend, aber
        beide getesteten Varianten Regressionen: -24 bzw. -28).
    - **`key_type=0` bei `KeyGroup` (offener Punkt, kein Dead End) weiter eingegrenzt:**
      betrifft NUR `NiPosData`/`NiFloatData` (6/6 Dateien systematisch geprüft, ALLE mit exakt
      `num_keys=2`) - KORREKTUR: `NiBoolData`/`NiColorData`-Fehlschläge, die anfangs
      demselben Muster zugeordnet wurden, zeigen bei genauerer Prüfung KOMPLETT ANDERE
      (eindeutig falsch ausgerichtete) Werte - andere, unabhängige Ursache, nicht `key_type=0`.
      An `EnvSet.nif` eine plausibel aussehende, aber NICHT verifizierte Hypothese gefunden
      (10 Floats/Key: `value(3)+tangent_a(3)+tangent_b(3)+time(1)`, Zeit am Ende statt am
      Anfang - steigende Zeiten 2.25→2.5, geteilte Zwischen-Tangente) - landet aber nicht auf
      einer per `LooksLikeTriDataHeader` plausiblen Blockgrenze, also UNBESTÄTIGT. NifSkope-
      Quelle geprüft, bringt nichts (rein XML-getrieben, keine Sonderbehandlung für Wert 0).
    - Siehe docs/MAP_FORMAT.md Abschnitt 46 für die vollständige Analyse.

30. **Strategiewechsel nach den drei Dead Ends: `NiLookAtInterpolator` komplett neu
    implementiert (+4 Dateien, `2618 → 2622/3436, 76.3%`) - erster echter Zuwachs dieser
    Sitzung.** Statt weiter an bestehenden Funktionen zu raten, gezielt nach einem komplett
    UNIMPLEMENTIERTEN Blocktyp gesucht (additiv, kein Regressionsrisiko mit bestehendem Code).
    Struktur aus `nif.xml`: `flags(u16)+look_at_ref(i32)+look_at_name(String)+
    NiQuatTransform(32 Byte)+3×Interpolator-Ref(i32)`. Byte-exakt an `H_AIRDOLL.nif`
    verifiziert (Einheits-Quaternion, `-FLT_MAX`-Sentinelwerte, `look_at_ref` zeigt exakt aufs
    nächste `NiNode`, Blockgrenze landet exakt auf `"Camera01.Target"`). VOR Übernahme in den
    echten Code voll gegen 7/7 Suiten + Massentest verifiziert (kein Debug-Kopie-Umweg nötig,
    da additiv). Siehe docs/MAP_FORMAT.md Abschnitt 47. **Lehre für Folgesessions:** immer
    zuerst die expliziten "Nicht unterstützter Block-Typ"-Meldungen der `mass_test`-Ausgabe
    prüfen, bevor an bestehenden (größtenteils funktionierenden) Funktionen geraten wird -
    dort ist Fortschritt am risikoärmsten zu holen.

31. **Zu strikte Mesh-Geometrie-Prüfung entschärft (+3 Dateien, `2622 → 2625/3436, 76.4%`).**
    `beraBN.nif`/`bera_BN01.nif`/`bera_BNset.nif` enthalten NACHWEISLICH ausschließlich
    `NiNode`/`NiCollisionData`/Properties - legitime, rein unsichtbare Kollisions-/
    Ankerpunkt-Objekte, keine kaputten Dateien. Der harte Fehler bei leerem `model.parts`
    wurde entfernt (`NifMeshRenderer` iteriert bereits sicher über leere `parts`-Listen).
    Siehe docs/MAP_FORMAT.md Abschnitt 48 Fund A.
32. **Version 10.1.0.0 hat ein zusätzliches, unversioniertes 4-Byte-Header-Feld (immer 0)
    direkt vor Block 0** - byte-exakt an ALLEN 7 betroffenen Dateien verifiziert (landet
    danach exakt auf dem lesbaren Namensfeld `"Scene Root"`, bei jeder der 7 Dateien
    identisch). Eng versionsgegated (nur `0x0a010000`), betrifft NUR diese 7 Dateien.
    **Kein direkter Massentest-Zuwachs** - die Dateien kommen jetzt bis Block 3/4 statt Block
    0, scheitern dort aber an weiteren, noch ungelösten 10.1.0.0-Eigenheiten (passt zur
    bereits in Abschnitt 15/29 dokumentierten Einschätzung: eigenes, größeres Projekt).
    Trotzdem übernommen (verifizierter echter Fortschritt, 0 Regressionsrisiko). Siehe
    docs/MAP_FORMAT.md Abschnitt 48 Fund B.
    - **Fund C (untersucht, NICHT gelöst):** `NiMeshPSysData→NiBoolData→...→NiNode`-Kette (5
      Dateien) zeigt ab dem zweiten Rotationsmatrix-Float ein Garbage-Denormal-Muster
      (`2.2779507836064226e-41`), das schon bei der `NiPSysBoxEmitter`-Sackgasse auftauchte
      (Abschnitt 46 Dead End 3) - ±4-Byte-Verschiebungstests lösen es nicht auf, tiefer
      liegende Fehlausrichtung vermutet. Für eine Folgesession mit mehr Zeitbudget.

33. **⭐ WICHTIGSTER OFFENER PUNKT für die nächste Sitzung - jetzt präzise vermessen, aber
    GRÖSSER als ursprünglich gedacht:** dieselbe "1.0f-Array"-Signatur taucht in VIER
    unabhängigen Kontexten auf (`NiPSysBoxEmitter`, `NiMeshPSysData→NiNode`,
    `NiPSysBoundUpdateModifier→NiNode→NiNode`, `NiFloatInterpolator/NiFloatData→
    NiSourceTexture`) - in JEDEM Fall wurde die VORANGEHENDE Blockgrenze unabhängig
    byte-exakt verifiziert, schließt Kaskade also aus. **Exakte Struktur vermessen
    (Abschnitt 50):** `0xFFFFFFFF`-Sentinel + Flag-Byte(`0x01`) + `N` Floats(`1.0`) +
    festes 3-Byte-Trennzeichen (`00 00 01`) + NOCHMAL `N` Floats(`1.0`) + Flag-Byte. `N`
    variiert (31/30/30/5) - ein echter Zähler, keine Konstante, Herkunft noch unklar.
    **WICHTIGE KORREKTUR (Abschnitt 51):** danach folgt KEINE kleine Lücke, sondern eine
    Nullregion von MINDESTENS 1,3 KB - das ist kein kleines fehlendes Feld, sondern
    vermutlich ein ganzer, bisher nicht erkannter Block oder ein großer, nur teilweise
    befüllter Puffer. NifSkope-Quelle hilft nicht weiter (rein `nif.xml`-getrieben, kennt
    keine proprietären Fork-Erweiterungen). **Realistische Einschätzung: dieses Rätsel ist
    ein eigenständiges, größeres Reverse-Engineering-Projekt** (vergleichbar mit
    `ParseNiTriStripsHeader` selbst), keine schnelle Korrektur. Siehe docs/MAP_FORMAT.md
    Abschnitt 51 für den vollständigen, ehrlichen Fahrplan der nächsten Sitzung.

## GUI-Umbau: neue Navigationsebene nach Mockup-Vorgabe ("NextGen-Editor")

David hat zwei Mockup-Bilder geschickt (Figma-artige Wireframes, App-Titel "NextGen-Editor")
mit einer komplett neuen, mehrstufigen Navigationsstruktur anstelle des bisherigen, immer
sichtbaren Andock-Fenster-Layouts. Wunsch: Beschriftungen zunächst auf Deutsch, Englisch
als Sprachumschaltung später hinzufügbar.

**Umgesetzt (nur syntaktisch geprüft, siehe Einschränkung unten):**
- **NEU `include/mapeditor/app/Localization.hpp`**: `T("schlüssel")`-Übersetzungssystem,
  `Language::German`/`Language::English`, umschaltbar zur Laufzeit (kleiner DE/EN-Dropdown
  oben rechts). Deutsche Texte vollständig für die neue Navigationsebene, englische Texte
  funktional vorbefüllt (NICHT von einem Muttersprachler geprüft - reine Vorbereitung wie
  gewünscht). Bewusst NUR für die neue Navigationsebene eingeführt - die bestehenden, tief
  verschachtelten Funktions-Panels (jetzt `DrawAdvancedFileOps`, ehemals das "Datei"-Menü)
  bleiben hartcodiertes Deutsch; vollständige Migration wäre ein eigener, separater Schritt.
- **Neuer Bildschirm-Zustandsautomat** (`AppScreen`: `ProjectHub`/`NewProjectConfig`/
  `MapEditorLauncher`/`MapEditorWorkspace`/`ComingSoon`) in `EditorState` statt des
  bisherigen einzelnen, immer aktiven Layouts.
- **KORREKTUR (wichtig):** die gelben Klebezettel-Notizen in den Mockups waren
  Umsetzungs-Hinweise für die Entwicklung, KEINE echten UI-Elemente - auf Rückfrage vom
  Nutzer klargestellt und in dieser Sitzung wieder entfernt. Betraf: die "Noch nicht
  entschieden"/Quest-Editor-Notiz auf den Karten, die Projekt-Ordner-Erklärung, den
  Dateiformat-Hinweis im Map-Editor-Start und den Tools-Spalten-Hinweis im Arbeitsbereich.
  Die dahinterliegenden BEDEUTUNGEN wurden trotzdem beachtet (z.B. zeigt die Tools-Spalte
  weiterhin je nach aktivem Tab unterschiedliche Werkzeuge, nur eben ohne die erklärende
  Notiz als sichtbares UI-Element).
- **Projekt-Hub** (`DrawProjectHub`): die 6 Editor-Karten aus dem Mockup (MapEditor, SHN
  Editor, Quest Editor, Interface Editor, Drop Table, Skill+Action) mit einfachen
  vektoriellen Icons (`DrawIconGlobe`/`DrawIconPencilPaper`/`DrawIconBook`/
  `DrawIconMonitorEye`/`DrawIconAtom`/`DrawIconClapper` - reine `ImDrawList`-Primitive, kein
  Bild-Asset nötig), Feature-Bulletpoints. NUR die MapEditor-Karte führt zu echter
  Funktionalität - alle anderen 5 zu `ComingSoon` (Platzhalter mit Zurück-Knopf).
- **Neues Projekt konfigurieren** (`DrawNewProjectConfig`): Projekt Name/Projekt Ordner/
  Client Ordner/Server Ordner (mit `BrowseForFolderWindows`), "Projekt erstellen/Speichern".
  NEU: `ProjectConfig`-Struktur + `SaveProjectConfig`/
  `TryLoadProjectConfig` - schreibt/liest eine einfache `project.tsproj`-Schlüssel-Wert-Datei
  im Projekt-Ordner (bewusst kein JSON - passend zum Rest der Codebasis, die ausschließlich
  native/Legacy-Formate ohne JSON-Abhängigkeit nutzt). **Die im Mockup beschriebene
  automatische Ordnerstruktur-Spiegelung zwischen Projekt-/Client-/Server-Ordner ist NOCH
  NICHT implementiert** - aktuell wird nur die Konfiguration selbst gespeichert; der
  Projekt-Ordner wird als Vorgabe-Speicherziel für "Save" im Arbeitsbereich verwendet, der
  Client-Ordner als Suchwurzel für "Map Öffnen". Die eigentliche
  Client/Server-Struktur-Mirroring-Logik aus der Mockup-Notiz braucht eine eigene Spezifikation.
- **Map-Editor-Start** (`DrawMapEditorLauncher`): "Create New Map" (Name/X Länge/Y
  Breite/Textur Layer, ersetzt das bisherige feste "Neu (257x257)" durch freie Maße) und
  "Browse Map's" (durchsucht jetzt automatisch `project.clientFolder` statt eines separat
  gewählten Asset-Ordners) nebeneinander.
- **Map-Editor-Arbeitsbereich** (`DrawMapEditorWorkspace` + `DrawWorkspaceTabBar`):
  Tab-Leiste (Hightmap/Texturing/Block-Walk/Objects/NPCs/NPC AI/Mobs/Mob AI + Zurück) steuert
  denselben `EditMode`, der vorher über Radio-Buttons im Werkzeuge-Fenster gewählt wurde -
  `EditMode` um vier neue, noch funktionslose Werte erweitert (`Npcs`/`NpcAi`/`Mobs`/`MobAi`
  - zeigen nur "Noch nicht implementiert"). Drei-Spalten-Layout: "Datei"
  (Save/Save as/Undo/Redo) + "Tools/etc" (komplett wiederverwendete bestehende
  Werkzeug-Logik, jetzt `DrawToolsContent` statt eigenes Fenster) links, "2D View"
  (`DrawEditor2DContent`, vormals `DrawEditor2D`) Mitte, "3D View" (`DrawPreview3DContent`,
  vormals `DrawPreview3D`, jetzt mit +/- Zoom-Knöpfen unten rechts wie im Mockup) rechts.
  Die komplette bisherige "Datei"-Menü-Funktionalität (alle nativen/Legacy-Import/Export-
  Formate) ist NICHT verloren gegangen, sondern unter einem einklappbaren "Erweitert"-Header
  in der Datei-Spalte erreichbar (`DrawAdvancedFileOps`, ehemals `DrawMenuBar`s Menü-Inhalt).
- Frei andockbares Fenster-Layout (`ImGui::DockSpace`) komplett entfernt - die neue
  Oberfläche nutzt ein einziges Vollbild-Host-Fenster mit fest layouteten Bereichen je
  Bildschirm, passend zum Mockup (kein frei verschiebbares Fenster-Chaos mehr).

**WICHTIGE EINSCHRÄNKUNG - nur syntaktisch geprüft, NICHT real gebaut:** diese Sandbox hat
keine echte GLFW/ImGui/glad-Umgebung zum tatsächlichen Kompilieren+Linken+Ausführen. Verifiziert
wurde ausschließlich `-fsyntax-only` (0 Fehler, 0 Warnungen mit `-Wall -Wextra`) gegen die
echten `imgui`-Header (docking-Branch, github.com/ocornut/imgui) + den echten GLFW-Header +
einen selbstgeschriebenen `glad.h`-Stub (nur Typen/Konstanten/Deklarationen, kein echter
GL-Loader). Das deckt Syntax- und Typfehler zuverlässig ab, aber NICHT: Linker-Fehler, Laufzeit-
verhalten, Layout-Feinheiten (Spaltenbreiten/Icon-Optik in der Praxis), oder ob
`ImDrawList::AddEllipse`/`AddPolyline` in der über vcpkg installierten ImGui-Version verfügbar
sind (beide existieren im `docking`-Branch, den `imgui[docking-experimental]` in vcpkg.json
auch anfordert - sollte also passen, aber ungeprüft). **Nächster Schritt für David: einmal
real mit vcpkg bauen und laufen lassen, dann Rückmeldung zu Bugs/Optik geben.**

### Erste echte Rückmeldung nach Build (v0.44.2) - zwei Bugs behoben
1. **"New Map"/"Map Öffnen" ohne Funktion** - beide Panels waren immer gleichzeitig sichtbar.
   Neu: `EditorState::MapLauncherView` (`NewMap`/`Browse`) steuert jetzt, welches Panel
   sichtbar ist; die Knöpfe schalten um und sind aktiv hervorgehoben.
2. **Karten unter `<Client>/resmap/...` wurden nicht gefunden, Ordnerdialog zeigte
   Inhalte nicht zuverlässig.** Zwei Ursachen: (a) `SHBrowseForFolder` (veraltete API,
   bekannt für unzuverlässige Navigation/Anzeige) ersetzt durch `IFileOpenDialog`
   (moderne COM-API, echtes Explorer-Fenster, jetzt als Kind-Fenster des Hauptfensters
   verankert über `glfwGetWin32Window`). (b) NEU `ResolveMapSearchRoot`: "Client Ordner"
   ist der Client-WURZELordner (enthält `resmap`), wird jetzt automatisch erkannt/
   durchsucht statt dass `resmap` manuell mit ausgewählt werden muss. Rescan-Trigger
   repariert (vorher: kein erneuter Scan nach Ordnerwechsel, wenn der letzte Scan 0
   Treffer hatte). "Browse Map's" zeigt jetzt Suchpfad + Trefferzahl sichtbar an statt
   bei 0 Treffern rätselhaft leer zu bleiben.

   **WICHTIG:** die `IFileOpenDialog`-Änderung liegt hinter `#ifdef _WIN32` und konnte in
   dieser Linux-Sandbox NICHT kompiliert werden (nur sorgfältig gegen die bekannte
   COM-API-Signatur von Hand geprüft, nicht automatisiert verifiziert) - nach dem nächsten
   Build bitte gezielt Rückmeldung dazu geben, ob der Ordnerdialog jetzt normal funktioniert.

3. **(v0.44.3) Kartensuche fand fälschlich "ressystem" statt "resmap".** Screenshot-Rückmeldung
   zeigte: `resmap` lag nicht direkt im gewählten Client-Ordner, wodurch die Suche
   stillschweigend auf den gesamten Client-Baum zurückfiel und zwei `.ini`-Dateien aus
   fremden "ressystem"-Ordnern fand. `FindResmapFolder` sucht jetzt gezielt (bis zu drei
   Ebenen tief) nach einem Ordner namens "resmap", steigt dabei NICHT in "ressystem"/
   "fieldTexture" ab, und `ResolveMapSearchRoot` fällt bei Nichtfund NICHT mehr auf den
   ganzen Client-Ordner zurück - stattdessen klare Meldung "'resmap' nicht gefunden".
4. **(v0.44.4) Kartensuche erstmals ECHT kompiliert und gegen reale Daten verifiziert**
   (nicht nur `-fsyntax-only`) - `ScanForMaps`/`FindResmapFolder` brauchen kein Windows/
   GLFW/ImGui, reines `std::filesystem`. Gegen 116 echte Kartenordner aus einem früheren
   Sitzungs-Korpus getestet (`resmap/field/<Map>/<Map>.ini` und
   `resmap/IDField/<Map>/<Map>.ini`) - alle 116 korrekt gefunden. Zusätzlich verschärft:
   nur noch `.ini`-Dateien akzeptiert, deren Name exakt zum eigenen Ordnernamen passt
   (`<MAPORDNER>/<MAPORDNER>.ini`). Tooltip mit vollem Pfad beim Überfahren eines
   Karten-Eintrags ergänzt (Diagnose ohne Karte öffnen zu müssen).
5. **(v0.44.5) Mehrere gleichnamige "resmap"-Ordner möglich.** Rückmeldung: gefundener Pfad
   ("Client/reschar/resmap") enthielt laut Nutzer keine echten Karten. `FindResmapFolder`
   (Einzeltreffer) durch `FindAllResmapCandidates` (sammelt ALLE "resmap"-Ordner, bis drei
   Ebenen tief) + `ResolveMapSearchRootAndScan` (scannt jeden Kandidaten, wählt den mit den
   meisten gefundenen Karten) ersetzt. Mit echten Testdaten verifiziert: künstlicher leerer
   "resmap"-Zweitordner wurde korrekt zugunsten des echten (116 Karten) übergangen.
6. **(v0.44.6) Map-Textur oben/unten vertauscht + NIF-Objekte ohne Textur.** Nutzer-Vergleich
   (2D View mit rotem Block/Walk-Overlay [korrekt] vs. Nahaufnahme der Diffuse-Textur
   [vertauscht]) zeigte: der in einer früheren Sitzung eingeführte Diffuse-V-Flip (siehe
   `docs/MAP_FORMAT.md`, "Diffuse-Textur war relativ zur Blend-Struktur oben/unten
   vertauscht") hat das Problem NICHT behoben - Block/Walk nutzt dieselbe `mapUv`-Achse wie
   Blend (beide korrekt), also war der Flip die falsche Richtung. **Zurückgenommen** -
   Diffuse-UV nutzt jetzt `mapUv` direkt wie Blend/Block/Walk. DDS-Decoder geprüft und für
   korrekt befunden (kein Flip im Zeilen-Lesen). Falls die Textur danach WEITERHIN falsch
   orientiert ist, liegt die Ursache vermutlich woanders (nicht mehr UV, nicht Decoder) -
   bitte erneut mit Screenshot melden.
   NIF-Objekt-Texturen: `.nif`-Dateien referenzieren Texturen oft nur als nackten Dateinamen
   (kein Verzeichnis, z.B. `"ELDERIN_wg.DDS"`) - `ResolveLegacyAssetPath` sucht solche Namen
   jetzt zusätzlich gezielt im `fieldTexture`-Ordner (auch Unterordner, begrenzt rekursiv).
   Zusätzlich toleriert: Leerzeichen MITTEN im Dateinamen vor der Endung (z.B.
   `"road01_lamp .dds"`, byte-exakt bestätigt). Mit simulierten Testdaten verifiziert (keine
   echten `.dds`-Dateien in der Sandbox vorhanden). Siehe docs/MAP_FORMAT.md für Details.
7. **(v0.44.7) Datei-Picker für Texturen/NIF-Modelle + feinerer Block/Walk-Pinsel.**
   Nutzerwunsch: manuelle Pfadeingabe für Textur-Layer und Objekt-Modelle durch
   "Durchsuchen..."-Knopf mit filterbarem Auswahl-Popup ersetzt (`DrawAssetPickerPopup`) -
   sucht automatisch in `fieldTexture` bzw. `nif`/`nifs` unterhalb des gefundenen `resmap`.
   Block/Walk-Pinselradius: Minimum von 10 auf 1 gesenkt, logarithmische Skalierung für
   feinere Kontrolle, neuer "1 Zelle"-Knopf (exakte Einzelzell-Präzision unabhängig von der
   nicht-quadratischen Zellgröße). Mit simulierten Dateistrukturen verifiziert.

## Was NICHT funktioniert / offen
1. **`.nif`: 811 von 3436 Dateien scheitern noch.** WICHTIGER HINWEIS: `NiPSysMeshEmitter`
   wird seit v0.32.0 mit einer empirisch besten, aber NICHT byte-exakt bewiesenen Skip-Länge
   behandelt (Restrisiko unbemerkt leicht verschobener Nachbargeometrie - siehe
   docs/MAP_FORMAT.md Abschnitt 19). Bei Auffälligkeiten in Partikelsystem-Nachbarblöcken
   ist diese Funktion der erste Verdächtige. Größte verbleibende Blocker laut letztem
   Massentest: 177 Dateien mit älterer NIF-Version (10.1.0.0/10.2.0.0) - Header jetzt gelöst,
   Blockinhalte (u.a. `NiSourceTexture`) weichen aber tiefer ab, eigenes Projekt (Abschnitt 15);
   weiterhin viele `NiTriStripsData`/`NiTriShapeData`- und `NiVertexColorProperty`-EOF-Fehler
   an unterschiedlichsten Blockindizes (~200-300 Dateien zusammen - `NiSkinInstance` selbst
   ist jetzt kein Blocker mehr, siehe Abschnitt 13, aber ähnliche, noch unentdeckte
   Sonderfälle sind wahrscheinlich; siehe die Warnungen zu `ParseNiTriStripsHeader` und
   `ParseNiMaterialProperty` oben, bevor diese Funktionen angefasst werden - andere,
   isoliertere Stellen wie der Trailer-Peek sind risikoärmer, siehe Abschnitt 13),
   `NiPSysMeshEmitter` und weitere seltene Partikel-Typen (`NiPSysMeshUpdateModifier`,
   `NiPSysModifierActiveCtlr`, `NiPSysGravityStrengthCtlr`), ~28 Dateien mit `NiPosData` bei
   ungültigem `key_type=0`, ~9 Dateien mit `NiLODNode` selbst noch blockiert.
2. **GELÖST in v0.22.0** (war hier als Folgefehler gelistet) - siehe Punkt 3 oben.
3. **`NiTriShapeData`/`NiTriStripsData` mit `num_vertices=0`** (~26 Dateien, vermutlich
   geskinnte Meshes): die auf `has_vertices=0` folgenden Bytes sind nicht als plausible
   `has_normals`/`num_uv_sets` interpretierbar. Nicht abschließend verifiziert.
4. **`NiPosData` mit `key_type=0`** (~28 Dateien): laut offiziellem `KeyType`-Enum ein
   ungültiger Wert. Schlägt bereits sauber fehl (kein Rateversuch).
5. **Objekt-Texturierung: UV-Extraktion jetzt korrekt, aber weiterhin KEIN echter Build/
   visueller Test.** Offene Frage für den nächsten echten Build: ob NIF-eigene UV-Koordinaten
   dieselbe V-Achsen-Behandlung wie die berechneten Terrain-UVs brauchen (dort war ein Flip
   nötig).
6. **`Eld`/`.sbi`-Format**: eigenständiges Subsystem, kein Kartenformat - bewusst nicht
   weiterverfolgt.
7. Karten mit >8 Textur-Layern zeigen nur die ersten 8 (Hardware-Grenze).
8. `.idm`-Zellzuordnung (1178 Gruppen) und `.shbd`-Bit-Semantik weiterhin Hypothese.
9. **Eingebettete NiPixelData-Textur, Pixelformat 3** (unkomprimiert/palettiert, ~12 Dateien
   im Massentest, z.B. `wall_4.png`, `floor_17.png`, `GroundText.dds` als NiSourceTexture-
   Name): `DecodeBcImage` unterstützt bisher nur Format 4/5/6 (DXT1/3/5). Format 3 noch nicht
   untersucht - vermutlich unkomprimiertes RGBA oder ein Palettenformat.
10. **Asset-Picker-Vorschaubilder + Layer-Icons (v0.44.9): weiterhin KEIN echter Build/
    visueller Test.** Nur syntaktisch geprüft (siehe "Diese Chat-Sitzung" oben).

## Nächste sinnvolle Schritte (Vorschlag, keine Pflicht)
- **NEU (18.09.2026): `.nif`-Parser-Fixes nach Platzierungs-Häufigkeit priorisieren, nicht
  nach Dateizahl.** Impact-Analyse gegen alle 214 echten Karten (siehe CHANGELOG [0.44.10])
  zeigt: die 811 scheiternden Dateien sind sehr ungleich verteilt. Konkrete Reihenfolge nach
  Platzierungs-Häufigkeit (Datei, Fehler-Block, Anzahl Platzierungen im Testdatensatz):
  `grass.nif` (Block 5/NiTriStrips EOF, 603x auf Karte TevaL - andere grass.nif-Kopien im
  Datensatz laden fehlerfrei, also KEIN systemischer Bug, sondern diese eine Datei),
  `sin_firelamp.nif` (Block 46/NiPSysSpawnModifier, 346x), `light-tru.nif` (Block 29/
  NiTriStrips, 316x), `Tunnel01_Wood4.nif`/`Tunnel02_Wood4.nif` (je Block 22/NiNode bzw.
  ähnlich, zusammen 464x), `woodbridge.nif` auf KDPrtShip (Block 16/NiCollisionData, 214x),
  `streetlight1.nif` (Block 52/NiTriStrips, scheitert an ALLEN 5 bekannten Kopien identisch -
  das IST ein systemischer Bug, kein Dateidefekt). Vor jedem Fix: exakte Fehlerstelle mit
  `LoadNifMesh` gegen mehrere unabhängige betroffene Dateien gegenprüfen, dann sofort per
  vollem Massentest auf Regressionen prüfen (wie immer).
- **Objekt-Texturierung nach dem nächsten echten Build visuell verifizieren** - jetzt der
  naheliegendste nächste Schritt für "Texturing", da die UVs erstmals korrekt sind
- **WICHTIGSTE ERKENNTNIS der letzten Sitzung (siehe Punkt 29 oben / MAP_FORMAT.md Abschnitt
  46):** `NiCollisionData`, `NiBillboardNode`, `NiNode` sind größtenteils reine
  `ParseNiTriStripsHeader`-Symptom-Buckets, KEIN eigener Fixbedarf. Bevor weitere Buckets
  untersucht werden, lohnt sich eine vollständige Mehrfach-Hop-Rückverfolgung ALLER 819
  Fehlschläge (nicht nur des direkten Vorgängers) - würde vermutlich zeigen, dass ein noch
  größerer Anteil als die bisher gemessenen ~20% (direkter Vorgänger) tatsächlich auf
  `ParseNiTriStripsHeader` zurückgeht. Bisher NICHT auf Kaskade geprüfte, potenziell frischere
  Buckets: `NiIntegerExtraData` (13), `NiStringExtraData` (9), `NiTexturingProperty` (11),
  `NiPSysGrowFadeModifier` (17), `NiPSysColorModifier` (8) - erst deren direkten Vorgänger
  prüfen, BEVOR ein Fix versucht wird (siehe unten).
- **ZWEI NEUE, per vollem Massentest widerlegte Fixes - NICHT ERNEUT VERSUCHEN ohne neue
  Evidenz:** `NiStencilProperty`-Trailer per Druckbarkeits-Peek (2618→2606) und `NiPSysEmitter`s
  "Unknown QQSpeed Floats" aus nif.xml (2618→2594). Details: MAP_FORMAT.md Abschnitt 46.
- **Bei jedem neuen Fix-Versuch: ZWINGEND gegen den VOLLEN Massentest prüfen, nicht nur gegen
  1-5 Einzeldateien.** Beide obigen Dead Ends sahen an mehreren Dateien byte-exakt korrekt aus
  und waren es trotzdem nicht (geteilte Basisfunktionen wie `ParseObjectNetBase` oder
  `NiPSysEmitterBase` werden von vielen verschiedenen, bereits korrekt funktionierenden
  Dateien mitgenutzt - ein Fix, der 4 Dateien rettet, kann leicht 16 andere brechen).
- **Referenzen über die nif.xml hinaus:** https://github.com/niftools bietet neben
  `nifxml` (nif.xml) auch **NifSkope** (C++, den De-facto-Referenzparser mit tatsächlichem
  Lese-/Schreibcode statt nur XML-Deklarationen) und **PyFFI** (Python) - bei mehrdeutigen
  oder widersprüchlichen nif.xml-Feldern (wie den zwei diese Sitzung widerlegten Fällen) lohnt
  sich ein Blick in den tatsächlichen NifSkope-Parsercode, nicht nur die deklarative XML.
  ABER: dieser custom Engine-Fork weicht nachweislich mehrfach von ALLEN drei Referenzen ab
  (`NiPixelData`, `ParseNiTriStripsHeader`, jetzt vermutlich auch `NiPSysEmitter`) - jede
  Referenz-Übernahme weiterhin zwingend gegen den vollen Massentest verifizieren.
- **Ältere NIF-Version (10.1.0.0/10.2.0.0, 177 Dateien) vollständig unterstützen** - der
  Header ist bereits gelöst (Abschnitt 15), aber `NiSourceTexture` und vermutlich weitere
  Blocktypen brauchen eine eigene, von 20.0.0.4 unabhängige Untersuchung dieser älteren
  Format-Ära. Guter Kandidat für eine eigene Session mit frischem Zeitbudget, da potenziell
  177 Dateien auf einmal profitieren würden.
- Seltenere Partikel-Modifier ergänzen: `NiPSysMeshEmitter`, `NiPSysMeshUpdateModifier`,
  `NiPSysModifierActiveCtlr`, `NiPSysGravityStrengthCtlr` (je 1-6 Dateien betroffen)
- **`key_type=0` bei `KeyGroup` (NUR NiPosData ~28 + NiFloatData, NICHT NiBoolData/
  NiColorData - siehe Korrektur oben) - IMMER mit `num_keys=2`.** Eine plausible, aber
  unverifizierte 10-Floats/Key-Hypothese liegt bereits vor (siehe docs/MAP_FORMAT.md
  Abschnitt 46, Nachtrag) - nächster Schritt wäre, sie zuerst an 2-3 weiteren Dateien gegen
  `LooksLikeTriDataHeader` bzw. den jeweils folgenden Blocktyp zu prüfen, dann erst per
  vollem Massentest testen (NICHT vorher übernehmen).
- Die verbleibenden ~9 `NiLODNode`-Dateien untersuchen
- `NiTriShapeData`/`NiTriStripsData` mit `num_vertices=0` untersuchen (Punkt 3, ~26 Dateien)
- `NiStencilProperty`s 7 unbekannte uint32-Felder inhaltlich entschlüsseln (nicht kritisch,
  und das ANGRENZENDE letzte-Feld-Problem ist jetzt als Dead End dokumentiert, siehe oben)

## Kontextdateien für diesen Chat
Siehe separate Nachricht/Anhänge - im Kern: der aktuelle Code-Stand (dieses Zip) plus die
echten Referenzdateien (Rou.* Kernset, MapLearnFiles.zip für Adl/Bera/Eld/RouVal01, optional
die 5 resmap-Zips für weitere `.nif`-Massentests - das komplette `Rou.*`-Kernset war zuletzt
zufällig bereits in `resmap__3_.zip` unter `resmap/field/Rou/` enthalten, falls es nicht
separat beiliegt). Die resmap-Zips enthalten auch echte Textur-Dateien (DDS/BMP) - nützlich für
End-to-End-Texturierungs-Verifikation, siehe docs/MAP_FORMAT.md Abschnitt 5/6.


## UI-Vision (verbindlich, 25.09.2026)

Die visuelle Zielrichtung ist unter `docs/ui-vision/README.md` versioniert. Bei UI-Änderungen dort zuerst Branding, Theme-Tokens, Default-Layout, Komponenten, Icons und Modul-Sollbilder prüfen. Ziel: dunkler, cyan/blau akzentuierter Premium-World-Editor; keine lokalen Ad-hoc-Stile, die die Vision verwässern.
