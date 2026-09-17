# HANDOFF — Stand für Fortsetzung in neuem Chat

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
  `NiLookAtInterpolator`, UND korrekte UV-Koordinaten. **Massentest: 2622 von 3436 echten
  Dateien ladbar (76.3%)**
- **3D-Rendering**: Terrain mit echter Multi-Layer-Textur (bis 8 Layer), Objekt-Marker
  (Platzhalter-Pyramiden, GPU-instanced), echte `.nif`-Meshes (`NifMeshRenderer`) - Objekt-
  Texturierung sollte jetzt mit korrekten UV-Koordinaten funktionieren, aber NOCH NICHT in
  einem echten Build visuell verifiziert (nächster naheliegender Schritt).
- **2D-Editor**: zeigt echte texturierte Draufsicht statt Graustufen (Heightmap/Texturing/
  Objekt-Placement), Block&Walk als halbtransparentes Rot-Overlay
- Windows-Build funktioniert (mehrere reale Fixes durch den Nutzer bestätigt: NOMINMAX,
  Ordner-Dialog, etc. - siehe CHANGELOG ab v0.7.2)

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

## Nächste sinnvolle Schritte (Vorschlag, keine Pflicht)
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
