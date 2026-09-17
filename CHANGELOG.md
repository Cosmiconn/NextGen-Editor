# Changelog — TheSeed Map-Editor

## [0.44.7] — Datei-Picker für Texturen/NIF-Modelle + feinerer Block/Walk-Pinsel

**Textur- und Modell-Auswahl:** Neuer "Durchsuchen..."-Knopf neben dem Diffuse-Feld
(Texturing-Panel) und dem Modellpfad-Feld (Object-Placement-Panel) - öffnet ein filterbares
Auswahl-Popup statt manueller Pfadeingabe. Sucht automatisch im `fieldTexture`- bzw.
`nif`/`nifs`-Ordner unterhalb des per Client-Ordner gefundenen `resmap` (inkl. Unterordner,
begrenzt rekursiv). Dateilisten werden beim Öffnen des Popups einmalig gescannt und gecacht
(nicht pro Frame - bei tausenden `.nif`-Dateien spürbar teuer). Mit simulierten Testdaten
verifiziert (verschachtelte Unterordner, `nif` singular wie vom Nutzer angegeben).

**Feinerer Block/Walk-Pinsel:** Radius-Schieberegler von 10-1000 auf 1-1000 abgesenkt, jetzt
logarithmisch skaliert (kleine Werte lassen sich dadurch deutlich präziser einstellen als bei
linearer Skalierung über einen so großen Bereich). Neuer "1 Zelle"-Knopf setzt den Radius
exakt auf die halbe Diagonale einer Walk-Gitterzelle - trifft garantiert nur die
nächstgelegene Zelle, unabhängig von der (nicht quadratischen) Zellgröße.

Nur syntaktisch geprüft bzw. an simulierten Daten getestet (kein echter GL-Renderer in dieser
Sandbox) - bitte nach dem nächsten Build Rückmeldung geben.

## [0.44.6] — Zwei echte Renderfehler behoben: Diffuse-Textur-Flip zurückgenommen +
    NIF-Objekt-Texturen finden jetzt den "fieldTexture"-Ordner

**Map-Textur oben/unten vertauscht:** Nutzer-Vergleich zweier Screenshots (2D View mit
rotem Block/Walk-Overlay vs. Nahaufnahme derselben Steintextur) zeigte: das Block/Walk-
Overlay ist korrekt ausgerichtet, aber die Diffuse-Textur weiterhin vertauscht - trotz des
in einer früheren Sitzung eingeführten V-Flips. Da Block/Walk dieselbe `mapUv`-Achse wie
Blend nutzt (beide korrekt), war dieser Flip die falsche Richtung. Zurückgenommen
(`Renderer.cpp`, `SampleLayer`): Diffuse-UV nutzt jetzt `mapUv` direkt, ohne V-Flip, wie
Blend/Block/Walk auch. DDS-Decoder selbst geprüft und für korrekt befunden (liest Zeilen
originalgetreu top-down, kein Flip nötig) - die Ursache lag ausschließlich in der
UV-Zuordnung.

**NIF-Objekte ohne Textur:** `.ini`-Pfade (Heightmap, Textur-Set) enthalten immer den vollen
`resmap\field\<Karte>\...`-Pfad, aber Objekt-Texturen aus `.nif`-Dateien sind oft NUR ein
nackter Dateiname ohne Verzeichnisangabe (z.B. `"ELDERIN_wg.DDS"`, byte-exakt bestätigt).
`ResolveLegacyAssetPath` sucht bei solchen Ein-Komponenten-Pfaden jetzt zusätzlich gezielt im
`fieldTexture`-Ordner (auch in dessen Unterordnern, begrenzt rekursiv). Zusätzlich toleriert:
manche Original-Dateinamen haben ein Leerzeichen MITTEN im Namen vor der Endung (z.B.
`"road01_lamp .dds"`) - wird jetzt beim Vergleich ignoriert. Mit simulierten Testdaten
verifiziert (echte `.dds`-Dateien in dieser Sandbox nicht vorhanden).

Beide Fixes nur syntaktisch geprüft bzw. an simulierten Daten getestet (kein echter
GL-Renderer in dieser Sandbox) - bitte nach dem nächsten Build erneut Rückmeldung geben.

## [0.44.5] — Mehrere gleichnamige "resmap"-Ordner möglich - wählt jetzt den mit echten
    Kartendaten statt blind den ersten Treffer

Rückmeldung: die Suche zeigte "Client/reschar/resmap" an, was es laut Nutzer nicht gibt bzw.
keine echten Karten enthält. Ursache: es kann MEHRERE Ordner namens "resmap" geben (z.B.
einen leeren/unbenutzten an anderer Stelle im Client-Baum) - die bisherige Suche
(`FindResmapFolder`) stoppte beim ERSTEN Treffer, unabhängig davon, ob er echte Kartendaten
enthielt.

Neu: `FindAllResmapCandidates` sammelt JETZT ALLE Ordner namens "resmap" (bis zu drei Ebenen
tief), `ResolveMapSearchRootAndScan` scannt JEDEN Kandidaten und wählt den mit den MEISTEN
gefundenen Karten. Mit echten Testdaten verifiziert (nicht nur `-fsyntax-only`): ein
künstlicher leerer "reschar/resmap"-Ordner neben dem echten, 116 Karten enthaltenden
"resmap"-Ordner wurde korrekt zugunsten des echten übergangen. Bei mehreren Kandidaten zeigt
die Oberfläche jetzt zusätzlich an, wie viele gefunden wurden.

## [0.44.4] — Kartensuche gegen echte Referenzdaten verifiziert (nicht nur syntaktisch
    geprüft) + strengerer .ini-Filter

Die Scan-Logik (`FindResmapFolder`/`ResolveMapSearchRoot`/`ScanForMaps`) braucht kein
GLFW/ImGui/Windows - reines `std::filesystem`. Deshalb erstmals ECHT kompiliert und gegen
116 reale Kartenverzeichnisse (`resmap/field/<Map>/<Map>.ini`, `resmap/IDField/<Map>/<Map>.ini`
aus einem früheren Sitzungs-Korpus) laufen lassen, nicht nur `-fsyntax-only` geprüft. Ergebnis:
alle 116 Karten korrekt gefunden, über beide Kategorie-Ordner hinweg.

Zusätzlich verschärft: `ScanForMaps` akzeptiert jetzt nur noch `.ini`-Dateien, deren Name
(ohne Endung) exakt mit dem Namen ihres eigenen Ordners übereinstimmt
(`<MAPORDNER>/<MAPORDNER>.ini`) - schließt versehentliche Treffer bei anderen,
nicht zu einer Karte gehörenden `.ini`-Dateien aus. Karten-Liste zeigt beim Überfahren mit
der Maus jetzt den vollen gefundenen Pfad als Tooltip (Diagnose ohne Karte öffnen zu müssen).

## [0.44.3] — Kartensuche fand fälschlich "ressystem" statt "resmap" - jetzt gezielte,
    mehrstufige Suche ohne Fallback auf den ganzen Client-Ordner

Rückmeldung mit Screenshot: "resmap" lag beim Nutzer nicht direkt im gewählten Client-Ordner,
wodurch die bisherige Logik (v0.44.2) stillschweigend auf eine Volltextsuche im kompletten
Client-Baum zurückfiel und dabei zwei `.ini`-Dateien aus fremden "ressystem"-Ordnern fand
(falsche Treffer, keine echten Karten).

Neu: `FindResmapFolder` sucht gezielt nach einem Ordner namens "resmap" (Groß-/Kleinschreibung
egal) - direktes Kind des Client-Ordners zuerst, sonst bis zu zwei Ebenen tiefer (deckt
Distributionen ab, bei denen "resmap" in einem Unterordner liegt). Steigt dabei bewusst NICHT
in "ressystem" oder "fieldTexture" ab. `ResolveMapSearchRoot` gibt jetzt `std::optional` zurück
und liefert `std::nullopt`, wenn kein "resmap" gefunden wurde - **kein Fallback auf den
gesamten Client-Ordner mehr**. Die Karten-Browse-Ansicht zeigt bei Nichtfund jetzt klar
"'resmap' nicht gefunden" statt falscher Treffer.

## [0.44.2] — Erste echte Rückmeldung nach Build: Panel-Umschaltung + Ordnerdialog/Kartensuche
    repariert

Nach dem ersten tatsächlichen Build/Start (danke für die Rückmeldung!) zwei konkrete Bugs
behoben:

1. **"New Map"/"Map Öffnen" taten nichts.** Beide Panels waren immer gleichzeitig sichtbar,
   die Knöpfe hatten keinen Klick-Handler. Jetzt schaltet `state.mapLauncherView` (neu in
   `EditorState`) zwischen den beiden Panels um, die Knöpfe sind aktiv hervorgehoben.
2. **Karten unter `<Client>/resmap/...` wurden nicht gefunden, Ordnerdialog zeigte den
   Inhalt nicht zuverlässig an.** Zwei Ursachen behoben:
   - `BrowseForFolderWindows` nutzte die veraltete `SHBrowseForFolder`-API (bekannt für
     unzuverlässige Inhaltsanzeige/Navigation) - ersetzt durch die moderne
     `IFileOpenDialog`-COM-API (echtes Explorer-Fenster), jetzt zusätzlich als Kind-Fenster
     des Hauptfensters verankert (`glfwGetWin32Window`), damit er nicht dahinter verschwinden
     kann.
   - NEU `ResolveMapSearchRoot`: "Client Ordner" ist der Client-WURZELordner (enthält
     `resmap`, nicht direkt die Karten) - wird jetzt automatisch erkannt und durchsucht.
     Rescan-Trigger repariert (vorher: nur beim allerersten Aufruf, ein leeres Ergebnis löste
     nie einen erneuten Scan nach Ordnerwechsel aus). "Browse Map's" zeigt jetzt den
     tatsächlich durchsuchten Pfad + Trefferzahl an und hat einen "Neu durchsuchen"-Knopf,
     statt bei 0 Treffern rätselhaft leer zu bleiben.

**Weiterhin nur syntaktisch geprüft** (Linux-Sandbox, kein Windows verfügbar) - die
`IFileOpenDialog`-Änderung liegt komplett hinter `#ifdef _WIN32` und konnte hier NICHT
kompiliert werden (nur sorgfältig gegen die bekannte COM-API-Signatur geprüft). Bitte nach
dem nächsten Build erneut Rückmeldung geben.

## [0.44.1] — Korrektur: Klebezettel-Notizen aus den Mockups waren Umsetzungs-Hinweise, keine
    UI-Elemente

Auf Rückfrage klargestellt: die gelben Notizzettel in den Mockup-Bildern ("Noch nicht
entschieden", Projekt-Ordner-Erklärung, Dateiformat-Hinweis, Tools-Spalten-Hinweis) waren als
Kontext für die Umsetzung gedacht, nicht als sichtbare Bestandteile der fertigen Oberfläche.
Wieder entfernt aus `DrawEditorCard` (Karten-Notiz-Box), `DrawNewProjectConfig`,
`DrawMapEditorLauncher` und `DrawMapEditorWorkspace`. Die dahinterliegende Bedeutung bleibt
funktional erhalten (z.B. zeigt die Tools-Spalte weiterhin je nach Tab unterschiedliche
Werkzeuge). Tote Übersetzungsschlüssel in `Localization.hpp` entsprechend entfernt.

## [0.44.0] — GUI-Umbau nach Mockup-Vorgabe ("NextGen-Editor"): neue mehrstufige Navigation
    (Projekt-Hub, Projekt-Konfiguration, Map-Editor-Start, Arbeitsbereich) + Lokalisierung

Komplette Neustrukturierung der Oberfläche nach zwei vom Nutzer bereitgestellten Mockups.
Ersetzt das bisherige, immer sichtbare Andock-Fenster-Layout durch einen Bildschirm-
Zustandsautomaten (`AppScreen`) mit vier Stufen:

- **Projekt-Hub**: 6 Editor-Karten (MapEditor, SHN Editor, Quest Editor, Interface Editor,
  Drop Table, Skill+Action) mit vektoriellen Icons, Feature-Listen, Notizzetteln. Nur
  MapEditor ist funktional, der Rest führt zu einem "Noch nicht implementiert"-Platzhalter.
- **Neues Projekt konfigurieren**: Projekt Name/Ordner, Client-/Server-Ordner, speichert
  eine `project.tsproj`-Konfigurationsdatei.
- **Map-Editor-Start**: "Create New Map" (freie Maße statt fixem 257x257) + "Browse Map's"
  (sucht jetzt im Projekt-Client-Ordner).
- **Map-Editor-Arbeitsbereich**: Tab-Leiste (Hightmap/Texturing/Block-Walk/Objects/NPCs/
  NPC AI/Mobs/Mob AI) statt Radio-Buttons, Drei-Spalten-Layout (Datei+Tools / 2D View /
  3D View mit Zoom-Knöpfen). Komplette bisherige Import/Export-Funktionalität bleibt unter
  einem einklappbaren "Erweitert"-Bereich erhalten.

NEU: `include/mapeditor/app/Localization.hpp` - deutsch-zuerst, englisch bereits als
Sprachumschaltung eingebaut (Dropdown oben rechts).

**Nur syntaktisch geprüft** (`-fsyntax-only` gegen echte imgui/GLFW-Header + selbstgeschriebenen
glad-Stub, 0 Fehler/Warnungen) - kein echter Build/Lauf in dieser Sandbox möglich. Siehe
HANDOFF.md für den vollständigen Umfang und die Einschränkung.

## [0.43.1] — NiLookAtInterpolator komplett neu implementiert (+4 Dateien, 76.3%)

Bisher komplett unimplementierter Blocktyp (`H_AIRDOLL.nif`, 4 identische Kopien). Struktur
aus der autoritativen `nif.xml`: `flags(u16)` + `look_at_ref(i32, Ptr auf NiNode)` +
`look_at_name(SizedString)` + `NiQuatTransform` (`translation(12)+rotation(16)+scale(4)` = 32
Byte - "TRS Valid" entfällt seit Version 10.1.0.109, betrifft unsere 20.0.0.4 nicht) + 3
weitere Interpolator-Refs (Translation/Roll/Scale, je i32).

Byte-exakt an `H_AIRDOLL.nif` Block 144 verifiziert: Rotation ist ein exakter
Einheits-Quaternion, Translation/Scale beide `-FLT_MAX` (bekannter NIF-Sentinelwert),
`look_at_ref` zeigt exakt auf das nächste `NiNode`, alle 3 Interpolator-Refs sauber `-1` -
die berechnete Blocklänge landet exakt auf dem lesbaren Namensfeld `"Camera01.Target"` des
folgenden `NiNode`.

Massentest **2618 → 2622/3436 (+4 Dateien, 76.3%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 47.

## [0.43.0] — DURCHBRUCH: vollständige ObjectNetBase-Kettenvalidierung erkennt benannte Objekte (+128 Dateien, zweitgrößter Einzelfund der Session, 76.2%!)

An `Tree01.nif` gefunden: nach NiPixelData folgt ein BENANNTES NiNode - der bisherige 4-Byte-
Trailer-Peek erkennt nur leere Namen. Anders als der in v0.32.1 gescheiterte
`LooksLikeFreshName()`-Versuch (nur das Namensfeld geprüft, katastrophaler Rückschritt
1818→363) validiert die neue Prüfung die GESAMTE ObjectNetBase-Kette: plausibler Name (1-40
Zeichen, druckbar) UND plausibles numExtra (0-10) UND plausibler controller (-1 oder 0-300) -
alle gleichzeitig. Diese Kombination ist weit seltener zufällig erfüllt als eine bloße
Namensform.

Byte-exakt an Tree01.nif verifiziert und stichprobenartig auf konsistente Geometrie geprüft
(4 Teile, Vertex=Normalen=UV-Anzahl). Voller Regressionstest (7/7 Suiten) grün.

Massentest **2490 → 2618/3436 (+128 Dateien, 76.2%!)**. Siehe docs/MAP_FORMAT.md
Abschnitt 45.

## [0.42.2] — NiTexturingProperty: sicherer Rückfallversuch bei CountU32-Überschreitung (keine neuen Dateien, aber wichtige Absicherung)

An `adel_terrain_root_town.nif` gefunden: peek=8 wurde fälschlich als 8 echte Extra-Daten-
Refs gelesen (alle zufällig <100000, bestehen die generelle Prüfung), texture_count landete
bei ~1,6 Milliarden. Anders als der in v0.40.1 verworfene "texture_count==0"-Versuch (schwaches
Signal) wird hier auf tatsächliche CountU32-Überschreitung geprüft (starkes, praktisch nie
zufälliges Signal). Neue `ByteReader::SetOk()`-Methode ermöglicht den gezielten Rückfall.

Massentest bleibt bei 2490/3436 (kein weiterer Treffer im aktuellen Korpus), aber echte,
bewiesene Korrektur ohne Regression. Siehe docs/MAP_FORMAT.md Abschnitt 44.

## [0.42.1] — NiTriShapeData: mysteriöses Feld entfällt bei älteren Versionen komplett (+22 Dateien, 72.5%)

Beim Verfolgen eines NiSkinInstance-Fehlers (`horse2.nif`/`horse3.nif`, Version 10.2.0.0)
gefunden: anders als bei NiTriStripsData (wo nur additional_data_ref bei älteren Versionen
entfällt) fehlt bei NiTriShapeData das "mysteriöse" u16-Feld nach den UV-Daten komplett.
Byte-exakt verifiziert: ohne das Feld ergeben sich consistency_flags=0x4000 (gültig),
num_triangles=700, num_triangle_points=2100 (exakt num_triangles*3).

Massentest **2468 → 2490/3436 (+22 Dateien, 72.5%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 43.

## [0.42.0] — DURCHBRUCH: das +4-Byte-Muster generalisiert (+411 Dateien, MIT ABSTAND größter Einzelfund der Sitzung, 71.8%!)

Nach den Funden bei NiZBufferProperty/NiAlphaProperty/NiVertexColorProperty/
NiSpecularProperty (je +4 Byte vor direkt folgender NiTriStripsData/NiTriShapeData) wurde
bestätigt: auch NiFloatData als Vorgänger braucht dasselbe Muster - das Problem hängt NICHT
vom spezifischen Vorgänger-Blocktyp ab.

**Generalisierte Lösung statt weiterer Einzelaufzählung:** Eine neue Hilfsfunktion
(`LooksLikeTriDataHeader`) prüft DIREKT am Zielblock (NiTriStripsData/NiTriShapeData), ob
die aktuelle Position wie ein gültiges num_vertices/keep_flags/compress_flags/has_vertices-
Muster aussieht - unabhängig davon, was davor stand. Wenn nicht, aber 4 Byte weiter schon,
wird dort weitergelesen.

Volle Regression (7/7 Suiten) weiterhin grün, alle bisherigen Referenzdateien unverändert
korrekt. Mehrere neu erfolgreiche Dateien stichprobenartig auf konsistente Geometrie
geprüft (Vertex=Normalen=UV-Anzahl, sinnvolle Dreieckszahlen).

Massentest **2057 → 2468/3436 (+411 Dateien, 71.8%!)**. Siehe docs/MAP_FORMAT.md
Abschnitt 42.

## [0.41.2] — NiMorphData ergänzt + Diagnose-Runde (keine neuen Dateien, aber vollständige Klärung mehrerer Fälle)

`NiMorphData` aus der Referenz ergänzt (byte-exakt verifiziert an `zzz_kong.nif`). Zusätzlich
zwei weitere Fälle vollständig diagnostiziert: `LegelDungeon.nif` als weitere Instanz des
bekannten `NiPixelData`-Trailer-Sonderfalls (Abschnitt 20) bestätigt; `KDVictor.nif`s
`NiCollisionData` als korrekt verifiziert, Fehlausrichtung liegt tiefer in
`NiTriStripsData` selbst (offen für Folgesession). `NiBoneLODController` geprüft, aber wegen
Komplexität für nur 1 Datei nicht implementiert.

Massentest bleibt bei 2057/3436 (59.9%). 7/7 Test-Suiten weiterhin grün, keine Regression.
Siehe docs/MAP_FORMAT.md Abschnitt 41.

## [0.41.1] — Offener Faden einzeln durchgetestet: 3 von 5 Eigenschaftstypen sicher (+16 Dateien, 59.9%)

Die fünf in v0.41.0 zurückgenommenen Eigenschaftstypen einzeln (nacheinander mit vollem
Massentest) erneut getestet: `NiVertexColorProperty` (+14, sicher), `NiStencilProperty`
(-24, verursacht Schaden - vermutlich wegen eigener interner Freitextfeld-Mehrdeutigkeit,
sofort zurückgenommen), `NiSpecularProperty` (+2, sicher), `NiFogProperty` und
`NiDitherProperty` (je ±0, neutral aber unschädlich, übernommen).

Massentest **2041 → 2057/3436 (+16 Dateien, 59.9%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 40.

## [0.41.0] — NiAlphaProperty-Fund: größter Einzelfund dieser Session (+62 Dateien, 59.4%)

Dasselbe "+4 Byte vor NiTriStripsData/NiTriShapeData"-Muster wie bei `NiZBufferProperty`
(v0.40.2), aber diesmal bei `NiAlphaProperty` UND unabhängig von der NIF-Version bestätigt
(`Leviathan_lightA_non.nif`, Version 20.0.0.4). Generalisierte Hilfsfunktion
`SkipExtraBytesIfFollowedByTriData` ergänzt und bei `NiAlphaProperty` angewendet.

Ein Versuch, dieselbe Prüfung auch auf `NiVertexColorProperty`, `NiStencilProperty`,
`NiSpecularProperty`, `NiFogProperty`, `NiDitherProperty` anzuwenden, verursachte einen
Netto-Rückschritt (-8 Dateien) und wurde sofort komplett zurückgenommen - offen für eine
Folgesession, die jeden Typ einzeln testet.

Massentest **1979 → 2041/3436 (+62 Dateien, größter Einzelfund dieser Session, 59.4%)**.
7/7 Test-Suiten weiterhin grün, keine Regression. Siehe docs/MAP_FORMAT.md Abschnitt 39.

## [0.40.2] — Offener Faden gelöst: NiZBufferProperty vor NiTriStripsData/NiTriShapeData braucht 4 Byte (+3 Dateien, 57.6%)

Der Abschnitt-37-Faden bestätigt an einer zweiten, unabhängigen Datei (`field_sky_01.nif`,
identische Werte `flags=1/function=3`). Ein unbedingter Test (immer +4 Byte) verursachte
eine Regression (-3 Dateien) und wurde verworfen - stattdessen eng auf die Nachbarschaft zu
`NiTriStripsData`/`NiTriShapeData` begrenzt (analog zum NiPixelData-Trailer-Muster).

Massentest **1976 → 1979/3436 (+3 Dateien, 57.6%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 38.

## [0.40.1] — Dritter NiPixelData-Trailer-Fall: manchmal 0 statt 4/8 Byte (+2 Dateien)

Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: nach `NiPixelData` kann manchmal
GAR KEIN Trailer nötig sein. Als zusätzliche, vor den bestehenden Fällen geprüfte Bedingung
ergänzt. Massentest **1974 → 1976/3436 (+2 Dateien, 57.5%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression.

Offener Faden dokumentiert (nicht implementiert): `NiZBufferProperty` gefolgt von
`NiTriStripsData` scheint in einem Fall 4 zusätzliche Byte zu brauchen - nur ein Beleg,
nicht ausreichend verifiziert für eine Regel. Siehe docs/MAP_FORMAT.md Abschnitt 37.

## [0.40.0] — Additional-Data-Feld entfällt bei älteren Versionen (+41 Dateien, größter Einzelfund seit ShaderTexDesc, 57.5%)

Direkte Fortsetzung von v0.39.0: `NiGeometryData`s "Additional Data"-Ref-Feld ist laut
autoritativer Referenz erst `since="20.0.0.4"` vorhanden - bei älteren Versionen
(10.1.0.0/10.2.0.0) entfällt es komplett. Bisher unbedingt gelesen, dadurch bei jeder
NiTriStripsData/NiTriShapeData in älteren Dateien eine 4-Byte-Fehlausrichtung.

Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: ohne das Feld ergeben sich
plausible `consistency_flags`, eine korrekte Streifenlänge (passt exakt zur
Dreieckszahl+2) und eine klassische Dreiecksstreifen-Indexfolge - landet danach exakt auf
einem gültigen Blockanfang.

`ParseNiTriStripsData`, `ParseNiTriShapeData`, `SkipNiGeometryDataHeader` erhalten einen
`isOlderVersion`-Parameter. Massentest **1933 → 1974/3436 (+41 Dateien, 57.5%)**. 7/7
Test-Suiten weiterhin grün, keine Regression trotz Änderung an den meistgenutzten
Geometrie-Parsing-Funktionen. Siehe docs/MAP_FORMAT.md Abschnitt 36.

## [0.39.0] — DURCHBRUCH bei älteren NIF-Versionen: drei zusammenhängende Funde (+31 Dateien, 56.3%)

Ausgehend von `skeleton_monolith_blood.nif` (Version 10.2.0.0, per `strings` identifiziert)
drei zusammenhängende, version-spezifische Strukturabweichungen gefunden und behoben:

1. **`TexDesc` hat zwei zusätzliche PS2-Felder** (`PS2 L`, `PS2 K`, je short) vor Version
   10.4.0.1 - bei 20.0.0.4 bereits entfallen. `ParseNiTexturingProperty` erhält einen
   `hasPS2Fields`-Parameter.
2. **`NiPixelData`s Kopfstruktur ist für ältere Versionen 50 Byte** (nicht 72 wie bei
   20.0.0.4, auch nicht die von nif.xml beschriebenen 36/58 Byte - noch eine
   Custom-Engine-Abweichung). Durch Mipmap-Ketten-Suche empirisch gefunden und byte-exakt
   verifiziert. `SkipNiPixelData` erhält einen `isOlderVersion`-Parameter.
3. **Der 4-vs-8-Byte-Trailer-Peek nach `NiPixelData` versagt strukturell**, wenn der nächste
   Block kein Namensfeld hat (`NiTriStripsData`/`NiTriShapeData`) - als zusätzliche,
   ergänzende Bedingung (nur für ältere Versionen) implementiert. **Der größte Einzelfund
   dieser Serie: +30 Dateien.**

Massentest **1902 → 1933/3436 (+31 Dateien, 56.3%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression trotz Änderungen an mehreren zentralen Funktionen. Siehe docs/MAP_FORMAT.md
Abschnitt 35.

## [0.38.2] — Zwei wichtige Gegenproben gegen die autoritative Referenz dokumentiert (keine Funktionsänderung, weiterhin 1902/3436)

**NiPixelData:** Kopf-Struktur weicht von BEIDEN nif.xml-Versionsvarianten ab (weder die
alte "until 10.4.0.1" noch die neue "seit 10.4.0.2" Variante ergeben plausible Werte an
`BerFrz01_Ice02.nif`) - die bereits empirisch funktionierende Eigenimplementierung bewusst
NICHT verändert.

**ParseNiTriStripsHeader:** ZWEITER bestätigter Beinahe-Katastrophenversuch. Die autoritative
Referenz beschreibt `has_shader` als echt konditional (wie in `SkipNiParticleSystem` bereits
korrekt behandelt) - byte-exakt an `Leviathan_deco1.nif` verifiziert (has_shader=0,
data_ref=15 korrekt). Trotzdem verursachte die Umsetzung einen KATASTROPHALEN Rückschritt
(1902 → 1486!) - sofort zurückgenommen. Dieser custom Engine-Fork weicht auch hier von der
Vanilla-Spezifikation ab. `ParseNiTriStripsHeader` bleibt ENDGÜLTIG OFF LIMITS.

Beide Funde ausführlich in docs/MAP_FORMAT.md Abschnitt 33/34 dokumentiert, um wiederholte
Versuche zu vermeiden. Massentest weiterhin 1902/3436, 7/7 Test-Suiten grün, keine Regression
im ausgelieferten Code.

## [0.38.1] — NiGeometryData-Kernstruktur bestätigt + latenter Tangenten-Bug behoben (netto keine neuen Dateien, aber wichtige Absicherung)

Die zentrale `NiGeometryData`-Struktur gegen die autoritative `nif.xml` geprüft: Feldreihenfolge
bestätigt (keine Änderung nötig). Dabei einen latenten Bug gefunden: das "Data Flags"-Feld
wurde mit einem zu engen Plausibilitäts-Cap (`CountU16(16u)`) gelesen, der Dateien mit
gesetztem Tangenten-Bit (Bit 12) fälschlich abgelehnt hätte. Cap entfernt, Tangenten/
Binormalen-Handling ergänzt.

Massentest bleibt bei 1902/3436 (aktueller Korpus scheint keine Tangenten zu nutzen), aber
wichtige Absicherung gegen zukünftige Dateien. 7/7 Test-Suiten weiterhin grün, keine
Regression trotz Änderung an der meistgenutzten Parser-Funktion. Auch bestätigt: `key_type=0`
bei `NiPosData` ist laut autoritativer Referenz (`KeyType`-Enum: nur Werte 1-5 definiert)
genuinely ungültig - betrifft 38 Kopien derselben `EnvSet.nif`-Datei quer über viele
Zonen-Ordner. Siehe docs/MAP_FORMAT.md Abschnitt 32.

## [0.38.0] — ShaderTexDesc implementiert (+34 Dateien, größter Einzelfund seit dem nif.xml-Durchbruch, 55.4%)

`NiTexturingProperty`s `num_shader_textures` führte bisher bei jedem Nicht-Null-Wert zu
sauberem Abbruch ("nie in Testdaten beobachtet"). Jetzt implementiert: `ShaderTexDesc` =
`has_map(bool)` + `[map(TexDesc) + map_id(u32)]`. Byte-exakt an `bossroom_wall.nif`
verifiziert (3 Shader-Texturen, source_refs 11/13/15, map_id 0/1/2 - alles plausibel).

Zusätzlich `NiGeomMorpherController` aus der Referenz ergänzt.

Massentest **1868 → 1902/3436 (+34 Dateien, 55.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 31.

## [0.37.0] — NiPSysDragModifier, NiPortal ergänzt + NiTexturingProperty-Korrekturen (+9 Dateien, 54.4%)

`NiPSysDragModifier` und `NiPortal` aus der Referenz ergänzt (+8 Dateien). Dazu
`NiTexturingProperty` korrigiert: hatte eine eigene, veraltete Kopie der
`num_extra_data_refs`-Peek-Logik statt `ParseObjectNetBase()` zu nutzen (an
`AdlF_field_burn_ground.nif` gefunden) - dazu die fehlenden 24 Byte des Bump-Map-Texturslots
(Index 5) ergänzt (+1 Datei).

Ein zusätzlicher Rückfallversuch bei `texture_count==0` wurde getestet, aber wegen eines
Netto-Rückschritts (-1 Datei) sofort verworfen und dokumentiert (siehe docs/MAP_FORMAT.md
Abschnitt 30) - ein weiteres Beispiel für die Grenzen von Peek-Heuristiken.

Massentest **1859 → 1868/3436 (+9 Dateien, 54.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression.

## [0.36.1] — Wiring-Fehler behoben (NiPointLight) + fünf weitere Blocktypen (+5 Dateien, 54.1%)

**Wichtiger Fund:** `NiPointLight` aus v0.36.0 war implementiert, aber NIE in die Dispatch-
Weiche eingebunden - blieb dadurch wirkungslos. Nach Einbindung systematisch alle anderen
neuen Typen aus v0.36.0 auf korrekte Einbindung geprüft (keine weiteren Lücken).

Zusätzlich ergänzt: `NiBoolTimelineInterpolator` (= NiBoolInterpolator), `NiRoom`,
`NiPSysPlanarCollider` (neue Basis NiPSysCollider entdeckt - keine NiObjectNET-Basis!),
`NiPSysEmitterLifeSpanCtlr` (= NiPSysModifierActiveCtlr).

Massentest **1854 → 1859/3436 (+5 Dateien, 54.1%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 29.

## [0.36.0] — DURCHBRUCH: autoritative nif.xml-Referenz direkt geladen (+10 Dateien, 54.0%)

### Effizienzgewinn
Die offizielle `niftools/nifxml`-Referenzdatei (nif.xml, 8563 Zeilen) direkt von
`raw.githubusercontent.com` heruntergeladen (auf der Netzwerk-Allowlist, anders als
`github.com` selbst) - ermöglicht präzises lokales Nachschlagen statt einzelner,
unvollständiger Web-Suchen.

### GROSSER FUND: NiTexturingProperty's num_shader_textures ist UNBEDINGT vorhanden
Widerlegt die Abschnitt-7/8-Regel ("nur bei controller_ref != -1") - laut autoritativer
Referenz gilt seit Version 10.0.1.0 keine Bedingung. Bedingung entfernt.
**Massentest 1849 → 1854 (+5 Dateien).**

### NiPSysMeshEmitter: empirischer Kompromiss durch exakte Struktur ersetzt
Der 244-Byte-Kompromiss aus v0.32.0 (dokumentiertes Restrisiko) war zudem durch
zwischenzeitliche andere Fixes bereits veraltet. Jetzt exakt: num_emitter_meshes + Refs +
initial_velocity_type + emission_type + emission_axis.

### Zwölf weitere Blocktypen ergänzt
`NiPointLight`, `NiSortAdjustNode`, `NiRoomGroup`, `NiPalette` (keine NiObjectNET-Basis!),
`NiVisController`, `NiPSysColliderManager`, `NiIntegersExtraData`,
`NiMultiTargetTransformController`, `NiPSysGravityStrengthCtlr`, `NiFogProperty`,
`NiDitherProperty`, `NiSourceCubeMap` - alle direkt aus der Referenz, mehrere mit
versionsabhängigen Feldern, die bei unserer Version (20.0.0.4) entfallen.
**Massentest 1845 → 1849 (+4 Dateien).**

### Gesamtergebnis
Massentest **1844 → 1854/3436 (+10 Dateien, 54.0%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 28.

## [0.35.0] — NiPSysModifierActiveCtlr + NiFlipController ergänzt (+2 Dateien, byte-exakt verifiziert)

`NiPSysModifierActiveCtlr` (30-Byte-Basis + modifier_name, byte-exakt: target zeigt exakt auf
NiParticleSystem, Name "NiPSysDragModifier(Z-Axis):10") und `NiFlipController` (30-Byte-Basis
+ texture_slot + Textur-Ref-Liste, byte-exakt: target zeigt exakt auf NiTexturingProperty,
30 Refs bilden eine regelmäßige Folge) ergänzt.

Massentest **1842 → 1844/3436 (+2 Dateien, 53.7%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 27.

## [0.34.0] — Vier weitere einfache NiExtraData-Varianten ergänzt (+6 Dateien, 53.6%)

`NiTextKeyExtraData` (byte-exakt verifiziert: lesbare Animationskommandos "start -name
idle01 ... -loop"/"end"), `NiFloatExtraData` (byte-exakt: Name "ambient", Wert 0.0),
`NiColorExtraData` (byte-exakt: Name "paramedgecolor", Wert (1,1,1,1)) und
`NiBooleanExtraData` (strukturell analog, nicht unabhängig verifiziert) ergänzt - alle
einfache, unzweideutige Erweiterungen der bereits vorhandenen `NiExtraData`-Basis.

Massentest **1836 → 1842/3436 (+6 Dateien, 53.6%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 26.

## [0.33.2] — Weiterer Gegenbeweis zur NiTexturingProperty num_shader_textures-Regel dokumentiert (keine Funktionsänderung, weiterhin 1836/3436)

An `Eff_2.nif` widerlegt: die Regel "`num_shader_textures` nur vorhanden wenn
`controller_ref != -1`" (Abschnitt 7/8) trifft hier nicht zu (`controller_ref=-1`, Feld
trotzdem vorhanden). Byte-exakt durch volle NiNode-Plausibilitätsprüfung verifiziert
(Rotationsmatrix, Skalierung, numProps). Ein `LooksLikeFreshName()`-Peek hätte hier KEINE
Unterscheidungskraft (beide Kandidatenpositionen sehen plausibel aus) - bewusst nicht per
Peek behoben, um keine dritte Beinahe-Katastrophe zu riskieren (siehe Abschnitt 20).
Ausführlich in docs/MAP_FORMAT.md Abschnitt 25 dokumentiert.

## [0.33.1] — NiPSysMeshUpdateModifier ergänzt (netto keine neuen Dateien, aber echte Erweiterung)

`NiPSysMeshUpdateModifier` laut Referenz eine einfache, unzweideutige Struktur
(`NiPSysModifierBase` + `num_meshes(u32)` + Ref-Liste) - direkt implementiert. Betroffene
Dateien (z.B. `Eff_2.nif`) kommen jetzt deutlich weiter (Block 74 → 93), scheitern aber an
einer unabhängigen Stelle (`NiBillboardNode`, bereits implementiert, aber durch
vorausgehende Fehlausrichtung betroffen - nicht weiter untersucht). Massentest bleibt bei
1836/3436, 7/7 Test-Suiten weiterhin grün, keine Regression. Siehe docs/MAP_FORMAT.md
Abschnitt 24.

## [0.33.0] — ParseObjectNetBase weiter generalisiert: num_extra_data_refs fehlt auch bei kleinen Controller-Werten (+18 Dateien, 53.4%)

### Gefunden und behoben
Der enge Fix aus v0.30.0 (nur `0xFFFFFFFF`) erkannte nicht, dass `num_extra_data_refs` auch
fehlen kann, wenn der Controller-Wert ein KLEINER, gültiger Block-Index ist (an
`AdlFH_field_burn_ground.nif` gefunden: Controller=6, fälschlich als "6 Extra-Daten-Refs"
interpretiert, deren Werte allesamt Float-Bitmuster waren). Generalisierter, aber weiterhin
konservativer Fix: bei einem potenziellen Zähler zwischen 1 und 1000 werden jetzt die
implizierten Extra-Refs UND der folgende Controller-Wert auf Plausibilität geprüft (jeweils
-1 oder ein Wert unter 100000) - nur bei fehlgeschlagener Prüfung wird das Feld als abwesend
behandelt.

### Ergebnis
Massentest **1818 → 1836/3436 (+18 Dateien, 53.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 23.

### Auch untersucht, ohne neue Fixes
Mehrere frische `NiTriStripsData`/`NiTriShapeData`-Fehlschläge zurückverfolgt - stellten sich
als weitere Instanzen der bereits dokumentierten, bewusst nicht behobenen `NiMaterialProperty`
14-vs-15-Float-Ambiguität heraus (Abschnitt 14), nur diesmal in beide Richtungen beobachtet
(zu wenig UND zu viele Floats je nach Datei). Bestätigt die bisherige Entscheidung, dies nicht
per Peek zu lösen.

## [0.32.2] — NiMeshPSysData: 17 Byte mehr als NiPSysData, byte-exakt (netto keine neuen Dateien, aber echte Korrektur)

`NiMeshPSysData` wurde bisher identisch zu `NiPSysData` behandelt. An `stone03.nif`
byte-exakt verifiziert: sie hat 17 zusätzliche Byte (u32+u8+u32+u32+i32, letzterer ein
gültiger Ref) vor dem nächsten Block. Massentest bleibt bei 1818/3436 (betroffene Dateien
scheitern an der bereits bekannten `NiPSysMeshEmitter`-Unsicherheit weiter hinten), aber dies
ist eine echte, bewiesene Korrektur (kein Kompromiss) - siehe docs/MAP_FORMAT.md Abschnitt
21. 7/7 Test-Suiten weiterhin grün, keine Regression.

## [0.32.1] — Gefährlicher Beinahe-Fund dokumentiert (keine Funktionsänderung, weiterhin 1818/3436)

An `BeraM_Wood3.nif` eine benannte `NiMaterialProperty` nach `NiPixelData` gefunden, die vom
bestehenden 4-vs-8-Byte-Trailer-Peek (der nur leere Objekte erkennt) falsch behandelt wird.
Ein Fix-Versuch mit dem sonst zuverlässigen `LooksLikeFreshName()` verursachte einen
KATASTROPHALEN Rückschritt (1818 → 363!) - sofort erkannt und zurückgenommen. Ursache: direkt
nach rohen Pixeldaten kommen "zufällig plausibel aussehende" kurze Namensfelder viel zu
häufig vor, damit eine reine Inhalts-Heuristik hier zuverlässig funktioniert. Ausführlich in
docs/MAP_FORMAT.md Abschnitt 20 dokumentiert, damit dieser Ansatz nicht wiederholt wird.

## [0.32.0] — NiPSysMeshEmitter: empirisch bester Kompromiss statt "nicht unterstützt" (+20 Dateien, mit dokumentiertem Restrisiko)

### Von Vorsicht zu kalkuliertem Fortschritt
Nach der Entscheidung in v0.31.1, `NiPSysMeshEmitter` NICHT zu raten, wurde die Untersuchung
auf alle 70 betroffenen Dateien ausgeweitet (statt nur 2). Ein systematischer Massentest-Scan
über Kandidaten-Skip-Längen (236-260 Byte) zeigte ein Plateau mehrerer gleichauf bester Werte
(1818/3436) statt eines einzelnen Optimums - ein klares Indiz, dass die wahre Feldlänge pro
Instanz variiert (`num_emitter_meshes`). Wert 244 gewählt.

### Bewusst dokumentiertes Restrisiko
Dies ist KEINE byte-exakt bewiesene Korrektur wie die übrigen Funde dieser Session, sondern
ein empirisch bester Kompromiss. Für Dateien mit abweichender tatsächlicher Länge sollte ein
sauberer Bounds-Check-Fehlschlag bei einem späteren Block folgen (die Werte selbst werden nie
gerendert) - ein kleines Restrisiko unbemerkt leicht verschobener Nachbargeometrie bleibt.
Ausführlich in docs/MAP_FORMAT.md Abschnitt 19 dokumentiert.

### Ergebnis
Massentest **1798 → 1818/3436 (+20 Dateien, 52.9%)**. 7/7 Test-Suiten weiterhin grün - der
Wert stieg über den gesamten getesteten Kandidatenbereich nie, sondern wurde durchgängig
gleich oder besser.

## [0.31.1] — NiPSysMeshEmitter untersucht, keine sichere Lösung gefunden (keine Funktionsänderung, weiterhin 1798/3436)

`NiPSysMeshEmitter` ist nach den Funden in v0.29.0-v0.31.0 der größte verbleibende Blocker
(70 Dateien). Empirische Untersuchung an 2 unabhängigen Dateien ergab eine Gesamtlängen-
Schätzung (~315 Byte), aber die vier laut Referenz (PyFFI) vorhandenen eigenen Felder lassen
sich damit nicht sauber (ganzzahlig) aufteilen - beide Testdateien sind zudem fast komplett
Null-gefüllt, was eine byte-exakte Verifikation verhindert. Bewusst NICHT implementiert, um
nicht für 70 Dateien unbemerkt falsch ausgerichtete Geometrie zu riskieren. Ausführlich in
docs/MAP_FORMAT.md Abschnitt 19 dokumentiert, inklusive eines konkreten Ansatzes für eine
Folgesession (Suche nach einer Datei mit nicht-trivialen Emitter-Werten als Anker).

## [0.31.0] — Dieselbe Ursache zweimal mehr gefunden: SkipNiStencilProperty + ParseNiMaterialProperty (+57 Dateien, 52.3%!)

### Gefunden und behoben
Gezielt nach weiteren Stellen gesucht, die `num_extra_data_refs` fälschlich als immer
abwesend annehmen (hartkodiertes `r.U32(); r.I32();` statt `ParseObjectNetBase()`):
- `SkipNiStencilProperty` - an `FighterDown.nif` gefunden, verschob nachfolgende Bytes um 4
- `ParseNiMaterialProperty` - dieselbe Ursache, sehr viel breiter wirksam (+42 Dateien allein)

Beide rufen jetzt `ParseObjectNetBase()` auf. Anschließend das gesamte restliche Codebase
nach ähnlichen Mustern durchsucht - keine weiteren Fundstellen mehr; alle übrigen
Property-Typen nutzten die gemeinsame Funktion bereits korrekt.

### Ergebnis
Massentest **1756 → 1798/3436 (+57 Dateien zusammen, 52.3%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression. Siehe docs/MAP_FORMAT.md Abschnitt 18.

### Gesamtbilanz dieser Sitzung (v0.29.0 - v0.31.0)
1676 → 1798/3436, **+122 Dateien, +3.5 Prozentpunkte** in einer einzigen Sitzung - die
ertragreichste seit dem ursprünglichen Trailer-Fund. Alle vier Funde derselben Grundursache
(num_extra_data_refs manchmal abwesend), aber jede Fundstelle brauchte eigene Untersuchung.

## [0.30.0] — num_extra_data_refs fehlt generell in ParseObjectNetBase (+26 Dateien, über 50%!)

### Gefunden und behoben
Direkt im Anschluss an v0.29.0 zeigte sich: dasselbe Phänomen (num_extra_data_refs fehlt)
tritt nicht nur bei `NiSourceTexture` auf, sondern generell in der gemeinsamen
`ParseObjectNetBase`-Funktion (verifiziert an `FighterDown.nif`: `NiVertexColorProperty` nach
einer `NiTriStrips`). Enger gefasster Fix als bei v0.29.0: nur wenn der Wert exakt
`0xFFFFFFFF` ist, wird er als controller statt als num_extra_data_refs behandelt - legitime
kleine Extra-Daten-Zähler bleiben unangetastet.

### Ergebnis
Massentest **1715 → 1741/3436 (+26 Dateien, 50.7%)** - zum ersten Mal über 50%. 7/7
Test-Suiten weiterhin grün, keine Regression trotz des sehr breiten Einsatzbereichs dieser
zentralen, von praktisch jedem Blocktyp verwendeten Funktion. Siehe docs/MAP_FORMAT.md
Abschnitt 17.

## [0.29.0] — NiSourceTexture: num_extra_data_refs fehlt bei aufeinanderfolgenden Instanzen (+39 Dateien!)

### Gefunden und behoben
49 Dateien haben im Blockindex zwei `NiSourceTexture`-Blöcke direkt hintereinander (z.B.
Haupttextur + "Dark Map" in derselben `NiTexturingProperty`). Bei der zweiten Instanz fehlt
das `num_extra_data_refs`-Feld komplett - byte-exakt durch Rückwärtsrekonstruktion von einer
eindeutig lesbaren Datei-Endung ("fence_dark.dds") bewiesen. Risikoarmer Peek-Fix: das Feld
wird nur konsumiert, wenn sein Wert 0 ist (was in JEDER bisher erfolgreich geparsten Datei
zutraf) - andernfalls übernimmt die bestehende Mystery-Feld/controller-Logik korrekt.

### Ergebnis
Massentest **1676 → 1715/3436** (+39 Dateien, +1.1 Prozentpunkte) - der bisher größte Fund
seit dem Trailer-Fix in einer früheren Session. 39 von 49 betroffenen Dateien laden jetzt
vollständig; die restlichen 10 scheitern an unabhängigen, bereits bekannten offenen
Sonderfällen weiter hinten in der jeweiligen Datei. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 16.

## [0.28.0] — Header-Unterstützung für ältere NIF-Versionen (10.1.0.0/10.2.0.0)

### Gefunden und behoben
177 Dateien (170x Version 10.2.0.0, 7x Version 10.1.0.0) scheiterten bisher am Header selbst.
Ursache: bei diesen älteren Versionen fehlt das Endian-Byte zwischen `version` und
`user_version`, das bei 20.0.0.4 vorhanden ist. Byte-exakt an 3 Dateien verifiziert
(`num_blocks` traf jeweils exakt mit dem separat abgelesenen Rohbyte-Wert überein).

### Bewusst nicht weiterverfolgt
Nach dem Header-Fix kommen diese Dateien deutlich weiter, scheitern aber weiterhin - bereits
`NiSourceTexture` ist für diese Version strukturell anders aufgebaut als bei 20.0.0.4 (unsere
modernen Korrekturen für Mystery-Feld und `use_external` treffen hier nicht zu). Eine
vollständige Unterstützung dieser älteren NIF-Ära wäre ein eigenständiges, größeres
Reverse-Engineering-Projekt - siehe docs/MAP_FORMAT.md Abschnitt 15 für den Stand und eine
Liste offener Fragen für eine Folgesession.

### Ergebnis
Massentest unverändert bei 1676/3436 (der Fix schaltet noch keine neuen Dateien frei, legt
aber die Grundlage für eine Folgesession). 7/7 Test-Suiten weiterhin grün, keine Regression -
rückwärtskompatibel, da nur Dateien betroffen sind, die vorher ohnehin komplett fehlschlugen.

## [0.27.1] — Zwei Untersuchungen ohne Erfolg dokumentiert (keine Funktionsänderung, weiterhin 1676/3436)

Zwei weitere Verdachtsstellen für Sonderfälle wurden untersucht und beide verworfen, da sie
im Massentest zu Rückschritten führten (jeweils sofort zurückgenommen, Code unverändert
gegenüber v0.27.0):
- `ParseNiTriStripsHeader`s Freitextfeld bei `CynDN_Tree00.nif` - eine Korpus-weite Auszählung
  (14560 Instanzen) zeigte, dass das fragliche Byte in 99.5% der Fälle 0 ist und nicht
  zuverlässig mit der An-/Abwesenheit des Freitextfelds korreliert - vermutlich ein echter
  Einzelfall, keine systematische Regel.
- `NiMaterialProperty`s 14-vs-15-Float-Frage bei `S_Tower02_ScanLine.nif` - ein
  Peek-basierter Versuch (Rückschritt 1676 → 1508) zeigte, dass die `LooksLikeFreshName()`-
  Heuristik bei aus Fließkommazahlen bestehenden Feldern unzuverlässig ist.

Beide Fälle ausführlich in docs/MAP_FORMAT.md Abschnitt 14 dokumentiert, damit sie nicht erneut
versucht werden. 7/7 Test-Suiten weiterhin grün, Massentest unverändert bei 1676/3436.

## [0.27.0] — NiSkinInstance/NiSkinData/NiSkinPartition implementiert + Trailer-Peek-Fix (1676/3436 ladbar)

### Hinzugefügt
Geskinnte Meshes: `NiSkinInstance` (data_ref + skin_partition + skeleton_root +
Knochen-Referenzen), `NiSkinData` (Bindungspose je Knochen: NiTransform + Bounding-Sphere +
optionale Vertex-Gewichte), `NiSkinPartition` (GPU-Partitionierung für Hardware-Skinning -
Strips/Dreiecke/Knochen-Indizes je Partition). Aus derselben Referenzimplementierung
übernommen. Der Editor stellt Meshes weiterhin nur in Bindungspose dar (keine Animation) -
die Blöcke werden korrekt übersprungen, nicht ausgewertet.

### Gefunden und behoben (Trailer-Logik aus v0.25.0)
- Eine `NiTriShapeData`/`NiTriStripsData` gefolgt von `NiSkinInstance` braucht ebenfalls
  keinen 8-Byte-Trailer - aber `NiSkinInstance` beginnt nicht mit einem Namensfeld, weshalb der
  bestehende `LooksLikeFreshName()`-Peek diesen Fall nicht selbst erkennen konnte. Behoben durch
  expliziten Ausschluss (analog zu NiTriStrips/NiTriShape).
- Dabei einen zweiten, subtileren Bug im bestehenden Peek-Override gefunden: er wirkte
  fälschlich auch im "expliziter Ausschluss"-Zweig und konnte dort eine korrekte,
  typ-basierte Entscheidung des Aufrufers wieder rückgängig machen. Der Override greift jetzt
  nur noch im Standardfall - eine Vereinfachung, die die Trailer-Logik insgesamt robuster
  macht.
- Byte-exakt verifiziert an `Tunnel02_Wood3.nif`: Knochenzahl der NiSkinInstance sprang von
  absurd 126 auf plausible 14, data_ref/skin_partition treffen exakt auf die folgenden Blöcke.

### Ergebnis
Massentest 1664 → 1676/3436 (48.4% → 48.8%). 7/7 Test-Suiten weiterhin grün, weiterhin 0
unplausible UVs, 303 real gefundene Texturen (unverändert).

## [0.26.0] — NiAmbientLight/NiSpecularProperty/NiPathInterpolator + ZWEITER GROSSER FUND: NiSourceTexture bei externen Texturen (1664/3436 ladbar, +199 Dateien)

### Hinzugefügt
- `NiAmbientLight` (reine `NiLight`-Basis, wie `NiDirectionalLight`)
- `NiSpecularProperty` als unterstützte Property - laut Referenz nur `NiObjectNET` + ein
  einzelnes `flags`(u16)-Feld, viel einfacher als der vorherige (nie verifizierte)
  8-Felder-Verdacht bei `NiStencilProperty`
- `NiPathInterpolator` (Pfad-/Wegpunkt-Animationen)

### GROSSER FUND: NiSourceTexture bei `use_external=1`
Bei rein externen Texturen (kein eingebettetes `NiPixelData` in der Datei) folgen nach der
Dateinamens-Referenz 18 weitere Byte (`pixel_layout`, `mipmap_format`, `alpha_format`,
`is_static`, `direct_render`, ein abschließendes Feld) - bei `use_external=0` (eingebettetes
`NiPixelData` vorhanden) fehlen diese komplett. Byte-exakt verifiziert an `RockD8.nif`.
Zusammen mit dem Trailer-Fund aus v0.25.0 hat das den Massentest an einem Nachmittag von 31.5%
auf 48.4% mehr als verdoppelt.

### Ergebnis
Massentest 1500 → 1664/3436 (43.7% → 48.4%). 7/7 Test-Suiten weiterhin grün, 0 unplausible
UVs, real gefundene Texturen 122 → 303.

### Versucht und verworfen
Der `has_shader`-Fix aus der Partikelsystem-Arbeit (v0.24.0) wurde versuchsweise auch auf
`ParseNiTriStripsHeader` (gemeinsame Basis für NiTriStrips/NiTriShape) angewendet - verursachte
einen katastrophalen Rückschritt (1664 → 4 im Massentest), sofort zurückgenommen. Siehe
docs/MAP_FORMAT.md Abschnitt 12 für Details - diese Funktion bewusst nicht weiter angefasst.

## [0.25.0] — GROSSER FUND: NiTriStripsData/NiTriShapeData-Trailer-Regel war zu eng gefasst (1465/3436 ladbar, +382 Dateien)

### Gefunden und behoben
Beim Ergänzen von `NiTextureEffect`/`NiDirectionalLight` fiel auf: die bisherige Regel
("8-Byte-Trailer fehlt NUR, wenn direkt ein weiteres NiTriStrips/NiTriShape folgt") war
unvollständig - der Trailer fehlt auch vor JEDEM ANDEREN Geschwister-Block (z.B. einem Licht,
einem Effekt, einem weiteren NiNode-Ast), nicht nur vor einem zweiten Mesh-Teil. Byte-exakt
verifiziert an `ItemShop02.nif` (NiTriStripsData gefolgt von NiDirectionalLight `"__MAX_
Default_Light"`).

### Neue Allzweck-Heuristik
`LooksLikeFreshName()`: prüft, ob eine Position plausibel der Anfang eines `SizedString`-
Namensfelds ist (Länge 0, oder 1-64 mit ausschließlich druckbarem ASCII danach - praktisch
jeder Blocktyp beginnt mit so einem Feld). Die bisherige "next==TriStrips/TriShape"-Regel
bleibt Standardfall, wird aber per Peek verworfen, wenn sie keinen plausiblen Namensanfang
ergibt, während die andere Variante sehr wohl einen liefert. Gleiches Muster wie der
`NiPixelData`-Trailer-Fix aus v0.24.0.

### Hinzugefügt
`NiTextureEffect` (70 Dateien) und `NiDirectionalLight`/`NiLight`-Basis (38 Dateien) - der
eigentliche Auslöser für diesen Fund.

### Ergebnis
**Massentest 1083 → 1465/3436 (31.5% → 42.6%) - der mit Abstand größte Einzelfund dieser
Session.** 7/7 Test-Suiten weiterhin grün, Texturierungs-Pipeline weiterhin bei 0 unplausiblen
UVs (122 real gefundene Texturen, vorher 94).

## [0.24.0] — Partikelsystem-Familie implementiert (NiParticleSystem, 1083/3436 ladbar)

### Hinzugefügt
Vollständige Partikelsystem-Blockkette: `NiParticleSystem`/`NiMeshParticleSystem`,
`NiPSysData`/`NiParticlesData` (inkl. gemeinsamem `NiGeometryData`-Kopf für Vertex-/Normalen-/
Farb-/UV-Daten), `NiPSysEmitterCtlr`/`NiPSysUpdateCtlr`, `NiBoolInterpolator`/`NiBoolData`,
`NiColorData`, sowie die Modifier `NiPSysAgeDeathModifier`, `NiPSysBoxEmitter`,
`NiPSysSpawnModifier`, `NiPSysGrowFadeModifier`, `NiPSysColorModifier`,
`NiPSysRotationModifier`, `NiPSysGravityModifier`, `NiPSysPositionModifier`,
`NiPSysBoundUpdateModifier`. Aus derselben Referenzimplementierung wie v0.20-0.23 übernommen
und einzeln byte-exakt verifiziert.

### Gefunden und behoben
- `NiParticleSystem`s `has_shader`-Byte ist echt konditional (anders als bei NiTriStrips/
  NiTriShape, wo zufällig immer Inhalt folgte) - eigene, korrekte Kopf-Funktion für Partikel.
- `KeyType`-Wert 5 (CONST) fehlte in beiden KeyGroup-Skip-Funktionen (nur 1/2/3 behandelt) -
  ergänzt, gleiche Schlüsselgröße wie LINEAR.
- `currentMeshHasTexturing` wurde nie für `NiParticleSystem` aus dessen eigener Properties-
  Liste aktualisiert (nur für NiTriStrips/NiTriShape) - genereller Bugfix, unabhängig vom
  Partikel-Thema relevant für jedes texturierte Partikelsystem.
- Nebenfund: `NiPixelData`s Abschluss-Trailer ist nicht immer 8 Byte (mind. 1 Datei mit
  unkomprimiertem 8-Bit-Format brauchte nur 4) - per Peek robust gelöst, rückwärtskompatibel.

### Offen
Bei mindestens einer Datei bleibt nach allen Fixen noch ein weiterer, nicht isolierter Versatz
vor `NiPSysData` bestehen - vermutlich ein vierter, noch unentdeckter Sonderfall (evtl.
`NiVertexColorProperty`-Länge im Partikel-Kontext). Guter Kandidat für eine Folgesession.

### Ergebnis
Massentest 1080 → 1083/3436 (31.4% → 31.5%). 7/7 Test-Suiten weiterhin grün, keine Regression,
Texturierungs-Pipeline weiterhin bei 0 unplausiblen UVs.

## [0.23.0] — NiLODNode/NiRangeLODData implementiert (1080/3436 ladbar)

### Hinzugefügt
- **`NiLODNode`**: `NiSwitchNode : NiNode` + `switch_flags`(u16) + `index`(u32) +
  `lod_level_data_ref`(i32). Rendering-seitig wie normaler `NiNode` behandelt (keine echte
  Distanz-basierte LOD-Umschaltung nötig). Byte-exakt verifiziert an `tree05.nif`
  (Name="LODGroup01", 3 Kinder, `lod_level_data_ref` trifft exakt).
- **`NiRangeLODData`**: weicht von der öffentlichen Referenzstruktur ab (kein Vector3-Center in
  diesem Fork) - stattdessen führendes u32(=0) + `num_lod_levels` + Level-Paare (near/far) +
  8-Byte-Trailer. Byte-exakt an 2 Dateien verifiziert (identische 3-Stufen-LOD-Konfiguration,
  vermutlich ein Baum-Preset) - beide Male trifft die berechnete Länge exakt auf das jeweilige
  Dateiende.

### Ergebnis
Massentest 1062 → 1080/3436 (30.9% → 31.4%). `NiRangeLODData` verschwindet komplett aus der
Fehlerliste. `NiLODNode` selbst bleibt bei ~9 Dateien noch blockiert (vermutlich weitere,
unabhängige Strukturvarianten - nicht untersucht). 7/7 Test-Suiten weiterhin grün.

## [0.22.0] — Folgefehler aus v0.21.0 gelöst: NiSourceTexture hatte dasselbe konditionale Muster (1062/3436 ladbar)

### Gelöst
Der in v0.21.0 gefundene Folgefehler (mehrfach texturierte Wasser-/Lava-/Effekt-Objekte
scheiterten an einer zweiten `NiSourceTexture`) ist behoben. Ursache: `NiSourceTexture`s
`NiObjectNET`-Basis hat - genau wie `NiTexturingProperty` in v0.21.0 - ein zusätzliches,
konditionales u32-Feld (empirisch immer 0) zwischen der Extra-Daten-Liste und dem
`controller`-Feld. Byte-exakt an 2 Dateien verifiziert (`santuary.nif`: Feld vorhanden, 17
Byte insgesamt; `SD_Vale01_machine02.nif`: Feld fehlt, 13 Byte insgesamt). Per Peek robust
unterscheidbar, rückwärtskompatibel zur bisherigen Mehrheit der Dateien.

Bewusst NUR dieses eine Feld korrigiert, nicht die komplette (umfangreichere)
`NiSourceTexture`-Struktur aus derselben Referenzimplementierung übernommen, da diese der
eigenen, seit Monaten extensiv verifizierten Blockreihenfolge (NiPixelData folgt direkt ohne
Zwischenraum) widerspricht - Lehre: jede Korrektur aus einer Referenz einzeln byte-exakt
verifizieren, nicht die ganze Struktur ungeprüft übernehmen.

### Ergebnis
Massentest 1047 → 1062/3436 (30.5% → 30.9%). Texturierungs-Verifikation bestätigt weiterhin 0
unplausible UVs bei jetzt 94 real gefundenen und dekodierten Texturen (vorher 91). 7/7
Test-Suiten weiterhin grün, keine Regression.

## [0.21.0] — NiTextureTransformController-Rätsel GELÖST (mit derselben Referenz wie v0.20.0)

### Gelöst
Das in der Vorsession als "strukturell inkonsistent" zurückgestellte `NiTextureTransformController`
ist tatsächlich **immer fest 39 Byte** - keine Variation zwischen Instanzen. Die wahre Ursache:
`NiTexturingProperty` hat nach den 7 Textur-Slots ein zusätzliches, **konditionales** u32-Feld
(vermutlich `num_shader_textures`), vorhanden NUR wenn die Property einen Controller referenziert
(`controller_ref != -1`). Byte-exakt an 4 echten Dateien verifiziert (2× mit, 2× ohne Controller
- jeweils exakt passend). Gefunden mit derselben unabhängigen Referenzimplementierung, die auch
das UV-Rätsel aus v0.20.0 löste.

### Ergebnis
`NiTextureTransformController` verschwindet komplett aus der Fehlerliste des Massentests
(vorher 134 Dateien direkt blockiert). Gesamt-Massentest bleibt bei 1047/3436, da die
betroffenen Dateien (mehrfach texturierte Wasser-/Lava-/Effekt-Objekte) jetzt an einem neuen,
strukturell ähnlichen Folgefehler bei einer zweiten `NiSourceTexture` scheitern (4-Byte-Versatz,
vermutlich ein weiteres konditionales Feld - nicht weiter untersucht, siehe docs/MAP_FORMAT.md
Abschnitt 7). 7/7 Test-Suiten weiterhin grün, keine Regression.

## [0.20.0] — UV-Rätsel aus v0.19.0 GELÖST: NIF-Objekt-Texturierung ist jetzt echt nutzbar

### Gelöst
Der in v0.19.0 gefundene UV-Bug (siehe dort) ist behoben. Auslöser war ein Hinweis des
Nutzers auf NifTools-Produkte (NifSkope u.a.) als unabhängige, funktionierende Referenz für
dieses Dateiformat. Eine öffentliche Referenzimplementierung (Rust-NIF-Parser, Ziel-Version
20.0.0.4) zeigte: das bisher direkt VOR den UV-Daten gelesene "uv_flags"-u16-Feld existiert an
dieser Stelle gar nicht - `vertex_colors` wird direkt von den UV-Sets gefolgt. Das
mysteriöse 2-Byte-Feld gehört stattdessen NACH die UV-Daten, direkt vor `consistency_flags`.
Zusätzlich: `num_uv_sets` muss mit `& 0x3F` maskiert werden (die oberen Bits gehören zu einem
separaten `tspace_flag`, bisher ohne Auswirkung, da in allen Testdateien 0).

Byte-exakt verifiziert an `santuary.nif`: alle 86 UV-Paare sind jetzt plausible, normalisierte
Texturkoordinaten (z.B. `(0.213, 0.015)`), UND alle nachfolgenden Felder treffen weiterhin exakt
bis zum Dateiende. Zusätzlich an `BerFrz_Thorn.dds`-Dateien bestätigt (saubere
0.0/0.5-Grid-UVs, passend zu einer Textur-Atlas-Aufteilung).

### Ergebnis
**Die Objekt-Texturierung aus v0.15/v0.16 ist erstmals echt nutzbar** - die in v0.19.0
eingebaute `SanitizeUvs()`-Sicherung greift jetzt bei keiner der 91 real gefundenen und
dekodierten Texturen mehr ein (vorher: 89-91 von 91). Massentest unverändert bei 1047/3436
(betrifft nur UV-Inhalte, nicht den Lade-Erfolg). 7/7 Test-Suiten weiterhin grün.

### Methodische Lehre
Byte-Längen-Verifikation über das Dateiende hinweg kann zwei kompensierende Fehler (ein
fehlendes Feld hier, ein überzähliges dort) nicht erkennen, wenn beide dieselbe Gesamtlänge
ergeben - siehe docs/MAP_FORMAT.md Abschnitt 6. Erst eine zusätzliche Inhalts-
Plausibilitätsprüfung deckte den echten Fehler auf.

## [0.19.0] — Wichtiger Fund: NIF-UV-Koordinaten sind unbrauchbar (Absicherung eingebaut, Massentest unverändert bei 1047/3436)

### Untersucht
End-to-End-Verifikation der Objekt-Texturierungs-Pipeline gegen echte Daten: alle 415 DDS- und
741 BMP-Texturdateien aus den bereitgestellten resmap-Archiven extrahiert und indiziert, dann
für jedes erfolgreich geladene `.nif`-Mesh die referenzierte Textur gesucht, mit `DdsImage`
dekodiert und die UV-Koordinaten auf Plausibilität geprüft (91 Treffer, alle DDS-Dekodierungen
fehlerfrei).

### Gefunden
**Die aus `.nif`-Meshes extrahierten UV-Koordinaten sind praktisch überall unbrauchbar** -
auch bei `santuary.nif`, dem am gründlichsten verifizierten Referenzobjekt des Projekts. Die
Byte-LÄNGE des UV-Abschnitts ist zweifelsfrei korrekt (alle Felder danach treffen exakt bis
zum Dateiende), und alle Daten VOR den UVs sind einwandfrei (86/86 Normalen exakte
Einheitsvektoren, plausible Bounding-Sphere) - die UV-WERTE selbst sind trotzdem astronomisch
große oder winzige Zahlen. Mehrere alternative Dekodierungen getestet und verworfen
(Halb-Präzision-Floats, Vector3 statt Vector2, Struct-of-Arrays, vertex-majore Anordnung).
Ursache bleibt ungeklärt - siehe docs/MAP_FORMAT.md, Abschnitt 5, für die vollständige
Herleitung und Liste ausgeschlossener Erklärungen.

### Korrigiert (Absicherung)
Neue `SanitizeUvs()`-Prüfung: verwirft ein extrahiertes UV-Set komplett, wenn auch nur ein
Wert nicht endlich oder implausibel groß ist (`|u|,|v| > 1000`). Der Renderer fällt dann
automatisch auf die Materialfarbe zurück (bereits vorhandener Mechanismus). **Deaktiviert
damit effektiv die in v0.15/v0.16 gebaute Objekt-Texturierung für praktisch alle echten
Objekte**, bis die eigentliche Ursache gefunden ist - bewusste Qualitätsentscheidung: lieber
korrekt eingefärbt als sicher falsch texturiert.

### Ergebnis
Massentest unverändert bei 1047/3436 (30.5%) - diese Änderung betrifft nur die UV-Werte
innerhalb bereits erfolgreich geladener Meshes, nicht den Lade-Erfolg selbst. 7/7 Test-Suiten
weiterhin grün.

## [0.18.0] — NiMaterialColorController + NiPoint3Interpolator + NiPosData implementiert (1047/3436 ladbar, unverändert - siehe Begründung)

### Hinzugefügt (.nif)
- **`NiMaterialColorController`**: dieselbe 30-Byte-`NiSingleInterpController`-Basis wie
  `NiAlphaController`/`NiTransformController`, plus `target_color`(u16)-Feld
- **`NiPoint3Interpolator`**: aktueller Wert(Vector3) + `data_ref`
- **`NiPosData`**: einzelne `KeyGroup<Vector3>` (analog zu `NiFloatData`)
- Byte-exakt verifiziert UND inhaltlich bestätigt an `Eff_2.nif`: eine pulsierende
  Effekt-Farbanimation über 5 Sekunden, deren Werte exakt zwischen Interpolator und
  referenzierter Keyframe-Daten übereinstimmen

### Ergebnis
Massentest unverändert bei 1047/3436 (30.5%) - `NiMaterialColorController` und
`NiPoint3Interpolator` verschwinden komplett aus der Fehlerliste (korrekt implementiert), aber
jede betroffene Datei hat mindestens einen weiteren, noch nicht unterstützten Blocker (meist
`NiTextureTransformController`). Trägt automatisch zu weiteren erfolgreichen Dateien bei,
sobald einer der verbleibenden Blocker gelöst wird. Keine Regression (7/7 Test-Suiten grün).

### Neuer offener Sonderfall
28 Dateien mit `NiPosData` bei `key_type=0` (laut offiziellem Enum ungültiger Wert) - weder
LINEAR- noch QUADRATIC- noch TBC-Interpretation ergab ein plausibles Muster. Schlägt bereits
korrekt sauber fehl (kein Rateversuch, keine Auswirkung auf andere `KeyGroup`-Nutzungen). Siehe
docs/MAP_FORMAT.md.

## [0.17.0] — NiTriShape/NiTriShapeData implementiert + ein weiterer latenter Bug in "verifiziertem" Code gefunden (1047/3436 ladbar)

### Hinzugefügt (.nif)
- **`NiTriShape`**: teilt sich den Kopf byte-exakt mit `NiTriStrips` (`NiTriBasedGeom`-Basis) -
  bewusst als separate Funktion dupliziert statt geteilt, um den bereits verifizierten
  `NiTriStrips`-Pfad nicht anzufassen
- **`NiTriShapeData`**: teilt sich Vertex-/Normalen-/Farben-/UV-Kopf mit `NiTriStripsData`,
  divergiert danach zu einer flachen Dreiecksliste statt Streifen (`num_triangle_points` +
  `has_triangles` + Indizes + Match-Groups). Verifiziert an `BerFrz01_IceSmog.nif`: 3
  aufeinanderfolgende Mesh-Teile, alle mit korrekten Dreiecks-Indizes (max. Index exakt
  `vertexCount-1`, keine Out-of-Bounds-Zugriffe)

### Korrigiert (.nif)
- **Latenter Bug in bereits seit v0.13 "verifiziertem" Code gefunden**: `Num Vertices` (in
  `NiTriStripsData` UND `NiTriShapeData`, geteilte Basisklasse) ist ein **uint16**, gefolgt von
  **Keep Flags(u8) + Compress Flags(u8)** - nicht ein einzelnes uint32 wie bisher angenommen.
  Der alte Read funktionierte nur, weil diese beiden Flag-Bytes in allen bisher getesteten
  Dateien zufällig 0 waren. Gefunden an `BerFrz01_IceSmog.nif` (Keep-Flags≠0): mit dem alten
  Read ergab sich eine absurde Vertex-Anzahl (3,3 Millionen), mit der Korrektur 72 plausible,
  radialsymmetrische Vertex-Koordinaten. Reiner Gewinn im Massentest (+39 Dateien), keine
  Regression bei vorher erfolgreichen Dateien
- **`NiTexturingProperty`**: manche Instanzen (bisher nur bei `NiTriShape`-referenzierten
  Texturen beobachtet) nutzen eine kurze 8-Byte-Basis statt der vollen 12-Byte-`ObjectNetBase`
  - per Peek-Erkennung robust behandelt, ohne den Normalfall zu beeinträchtigen

### Ergebnis
**Massentest über alle 3436 echten Dateien: 1047 laden (30.5%)**, vorher 1001 (29.1%).

### Zurückgerollter Fehlversuch (dokumentiert, kein Codeeinfluss)
Kurzzeitige Hypothese, der bekannte 8-Byte-`NiPixelData`-Trailer sei manchmal nur 4 Byte lang,
wurde getestet und verursachte einen massiven Einbruch (1008→85 Dateien) - sofort
zurückgerollt. Der 8-Byte-Trailer bleibt unverändert korrekt für die weit überwiegende
Mehrheit. Siehe docs/MAP_FORMAT.md für Details.

### Bekannter offener Sonderfall
`NiTriShapeData`/`NiTriStripsData` mit `num_vertices=0` (vermutlich geskinnte Meshes) scheitern
weiterhin (~26 Dateien) - vermutlich werden die optionalen Normalen-/Farben-/UV-Datenblöcke bei
0 Vertices komplett ausgelassen statt als leere Flags serialisiert. Nicht abschließend
verifiziert, siehe docs/MAP_FORMAT.md.

## [0.16.0] — Weitere .nif-Blocktypen: Kollision, Extra-Daten, Transform-/Alpha-Animation (1001/3436 ladbar) + ein latenter Texturing-Bug behoben

### Hinzugefügt (.nif)
- **`NiCollisionData`** (Box/Sphere/Capsule-Bounding-Volumes) - größter Einzel-Hebel, löste
  allein einen Großteil der zuvor blockierten Dateien
- **`NiStringExtraData`** + **`NiIntegerExtraData`** (eigene, von `NiObjectNET` abweichende
  Basis: nur ein Namensfeld, kein Extra-Daten-Zähler, kein Controller)
- **`NiBillboardNode`** (wie `NiNode` + `billboard_mode`-Feld)
- **`NiTransformController` → `NiTransformInterpolator` → `NiTransformData`**: komplette
  Transform-Keyframe-Kette inkl. `XYZ_ROTATION_KEY`-Zweig mit gemischten Interpolationstypen -
  inhaltlich bestätigt an einer schaukelnden Blumen-Animation (`AdlF_Flower.nif`)
- **`NiAlphaController` + `NiFloatInterpolator` + `NiFloatData`**: Alpha-Keyframe-Animation -
  inhaltlich bestätigt an einem flackernden Brand-Effekt (`AdlFH_field_burn_ground.nif`)
- Alle neuen Blocktypen per Landmarken-Technik byte-exakt gegen mehrere echte, strukturell
  unterschiedliche Dateien verifiziert (siehe docs/MAP_FORMAT.md für Details und
  Referenzdateien)

### Korrigiert (.nif)
- **Latenter Bug in bereits "verifiziertem" Code gefunden**: `NiTexturingProperty`s
  pro-Slot-Textur-Transform ist 32 Byte (8 Felder, inkl. eines zuvor übersehenen
  `transform_type`-u32-Feldes zwischen Rotation und Center), nicht 28 Byte (7 Felder) wie
  bisher angenommen. Betraf potenziell auch die in v0.15.0 gebaute, noch unverifizierte
  Objekt-Texturierung bei UV-transformierten Texturen. Gefunden an
  `SD_Vale01_machine02.nif` (4 transformierte Slots in einer Property) - siehe
  docs/MAP_FORMAT.md
- **`NiIntegerExtraData`**: seltenes führendes -1-Feld (~1% der Instanzen, bisher nur bei einem
  benannten Multi-Textur-Blend-Shader beobachtet) per sicherem `PeekU32`-Check erkannt und
  übersprungen, ohne die übrigen ~99% der Instanzen zu beeinträchtigen

### Ergebnis
**Massentest über alle 3436 echten Dateien: 1001 laden (29.1%)**, vorher 579 (16.9%) -
0 Abstürze, alle nicht unterstützten Fälle scheitern weiterhin sauber.

### Bewusst NICHT umgesetzt: `NiTextureTransformController`
Struktur erwies sich als inkonsistent zwischen Instanzen (39 vs. 43 Byte für augenscheinlich
identisch aufgebaute, aufeinanderfolgende Vorkommen im selben Objekt), ohne auffindbares
Unterscheidungsmerkmal. Um das Risiko stiller Datenkorruption zu vermeiden, bleibt dieser Typ
bewusst nicht unterstützt (110 von 3436 Dateien blockiert) - vollständige Analyse und nächste
Schritte in docs/MAP_FORMAT.md.

## [0.15.0] — Objekte werden jetzt texturiert + zwei weitere .nif-Fixes (579/3436 ladbar)

### Hinzugefügt
- **`NifMeshRenderer` lädt jetzt echte Diffuse-Texturen für Objekte** (dieselbe `DdsImage`-
  Infrastruktur wie die Terrain-Texturen) - sofern ein Mesh-Teil sowohl eine
  Textur-Dateireferenz als auch eigene UV-Koordinaten mitbringt. Vertex-Puffer um UV-Attribut
  erweitert, Textur-Cache über Modelle/Karten hinweg. Ohne Textur/UVs bleibt der Fallback
  (Materialfarbe) unverändert. **Nicht verifiziert:** ob NIF-UVs dieselbe V-Achsen-Behandlung
  wie die berechneten Terrain-UVs brauchen (siehe docs/MAP_FORMAT.md)

### Korrigiert (.nif)
- **`NiStencilProperty` byte-genau vermessen**: kurze Basis (8 Byte) + 7 uint32-Felder (nicht
  8, wie zunächst angenommen) + 1 Einzelbyte + eingebettetes Namensfeld
- **Zweiter konditionaler Trailer gefunden**: der 8-Byte-Trailer am Ende von `NiTriStripsData`
  fehlt (wie schon bei `NiPixelData` bekannt), wenn direkt ein weiteres `NiTriStrips` folgt -
  betrifft Mehrfach-Mesh-Objekte (z.B. `Rou_M_Tube.nif`, `rouval_Tower.nif`)
- **Massentest über alle 3436 echten Dateien: 579 ladbar (16.9%)**, vorher 257 (7.5%) - mehr
  als verdoppelt durch diese zwei Fixes
- `tests/test_nif_model.cpp` aktualisiert: die zweite Testdatei muss nicht mehr zwingend
  fehlschlagen (vorher hart erwartet, jetzt da die Parser-Abdeckung wächst als möglicher
  Erfolg mit Geometrie-Validierung behandelt)

## [0.14.3] — Diffuse-Textur war oben/unten vertauscht (gezielter V-Flip, Blend unverändert)

### Korrigiert
- **Diffuse-Textur war relativ zur Blend-/Heightmap-Struktur oben/unten vertauscht.** Vom
  Nutzer anhand eines Screenshots klar identifiziert: oberer Bildbereich = korrekte Struktur
  (Blend/Heightmap), unterer Bildbereich = Diffuse-Textur, beide systematisch gegeneinander
  gespiegelt - durchgängig bei allen Texturen, nicht nur einem Einzelfall
- Die V-Achse wird jetzt NUR für die Diffuse-Textur-Zuordnung gespiegelt
  (`vec2(mapUv.x, 1.0 - mapUv.y)`), der Blend-Gewichts-Lookup bleibt unverändert bei `mapUv`
  (dessen Ausrichtung war bereits vor dieser Session an Heightmap/Objekt-Positionen validiert).
  Die in v0.14.2 korrigierte Kachel-Skalierung (UVScaleDiffuse als Wiederholungsanzahl über die
  Kartenfläche) bleibt unverändert bestehen - beide Fixes zusammen ergeben jetzt korrekt
  orientierte, korrekt skalierte Diffuse-Texturen

## [0.14.2] — UVScaleDiffuse-Formel korrigiert (Nutzer-Screenshots zeigten vervielfachtes Emblem)

### Korrigiert
- **Diffuse-Textur-Kachelung war fest auf 500 Welteinheiten verdrahtet**, unabhängig von der
  Kartengröße - bei größeren Karten (z.B. Rou, 12800 Einheiten Spannweite) wiederholte sich
  jede Bodentextur ca. 100x statt der durch echte `UVScaleDiffuse`-Werte (4-5) vermutlich
  beabsichtigten 4-5x. Nutzer-Screenshots zeigten dadurch ein eigentlich einmaliges
  kreisrundes Boden-Emblem mehrfach wiederholt
- **Korrigierte Formel**: `UVScaleDiffuse` wird jetzt als Wiederholungsanzahl ÜBER DIE GESAMTE
  KARTENFLÄCHE interpretiert (`uv = weltposition/kartenspanne * UVScaleDiffuse`) statt als
  Kehrwert einer festen 500-Einheiten-Periode - besser durch echte `.ini`-Werte gestützt
- Die zwischenzeitlich probeweise eingebaute DirectX/OpenGL-V-Flip-Spekulation für die
  Diffuse-Textur wurde zurückgenommen (unbestätigt, durch diesen fundierteren Fix ersetzt)

### Hinweis zur Sandbox
Die Entwicklungsumgebung wurde zwischen Sessions zurückgesetzt - der Code-Stand wurde aus dem
zuletzt ausgelieferten Paket (v0.14.1) wiederhergestellt, alle 7 Testsuiten liefen danach
weiterhin fehlerfrei (0 Regressionen).

## [0.14.1] — Objekt-Rotation korrigiert (.nif-Achsentausch), Textur-Spiegelung: begründeter Fix

### Korrigiert
- **`.nif`-Vertex-Positionen und -Normalen fehlte das Z-up→Y-up-Achsen-Remap**, das für den
  Rest des Legacy-Formats bereits gilt (siehe `ObjectPlacementIO.cpp`) - dadurch standen
  platzierte Objekte in falscher Rotation. Gleiches Remap jetzt auch für `.nif`-Meshdaten
  angewendet (Achsentausch Y↔Z). Reine Umsortierung, keine Byte-Anzahl-Änderung - alle
  bisherigen Verifikationen bleiben gültig (Massentest weiterhin 257/3436)

### Vermutlich behoben, aber nicht verifiziert (kein Screenshot verfügbar)
- **Diffuse-Textur-Spiegelung**: V-Komponente der Diffuse-Textur-Koordinate im Terrain-Shader
  gespiegelt - wahrscheinlichste Ursache ist eine DirectX/OpenGL-Konventionsdifferenz für die
  vertikale Textur-Achse (Original-Engine vermutlich DirectX-basiert). Bewusst nur die
  Diffuse-Textur betroffen, nicht die Blend-Gewichts-Textur (deren Ausrichtung war bereits
  validiert). Ohne Screenshot nicht abschließend verifizierbar - falls die Karte weiterhin
  falsch orientiert erscheint, liegt die Ursache woanders

## [0.14.0] — .nif: Material/Texturing-Basis korrigiert (257 von 3436 Dateien ladbar, vorher 17)

### Korrigiert
- **`NiTexturingProperty`**: volle 12-Byte-`ObjectNetBase` statt der zuvor angenommenen
  verkürzten 8-Byte-Version - fiel erst bei einer dritten, strukturell anderen Vergleichsdatei
  (`R_Helga01GL.nif`) auf, da sich dieser Fehler bei den ersten beiden Testdateien zufällig mit
  einem zweiten Fehler kompensierte
- **`NiMaterialProperty`**: optionales 15. Float (0.0), abhängig davon, ob das zugehörige
  `NiTriStrips` eine `NiTexturingProperty` referenziert (untexturiert → 15 Floats, texturiert →
  14) - an 4 echten Dateien byte-exakt bis Dateiende verifiziert
- **`NiPixelData`**: konditionaler 8-Byte-Trailer, nur vorhanden wenn NICHT direkt eine weitere
  `NiSourceTexture` folgt
- **`NiAlphaProperty`** (15 Byte) und **`NiStencilProperty`** (vorläufige Länge) ergänzt -
  `NiAlphaProperty` blockierte allein 1989 der 3436 echten Dateien
- **Performance-Fix**: Zählfelder hatten zu großzügige Sicherheitsgrenzen (2 Mio.), wodurch
  fehlausgerichtete Dateien sehr langsam (nicht unendlich, aber spürbar träge) verarbeitet
  wurden. Kontextspezifische, deutlich engere Grenzen ergänzt (z.B. Vertex-Anzahl max. 200.000)
  - Massentest über alle 3436 Dateien läuft jetzt in unter 90 Sekunden

### Ergebnis
**Massentest über alle 3436 echten `.nif`-Dateien: 257 laden mit vollständiger Geometrie
(vorher 17, davor 166 in einem Zwischenschritt)** - 0 Abstürze, alle nicht unterstützten Fälle
scheitern weiterhin sauber mit Fehlermeldung. `NiStencilProperty` ist jetzt der größte
verbleibende Blocker (~270 Dateien) und noch nicht verifiziert (Platzhalter-Länge).

## [0.13.2] — NiTexturingProperty/NiSourceTexture/NiPixelData-Kette fast lückenlos nachvollzogen

### Hinzugefügt (Dokumentation, noch nicht in Code umgesetzt)
- `NiTexturingProperty`: `apply_mode`, `texture_count`, 7 Textur-Slots mit `TexDesc`-Struktur
  vollständig nachvollzogen - **beide belegten `source_ref`-Werte zeigen exakt auf die laut
  Block-Typ-Liste erwarteten `NiSourceTexture`-Blöcke**
- `NiSourceTexture`: Dateiname-Position exakt verifiziert (`"top_wall_c256.dds"`),
  `pixel_data`-Referenz **verifiziert: zeigt exakt auf den erwarteten `NiPixelData`-Block**
- `NiPixelData`: Größenfeld für die eingebettete Rohpixel-Daten (vermutlich ein Asset-Browser-
  Thumbnail, da die echte Textur separat als `.dds` vorliegt) empirisch lokalisiert - die
  gesamte Kette `NiTexturingProperty → NiSourceTexture(Datei 1) → NiPixelData(43704 Byte) →
  NiSourceTexture(Datei 2)` trifft **exakt** auf den unabhängig gefundenen zweiten
  Textur-Dateinamen
- **Einziges verbleibendes Puzzlestück:** der variable Mipmap-Header VOR diesem Größenfeld
  (bei der untersuchten Datei 175 Byte, vermutlich abhängig von der Mipmap-Anzahl) ist noch
  nicht feldweise entschlüsselt - bräuchte eine zweite Vergleichsdatei mit abweichender
  Mipmap-Anzahl (analog zur `.shbd`-Methode), um robust zu werden. Ohne das bleibt
  `LoadNifMesh` bei texturierten Meshes weiterhin auf "nicht unterstützt", auch wenn die
  Struktur jetzt fast vollständig verstanden ist

## [0.13.1] — Format-Übersicht dokumentiert, NiTexturingProperty teilweise entschlüsselt

### Hinzugefügt
- Neue Übersichtstabelle am Anfang von `docs/MAP_FORMAT.md`: alle Dateiformate (`.tshm`, `.HTD`,
  `.shbd`, `.shmd`, `.idm`/`.aid`, `.dds`, `.nif`, `.sbi` u. a.) mit Status und Verweis auf den
  jeweiligen Detailabschnitt
- **`NiTexturingProperty`-Struktur positionsgenau nachvollzogen** (noch nicht in Code
  umgesetzt): kurze Basis, `apply_mode`, `texture_count`, 7 Textur-Slots mit `TexDesc`
  (Quell-Referenz + Clamp/Filter-Modus + UV-Set + optionale Transform) - **die berechnete
  Position landet exakt auf dem unabhängig gefundenen Textur-Dateinamen**, und beide belegten
  `source_ref`-Werte zeigen exakt auf die laut Block-Typ-Liste erwarteten
  `NiSourceTexture`-Blöcke. `NiSourceTexture`/`NiPixelData` selbst noch offen - texturierte
  Meshes bleiben vorerst nicht ladbar

## [0.13.0] — Echte Meshes in 3D gerendert + texturierte 2D-Draufsicht (Heightmap/Texturing/Walk)

### Hinzugefügt
- **`NifMeshRenderer`**: lädt und rendert echte `.nif`-Geometrie für alle Objekte, bei denen
  `core::LoadNifMesh` erfolgreich ist (aktuell 17 von 3436 echten Dateien, siehe v0.12.0) -
  einfacher Lambert-Shader mit der extrahierten Materialfarbe, kein Instancing (jedes Modell hat
  eigene Geometrie). Wird automatisch beim Öffnen einer Karte sowie bei den granularen
  `.tsobj`/`.shmd`-Objekt-Importen neu geladen. `ObjectMarkerRenderer` zeichnet die
  Platzhalter-Pyramide jetzt NUR NOCH für Objekte, für die kein echtes Mesh geladen werden
  konnte (kein doppeltes Zeichnen mehr)
- **Texturierte Draufsicht für den 2D-Editor**: `HeightmapRenderer::BeginTopDownScene` nutzt
  denselben Multi-Layer-Diffuse-Blend-Shader wie die 3D-Ansicht, nur mit einer orthographischen
  Kamera direkt von oben (`OrthoTopDownViewProj`, neu in `Camera.cpp`) - Heightmap-, Texturing-
  und Objekt-Placement-Modus zeigen jetzt die echte, alle Layer bereits zusammengemischte
  Kartentextur statt einer reinen Graustufen-Vorschau
- **Block&Walk-Overlay**: `HeightmapRenderer::DrawTopDownOverlay` legt die Block&Walk-Heatmap
  halbtransparent rot (Deckkraft nach Heat-Wert) über die echte Kartentextur, statt sie isoliert
  in Graustufen anzuzeigen - eigener zweiter Framebuffer (`fbo2d_`, unabhängig von der
  3D-Vorschau) und ein einfacher Vollbild-Quad-Shader für die Überlagerung

### Bekannte Einschränkungen (v0.13.0)
- Nicht mit echtem GLFW/ImGui/glad gegenkompiliert (wie der Rest der App-Schicht) - nur
  syntaktisch gegen einen erweiterten GL-Stub geprüft
- Overlay-Ausrichtung (welche Bildseite welcher WalkGrid-Zeile entspricht) ist über sorgfältige
  Herleitung der OpenGL-Textur-Konventionen bestimmt, aber nicht visuell verifizierbar - falls
  die Block&Walk-Heatmap nach dem Bauen vertikal gespiegelt zur Kartentextur erscheint, muss nur
  die uv.y-Berechnung im `quadVerts`-Array (Renderer.cpp) umgekehrt werden
- `previewTex`/`layerPreviewTex` (die alten Graustufen-Texturen) werden weiterhin berechnet, aber
  nicht mehr angezeigt - könnten in einem Aufräum-Durchgang entfernt werden

## [0.12.0] — `.nif`-Parser: echte Geometrie für untexturierte Meshes extrahierbar

### Hinzugefügt
- `core::LoadNifMesh` (`NifModel.hpp`/`.cpp`): GUI-freier Parser für Gamebryo/NetImmerse-Dateien
  (Version 20.0.0.4). Verifiziert per Byte-exaktem Dateiende-Abgleich an 2 echten Dateien
  unterschiedlicher Vertex-/Dreieckszahl (`Eld_CD.nif`, `AddSharpCD.nif`) - komplette Struktur
  für Datei-Header, `NiNode`-Szenengraph, `NiMaterialProperty` und das `NiTriStrips`/
  `NiTriStripsData`-Paar (Vertices, Normalen, Vertexfarben, UVs, Dreiecksstreifen) entschlüsselt
  und in echten C++-Code überführt. Details und Herleitung siehe docs/MAP_FORMAT.md
- Sicherheitsgrenzen (`CountU32`/`CountU16`) gegen `bad_alloc`-Abstürze bei unerwarteten/nicht
  unterstützten Block-Strukturen ergänzt - der Parser bricht bei einem nicht unterstützten
  Block-Typ (z. B. `NiTexturingProperty`) kontrolliert mit Fehlermeldung ab, statt zu
  versuchen, riesige Fake-Arraygrößen zu allozieren
- **Massentest über alle 3436 echten `.nif`-Dateien** aus den bereitgestellten Kartensets: 17
  laden mit vollständiger, korrekter Geometrie (0 Abstürze bei den restlichen 3419, alle mit
  klarer Fehlermeldung statt Absturz oder falschen Daten)
- 8 neue Tests in `tests/test_nif_model.cpp` (Gesamt: 108 Checks über 6 statische + 2
  parametrisierte Testdateien)

### Bekannte Einschränkungen (v0.12.0)
- **Texturierte Meshes werden noch nicht unterstützt** (die große Mehrheit der echten Dateien) -
  `NiTexturingProperty`/`NiSourceTexture`-Struktur noch nicht entschlüsselt (Textur-Dateiname
  wurde per Landmarken-Suche gefunden, aber die ca. 74 Byte davor noch nicht vollständig
  aufgeschlüsselt)
- Kein GL-Rendering der extrahierten Geometrie integriert - `ObjectMarkerRenderer` zeigt
  weiterhin nur Platzhalter-Marker, auch für Objekte mit erfolgreich geladener echter Geometrie.
  Das ist der nächste konkrete Schritt für "echte Objekte in der 3D-Vorschau"
- Weitere unbekannte Block-Typen (`NiCollisionData`, `NiStringExtraData`, `NiPointLight`,
  `NiTriShape`/`NiTriShapeData` u. a.) nicht unterstützt

## [0.11.1] — 2D-Editor zeigt Rohgitter jetzt immer in Heightmap-Form an

### Korrigiert
- Nutzer-Anforderung: Block&Walk (und Texturing) sollen auf einer Fläche bearbeitet werden, die
  wie die 2D-Heightmap aussieht - nicht in der nativen (bei Block&Walk extrem länglichen, 1:16)
  Pixel-Form des Rohgitters. `DrawEditor2D` bemisst die Anzeigefläche jetzt immer anhand der
  Heightmap-Ausdehnung statt anhand der jeweiligen Rohgitter-Dimensionen - die Rohdaten werden
  beim Zeichnen automatisch auf diese Form gestreckt/gestaucht (wie eine Textur auf ein
  andersförmiges Quad)
- **Kein Save-seitiger Konvertierungscode nötig:** die Mal-Logik (Weltposition → Rasterzelle)
  rechnete bereits vorher korrekt über die tatsächliche Kartenausdehnung um, unabhängig von der
  Anzeigegröße - nur die Anzeige selbst war falsch bemessen. `ExportLegacyShbd` schreibt
  weiterhin exakt die unveränderte Rohform zurück (byte-exakt, unverändert getestet)
- Als Nebeneffekt auch für Texturing konsistent: unabhängig auflösende Textur-Layer werden jetzt
  ebenfalls immer in Heightmap-Form angezeigt statt in ihrer eigenen (meist quadratischen, aber
  nicht notwendig heightmap-proportionalen) nativen Form

## [0.11.0] — Block&Walk-Gitter-Auflösung endgültig korrigiert (vom Nutzer per Screenshot gefunden)

### Korrigiert — wichtigster Fund seit Projektbeginn
- **Block&Walk-Gitter war NICHT quadratisch**, wie seit v0.4.0 angenommen. Der Nutzer bemerkte
  im laufenden Tool eine sichtbare 4-fache Wiederholung derselben Silhouette im 2D-Editor
  (Screenshot). Direkte Visualisierung der rohen `.shbd`-Bytes bei verschiedenen Kandidaten-
  Breiten (Python, unabhängig vom Tool) bestätigte: **Breite = QuadsBreite/2, Höhe =
  QuadsBreite×8** (Verhältnis exakt 1:16) - nicht `max(QuadsBreite,QuadsHöhe)×2` (quadratisch)
  wie zuvor angenommen. **Wichtige methodische Lehre:** Der bisherige Byte-für-Byte-Roundtrip-
  Test verifizierte nur die GESAMT-Byte-Anzahl (512×512 = 128×2048 = 262144 Elemente in beiden
  Fällen), nicht die tatsächliche Breite/Höhe-Aufteilung - ein Fehler in der 2D-Interpretation
  konnte dadurch unentdeckt bleiben, obwohl alle Roundtrip-Tests durchgehend grün waren. Erst
  visuelle Kontrolle deckte es auf
- `core::PeekLegacyShbdHeader` (neu): liest die tatsächliche Gitterhöhe direkt aus dem
  Datei-Header (zweites Feld, an allen 4 Karten exakt bestätigt) statt sie aus einer Formel
  herzuleiten - robuster, da selbstbeschreibend. `OpenLegacyMap` nutzt das jetzt bevorzugt,
  mit der Formel als Fallback
- `SyncWalkGridSize` (main.cpp, für neue native Karten) und `OpenLegacyMap` (Legacy-Import)
  beide korrigiert. `tests/test_walk_grid.cpp` von hartkodierten 512×512 auf die korrekten
  128×2048 aktualisiert (bestand vorher nur zufällig, weil beide Werte dieselbe Gesamt-
  Elementanzahl ergeben)
- **Verifiziert an allen 4 echten Kartensets** (Rou/Bera: 128×2048, RouVal01/Eld: 256×4096,
  Adl: 475×7600 - Verhältnis überall exakt 16.0): korrekte, nicht wiederholte Interpretation
  bestätigt, UND weiterhin byte-exakter Export-Roundtrip erhalten

### Bekannte Einschränkungen (v0.11.0)
- Die genaue Achsen-Zuordnung (welche der beiden stark unterschiedlichen Dimensionen der Welt-
  X- bzw. -Z-Achse entspricht) bleibt ungeklärt - betrifft nur die Orientierung der Anzeige/des
  Pinsels, nicht die Datenkorrektheit
- `.nif`-Objektgeometrie-Parsing (für echte Meshes statt Platzhalter-Marker) weiterhin in
  Arbeit - Datei-Header und Szenengraph-Traversierung (NiNode-Hierarchie) sind verifiziert,
  die eigentlichen Geometrie-Blöcke (Vertex-/Dreiecksdaten) noch nicht vollständig entschlüsselt

## [0.10.0] — Echte Diffuse-Texturen im 3D-Terrain (DDS/BC1-BC3 + Multi-Layer-Blend-Shader)

### Hinzugefügt
- `core::DdsImage` (`LoadDdsImage`): GUI-freier DDS-Loader, dekodiert BC1/DXT1, BC2/DXT3 und
  BC3/DXT5 (die einzigen in den 160 echten Field-Texturen vorkommenden Formate, empirisch
  geprüft) zu rohem RGBA8. **Verifiziert gegen eine unabhängige Referenzimplementierung**
  (Pillow/libImaging) an je einer echten DXT1-, DXT3- und DXT5-Datei: max. Abweichung 1/255 pro
  Kanal (Rundungsdifferenz bei der RGB565→888-Konvertierung, keine strukturelle Abweichung)
- `HeightmapRenderer::LoadTerrainTextures`/`UpdateBlendTextures`: lädt bis zu 8 echte
  Diffuse-Texturen + deren Blend-Gewichte und rendert sie im 3D-Terrain gemischt, statt des
  bisherigen reinen Höhen-Farbverlaufs (der als Fallback erhalten bleibt, z.B. direkt nach
  "Neu"). Diffuse-Pfade werden über dieselbe Mehrfach-Wurzel-Pfadauflösung wie die Blend-BMPs
  aufgelöst; nicht ladbare Layer bekommen eine graue Platzhalter-Textur statt den Aufbau
  abzubrechen
- Fragment-Shader nutzt 8 fest benannte Sampler-Uniform-Paare (Diffuse+Blend) statt eines
  Sampler-Arrays - dynamische Array-Indizierung von Samplern ist im strikten GLSL-330-Core-
  Profil nicht garantiert, feste Uniforms sind auf jeder GL-3.3-Hardware sicher
- Textur-Malen aktualisiert die 3D-Ansicht jetzt live (Blend-Gewichte werden bei jedem
  Pinselstrich und bei Undo/Redo neu hochgeladen, ohne die Diffuse-DDS neu zu laden)

### Bekannte Einschränkungen (v0.10.0)
- **2D-Editor zeigt weiterhin nur Graustufen** (Höhe bzw. einzelnes Layer-Gewicht), keine echte
  Textur-Komposition - eigener Folgeschritt (z.B. über eine zusätzliche orthographische
  Top-Down-Render-Passage mit demselben Shader), noch nicht umgesetzt
- Karten mit mehr als 8 Textur-Layern (z.B. `Teva` mit 13) zeigen nur die ersten 8 texturiert -
  Grenze durch garantierte Mindestanzahl an Textur-Units in GL 3.3 (16, davon 2 pro Layer)
- Bedeutung von `UVScaleDiffuse` aus der Legacy-`.ini` nicht abschließend gesichert - aktuelle
  Kachelung (alle 500 Welteinheiten, skaliert mit diesem Wert) ist eine plausible Annahme, nicht
  gegen das Original-Rendering verifizierbar
- Renderer-Änderungen syntaktisch gegen erweiterten GL-Stub geprüft (kompiliert sauber), aber
  wie der Rest der App-Schicht nicht mit echtem GLFW/ImGui/glad gegenkompiliert

## [0.9.0] — Nutzer-Feedback aus erstem echten Testlauf: Map-Öffnen-Fix, Rot-Tint-Fix, bewegliche Kamera

### Korrigiert
- **`.HTD`-Import scheiterte bei mehreren echten Karten** (`BigCoast.HTD`/`UrgDark01.HTD`/
  `UrgSwa01.HTD`), weil diese zusätzliche Daten nach dem reinen Höhenraster enthalten
  (4 bis 5268 Byte, Bedeutung nicht gesichert). `ImportLegacyHtd`/`ExportLegacyHtd` erfassen und
  reproduzieren diese Bytes jetzt statt den Import abzubrechen - **verifiziert an allen drei
  echten Dateien: Import erfolgreich UND Re-Export weiterhin byte-exakt**. Ergebnis am realen
  29-Karten-Set: 26 von 29 laden jetzt vollständig (vorher 23 von 29) - die 3 verbleibenden sind
  Datenvollständigkeits-/Formatfragen, keine Code-Bugs (siehe docs/MAP_FORMAT.md)
- **Graustufen-Vorschauen erschienen ROT statt grau** (Heightmap-, Textur-Layer- und
  Block&Walk-Vorschau im 2D-Editor): fehlender Textur-Swizzle bei `GL_R8`-Einkanaltexturen -
  OpenGL liest G/B-Kanäle ohne expliziten Swizzle als 0, wodurch nur der Rot-Kanal Werte trägt.
  `GL_TEXTURE_SWIZZLE_G/B/A` ergänzt an allen drei Stellen

### Hinzugefügt
- **Bewegliche 3D-Kamera**: rechte Maustaste + ziehen verschiebt jetzt das Kamera-Zielzentrum
  (Pan), skaliert mit der aktuellen Kameraentfernung; vorher war nur Rotation um einen fest bei
  (0,0,0) verankerten Punkt möglich
- "Kamera zentrieren"-Button im Werkzeuge-Panel; Kamera wird beim Öffnen einer Karte jetzt
  automatisch auf deren Mittelpunkt zentriert und die Entfernung an die Kartengröße angepasst
  (vorher: fester Startpunkt/Zoom unabhängig von der tatsächlichen Kartengröße)

### Bekannte Einschränkungen (v0.9.0) / als Nächstes
- Echte Diffuse-/Blend-Texturen werden in 2D UND 3D weiterhin nicht angezeigt (nur Graustufen
  bzw. prozeduraler Höhen-Farbverlauf) - als Nächstes geplant. Diffuse-Texturen sind DXT1-
  komprimierte `.dds`-Dateien (256x256, mit Mipmaps, per Header-Analyse bestätigt) - eigener
  BC1-Decoder + Mehrlagen-Blend-Shader nötig, noch nicht begonnen
- Objekte zeigen weiterhin nur Platzhalter-Marker (Pyramide+Pfeil), keine echten `.nif`-Meshes
- Offene Rückfrage vom Nutzer ("Block&Walk muss auf ein Segment gematcht werden nicht 4") noch
  nicht umgesetzt - Bedeutung nicht abschließend geklärt

## [0.8.1] — Fix: windows.h min/max-Makro-Kollision mit std::max/std::min

### Korrigiert
- `<windows.h>` wurde ohne `NOMINMAX` eingebunden - windows.h definiert dadurch `min`/`max` als
  Präprozessor-Makros, die jedes `std::max(...)`/`std::min(...)` im restlichen `main.cpp` kaputt
  machen (MSVC-Fehler `C2589`, "ungültiges Token rechts von ::"). `#define NOMINMAX` vor dem
  Include ergänzt - klassischer, gut bekannter Windows-Header-Stolperstein

## [0.8.0] — Nativer Ordner-Browser + automatische Kartenerkennung

### Hinzugefügt
- "Asset-Ordner wählen..."-Button im Datei-Menü öffnet einen nativen Windows-Ordnerdialog
  (`SHBrowseForFolder`, Windows SDK, keine zusätzliche Abhängigkeit) - Nutzer muss keine Pfade
  mehr von Hand eintippen
- Automatischer Scan (`ScanForMaps`, begrenzte Rekursionstiefe) findet alle `.ini`-Dateien unter
  dem gewählten Ordner und listet sie als anklickbare Kartenliste; steigt bewusst NICHT in
  `fieldTexture/`-Ordner ab (dort liegen nur geteilte Texturen, keine Karten, aber potenziell
  viele Dateien) - Auswahl aus der Liste befüllt automatisch das "Karte öffnen"-Feld
- Gleicher Ordnerdialog auch für das Speichern-Ausgabeverzeichnis verfügbar
- **Scan-Logik isoliert (ohne GL/ImGui-Abhängigkeit) gegen die echten Kartenordner getestet:**
  findet alle 4 echten Karten (Adl/Bera/Eld/RouVal01) korrekt, überspringt `fieldTexture`
  zuverlässig

### Bekannte Einschränkungen (v0.8.0)
- Ordnerdialog ist Windows-only (`#ifdef _WIN32`) - unter Linux weiterhin nur manuelle
  Pfadeingabe (das Textfeld bleibt in jedem Fall sichtbar/nutzbar, auch unter Windows)
- Ältere `SHBrowseForFolder`-API statt der moderneren `IFileOpenDialog`-COM-API gewählt (weniger
  fehleranfällig ohne lokale Kompilierbarkeit, dafür optisch schlichter - kein "Neuer Ordner"-
  Button im Dialog)
- Scan-Logik selbst getestet, aber `BrowseForFolderWindows` (der eigentliche Win32-Dialog-Aufruf)
  naturgemäß nicht - das ist der Teil, der jetzt einen echten Windows-Testlauf braucht

## [0.7.3] — Zwei echte main.cpp-Bugs aus dem ersten GUI-Compile-Versuch behoben

### Korrigiert (gefunden durch den ersten echten main.cpp-Compile mit MSVC - vorher nie kompiliert,
nur die reinen GL-Dateien Renderer.cpp/ObjectMarkerRenderer.cpp hatte ich mit einem Stub geprüft)
- `EditorState::legacySpatialIndex` war fälschlich als `core::legacy::ObjectSpatialIndex`
  deklariert - der Typ liegt aber in `core::ObjectSpatialIndex` (ohne `::legacy::`). Tippfehler
  beim Bau des Objekt-Placement-Panels
- `EditorState::objectPlaceMode` war als `bool` deklariert, wurde aber mit
  `ImGui::RadioButton(const char*, int*, int)` verwendet, das einen `int*` erwartet - auf `int`
  umgestellt (1 = Platzieren, 0 = Auswählen)

### Bekannte Einschränkungen (v0.7.3)
- Nach diesen zwei Fixes noch kein erneuter Build-Versuch bestätigt - da main.cpp vorher nie
  kompiliert wurde, sind weitere kleinere Fehler dieser Art (Namespace-Tippfehler, ImGui-
  Overload-Mismatches) beim nächsten Versuch nicht auszuschließen

## [0.7.2] — Erster echter Windows-Build (MSVC): Core + alle Tests kompilieren, ein GUI-Fix

### Bestätigt (echter Windows/MSVC-Build durch den Nutzer, nicht mehr nur Syntax-Stub)
- `mapeditor_core.lib` UND alle 6 Test-Executables kompilieren und linken sauber mit MSVC
  (Visual Studio 2022, `/std:c++latest`) - erste echte Bestätigung, dass `std::expected` und der
  gesamte C++23-Code cross-platform funktionieren, nicht nur unter GCC/Linux

### Korrigiert
- `main.cpp` inkludierte die Dear-ImGui-Backend-Header über `backends/imgui_impl_glfw.h` /
  `backends/imgui_impl_opengl3.h` (Layout des offiziellen Dear-ImGui-Repos). Der vcpkg-`imgui`-
  Port installiert diese Header aber OHNE `backends/`-Unterordner direkt ins Include-Verzeichnis.
  Includes entsprechend angepasst (kein `backends/`-Präfix mehr)

### Bekannte Einschränkungen (v0.7.2)
- `main.cpp`/`Renderer.cpp`/`ObjectMarkerRenderer.cpp`/`Camera.cpp` nach diesem Fix noch nicht
  erneut gegenkompiliert (wartet auf Rückmeldung vom nächsten Windows-Build-Versuch) - weitere
  kleinere ImGui-API-Abweichungen sind beim ersten echten GUI-Build nicht auszuschließen

## [0.7.1] — Fix: inkonsistente Blend-Auflösung innerhalb einer Karte (Adl)

### Korrigiert
- `ImportLegacyTextureSet` verwarf bisher Textur-Layer, deren Blend-BMP von der für die Karte
  erkannten Auflösung abwich (bei der echten Adl-Karte: 8 von 10 Layern, 476×476 statt 512×512).
  Neue `core::ResampleBlendMap` (bilinear) resampelt solche Layer jetzt auf die Stack-Auflösung,
  statt sie ohne Daten zu lassen. **Verifiziert: alle 10 von 10 Adl-Layer haben jetzt echte
  Blend-Daten** (vorher 2 von 10). Vollständiger Open→Save→Reopen-Rundlauf weiterhin fehlerfrei
  für Adl/Bera/RouVal01

### Bekannte Einschränkungen (v0.7.1)
- Resampling ist nicht verlustfrei (nur relevant bei starkem Auflösungsunterschied zwischen
  Layern derselben Karte - bei Adl 476→512, ein moderater Unterschied)
- `Eld` weiterhin nicht öffenbar (anderes Format) - laut Nutzer vermutlich Gamebryo-bezogen,
  keine Dokumentation dazu verfügbar; ohne weitere `.sbi`-Referenzdateien nicht weiter
  aufschlüsselbar (anders als bei `.shbd`, wo der Vergleich über mehrere echte Karten
  unterschiedlicher Form die entscheidende Erkenntnis brachte)

## [0.7.0] — Vereinheitlichter "Karte öffnen/speichern"-Workflow

### Hinzugefügt
- `core::legacy::LegacyMapProject` + `OpenLegacyMap`/`SaveLegacyMap`: öffnet/speichert eine
  komplette Legacy-Karte (Heightmap + Texturing + Block&Walk + Objekt-Placement + räumlicher
  Index + Zonen-Metadaten) über EINE Aktion statt sieben Einzel-Importen. Fehlende/nicht ladbare
  Teile sind nicht fatal (Report statt Abbruch) - nur die `.ini` selbst ist zwingend
- `core::legacy::LegacyPathResolve`: gemeinsame Pfad-Resolver-Utilities aus
  `LegacyTextureSetIO.cpp` herausgelöst (keine Code-Duplikate mehr), plus neue
  `FindSiblingFileByStem` für die Begleitdatei-Auffindung
- GUI: neuer, prominent platzierter "Karte öffnen/speichern"-Bereich ganz oben im Datei-Menü;
  bisherige Einzel-Modul-Importe bleiben als "fortgeschritten" darunter erhalten
- 15 neue Tests in `tests/test_legacy_map_project.cpp` (voller Open→Save→Reopen-Rundlauf,
  parametrisiert über Kommandozeilenargument - läuft gegen Bera/Adl/RouVal01 mit identischem
  Ergebnis: Heightmap byte-exakt, alle Modul-Dimensionen und Objekt-Anzahl nach Rundlauf
  identisch). Gesamt: 100 Checks über 5 statische + 1 parametrisierten Test

### Korrigiert
- Granulare `.HTD`/`.shbd`-Import-Buttons lasen den Legacy-Header bisher NICHT ein (nutzten beim
  Export immer den Default-Header statt des tatsächlich importierten) - jetzt wird der Header
  beim Import erfasst und beim Export wiederverwendet, für byte-exakten Re-Export auch im
  granularen Workflow

### Bekannte Einschränkungen (v0.7.0)
- RouVal01 hat keine `.aid`-Datei (Zonen-Metadaten) - wird als "nicht vorhanden" behandelt, kein
  Fehler
- Adl hat inkonsistente Blend-Auflösung zwischen Layern derselben Karte (siehe v0.6.0/
  docs/MAP_FORMAT.md) - 8 von 10 Textur-Layern laden dort keine echten Blend-Daten
- `Eld` weiterhin nicht öffenbar (anderes Format, siehe v0.4.0)
- GUI-Code (`main.cpp`) weiterhin nicht gegen echte GLFW/ImGui/glad-Bibliotheken kompiliert
  (siehe v0.1.0/v0.6.0) - Core-Workflow (`OpenLegacyMap`/`SaveLegacyMap`) dagegen vollständig
  kompiliert und gegen echte Kartensets getestet

## [0.6.0] — Textur-Auflösung entkoppelt, Legacy-Pfadauflösung repariert, Windows-Härtung

### Korrigiert — drei zusammenhängende Funde anhand der echten Kartensets
- **Textur-Layer-Auflösung war fälschlich an die Heightmap gekoppelt** (siehe v0.4.0-Fund).
  `TextureLayerStack` nutzt jetzt eine unabhängige Auflösung; `ImportLegacyTextureSet` bestimmt
  sie aus der ERSTEN tatsächlich ladbaren Blend-BMP statt sie zu erzwingen; Fallback 512x512
  (dokumentierter Platzhalter) falls keine BMP lesbar ist. `ExportLegacyTextureSet` überschreibt
  die Heightmap-Ini-Felder nicht mehr fälschlich mit der Textur-Auflösung. GUI-Pinsel-Mapping
  (Texturing UND Zuordnung im 2D-Editor) entsprechend korrigiert
- **Legacy-Blend-Pfade waren nicht auflösbar:** echte Karten referenzieren Blend-BMPs relativ zu
  einer GETEILTEN Asset-Wurzel (z.B. `fieldTexture/` als Geschwister-Ordner von `field/<Karte>/`),
  nicht relativ zum Kartenordner selbst; zusätzlich sind die Pfade case-sensitiv falsch
  (Windows-authored, `moss.bmp` vs. echte Datei `Moss.BMP`). Neuer Resolver
  (`ResolveLegacyAssetPath`) probiert mehrere plausible Wurzeln mit case-insensitivem Fallback
  pro Pfad-Komponente. Das virtuelle `resmap`-Präfix (kein echter Ordner) wird jetzt konsistent
  von Import UND Export entfernt (vorher nur beim Import - Export schrieb an eine andere Stelle,
  als der Import erwartete). **Verifiziert: voller Import→Export→Reimport-Rundlauf mit den
  echten Bera-Daten (10 Layer, 512×512) - 0.0 Abweichung, 0 fehlende Dateien**
- **Windows-Härtung** (Nutzer-Anforderung: Editor muss am Ende auf Windows laufen): `strnlen`
  (zwar auch unter MSVC vorhanden, aber POSIX-Herkunft) durch reine Standardbibliotheks-Variante
  ersetzt; `/utf-8`-Compiler-Flag für MSVC ergänzt (`main.cpp` enthält direkt eingebettete
  deutsche Umlaute in ImGui-Labels, MSVC würde diese sonst nach Systemcodepage statt UTF-8
  interpretieren); README um expliziten Windows-Build-Abschnitt inkl. Compiler-Versionsanforderung
  (`std::expected` braucht VS2022 17.9+) ergänzt

### Bekannte Einschränkungen (v0.6.0)
- Windows-Build weiterhin nicht tatsächlich kompiliert (kein Windows-Rechner in dieser Umgebung
  verfügbar) - Code wurde aber gezielt auf bekannte Windows-Stolpersteine durchsucht und
  gehärtet, siehe README.md
- `.nif`-Import, `Eld`/`.sbi`-Format weiterhin offen (siehe v0.4.0/v0.5.0)

## [0.5.0] — Heightmap + Objekt-Placement gemeinsam im 3D-Preview

### Hinzugefügt
- `app::ObjectMarkerRenderer`: GPU-instanced Platzhalter-Marker (Pyramide + Richtungspfeil) für
  jedes platzierte Objekt im 3D-Preview - zeigt Position, Blickrichtung (Rotation um die
  Hochachse) und Skalierung, ausgewähltes Objekt farblich hervorgehoben. Kein echtes
  `.nif`-Mesh-Rendering (eigenes, größeres Folgeprojekt, siehe docs/MAP_FORMAT.md) - Zweck ist,
  Layout/Dichte der Objekt-Placements im Kontext des Terrains einschätzen zu können
- `HeightmapRenderer::BeginScene`/`EndScene`: Terrain- und Objekt-Rendering teilen sich jetzt
  einen gemeinsamen Framebuffer-Pass (korrekter Depth-Test zwischen beiden)

### Korrigiert — wichtiger Fund
- **Achsen-Konvention:** `Rou.shmd` (und vermutlich alle Legacy-Objektdaten) sind **Z-up**
  (X/Y = horizontale Ebene, Z = Höhe), das Heightmap-Modul ist **Y-up**. Verifiziert durch
  Abgleich echter Objekt-Positionen gegen `Heightmap.SampleWorld()` (Diff nahe 0 für
  bodenstehende Objekte vor dem Fix bei falscher Achse, nach dem Fix bei korrekter). Ohne
  diesen Fix wären Objekte beim Rendern um die komplette Kartenhöhe (mehrere Hundert bis
  Tausend Einheiten) falsch platziert gewesen. Fix sitzt an der Legacy-Import/-Export-Grenze
  (`ParseLegacyShmd`/`SerializeLegacyShmd` tauschen Y/Z für Position UND Rotation) - reines
  Vertauschen ohne Berechnung, `.shmd`-Byte-Exaktheit bleibt dadurch erhalten (erneut gegen
  `Rou.shmd` verifiziert). GUI-Rotationsfeld entsprechend von "Rotation Z" auf "Rotation um
  Hochachse" umbenannt (liegt jetzt korrekt auf `rotY` statt `rotZ`)

### Bekannte Einschränkungen (v0.5.0)
- Objekt-Marker sind Platzhalter (keine echten Meshes) - `.nif`-Import bleibt offen
- Achsen-Fix nur für reine Yaw-Rotation verifiziert (siehe v0.4.0-Eintrag zu zusammengesetzten
  Rotationen)
- `ObjectMarkerRenderer.cpp`/`Renderer.cpp` sind syntaktisch gegen einen minimalen GL-Funktions-
  Stub geprüft (kompiliert sauber), aber wie der Rest der App-Schicht nicht mit echten
  GLFW/ImGui/glad-Bibliotheken gegenkompiliert - siehe v0.1.0-Hinweis

## [0.4.0] — Phase 4: Objekt-Placement-Modul + Validierung an 4 zusätzlichen echten Karten

### Hinzugefügt
- `core::ObjectPlacementSet`: Kategorie-Listen, Szene-Umgebung (Licht/Nebel/Hintergrund), flache
  Instanzliste (Modellpfad + Position + Quaternion-Rotation + Scale)
- `core::ObjectPlacementIO`: natives `.tsobj`-Format + `legacy::ParseLegacyShmd`/
  `SerializeLegacyShmd` — **byte-für-byte identisch** zur echten `Rou.shmd` (inkl. exakter
  Fließkomma-Formatierung, siehe "Korrigiert")
- `core::ObjectSpatialIndex` + `legacy::ParseLegacyIdm`/`SerializeLegacyIdm` — **byte-für-byte
  identisch** zur echten `Rou.idm` (1178 variabler-Länge-Gruppen, Struktur vollständig verifiziert)
- `legacy::ParseLegacyAid`/`SerializeLegacyAid` — **byte-für-byte identisch** zur echten `Rou.aid`
  (inkl. nicht genullter Speicherreste im Namensfeld, roh durchgereicht statt rekonstruiert)
- GUI: vierter Editor-Modus "Objekte" (Platzieren/Auswählen per Klick, Marker-Overlay, Rotation/
  Skalierung editierbar), Menü um Objekt-Placement/idm/aid-Import-Export ergänzt
- 22 neue Tests in `tests/test_object_placement.cpp` (Gesamt: 78 Checks über fünf Testdateien)

### Korrigiert (anhand von 4 zusätzlichen echten Kartensets: Adl, Bera, Eld, RouVal01)
- **BMP-Format:** Blend-Bitmaps sind real **24-bit RGB**, nicht 8-bit indiziert wie in v0.3.0
  angenommen — Codec umgeschrieben und gegen 9 echte Dateien verifiziert (vorher nur Selbsttest)
- **Block&Walk-Gitterauflösung:** ist IMMER quadratisch (`max(QuadsBreite,QuadsHöhe) × 2`), nicht
  pro Achse unabhängig skaliert wie in v0.3.0 angenommen — an der nicht-quadratischen Adl-Karte
  (950×475 Quads → 1900×1900 Gitter) eindeutig widerlegt und korrigiert
  Rundungs-Tie-Breaking (round-half-away-from-zero) reproduziert dies exakt; bei Bera/Eld
  verhindern Inkonsistenzen der Original-Dateien selbst (unterschiedliche Tool-Versionen über die
  Entwicklungszeit) 100%-Bytegleichheit trotz korrekter Werte, siehe docs/MAP_FORMAT.md

### Bekannte Einschränkungen (v0.4.0)
- Texture-Layer-Auflösung ist an die Heightmap gekoppelt; reale Karten nutzen eine unabhängige,
  oft niedrigere Blend-Auflösung (z. B. 512×512 unabhängig von 257×257- oder 513×513-Heightmaps)
  — Architektur-Diskrepanz, noch nicht behoben
  aus dem NIF-Format
- `.idm`-Zellzuordnung (welche der 1178 Gruppen zu welcher Rasterzelle gehört) bleibt Hypothese
- `Eld`-Kartenset nutzt ein anderes/neueres Format (leere `.ini`, keine `.HTD`, stattdessen
  `.sbi`/`.sbisss`) — nicht abgedeckt
- `main.cpp`/`Renderer.cpp`/`Camera.cpp` weiterhin nicht in dieser Umgebung kompilierbar (siehe
  v0.1.0); alle Core-Module sind dagegen kompiliert UND gegen echte Referenzdateien getestet

## [0.3.0] — Phase 3: Block&Walk-Modul + geschlossene Roundtrip-Lücken

### Hinzugefügt
- `core::WalkGrid`: rohes int16-Gitter (512×512 bei Standard-Heightmap-Auflösung, hergeleitet
  aus 256 Quads × 2 Subzellen — siehe docs/MAP_FORMAT.md für die Korrektur der ursprünglich
  falschen "64×8"-Herleitung)
- `core::WalkEditOps`: Stempel-Pinsel (harte Kante, kein Falloff — passend für diskrete
  Flag-/Bitmask-Werte) + eigener Undo/Redo-Stack
- `core::WalkGridIO`: natives `.tswalk`-Format + `ImportLegacyShbd`/`ExportLegacyShbd` —
  **verifiziert byte-für-byte identisch** zur echten `Rou.shbd`
- `legacy::BmpBlendMap`: 8-bit-Graustufen-BMP-Codec für Blend-Bitmaps (Lesen/Schreiben) —
  schließt die Texturing-Roundtrip-Lücke aus v0.2.0 (Selbsttest, da keine `.BMP`-Referenzdatei
  verfügbar war)
- `legacy::LegacyTextureSetIO`: bündelt `.ini` + alle Blend-BMPs zu einem kompletten
  Texturing-Set-Import/-Export (inkl. Rekonstruktion der Legacy-Verzeichnisstruktur)
- GUI: 2D-Editor jetzt mit drei Modi (Heightmap / Textur malen / Block&Walk), Werkzeuge-Panel
  entsprechend erweitert, Menü um Block&Walk-Laden/Speichern/Legacy-Import/-Export ergänzt
- 11 neue Tests in `tests/test_walk_grid.cpp`, 15 neue in `tests/test_legacy_texture_roundtrip.cpp`
  (Gesamt: 56 Checks über vier Testdateien, alle grün)

### Korrigiert
- `ExportLegacyHtd` ergänzt (v0.2.0 hatte nur Import — siehe v0.2.0-Eintrag)
- Falsche Auflösungsherleitung für das Block&Walk-Gitter korrigiert, bevor sie in der GUI
  verbaut wurde (256×2 statt 64×8 — siehe docs/MAP_FORMAT.md)

### Bekannte Einschränkungen (v0.3.0)
- `.idm` (Instanz-/Index-Tabelle) und `.shmd` (Objekt-Placement-Liste) sowie `.aid`
  (Zonen-Metadaten) noch nicht implementiert — nächste Phase
- Bit-Semantik der `Rou.shbd`-Werte bleibt Hypothese (zusammenhängende Bitmasken erkannt, aber
  keine Tool-Doku zur Bestätigung verfügbar) — Editor arbeitet daher mit Rohwerten statt einer
  vermuteten Bedeutung
- `main.cpp`/`Renderer.cpp`/`Camera.cpp` weiterhin nicht in dieser Umgebung kompilierbar (siehe
  v0.1.0) — alle Core-Module (`Heightmap`, `TextureLayer*`, `WalkGrid*`, `legacy::*`) sind
  dagegen kompiliert UND gegen echte Referenzdateien getestet

## [0.2.0] — Phase 2: Texturing-Modul + Legacy-Export

### Hinzugefügt
- `core::BlendMap` / `core::TextureLayerStack`: Gewichtsgitter pro Layer, gemeinsame Gitter-
  auflösung, Basis-Layer wird automatisch voll belegt (Summe aller Layer-Gewichte = 1.0)
- `core::TexturePaintOps`: `PaintLayerWeight` mit Cross-Layer-Normalisierung (Erhöhen eines
  Layers senkt die übrigen proportional zu ihrem Anteil ab) + eigener Undo/Redo-Stack
- `core::TextureLayerIO`: natives `.tstex`-Format (Layer-Metadaten + quantisierte Gewichte)
- `core::legacy::LegacyMapIni`: gemeinsamer Parser für das `.ini`-Format (Heightmap-Metadaten
  UND Layer-Definitionen), getestet gegen den echten Inhalt von `Rou.ini`
- `ExportLegacyHtd`: Heightmap zurück ins Legacy-`.HTD`-Layout schreiben — **verifiziert
  byte-für-byte identisch** zur hochgeladenen Original-`Rou.HTD`
- `legacy::SerializeLegacyMapIni`: `.ini` zurückschreiben — verifiziert wertgleich nach
  Parse→Serialize→Reparse
- 12 neue Tests in `tests/test_texture_layers.cpp` (Gesamt: 30 Checks über beide Testdateien,
  alle grün), inkl. `tests/fixtures/Rou.ini` mit dem echten Original-Inhalt

### Bekannte Einschränkungen (v0.2.0)
- Texturing hat noch **kein GUI-Panel** in `main.cpp` (nur Heightmap ist bisher an die App
  angebunden) — folgt zusammen mit Block&Walk in einem gemeinsamen UI-Update
- Blend-**Bitmaps** (`.BMP`) werden nur als Pfad-Metadatum erfasst, kein Pixel-Import/-Export
  (keine `.BMP`-Datei in den Referenzdaten vorhanden) — siehe `docs/MAP_FORMAT.md`, Abschnitt
  "Roundtrip-Status"
- `.ini`-Export reproduziert die Nutzdaten vollständig, aber nicht die Original-Formatierung
  (Kommentare, exaktes Tab-Layout)

## [0.1.0] — Phase 1: Heightmap-Modul

### Hinzugefügt
- `core::Heightmap`: GUI-freier Datencontainer (Vertex-Grid, bilineares `SampleWorld`, `MinMax`)
- `core::HeightmapIO`: natives `.tshm`-Format (selbstbeschreibend, versioniert) + Importer für
  das reverse-engineerte Legacy-Format (`Rou.HTD`/`Rou.HTDG`), Details siehe `docs/MAP_FORMAT.md`
- `core::EditOps`: Pinsel-Operationen (Anheben/Absenken/Glätten/Einebnen) mit Smoothstep-Falloff
  + `UndoStack` (Diff-Patches, nicht komplette Gitter-Snapshots)
- Standalone-App (`map_editor`): GLFW + Dear ImGui (Docking) + OpenGL 3.3
  - 2D-Panel: Graustufen-Höhenbild, Pinsel direkt per Maus auf dem Bild
  - 3D-Panel: schattiertes Mesh mit Höhen-Farbverlauf, Orbit-Kamera, Wireframe-Toggle
  - Menü: Neu / Laden / Speichern (`.tshm`), Legacy-Import mit manueller Dimensionseingabe
- `tests/test_heightmap_core.cpp`: GUI-freier Test, baubar direkt mit `g++` (kein CMake/GL
  nötig) — verifiziert Kernlogik UND den Legacy-Importer gegen echte `Rou.HTD`/`Rou.HTDG`

### Bekannte Einschränkungen (v0.1.0)
- Nur Heightmap-Modul; Texturing / Block&Walk / Objekt-Placement folgen als eigene Module
- Ein Pinsel-"Stempel" pro Frame erzeugt ein eigenes Undo-Patch (kein Zusammenfassen ganzer
  Striche) — funktional korrekt, aber mehr Undo-Schritte als nötig bei langem Strich
- `Smooth`-Modus mittelt innerhalb eines einzelnen Anwendungsschritts direkt auf dem Live-Gitter
  (keine getrennte Lese-/Schreib-Kopie) — bei sehr großem Radius/Strength minimal abweichend von
  einem "reinen" Weichzeichner, in der Praxis für Terrain-Editing unauffällig
- `main.cpp` / `Renderer.cpp` / `Camera.cpp` sind **nicht** in dieser Umgebung kompiliert worden
  (keine GLFW/ImGui/glad-Libs, keine Netzwerkverbindung zum Nachinstallieren verfügbar) —
  Core-Modul (`Heightmap`/`HeightmapIO`/`EditOps`) inkl. Legacy-Import wurde dagegen kompiliert
  UND gegen die echten `Rou.HTD`/`Rou.HTDG`-Dateien getestet (14/14 Checks grün)

## 2026-09-16 – SHN Editor + CI Build Workflow

- SHN-Core-Parser/Writer für verschlüsselte und raw `.shn` Dateien ergänzt.
- Fiesta Crypto-Header und Verschlüsselungsalgorithmus beim Speichern erhalten.
- Unterstützung für bekannte SHN-Spaltentypen inklusive Typ 26 und Typ 29.
- Unbekannte Typen werden verlustarm als Rohbytes geladen/gespeichert.
- SHN Editor GUI mit Single-/Multi-/XP-/Buy&Sell-/Quest-Reitern begonnen.
- Zellbearbeitung per Doppelklick, Suche und mehrere gleichzeitig geöffnete SHN-Dateien ergänzt.
- GitHub Actions Build-/Test-Workflow für Windows und Linux ergänzt.
