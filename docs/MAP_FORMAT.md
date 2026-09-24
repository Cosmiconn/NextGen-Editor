> Historisches Forschungsprotokoll. Für den aktuellen Stand einschließlich entfernter eigener Formate gilt [FIESTA_FORMAT_STATUS.md](FIESTA_FORMAT_STATUS.md). Frühere Vollständigkeitsangaben beziehen sich nur auf die damaligen Fixtures.

# Kartenformate

## 0. Übersicht — alle Dateiformate auf einen Blick

| Datei | Zweck | Status | Details |
|---|---|---|---|
| `.tshm` | Native Heightmap (TheSeed) | ✅ Vollständig, selbstbeschreibend | Abschnitt 1 |
| `.HTD`/`.HTDG` | Legacy-Heightmap | ✅ Byte-exakt verifiziert (4 Karten). Manche Dateien haben Trailing-Bytes nach dem Raster (jetzt erhalten statt Fehler) | Abschnitt 2a |
| `.ini` (Legacy) | Kartenparameter (Größe, Pfade, Layer-Namen) | ✅ Alle benötigten Felder gefunden und geparst | Abschnitt 2 |
| BMP-Blend-Maps + `.ini`-Layer-Sektion | Textur-Layer-Gewichte (Legacy) | ✅ 24-bit RGB verifiziert (9 Dateien, 3 Kartensets). Bei abweichender Layer-Auflösung wird resampelt | Abschnitt 2c |
| `.dds` (Diffuse-Texturen) | Echte Bodentexturen | ✅ BC1/BC2/BC3 verifiziert gegen unabhängigen Referenz-Decoder (Pillow) | `core/DdsImage.hpp` |
| `.shbd` | Block&Walk-Kollisionsgitter | ✅ Byte-exakt verifiziert (4 Karten), Auflösung jetzt korrekt (Breite=Quads/2, Höhe=Quads×8, aus Header lesbar). Bit-Semantik der Werte weiterhin Hypothese | Abschnitt 2a |
| `.shmd` | Objekt-Platzierung (Legacy, Text) | ✅ Byte-exakt verifiziert (Rou, Bera, Eld) | Abschnitt 2b |
| `.idm`/`.aid` | Objekt-Platzierung (Legacy, Binär) | ✅ Struktur verifiziert, Zellzuordnung der 1178 Gruppen weiterhin Hypothese | Abschnitt 2b |
| `.tstex`/`.tswalk`/`.tsobj` | Native Formate (Texturing/Walk/Objekte) | ✅ Vollständig, selbstbeschreibend | jeweilige `*IO.hpp` |
| `.nif` (Gamebryo 20.0.0.4) | 3D-Objektmodelle | ⚠️ Teilweise: Header, Szenengraph, Material, untexturierte Meshes (17/3436 Dateien) verifiziert. `NiTexturingProperty` positionsgenau nachvollzogen, aber `NiSourceTexture`/`NiPixelData` noch offen | Abschnitt 4 |
| `.sbi`/`.sbisss` (Eld u. Varianten) | Eigenständiges Subsystem (vermutlich Dungeon-Instanzen) | ❌ Kein Kartenformat - andere Datenstruktur (benannte Einträge), nicht analysiert | - |

**Reverse-Engineering-Methode** (durchgängig verwendet, siehe Detailabschnitte): echte
Referenzdateien vergleichen (oft mehrere unterschiedlicher Größe), exakte Dateigröße/-ende als
hartes Constraint nutzen, Landmarken-Suche nach bekannten Strings (Namen, Dateipfaden) zur
Verifikation von Byte-Offsets. Ein reiner Byte-*Anzahl*-Abgleich reicht NICHT als Beweis für die
richtige 2D-Interpretation (siehe die Block&Walk-Korrekturgeschichte in Abschnitt 2a) - wo
möglich wurde zusätzlich visuell/strukturell gegen unabhängige Quellen (Pillow für DDS,
mehrere Karten für Formeln) geprüft.

## 1. Natives TheSeed-Format: `.tshm` (TheSeed HeightMap)

Selbstbeschreibender, versionierter Little-Endian-Container. Im Gegensatz zum Legacy-Format
(siehe unten) stehen die Gitterdimensionen **in der Datei selbst** — kein externes `.ini` nötig.

| Offset | Typ       | Feld         | Beschreibung                                  |
|-------:|-----------|--------------|------------------------------------------------|
| 0      | char[4]   | Magic        | `"TSHM"`                                       |
| 4      | uint32    | Version      | aktuell `1`                                    |
| 8      | uint32    | Width        | Vertex-Anzahl X                                |
| 12     | uint32    | Height       | Vertex-Anzahl Z                                |
| 16     | float32   | BlockWidth   | Abstand zwischen Vertizes, X-Richtung          |
| 20     | float32   | BlockHeight  | Abstand zwischen Vertizes, Z-Richtung          |
| 24     | float32[] | Heights      | `Width * Height` Werte, row-major (z-major)    |

Implementierung: `include/mapeditor/core/HeightmapIO.hpp` (`LoadTshm` / `SaveTshm`).

## 2. Referenz: reverse-engineertes Legacy-Format (`Rou.*`)

Analyse der vom Nutzer bereitgestellten Beispieldateien eines älteren MMORPG-Kartenformats.
Dient als Vorbild/Migrationsquelle, **nicht** als Zielformat für TheSeed. Confidence-Level ist
pro Datei angegeben.

### `Rou.ini` — Sicher (Klartext)
Beschreibt Gitterauflösung (hier: 257×257 Vertizes, 64×64 Quads à 50 Einheiten) sowie eine Liste
von Textur-Layern (Diffuse-Textur + Blend-Bitmap + UV-Scale je Layer) → Vorbild für das
**Texturing-Modul**.

### `Rou.HTD` / `Rou.HTDG` — Verifiziert (siehe `tests/test_heightmap_core.cpp`)
- 4-Byte-Header (in den Referenzdateien `01 02 01 00` — Bedeutung nicht gesichert, vermutlich
  Format-/Versionskennung), danach `Width * Height` Float32-Werte, row-major.
- Dimensionen stehen **nicht** in der Datei selbst, sondern in der begleitenden `.ini`.
- **Offener Punkt:** `.HTD` und `.HTDG` haben identisches Layout und identische Größe, enthalten
  aber zu ~87 % unterschiedliche Werte (gemessen an der echten Referenzdatei) — `.HTDG` ist also
  **keine** reine Backup-/Arbeitskopie von `.HTD`. Mögliche Erklärungen: getrennter
  Gradient-/Normalen-Buffer, zweite Höhen-Ebene (z. B. Wasserstand) oder ein komplett anderer
  Zweck. Ohne weitere Referenz (Tool-Doku, weitere Beispieldateien mit erkennbarem Muster) nicht
  abschließend zu klären — der Importer liest beide Dateien identisch ein und überlässt die
  Interpretation der aufrufenden Stelle.

### `Rou.shbd` — Struktur verifiziert (Byte-für-Byte-Roundtrip), Bit-Semantik weiterhin Hypothese
8-Byte-Header (`00 01 00 00 00 08 00 00`), danach ein Int16-Grid, row-major —
**verifiziert per Byte-für-Byte-Export-Vergleich** gegen die echte Referenzdatei (`tests/test_walk_grid.cpp`).

Vollständige Werteanalyse der echten Datei: 340 unterschiedliche Werte, `-1` (≈79 %, "frei/
unbelegt") und `0` (≈16 %) dominieren, der Rest verteilt sich auf Werte wie `0x01FF`, `0x0FFF`,
`0xFFE0`, `0xFFC0`, `0xE000`, `0x8000` — durchgehend **zusammenhängende Bitmasken** (Folgen von
1-Bits von einem Ende des 16-bit-Worts), kein einfaches Begehbar/Blockiert-Bit. Naheliegende
Interpretation: 16 Richtungs- oder Höhen-Sektoren, von denen ein zusammenhängender Bereich
blockiert ist — aber **nicht verifiziert** (keine Tool-Dokumentation verfügbar). Der Editor
behandelt die Werte deshalb als rohe, verlustfrei durchgereichte int16-Zahlen statt sie auf eine
vermutete Bedeutung zu reduzieren.

**Auflösung — zweite Korrektur, jetzt an vier echten Karten visuell verifiziert (nicht nur
Byte-Anzahl):** Frühere Annahmen ("64 Quads × 8", dann "quadratisch, QuadsBreite × 2") waren
beide falsch. Das Gitter ist **NICHT quadratisch**: Breite = QuadsBreite/2, Höhe = QuadsBreite×8
(Seitenverhältnis exakt 1:16 bei allen vier Karten). Gefunden, weil der Nutzer beim Betrachten
der App eine sichtbare Wiederholung entdeckte ("zeigt 4× dasselbe nebeneinander") — die alte
"quadratisch"-Annahme bestand zwar den Byte-Anzahl-Test (512×512 = 128×2048 = 262144 Elemente
in beiden Fällen), interpretierte die Daten aber mit falschem Breite/Höhe-Verhältnis, wodurch
sich das Bild bei falscher (zu breiter) Interpretation sichtbar wiederholte. Verifiziert durch
direktes Rendern der rohen Werte bei mehreren Kandidaten-Breiten: nur bei Breite=QuadsBreite/2
ergibt sich ein einzelnes, nicht wiederholtes Bild (bei allen vier Karten). Das zweite
Header-Feld enthält praktischerweise die Höhe direkt (`00 08 00 00` = 2048 bei Rou/Bera,
ebenso exakt bei Adl/RouVal01/Eld) — `PeekLegacyShbdHeader` liest das jetzt direkt aus der
Datei, statt sich auf die Formel zu verlassen. **Wichtige Lehre:** Ein Byte-Anzahl-Test allein
verifiziert nur die Gesamtgröße, nicht die tatsächliche 2D-Interpretation (Breite×Höhe-
Aufteilung) — das hätte früher auffallen können, wurde aber erst durch visuelle Kontrolle
sichtbar. Die genaue Achsen-Zuordnung (welche der beiden Dimensionen der Welt-X- bzw.
-Z-Achse entspricht) bleibt ungeklärt.

**UX-Konsequenz:** Da das Rohgitter ein extremes Seitenverhältnis hat (1:16), zeigt der Editor
es NICHT in seiner nativen Pixel-Form an. Stattdessen wird die Anzeigefläche im 2D-Editor immer
anhand der Heightmap-Form bemessen (siehe `DrawEditor2D` in `main.cpp`) - die Grafikkarte
streckt/staucht die Rohdaten beim Zeichnen automatisch auf diese Form, exakt wie eine Textur auf
ein andersförmiges Quad. Der Nutzer malt dadurch auf einer Fläche, die wie die Heightmap
aussieht; die Umrechnung in die krumme Rohform passiert unsichtbar bei jedem Pinselstrich
(Weltposition → Rasterzelle, bereits vorher korrekt implementiert) und beim Speichern ändert
sich nichts (`ExportLegacyShbd` schreibt weiterhin exakt die unveränderte Rohform zurück).

### `Rou.idm` — Struktur verifiziert (Byte-für-Byte-Roundtrip), Zellzuordnung weiterhin Hypothese
32-Zeichen-ASCII-Hex-Hash (vermutlich Sync-Check gegen `.shmd`) + Zeilenumbruch, dann ein
führender int32-Wert (`1078` in der Referenzdatei, Bedeutung ungeklärt), dann **1178 Gruppen**
variabler Länge `[count, idx_1..idx_count]` bis Dateiende — **verifiziert byte-für-byte** gegen
die echte Referenzdatei (`tests/test_object_placement.cpp`), Parser konsumiert die komplette
Payload ohne Rest. Naheliegende Interpretation: räumlicher Index, der Rasterzellen mit
Objekt-Indizes aus `.shmd` verknüpft (Werte bis ~1067, plausibel als Index in die 1580-elementige
Objektliste) — die genaue 2D-Zellzuordnung der 1178 Gruppen (z. B. auf ein Rasterschema) ist
**nicht verifiziert**, der Editor behandelt sie daher als flache, geordnete Liste statt ein
2D-Grid-Objekt zu erzwingen.

### `Rou.shmd` — Vollständig verifiziert (Byte-für-Byte-Roundtrip)
Menschenlesbares, Token-basiertes Format (nicht zeilenkritisch, robust gegenüber CRLF/LF):
`shmd0_5` (Versionskennung) → generische Kategorie-Blöcke (`Sky`/`Water`/`GroundObject` in der
Referenzdatei) mit reinen Modellpfad-Listen → globale Szene-Parameter (`GlobalLight`, `Fog`,
`BackGroundColor`, `Frustum`) → Objekt-Placement-Blöcke (`<Modellpfad> <Anzahl>` gefolgt von
Transform-Zeilen: Position + Quaternion-Rotation + Scale) bis `DataObjectLoadingEnd` →
abschließend `DirectionLightAmbient`/`DirectionLightDiffuse`. **Verifiziert byte-für-byte**
gegen die echte Referenzdatei inkl. exakter Fließkomma-Formatierung (`tests/test_object_placement.cpp`)
→ direktes Vorbild für das **Objekt-Placement-Modul**.

### `Rou.aid` — Sicher (kurze Binärstruktur, vollständig geparst)
`int32 (=1)` + `char[32]` Zonenname (z. B. `"MH_Zone1"`) + `int32 (=1)` + `float[5]`
(Bounding-/Trigger-Volumen) → Zonen-/Area-Metadaten.

### `Rou.conf` — Sicher (Klartext, INI)
Pro-Karte Renderparameter (Glow-Postprocessing etc.) — kein Kernbestandteil eines der vier
angefragten Module, aber gutes Vorbild für map-lokale Settings-Dateien.

## 2a. Roundtrip-Status: Legacy-Format rein UND wieder raus?

| Datei | Import | Export | Verifiziert |
|---|---|---|---|
| `Rou.HTD` | ✅ | ✅ `ExportLegacyHtd` | **Byte-für-Byte identisch** zum Original, bestätigt an 4 echten Karten unterschiedlicher Größe (257×257 bis 951×476) |
| `Rou.HTDG` | ✅ (gleiche Funktion) | ✅ (gleiche Funktion) | Layout identisch zu `.HTD` |
| `Rou.ini` | ✅ | ✅ `SerializeLegacyMapIni` | **Wertgleich** nach Parse→Serialize→Reparse (Kommentare/Whitespace des Originals werden NICHT reproduziert) |
| Blend-`.BMP` | ✅ `ReadBlendMapBmp` | ✅ `WriteBlendMapBmp` | **Verifiziert gegen 9 echte Blend-Bitmaps** aus 3 Kartensets (Adl/Bera/RouVal01) — Format ist 24-bit RGB, nicht 8-bit indiziert wie ursprünglich angenommen (siehe unten) |
| `Rou.shbd` | ✅ `ImportLegacyShbd` | ✅ `ExportLegacyShbd` | **Byte-für-Byte identisch**, bestätigt an 4 echten Karten (512×512 bis 1900×1900) |
| `Rou.shmd` | ✅ `ParseLegacyShmd` | ✅ `SerializeLegacyShmd` | **Byte-für-Byte identisch** für Rou/Adl/RouVal01; bei Bera/Eld verhindern Inkonsistenzen der Original-Dateien selbst (siehe unten) 100%-Bytegleichheit — Werte sind in allen Fällen korrekt |
| `Rou.idm` | ✅ `ParseLegacyIdm` | ✅ `SerializeLegacyIdm` | **Byte-für-Byte identisch** zum Original |
| `Rou.aid` | ✅ `ParseLegacyAid` | ✅ `SerializeLegacyAid` | **Byte-für-Byte identisch** (inkl. nicht genullter Speicherreste im Namensfeld) |
| `Rou.conf` | ❌ noch nicht implementiert | ❌ | kein Kernbestandteil eines der vier Module |

## 2b. Zusätzliche Validierung: vier echte Kartensets (Adl, Bera, Eld, RouVal01)

Der Nutzer hat komplette Kartenordner mit echten Field-Texturen, Blend-Bitmaps und `.nif`-
Objekten bereitgestellt. Wichtigste Erkenntnisse daraus:

- **BMP-Format korrigiert:** Echte Blend-Bitmaps sind durchgehend **24-bit RGB** (R=G=B,
  `dataOffset=54`, keine Palette) statt der ursprünglich angenommenen 8-bit-indizierten Variante.
  Codec wurde entsprechend umgeschrieben und gegen 9 echte Dateien aus 3 Kartensets verifiziert
  (siehe `src/core/legacy/BmpBlendMap.cpp`).
- **Blend-Auflösung ist unabhängig von der Heightmap-Auflösung:** Bera hat eine 257×257-Heightmap,
  aber 512×512-Blend-Bitmaps; RouVal01 hat 513×513-Heightmap und ebenfalls 512×512-Blend-Bitmaps.
  Die aktuelle `TextureLayerStack`-Implementierung geht von gemeinsamer Auflösung mit der
  Heightmap aus — **bekannte Diskrepanz zur Realität, noch nicht behoben** (würde ein separates
  Auflösungsfeld pro Layer-Set erfordern).
- **Block&Walk-Gitter — KORRIGIERT (dritte Version dieser Erkenntnis):** ist NICHT quadratisch.
  Die vorherige "quadratisch, `max(QuadsBreite,QuadsHöhe)×2`"-Annahme bestand nur den
  Gesamt-Byte-Test, nicht die tatsächliche Breite/Höhe-Aufteilung. Richtig (an allen 4 Karten
  visuell verifiziert): Breite = QuadsBreite/2, Höhe = QuadsBreite×8 (Verhältnis exakt 1:16).
  Vom Nutzer per Screenshot entdeckt (sichtbare 4-fache Wiederholung im 2D-Editor) — Details
  und die visuelle Verifikationsmethode siehe oben im `Rou.shbd`-Abschnitt.
- **`.shbd`-Header ergibt jetzt vollständig Sinn:** Erste 4 Byte = ursprüngliche Quad-Anzahl
  (X-Achse), zweite 4 Byte = die tatsächliche Gitterhöhe **direkt** (nicht "Breite × 4" wie
  zuvor vermutet) — konsistent an allen 4 Karten exakt bestätigt. `PeekLegacyShbdHeader` liest
  das jetzt direkt aus der Datei, robuster als jede Formel.
- **`.shmd`-Formatierung ist zwischen Karten leicht inkonsistent:** Bera hat bei einer
  Kategorie-Pfadzeile kein trailing Space (wo andere Karten eins haben); mindestens eine
  Eld-Zeile zeigt eine echte Fließkomma-Rundungs-Tie (`15382.9765625`), die dort "round-half-to-
  even" statt "round-half-away-from-zero" (wie bei Rou) verwendet. Das sind Eigenschaften der
  Original-Dateien (vermutlich unterschiedliche Tool-/Compiler-Versionen über die
  Entwicklungszeit), keine Parser-Fehler — alle Werte werden in jedem Fall korrekt gelesen, nur
  die 100%-Byte-Reproduktion ist für diese beiden Dateien nicht erreichbar.
- **`.nif`-Objektdateien** sind im offenen **Gamebryo/NetImmerse-Format (Version 20.0.0.4)** —
  demselben Format-Fundament wie bei Morrowind/Oblivion/Skyrim, mit etablierter
  Community-Dokumentation (niflib/niftools). Kein Parser dafür in diesem Modul implementiert
  (außerhalb des ursprünglichen Vier-Module-Umfangs), aber gute Ausgangslage für ein künftiges
  Import-Modul, falls gewünscht.
- **Achsen-Konvention in `.shmd` ist Z-up** (X/Y = horizontale Ebene, Z = Höhe) — verifiziert durch
  Abgleich echter Objekt-Positionen gegen `Heightmap.SampleWorld()` an derselben Stelle (Diff nahe
  0 für bodenstehende Objekte, siehe Testskript-Ergebnis in der Session-Historie). Das
  Heightmap-Modul verwendet Y-up. `ParseLegacyShmd`/`SerializeLegacyShmd` tauschen daher Y und Z
  (Position UND Rotations-Quaternion) an der Legacy-Grenze - reines Vertauschen ohne Berechnung,
  bleibt byte-exakt roundtrip-fähig. Ohne diesen Fix wären importierte Objekte um die Kartenhöhe
  (mehrere Hundert bis Tausend Einheiten) falsch im Raum platziert gewesen.
- **`Eld` nutzt ein neueres/anderes Format:** `Eld.ini` ist leer, keine `.HTD`-Datei vorhanden,
  stattdessen `.sbi`/`.sbisss`-Dateien (nicht analysiert) — vermutlich eine spätere
  Formatgeneration, die vom aktuellen Legacy-Importer nicht abgedeckt wird. `OpenLegacyMap`
  gibt dafür einen klaren Fehler zurück (`HEIGHTMAP_WIDTH/HEIGHT fehlt`), statt abzustürzen.
- **Adl hatte urspr\u00fcnglich inkonsistente Blend-Aufl\u00f6sung innerhalb derselben Karte** (8 von 10
  Layern 476×476 statt 512×512 wie der Rest) — **behoben**: `ImportLegacyTextureSet` resampelt
  abweichende Layer jetzt bilinear auf die Stack-Aufl\u00f6sung, statt sie ohne Daten zu lassen.
  Nicht verlustfrei (Resampling-Artefakte bei starkem Auflösungsunterschied möglich), aber alle
  10 von 10 Layern haben jetzt echte Blend-Daten statt vorher nur 2 von 10.

`ImportLegacyTextureSet` / `ExportLegacyTextureSet` bündeln ini + alle Blend-BMPs zu einem
Aufwasch (inkl. Rekonstruktion der Legacy-Verzeichnisstruktur, z. B.
`.\resmap\fieldtexture\L1_A.BMP` → `<outDir>/resmap/fieldtexture/L1_A.BMP`).

## 2c. Vereinheitlichter "Karte öffnen/speichern"-Workflow (`LegacyMapProject`)

`core::legacy::OpenLegacyMap(iniPath)` / `SaveLegacyMap(project, outDir, stem)` bündeln alle vier
Module zu EINER Aktion, statt sieben Einzel-Importe mit manuellen Pfad-/Maßangaben zu verlangen:

- Auffindung der Begleitdateien: gleicher Namens-Stamm wie die `.ini`, case-insensitiv, im
  selben Verzeichnis (bestätigt für `.shbd`/`.shmd`/`.idm`/`.aid` über alle 4 Kartensets mit
  gültiger ini). **Ausnahme `.HTD`:** RouVal01s Heightmap-Datei heißt `darkVally.HTD`, nicht
  `RouVal01.HTD` - deshalb wird sie primär über das `#HeightFileName`-Feld der `.ini` aufgelöst
  (mit Namens-Stamm-Suche als Fallback).
- Fehlende/nicht ladbare Teile sind NICHT fatal (außer der `.ini` selbst) - jedes Modul wird
  einzeln geladen, was fehlt landet als Eintrag in `LegacyMapOpenReport`, der Rest lädt normal.
- **Verifiziert per vollem Open→Save→Reopen-Rundlauf gegen echte Kartensets** (Bera/Adl/RouVal01 -
  `tests/test_legacy_map_project.cpp`): Heightmap byte-exakt, Textur-Layer-Anzahl und
  -Auflösung, Block&Walk-Dimensionen, Objekt-Anzahl und erste Objekt-Position identisch nach dem
  Rundlauf. `Eld` schlägt wie erwartet beim Öffnen fehl (anderes Format, siehe oben).

## 2d. Weitere echte Kartensets (29 Karten aus vollständigem Client-Repository)

Der Nutzer hat ein umfangreicheres Set echter Karten (29 mit gültiger `.ini`) plus den
tatsächlichen `resmap/`-Wurzelordner bereitgestellt. Ergebnis des `OpenLegacyMap`-Rundlaufs:
26 von 29 laden die Heightmap vollständig (vorher mit der ursprünglichen Implementierung nur
23 von 29). Zwei Funde dabei:

- **`.HTD`-Dateien können zusätzliche Daten NACH dem reinen Höhenraster enthalten**
  (beobachtet: 4 Byte bei `BigCoast.HTD`, 80 Byte bei `UrgDark01.HTD`, 5268 Byte bei
  `UrgSwa01.HTD` - Bedeutung nicht gesichert). `ImportLegacyHtd` behandelte das bisher als
  Fehler (verhinderte den Import dieser Karten komplett) - werden jetzt unverändert erfasst
  und mitgeführt (`outTrailingBytes`-Parameter), damit ein Re-Export weiterhin byte-exakt
  bleibt, ohne den Import zu blockieren. **Verifiziert: alle drei betroffenen Dateien
  importieren jetzt erfolgreich UND exportieren byte-für-byte identisch zum Original.**
- **`X_Adl` referenziert einen fremden Kartenordner:** `#HeightFileName` zeigt auf
  `.\resmap\field\Adl\Adl.HTD` (Ordner `Adl`, nicht `X_Adl`) - vermutlich teilt sich die
  "X_"-Variante die Original-Heightmap. Kein Bug: der `Adl`-Ordner war in diesem konkreten
  Upload schlicht nicht enthalten (nur `X_Adl`) - mit einem vollständigen Client-Repository
  würde die bestehende Mehrfach-Wurzel-Pfadauflösung das automatisch korrekt finden.
- **`Windycave` und `X_Eld` bleiben ungeöffnet** (leere `.ini`, wie das bereits bekannte
  `Eld`) - bestätigt, dass dies ein eigenständiges, nicht abgedecktes Kartenformat ist, keine
  Einzelfälle.

## 3. Architekturprinzip (von der Legacy-Struktur übernommen)

Jede Legacy-Datei ist eigenständig lesbar, trägt eine eigene Versionskennung und ist über
Referenzen (Pfade, Hash) statt harter Kopplung mit den anderen verbunden — kein monolithisches
Kartenformat. Dieses Prinzip wird für TheSeed übernommen: **ein Modul, ein Dateiformat**
(`.tshm` für Heightmap, künftig eigene Formate für Texturing/Block&Walk/Objekt-Placement),
geladen über die bestehende `ModuleRegistry`.

## 4. `.nif`-Objektformat (Gamebryo/NetImmerse 20.0.0.4)

Reverse-Engineering-Methode: Vergleich mehrerer echter Dateien unterschiedlicher Größe (analog
zu `.shbd`), plus Nutzung des exakten Dateiendes als hartes Constraint bei kleinen Dateien
(`Eld_CD.nif`, 892 Byte; `AddSharpCD.nif`, 1335 Byte - beide mit identischer Block-Typ-Liste,
aber unterschiedlicher Vertex-/Dreiecksanzahl).

### Verifiziert
- **Datei-Header**: Signaturzeile, Version (uint32), Endian-Byte, User-Version (uint32),
  Block-Anzahl, Block-Typen-Liste (längenpräfixierte Strings), Block-Type-Index (uint16 je
  Block), Gruppen-Anzahl + Gruppen-Array. Kein Block-Größen-Table vorhanden - jeder Block muss
  vollständig und korrekt gelesen werden, um beim nächsten anzukommen (kein wahlfreier Zugriff).
- **`NiNode`-Szenengraph**: Name (Sized-String), `NiObjectNET`-Basis (Extra-Daten-Liste,
  Controller-Ref), `NiAVObject`-Basis (Flags **als uint16**, Translation, Rotation-3×3-Matrix,
  Skalierung, Properties-Liste, Collision-Ref), Kinder-Liste, Effekte-Liste. Verifiziert an
  "Scene Root" (Identitäts-Transform) und mehrstufiger Verschachtelung ("Object18" → zwei
  `NiTriStrips`-Kinder, beide Typen exakt wie in der Block-Typen-Liste vorhergesagt).
- **`NiMaterialProperty`**: Ambient/Diffuse/Specular/Emissive (je 3 Floats) + Glossiness + Alpha
  + 1 zusätzliches, immer-0-Feld = 15 Floats. Werte sehen durchgehend nach echten Materialfarben
  aus (z. B. reines Weiß `(1,1,1)`, oder `(0.588,0.588,0.588)`-Grau).
- **`NiTriStrips`/`NiTriStripsData`-Paar (nur für untexturierte Meshes)**: Nach
  `NiAVObject`-Basis folgen `data_ref`, `skin_instance_ref`, ein unbekanntes Byte (immer 0) und
  ein Freitext-Feld (in echten Dateien mit sichtlichem Platzhaltertext von Künstlern gefüllt,
  z. B. `"fdfdsafdafdsfdah"` oder `"origsdorig_12 - Default"` - vermutlich ein Shader-/
  Effekt-Name). Die zugehörige `NiTriStripsData` enthält: Vertex-Anzahl (uint32) +
  Vertices (3 Floats je) + ein unbekanntes uint16-Feld (immer 0) + Normalen-Flag + Normalen
  (falls vorhanden) + Bounding-Sphere (Center + Radius) + Vertexfarben-Flag + Vertexfarben
  (RGBA, falls vorhanden) + UV-Flags (untere 6 Bit = Anzahl UV-Sets) + UV-Koordinaten je Set +
  ein Ref-Feld (immer -1) + Dreieckszahl + Streifenzahl + Streifenlängen + Punkte-Flag (**1
  Byte**, nicht uint16!) + Punkt-Indizes je Streifen + ein konstanter 8-Byte-Trailer
  (`01 00 00 00 00 00 00 00` in beiden Testdateien, Bedeutung ungeklärt).
  **Verifiziert byte-exakt an 2 echten Dateien unterschiedlicher Vertex-/Dreieckszahl** - die
  Struktur landet nach dem Parsen exakt auf dem jeweiligen Dateiende. Dreiecksstreifen werden
  per Standard-Verfahren (alternierende Wicklung, entartete Dreiecke mit wiederholten Indizes
  übersprungen) in einzelne Dreiecke aufgelöst.
- **Massentest über alle 3436 echten `.nif`-Dateien** aus den bereitgestellten Kartensets: 17
  laden mit echter, korrekter Geometrie (0 Abstürze, alle anderen scheitern sauber mit
  Fehlermeldung statt falscher/korrupter Daten zu liefern).

### Teilweise entschlüsselt: `NiTexturingProperty` (noch nicht in Code umgesetzt)
Struktur bis zum Textur-Dateinamen **positionsgenau verifiziert** (berechnete Position landet
exakt auf dem unabhängig gefundenen Dateinamen `"top_wall_c256.dds"`):
- Kurze Basis: 2 Felder (uint32=0, int32=-1) - dasselbe verkürzte Muster wie bei
  `NiMaterialProperty`, nicht die volle `NiObjectNET`-Basis mit Namen
- `apply_mode` (uint32, z.B. 2 = vermutlich MODULATE)
- `texture_count` (uint32, beobachtet: 7 - Standard-Slot-Anzahl)
- 7 Textur-Slots (vermutete Standard-Reihenfolge: Base/Dark/Detail/Gloss/Glow/Bump/Decal), je:
  Vorhanden-Flag (1 Byte) + falls vorhanden ein `TexDesc` (Quell-Referenz int32 + Clamp-Modus
  uint32 + Filter-Modus uint32 + UV-Set-Index uint32 + Transform-Flag 1 Byte + 7 Floats falls
  Transform gesetzt, Feldaufteilung dieser 7 Floats ungeprüft). **Die `source_ref`-Werte beider
  belegten Slots (Base, Detail) zeigten exakt auf die laut Block-Typ-Liste erwarteten
  `NiSourceTexture`-Blockindizes** - starke Bestätigung der Struktur
- Trailer vor dem nächsten Block: 12 Nullbytes + int32(-1) + 1 Byte (Bedeutung ungeklärt)

Nach dem Dateinamen folgt bei `NiSourceTexture`: `pixel_data`-Referenz (int32) - **verifiziert:
zeigt exakt auf den laut Block-Typ-Liste erwarteten `NiPixelData`-Block** (weiteres starkes
Indiz für die Struktur) - danach mehrere Felder unklarer Bedeutung, dann beginnt `NiPixelData`.

`NiPixelData` enthält die eingebetteten Rohpixel-Daten (vermutlich ein Thumbnail für einen
Asset-Browser, da die eigentliche Textur ja separat als `.dds`-Datei vorliegt - bei der
untersuchten Datei 43704 Byte). **Das Größenfeld für diesen Rohdatenblock wurde empirisch
lokalisiert** (an der Stelle, wo Header-Ende + Rohdaten-Länge + der bekannte 17-Byte-Trailer
exakt auf den zweiten, unabhängig gefundenen Textur-Dateinamen führt). Die Kette
`NiTexturingProperty → NiSourceTexture(Dateiname 1) → NiPixelData(43704 Byte Rohdaten) →
NiSourceTexture(Dateiname 2, exakt getroffen)` ist damit lückenlos nachvollzogen.

**Einziges verbleibendes Puzzlestück:** der Header von `NiPixelData` VOR dem Größenfeld (Pixel-
Format, Bitmasken, vermutlich eine Mipmap-Level-Liste mit variabler Länge - bei der
untersuchten Datei 175 Byte) ist noch nicht feldweise entschlüsselt. Da die Länge dieses Headers
vermutlich von der Mipmap-Anzahl abhängt (variabel, nicht fest), bräuchte eine robuste
Entschlüsselung eine zweite Vergleichsdatei mit abweichender Mipmap-Anzahl - analog zur
`.shbd`-Methode. Ohne das bleibt `LoadNifMesh` bei texturierten Meshes vorerst "nicht
unterstützt", auch wenn die Struktur jetzt fast vollständig verstanden ist.

### Noch nicht unterstützt
- **Texturierte Meshes** (die große Mehrheit der 3436 Dateien): Struktur bis zu den rohen
  Pixel-Daten von `NiPixelData` jetzt lückenlos nachvollzogen (siehe oben), aber dessen
  variabler Mipmap-Header selbst noch nicht feldweise entschlüsselt - `LoadNifMesh` erkennt
  diesen Fall weiterhin zuverlässig (über die Properties-Liste des `NiTriStrips`-Blocks) und
  schlägt sauber fehl, statt falsch zu parsen.
- Weitere unbekannte Block-Typen aus den echten Dateien (nicht analysiert): `NiCollisionData`,
  `NiStringExtraData`, `NiPointLight`, `NiStencilProperty`, `NiAlphaProperty`, `NiTriShape`/
  `NiTriShapeData` (Dreiecksliste statt -streifen), Skinning/Animation-Blöcke.
- Kein GL-Rendering der extrahierten Geometrie integriert - `LoadNifMesh` liefert Vertex-/
  Dreiecksdaten, aber `ObjectMarkerRenderer` zeigt weiterhin nur Platzhalter-Marker an. Nächster
  Schritt für echte Objekte in der 3D-Vorschau.

### Endgültig korrigiert nach mehreren Fehlversuchen (Material-Float-Anzahl & Basis-Größen)
Zwei kompensierende Fehlannahmen fielen erst bei einer dritten Vergleichsdatei
(`R_Helga01GL.nif`, texturiert, aber mit `NiAlphaProperty`/zweitem `NiVertexColorProperty`
zwischen Material und Texturing) auseinander, nachdem sie sich bei den ersten beiden
Testdateien zufällig gegenseitig aufgehoben hatten:
- `NiTexturingProperty` hat die **volle 12-Byte-`ObjectNetBase`** (name_len+extra_count+
  controller), NICHT die zuvor angenommene verkürzte 8-Byte-Version (0,-1)
- `NiMaterialProperty` hat ein **optionales 15. Float** (immer 0.0) NACH den 14 Kernfeldern -
  vorhanden bei untexturierten Meshes (kein `NiTexturingProperty` in der Properties-Liste des
  zugehörigen `NiTriStrips`), NICHT vorhanden bei texturierten. Grund für die Korrelation
  ungeklärt, aber an 4 echten Dateien (`Eld_CD.nif`/`AddSharpCD.nif` untexturiert → 15 Floats;
  `santuary.nif`/`R_Helga01GL.nif` texturiert → 14 Floats) byte-exakt bis zum Dateiende
  verifiziert
- `NiPixelData` endet mit einem **konditionalen 8-Byte-Trailer** (Bedeutung ungeklärt, Werte
  variieren zwischen Dateien) - vorhanden, wenn NICHT direkt eine weitere `NiSourceTexture`
  folgt (z.B. vor `NiTriStripsData`), sonst nicht (die 8 Byte sind dann Teil der
  17-Byte-`NiSourceTexture`-Präambel)
- `NiAlphaProperty` = 15 Byte (volle 12-Byte-Basis + flags(u16) + threshold(u8))
- `NiStencilProperty` noch nicht verifiziert (Platzhalter-Länge, blockiert aktuell ~270 echte
  Dateien laut Massentest)

**Lehre:** Zwei falsche Annahmen, die sich zufällig zu einem korrekten Gesamtergebnis addieren,
sind schwerer zu entdecken als eine einzelne falsche Annahme - erst eine dritte, strukturell
andere Vergleichsdatei deckte den Widerspruch auf. Bestätigt erneut den Wert, IMMER mehrere
unterschiedlich aufgebaute echte Dateien zu prüfen, nicht nur zwei ähnliche.

### Performance-Absicherung
Nach den obigen Korrekturen fiel ein separates Problem auf: bei fehlausgerichteten/nicht
unterstützten Dateien konnten einzelne Zählfelder (z.B. Vertex-Anzahl) als große, aber noch
innerhalb der ursprünglichen Sicherheitsgrenze (2 Millionen) liegende Zahlen gelesen werden,
was zu sehr langsamer (nicht unendlicher, aber spürbar träger) Verarbeitung über alle 3436
Dateien führte. Alle Zählfelder haben jetzt kontextspezifische, deutlich engere Obergrenzen
(z.B. Vertex-Anzahl max. 200.000, Kind-Knoten max. 10.000) statt einer einzigen generischen
Grenze - Massentest über alle 3436 Dateien läuft jetzt in unter 90 Sekunden.

### Achsen-Konvention: auch .nif-Vertices sind Z-up
Nutzer-Feedback: platzierte Objekte standen in falscher Rotation. Ursache gefunden: `.nif`-
Vertex-Positionen und -Normalen wurden bisher OHNE Achsen-Remap direkt übernommen. Wie das
gesamte übrige Legacy-Format (siehe `ObjectPlacementIO.cpp`) ist auch `.nif` Z-up (X/Y =
horizontale Ebene, Z = Höhe), intern durchgängig Y-up. Gleiches Remap wie bei der
Objekt-Platzierung angewendet (Achsentausch Y↔Z, keine Vorzeichenumkehr): `{x,y,z}` (Legacy) →
`{x,z,y}` (intern). Reine Umsortierung gelesener Werte, keine Änderung der Byte-Anzahl - alle
bisherigen Verifikationen (Vertex-/Dreieckszahlen, Dateiende-Treffer) bleiben gültig.

### Bekannter, aber unverifizierter Fix: Diffuse-Textur-Spiegelung
Nutzer-Feedback: die Kartentextur erschien gespiegelt. Ohne Screenshot nicht abschließend
diagnostizierbar. Wahrscheinlichste Ursache: die Original-Engine ist vermutlich DirectX-
basiert (typisch für Spiele dieser Ära/Region), und DirectX/OpenGL haben unterschiedliche
Konventionen für die vertikale Textur-Achse. Die V-Komponente der Diffuse-Textur-Koordinate
wurde daher gespiegelt (`Renderer.cpp`, `DrawTerrainMesh`-Shader) - bewusst NUR die Diffuse-
Textur, NICHT die Blend-Gewichts-Textur (deren Ausrichtung war bereits vor dieser Session an
Heightmap/Objekt-Positionen validiert). Falls die Karte danach weiterhin falsch orientiert
erscheint, war die Ursache woanders.

### UVScaleDiffuse: korrekte Bedeutung gefunden (Nutzer-Screenshots zeigten vervielfachtes Emblem)
Nutzer-Screenshots zeigten ein kreisrundes Boden-Emblem mehrfach wiederholt (vermutlich
eigentlich als einmaliges Bodendekor gedacht). Ursache: die bisherige Diffuse-UV-Formel
(`weltposition / 500`) erzwang eine FESTE Wiederholung alle 500 Welteinheiten für JEDE Textur,
unabhängig von der Kartengröße. Bei einer 12800 Einheiten breiten Karte (Rou) und
`UVScaleDiffuse=4` bzw. `5` (echte Werte aus `Rou.ini`) hätte sich eine Textur damit ca. 100x
statt der vermutlich beabsichtigten 4-5x wiederholt.

**Korrigierte Deutung:** `UVScaleDiffuse` ist die Anzahl Wiederholungen ÜBER DIE GESAMTE
KARTENFLÄCHE (Standard-Konvention in den meisten Editoren) - `uv = (weltposition/kartenspanne)
* UVScaleDiffuse`, dieselbe normalisierte 0..1-Basis wie beim Blend-Gewichts-Lookup, nur
zusätzlich skaliert. Nicht anhand einer Referenzimplementierung verifizierbar (keine
zugänglich), aber deutlich besser durch die echten `.ini`-Werte gestützt als die vorherige
Annahme. Falls Bodentexturen danach immer noch falsch wirken, wäre der nächste Schritt, echte
Screenshots aus dem Originalspiel (falls verfügbar) zum Abgleich heranzuziehen.

Die zwischenzeitlich vermutete DirectX/OpenGL-V-Flip-Notwendigkeit für die Diffuse-Textur wurde
in diesem Zuge wieder zurückgenommen (unbestätigte Spekulation, durch den UV-Scale-Fix ersetzt) -
falls nach diesem Fix weiterhin eine Links-Rechts- oder Oben-Unten-Spiegelung sichtbar ist
(nicht nur Wiederholung), ist das ein separates, noch offenes Problem.

### Diffuse-Textur war relativ zur Blend-Struktur oben/unten vertauscht ("finaler" Fix -
    SPÄTER ALS FALSCH ERKANNT, siehe Nachtrag unten)
Nutzer-Klarstellung: Beim Vergleich zweier Bildbereiche im selben Screenshot zeigte sich, dass
die Blend-/Heightmap-Struktur (korrekt ausgerichtet) und die darüber gelegte Diffuse-Textur
systematisch oben/unten vertauscht waren - nicht nur bei diesem einen Emblem, sondern
durchgängig bei allen Texturen. Die vorherige UV-Scale-Korrektur (siehe oben) war zwar
notwendig (behebt die Wiederholungshäufigkeit), aber nicht ausreichend - zusätzlich musste die
V-Achse NUR für die Diffuse-Textur-Zuordnung gespiegelt werden, relativ zur (unverändert
korrekten) Blend-Gewichts-Achse: `diffuseUv = vec2(mapUv.x, 1.0 - mapUv.y) * uvScale` statt
`mapUv * uvScale`. Blend-Lookup bleibt exakt wie zuvor (`mapUv` unverändert), da dessen
Ausrichtung an Heightmap/Objekt-Positionen bereits aus früheren Sessions validiert war - nur
die neu hinzugekommene Diffuse-Textur-Sampling-Richtung war betroffen.

**NACHTRAG (Folgesitzung, v0.44.6) - dieser Fix war die falsche Richtung, zurückgenommen:**
Nutzer-Vergleich zweier Screenshots (2D View MIT rotem Block/Walk-Overlay vs. Nahaufnahme
derselben Steintextur OHNE Overlay) zeigte: das Block/Walk-Overlay (nutzt dieselbe `mapUv`-
Achse wie Blend) ist korrekt ausgerichtet, die Diffuse-Textur trotz des obigen Flips WEITERHIN
oben/unten vertauscht - exakt das Symptom, das dieser Fix eigentlich beheben sollte. Da
Block/Walk und Blend denselben `mapUv` nutzen und beide korrekt sind, war der Flip
nachweislich die falsche Richtung (vermutlich beruhte die ursprüngliche "Klarstellung" auf
einem Vergleich, der selbst nicht eindeutig war - ohne das exakte damalige Screenshot-Paar
lässt sich das im Nachhinein nicht mehr rekonstruieren). Der Flip wurde entfernt -
`diffuseUv = mapUv * uvScale`, identisch zur Blend-Achse. Der DDS-Decoder selbst wurde
zusätzlich geprüft (liest Zeilen originalgetreu top-down, kein Flip im Code) und als nicht
ursächlich eingestuft. **Falls die Textur nach diesem Revert WEITERHIN falsch orientiert ist,
liegt die Ursache nicht in der UV-Zuordnung oder dem Decoder - das wäre dann ein neues,
bisher nicht untersuchtes Problem** (z.B. eine horizontale statt vertikale Spiegelung, oder
etwas an der Heightmap-Achse selbst).

**Übergreifende Lehre:** dies ist bereits die ZWEITE Kehrtwende bei diesem exakten Flip
(vorher: hinzugefügt → zurückgenommen → wieder hinzugefügt → jetzt wieder zurückgenommen) -
ein Muster, das nahelegt, dass Flip-Bugs dieser Art besonders leicht falsch diagnostiziert
werden, wenn der Vergleich nicht durch ein eindeutiges, gemeinsames Referenzelement (wie hier
das Block/Walk-Overlay) abgesichert ist. Für zukünftige Orientierungs-Bugs: IMMER nach einem
Vergleich mit einem bereits als korrekt bestätigten Referenzelement im SELBEN Bild fragen,
nicht nur nach einer allgemeinen "sieht falsch aus"-Beschreibung.

### NiStencilProperty vermessen + zweiter konditionaler Trailer gefunden
`NiStencilProperty` = kurze Basis (0,-1, 8 Byte) + 7 unbekannte uint32-Felder + 1 Einzelbyte +
ein eingebettetes Namens-/Beschreibungsfeld (Sized-String, variable Länge). Byte-genau
vermessen an `Rou_M_Tube.nif` (Landmarken-Suche: die berechnete Position trifft exakt den
Beginn der nachfolgenden `NiMaterialProperty`, erkennbar an deren charakteristischer
kurzer Basis + Farbfloats beginnend bei 1.0).

Zusätzlich gefunden: der 8-Byte-Trailer am Ende von `NiTriStripsData` ist EBENFALLS
konditional (wie bei `NiPixelData`) - er fehlt, wenn direkt ein weiteres `NiTriStrips` folgt
(zweites/drittes Mesh desselben Objekts, z.B. `Rou_M_Tube.nif` mit 2 Meshes,
`rouval_Tower.nif` mit mehreren). Mit beiden Fixes: Massentest über alle 3436 echten Dateien
verbessert sich von 257 (7.5%) auf 579 (16.9%) - mehr als verdoppelt.

**Verbleibend größte Blocker:** unbekannte Blocktypen an Block 3-6 (vermutlich
`NiCollisionData`, `NiStringExtraData`, `NiPointLight` je nach Datei) sowie ca. 180 Dateien mit
generischem Header-Lesefehler (andere NIF-Version oder korrupte Testdatei, nicht untersucht).

### Objekt-Texturierung implementiert
`NifMeshRenderer` lädt jetzt echte Diffuse-Texturen für Objekte (dieselbe `DdsImage`-
Infrastruktur wie die Terrain-Texturen), sofern ein Mesh-Teil sowohl eine Textur-Dateireferenz
(`core::NifMeshPart::diffuseTexture`, siehe oben) als auch eigene UV-Koordinaten mitbringt.
Vertex-Puffer um UV-Attribut erweitert (8 statt 6 Floats/Vertex: Position+Normale+UV), Textur-
Cache verhindert mehrfaches Laden derselben Datei über mehrere Objekte/Karten hinweg. Ohne
Textur oder UVs bleibt der bisherige Fallback (extrahierte Materialfarbe) unverändert.

**Nicht verifiziert:** ob NIF-eigene UV-Koordinaten dieselbe V-Achsen-Behandlung wie die
computed Terrain-UVs brauchen (dort war ein Flip nötig, siehe oben) - bei authored Mesh-UVs
(nicht wie beim Terrain aus der Weltposition berechnet, sondern direkt aus der Datei) ist das
nicht zwingend derselbe Fall. Aktuell ohne Flip implementiert (direkte Übernahme) - falls
Objekt-Texturen nach dem Testen ebenfalls gespiegelt erscheinen, ist das der erste
Verdächtige.

### Weitere Blocktypen vermessen (0.16.0): 579 → 1001 von 3436 Dateien (16.9% → 29.1%)

Systematisch weitere `.nif`-Blocktypen anhand der öffentlichen, community-gepflegten
`nif.xml`-Formatdokumentation (NifTools-Projekt, GitHub) als Ausgangshypothese vermessen, dann
byte-exakt an mehreren echten, strukturell unterschiedlichen Dateien verifiziert (Landmarken-
Technik: berechnete Länge muss exakt auf das Namens-/Datenfeld des bekannten Folgeblocks
treffen). Die öffentliche NIF-Formatdokumentation ist unabhängig vom Fiesta-Server-Cleanroom-
Constraint (das betrifft ausschließlich proprietäre Server-Binärdateien) - sie diente hier nur
als Hypothesenquelle, nicht als Ersatz für die Verifikation an echten Dateien.

**`NiCollisionData`** (größter Einzel-Hebel, 852 von 3436 Dateien blockiert): `NiCollisionObject`-
Basis (nur `target`-Ptr auf das besitzende `NiAVObject`, int32, KEIN Name) + `propagation_mode`
(u32) + `collision_mode` (u32) + `use_abv` (u8) + optional eine Bounding-Volume-Struktur (nur
falls `use_abv != 0`): `collision_type` (u32: 0=Sphere, 1=Box, 2=Capsule, 3=Union, 4=Halfspace)
+ typspezifische Floats (Sphere 16 Byte, Box 60 Byte, Capsule 32 Byte). Union/Halfspace in den
3436 Testdateien nicht beobachtet - bewusst nicht geraten, schlägt sauber fehl statt falsch
weiterzulesen. Byte-exakt verifiziert an `AdlF_maingate_A.nif` (Box-Variante): `target=0` zeigt
auf den eigenen Scene-Root-`NiNode`, `collision_type=1` (Box) mit Einheits-Rotationsmatrix - die
berechnete Gesamtlänge (77 Byte) landet exakt auf dem Namensfeld des folgenden `NiTriStrips`
(`"AdlF_maingate01"`, längenpräfixiert).

**`NiStringExtraData`** (331 Dateien) und **`NiIntegerExtraData`** (88 Dateien): `NiExtraData`
erbt NICHT von `NiObjectNET` (anders als Properties/Nodes), sondern direkt von `NiObject` - die
Basis ist NUR ein Namensfeld (Sized-String), OHNE Extra-Daten-Liste und OHNE Controller-Ref.
`NiStringExtraData` = Basis + ein weiteres Sized-String-Feld (Wert). `NiIntegerExtraData` =
Basis + ein uint32-Feld. Byte-exakt verifiziert an `olber_sword.nif`: Name =
`"LODDistance = 0.0\r\n"` (Freitext direkt im Namensfeld, kein separater Wert nötig), landet
exakt auf die folgende `NiStencilProperty`.

**Bug gefunden und behoben:** bei ~1% der `NiIntegerExtraData`-Instanzen (bisher nur im Kontext
eines benannten Multi-Textur-Blend-Shaders wie `"VCAlphaTextureBlender"` beobachtet, siehe
`bossroom_wall.nif`) geht dem Namensfeld ein zusätzliches int32-Feld mit Wert -1 voraus
(vermutlich Rest eines alten Ketten-/Controller-Zeigers). `0xFFFFFFFF` ist als String-Länge nie
plausibel, daher per `PeekU32` sicher erkennbar und übersprungen, ohne bei den übrigen ~99% der
Instanzen etwas zu verändern. Löste 29 von 34 betroffenen Dateien direkt; die restlichen 5
scheitern jetzt sauber an einer noch offenen, andersartigen Struktur-Abweichung im selben Objekt
(ein zusätzliches Namensfeld nach dem letzten von mehreren `NiIntegerExtraData` in Folge, nicht
weiter untersucht, betrifft nur diese 5 Dateien).

**`NiBillboardNode`** (120 Dateien): wie `NiNode`, plus ein zusätzliches uint16-Feld
(`billboard_mode`) am Ende. Rendering-seitig aktuell wie ein normaler `NiNode` behandelt (kein
eigenes Billboard-Verhalten im Editor nötig, da Objekte nur platziert, nicht live animiert
gerendert werden müssen).

**`NiTransformController` → `NiTransformInterpolator` → `NiTransformData`** (zweitgrößter
Hebel, 247 Dateien direkt blockiert): komplette Animationskette für Transform-Keyframes,
vollständig byte-exakt verifiziert UND inhaltlich bestätigt an `AdlF_Flower.nif` (eine
schaukelnde Blumen-Animation, Y-Rotation 0→-0.042rad→0 über 6.667 Sekunden):
- `NiTimeController`-Basis (gemeinsam für "einfache" Controller wie diesen und
  `NiAlphaController`): `next_controller`(Ref,i32) + `flags`(u16) + `frequency`(f32) +
  `phase`(f32) + `start_time`(f32) + `stop_time`(f32) + `target`(Ptr,i32) = 26 Byte.
  `NiSingleInterpController` ergänzt `interpolator_ref`(i32) = 30 Byte gesamt.
- `NiTransformInterpolator`: `Translation`(Vector3) + `Rotation`(Quaternion, 4 Floats) +
  `Scale`(float) + `data_ref`(i32) = 36 Byte. Nicht angewandte Komponenten sind mit `-FLT_MAX`
  (`0xFF7FFFFF`) belegt, nicht 0 - Sentinel für "keine Override-Bewegung".
- `NiTransformData`: `num_rotation_keys`(u32) + falls `>0` `rotation_type`(u32) + entweder
  (Typ 4, `XYZ_ROTATION_KEY`) drei skalare `KeyGroup`s für X/Y/Z-Rotation, oder (sonst)
  `num_rotation_keys` Quaternion-Keys (Zeit+4 Floats, bei TBC-Typ +3 weitere - Quaternion-Keys
  haben laut Format nie eigene Tangenten) - danach `Translation`-`KeyGroup` (Vector3) und
  `Scale`-`KeyGroup` (Skalar). `KeyGroup<T>` = `num_keys`(u32) + falls `!=0` `key_type`(u32:
  1=LINEAR, 2=QUADRATIC, 3=TBC - **0 ist laut offiziellem Enum kein gültiger Wert**, siehe
  unten) + `num_keys` Einträge (LINEAR: Zeit+Wert; QUADRATIC: +2 Tangenten derselben Größe wie
  der Wert; TBC: +3 Floats Tension/Bias/Continuity).

**`NiAlphaController` + `NiFloatInterpolator` + `NiFloatData`** (71 Dateien direkt blockiert):
`NiAlphaController` nutzt exakt dieselbe 30-Byte-`NiSingleInterpController`-Basis wie
`NiTransformController`, ohne weitere eigene Felder. `NiFloatInterpolator` = aktueller
Wert(float) + `data_ref`(i32) = 8 Byte. `NiFloatData` = eine einzelne `KeyGroup<float>`.
Byte-exakt verifiziert UND inhaltlich bestätigt an `AdlFH_field_burn_ground.nif`: Alpha
oszilliert 0.5→1.0→0.5 über 3.33 Sekunden (QUADRATIC-Keys) - ein flackernder Brand-Boden-Effekt.

**Bug in bereits "verifiziertem" Code gefunden: `NiTexturingProperty`s Textur-Transform war zu
kurz.** Die pro-Slot-Transform (`TexDesc`, nur falls `hasTransform=1`) wurde bisher als 7 Floats
(28 Byte: Translation-U/V, Scale-U/V, Rotation, Center-U/V) angenommen. Tatsächlich sind es 8
Felder / 32 Byte: zwischen Rotation und Center steckt ein zusätzliches `transform_type`-Feld
(u32) - als Float fehlinterpretiert erschien es als winziger Denormal-Wert (z.B. `1.4e-45` für
den Integer-Wert 1), was den Bug lange unauffällig machte, solange nur ein einzelner
transformierter Slot pro Objekt vorkam. Gefunden an `SD_Vale01_machine02.nif` (4 transformierte
Slots in einer `NiTexturingProperty`): mit 32 statt 28 Byte pro Slot ergeben sich für alle 4
Slots identische, plausible `transform_type=1` und `center=(0.5, 0.5)`-Werte, UND die vier
`source_ref`-Werte (35, 37, 39, 41) treffen exakt auf die vier `NiSourceTexture`-Blöcke der
Datei. Dieser Bug betraf potenziell auch die schon gebaute, aber noch unverifizierte
Objekt-Texturierung (siehe v0.15.0) bei Objekten mit UV-transformierten Texturen.

### Offenes Problem: `NiTextureTransformController` - inkonsistente Blocklänge (NICHT gelöst)

**Bewusst nicht implementiert, um keine stille Datenkorruption zu riskieren.** Erste Hypothese
(analog zu `NiAlphaController`: 30-Byte-Basis + `Unknown2`(byte) + `texture_slot`(u32) +
`operation`(u32) = 39 Byte) erwies sich bei genauerer Prüfung als unvollständig: eine
Basis-Variante mit einem zusätzlichen führenden uint32-Feld (Wert 0 oder 1, Position VOR
`next_controller`) wurde ebenfalls beobachtet. Entscheidend ist aber: **selbst bei identischer
Feld-Interpretation zeigten aufeinanderfolgende `NiTextureTransformController`-Instanzen
IM SELBEN OBJEKT unterschiedliche Gesamtlängen** (39 vs. 43 Byte), beide für sich genommen
byte-exakt plausibel:

- An `SD_Vale01_machine02.nif` (6 aufeinanderfolgende Instanzen): alle 6 einheitlich 39 Byte,
  mit einer sauberen `next_controller`-Verkettung (17→18→19→20→21→22→(-1)) und `target=16`
  (die gemeinsame `NiTexturingProperty`) bei jeder Instanz - **hier eindeutig 39 Byte**.
- An `AdlFH_field_burn_ground.nif` (2 Instanzen): erste Instanz 39 Byte (verkettet `next=13`
  auf die zweite), zweite Instanz aber **43 Byte** (verifiziert über `target=11` und
  `interpolator_ref` mit exaktem Treffer auf die jeweils erwarteten Nachbarblöcke) - 4 Byte
  mehr als die erste, obwohl beide denselben Typnamen tragen und strukturell identisch sein
  sollten.

Zusätzlich zeigte die von `NiTextureTransformController` referenzierte `NiFloatInterpolator` in
einem Fall 12 statt der sonst überall (u.a. bei `NiAlphaController`) verifizierten 8 Byte (ein
zusätzliches Float zwischen Wert und `data_ref`) - ebenfalls ohne erkennbares
unterscheidendes Merkmal.

**Kein zuverlässiges Unterscheidungsmerkmal gefunden**, das VOR dem Lesen verrät, welche
Variante vorliegt (das einzige auffällige Bit-Feld, ein führendes 0/1-Flag, korrelierte in
`machine02.nif` nachweislich NICHT mit der Länge - dort waren alle 6 Instanzen trotz
alternierendem Flag einheitlich 39 Byte). Da ein falscher Griff hier - anders als bei den oben
beschriebenen, sauber verifizierten Blocktypen - das Risiko birgt, in einem unglücklichen Fall
eine falsche Byte-Position als Erfolg durchzureichen (der Bounds-Check des `ByteReader` fängt
nicht jede Fehlausrichtung ab, wie diese Untersuchung zeigte), bleibt
`NiTextureTransformController` bewusst **nicht unterstützt** (110 von 3436 Dateien direkt
blockiert). **Nächster Schritt:** weitere Instanzen mit unterschiedlichem `operation`-Wert
gezielt vergleichen (Verdacht: die Länge könnte von der Art der animierten UV-Operation
abhängen, z.B. Translation vs. Rotation vs. Skalierung mit unterschiedlicher Parameterzahl),
oder eine Datei mit GENAU EINER Instanz suchen (in den 3436 Testdateien kam keine einzige mit
nur einem Vorkommen vor - sie treten immer in Gruppen von 2+ auf).

### Verbleibend größte Blocker nach 0.16.0
`NiTriShape`/`NiTriShapeData` (156 Dateien, alternative Geometrie-Repräsentation mit
Dreieckslisten statt -streifen - noch nicht angegangen), `NiTextureTransformController` (110,
siehe oben), `NiLODNode` (93), `NiParticleSystem`-Familie, `NiMaterialColorController` +
`NiPoint3Interpolator` (ähnliches Cluster wie Alpha/Transform, noch nicht verifiziert),
`NiTextureEffect`, `NiDirectionalLight`, sowie ca. 177 Dateien mit generischem
Header-Lesefehler (andere NIF-Version oder korrupte Testdatei, nicht untersucht).

### `NiTriShape`/`NiTriShapeData` implementiert + ein weiterer latenter Bug in bereits "verifiziertem" Code gefunden (1001 → 1047 von 3436, 29.1% → 30.5%)

**`NiTriShape`**: teilt sich den kompletten Kopf byte-exakt mit `NiTriStrips` (`NiTriBasedGeom`-
Basis: `AVObjectBase` + `data_ref` + `skin_instance_ref` + ein unbekanntes Byte + Freitext-Feld).
Bewusst als separate Funktion dupliziert statt mit `NiTriStrips` geteilt, um den bereits
verifizierten `NiTriStrips`-Pfad nicht anzufassen. Byte-exakt verifiziert an
`BH_Albi_Ground.nif` und `BerFrz01_IceSmog.nif`: Namen ("Splited", "PolyMesh"), Properties- und
Extra-Daten-Referenzen sowie `data_ref` treffen jeweils exakt auf die im Block-Typ-Index
vorhergesagten Nachbarblöcke.

**`NiTriShapeData`**: teilt sich den kompletten Vertex-/Normalen-/Farben-/UV-Kopf mit
`NiTriStripsData` bis einschließlich des gemeinsamen `num_triangles`(u16)-Felds (siehe
`NiTriBasedGeomData`-Basisklasse), divergiert danach: statt Streifen eine FLACHE Dreiecksliste
- `num_triangle_points`(u32, = `num_triangles`×3) + `has_triangles`(u8) + falls vorhanden
`num_triangle_points` Vertex-Indizes (u16, direkt 3er-Gruppen, keine Streifen-Expansion nötig)
+ `num_match_groups`(u16) + je Gruppe `num_vertices`(u16) + Vertex-Indizes (geteilte-Normalen-
Gruppen, nicht weiterverwendet). Gleicher konditionaler 8-Byte-Trailer wie `NiTriStripsData`.

**Verifikationsmethode:** da in den 3436 Testdateien `NiTriShape` fast ausschließlich in
Kombination mit Partikeleffekten auftritt (die noch nicht unterstützte Blocktypen wie
`NiParticleSystem` benötigen), gibt es KEINE Datei, die allein durch diesen Fix vollständig
ladbar wird. Stattdessen wurde die Korrektheit an `BerFrz01_IceSmog.nif` durch DREI
aufeinanderfolgende, vollständig durchlaufene `NiTriShape`+`NiTriShapeData`-Zyklen verifiziert
(jeder landet exakt auf dem nächsten Block), UND durch explizite Prüfung der resultierenden
Geometrie: alle 3 Mesh-Teile haben einen maximalen Dreiecks-Index von exakt `vertexCount-1`
(72/72, 72/72, 12/12 Vertices) - keine Out-of-Bounds-Indizes, plausible Dreiecks-Vertex-
Verhältnisse. Der Massentest bestätigt zusätzlich: `NiTriShape` selbst verschwindet komplett
aus der Fehlerliste (vorher 156 Dateien direkt blockiert).

**Latenter Bug gefunden (betrifft AUCH das bereits seit v0.13 "verifizierte" `NiTriStripsData`):**
`Num Vertices` ist laut Format ein **uint16**, gefolgt von **Keep Flags(u8) + Compress
Flags(u8)** - NICHT ein einzelnes uint32 wie bisher angenommen. Der alte u32-Read war nur
deshalb "byte-exakt verifiziert", weil Keep-/Compress-Flags in allen bisher getesteten
Referenzdateien zufällig 0 waren (macht den u32-Wert zahlengleich zum echten u16-Wert - ein
klassischer Fall von zwei kombinierten Feldern, die sich per Zufall wie ein einzelnes größeres
Feld verhalten). Gefunden an `BerFrz01_IceSmog.nif` (`NiTriShapeData`, Keep-Flags=0x33=51,
ungleich 0): mit dem alten u32-Read ergab sich eine absurde "Vertex-Anzahl" von 3.342.408; mit
der Korrektur ergeben sich 72 plausible, sogar radialsymmetrische Vertex-Koordinaten (z.B.
`v1.y ≈ v4.x ≈ 492.24`). Fix in BEIDEN Funktionen angewendet (`ParseNiTriStripsData` UND
`ParseNiTriShapeData`, da geteilte Basisklasse) - Massentest bestätigt: reiner Gewinn (+39
Dateien), keine einzige vorher erfolgreiche Datei betroffen (byte-identisches Verhalten, wenn
die beiden Flag-Bytes 0 sind, was für die weit überwiegende Mehrheit der Dateien zutrifft).

**Zusätzlich gefunden und behoben:** `NiTexturingProperty`s Basis ist NICHT einheitlich - bei
den meisten Dateien die volle 12-Byte-`ObjectNetBase` (siehe v0.14.0-Korrektur), aber bei
manchen Dateien (bisher nur beobachtet, wenn die Textur von einem `NiTriShape` statt
`NiTriStrips` referenziert wird) fehlt das `numExtra`-Feld komplett - kurze 8-Byte-Basis
(`name_len=0` + `controller` direkt). Per Peek unterscheidbar (eine echte `numExtra`-Anzahl ist
laut `ParseObjectNetBase` nie größer als 1000, ein "kurzer" Controller-Ref an dieser Stelle oft
`-1` = `0xFFFFFFFF`). Byte-exakt verifiziert an `BH_Albi_Ground.nif`: `source_ref=11` trifft
exakt auf die folgende `NiSourceTexture`.

**Sackgasse, bewusst nicht weiterverfolgt:** kurzzeitig wurde vermutet, der seit v0.14.0
bekannte konditionale 8-Byte-Trailer von `NiPixelData` sei in manchen Fällen nur 4 Byte lang
(Indiz an `BH_Albi_Ground.nif`: eine `"00000000 00000000 ffffffff"`-Landmarke schien 4 Byte
früher zu passen als mit dem 8-Byte-Trailer berechnet). Ein globaler Test dieser Hypothese
verursachte einen massiven Einbruch im Massentest (1008 → 85 Dateien!) und wurde sofort
zurückgerollt - der 8-Byte-Trailer ist für die weit überwiegende Mehrheit korrekt.
`BH_Albi_Ground.nif`s eigentliches Problem an dieser Stelle bleibt ungeklärt (nicht
weiterverfolgt, da eine andere, eng begrenzte Anomalie in exakt dieser einen Datei plausibler
ist als eine falsche allgemeine Regel).

### Bekannter, noch offener Sonderfall: `NiTriShapeData`/`NiTriStripsData` mit `num_vertices=0`
Bei ca. 26 verbleibenden Dateien schlägt `NiTriShapeData` weiterhin fehl, in mindestens einem
untersuchten Fall (`BeraM_Wood3.nif`) mit `num_vertices=0` (kein gespeichertes Vertex-Array -
vermutlich ein geskinntes Mesh, dessen tatsächliche Positionen zur Laufzeit aus
Knochen-Transformationen berechnet werden, `skin_instance` wurde nicht geprüft). Direkt nach
dem `has_vertices=0`-Byte folgen Werte, die nicht als plausible `has_normals`/`num_uv_sets`
interpretierbar sind (z.B. `has_normals=32` statt 0/1) - Verdacht: bei `num_vertices=0` werden
die nachfolgenden optionalen Datenblöcke (Normalen/Farben/UVs) MÖGLICHERWEISE komplett
ausgelassen statt als leere/falsche Flag-Bytes serialisiert, analog zu anderen konditionalen
Mustern in diesem Format (siehe `NiPixelData`-Trailer). Nicht abschließend verifiziert - nächster
Schritt wäre ein direkter Vergleich mit einer erfolgreich geparsten `NiTriStripsData`-Datei, die
ebenfalls `num_vertices=0` hat (falls eine solche existiert), um die Struktur einzugrenzen.

### Verbleibend größte Blocker nach diesem Fix
`NiTextureTransformController` (134, bewusst nicht umgesetzt, siehe oben), `NiLODNode` (95),
177 Dateien mit generischem Header-Lesefehler (nicht untersucht), diverse verbleibende
`NiTriStripsData`/`NiNode`-Fehlschläge an späten Blockindizes (vermutlich weitere, noch nicht
entdeckte Struktur-Varianten ähnlich den oben beschriebenen - jeweils nur wenige Dateien
betroffen, kein einzelner dominanter Blocker mehr erkennbar), `NiMaterialColorController` (65),
`NiParticleSystem`-Familie, `NiTextureEffect` (47), `NiDirectionalLight`.

### `NiMaterialColorController` + `NiPoint3Interpolator` + `NiPosData` implementiert (Massentest unverändert bei 1047 - siehe unten warum)

Gleiche Methode wie bei `NiAlphaController`/`NiTransformController`: `NiMaterialColorController`
nutzt dieselbe 30-Byte-`NiSingleInterpController`-Basis, plus ein zusätzliches
`target_color`(u16)-Feld (welcher Materialfarbkanal animiert wird). `NiPoint3Interpolator` =
aktueller Wert(Vector3, 12 Byte) + `data_ref`(i32). `NiPosData` = eine einzelne
`KeyGroup<Vector3>`. Byte-exakt verifiziert UND inhaltlich bestätigt an `Eff_2.nif`: `target=15`
zeigt exakt auf die zugehörige `NiMaterialProperty`, `interpolator_ref=18` exakt auf die
folgende `NiPoint3Interpolator`, `target_color=3` (vermutlich Emissive) - der erste Keyframe-
Wert der referenzierten `NiPosData` (`(0, 0.447, 1)`) stimmt exakt mit dem "aktuellen Wert" der
Interpolator überein, eine Farbanimation über 3 QUADRATIC-Keys `(0,0.45,1)→(0.52,1,0)→
(1,0.87,0)` über 5 Sekunden - plausibel für eine pulsierende Effekt-Farbe.

**Massentest-Ergebnis unverändert (1047 Dateien)**, obwohl `NiMaterialColorController` und
`NiPoint3Interpolator` komplett aus der Fehlerliste verschwinden (alle Vorkommen erfolgreich
geparst) - jede betroffene Datei hat mindestens einen weiteren, noch nicht unterstützten
Blocker (meist `NiTextureTransformController`, da Effekt-Dateien mit Farb-Controllern häufig
auch Textur-Animationen haben). Trotzdem sinnvoll: sobald einer der verbleibenden Blocker
(insbesondere `NiTextureTransformController`) gelöst wird, tragen diese bereits korrekt
implementierten Typen automatisch zu weiteren erfolgreichen Dateien bei.

**Neuer, noch ungelöster Sonderfall gefunden:** bei 28 Dateien scheitert `NiPosData` mit
`key_type=0` - ein laut offiziellem `KeyType`-Enum (1=LINEAR, 2=QUADRATIC, 3=TBC, 4=XYZ,
5=CONST) ungültiger Wert. Weder LINEAR- noch QUADRATIC- noch TBC-Interpretation der
nachfolgenden Bytes ergab ein plausibles Muster oder eine saubere Landung auf dem erwarteten
Folgeblock (an `EnvSet.nif` mit mehreren Kandidatenlängen per Brute-Force geprüft, siehe
Beispiel unten). `SkipKeyGroup` schlägt für `key_type=0` bereits korrekt sauber fehl
(`Invalidate()`, kein Rateversuch) - betrifft nur `NiPosData`-Vorkommen mit diesem speziellen
Wert, keine Auswirkung auf die übrigen, korrekt funktionierenden `KeyGroup`-Nutzungen
(`NiFloatData`, `NiTransformData`). Nicht weiterverfolgt - würde eine weitere Vergleichsdatei
mit demselben Phänomen brauchen, um die Struktur einzugrenzen.

## 5. UV-Koordinaten aus `.nif`-Meshes sind in der Praxis unbrauchbar (wichtiger Fund, ungelöst)

Bisherige Verifikationen der UV-Extraktion (siehe Abschnitt 4) prüften nur, dass die
BYTE-LÄNGE des UV-Abschnitts korrekt berechnet wird (d.h. dass alle nachfolgenden Felder -
Dreieckszahl, Streifenlängen, der bekannte Trailer - byte-exakt bis zum Dateiende treffen).
Diese Session wurde erstmals zusätzlich geprüft, ob die extrahierten UV-WERTE selbst plausible
Texturkoordinaten sind (typischerweise im Bereich -10..10) - mit einem End-to-End-Test gegen
echte Daten: `.nif` laden → Textur-Dateiname auflösen → echte `.dds`-Datei im bereitgestellten
Ressourcen-Set suchen → mit `DdsImage` dekodieren → UV-Werte auf Endlichkeit und Plausibilität
prüfen (891 echte Textur-Dateien aus den resmap-Archiven indiziert, 91 Treffer für erfolgreich
geladene Mesh-Teile gefunden, alle 91 fehlerfrei dekodiert - die DDS-Dekodierung selbst ist also
solide).

**Ergebnis: praktisch JEDES getestete Mesh mit UV-Koordinaten liefert unbrauchbare Werte** -
extrem große (bis 1e38) oder winzige (1e-41, Denormals) Floats, unabhängig von Textur,
Geometrie, `numUvSets` oder Dateigröße. Selbst `santuary.nif` - das am gründlichsten verifizierte
Referenzobjekt dieses Projekts (Vertex-/Normalen-/Dreiecksdaten seit v0.13 byte-exakt bis zum
Dateiende bestätigt) - liefert für alle 86 UV-Paare Werte wie `(-1.1e-08, 4.6e+18)`.

**Was zweifelsfrei AUSGESCHLOSSEN werden konnte** (jeweils an `santuary.nif` geprüft):
- **Die Byte-Position ist korrekt**: mit der angenommenen Länge (`numVerts × 8 Byte × numUvSets`,
  nur Set 0 behalten) trifft `consistency_flags` exakt auf `-1`, `num_triangles=284` stimmt exakt
  mit der Streifenlänge überein (286 Punkte → 284 Dreiecke), und die letzten 8 Byte der Datei
  sind exakt der bekannte Trailer (`01 00 00 00 00 00 00 00`) - alles bis zum letzten Byte der
  Datei (6943 Byte) korrekt.
- **Die Positionsverfolgung VOR den UVs ist korrekt**: alle 86 Normalen sind exakte
  Einheitsvektoren (Betrag 1.0 ± 0.05), die berechnete Bounding-Sphere `(5.18, ~0, 25.78, r=26.5)`
  ist eine plausible Größe für ein kleines Objekt ("buoy" = Boje).
- **Kein einfacher Formatfehler**: getestet und verworfen wurden - Halbe-Präzision-Floats
  (16-Bit, ergibt unplausible/nicht-variierende Werte), Vector3 statt Vector2 (kein
  Einheitsvektor-Muster wie bei Tangenten/Binormalen zu erwarten wäre), Struct-of-Arrays statt
  Array-of-Structs (alle U-Werte dann alle V-Werte), vertex-majore statt set-majore Anordnung
  bei mehreren UV-Sets (betrifft nur Dateien mit `numUvSets > 1`, aber `santuary.nif` hat bereits
  bei `numUvSets = 1` das Problem).
- **Kein reines `numUvSets > 1`-Problem**: tritt genauso bei `numUvSets = 1` auf.

**Beobachtetes Muster (ungeklärt):** benachbarte oder durch Vertex-Symmetrie verwandte Indizes
liefern oft sehr ähnliche/identische "UV"-Werte (z.B. Index 0 und 8, oder 82/83 und 84/85 bei
`santuary.nif`), was eher nach verschobenen/wiederverwendeten VERTEX- oder NORMALEN-artigen
Fließkommadaten aussieht als nach echten UV-Koordinaten - aber eine Fehlausrichtung wurde ja
gerade durch die Bounding-Sphere- und Dateiende-Prüfung ausgeschlossen. Denkbare Erklärungen,
keine davon verifiziert:
1. Die Original-Assets haben tatsächlich nie befüllte/ungenutzte UV-Kanäle (denkbar, wenn dieses
   Engine-/Tooling-Setup Texturen prozedural oder über einen anderen Mechanismus zuordnet, ähnlich
   wie beim Terrain, wo die Diffuse-UV aus der Weltposition berechnet wird statt aus Datei-Daten,
   siehe Abschnitt 4 "UVScaleDiffuse").
2. Es gibt eine noch nicht gefundene, korrekte Dekodierung (z.B. ein anderes Zahlenformat als
   IEEE-754-Float, oder ein zusätzliches, noch unbekanntes Feld genau in diesem Byte-Bereich, das
   die Fließkomma-Interpretation verschiebt, ohne die GESAMTLÄNGE zu verändern).

**Vorläufige Absicherung (umgesetzt):** `SanitizeUvs()` prüft jedes extrahierte UV-Set auf
Endlichkeit und Plausibilität (`|u|,|v| ≤ 1000`) und verwirft es komplett bei einem einzigen
Verstoß - der Aufrufer (Renderer) fällt dann automatisch auf die bereits vorhandene
Materialfarben-Alternative zurück (siehe `NifMeshPart::uvs`-Dokumentation). Ergebnis nach dem
Fix: 0 von 91 real gefundenen und dekodierten Texturen zeigen noch unplausible UVs (weil sie
jetzt korrekt verworfen werden) - **die in v0.15/v0.16 gebaute Objekt-Texturierung wird dadurch
für praktisch alle echten Objekte deaktiviert (Fallback auf Materialfarbe)**, bis die
eigentliche Ursache gefunden ist. Das ist eine bewusste Qualitätsentscheidung: lieber korrekt
eingefärbte Objekte als sicher falsch texturierte.

**Nächster möglicher Schritt:** ein Objekt aus einem ANDEREN, unabhängigen Quellformat
desselben Spiels vergleichen (falls verfügbar - z.B. ein Community-Tool, das dieses
Dateiformat bereits erfolgreich mit korrekten Texturen lädt), um zu sehen, welche Dekodierung
dort verwendet wird. Ohne eine solche Referenz ist eine weitere Fehlersuche reines Raten.

## 6. UV-Rätsel aus Abschnitt 5 GELÖST: `NiGeometryData` hat ein falsch platziertes 2-Byte-Feld

**Hinweis des Nutzers, der zum Durchbruch führte:** "Die Produkte von NifTool können alle
Dateien öffnen, rendern und texturieren" - der Hinweis, dass eine unabhängige, öffentliche
Referenzimplementierung (NifTools-Projekt: NifSkope/PyFFI/nifxml, sowie ein quelloffener
Rust-NIF-Parser, der EXPLIZIT Version 20.0.0.4 als Ziel angibt) dieses Format bereits korrekt
handhabt. Das war genau die in Abschnitt 5 als fehlend benannte unabhängige Referenz.

**Herleitung:** die authoritative `NiGeometryData`-Struktur (aus dem Rust-Referenzparser)
zeigte zwei entscheidende Abweichungen von der bisherigen Annahme:
1. `num_uv_sets` ist ein **uint8**, gefolgt von einem separaten **uint8 `tspace_flag`**
   (Tangenten/Binormalen-Flag) - zusammen weiterhin 2 Byte wie bisher als u16 gelesen, ABER: laut
   Referenz zählen nur die UNTEREN 6 BIT als echte UV-Set-Anzahl (`num_uv_sets & 0x3F`). Bei
   allen bisher getesteten Dateien war `tspace_flag = 0`, wodurch diese Maskierung bisher
   zahlengleiche Ergebnisse lieferte - ändert also für sich genommen nichts.
2. **Der entscheidende Fund**: in der Referenzstruktur folgt auf `vertex_colors` DIREKT
   `uv_sets` - es gibt dort KEIN separates "Flags"-Feld. Das bisher als "uv_flags" bezeichnete,
   direkt VOR den UV-Daten gelesene u16-Feld existiert an dieser Position schlicht nicht.

**Byte-exakt verifiziert an `santuary.nif`:** Entfernen des u16-Reads vor den UV-Daten ergab
sofort plausible, normalisierte Texturkoordinaten (z.B. `(0.213, 0.015)`, `(0.004, 0.310)`),
brach aber die nachfolgende Byte-Ausrichtung (Streifenlängen explodierten auf tausende Einträge,
weit über das Dateiende hinaus) - die fehlenden 2 Byte mussten also woanders hin. Systematisches
Verschieben ergab die Lösung: **das mysteriöse 2-Byte-Feld gehört NICHT vor, sondern NACH die
UV-Daten**, direkt vor `consistency_flags`. Mit dieser Anordnung passt wieder ALLES exakt
zusammen: plausible UVs UND `consistency_flags = -1` UND `num_triangles = 284` (exakt
Streifenlänge 286 − 2) UND der bekannte 8-Byte-Trailer als letzte Bytes der Datei. Zusätzlich an
`BerFrz_Thorn.dds`-referenzierenden Dateien bestätigt: saubere, sich wiederholende
Grid-UVs (`0.0`/`0.5`-Muster, passend zu einer in 4 Quadranten aufgeteilten Textur).

Die Bedeutung des verschobenen 2-Byte-Feldes selbst bleibt unbekannt (evtl. ein zusätzliches
Padding- oder Alignment-Feld, das in der öffentlichen Referenzdokumentation nicht separat
benannt ist) - für unsere Zwecke reicht es, es an der richtigen Stelle zu überspringen.

**Auswirkung:** betrifft `NiTriStripsData` UND `NiTriShapeData` gleichermaßen (geteilte
`NiGeometryData`-Basis). Die in v0.19.0 eingebaute `SanitizeUvs()`-Sicherung bleibt als
Verteidigungsmaßnahme für eventuelle noch unbekannte Randfälle bestehen, greift aber nach
diesem Fix bei keiner der 91 real gefundenen und dekodierten Texturen mehr ein - **die
Objekt-Texturierung aus v0.15/v0.16 ist damit erstmals echt nutzbar.**

**Lehre:** die Verifikationsmethode selbst (Byte-Länge über das Dateiende hinweg bestätigen)
war unvollständig - sie kann zwei kompensierende Fehler (ein fehlendes Feld an der einen Stelle,
ein überzähliges an einer anderen) nicht von der korrekten Struktur unterscheiden, wenn beide
Fehler zufällig dieselbe Gesamtlänge ergeben. Erst der Inhalts-Plausibilitätstest (UV-Werte im
0..1-Bereich) deckte den echten Fehler auf. Für zukünftige Verifikationen: wo möglich, IMMER
zusätzlich zur Längenprüfung eine Inhalts-Plausibilitätsprüfung durchführen, nicht nur auf
"landet exakt auf dem Dateiende" vertrauen.

## 7. `NiTextureTransformController`-Rätsel GELÖST: `NiTexturingProperty` hatte ein konditionales, fehlendes Feld

**Mit demselben Referenzfund aus Abschnitt 6 gelöst.** Die authoritative Struktur (Rust-NIF-
Parser, Ziel-Version 20.0.0.4) zeigte: `NiTextureTransformController` ist tatsächlich **immer
fest 39 Byte** (dieselbe 30-Byte-`NiSingleInterpController`-Basis wie `NiAlphaController` +
`shader_map`(u8) + `texture_slot`(u32) + `operation`(u32)) - **keine Variation zwischen
Instanzen**, wie in der vorherigen Session fälschlich angenommen (siehe die dort dokumentierte,
jetzt überholte "39 vs. 43 Byte"-Beobachtung).

**Die wahre Ursache der scheinbaren Inkonsistenz:** `NiTexturingProperty` hat nach den 7
Textur-Slots ein zusätzliches, **konditionales** u32-Feld (vermutlich `num_shader_textures` +
ggf. eine `ShaderTexDesc`-Liste, deren Struktur nicht verifiziert ist, da in allen Testdateien
der Wert 0 war) - vorhanden NUR, wenn die Property selbst einen Controller referenziert
(`controller_ref != -1`). Byte-exakt an 4 echten Dateien verifiziert:
- `santuary.nif` und `BH_Albi_Ground.nif` (`controller_ref = -1`): Feld NICHT vorhanden - die
  Property landet direkt auf der 17-Byte-`NiSourceTexture`-Präambel.
- `AdlFH_field_burn_ground.nif` (`controller_ref = 12`) und `SD_Vale01_machine02.nif`
  (`controller_ref = 17`): Feld SEHR WOHL vorhanden (Wert jeweils 0) - erst danach treffen
  sämtliche nachfolgenden `NiTextureTransformController`-Ketten exakt auf ihre erwarteten
  Nachbarblöcke (`next_controller`-Verkettung, `target`- und `interpolator_ref`-Treffer).

Ohne diese Bedingung schlägt entweder der einfache Fall fehl (Feld immer lesen bricht
`santuary.nif`) oder der Controller-Fall bleibt weiterhin scheinbar inkonsistent (Feld nie
lesen lässt `NiTextureTransformController` wie in der Vorsession als "variabel lang"
erscheinen, obwohl es das nicht ist - der Versatz kam ausschließlich von diesem einen,
übersehenen Feld VOR den Controllern, nicht von den Controllern selbst).

**Ergebnis:** `NiTextureTransformController` verschwindet komplett aus der Fehlerliste des
Massentests (vorher 134 Dateien direkt blockiert). Der Gesamt-Massentest-Wert bleibt dennoch
unverändert (1047/3436), da die betroffenen Dateien fast durchweg mehrfach texturierte Wasser-/
Lava-/Effekt-Objekte sind, die jetzt an einer ANDEREN, noch offenen Stelle scheitern (siehe
unten) - ein Muster, das in dieser Session schon mehrfach auftrat (ein Block-Typ wird korrekt,
aber die Datei hat noch einen weiteren, bisher verdeckten Blocker).

### Neu sichtbar gewordener, noch offener Folgefehler
Nach diesem Fix scheitern viele der zuvor an `NiTextureTransformController` blockierten Dateien
jetzt an einer zweiten `NiSourceTexture` (bei Objekten mit mehreren Textur-Slots UND mehreren
Textur-Transform-Controllern, z.B. `adlF_town_water.nif`: 4 Controller + 4 Interpolator/Data-
Paare, dann eine zweite Textur). Byte-Versatz erneut exakt 4 Byte (dieselbe Größenordnung wie
der gerade gelöste Fund) - vermutlich ein ähnliches konditionales Feld an anderer Stelle
(z.B. pro zusätzlichem Textur-Slot mit `hasTransform`, oder erneut an `NiTexturingProperty`
selbst bei mehreren aktiven Controllern). Nicht weiter untersucht in dieser Session - nächster
Kandidat für dieselbe Ermittlungsmethode (Referenzabgleich + Byte-Versatz-Suche).

## 8. Folgefehler aus Abschnitt 7 gelöst: `NiSourceTexture` hat DASSELBE konditionale Muster

Nach dem `NiTextureTransformController`-Fix (Abschnitt 7) scheiterten mehrfach texturierte
Wasser-/Lava-/Effekt-Objekte (z.B. `adlF_town_water.nif`, `SD_Vale01_machine02.nif`) an einer
zweiten `NiSourceTexture` - erneut mit einem 4-Byte-Versatz. Mit derselben Methode (Referenz-
abgleich + gezielte Byte-Suche) gelöst: `NiSourceTexture`s `NiObjectNET`-Basis hat - genau wie
bei `NiTexturingProperty` - ein zusätzliches, konditionales u32-Feld (Bedeutung ungeklärt,
empirisch immer 0) zwischen der (in allen Testdateien leeren) Extra-Daten-Liste und dem
`controller`-Feld.

**Byte-exakt an 2 Dateien verifiziert:**
- `santuary.nif` (einfaches, einzeln texturiertes Objekt): Feld VORHANDEN - 12 Nullbytes vor
  `controller=-1`, danach `use_external`(1 Byte) - Gesamtlänge 17 Byte, landet exakt auf den
  Dateinamen `"buoy.dds"`.
- `SD_Vale01_machine02.nif` (Objekt mit 6 vorausgehenden `NiTextureTransformController`n):
  Feld FEHLT - nur 8 Nullbytes vor `controller=-1` - Gesamtlänge 13 Byte, landet exakt auf den
  Dateinamen der zweiten Textur.

Per Peek robust unterscheidbar (analog zur `NiTexturingProperty`-Lösung): liegt der Wert
`0xFFFFFFFF` (`controller=-1`) direkt nach den ersten 8 Nullbytes, fehlt das Mystery-Feld;
andernfalls ist es vorhanden und wird übersprungen. Bewusst NUR dieser eine Fund umgesetzt -
NICHT die vollständige, umfangreichere `NiSourceTexture`-Struktur aus derselben
Referenzimplementierung übernommen (die zeigt zusätzliche Felder wie `pixel_layout`,
`mipmap_format`, `alpha_format`, `is_static`, `direct_render` NACH der `pixel_data_ref` - das
widerspricht der eigenen, seit Monaten extensiv verifizierten Blockreihenfolge dieses Forks,
bei der `NiPixelData` OHNE Zwischenraum direkt auf `NiSourceTexture` folgt). Nur die eine,
gezielt verifizierte Korrektur übernommen, der Rest der Funktion unverändert gelassen - auch
eine gute Referenz kann für einzelne Felder eines Forks danebenliegen, siehe die Lehre in
Abschnitt 6/7: jede übernommene Korrektur einzeln byte-exakt verifizieren, nicht die ganze
Referenzstruktur ungeprüft übernehmen.

**Ergebnis:** Massentest 1047 → 1062 von 3436 (30.5% → 30.9%). Reiner Gewinn, keine Regression
(Fix ist rückwärtskompatibel: der 17-Byte-Fall bleibt für alle bisher erfolgreichen Dateien
unverändert). Texturierungs-Verifikation (siehe Abschnitt 5/6) bestätigt weiterhin 0
unplausible UVs bei jetzt 94 real gefundenen und dekodierten Texturen (vorher 91).

## 9. `NiLODNode`/`NiRangeLODData` implementiert (1062 → 1080 von 3436)

Gleiche Methode (Referenzabgleich + byte-exakte Verifikation): `NiLODNode : NiSwitchNode :
NiNode`. `NiSwitchNode` ergänzt `switch_flags`(u16) + `index`(u32, aktuell aktiver Kind-Index),
`NiLODNode` ergänzt `lod_level_data_ref`(i32). Rendering-seitig wie ein normaler `NiNode`
behandelt (alle Kinder werden platziert, keine echte entfernungsbasierte LOD-Umschaltung im
Editor - für ein Offline-Bildungsprojekt ohne Kamera-Distanz-Logik nicht relevant).

Byte-exakt verifiziert an `tree05.nif`: Name=`"LODGroup01"`, 3 Kinder (je ein LOD-Level-Netz),
`lod_level_data_ref` zeigt exakt auf die zugehörige `NiRangeLODData`.

**`NiRangeLODData` weicht von der öffentlichen Referenzstruktur ab** (dort: `center`-Vector3 +
`num_lod_levels` + Level-Paare). In diesem Fork stattdessen: ein einzelnes führendes u32-Feld
(empirisch immer 0 - evtl. eine vereinfachte/weggelassene "Center"-Angabe) + `num_lod_levels`
(u32) + je Level `near`(f32)+`far`(f32) + ein abschließender 8-Byte-Trailer (Bedeutung
ungeklärt, empirisch immer `01 00 00 00 00 00 00 00`). Byte-exakt an 2 unabhängigen Dateien
verifiziert (`Adl_field_tree01.nif`, `Adl_field_tree02.nif` - beide mit identischer 3-Stufen-
LOD-Konfiguration 0-1000/1000-2000/2000-100000000 Einheiten, vermutlich ein wiederverwendetes
Baum-Preset): die berechnete Länge (40 Byte) trifft in BEIDEN Fällen exakt auf das jeweilige
jeweils letzte Byte der Datei (`NiRangeLODData` war in beiden Testdateien der letzte Block).

**Ergebnis:** Massentest 1062 → 1080/3436 (30.9% → 31.4%). `NiRangeLODData` verschwindet
komplett aus der Fehlerliste. `NiLODNode` selbst bleibt bei ca. 9 Dateien noch blockiert
(vermutlich weitere, unabhängige Strukturvarianten innerhalb dieser Dateien - nicht
untersucht). 7/7 Test-Suiten weiterhin grün, keine Regression.

## 10. Partikelsystem-Familie implementiert (`NiParticleSystem` und Umfeld) - drei eigenständige Funde

Größter verbleibender Einzel-Blocker (61 Dateien) angegangen. Vollständige Struktur-Kette
(`NiParticleSystem`, `NiPSysData`/`NiParticlesData`, `NiPSysEmitterCtlr`/`NiPSysUpdateCtlr`,
`NiBoolInterpolator`/`NiBoolData`, `NiColorData`, sowie die gängigen Modifier: AgeDeath,
BoxEmitter, Spawn, GrowFade, Color, Rotation, Gravity, Position, BoundUpdate) aus derselben
Referenzimplementierung (Rust-"nif"-Crate) übernommen und Feld für Feld an echten Dateien
verifiziert. Dabei drei eigenständige, echte Bugs gefunden:

**Fund 1 - `NiParticleSystem`s `has_shader`-Byte ist ECHT konditional.** Bei `NiTriStrips`/
`NiTriShape` folgt auf dieses Byte in praktisch jeder Testdatei ein gültiges (ggf. leeres)
Freitextfeld, weshalb der bisherige, unbedingte `SizedString`-Read dort nie auffiel - inhaltlich
aber genaugenommen falsch (siehe `MaterialDataShader`-Struktur der Referenz: Freitext nur bei
`has_shader=1`). Bei `NiParticleSystem` ist `has_shader` dagegen meist 0, ein unbedingtes Lesen
zerstört die Ausrichtung komplett. KORRIGIERT für den Partikel-Pfad (eigene, korrekte
Kopf-Funktion, NiTriStrips/NiTriShape bewusst unangetastet gelassen). Byte-exakt verifiziert an
`Leviathan_altar_water_effect01.nif`: 9 Modifikator-Referenzen (allesamt plausible Blockindizes),
landet exakt auf die folgende `NiPSysEmitterCtlr` (deren `target`-Feld exakt hierher zurückzeigt).

**Fund 2 - `KeyType`-Wert 5 (CONST) fehlte in `SkipKeyGroup`/`SkipKeyGroupBytes`.** Bislang nur
1/2/3 (LINEAR/QUADRATIC/TBC) behandelt, Wert 5 löste `Invalidate()` aus. CONST verwendet
dieselbe Schlüsselgröße wie LINEAR (Zeit+Wert, keine Tangenten). Byte-exakt verifiziert an
`BH_Karen_water_effect.nif`s `NiBoolData` (Sichtbarkeits-Keyframes 1→0 über 4 Sekunden, ein
Emitter-Fadeout) - landet exakt auf die folgende `NiTexturingProperty`.

**Fund 3 - `currentMeshHasTexturing` wurde nie für `NiParticleSystem` aktualisiert.** Diese
Zustandsvariable (steuert, ob `NiMaterialProperty` ein optionales 15. Float-Feld liest, siehe
Abschnitt zu `NiMaterialProperty`) wurde bisher AUSSCHLIESSLICH beim Betreten von
`NiTriStrips`/`NiTriShape` aus deren Properties-Liste neu gesetzt - bei einem
`NiParticleSystem` blieb der Wert vom zuletzt gesehenen, komplett unabhängigen Mesh stehen. Die
zum Partikelsystem gehörende `NiMaterialProperty` bekam dadurch potenziell den falschen Wert
übergeben. KORRIGIERT: `NiParticleSystem`/`NiMeshParticleSystem` aktualisieren
`currentMeshHasTexturing` jetzt genauso aus ihrer eigenen Properties-Liste wie `NiTriStrips`.
Ein genereller, unabhängig vom Partikel-Thema gültiger Bugfix.

**Offen:** Bei mindestens einer Datei (`BH_Karen_water_effect.nif`) bleibt nach allen drei
Fixen noch ein weiterer, nicht isolierter Versatz vor `NiPSysData` bestehen (Fund 3 verschob die
Position um die erwarteten 4 Byte in die richtige Richtung, traf aber nicht exakt) - vermutlich
ein VIERTER, noch unentdeckter Sonderfall, evtl. bei `NiVertexColorProperty`s Byte-Länge im
Partikel-Kontext. Nicht weiter verfolgt, um das Budget dieser Session nicht zu sprengen - guter
Kandidat für eine Folgesession mit frischem Blick und mehr Vergleichsdateien.

Zusätzlich als Nebenfund: `NiPixelData`s Abschluss-Trailer (siehe frühere Abschnitte) ist NICHT
in jedem Fall exakt 8 Byte - bei mindestens einer echten Datei mit unkomprimiertem 8-Bit-
Pixelformat (`BH_Karen_water_effect.nif`) reichen 4 Byte, obwohl der Pixelformat-Header
byte-identisch zu einer Datei ist, die die vollen 8 Byte braucht
(`BH_Karen_fire.nif`) - die eigentliche Unterscheidung bleibt ungeklärt. Per Peek robust
gelöst: 8 Byte bleibt der Standardfall, 4 Byte wird nur verwendet, wenn an dieser Position ein
plausibler leerer Objektname-Header (`0,0,-1`) erkennbar ist, den es bei 8 Byte nicht gibt -
byte-exakt verifiziert (trifft danach exakt auf `NiAlphaProperty` UND die folgende
`NiVertexColorProperty`), rückwärtskompatibel zum bisherigen 8-Byte-Standardfall.

**Ergebnis:** Massentest 1080 → 1083/3436 (31.4% → 31.5%). Kleiner werdender, aber weiterhin
positiver Nettogewinn - die drei Funde selbst sind unabhängig von diesem einen Dateibudget
wertvoll (Fund 3 insbesondere ist ein genereller Bugfix, der jedem zukünftigen texturierten
Partikelsystem UND jedem Mesh direkt nach einem texturierten Partikelsystem im selben Objekt
zugutekommt). 7/7 Test-Suiten weiterhin grün, keine Regression, Texturierungs-Pipeline
weiterhin bei 0 unplausiblen UVs (94 real gefundene Texturen).

## 11. GROSSER FUND: Der konditionale 8-Byte-Trailer bei NiTriStripsData/NiTriShapeData war zu eng gefasst

Ausgangspunkt war ein kleiner Fund beim Ergänzen von `NiTextureEffect`/`NiDirectionalLight`:
`ItemShop02.nif` scheiterte bei `NiDirectionalLight`, obwohl der Block selbst korrekt
implementiert war. Ursache: die VORAUSGEHENDE `NiTriStripsData` hatte einen 8-Byte-Trailer
gelesen, den es an dieser Stelle nicht geben sollte. Die bisherige Regel ("Trailer fehlt NUR,
wenn direkt ein weiteres NiTriStrips/NiTriShape folgt", siehe Abschnitt zu `Rou_M_Tube.nif`)
war also **unvollständig** - der Trailer fehlt auch, wenn eine *andere* Art von Geschwister-
Knoten folgt (hier: `NiDirectionalLight`).

**Byte-exakt verifiziert an `ItemShop02.nif`:** Mit Trailer landet die Position mitten in den
Namenstext `"__MAX_Default_Light"` hinein (erkennbar an eindeutig druckbaren ASCII-Bytes an
falscher Stelle); ohne Trailer trifft die Position exakt auf die Längenangabe (19) direkt vor
diesem Namen.

**Lösung:** Eine neue Allzweck-Heuristik `LooksLikeFreshName()` prüft, ob eine gegebene
Position plausibel der Anfang eines `SizedString`-Namensfelds sein könnte (Länge 0 = leerer
Name, ODER Länge 1-64 mit ausschließlich druckbarem ASCII-Text danach - praktisch jeder
Blocktyp beginnt mit einem solchen Namensfeld aus `NiObjectNET`/`AVObjectBase`). Die bisherige
"next==TriStrips/TriShape"-Regel bleibt die Vorgabe (sie war für die überwiegende Mehrheit
korrekt), wird aber verworfen, wenn sie an der berechneten Position KEINEN plausiblen
Namensanfang ergibt, während die jeweils andere Variante (mit/ohne Trailer) SEHR WOHL einen
ergibt - ein robuster, rückwärtskompatibler Kompatibilitäts-Check nach demselben Muster wie
der `NiPixelData`-Trailer-Fix in Abschnitt 10.

**Ergebnis: Massentest 1083 → 1465/3436 (31.5% → 42.6%) - mit Abstand der größte Einzelfund
dieser Session.** Die betroffene Konstellation (Mesh gefolgt von einem NICHT-Geometrie-
Geschwisterknoten wie Licht, Effekt, oder einem weiteren NiNode-Ast) ist in echten Dateien
offenbar SEHR häufig - viel häufiger als die ursprünglich angenommene "zwei TriStrips
hintereinander"-Konstellation. 7/7 Test-Suiten weiterhin grün, Texturierungs-Pipeline
weiterhin bei 0 unplausiblen UVs (jetzt 122 real gefundene und dekodierte Texturen, vorher 94)
- der Fund betraf ausschließlich die Trailer-Länge, nicht die UV-Extraktion selbst.

**Lehre:** Ein an wenigen Beispielen verifizierter, aber NICHT über eine unabhängige Referenz
abgesicherter Fix (wie die ursprüngliche "next==TriStrips"-Regel aus einer früheren Session)
kann trotz korrekter Herleitung an den geprüften Beispielen weit von der vollständigen Wahrheit
entfernt sein. Die generische `LooksLikeFreshName()`-Heuristik ist jetzt der bevorzugte Weg für
ähnliche konditionale Trailer/Felder, wo eine erschöpfende Referenz fehlt - sie braucht keine
Kenntnis des KONKRETEN Folgeblocktyps, nur die Beobachtung, dass praktisch jeder Blocktyp mit
einem Namensfeld beginnt.

## 12. ZWEITER GROSSER FUND: NiSourceTexture braucht 18 weitere Byte bei externen Texturen (use_external=1)

Direkte Folge des Trailer-Funds in Abschnitt 11: mit dem gefixten Trailer wurden plötzlich
sehr viele NEUE `NiTriStripsData`-Dateien erreichbar, von denen ein großer Teil sofort mit
astronomisch großen/winzigen Vertex-Koordinaten scheiterte (RockD8.nif als erstes Beispiel).

**Ursache gefunden:** `NiSourceTexture`s `use_external`-Feld ist nicht nur ein informatives
Flag, sondern bestimmt eine STRUKTURELLE Verzweigung. Bei `use_external=0` (Textur über ein
eingebettetes `NiPixelData` referenziert - der bisher einzige verifizierte Fall) endet
`NiSourceTexture` direkt nach der `pixel_data`-Referenz. Bei `use_external=1` (rein externe
Textur-Datei, KEIN `NiPixelData` in der Datei vorhanden) folgen laut Referenzimplementierung
noch 18 weitere Byte: `pixel_layout`(u32) + `mipmap_format`(u32) + `alpha_format`(u32) +
`is_static`(u8) + `direct_render`(u8) + ein abschließendes u32-Feld (Bedeutung ungeklärt,
empirisch immer 0) - vermutlich deshalb nötig, weil ohne `NiPixelData` sonst nirgends stünde,
in welchem Pixelformat die externe Datei vorliegt.

**Byte-exakt verifiziert an `RockD8.nif`** (`use_external=1`, Datei `"rock_details.dds"`,
kein `NiPixelData` im gesamten Block-Typen-Inventar der Datei): `pixel_layout=6`,
`mipmap_format=1`, `alpha_format=3`, `is_static=1`, `direct_render=0` - allesamt plausible,
kleine Enum-Werte - danach trifft die Position exakt auf sinnvolle, kleine Vertex-Koordinaten
der folgenden `NiTriStripsData` (vorher: Werte im Bereich 10²³ bis 10⁻⁴², eindeutig
Fehlausrichtung).

**Ergebnis: Massentest 1500 → 1664/3436 (43.7% → 48.4%).** Zusammen mit dem Trailer-Fund aus
Abschnitt 11 hat dieser Nachmittag den Massentest von 31.5% auf 48.4% mehr als verdoppelt.
Real gefundene und dekodierte Texturen sprangen von 122 auf 303 (viele der neu ladbaren
Dateien nutzen naturgemäß externe statt eingebetteter Texturen). 7/7 Test-Suiten weiterhin
grün, 0 unplausible UVs auch bei der neuen, deutlich größeren Stichprobe.

**Vorsicht/Negativ-Ergebnis (festgehalten, um es nicht erneut zu versuchen):** Der `has_shader`-
Bugfix, der für `NiParticleSystem` (Abschnitt 10) korrekt war, wurde versuchsweise auch auf
`ParseNiTriStripsHeader` (gemeinsame Kopf-Funktion für `NiTriStrips`/`NiTriShape`) angewendet,
da eine einzelne Datei (`CynDN_Tree00.nif`) dort eine 4-Byte-Fehlausrichtung zeigte, die
oberflächlich genauso aussah. Das Ergebnis war ein KATASTROPHALER Rückschritt im Massentest
(1664 → 4) - die Interpretation des Bytes als `has_shader`-Flag ist für `NiTriStrips`/
`NiTriShape` in diesem Fork offenbar FALSCH (das Byte+Freitextfeld ist dort vermutlich
unbedingt vorhanden, nicht konditional wie bei `NiParticleSystem`). Die Änderung wurde sofort
zurückgenommen. Die eigentliche Ursache der 4-Byte-Verschiebung bei `CynDN_Tree00.nif` bleibt
ungeklärt und sollte NICHT über eine Änderung an `ParseNiTriStripsHeader` gelöst werden -
diese Funktion ist die Grundlage für die überwiegende Mehrheit aller erfolgreich geladenen
Dateien und jede Änderung daran muss extrem vorsichtig und mit vollständigem Regressionstest
erfolgen. Lehre: dieselbe Byte-Semantik (hier: ein Flag-artiges Byte vor einem Freitextfeld)
kann sich zwischen strukturell ähnlichen, aber unterschiedlichen Blocktypen unterscheiden -
eine an Typ A verifizierte Korrektur ist keine Garantie für Typ B, selbst wenn beide dieselbe
Basisklasse (`NiGeometry`) in der allgemeinen Referenz teilen.

## 13. NiSkinInstance/NiSkinData/NiSkinPartition implementiert - plus eine wichtige Korrektur am Trailer-Peek selbst

Geskinnte Meshes (`NiSkinInstance` + referenzierte `NiSkinData`/`NiSkinPartition`) aus
derselben Referenz implementiert - Editor stellt Meshes weiterhin nur in Bindungspose dar
(keine Animation/Deformation), die Blöcke werden also nur korrekt übersprungen, nicht für
Rendering ausgewertet.

**Dabei ein wichtiger Fund am bestehenden Trailer-Peek (Abschnitt 11) selbst:** eine
`NiTriShapeData`/`NiTriStripsData`, der eine `NiSkinInstance` folgt, braucht ebenfalls KEINEN
8-Byte-Trailer (wie bei einem zweiten Mesh-Teil) - aber `NiSkinInstance` beginnt NICHT mit
einem Namensfeld (nur rohe Referenzen: `data_ref`+`skin_partition`+`skeleton_root`+
`num_bones`), weshalb `LooksLikeFreshName()` diesen Fall nicht selbst erkennen kann. Die
einfache Lösung: `NiSkinInstance` explizit zur bestehenden "kein Trailer"-Ausnahmeliste
hinzugefügt (analog zu `NiTriStrips`/`NiTriShape`).

**Zweiter, subtilerer Fund dabei:** der bestehende Peek-Override im Abschnitt-11-Fix wendete
sich fälschlich auch im "hasTrailer=false"-Zweig an (als zusätzliche Absicherung gedacht) -
das führte dazu, dass der neue, EXPLIZITE `NiSkinInstance`-Ausschluss vom Aufrufer durch den
Peek wieder RÜCKGÄNGIG gemacht wurde (da `NiSkinInstance`s rohe Ref-Werte nicht wie ein
plausibler Name aussehen, "gewann" fälschlich die Trailer-Variante). KORRIGIERT: der Peek-
Override greift jetzt NUR NOCH im Standardfall (`hasTrailer=true`, wenn der Aufrufer den
Trailer für nötig hält) - ein expliziter Ausschluss durch den Aufrufer (anhand des konkreten
Folgeblocktyps) wird nicht mehr in Frage gestellt.

Byte-exakt verifiziert an `Tunnel02_Wood3.nif`: `NiSkinInstance.data_ref=12` zeigt danach exakt
auf die folgende `NiSkinData`, `skin_partition=13` exakt auf die übernächste
`NiSkinPartition`, plausible Knochenzahl 14 (vorher, mit dem Fehler: absurd 126).

**Ergebnis: Massentest 1664 → 1676/3436 (48.4% → 48.8%).** 7/7 Test-Suiten weiterhin grün,
weiterhin 0 unplausible UVs. Kleinerer, aber sauberer Fund - die Vereinfachung des Peek-
Overrides (ein "else"-Zweig entfernt) macht die Trailer-Logik zusätzlich robuster für
zukünftige, noch unbekannte Ausnahmefälle.

**Wichtige Lehre (Fortsetzung von Abschnitt 12):** `ParseNiTriStripsHeader` bleibt weiterhin
bewusst unangetastet (siehe die dortige Warnung) - dieser Fund betraf eine ANDERE, bereits
vorsichtig mit Peek-Fallback abgesicherte Stelle (den Trailer NACH den Geometriedaten, nicht
das Freitextfeld VOR den Geometriedaten) und war entsprechend risikoärmer zu korrigieren.

## 14. Zwei weitere Untersuchungen ohne Erfolg (bewusst dokumentiert, um sie nicht zu wiederholen)

Nach den erfolgreichen Funden in Abschnitt 11-13 wurden zwei weitere Verdachtsstellen
untersucht, die sich beide NICHT als robust fixbar herausstellten - hier festgehalten, damit
zukünftige Sessions sie nicht erneut versuchen.

### `ParseNiTriStripsHeader`s Freitextfeld bei `CynDN_Tree00.nif`
Eine Datei (`CynDN_Tree00.nif`) zeigte eine 4-Byte-Fehlausrichtung, die genau wie das bei
`NiParticleSystem` gefundene `has_shader`-Muster aussah (Byte=0, aber trotzdem KEIN
Freitextfeld vorhanden). Eine systematische Auszählung über den gesamten Korpus (14560
NiTriStrips/NiTriShape-Instanzen) zeigte jedoch: das Byte ist in 14488 von 14560 Fällen (99.5%)
gleich 0 - UND davon haben 5008 Instanzen ein ECHTES, nicht-leeres Freitextfeld, das gelesen
werden MUSS. Das Byte korreliert also NICHT zuverlässig mit der An-/Abwesenheit des
Freitextfelds - `CynDN_Tree00.nif` ist vermutlich ein echter Einzelfall (evtl. eine andere
Export-Version oder ein Sonderfall des Autorenwerkzeugs), keine systematische Regel. Ein
Peek-basierter Versuch (analog zum Trailer-Fix) wurde getestet und verursachte einen
Rückschritt von 1664 auf 1280 - sofort zurückgenommen. `ParseNiTriStripsHeader` bleibt
unverändert (siehe bereits bestehende Warnung in Abschnitt 12/13).

### `NiMaterialProperty`s 14-vs-15-Float-Frage bei `S_Tower02_ScanLine.nif`
Eine Datei ohne jegliche `NiTexturingProperty` im gesamten Block-Inventar brauchte trotzdem die
"texturierte" 14-Float-Variante (statt der laut bisheriger Regel erwarteten 15). Ein
Peek-basierter Versuch (`LooksLikeFreshName` nach beiden Kandidatenlängen, exakt nach demselben
Muster wie der erfolgreiche Trailer-Fix) verursachte einen deutlichen Rückschritt (1676 → 1508)
und wurde sofort zurückgenommen. Vermutliche Ursache: Materialfarben-Floats erzeugen offenbar
recht häufig zufällig täuschend "plausible" Namens-Byte-Muster (im Gegensatz zu den echten
Namensfeldern bei den Trailer-Fällen, wo der Peek zuverlässig funktionierte) - der Peek ist für
diese Art von Ambiguität also NICHT geeignet. Die einfache `meshHasTexturing`-Regel bleibt
Standard; der Einzelfall bei `S_Tower02_ScanLine.nif` bleibt ungelöst.

**Übergreifende Lehre:** Die `LooksLikeFreshName()`-Heuristik funktioniert zuverlässig, wenn
die Position DANACH tatsächlich einem generischen `NiObjectNET`/`AVObjectBase`-Namensfeld
entspricht (das ist bei den meisten Blockübergängen der Fall) - sie ist aber KEIN
Allzweckwerkzeug für jede Art von Struktur-Ambiguität. Bei Feldern, die selbst aus beliebigen
Fließkommazahlen bestehen (wie Materialfarben) oder wo die Fallunterscheidung nicht an einem
Blockübergang, sondern mitten in einer anderen Struktur liegt, ist der Peek unzuverlässig und
sollte nicht ohne sehr sorgfältige, breite Verifikation eingesetzt werden.

## 15. Ältere NIF-Versionen (10.1.0.0/10.2.0.0): Header gelöst, Blockinhalte bleiben ein eigenes Projekt

177 Dateien (170x Version 10.2.0.0, 7x Version 10.1.0.0 - zusammen exakt die Anzahl der
"Unerwartetes Dateiende im NIF-Header"-Fehler) scheiterten bisher schon am Datei-Header selbst,
noch bevor irgendein Block gelesen wurde.

**Gelöst:** Der Header-Aufbau unterscheidet sich zwischen den Versionen. Bei 20.0.0.4 gibt es
zwischen `version` und `user_version` ein zusätzliches Endian-Byte; bei 10.1.0.0/10.2.0.0 FEHLT
dieses Byte. Byte-exakt ermittelt durch systematisches Ausprobieren mehrerer Kandidaten-Offsets
an `horse2.nif` (10.2.0.0): mit Endian-Byte ergaben sich Garbage-Werte für `num_block_types`,
ohne Endian-Byte (direkt `user_version(u32)+num_blocks(u32)+num_block_types(u16)`) landet man
exakt auf einer gültigen, mit "NiNode" beginnenden Blocktyp-Tabelle. Verifiziert an 3 echten
Dateien (2x Version 10.2.0.0, 1x Version 10.1.0.0) - `num_blocks` traf in allen Fällen exakt
mit dem separat aus dem Rohbyte-Muster abgelesenen Wert überein (z.B. 170 bei `horse2.nif`).

**NICHT gelöst (eigenes, größeres Projekt für eine Folgesession):** Nach dem Header-Fix
kommen die Dateien zwar viel weiter (i.d.R. bis zu `NiSourceTexture`/`NiPixelData`), scheitern
dort aber weiterhin. Genauere Untersuchung zeigt: bereits `NiSourceTexture` selbst ist für
diese älteren Dateien anders aufgebaut als für 20.0.0.4 (unsere modernen, sorgfältig
verifizierten Korrekturen für das "Mystery-Feld" und den `use_external`-Sonderfall, siehe
Abschnitt 8/12, greifen hier NICHT - der berechnete Dateiname ist leer, die berechnete
`pixel_data`-Referenz ist offensichtlich Unsinn). Das deutet darauf hin, dass mehrere
Blocktypen zwischen den NIF-Versionen strukturell abweichen, nicht nur `NiPixelData` - eine
vollständige Unterstützung dieser älteren Version wäre ein eigenständiges
Reverse-Engineering-Projekt (eigene Referenzrecherche für die 10.x-Ära, vermutlich mehrere
Blocktypen betroffen), keine punktuelle Korrektur wie die bisherigen Funde dieser Session.

**Ergebnis:** Massentest bleibt bei 1676/3436 (der Header-Fix allein schaltet noch keine
Dateien frei, da die Blockinhalte selbst weitere, ungelöste Differenzen haben) - aber die
Grundlage ist jetzt vorhanden: eine zukünftige Session, die die 10.x-Ära-Blockformate
untersucht, findet mit diesem Fix bereits einen funktionierenden Einstiegspunkt (Header +
Blocktyp-Tabelle) vor, statt bei Null anfangen zu müssen. 7/7 Test-Suiten weiterhin grün, keine
Regression - der Fix ist rückwärtskompatibel (betrifft nur Dateien mit Version ungleich
20.0.0.4, die vorher ohnehin komplett fehlschlugen).

## 16. GROSSER FUND: `NiSourceTexture`s `num_extra_data_refs`-Feld fehlt bei direkt
    aufeinanderfolgenden `NiSourceTexture`-Blöcken (+39 Dateien)

49 Dateien im Testkorpus haben im Blockindex zwei `NiSourceTexture`-Blöcke DIREKT
hintereinander (z.B. eine Haupttextur gefolgt von einer "Dark Map" in derselben
`NiTexturingProperty`). Bei der ZWEITEN dieser beiden Instanzen ist das
`num_extra_data_refs`-Feld (das bei jeder anderen NiObjectNET-Instanz im gesamten
Korpus immer 0 war) komplett ABWESEND - nicht 0, sondern schlicht nicht vorhanden.

**Gefunden an `fence_dis.nif`:** Block 8 (die zweite `NiSourceTexture`) beginnt mit
`name_len=0`, gefolgt DIREKT von `0xFFFFFFFF` (dem controller-Feld, `-1`) - kein
zusätzliches Nullfeld dazwischen. Byte-exakt verifiziert durch Rückwärtsrekonstruktion
von einer eindeutig lesbaren Zeichenkette aus: der Dateiname "fence_dark.dds" (14 Zeichen)
plus vorausgehender Längenangabe plus `use_external`-Byte ergaben rückwärts gerechnet exakt
diese Feldreihenfolge - byte-exakter Beweis, keine Vermutung.

**Fix (risikoarm, da `num_extra_data_refs` in JEDER bisher erfolgreich geparsten Datei exakt
0 war):** Nach dem `name_len=0`-Feld wird der nächste Wert gepeekt. Ist er 0, wird er wie
bisher als `num_extra_data_refs=0` konsumiert (Normalfall, ändert nichts an allen bereits
funktionierenden Dateien). Ist er NICHT 0, wird er NICHT konsumiert - das bestehende
Mystery-Feld/controller-Peek direkt danach übernimmt dann korrekt, da `0xFFFFFFFF` (der
echte controller-Wert) ohnehin schon die "kein Mystery-Feld"-Bedingung erfüllt.

**Ergebnis:** Massentest 1676 → **1715/3436** (+39 Dateien, +1.1 Prozentpunkte). Von den 49
betroffenen Dateien laden jetzt 39 vollständig; die restlichen 10 scheitern an anderen,
unabhängigen, noch nicht gefundenen Stellen weiter hinten in der jeweiligen Datei (z.B.
`fence_dis.nif` selbst scheitert jetzt bei einer `NiTriStripsData`-Instanz, einem der
bekannten offenen EOF-Sonderfälle). 7/7 Test-Suiten weiterhin grün, keine Regression.

## 17. GROSSER FUND #2: `num_extra_data_refs` fehlt generell in `ParseObjectNetBase`
    (nicht nur bei `NiSourceTexture`) - +26 weitere Dateien

Direkt im Anschluss an Abschnitt 16 zeigte sich: dasselbe Phänomen (das
`num_extra_data_refs`-Feld der gemeinsamen NiObjectNET-Basis fehlt manchmal komplett) tritt
NICHT nur bei `NiSourceTexture` auf, sondern generell in der gemeinsamen `ParseObjectNetBase`-
Funktion, die von praktisch allen Blocktypen verwendet wird.

**Gefunden an `FighterDown.nif`:** Block 6 (`NiVertexColorProperty`, folgt auf eine
`NiTriStrips`) beginnt mit `name_len=0`, gefolgt DIREKT von `0xFFFFFFFF` (dem
controller-Feld, `-1`) - ohne `num_extra_data_refs` dazwischen. Byte-exakt verifiziert:
mit dieser Annahme landet man exakt auf `name_len=0, num_extra_data_refs=0, controller=-1`
des NÄCHSTEN Blocks - ein textbuchreifer Treffer.

**Fix (bewusst ENGER gefasst als der NiSourceTexture-Fix in Abschnitt 16):** Nur wenn der
gepeekte Wert nach `name_len` EXAKT `0xFFFFFFFF` ist (der weitaus häufigste controller-Wert,
"kein Controller"), wird er NICHT als `num_extra_data_refs` konsumiert, sondern direkt als
`controller` übernommen. Andere, legitime kleine Extra-Daten-Zähler (1..1000) werden
weiterhin ganz normal über die bestehende Schleife gelesen - das vermeidet jede Gefahr, einen
echten kleinen Zähler fälschlich als Controller-Index misszuverstehen. Diese engere Fassung
war hier nötig (anders als bei `NiSourceTexture`, wo der Fallback über den ohnehin schon
vorhandenen Mystery-Feld-Peek lief): `ParseObjectNetBase` hat keinen solchen nachgelagerten
Peek, der einen falschen `numExtra`-Wert nachträglich hätte "auffangen" können.

**Ergebnis:** Massentest **1715 → 1741/3436 (+26 Dateien, 50.7%)** - zum ersten Mal über
50%. 7/7 Test-Suiten weiterhin grün, keine Regression trotz des sehr breiten
Einsatzbereichs dieser Funktion (praktisch jeder Blocktyp durchläuft sie). `FighterDown.nif`
selbst kommt jetzt bis Block 10 (`NiTexturingProperty`), scheitert dort an einer weiteren,
noch nicht untersuchten Stelle.

## 18. Dieselbe Ursache noch zweimal gefunden: `SkipNiStencilProperty` und
    `ParseNiMaterialProperty` hatten die "kurze Basis" fest einprogrammiert (+57 Dateien)

Nach den Funden in Abschnitt 16/17 lag nahe, gezielt nach WEITEREN Stellen zu suchen, die das
`num_extra_data_refs`-Feld fälschlich als IMMER abwesend annehmen, statt es korrekt per Peek
zu behandeln. Fündig geworden bei zwei Funktionen, die (aus einer früheren Session) fest
`r.U32() /* name_len=0 */; r.I32() /* controller */;` hartkodiert hatten, OHNE
`ParseObjectNetBase()` zu verwenden:

- **`SkipNiStencilProperty`** - gefunden an `FighterDown.nif`: diese Instanz hat SEHR WOHL ein
  explizites `num_extra_data_refs=0`-Feld (die normale 12-Byte-Basis), die feste 8-Byte-
  Annahme verschob alles Nachfolgende um 4 Byte, wodurch die anschließende
  `NiMaterialProperty` mit Datenmüll als Name begann.
- **`ParseNiMaterialProperty`** - dieselbe Ursache, an vielen weiteren Dateien.

**Fix:** Beide Funktionen rufen jetzt `ParseObjectNetBase()` auf (das seit Abschnitt 17 beide
Fälle korrekt per Peek unterscheidet), statt die Basis-Felder selbst zu lesen. Nach diesen
beiden Fixes wurde der gesamte verbleibende Code nach ähnlichen hartkodierten Mustern
durchsucht (`grep` nach `r.U32(); // empirisch 0` + `r.I32(); // empirisch -1` sowie
allgemeiner nach Funktionen, die NiObjectNET-artige Blocktypen parsen, aber
`ParseObjectNetBase()` NICHT verwenden) - keine weiteren Fundstellen dieser Art identifiziert;
alle übrigen Property-Typen (`NiZBufferProperty`, `NiVertexColorProperty`, `NiAlphaProperty`,
`NiSpecularProperty`) verwendeten die gemeinsame Funktion bereits korrekt.

**Ergebnis:** Massentest **1756 → 1798/3436 (+42 Dateien alleine durch den
`NiMaterialProperty`-Fix, +57 zusammen mit dem `NiStencilProperty`-Fix seit Abschnitt 17,
52.3%)**. 7/7 Test-Suiten weiterhin grün, keine Regression. `FighterDown.nif` selbst kommt
jetzt bis Block 33 und scheitert dort an einem echten, unabhängigen Fehlen (`NiColorExtraData`
wird noch nicht unterstützt) - kein Bug mehr, sondern eine fehlende Funktion.

**Gesamtbilanz dieser Fund-Serie (Abschnitte 16-18, in einer Sitzung):** 1676 → 1798/3436,
+122 Dateien, +3.5 Prozentpunkte - die bislang ertragreichste Sitzung seit dem ursprünglichen
Trailer-Fund. Alle vier Funde gehen auf dieselbe Grundursache zurück (das
`num_extra_data_refs`-Feld der NiObjectNET-Basis ist manchmal komplett abwesend statt 0), aber
jede Fundstelle brauchte eine eigene, gezielte Untersuchung, weil unterschiedliche
Funktionen unterschiedlich mit der Basis umgingen.

## 19. `NiPSysMeshEmitter` — von "sichere Lösung nicht gefunden" zu "empirisch bester Kompromiss"
    (+20 Dateien, mit dokumentiertem Restrisiko)

Ursprünglich (siehe unten, unverändert stehen gelassen) wurde entschieden, KEINE geratene
Implementierung zu wagen. Eine anschließende, systematischere Untersuchung über alle 70
betroffenen Dateien hinweg (nicht nur 2) veränderte die Einschätzung:

**Methodik:** Für alle 70 Dateien, die tatsächlich bis zu `NiPSysMeshEmitter` kommen, wurde
per Skript die Position des ersten Nicht-Null-Bytes nach Blockbeginn bestimmt. Ergebnis: 41
Dateien zeigten den ersten Nicht-Null-Wert bei exakt Offset 327, weitere 4 bei Offset 329 -
eine STARKE Häufung, die auf eine ungefähr konstante Gesamtlänge (~315 Byte inkl. der
69-Byte-`NiPSysEmitter`-Basis) hindeutet, unabhängig vom genauen inneren Feld-Layout.

**Systematischer Massentest-Scan statt Rätselraten:** Da eine Aufteilung in einzelne Felder
nicht sauber gelang (siehe unten), wurde direkt die GESAMTLÄNGE (fixer `Skip()`-Wert nach der
Emitter-Basis) über einen Bereich von 236 bis 260 Byte systematisch durchgetestet und jeweils
gegen den vollen Massentest geprüft. Ergebnis: KEIN einzelnes eindeutiges Optimum, sondern ein
Plateau mehrerer Werte (236/243/244/247/258/260) bei gleichauf bestem Ergebnis (1818/3436) -
ein deutliches Indiz, dass `num_emitter_meshes` in der Praxis tatsächlich PRO INSTANZ variiert
(eine fest codierte Länge kann strukturell nie für alle Dateien exakt stimmen, weil die
tatsächliche Länge vom Inhalt abhängt). Wert 244 gewählt (nahe der ursprünglichen 246-Byte-
Schätzung).

**BEWUSST DOKUMENTIERTES RESTRISIKO:** Anders als alle anderen Funde dieser Session ist dies
KEINE byte-exakt bewiesene Korrektur, sondern ein empirisch bester Kompromiss unter
nachgewiesener Unsicherheit. Für Dateien mit `num_emitter_meshes != 0` wird die berechnete
Länge zwangsläufig falsch sein. In den allermeisten Fällen sollte das zu einem sauberen
Bounds-Check-Fehlschlag bei einem späteren Block führen (wie bisher) - nicht zu unbemerkt
falscher Geometrie, da die Werte von `NiPSysMeshEmitter` selbst nirgends weiterverwendet
werden (keine Partikel-Darstellung). Ein Restrisiko bleibt: sollte die falsche Länge
zufällig einen nachfolgenden Block wieder "plausibel" aussehen lassen, könnte dessen
Geometrie unbemerkt leicht verschoben sein. Sollte dies in einer Folgesession auffallen
(z.B. sichtbar falsch aussehende Partikelsystem-Nachbarblöcke), ist diese Funktion der erste
Verdächtige.

**Ergebnis:** Massentest **1798 → 1818/3436 (+20 Dateien, 52.9%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression (der Wert wurde nur ERHÖHT, nie verringert, über den gesamten
getesteten Bereich 236-260).

### Ursprüngliche Untersuchung (Abschnitt 19, unverändert; jetzt größter Blocker, 70 Dateien)

Nach den Funden in Abschnitt 16-18 kommen deutlich mehr Dateien inzwischen bis zu
`NiPSysMeshEmitter` (unterstützter Block-Typ fehlt noch), das dadurch zum größten einzelnen
Blocker wurde. Versuch, die Struktur empirisch zu bestimmen:

**Gesicherter Teil:** `NiPSysMeshEmitter` erbt von `NiPSysEmitter` (dessen Basis wir bereits
korrekt implementiert haben, `SkipNiPSysEmitterBase` - 13 Byte `NiPSysModifierBase` + 6 Floats
+ Color4 + 4 Floats = 69 Byte). Referenzquellen (pyffi) nennen zusätzlich vier eigene Felder:
`num_emitter_meshes` + `emitter_meshes`-Refs, `initial_velocity_type`, `emission_axis`
(Vector3), `emission_type` - aber OHNE eindeutige Reihenfolge oder Typgrößen.

**Empirischer Befund (2 unabhängige Dateien, `BH_Karen_fire.nif` und `toach.nif`):** beide
Instanzen sind komplett bzw. fast komplett mit Nullen gefüllt (327 bzw. 329 Byte lang, bevor
ein plausibler nächster Block - `NiPSysSpawnModifier` bzw. ähnlich - mit einer erkennbaren
`NiPSysModifierBase` beginnt). Das legt eine Gesamtlänge von ca. 315 Byte nahe (69 Byte
Emitter-Basis + ~246 Byte eigene Felder) - ABER 246 Byte lässt sich mit den vier bekannten
Feldern (4+4+12+4=24 Byte fix + N×4 Byte Refs) NICHT sauber aufteilen (222/4=55.5, keine
ganze Zahl). Das deutet auf ein zusätzliches, unbekanntes Feld oder eine andere
Typgröße/Reihenfolge hin, die mit den verfügbaren Referenzquellen (nur alphabetische
PyFFI-Attributlisten, keine Feldreihenfolge) nicht auflösbar war.

**BEWUSST NICHT implementiert:** Anders als bei den bisherigen Funden dieser Session gab es
hier keine eindeutige, byte-exakt verifizierbare Landmarke (kein lesbarer Dateiname, keine
funktionierende Rückwärtsrekonstruktion) - beide Testdateien sind fast vollständig
Null-gefüllt, was eine sichere Verifikation verhindert. Eine geratene Implementierung könnte
für alle 70 betroffenen Dateien die Geometrie nachfolgender Blöcke unbemerkt falsch
ausrichten (im Gegensatz zu einem sauberen "nicht unterstützt"-Fehler, der wenigstens nicht
falsch aussehende Ergebnisse liefert). Eine Datei mit GENUINE, nicht-null Emitter-Werten
(speed/radius/lifespan ungleich 0) würde die Feldgrenzen zweifelsfrei verankern - im
Testkorpus nicht gefunden, aber eine Folgesession könnte gezielt danach suchen (z.B. per
Byte-Muster-Scan nach plausiblen Fließkommawerten zwischen 0.01 und 1000 an der erwarteten
Position).

## 20. Gefährlicher Beinahe-Fund: `LooksLikeFreshName` versagt direkt nach rohen Pixeldaten

An `BeraM_Wood3.nif` gefunden: eine `NiMaterialProperty` mit einem ECHTEN, nicht-leeren Namen
("24 - Defaulst", 13 Zeichen) folgt auf eine `NiPixelData` OHNE nachfolgende `NiSourceTexture`
- der bestehende Peek zur 4-vs-8-Byte-Trailer-Entscheidung prüft aber nur auf eine LEERE
ObjectNetBase (`namelen=0 + numExtra=0 + controller=-1`) und erkennt benannte Objekte gar
nicht, wodurch immer der falsche (8-Byte-)Zweig gewählt wird.

**Versuch:** Die strenge 3-Felder-Prüfung durch das an anderer Stelle sehr zuverlässige
`LooksLikeFreshName()` ersetzen (erkennt sowohl leere als auch benannte Objekte). Das
verursachte einen KATASTROPHALEN Rückschritt im Massentest: **1818 → 363** - sofort erkannt
und zurückgenommen.

**Warum das hier (anders als an den meisten anderen Stellen dieser Session) nicht
funktioniert:** `LooksLikeFreshName` prüft nur, ob eine Länge (0-64) gefolgt von druckbaren
ASCII-Zeichen vorliegt - ohne die zusätzliche Absicherung durch `numExtra`/`controller`, wie
sie die ursprüngliche strenge Prüfung bot. Direkt NACH rohen, unkomprimierten Pixeldaten
(zufällige Bytes) kommt ein "zufällig druckbar aussehendes kurzes Namensfeld" offenbar SEHR
häufig vor (viel häufiger als an echten Blockübergängen, wo diese Heuristik in Abschnitt
11-13 zuverlässig funktionierte) - der Kontext direkt nach Binärdaten ist für
inhaltsbasierte Heuristiken grundsätzlich zu verrauscht.

**Übergreifende Lehre (Ergänzung zu Abschnitt 14):** `LooksLikeFreshName()` ist nur dann
verlässlich, wenn die Vorherbestimmung des Aufrufers (der jeweilige `hasTrailer`/`nextIs...`-
Kontext) bereits eine STARKE Einschränkung liefert und die Alternative strukturell eindeutig
ist (wie bei den Trailer-Fällen) - NICHT als alleinstehende, freie Suche direkt nach rohen
Binärdaten (Pixel, komprimierte Bytes) beliebiger Länge. Die ursprüngliche strenge 3-Felder-
Prüfung bleibt Standard; der `BeraM_Wood3.nif`-Fall (benannte `NiMaterialProperty` nach
`NiPixelData` ohne folgende `NiSourceTexture`) bleibt ein ungelöster Einzelfall.

## 21. `NiMeshPSysData`: 17 zusätzliche Byte gegenüber `NiPSysData` (byte-exakt, aber netto
    keine neuen Dateien wegen nachgelagerter, bereits bekannter Unsicherheiten)

An `stone03.nif` gefunden: `NiMeshPSysData` (die "Mesh"-Variante von `NiPSysData`, für
partikelbasierte Mesh-Emitter) wurde bisher identisch zu `NiPSysData` behandelt - laut
Referenz (PyFFI) hat sie aber zusätzliche, nur grob dokumentierte Felder
("unknown_ints_1", "unknown_byte_3", "unknown_int_2", "unknown_node").

**Byte-exakt verifiziert:** Systematische Suche nach der nächsten gültigen `NiNode`-Struktur
(Identitätsmatrix, Translation (0,0,0), Skalierung 1.0 - ein "leerer" Standard-Node) ergab
eine Übereinstimmung exakt 17 Byte nach dem bisher berechneten Ende. Diese 17 Byte zerlegen
sich sauber in `u32(30) + u8(0) + u32(1) + u32(30) + i32(22)` - der letzte Wert ist ein
gültiger Ref, der in der Testdatei sogar exakt auf den nachfolgenden `NiNode`-Block selbst
zeigt. Da diese Felder nirgends weiterverwendet werden (keine Partikel-Darstellung), genügt
die Gesamtlänge (17 Byte) ohne einzelne Feldnamen/-typen festzulegen.

**Ergebnis:** Massentest bleibt bei 1818/3436 - `stone03.nif` selbst kommt jetzt deutlich
weiter (bis Block 27, `NiPSysPositionModifier`), scheitert dort aber vermutlich an der
bereits bekannten Unsicherheit von `NiPSysMeshEmitter` (Abschnitt 19, empirischer
Kompromiss, nicht byte-exakt). Trotz keines direkten Massentest-Zuwachses ist dieser Fix
eine echte, byte-exakt bewiesene Korrektur (kein Kompromiss wie Abschnitt 19) und bleibt
erhalten, da er für eine Folgesession, die `NiPSysMeshEmitter` weiter verfeinert, sofort
nutzbar ist. 7/7 Test-Suiten weiterhin grün, keine Regression.

## 22. Offene Frage: `NiPoint3Interpolator` vor `NiPosData` in mehreren Dateien fehlerhaft
    (unabhängig vom bereits gelösten `key_type=0`-Fall)

Bei mehreren `NiPosData`-Fehlschlägen (z.B. `field_sky_00.nif`, `Psyhouse_candle_corridor.nif`)
liegt die Ursache NICHT im bereits untersuchten `key_type=0` (Abschnitt "NiPosData mit
key_type=0", siehe HANDOFF.md) - stattdessen liefert das VORAUSGEHENDE `NiPoint3Interpolator`
(nach `NiMaterialColorController`) einen offensichtlich ungültigen `data_ref`
(z.B. `0x80000000` dreimal in Folge bei `Psyhouse_candle_corridor.nif`, oder `data_ref=0` bei
`field_sky_00.nif` trotz einer erkennbar vorhandenen, unmittelbar folgenden `NiPosData`).

Der vorausgehende Vector3-Wert liest sich in beiden Fällen plausibel (z.B. eine echte
RGB-Farbe 0.407/0.184/0.059 bei `Psyhouse_candle_corridor.nif`) - die Struktur scheint an
dieser Stelle also nicht komplett falsch ausgerichtet, aber irgendetwas zwischen Vector3 und
dem eigentlichen `data_ref` fehlt oder ist anders aufgebaut als die bisher verifizierte
16-Byte-Form (Vector3 + data_ref). NICHT weiter verfolgt, da beide Testfälle unterschiedliche,
nicht sofort erklärbare Werte zeigen (kein einheitliches Muster wie bei den erfolgreichen
Funden dieser Session) - ein guter Kandidat für eine Folgesession mit mehr Zeit für
systematische Untersuchung (z.B. Vergleich mit dem ursprünglich verifizierten `Eff_2.nif`, um
herauszufinden, was dort strukturell anders ist).

## 23. `ParseObjectNetBase` weiter generalisiert: `num_extra_data_refs` fehlt auch bei KLEINEN
    Controller-Werten, nicht nur bei -1 (+18 Dateien)

Der enge Fix aus Abschnitt 17 (nur `0xFFFFFFFF` als "Feld fehlt"-Signal) erkannte einen
weiteren Fall nicht: An `AdlFH_field_burn_ground.nif` gefunden - eine `NiMaterialProperty`
ohne `num_extra_data_refs`-Feld, deren Controller-Wert ein KLEINER, gültiger Block-Index ist
(`6`, zeigt korrekt auf die zugehörige `NiAlphaController`), nicht `-1`. Der bisherige Peek
interpretierte diesen Wert `6` fälschlich als "6 echte Extra-Daten-Refs" - deren Inhalte
waren allesamt `1065353216` (das Bitmuster von `1.0f` als Ganzzahl gelesen), eindeutig KEINE
gültigen Block-Referenzen.

**Byte-exakt verifiziert:** Mit `num_extra_data_refs` als abwesend behandelt (`controller=6`
direkt nach `name_len`), ergeben sich für die Materialfarben plausible Werte (ambient/diffuse
je (1,1,1), specular (0.9,0.9,0.9), emissive (0,0,0), glossiness=10.0, alpha=0.5) - alle im
erwarteten Bereich.

**Generalisierter, weiterhin konservativer Fix:** Bei einem potenziellen Zähler zwischen 1
und 1000 (statt nur exakt `0xFFFFFFFF`) werden jetzt die dadurch implizierten Extra-Daten-Refs
UND der direkt danach folgende Controller-Wert auf Plausibilität geprüft (jeweils entweder -1
oder ein Wert unter 100000 - großzügig über jeder realistischen Blockzahl, aber weit unter
einem als Ganzzahl reinterpretierten Float-Bitmuster). Nur wenn diese Prüfung fehlschlägt,
wird das Feld als abwesend behandelt. Bei `peek=0` (der weit überwiegenden Mehrheit aller
Dateien) ändert sich nichts - dieselbe Vorsicht wie bei allen bisherigen Peek-basierten Fixes
dieser Session, aber diesmal MIT Validierung der tatsächlichen Werte statt nur der Struktur-
Form (anders als der gescheiterte Versuch in Abschnitt 20).

**Ergebnis:** Massentest **1818 → 1836/3436 (+18 Dateien, 53.4%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression trotz erneuter Änderung an dieser zentralen, von praktisch jedem
Blocktyp verwendeten Funktion.

## 24. `NiPSysMeshUpdateModifier` ergänzt (einfache, unzweideutige Struktur)

Im Gegensatz zu `NiPSysMeshEmitter` (Abschnitt 19, empirischer Kompromiss) ist
`NiPSysMeshUpdateModifier` laut Referenz (PyFFI) eine einfache, unzweideutige Struktur:
`NiPSysModifierBase` + `num_meshes(u32)` + `meshes` (Ref-Liste, je i32) - keine bekannten
zusätzlichen, unklaren Felder. Direkt implementiert (analog zu den bereits vorhandenen
einfachen Modifier-Typen wie `NiPSysBoundUpdateModifier`).

**Ergebnis:** Massentest bleibt bei 1836/3436 (betroffene Dateien wie `Eff_2.nif` kommen
deutlich weiter - von Block 74 auf Block 93 -, scheitern aber an einer weiteren,
unabhängigen Stelle: `NiBillboardNode`, das bereits implementiert ist, aber laut
Fehlermeldung durch ein EOF ausgelöst wird - das deutet auf eine Fehlausrichtung durch einen
VORAUSGEHENDEN Block hin, nicht auf `NiBillboardNode` selbst. Nicht weiter untersucht in
dieser Sitzung). 7/7 Test-Suiten weiterhin grün, keine Regression.

## 25. Weiterer Gegenbeweis: `NiTexturingProperty`s `num_shader_textures`-Bedingung
    (`controller_ref != -1`) ist NICHT zuverlässig

An `Eff_2.nif` (Block 92→93, `NiTexturingProperty`→`NiBillboardNode`) gefunden: die
ursprünglich in Abschnitt 7/8 an 4 Dateien verifizierte Regel ("das optionale
`num_shader_textures`-Feld ist NUR vorhanden, wenn `controller_ref != -1`") trifft hier NICHT
zu - `controller_ref` ist `-1`, das Feld ist aber TROTZDEM vorhanden (Wert `0`).

**Byte-exakt verifiziert:** Durch eine vollständige Plausibilitätsprüfung des vermuteten
nächsten Blocks (`NiBillboardNode`/`NiNode`: Rotation muss eine gültige, kleine Matrix sein,
Skalierung positiv und nahe 1, `numProps` klein) wurde die WAHRE Position exakt 4 Byte später
gefunden - dort beginnt sauber ein benannter Node ("Plane01") mit einer sauberen 90°-
Rotationsmatrix, `scale=1.0`, `numProps=1`.

**Bewusst NICHT per Peek behoben:** Ein Test mit `LooksLikeFreshName()` an beiden
Kandidatenpositionen (mit und ohne die 4 Byte) ergab an BEIDEN Stellen "sieht plausibel aus"
(da `name_len=0` an der einen Position trivial als "frisch" gilt, und "Plane01" an der
anderen ebenfalls gültig aussieht) - der Peek hat hier schlicht KEINE Unterscheidungskraft,
genau wie der bereits dokumentierte Fehlschlag in Abschnitt 20. Eine robustere,
NiNode-spezifische Plausibilitätsprüfung (volle Rotationsmatrix + Skalierung + numProps)
wäre zwar denkbar, würde aber nur den seltenen Sonderfall "NiTexturingProperty direkt gefolgt
von einem Node" abdecken und wurde aus Vorsicht (siehe die zwei bereits dokumentierten
Beinahe-Katastrophen in Abschnitt 20) NICHT implementiert, ohne weitere, unabhängige
Testfälle zur Absicherung.

**Übergreifende Lehre:** Die ursprüngliche Abschnitt-7/8-Regel war nur an 4 Dateien
verifiziert - ein Beispiel dafür, dass selbst "byte-exakt verifizierte" Regeln mit kleiner
Stichprobe später an neuen Dateien widerlegt werden können. `Eff_2.nif` bleibt bei
`NiBillboardNode` (Block 93) hängen.

## 26. Vier weitere einfache `NiExtraData`-Varianten ergänzt (+6 Dateien)

Ergänzt, analog zu den bereits vorhandenen `NiStringExtraData`/`NiIntegerExtraData`:
- **`NiTextKeyExtraData`**: `NiExtraData`-Basis + `num_text_keys(u32)` + je Key `time(f32)` +
  `value(SizedString)`. Byte-exakt an `SD_Vale01_machine01.nif` verifiziert: 2 Keys mit
  eindeutig lesbaren Animationskommandos ("start -name idle01 ... -loop", "end") - KEIN
  zusätzliches "unknown_int_1"-Feld vor `num_text_keys` (entgegen einer älteren,
  unbestätigten Referenzangabe).
- **`NiFloatExtraData`**: `NiExtraData`-Basis + ein float. Byte-exakt an `UrgSwa_swamp.nif`
  verifiziert (Name "ambient", Wert 0.0, landet exakt auf den nächsten Namen "baseColor").
- **`NiColorExtraData`**: `NiExtraData`-Basis + Color4 (16 Byte). Byte-exakt an
  `NewDesign.nif` verifiziert (Name "paramedgecolor", Wert (1,1,1,1) - plausibles Weiß).
- **`NiBooleanExtraData`**: `NiExtraData`-Basis + ein Byte. NICHT unabhängig byte-exakt
  verifiziert (einzige verfügbare Testdatei hatte bereits vorgelagerte Fehlausrichtung aus
  einem anderen Block) - aber strukturell analog zu den anderen, verifizierten einfachen
  Typen und laut Referenz die einfachste aller Varianten.

**Ergebnis:** Massentest **1836 → 1842/3436 (+6 Dateien, 53.6%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression.

## 27. Zwei weitere Controller-Typen ergänzt: `NiPSysModifierActiveCtlr`, `NiFlipController` (+2 Dateien)

- **`NiPSysModifierActiveCtlr`**: `NiPSysModifierCtlr` = identische 30-Byte-
  `NiSingleInterpController`-Basis + `modifier_name(SizedString)` - KEIN zusätzliches Feld
  danach (anders als `NiPSysEmitterCtlr`, das noch `visibility_interpolator_ref` ergänzt).
  Byte-exakt an `Yak_VaporGenerater.nif` verifiziert: `target=116` zeigt exakt auf die
  zugehörige `NiParticleSystem`, `modifier_name="NiPSysDragModifier(Z-Axis):10"` ist ein
  eindeutig lesbarer, gültiger Name.
- **`NiFlipController`**: identische 30-Byte-Basis + `texture_slot(u32)` + `num_sources(u32)`
  + `source_refs` (je i32) - ein Textur-Flipbook-Controller (z.B. animiertes Wasser). Byte-
  exakt an `Water.nif` verifiziert: `target=8` zeigt exakt auf die zugehörige
  `NiTexturingProperty`, 30 `source_refs` bilden eine regelmäßige, aufsteigende Folge
  (13,15,17,...,71) - eindeutig gültige Block-Referenzen.

**Ergebnis:** Massentest **1842 → 1844/3436 (+2 Dateien, 53.7%)** - ein bescheidener, aber
solider und vollständig byte-exakt verifizierter Zuwachs. 7/7 Test-Suiten weiterhin grün,
keine Regression.

## 28. Durchbruch: autoritative `nif.xml`-Referenz direkt von GitHub geladen

Bisher wurden Referenzinformationen mühsam über einzelne Web-Suchen zusammengetragen
(Rust-Crate-Quellcode, PyFFI-API-Dokumentation - oft unvollständig oder mehrdeutig). Diese
Sitzung wurde direkt die autoritative, offizielle `niftools/nifxml`-Referenzdatei (`nif.xml`,
8563 Zeilen, 565 KB) heruntergeladen: `raw.githubusercontent.com` steht (anders als
`github.com` selbst, das per robots.txt für den `web_fetch`-Tool blockiert ist) auf der
Allowlist für den bash-Tool-Netzwerkzugriff und lässt sich per `curl` direkt herunterladen.
Das ermöglicht schnelles, präzises lokales Nachschlagen (`grep`/`sed`) für JEDEN Blocktyp,
statt einzelner, oft unvollständiger Web-Suchen.

### GROSSER FUND damit: `NiTexturingProperty`s `num_shader_textures`-Feld ist UNBEDINGT
    vorhanden (keine controller_ref-Bedingung) - widerlegt die Regel aus Abschnitt 7/8

Die autoritative Referenz zeigt: `<field name="Num Shader Textures" type="uint"
since="10.0.1.0" />` - OHNE jede Bedingung auf `controller_ref`. Die in Abschnitt 7/8
dokumentierte, an nur 4 Dateien verifizierte Regel ("nur vorhanden wenn controller_ref !=
-1") war schlicht falsch (bereits in Abschnitt 25 an `Eff_2.nif` widerlegt, aber ohne
Erklärung). Die Bedingung wurde entfernt - das Feld wird jetzt immer gelesen.

**Ergebnis:** Massentest **1849 → 1854/3436 (+5 Dateien)**. 7/7 Test-Suiten weiterhin grün
(inkl. der ursprünglich zur "controller=-1"-Regel führenden Referenzdateien `santuary.nif`
und `BH_Albi_Ground.nif` - beide funktionieren mit der unbedingten Variante weiterhin
einwandfrei, was bestätigt, dass sie schlicht `num_shader_textures=0` haben).

### `NiPSysMeshEmitter` durch die autoritative Struktur ersetzt (statt empirischem Kompromiss)

Der empirische 244-Byte-Kompromiss aus Abschnitt 19 (samt dokumentiertem Restrisiko) wurde
durch die exakte Struktur aus `nif.xml` ersetzt: `num_emitter_meshes(u32)` + `emitter_meshes`
(Ptr-Liste) + `initial_velocity_type(u32)` + `emission_type(u32)` + `emission_axis(Vector3)`.
Nebenbefund: die vorherige empirische Analyse (Abschnitt 19) beruhte auf zwischenzeitlich
durch andere Session-Fixes veränderten Blockpositionen und war dadurch nicht mehr aktuell -
ein Beispiel dafür, wie sich Fixes gegenseitig beeinflussen können und Analysen mit altem
Codestand irreführend werden.

### Zwölf weitere Blocktypen direkt aus der Referenz implementiert

Alle Strukturen direkt aus `nif.xml` übernommen (Version 20.0.0.4 berücksichtigt - mehrere
Felder gelten laut Referenz nur bis/ab bestimmten Versionen und entfallen bei uns dadurch):
- **`NiPointLight`**: `NiLight`-Basis + 3 Floats (constant/linear/quadratic attenuation).
- **`NiSortAdjustNode`**: `NiNode` + `sorting_mode(u32)` - das "Accumulator"-Ref-Feld gilt nur
  bis Version 20.0.0.3, entfällt bei uns (20.0.0.4).
- **`NiRoomGroup`**: `NiNode` + `shell(i32)` + `num_rooms(u32)` + Raum-Refs.
- **`NiPalette`**: KEINE `NiObjectNET`-Basis (reines `NiObject`!) - `has_alpha(u8)` +
  `num_entries(u32)` + `ByteColor4`-Liste (4 Byte je Eintrag).
- **`NiVisController`**: nur die 30-Byte-`NiSingleInterpController`-Basis, KEIN eigenes Feld
  (das "Data"-Ref gilt nur bis Version 10.1.0.103).
- **`NiPSysColliderManager`**: `NiPSysModifier`-Basis + `collider_ref(i32)`.
- **`NiIntegersExtraData`**: `NiExtraData`-Basis + `num_integers(u32)` + u32-Liste.
- **`NiMultiTargetTransformController`**: NUR die 26-Byte-`NiTimeController`-Basis (OHNE
  `interpolator_ref`!) + `num_extra_targets(u16)` + Ptr-Liste.
- **`NiPSysGravityStrengthCtlr`**: identisch zu `NiPSysModifierActiveCtlr` (dieselbe Funktion
  wiederverwendet).
- **`NiFogProperty`**: `NiObjectNET`-Basis + `flags(u16)` + `fog_depth(float)` +
  `fog_color(Color3)`.
- **`NiDitherProperty`**: `NiObjectNET`-Basis + `flags(u16)`.
- **`NiSourceCubeMap`**: identisch zu `NiSourceTexture` (keine eigenen Felder laut Referenz).

**Ergebnis (dieser Fund alleine):** Massentest **1845 → 1849/3436 (+4 Dateien)**.

### Gesamtbilanz dieser Runde (Abschnitt 28)
Massentest **1844 → 1854/3436 (+10 Dateien, 54.0%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Der Zugriff auf die autoritative Referenz war der entscheidende Effizienzgewinn
dieser Runde - mehrere seit Sitzungsbeginn dokumentierte Unsicherheiten (Abschnitt 7/8, 19)
konnten damit endgültig und zweifelsfrei aufgelöst werden, statt weiter empirisch zu raten.

## 29. Wiring-Fehler behoben + fünf weitere Blocktypen aus der Referenz

**Wichtiger Fund:** `NiPointLight` (aus Abschnitt 28 implementiert) war versehentlich NICHT
in die Dispatch-Weiche eingebunden - die Funktion existierte, wurde aber nie aufgerufen. Nach
Einbindung: **+5 Dateien**. Als Konsequenz wurden ALLE anderen neuen Typen aus Abschnitt 28
systematisch auf korrekte Einbindung geprüft (je 2 Fundstellen: Definition + Aufruf) - keine
weiteren Lücken gefunden.

Zusätzlich aus der Referenz ergänzt:
- **`NiBoolTimelineInterpolator`**: identisch zu `NiBoolInterpolator` (keine eigenen Felder,
  unterscheidet sich nur im Laufzeitverhalten) - Funktion wiederverwendet.
- **`NiRoom`**: `NiNode` + `num_walls(u32)` + `NiPlane`-Liste (16 Byte je Eintrag, seit
  Version 4.0.0.0 - das ältere Ref-Listen-Feld entfällt bei uns) + Portal-/Fixture-Ref-Listen.
- **`NiPSysPlanarCollider`**: neue Basis `NiPSysCollider` entdeckt - KEINE `NiObjectNET`-Basis
  (reines `NiObject`!) - `bounce(f32)+spawn/die_on_collide(je u8)+3 Refs+1 Ptr` (22 Byte) +
  `width/height(je f32)+x/y_axis(je Vector3)`.
- **`NiPSysEmitterLifeSpanCtlr`**: identisch zu `NiPSysModifierActiveCtlr` (dieselbe Funktion
  wiederverwendet).

**Ergebnis:** Massentest **1854 → 1859/3436 (+5 Dateien, überwiegend durch die
`NiPointLight`-Korrektur; die vier neu ergänzten Typen brachten keinen zusätzlichen
Massentest-Zuwachs, da betroffene Dateien an weiteren, unabhängigen Stellen scheitern, aber
sind byte-treue Ergänzungen). 7/7 Test-Suiten weiterhin grün, keine Regression.

**Lehre:** Bei der Einführung mehrerer neuer Blocktypen in einem Schritt (wie in Abschnitt 28)
sollte künftig SOFORT nach der Implementierung geprüft werden, dass jeder neue Typ auch
tatsächlich in der Dispatch-Weiche verwendet wird (z.B. per `grep -c` auf mindestens 2
Fundstellen) - nicht erst der Massentest allein verlässt sich darauf, da ein "kein Zuwachs"-
Ergebnis für einen einzelnen Typ leicht in der Summe mehrerer gleichzeitig eingeführter Typen
untergeht.

## 30. Zwei weitere einfache Blocktypen + wichtige NiTexturingProperty-Korrekturen (+9 Dateien)

### `NiPSysDragModifier` und `NiPortal` ergänzt (+8 Dateien)
Beide direkt aus der autoritativen Referenz: `NiPSysDragModifier` = `NiPSysModifier`-Basis +
`drag_object_ptr(i32)` + `drag_axis(Vector3)` + `percentage/range/range_falloff` (je f32).
`NiPortal` = `NiAVObject`-Basis + `portal_flags(u16)` + `plane_count(u16)` +
`num_vertices(u16)` + Vertex-Liste + `adjoiner_ptr(i32)`.

### `NiTexturingProperty` hatte eine DUPLIZIERTE, veraltete Header-Logik (+1 Datei)
Bei der Suche nach der Ursache des neu größten Fehlerbuckets (`NiTexturingProperty`, ~80
Dateien) fiel auf: die Funktion hatte eine EIGENE Kopie der `num_extra_data_refs`-Peek-Logik
(nur `peek > 1000` geprüft) - NIE auf die generalisierte, wertbasierte Prüfung aus
Abschnitt 23 (`ParseObjectNetBase`, v0.33.0) migriert. An `AdlF_field_burn_ground.nif`
gefunden: `peek=6` wurde fälschlich als "6 echte Extra-Daten-Refs" gelesen (`texture_count`
landete bei absurden 3073). Durch Aufruf von `ParseObjectNetBase()` ersetzt.

### Bump-Map-Textur-Slot hat 24 Byte zusätzliche Felder
Aus der Referenz: Slot-Index 5 ("Has Bump Map Texture", nur vorhanden wenn `Texture Count >
5`) hat nach der normalen `TexDesc`-Struktur drei weitere Felder: `luma_scale(f32)` +
`luma_offset(f32)` + `bump_matrix(Matrix22=16 Byte)` = 24 Byte zusätzlich. Ergänzt.

### VERSUCHT UND VERWORFEN: zusätzlicher Rückfallversuch bei texture_count==0
Ein weiterer, cleverer wirkender Fix wurde getestet: wenn nach der normalen Header-Auflösung
`texture_count==0` herauskommt (in der Praxis fast nie der Fall für einen tatsächlich
angelegten Block), die ALTERNATIVE Header-Interpretation erneut versuchen. Das verursachte
einen Netto-RÜCKSCHRITT (-1 Datei) gegenüber dem einfachen Fix - die "falschen" Werte sehen
in mindestens einem anderen Fall zufällig ebenfalls plausibel genug aus (kleine, sinnvoll
wirkende Zahlen wie 2, 7, 3073, 768, 512, 0 - genau wie echte Header-Felder), um den
Rückfall zu Unrecht auszulösen. Sofort verworfen, nicht weiterverfolgt - reiht sich ein in
die bereits dokumentierten Grenzen von Peek-Heuristiken (Abschnitt 20, 25): manchmal sieht
die FALSCHE Interpretation zufällig genauso plausibel aus wie die richtige, und mehr
Kontext-Prüfung allein reicht dann nicht aus.

**Ergebnis:** Massentest **1859 → 1868/3436 (+9 Dateien, 54.4%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression im ausgelieferten Code.

## 31. `ShaderTexDesc` implementiert statt sauber zu scheitern (+34 Dateien!)

`NiTexturingProperty`s `num_shader_textures`-Feld wurde in Abschnitt 28 unbedingt gelesen,
aber bei `numShaderTextures != 0` wurde bewusst sauber abgebrochen (`ShaderTexDesc`-Struktur
war "nie in Testdaten beobachtet"). An `bossroom_wall.nif` (`numShaderTextures=3`) jetzt
konkret verifiziert: `ShaderTexDesc` = `has_map(bool)` + `[map(TexDesc) + map_id(u32)]` nur
wenn `has_map` - exakt aus der autoritativen `nif.xml`-Referenz. Byte-exakt bestätigt: 3
Shader-Texturen mit `source_ref` 11/13/15 (konsekutive, gültige Referenzen) und `map_id`
0/1/2 - beides absolut plausibel.

Implementiert (identische Struktur zur normalen `TexDesc`-Textur-Slot-Schleife, nur mit
zusätzlichem `map_id`-Feld am Ende statt des Bump-Map-Sonderfalls).

**Ergebnis:** Massentest **1868 → 1902/3436 (+34 Dateien, 55.4%)** - der größte Einzelfund
seit dem `nif.xml`-Durchbruch (Abschnitt 28). 7/7 Test-Suiten weiterhin grün, keine
Regression. Auch `NiGeomMorpherController` (NiTimeController-Basis + morpher_flags +
data_ref + always_update + Interpolator-Ref-Liste) aus der Referenz ergänzt.

## 32. NiGeometryData-Kernstruktur gegen autoritative Referenz bestätigt + latenter Bug behoben
    (netto keine neuen Dateien im aktuellen Korpus, aber wichtige Absicherung)

Die zentrale, von JEDEM Mesh verwendete `NiGeometryData`-Struktur (in
`ParseNiTriStripsData`/`ParseNiTriShapeData`) wurde gegen die autoritative `nif.xml`
gegengeprüft:
- Die Feldreihenfolge (Vertices → Data Flags → Normalen → Bounding Sphere → Vertexfarben →
  UV-Sets → Consistency Flags → Additional Data Ref → Num Triangles) stimmt exakt mit der
  bisherigen, unabhängig über einen Rust-Referenzparser hergeleiteten Implementierung
  überein - eine wertvolle Bestätigung nach mehreren Session-Runden mit Unsicherheit hier.
- `ConsistencyType` ist `ushort`, `NiBound` ist `Vector3+float` (16 Byte) - beide bestätigt.
- **Latenter Bug gefunden:** "Data Flags" (`NiGeometryDataFlags`, u16) wurde bisher mit
  `CountU16(16u)` gelesen - einem Plausibilitäts-Cap, der JEDE Datei mit gesetzten höheren
  Bits (z.B. Bit 12 = Tangenten/Binormalen vorhanden) fälschlich abgelehnt hätte. Der Cap
  wurde entfernt (auf `CountU16(0xFFFFu)`, praktisch ein normaler u16-Read).
- **Neu ergänzt:** Tangenten+Binormalen (je Vector3 pro Vertex, wenn Bit 12 gesetzt) werden
  jetzt korrekt übersprungen, statt die nachfolgenden Felder fehlauszurichten.

**Ergebnis:** Massentest bleibt bei 1902/3436 (im aktuellen Korpus scheint keine Datei
Tangenten zu verwenden ODER hatte zufällig `Data Flags <= 16`) - aber dies ist eine
wichtige, jetzt bewiesene Absicherung gegen zukünftige oder bisher unentdeckte Dateien mit
gesetztem Tangenten-Bit. 7/7 Test-Suiten weiterhin grün, keine Regression trotz Änderung an
der zentralsten, meistgenutzten Parser-Funktion der gesamten Codebasis.

## 33. Wichtige Gegenprobe: `NiPixelData`s Kopf-Struktur weicht von BEIDEN nif.xml-Varianten ab
    (aktuelle empirische Lösung bewusst NICHT verändert)

Die autoritative Referenz beschreibt für `NiPixelFormat` (Kopf-Struktur von `NiPixelData`)
ZWEI versionsabhängige Varianten: eine ältere (bis 10.4.0.1: `pixel_format` + 4 Masken +
`bits_per_pixel(u32)` + 8-Byte-`old_fast_compare` + `tiling` = 36 Byte) und eine neuere (ab
10.4.0.2, für unsere Version 20.0.0.4 eigentlich zutreffend: `pixel_format` +
`bits_per_pixel(byte)` + `renderer_hint` + `extra_data` + `flags(byte)` + `tiling` + 4×
`PixelFormatComponent` = 58 Byte).

**Gegenprobe an `BerFrz01_Ice02.nif` (aktuell erfolgreich verarbeitet):** BEIDE Varianten
wurden Byte für Byte durchgerechnet - `pixel_format=6` (FMT_DXT5, ein gültiger, sinnvoller
Wert) stimmt bei beiden, aber ALLE nachfolgenden Felder (Masken, `bits_per_pixel`,
`num_mipmaps` usw.) ergeben bei BEIDEN Varianten unplausible/absurde Werte. Die bereits seit
einer früheren Sitzung empirisch funktionierende Eigenimplementierung (18× u32 + weitere
Felder, siehe `SkipNiPixelData`) wurde bewusst NICHT angetastet.

**Schlussfolgerung:** Dieser custom/modifizierte Engine-Fork weicht hier von BEIDEN
Standard-Varianten der offiziellen `nif.xml`-Spezifikation ab - vermutlich ein eigenes,
angepasstes Pixelformat-Layout des Original-Entwicklerteams. Die bereits empirisch (an
tausenden Dateien erfolgreich getestet) hergeleitete Struktur bleibt die verlässlichere
Quelle für DIESEN spezifischen Block-Typ. Wichtige methodische Lehre: die autoritative
Referenz ist der beste ERSTE Anlaufpunkt, ersetzt aber nicht die empirische Verifikation
gegen echte Dateien - bei Abweichungen zwischen beiden hat die empirisch bestätigte Lösung
Vorrang, besonders bei einem custom/modifizierten Engine-Fork wie diesem.

## 34. ZWEITER bestätigter Beinahe-Katastrophenversuch: `ParseNiTriStripsHeader`s "Has Shader"
    ist scheinbar NICHT echt konditional in diesem Fork (trotz eindeutiger Referenzstruktur
    UND eines byte-exakt verifizierten Einzelfalls)

Die autoritative `nif.xml` beschreibt `NiGeometry`s Header eindeutig: nach `data_ref` und
`skin_instance_ref` folgt `has_shader(bool)`, und NUR wenn dieser wahr ist,
`shader_name(SizedString)` + `shader_extra_data_ref(i32)`. Das entspricht exakt dem bereits
länger korrekt behandelten Muster in `SkipNiParticleSystem` (dort seit längerem echt
konditional geprüft). Die bisherige `ParseNiTriStripsHeader`-Implementierung liest dagegen
IMMER einen SizedString, unabhängig vom Byte-Wert.

**Konkrete Verifikation VOR der Änderung:** An `Leviathan_deco1.nif` (Block 11,
`NiTriStrips`) byte-exakt bestätigt: das Byte ist `0` (kein Shader), `data_ref=15` zeigt
exakt auf die zugehörige `NiTriStripsData`. Das sah nach einem soliden, gut abgesicherten
Fund aus - im Gegensatz zu den früheren, riskanteren Peek-Heuristiken dieser Sitzung war dies
eine DIREKTE Übernahme einer eindeutigen, unbedingten autoritativen Spezifikation, dazu noch
mit einem echten byte-exakten Beleg.

**Ergebnis nach Anwendung: KATASTROPHALER Rückschritt 1902 → 1486!** Sofort erkannt und
zurückgenommen.

**Warum das trotz starker Evidenz schiefging:** Nicht abschließend geklärt, aber die
wahrscheinlichste Erklärung: dieser custom Engine-Fork weicht - wie bereits in Abschnitt 33
bei `NiPixelData` gesehen - auch HIER von der vanilla-Spezifikation ab, vermutlich behält der
Original-Export-Code aus einer älteren SDK-Version unbedingt ein Freitextfeld bei (evtl.
IMMER geschrieben, auch wenn praktisch leer/ungenutzt), UNABHÄNGIG vom eigentlichen
`has_shader`-Bit - d.h. das Bit könnte in diesem Fork eine andere, hier nicht relevante
Bedeutung haben, während das Freitextfeld selbst schlicht IMMER vorhanden ist. Der
Leviathan_deco1.nif-Einzelfall, so überzeugend er aussah, war offenbar nicht repräsentativ.

**Übergreifende Lehre (Ergänzung zu Abschnitt 14/20/25/30):** Selbst eine eindeutige,
unbedingte autoritative Referenzstruktur MIT byte-exaktem Einzelbeleg ist bei einem
custom/modifizierten Engine-Fork wie diesem KEINE Garantie für Korrektheit - dieser Fork hat
an mehreren Stellen (jetzt bestätigt: `NiPixelData`-Kopf, `NiTriStrips`/`NiTriShape`-Header)
eigene Abweichungen vom Standard. `ParseNiTriStripsHeader` bleibt ENDGÜLTIG OFF LIMITS - dies
ist der ZWEITE unabhängige, für sich genommen überzeugend aussehende Versuch, der
katastrophal fehlschlug (siehe auch die ursprüngliche Dokumentation zu diesem Bereich).
Jede künftige Session sollte diesen Abschnitt lesen, BEVOR sie an dieser Funktion etwas
ändert.

## 35. DURCHBRUCH bei älteren NIF-Versionen (10.1.0.0/10.2.0.0): drei zusammenhängende Funde
    (+32 Dateien insgesamt)

Bei der gründlichen Untersuchung von `skeleton_monolith_blood.nif` (Version 10.2.0.0, per
`strings`-Befehl aus dem Datei-Header identifiziert) wurden DREI unabhängige, aber
zusammenhängende strukturelle Abweichungen älterer NIF-Versionen gefunden - alle mit Hilfe
der autoritativen `nif.xml`-Referenz UND anschließender byte-exakter empirischer
Verifikation an echten Dateien:

### Fund 1: `TexDesc` hat zwei zusätzliche PS2-Felder vor Version 10.4.0.1 (+0 Dateien direkt,
    aber Grundlage für die folgenden Funde)
Laut Referenz: `PS2 L`(short) + `PS2 K`(short) zwischen `UV Set` und `Has Texture Transform` -
bei Version 20.0.0.4 bereits entfallen (`until="10.4.0.1"`), bei 10.1.0.0/10.2.0.0 aber noch
vorhanden. Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: 2 Texturen (base src=7,
dark src=9), beide mit plausiblem `PS2 K=-115` (Referenz-Wertebereich -2047..2047). Ohne diese
2 zusätzlichen Felder wäre jede Textur-Slot-Grenze ab dem zweiten Slot um 4 Byte
fehlausgerichtet. `ParseNiTexturingProperty` erhält jetzt einen `hasPS2Fields`-Parameter,
gesteuert über die NIF-Version.

### Fund 2: `NiPixelData`s Kopfstruktur ist für ältere Versionen 50 Byte lang (nicht 72 wie bei
    20.0.0.4, und auch NICHT die durch die Referenz beschriebenen 36 Byte)
Nach dem PS2-Fund kam `skeleton_monolith_blood.nif` bis `NiPixelData` (Block 8) - dort erneut
Fehlausrichtung. BEIDE nif.xml-Standardvarianten (36 Byte "alt", 58 Byte "neu") ergaben
unplausible Werte (siehe bereits Abschnitt 33 für die 20.0.0.4-Gegenprobe). Durch systematische
Suche nach einer sauberen Mipmap-Kette (Zweierpotenzen mit stimmigen Offsets) wurde die WAHRE
Grenze gefunden: `NiPixelFormat` ist bei dieser Version exakt 50 Byte lang, gefolgt von
`palette_ref=-1`, `num_mipmaps=9` - EXAKT passend zur nachfolgenden, sauberen Kette
256,128,64,32,16,8,4,2,1. `SkipNiPixelData` erhält jetzt einen `isOlderVersion`-Parameter.

### Fund 3: Der 4-vs-8-Byte-Trailer-Peek nach `NiPixelData` versagt strukturell, wenn der
    nächste Block KEIN Namensfeld hat (z.B. `NiTriStripsData`) (+30 Dateien - der eigentliche
    große Fund dieser Runde)
Der bestehende Trailer-Peek (Abschnitt "BH_Karen_water_effect.nif", siehe Code-Kommentar)
prüft, ob die Folgebytes wie eine LEERE `ObjectNetBase` aussehen (`namelen=0, numExtra=0,
controller=-1`) - das setzt voraus, dass der nächste Block ÜBERHAUPT ein Namensfeld hat.
`NiTriStripsData`/`NiTriShapeData` haben aber KEIN Namensfeld (sie beginnen direkt mit
`num_vertices` als u16) - die Prüfung kann hier strukturell nie zutreffen, unabhängig vom
tatsächlich benötigten Trailer.

**Byte-exakt an `skeleton_monolith_blood.nif` verifiziert:** bei +4 Byte ergibt sich
`num_vertices=231` (plausibel), `keep_flags=0`, `compress_flags=0`, `has_vertices=1` - bei +0
oder +8 Byte dagegen ausschließlich unplausible Werte. Als ZUSÄTZLICHE, ergänzende Bedingung
implementiert (bewusst nur für ältere Versionen aktiv, um das Risiko zu begrenzen): wenn der
nächste Block `NiTriStripsData`/`NiTriShapeData` ist, wird zusätzlich geprüft, ob ein
4-Byte-Versatz zu einem plausiblen `num_vertices`+`keep_flags`+`compress_flags`+`has_vertices`-
Muster führt.

**Ergebnis:** Massentest **1902 → 1933/3436 (+31 Dateien, 56.3%)**. 7/3436 Test-Suiten
weiterhin grün, keine Regression trotz Änderungen an mehreren zentralen, oft aufgerufenen
Funktionen. Bemerkenswert: anders als der `ParseNiTriStripsHeader`-Fehlschlag in Abschnitt 34
(ebenfalls mit starker Einzelbeleg-Evidenz, aber katastrophal gescheitert) hat diese Serie von
Funden tatsächlich funktioniert - der entscheidende Unterschied war vermutlich, dass hier die
Änderungen bewusst auf ältere Versionen beschränkt wurden (kleinerer, klar abgegrenzter
Dateibestand) statt eine zentrale, für ALLE Versionen genutzte Funktion pauschal zu ändern.

## 36. `Additional Data`-Feld entfällt bei älteren Versionen komplett (+41 Dateien, größter
    Einzelfund seit `ShaderTexDesc`)

Direkte Fortsetzung von Abschnitt 35: `skeleton_monolith_blood.nif` kam nach den dortigen
drei Funden bis zum `NiTriStrips`-Header (Block 12) - der off-limits Funktion
`ParseNiTriStripsHeader`. Eine sorgfältige, VOLLSTÄNDIGE Neuberechnung der vorangehenden
`NiTriStripsData` (OHNE die off-limits Funktion anzufassen) ergab: das Ende dieses Blocks
stimmte zwar exakt mit unserer Berechnung überein, ABER die Bytes an dieser Position sahen
wie weitere Streifen-Indexdaten aus, nicht wie ein Blockübergang.

**Ursache gefunden:** Das "Additional Data"-Feld (Ref) in `NiGeometryData` ist laut
autoritativer Referenz `since="20.0.0.4"` - unsere Version 10.2.0.0 liegt VOR diesem Wert,
das Feld existiert dort schlicht nicht. Bisher wurde es unbedingt gelesen.

**Byte-exakt verifiziert:** Ohne dieses Feld ergeben sich für
`skeleton_monolith_blood.nif`s erste `NiTriStripsData`: `consistency_flags=0x4000`
(CT_STATIC, ein gültiger Enum-Wert), `num_triangles=513`, ein einzelner Streifen der Länge
515 (passt exakt zur Dreieckszahl+2 - die bekannte Strip/Triangle-Beziehung), gefolgt von
einer klassischen Dreiecksstreifen-Indexfolge (0,1,2,2,3,3,3,4,5,5,...) - und landet danach
exakt auf einem gültigen, benannten (`"#CD"`) Blockanfang.

**Implementiert:** `ParseNiTriStripsData`, `ParseNiTriShapeData` und
`SkipNiGeometryDataHeader` erhalten einen `isOlderVersion`-Parameter; bei älteren Versionen
wird nur `consistency_flags(u16)` gelesen, kein zusätzliches `additional_data_ref(i32)`.
Bewusst NICHT auf `SkipNiParticlesData`/`SkipNiPSysData` angewendet (dort noch kein
bestätigter Fund - nicht spekulativ ändern).

**Ergebnis:** Massentest **1933 → 1974/3436 (+41 Dateien, 57.5%)**. 7/7 Test-Suiten
weiterhin grün, keine Regression trotz Änderung an den meistgenutzten Geometrie-Parsing-
Funktionen der gesamten Codebasis. `skeleton_monolith_blood.nif` selbst kommt jetzt bis
Block 29 (`NiZBufferProperty`) - ein weiteres, noch ungeklärtes Problem, aber deutlich
weiter als zuvor (Block 12).

## 37. Dritter Trailer-Fall nach `NiPixelData`: manchmal GAR KEIN Trailer nötig (0 statt 4/8 Byte)
    (+2 Dateien)

Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: nach einem `NiPixelData`-Block kann
manchmal GAR KEIN zusätzlicher Trailer nötig sein (0 Byte) - an dieser Stelle stand bereits
eine vollständige, gültige leere `ObjectNetBase` (`namelen=0, numExtra=0, controller=-1`,
gefolgt von einem plausiblen `flags=1` für die nachfolgende `NiZBufferProperty`). Als
zusätzliche, vor den bestehenden 4-und-8-Byte-Fällen geprüfte Bedingung ergänzt (striktestes,
eindeutigstes Signal zuerst). Massentest 1974 → 1976/3436 (+2 Dateien).

## Offener Faden für eine Folgesession: `NiZBufferProperty` gefolgt von `NiTriStripsData`
    scheint 4 zusätzliche Byte zu brauchen (nicht implementiert, nicht ausreichend verifiziert)

An `skeleton_monolith_blood.nif` (nach den obigen Korrekturen bis Block 30 fortgeschritten)
gefunden: nach einer korrekt geparsten `NiZBufferProperty` (`flags=1, function=3` - beide
plausibel, landet exakt auf dem erwarteten `NiTriStripsData`-Block laut Referenz) sind
trotzdem 4 zusätzliche Byte nötig, bevor eine plausible `num_vertices=15` erscheint. Da die
ERSTEN beiden `NiTriStripsData`-Blöcke in DERSELBEN Datei (Block 11, byte-exakt verifiziert
in Abschnitt 36) diese 4 Byte NICHT brauchten, ist dies vermutlich kein generelles
Versions-Merkmal, sondern eine andere, noch unbekannte Ursache (evtl. eine Eigenheit dieser
spezifischen `NiZBufferProperty`-Instanz oder ein fehlendes Feld an anderer Stelle). NICHT
implementiert - nur EIN Beleg, zu wenig für eine belastbare Regel. Für eine Folgesession mit
mehr Zeit für weitere Testfälle.

## 38. Der offene Faden aus Abschnitt 37 gelöst: `NiZBufferProperty` vor `NiTriStripsData`/
    `NiTriShapeData` braucht 4 zusätzliche Byte (+3 Dateien) - aber NUR in dieser Nachbarschaft

Der in Abschnitt 37 als "nur ein Beleg, nicht ausreichend verifiziert" dokumentierte Fund
wurde an einer ZWEITEN, unabhängigen Datei bestätigt: `field_sky_01.nif` zeigt exakt
dieselben Werte (`flags=1, function=3`) UND dieselbe Notwendigkeit von genau 4 zusätzlichen
Byte vor der nachfolgenden `NiTriStripsData` wie `skeleton_monolith_blood.nif`.

**VERSUCHT UND VERWORFEN (unbedingte Variante):** Ein erster Test - IMMER 4 Byte extra bei
`NiZBufferProperty` in älteren Versionen, unabhängig vom Nachbarblock - verursachte eine
Regression (-3 Dateien): das zusätzliche Feld ist offenbar NICHT ein generelles Merkmal von
`NiZBufferProperty` in älteren Versionen, sondern spezifisch an die Nachbarschaft zu
`NiTriStripsData`/`NiTriShapeData` gebunden (Ursache weiterhin ungeklärt - nicht in der
autoritativen Referenz für `NiProperty`/`NiObjectNET` zu finden - evtl. eine
Custom-Engine-Eigenheit ähnlich den bereits gefundenen Abweichungen in Abschnitt 33/34).

**Eng begrenzte, funktionierende Variante:** Nur wenn der UNMITTELBAR folgende Block
`NiTriStripsData` oder `NiTriShapeData` ist (analog zum bereits erfolgreichen
`NiPixelData`-Trailer-Muster aus Abschnitt 35/37), werden 4 zusätzliche Byte übersprungen.

**Ergebnis:** Massentest **1976 → 1979/3436 (+3 Dateien, 57.6%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression. Zeigt erneut: bei diesem custom Engine-Fork lohnt sich IMMER die enge
Eingrenzung auf die konkret beobachtete Nachbarschaft, statt eine Änderung pauschal für einen
ganzen Blocktyp/Versionsbereich zu übernehmen.

## 39. `NiAlphaProperty` vor `NiTriStripsData`/`NiTriShapeData` braucht ebenfalls 4 zusätzliche
    Byte - VERSIONSUNABHÄNGIG (+62 Dateien, größter Einzelfund dieser Session!)

Beim Verfolgen eines `NiTriStripsData`-Fehlers an `Leviathan_lightA_non.nif` (Version
20.0.0.4, NICHT älter!) das gleiche Muster wie in Abschnitt 38 gefunden, aber diesmal bei
`NiAlphaProperty` UND unabhängig von der NIF-Version: nach korrekt geparster
`NiAlphaProperty` (`flags=4844, threshold=255` - beide plausibel) fehlen 4 Byte vor einer
direkt folgenden `NiTriStripsData` (`numVerts=13` erst mit den +4 Byte plausibel, davor
`numVerts=0/hasVerts=13`).

**Generalisierte Hilfsfunktion `SkipExtraBytesIfFollowedByTriData` ergänzt** (OHNE
Versions-Bedingung, da hier eindeutig nicht version-abhängig) und bei `NiAlphaProperty`
angewendet: **+62 Dateien allein durch diesen einen Fund** - der größte Einzelfund dieser
gesamten Sitzung.

**VERSUCHT UND VERWORFEN (Verallgemeinerung auf weitere Eigenschaftstypen):** Probeweise
wurde dieselbe Prüfung auch bei `NiVertexColorProperty`, `NiStencilProperty`,
`NiSpecularProperty`, `NiFogProperty` und `NiDitherProperty` ergänzt - das verursachte einen
Netto-RÜCKSCHRITT (2041 → 2033, -8 Dateien): mindestens einer dieser Typen erzeugt falsche
Positive (wird fälschlich als "braucht +4 Byte" erkannt, obwohl an der jeweiligen Stelle
bereits alles korrekt war). Sofort komplett zurückgenommen - NUR `NiAlphaProperty` (und
weiterhin `NiZBufferProperty` mit seinem bestehenden Versions-Gate aus Abschnitt 38) bleiben
aktiv.

**Für eine Folgesession:** jeden der zurückgenommenen fünf Eigenschaftstypen EINZELN (nicht
alle gleichzeitig) testen, um herauszufinden, welche(r) davon tatsächlich sicher von dieser
Korrektur profitieren würde und welche(r) die Regression verursacht.

**Ergebnis:** Massentest **1979 → 2041/3436 (+62 Dateien, 59.4%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression im ausgelieferten Code. Wir haben damit die 59%-Marke überschritten.

## 40. Der offene Faden aus Abschnitt 39 einzeln durchgetestet: 3 von 5 Eigenschaftstypen
    profitieren sicher, einer verursacht Schaden (+16 Dateien netto)

Wie in Abschnitt 39 angekündigt, wurden die fünf zurückgenommenen Eigenschaftstypen EINZELN
(nacheinander, jeweils mit vollem Massentest zwischen jedem Schritt) erneut getestet:

- **`NiVertexColorProperty`**: **+14 Dateien** - sicher, übernommen.
- **`NiStencilProperty`**: **-24 Dateien (!)** - verursacht erheblichen Schaden, SOFORT
  zurückgenommen. Vermutliche Erklärung: `NiStencilProperty` hat bereits eine EIGENE,
  komplexere interne Mehrdeutigkeit (das eingebettete Freitextfeld mit Namen wie
  "21 - Default", siehe frühere Abschnitte dieser Datei) - die zusätzliche
  "+4-Byte-Heuristik" kollidiert offenbar mit dieser bereits vorhandenen Unsicherheit und
  löst häufiger falsch aus, als sie hilft.
- **`NiSpecularProperty`**: **+2 Dateien** - sicher, übernommen.
- **`NiFogProperty`**: **±0 Dateien** (neutral, aber unschädlich) - übernommen für
  zukünftige, im aktuellen Korpus nicht vertretene Dateien.
- **`NiDitherProperty`**: **±0 Dateien** (neutral, aber unschädlich) - übernommen.

**Ergebnis:** Massentest **2041 → 2057/3436 (+16 Dateien netto, 59.9%)**. 7/7 Test-Suiten
weiterhin grün, keine Regression. Bestätigt die Abschnitt-39-Vermutung: die meisten
Eigenschaftstypen profitieren sicher von dieser Korrektur, aber pauschales Verallgemeinern
ohne Einzeltest wäre falsch gewesen (der `NiStencilProperty`-Schaden hätte den `NiVertexColor`-
und `NiSpecular`-Gewinn in der Summe überdeckt, wie bereits in Abschnitt 39 beobachtet).

## 41. `NiMorphData` ergänzt + zwei weitere bestätigte Dead Ends (netto keine neuen Dateien,
    aber vollständige Diagnose)

- **`NiMorphData`** aus der Referenz ergänzt (`num_morphs` + `num_vertices` +
  `relative_targets` + je Morph `frame_name` + `legacy_weight` + `vectors`). Byte-exakt an
  `zzz_kong.nif` verifiziert (`num_morphs=0`, landet exakt auf einem gültigen Namensfeld).
  Betroffene Datei scheitert vermutlich an anderer Stelle weiter - kein Massentest-Zuwachs,
  aber eine echte, korrekte Ergänzung.
- **`LegelDungeon.nif`** (`NiMaterialProperty`-Bucket) als WEITERE Instanz des bereits in
  Abschnitt 20 dokumentierten, bewusst nicht behobenen `NiPixelData`-Trailer-Sonderfalls
  bestätigt (benannte Eigenschaft "14 - Default" nach `NiPixelData` ohne folgende
  `NiSourceTexture`) - keine neue Ursache, nur zusätzliche Bestätigung.
- **`KDVictor.nif`** (`NiTriStripsData`-Bucket): `NiCollisionData` (davor) parst
  nachweislich korrekt (`target=4`, plausible Werte, landet exakt auf dem erwarteten
  Block) - die Fehlausrichtung liegt eindeutig innerhalb von `NiTriStripsData` selbst, ohne
  ein einfaches Vier-Byte-Muster (im Gegensatz zu den Property-Fällen aus Abschnitt 39/40).
  Nicht weiter untersucht - für eine Folgesession.

`NiBoneLODController` (1 Datei) geprüft, aber wegen Komplexität (verschachtelte
`NodeSet`/`SkinInfoSet`-Strukturen) für nur eine betroffene Datei nicht implementiert -
Aufwand/Nutzen-Verhältnis zu ungünstig.

## 42. DURCHBRUCH: das "+4-Byte-Muster" generalisiert - unabhängig vom Vorgänger-Blocktyp
    (+411 Dateien, MIT ABSTAND größter Einzelfund dieser gesamten Sitzung!)

Nach den Funden in Abschnitt 39/40 (NiZBufferProperty, NiAlphaProperty, NiVertexColorProperty,
NiSpecularProperty brauchen +4 Byte vor direkt folgender NiTriStripsData/NiTriShapeData) fiel
bei `Adl_field_hole02.nif` auf: auch **NiFloatData** als Vorgänger braucht exakt dasselbe
Muster (`numVerts=0/hasVerts=15` → `numVerts=15/hasVerts=1` nach +4 Byte). Damit war klar:
das Muster hat NICHTS mit dem spezifischen Vorgänger-Blocktyp zu tun - es tritt bei
IRGENDEINEM Blocktyp auf, der direkt vor einer `NiTriStripsData`/`NiTriShapeData` steht.

**Generalisierte Lösung:** Statt weiter einzelne Vorgänger-Blocktypen aufzuzählen (riskant,
siehe die gemischten Ergebnisse in Abschnitt 40 - manche Typen profitieren, `NiStencilProperty`
schadete), wird die Prüfung jetzt DIREKT am Zielblock durchgeführt: unmittelbar bevor
`NiTriStripsData`/`NiTriShapeData` geparst wird, prüft eine neue Hilfsfunktion
(`LooksLikeTriDataHeader`), ob die aktuelle Position bereits wie ein gültiges
`num_vertices(u16)+keep_flags(u8)+compress_flags(u8)+has_vertices(u8)`-Muster aussieht. Ist
das NICHT der Fall, aber 4 Byte weiter schon, wird dort weitergelesen.

**Warum das so viel sicherer ist als frühere Peek-Heuristiken dieser Sitzung:** Anders als
z.B. der gescheiterte `LooksLikeFreshName`-Versuch (Abschnitt 20) prüft dies MEHRERE
unabhängige, eng eingegrenzte Bedingungen gleichzeitig (Wertebereich UND drei separate
Boolesche Felder), direkt an der Zielstruktur selbst - nicht an einer vagen "sieht nach
einem Namen aus"-Heuristik in einem beliebigen, nicht notwendigerweise verwandten Kontext.

**Verifiziert:** Volle Regression (7/7 Suiten) weiterhin grün - insbesondere blieben ALLE
bereits vorher korrekt geparsten Referenzdateien (inkl. `santuary.nif` mit exakt geprüften
Vertex-/Dreieckszahlen) unverändert korrekt, da die Prüfung nur eingreift, wenn die
UNVERÄNDERTE Position bereits als implausibel erkannt wird. Stichprobenartig geprüft: mehrere
neu erfolgreiche Dateien liefern saubere, in sich konsistente Geometrie (Vertex-Anzahl =
Normalen-Anzahl = UV-Anzahl, sinnvolle Dreieckszahlen).

**Ergebnis:** Massentest **2057 → 2468/3436 (+411 Dateien, 71.8%!)**. 7/7 Test-Suiten
weiterhin grün, keine Regression. Ein einzelner, gut verallgemeinerter Fund hat damit den
größten Sprung dieser gesamten Sitzung gebracht - deutlich über die 70%-Marke hinweg.

## 43. `NiTriShapeData`s "mysteriöses" Feld existiert bei älteren Versionen GAR NICHT (nicht
    nur `additional_data_ref`) - +22 Dateien

Beim Verfolgen des `NiSkinInstance`-Fehlers an `horse2.nif`/`horse3.nif` (Version 10.2.0.0,
per `strings` identifiziert) wurde `NiTriShapeData` erneut vollständig manuell durchgerechnet.
Mit der bestehenden Abschnitt-36-Korrektur (nur `additional_data_ref` entfällt bei älteren
Versionen, das "mysteriöse" u16-Feld bleibt) ergaben sich weiterhin unplausible Werte
(`num_triangles` und die Dreieckspunkt-Anzahl passten nicht zusammen).

**Fund:** Anders als bei `ParseNiTriStripsData` (dort byte-exakt bestätigt: das Feld bleibt
für ALLE Versionen bestehen, siehe Abschnitt 36) entfällt bei `ParseNiTriShapeData` das
"mysteriöse" u16-Feld bei älteren Versionen KOMPLETT - nicht nur `additional_data_ref`.

**Byte-exakt an `horse2.nif` verifiziert:** ohne dieses Feld ergeben sich
`consistency_flags=0x4000` (CT_STATIC, gültiger Enum-Wert), `num_triangles=700` und
`num_triangle_points=2100` - EXAKT `num_triangles*3`, die erwartete Beziehung zwischen
beiden Feldern. Mit dem Feld dagegen durchgehend unplausible Werte.

**Ergebnis:** Massentest **2468 → 2490/3436 (+22 Dateien, 72.5%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression. `horse2.nif`/`horse3.nif` parsen jetzt vollständig erfolgreich.
Interessante Erkenntnis: `NiTriStripsData` und `NiTriShapeData` verhalten sich bei ein und
demselben, scheinbar identischen "mysteriösen" Feld UNTERSCHIEDLICH je nach Version - ein
weiteres Beispiel für die Eigenheiten dieses custom Engine-Forks, bei dem man auch
strukturell sehr ähnliche Blocktypen nicht ungeprüft gleich behandeln darf.

## 44. Ein weiterer `NiTexturingProperty`-Fund: sicherer Rückfallversuch bei tatsächlicher
    CountU32-Überschreitung (netto keine neuen Dateien im aktuellen Korpus, aber wichtige
    Absicherung)

An `adel_terrain_root_town.nif` gefunden: `peek=8` wurde fälschlich als "8 echte Extra-Daten-
Refs" gelesen - alle acht Werte waren zufällig kleiner als unsere Plausibilitätsgrenze
(100000) und bestanden damit die generelle Prüfung aus `ParseObjectNetBase` (Abschnitt 23).
`texture_count` landete dadurch bei absurden ~1,6 Milliarden.

**Unterschied zum verworfenen Versuch aus Abschnitt 30:** dort wurde bei `texture_count==0`
zurückgefallen - ein SCHWACHES Signal, das in einem anderen Fall zufällig ebenfalls zutraf
und eine Regression verursachte. Hier wird stattdessen geprüft, ob `texture_count` die
harte `CountU32`-Obergrenze (64) tatsächlich ÜBERSCHREITET - ein extrem starkes Signal, das
praktisch nie zufällig auftritt (im Gegensatz zu einem einzelnen Grenzwert wie 0).

**Umsetzung:** eine neue `ByteReader::SetOk()`-Methode erlaubt, eine durch `CountU32`
ausgelöste Invalidierung gezielt rückgängig zu machen, nachdem die Position zurückgesetzt
und der alternative Parse-Pfad (kein `num_extra_data_refs`-Feld) versucht wurde.

**Byte-exakt verifiziert:** mit der Alternative ergeben sich `apply_mode=2`,
`texture_count=7` - beide plausibel (7 entspricht sogar exakt dem Referenz-Standardwert).
`adel_terrain_root_town.nif` kommt dadurch von Block 7 bis Block 37 voran (scheitert dort an
einer unabhängigen Stelle weiter).

**Ergebnis:** Massentest bleibt bei 2490/3436 (im aktuellen Korpus offenbar kein weiterer
Treffer für exakt dieses Muster, oder betroffene Dateien scheitern weiterhin an anderer
Stelle) - aber eine echte, bewiesene Korrektur mit striktem Sicherheitsnachweis (kein
Rückschritt bei voller Regression). 7/7 Test-Suiten weiterhin grün.

## 45. DURCHBRUCH: vollständige ObjectNetBase-Validierung erkennt benannte Objekte sicher
    (+128 Dateien - zweitgrößter Einzelfund dieser Session!)

An `Tree01.nif` gefunden: nach `NiPixelData` folgt direkt ein BENANNTES `NiNode` (Name der
Länge 9) - der bestehende 4-Byte-Trailer-Peek erkennt nur LEERE Namen (`namelen=0`) und
scheiterte deshalb. Dies ist dieselbe Grundproblematik wie der in Abschnitt 20 dokumentierte,
katastrophal gescheiterte Versuch (`LooksLikeFreshName()` verursachte einen Rückschritt von
1818 auf 363).

**Der entscheidende Unterschied zu Abschnitt 20:** Der gescheiterte Versuch prüfte NUR das
Namensfeld selbst (Länge + druckbare Zeichen) - direkt nach rohen Pixeldaten sehen zufällig
sehr oft kurze "Namen" plausibel aus. Die neue Prüfung validiert dagegen die GESAMTE
`ObjectNetBase`-Struktur als Kette voneinander abhängiger Bedingungen: ein plausibler Name
(Länge 1-40, ausschließlich druckbare Zeichen) MUSS von einem plausiblen `numExtra` (0-10)
GEFOLGT von einem plausiblen `controller` (-1 oder 0-300) gefolgt sein. Diese Kombination
mehrerer unabhängiger, aufeinanderfolgender Bedingungen ist um Größenordnungen seltener
zufällig erfüllt als eine bloße Namensform allein.

**Byte-exakt an `Tree01.nif` verifiziert:** Name der Länge 9, `numExtra=0`, `controller=-1`,
anschließend `flags=16` und plausible Weltkoordinaten (88.3, 0.68, 32.9) für das folgende
`NiNode`. Stichprobenartig geprüft: die Datei liefert danach vollständig saubere, in sich
konsistente Geometrie (4 Teile, Vertex-/Normalen-/UV-Anzahlen stimmen jeweils überein).

**Ergebnis:** Massentest **2490 → 2618/3436 (+128 Dateien, 76.2%!)** - der zweitgrößte
Einzelfund dieser gesamten Sitzung nach Abschnitt 42. 7/7 Test-Suiten weiterhin grün, keine
Regression. Auch der in Abschnitt 20 dokumentierte `BeraM_Wood3.nif`-Fall profitiert
teilweise: die Datei kommt jetzt bis Block 214 (vorher Block 9), auch wenn sie letztlich noch
an einer anderen, unabhängigen Stelle scheitert.

**Übergreifende Lehre:** Bestätigt und verschärft die bereits in Abschnitt 20/25/30
dokumentierte Erkenntnis: eine einzelne, isolierte Plausibilitätsprüfung (nur das Namensfeld)
ist im Kontext direkt nach rohen Binärdaten unzuverlässig - aber eine KETTE mehrerer
unabhängiger, aufeinander aufbauender Bedingungen (Name UND numExtra UND controller,
alle gleichzeitig plausibel) kann denselben Kontext sicher auflösen. Der Unterschied liegt
nicht in der Art der Prüfung, sondern in ihrer strukturellen Tiefe.

## 46. Neue Sitzung: eigenes `mass_test`-Tool gebaut, sechs Buckets systematisch durchleuchtet -
    DREI weitere bestätigte Dead Ends (kein Massentest-Zuwachs, aber vollständige Diagnose)
    + wichtiger struktureller Befund zur Fehlerverteilung + `key_type=0` weiter eingegrenzt

**Werkzeug:** Da kein dediziertes Massentest-Tool im Repo lag, wurde eines gebaut
(`mass_test.cpp`, walkt ein Verzeichnis, ruft `LoadNifMesh` auf, bucketed Fehler nach
`Block N (Typ)` per Regex, zeigt zusätzlich eine nach Typ AGGREGIERTE Verteilung über alle
Block-Indizes hinweg - deutlich aussagekräftiger als die reine Block-Index-Sicht). Reproduziert
den gemeldeten Stand exakt (2618/3437, 76.2%, identische Top-Buckets). Für Debug-Sessions
wurde zusätzlich eine INSTRUMENTIERTE Kopie von `NifModel.cpp` unter `/home/claude/work/debug/`
gepflegt (nie der echte Quellcode) - mit Umgebungsvariable `NIF_TRACE_FILE` für
Block-für-Block-Traces einzelner Dateien. Empfehlung für Folgesessions: dieses Tool (oder ein
Äquivalent) gleich zu Beginn mitbringen/wiederverwenden, spart erheblich Zeit.

**Aggregierte Fehlerverteilung nach Typ** (aufschlussreicher als Block-Index-Buckets):
`202x NiTriStripsData, 97x NiCollisionData, 94x NiTriStrips, 82x NiNode, 54x NiTriShapeData,
41x NiMaterialProperty, 30x NiBillboardNode, 29x NiPosData, 21x NiPSysSpawnModifier, ...`
(vollständige Liste im Sitzungsverlauf, nicht hier dupliziert).

### Struktureller Befund: die großen Buckets sind fast vollständig Kaskaden des off-limits Codes

Vier der fünf größten Buckets wurden auf ihren unmittelbaren Vorgänger-Blocktyp hin
untersucht (Stichproben + z.T. vollständige Header-Analyse aller 819 Fehlschläge):

- **NiCollisionData (97):** 85/97 haben `NiTriStrips`/`NiTriShape` als direkten Vorgänger
  (also `ParseNiTriStripsHeader`-Kaskade, off-limits). Die restlichen 12 (Vorgänger
  `NiStencilProperty`/`NiStringExtraData`) → siehe Dead-End-Fund unten.
- **NiBillboardNode (30):** 4/4 Stichproben (`H_streetlamp-ur.nif`, `Legel_CrossBowCart.nif`,
  `LightRay.nif`, `AlDn01_coco.nif`) haben `NiTriStrips` als direkten Vorgänger, jeweils im
  Muster "billboard grass/leaf": `NiTriStrips` OHNE unmittelbar folgende eigene
  `NiTriStripsData` (Geometrie referenziert vermutlich extern/gemeinsam) - `ParseNiTriStripsHeader`
  scheint genau diesen Fall falsch zu behandeln.
- **NiNode (82):** 4/4 Stichproben ebenfalls mit `NiTriStrips`/`NiTriStripsData` als direktem
  Vorgänger.
- Eine vollständige Auswertung ALLER 819 Fehlschläge (nicht nur Stichproben) nach direktem
  Vorgänger-Blocktyp ergab: `NiCollisionData` (177x), `NiTriStrips` (104x), `NiPixelData`
  (69x), `NiStringExtraData` (52x), `NiIntegerExtraData` (42x), `NiTriStripsData` (41x),
  `NiNode` (34x), ... - **NUR 19.8% haben NiTriStrips/NiTriShape/*Data als UNMITTELBAREN
  Vorgänger**, aber da NiCollisionData selbst überwiegend eine `NiTriStrips`-Kaskade ist
  (s.o.), ist die tatsächliche indirekte Kaskaden-Quote deutlich höher. Eine
  Mehrfach-Hop-Rückverfolgung (statt nur des direkten Vorgängers) wäre für eine Folgesession
  aufschlussreicher, war in dieser Sitzung aber nicht mehr im Zeitbudget.

**Schlussfolgerung:** `ParseNiTriStripsHeader` (weiterhin off-limits, siehe Abschnitt 28/34)
ist mit hoher Wahrscheinlichkeit der dominante Einzelblocker für einen GROSSEN Teil der
verbleibenden 819 Dateien - nicht nur für die Dateien, die direkt als `NiTriStripsData`
scheitern. Buckets wie `NiCollisionData`, `NiBillboardNode`, `NiNode` sind größtenteils reine
Symptom-Buckets ohne eigenen Fixbedarf.

### Dead End 1: `NiStencilProperty`s letztes Feld ist NICHT zuverlässig per Druckbarkeits-Peek
    von "kein String" zu unterscheiden

An 4 unabhängigen Dateien (`horse1.nif`, `Urg_lefte_w_snow_blend.nif`, `ElderinGround.nif`
[UrgFire01], `swa_leaf.nif`) byte-exakt verifiziert: nach den 7 bekannten u32-Feldern + 1 Byte
folgt manchmal EIN reines 4-Byte-Skalarfeld (Block endet sofort danach), manchmal ein
SizedString mit echtem Text (wie im ursprünglichen `Rou_M_Tube.nif`-Beleg, "21 - Default").
Per `LooksLikeFreshName`-Peek (druckbares ASCII als Diskriminator) unterschieden - Fix schien
an allen 5 Belegdateien exakt zu passen.

**Vollständiger Massentest widerlegte das:** `2618 → 2606/3437 (netto -12: +4 gelöst
[horse1.nif-Kopien], aber +16 neue Regressionen)`. Ursache am Beispiel `Malontent1.nif`
(vorher erfolgreich) verifiziert: exakt derselbe 7-Felder-Satz `[1024,0,0xFFFFFF00,255,0,768,
768]` + Flag=0, aber das anschließende String-Feld hat ebenfalls NICHT-druckbaren Inhalt
(`b'\xb1\xe4\xc5\xd9\xc6\xae3'`) - UND trotzdem ist hier die UNBEDINGTE
SizedString-Interpretation (Original-Verhalten) die korrekte. Druckbarkeit des Inhalts ist
also kein verlässlicher Diskriminator - offenbar können sowohl "echte" Namen als auch
Nicht-Namen non-ASCII-Bytes enthalten. **Kein sauberer Diskriminator ohne weitere,
bislang unbekannte Information gefunden - NICHT ERNEUT VERSUCHEN ohne neue Evidenz.**

### Dead End 2: `NiPSysEmitter`s "Unknown QQSpeed Floats" (autoritative nif.xml) existieren in
    diesem Fork NICHT

Die autoritative `nif.xml` (niftools/nifxml) listet für `NiPSysEmitter` (Basis aller
Emitter-Typen: `NiPSysBoxEmitter`, `NiPSysMeshEmitter`, ...) ein unversioniertes,
zusätzliches Feld "Unknown QQSpeed Floats" (2 Floats, 8 Byte) NACH den bekannten
6+4 Floats/Color4. Hypothese: dieses fehlende Feld verursacht die
`NiPSysSpawnModifier`-Fehlschläge (Vorgänger häufig `NiPSysBoxEmitter`/`NiPSysMeshEmitter`).

Einzeldatei-Analyse (`FirePath.nif`) war uneindeutig (langer Zero-Padding-Lauf, ungewöhnliches
Float-Array direkt danach - beide Hypothesen ergaben implausible Folgewerte). **Vollständiger
Massentest klärte es eindeutig:** `2618 → 2594/3437 (netto -24)`. Das Feld existiert in diesem
Fork nicht (oder nicht an dieser Stelle) - reiht sich ein in die bereits bekannten
Abweichungen von der Vanilla-Spezifikation (`NiPixelData`-Kopf, Abschnitt 33;
`ParseNiTriStripsHeader`, Abschnitt 28/34). **NICHT ERNEUT VERSUCHEN.**

**Übergreifende Lehre dieser Sitzung:** ZWEI verschiedene, jeweils an mehreren Dateien
byte-exakt aussehende Fixes wurden durch den vollen Massentest widerlegt. Bestätigt erneut
(wie in Abschnitt 14/20/28/34 bereits mehrfach gelernt): Einzeldatei-Verifikation, auch an
mehreren Belegen, ist NIEMALS ausreichend - nur der volle Massentest zeigt die Wahrheit. Bei
kleinen Buckets (<30 Dateien) lohnt sich ein Fix nur dann, wenn er nachweislich NULL
Interaktion mit bereits korrekt geparsten Dateien hat (schwer zu garantieren bei geteilten
Basisfunktionen wie `ParseObjectNetBase` oder `NiPSysEmitterBase`, die von vielen Blocktypen
genutzt werden).

### Dead End 3: kein universeller "2-Byte-Lücke"-Fix für `NiPSysModifierBase`/`NiPSysEmitterBase`

An 3 identischen "Leviathan"-Partikeleffekt-Dateien (`Leviathan_chandelierA/B.nif`,
`Leviathan_deco3.nif`, alle mit identischem `NiPSysBoxEmitter`-Inhalt) sah es byte-exakt so
aus, als fehlten zwischen `order` und `target_ref` in `NiPSysModifierBase` 2 Byte (danach
landen `target_ref=-1` und `active=1` exakt plausibel, und eine lange Folge von `1.0`-Floats
beginnt exakt an der erwarteten Position). ZWEI Varianten getestet - beide per vollem
Massentest widerlegt:
- 2 Byte direkt in `NiPSysModifierBase` (betrifft ALLE Partikel-Modifier/Emitter-Typen):
  `2618 → 2594 (netto -24)`.
- 2 Byte nur in `NiPSysEmitterBase` (betrifft nur Emitter-Typen, nicht Modifier): `2618 → 2590
  (netto -28)`.

Beide unconditional - d.h. für die überwältigende Mehrheit der bereits korrekt geparsten
Partikel-Dateien ist die AKTUELLE (ohne die 2 Byte) Struktur korrekt; die "Leviathan"-Dateien
brauchen offenbar etwas anderes (eine echte Bedingung, kein pauschaler Fix). **NICHT ERNEUT
VERSUCHEN ohne einen erkennbaren, VOR dem Lesen prüfbaren Unterschied zwischen den
betroffenen und unbetroffenen Instanzen.**

### Offener, aber weiter eingegrenzter Punkt: `key_type=0` bei `KeyGroup` (NiFloatData,
    NiBoolData, NiPosData - typübergreifend, nicht nur NiPosData wie bisher notiert)

Die autoritative `nif.xml` definiert `KeyType` nur für Werte 1-5 (LINEAR/QUADRATIC/TBC/
XYZ_ROTATION/CONST) - Wert 0 ist offiziell undefiniert. Bisher nur für `NiPosData` (~28
Dateien) notiert; diese Sitzung bestätigt, dass **derselbe `key_type=0` auch bei
`NiFloatData` und `NiBoolData` auftritt** (unabhängige Fundstellen: `portal_aura.nif` und
`Yak_desk.nif`, BEIDE mit `num_keys=2, key_type=0` - exakt derselbe Wert, kein Zufall). Die
NifSkope-Quelle (github.com/niftools/nifskope) wurde geprüft, bringt aber keine
zusätzliche Erkenntnis, da NifSkope selbst rein XML-getrieben ist (dieselbe `nif.xml`, keine
fest codierte Sonderbehandlung für Wert 0).

Mehrere Byte-Interpretationen für die auf `key_type=0` folgenden Daten wurden per
Brute-Force-Suche nach der nächsten plausiblen Blockgrenze geprüft (unterschiedliche
Floats-pro-Key-Annahmen: 1, 2, 4, 5, 10) - keine ergab eine eindeutig überzeugende, in sich
konsistente Zeitfolge (Keyframe-Zeiten sollten monoton steigen, taten es in keiner getesteten
Variante sauber). Eine Kandidatenposition (10 Floats/Key, 88 Byte Gesamtlänge) landet
zumindest auf einem sehr plausiblen `ObjectNetBase`-Anfang (`peek0=0xFFFFFFFF`,
`controller=-1`) - aber OHNE Massentest-Verifikation NICHT vertrauenswürdig genug zum
Übernehmen (siehe Dead Ends 1-3 oben - genau diese Art von Einzeldatei-Plausibilität hat
sich wiederholt als trügerisch erwiesen).

**Für eine Folgesession:** `num_keys=2` scheint das charakteristische Merkmal zu sein (nicht
zufällig, in allen bisher gesehenen `key_type=0`-Fällen identisch) - eventuell ein
fork-spezifisches "2-Punkt-Pulsieren" ohne volle Zeit-/Tangenten-Daten. Lohnt sich, gezielt
NACH weiteren `key_type=0`-Instanzen zu suchen (auch in den ~28 bereits bekannten
`NiPosData`-Fällen) und deren `num_keys`-Werte zu vergleichen, bevor eine Byte-Struktur
geraten wird.

### Nachtrag zu `key_type=0`: systematische Auswertung aller 42 betroffenen Dateien - wichtige
    Korrektur + neue, aber weiterhin unbestätigte Hypothese

Alle 42 im Massentest als `NiPosData`/`NiFloatData`/`NiBoolData`/`NiColorData` fehlschlagenden
Dateien einzeln decodiert (12 strukturell unterschiedliche, Rest Duplikate v.a. von
`EnvSet.nif`). Wichtige Korrektur der vorherigen Vermutung:

- **NUR `NiPosData` und `NiFloatData` zeigen sauber `key_type=0`** (6/6 Dateien, ALLE mit
  exakt `num_keys=2`: `EnvSet.nif`, `portal_aura.nif`, `Water.nif`,
  `Yak_ElectronicGenerator.nif`, `Yak_desk.nif`, `DarkVallyEffect.nif`).
- **`NiBoolData`/`NiColorData` zeigen dagegen KOMPLETT ANDERE, eindeutig falsch
  ausgerichtete Werte** (`num_keys` liest z.B. `1065353216` = Bitmuster von `1.0f`,
  `4294967295` = `0xFFFFFFFF`) - das ist eine VÖLLIG ANDERE, unabhängige Ursache (vermutlich
  wieder eine Partikelsystem-/`NiTriStrips`-Kaskade, siehe `Cypian_Bridge.nif`s
  `NiPSysUpdateCtlr`-Vorgängerkette weiter oben), NICHT `key_type=0`. Sollte separat
  untersucht werden, nicht zusammen mit dem echten `key_type=0`-Phänomen.

**Tiefergehende Byte-Analyse an `EnvSet.nif`** (Block 11, `NiPosData`, `valueFloats=3`,
gefolgt von `NiTriStripsData` in Block 12): die 20 Floats nach `num_keys=2, key_type=0`
zeigen ein auffälliges Muster - interpretiert als 2× 10 Floats (`value(3) + tangent_a(3) +
tangent_b(3) + time(1)`, ZEIT AM ENDE statt am Anfang): Key1 = Wert(0.592, 0.843, 0.706),
Tangenten((0,0,0), (-0.455,-0.439,-0.086)), Zeit=2.25; Key2 = Wert(0.137, 0.404, 0.620),
Tangenten((-0.455,-0.439,-0.086), (0.294,0.630,0.133)), Zeit=2.5. Bemerkenswert: Key2s erste
Tangente ist IDENTISCH zu Key1s zweiter Tangente (plausibel für eine geteilte
Zwischen-Tangente), und die Zeiten steigen sauber (2.25 → 2.5). ABER: die daraus berechnete
Blockgrenze (20 Floats = 80 Byte nach dem 8-Byte-Header) landet NICHT auf einer per
`LooksLikeTriDataHeader` (siehe Abschnitt 42) plausiblen `NiTriStripsData`-Kopfposition
(errechnete `num_vertices` dort: 54486, weit über der Plausibilitätsgrenze). Auch ein
Fenster von ±20 Byte um diese Position bringt keinen Treffer. **Trotz der auf den ersten
Blick überzeugenden Zahlenmuster (steigende Zeiten, geteilte Tangente) bleibt die Hypothese
UNBESTÄTIGT - genau die Art Befund, die diese Sitzung bereits zweimal (NiStencilProperty,
NiPSysModifierBase) beim vollen Massentest widerlegt hat. NICHT ohne Massentest-Verifikation
übernehmen, und angesichts der Historie selbst dann nur mit größter Vorsicht.**

**Fazit:** `key_type=0` bleibt ein CHARAKTERISIERTER, aber UNGELÖSTER Punkt - klar
eingegrenzt auf `NiPosData`/`NiFloatData` mit `num_keys=2`, mit einer plausibel aussehenden,
aber nicht verifizierten 10-Floats-pro-Key-Hypothese. Eine Folgesession könnte versuchen: (a)
weitere `key_type=0`-Instanzen außerhalb des aktuellen Fehlschlags-Korpus zu finden (z.B. in
bereits ERFOLGREICH geparsten Dateien, falls `key_type=0` dort mit `num_keys=0` harmlos
vorkommt - würde die Struktur nicht klären, aber die Häufigkeit), oder (b) eine der
"EnvSet.nif"-Kopien direkt in NifSkope/PyFFI zu laden (falls verfügbar) und den tatsächlich
interpretierten Wert zu vergleichen - dieser Fork weicht aber nachweislich mehrfach von der
Vanilla-Spezifikation ab, also auch das nur ein Ausgangspunkt, kein Beweis.

**Ergebnis:** Massentest bleibt bei **2618/3436 (76.2%)** - kein Zuwachs, aber DREI
Sackgassen vollständig dokumentiert, ein vierter Punkt (key_type=0) deutlich enger
eingegrenzt als zuvor (inkl. einer plausiblen, aber unverifizierten Struktur-Hypothese für
eine Folgesession), und der strukturelle Zusammenhang zwischen den großen verbleibenden
Buckets und `ParseNiTriStripsHeader` erstmals systematisch belegt (statt nur vermutet). 7/7
Test-Suiten weiterhin grün, echter Quellcode in dieser Sitzung NICHT verändert (alle
Experimente liefen gegen eine isolierte Debug-Kopie).

## 47. STRATEGIEWECHSEL nach mehreren Dead Ends: `NiLookAtInterpolator` komplett neu
    implementiert (+4 Dateien) - erster echter Massentest-Zuwachs dieser Sitzung

Nach den drei widerlegten "Byte-Lücken-Rate"-Versuchen (Abschnitt 46) Strategie gewechselt:
statt weiter an bestehenden, bereits größtenteils funktionierenden Funktionen zu raten
(hohes Regressionsrisiko, siehe die drei Dead Ends), gezielt nach Dateien gesucht, die an
einem KOMPLETT UNIMPLEMENTIERTEN Blocktyp scheitern (explizite "Nicht unterstützter
Block-Typ"-Fehlermeldung, kein Alignment-Problem) - ein additiver neuer Codepfad hat per
Definition keine Interaktion mit bereits funktionierendem Code, also strukturell viel
risikoärmer.

**Gefunden:** `NiLookAtInterpolator` (4 identische Kopien von `H_AIRDOLL.nif`) war schlicht
nicht implementiert. Struktur aus der autoritativen `nif.xml` übernommen: `flags(u16)` +
`look_at_ref(i32, Ptr auf NiNode)` + `look_at_name(SizedString)` + `NiQuatTransform`
(`translation(Vector3=12)` + `rotation(Quaternion=16)` + `scale(float=4)` = 32 Byte - das
"TRS Valid"-Feld entfällt seit Version 10.1.0.109, betrifft unsere 20.0.0.4 also nicht) + 3
weitere Interpolator-Refs (Translation/Roll/Scale, je i32). `NiInterpolator`/`NiObject`
selbst haben keine eigenen Felder (wie bei den bereits vorhandenen
NiFloatInterpolator/NiPoint3Interpolator/NiBoolInterpolator auch).

**Byte-exakt an `H_AIRDOLL.nif` Block 144 verifiziert - ungewöhnlich viele unabhängige
Bestätigungssignale gleichzeitig:** `flags=4` (plausibler kleiner Wert), `look_at_ref=145`
zeigt EXAKT auf das nächste `NiNode` (semantisch sinnvoll: das Objekt "schaut" auf einen
nahen Knoten), die Rotation ist ein exakter Einheits-Quaternion (Betrag ≈1.0), Translation
UND Scale sind beide `-FLT_MAX` (`-3.4028235e+38`, ein bekannter NIF-Sentinelwert für "nicht
gesetzt"), alle 3 Interpolator-Refs sauber `-1` (kein Interpolator zugewiesen) - und die
berechnete Blocklänge landet exakt auf dem lesbaren Namensfeld `"Camera01.Target"` des
folgenden `NiNode` (ein plausibler Bone-/Dummy-Name für ein Kamera-Ziel).

**Verifiziert:** volle Regression (7/7 Suiten + voller Massentest) VOR Übernahme in den
echten Code geprüft: `2618 → 2622/3436 (+4, netto exakt +4, keine Regression)`. Anders als
die drei Dead Ends dieser Sitzung sofort im echten Quellcode übernommen (nicht nur in der
Debug-Kopie), da additive neue Blocktypen strukturell nicht dasselbe Interaktionsrisiko mit
bereits funktionierendem Code haben wie Änderungen an geteilten Basisfunktionen.

**Übergreifende Lehre:** nach mehreren gescheiterten "bestehende Funktion nachbessern"-
Versuchen war der Wechsel zu "fehlende Funktion ergänzen" die bessere Zeitinvestition - beide
Fehlerarten sehen im Massentest ähnlich aus ("Unerwartetes Dateiende"), aber nur bei echten
NEUEN, unimplementierten Typen (erkennbar an der expliziten "Nicht unterstützter
Block-Typ"-Meldung statt der generischen EOF-Meldung) ist das Regressionsrisiko strukturell
niedrig. Für Folgesessions: IMMER zuerst die `(kein Block-Muster) Nicht unterstützter
Block-Typ '...'`-Zeilen der `mass_test`-Ausgabe prüfen, bevor an bestehenden Funktionen
herumgerätselt wird - dort ist echter Fortschritt am günstigsten zu holen.

**Verbleibende explizit unterstützte-aber-fehlende Typen (nach diesem Fix):**
`NiBoneLODController` (1 Datei, `KDAlice_Slime.nif` - bereits als zu komplex für 1 Datei
verworfen, siehe Abschnitt 41) und ein einzelner Fall mit `NiNode` als Eigenschafts-Referenz
(`KDFargels_circle01.nif` - vermutlich eine echte Datenanomalie oder Kaskade, nicht
weiterverfolgt). Keine weiteren komplett fehlenden Blocktypen mehr im aktuellen
815-Dateien-Fehlschlagskorpus.

**Ergebnis:** Massentest **2618 → 2622/3436 (76.3%)**. 7/7 Test-Suiten weiterhin grün. Erster
echter Fortschritt dieser Sitzung nach vier reinen Diagnose-/Dead-End-Funden.

## 48. Zwei weitere echte Funde: zu strikte Mesh-Geometrie-Prüfung entschärft (+3) und
    Version-10.1.0.0-Header-Feld gefunden (+0, aber verifizierter echter Fortschritt)

**Fund A - `beraBN.nif`/`bera_BN01.nif`/`bera_BNset.nif` (+3 Dateien):** diese Dateien
enthalten NACHWEISLICH (komplette Block-Typ-Liste geprüft) AUSSCHLIESSLICH `NiNode`/
`NiCollisionData`/Properties - KEIN einziges `NiTriStrips`/`NiTriShape`/`*Data`. Das sind
legitime, rein unsichtbare Kollisions-/Ankerpunkt-Objekte (vermutlich "Bounding Node" -
Navigations- oder Trigger-Volumen ohne sichtbare Geometrie), keine kaputten Dateien. Der
bisherige harte Fehler `"Keine Mesh-Geometrie in dieser Datei gefunden"` bei leerem
`model.parts` war zu strikt. `NifMeshRenderer` iteriert bereits sicher über eine leere
`parts`-Liste (`for (const auto& part : nifResult->parts)`, kein Sonderfall nötig) - die
Prüfung wurde ersatzlos entfernt, keine andere Codestelle setzt einen nicht-leeren
`model.parts` voraus (per Grep verifiziert).

Massentest **2622 → 2625/3436 (+3, 76.4%)**. 7/7 Suiten grün, keine Regression.

**Fund B - Version 10.1.0.0 hat ein zusätzliches, unversioniertes 4-Byte-Header-Feld
(Wert immer 0) direkt nach den Header-`Groups`, VOR Block 0** (7 betroffene Dateien:
`EnvSet.nif`, `RouTempDn01_ground.nif`, `TreeThin2.nif`, `thornOnly.nif`, `thornOnlytop.nif`,
`treeThin.nif`, `MapLinkGate.nif` - ALLE scheiterten bisher direkt bei Block 0). Byte-exakt
an ALLEN 7 Dateien verifiziert: ohne das Feld beginnt Block 0 mit sinnlosem Datenmüll, mit
ihm landet man exakt auf einem gültigen, lesbaren Namensfeld `"Scene Root"` (der bei
3D-Exportern übliche Standardname für den Wurzelknoten) - bei JEDER der 7 Dateien identisch.
Version 10.2.0.0 hat dieses Feld NICHT (nur 10.1.0.0), im Code entsprechend eng
versionsgegated (`hdr.version == 0x0a010000u`), betrifft also nur diese 7 seltenen Dateien.

**Wichtig - kein Massentest-Zuwachs, aber verifizierter echter Fortschritt:** alle 7 Dateien
kommen jetzt deutlich weiter (von Block 0 auf Block 3/4), scheitern dort aber an weiteren,
noch ungelösten Strukturunterschieden dieser seltenen Alt-Version (3 an `NiTriStrips`
selbst - off-limits; die anderen an `NiNode` mit ähnlichem Muster wie unten in Abschnitt 48
Fund C beschrieben). Passend zur bereits in Abschnitt 15 dokumentierten Einschätzung: die
10.1.0.0/10.2.0.0-Ära braucht eine eigene, größere Untersuchung mehrerer Blocktypen. Dieser
Fund ist ein bestätigter Baustein dafür, kein Endergebnis.

Massentest bleibt bei 2625/3436 (kein direkter Zuwachs), 7/7 Suiten weiterhin grün, keine
Regression (per vollständigem Fail-Listen-Diff bestätigt: exakt dieselben Dateien scheitern
wie vorher, nur mit späterem, weiter fortgeschrittenem Block-Index).

**Fund C (UNTERSUCHT, aber NICHT GELÖST) - `NiMeshPSysData`→`NiNode`-Kette (5 Dateien:
`SD_Vale01_machine01_1.nif`, `KDFargels_Bijou01.nif`, `BerFrz01_DeathEye_EF.nif`,
`BerFrz01_wood00.nif`, `IceWater.nif`):** alle 5 zeigen denselben Vorgänger-Kette
`NiMeshPSysData→NiBoolData→NiBoolInterpolator→NiFloatInterpolator` vor dem scheiternden
`NiNode`. An `BerFrz01_wood00.nif` untersucht: `NiMeshPSysData`s eigene, bereits in Abschnitt
19 dokumentierte 17-Byte-Trailer-Länge rechnet sich intern konsistent (Blockende landet exakt
auf einem NAMENSFELD-Anfang des folgenden `NiNode`), aber die anschließenden `NiNode`-Felder
zeigen ab dem zweiten Rotationsmatrix-Float garbage-artige, sich wiederholende Denormal-Werte
(`2.2779507836064226e-41` - DERSELBE Bitmuster-"Fingerabdruck", der auch in der
`NiPSysBoxEmitter`-Untersuchung dieser Sitzung auftauchte, siehe Abschnitt 46 Dead End 3).
±4-Byte-Verschiebung der `NiMeshPSysData`-Trailerlänge getestet (13/17/21 Byte) - das
Garbage-Muster verschiebt sich nur mit, löst sich aber bei KEINER Variante auf. Das deutet
auf eine tiefer liegende Fehlausrichtung hin (vermutlich innerhalb von `NiMeshPSysData`s
eigenen "nur grob dokumentierten" Zusatzfeldern selbst, nicht nur der abschließenden
Trailerlänge) - für eine Folgesession mit mehr Zeitbudget, NICHT auf Basis der aktuellen
Evidenz spekulativ fixen.

**Ergebnis dieses Abschnitts:** Massentest **2622 → 2625/3436 (76.4%)**. 7/7 Test-Suiten
weiterhin grün, keine Regression. Alle drei Funde (A/B/C) direkt in der echten Codebasis
verifiziert (A/B übernommen, C dokumentiert aber nicht implementiert, da unbestätigt).

## 49. ⭐ WICHTIGSTER OFFENER PUNKT für die nächste Sitzung: dieselbe "1.0-Float-Array"-
    Signatur taucht in VIER völlig unabhängigen Kontexten auf - eindeutig kein
    Kaskaden-Zufall, sondern vermutlich ein fehlendes, noch nicht identifiziertes Datenfeld

Bei der Untersuchung mehrerer, strukturell komplett unterschiedlicher Cluster fiel eine exakt
IDENTISCHE Byte-Signatur wiederholt auf: eine Folge von Nullbytes, dann `FF FF FF FF`
(klassischer `-1`-Sentinel), dann eine LANGE, sich wiederholende Folge von `1.0f`-Werten
(`00 00 80 3F` wiederholt viele Male). Beobachtet in VIER unabhängigen Kontexten:

1. `NiPSysBoxEmitter` (Leviathan-Cluster, Abschnitt 46 Dead End 3 - `target=-1, active=1`
   gefolgt von endlosen `1.0f`)
2. `NiMeshPSysData`→`NiNode`-Kette (Abschnitt 48 Fund C - Rotationsmatrix ab Float 2 nur noch
   Denormale/Garbage in diesem Muster)
3. `NiPSysBoundUpdateModifier`→`NiNode`(sauber, alles 0)→`NiNode`(scheitert) - an
   `store.nif` verifiziert: die VORANGEHENDE `NiNode`-Instanz rechnet sich byte-exakt korrekt
   bis zur Blockgrenze (alle Felder 0 - ein leerer, aber valider Platzhalter-Knoten), die
   FOLGENDE `NiNode`-Instanz beginnt exakt mit derselben Signatur (`peek0=0x80000000`,
   dann `FFFFFFFF`, dann endlose `1.0f`).
4. `NiFloatInterpolator`/`NiFloatData`-Doppelkette→`NiSourceTexture` (`KDUnHall_Effect_Blue.
   nif`, Block 56) - beide vorangehenden Interpolator/Data-Paare rechnen sich exakt (8 Byte
   bzw. 4 Byte bei `num_keys=0`), aber `NiSourceTexture`s `filename_len` liest danach
   dieselbe Signatur (impliziter riesiger Wert, dann `FFFFFFFF`, dann `1.0f`-Serie).

**Warum das wichtig ist:** in JEDEM der vier Fälle wurde die VORANGEHENDE Blockgrenze
unabhängig byte-exakt verifiziert (nicht nur angenommen) - das schließt einfache
Kaskaden-Fehlausrichtung als Ursache aus. Die Konsistenz der Signatur über vier strukturell
komplett verschiedene Blocktyp-Übergänge hinweg spricht stark dafür, dass hier ein
EIGENSTÄNDIGES, bisher nicht identifiziertes Datenfeld oder Array existiert (vermutlich ein
Gewichts-/Skalierungs-Array, das per Default mit `1.0` gefüllt wird - typisch für Bone-
Weights, Blend-Shape-Gewichte, oder ein ähnliches "einheitliches" Array), das der aktuelle
Parser an mehreren, unterschiedlichen Stellen systematisch NICHT überspringt.

**Für eine Folgesession:** dies ist der aussichtsreichste offene Ansatzpunkt - deutlich
vielversprechender als die vier bereits einzeln versuchten (und gescheiterten) lokalen
Byte-Patches dieser Sitzung, weil die Konsistenz über so unterschiedliche Kontexte hinweg auf
eine EINZIGE, gemeinsame Ursache hindeutet statt vier unabhängige Zufälle. Empfohlener
nächster Schritt: die exakte LÄNGE dieser wiederholten `1.0f`-Sequenz in mehreren Instanzen
präzise vermessen (endet sie an einer erkennbaren Grenze? Skaliert die Länge mit einem
bereits gelesenen Zähler - z.B. `numVerts`, `numBones`, einer Partikelanzahl?) - das war in
dieser Sitzung aus Zeitgründen nicht mehr möglich.

**Ergebnis:** kein Massentest-Zuwachs aus diesem Abschnitt (rein diagnostisch), aber ein
klar priorisierter, gut belegter Ansatzpunkt für die nächste Sitzung. Massentest bleibt bei
**2625/3436 (76.4%)**.

## 50. Nachtrag zu Abschnitt 49: exakte Byte-Struktur der "1.0f-Array"-Signatur vollständig
    vermessen (weiterhin UNGELÖST, aber jetzt präzise charakterisiert statt nur beobachtet)

Die "endlose 1.0f-Sequenz" aus Abschnitt 49 wurde an allen vier Belegstellen exakt vermessen
(Fundstellen-Suche nach dem 4-Byte-Muster `00 00 80 3F`, Lückenanalyse zwischen den Treffern):

```
store.nif (NiNode 43):              31 Floats + [00 00 01] + 31 Floats
KDUnHall_Effect_Blue.nif (Block 56): 30 Floats + [00 00 01] + 30 Floats
Leviathan_chandelierA.nif (Block 62): 30 Floats + [00 00 01] + 30 Floats
BerFrz01_wood00.nif (Block 128):      5 Floats + [00 00 01] +  5 Floats
```

**Die Struktur ist bei allen vier Belegen identisch aufgebaut:** ein `0xFFFFFFFF`-Sentinel,
gefolgt von einem einzelnen Flag-Byte (`0x01`), gefolgt von exakt `N` Floats mit Wert `1.0`,
gefolgt von GENAU DREI Byte `00 00 01` (byte-identisch in allen vier Fällen - kein Zufall),
gefolgt von NOCHMAL exakt `N` Floats mit Wert `1.0` (symmetrisch zur ersten Gruppe), gefolgt
von einem weiteren Flag-Byte (`0x01`), danach Nullbytes/andere Daten.

**Wichtigste neue Erkenntnis: `N` ist NICHT konstant** (31/30/30/5) - das ist ein echter,
dateiabhängiger Zähler, keine feste Fork-Eigenheit. Die beiden gleich langen Arrays
symmetrischer Länge `N`, getrennt durch ein festes 3-Byte-Muster, erinnern stark an ein
Gewichts- oder Bindungs-Array-Paar (z.B. zwei parallele Float-Arrays gleicher Länge, wie sie
bei Skinning/Bone-Weights oder Partikel-Attributen üblich sind - beide hier zufällig/
standardmäßig mit `1.0` gefüllt).

**Versuchte Korrelation mit bereits gelesenen Zählern:** die unmittelbar VOR dem
`0xFFFFFFFF`-Sentinel gelesenen Werte (an allen vier Stellen ein Paar `0x00000000`/
`0x80000000`) korrelieren NICHT direkt mit `N`. Für `store.nif` (N=31) zusätzlich geprüft:
weder `NiPSysMeshEmitter`s `num_emitter_meshes` (=0) noch die direkt vorausgehende, komplett
leere `NiNode`(42)-Instanz (children=0, properties=0, effects=0) liefern den Wert 31 - eine
tiefere Suche (z.B. in `NiPSysData`s Partikelanzahl oder noch weiter zurückliegenden
Blöcken) war in dieser Sitzung aus Zeitgründen nicht mehr möglich.

**Für eine Folgesession - konkreter, mit Zahlen unterlegter Fahrplan:** (1) prüfen, ob `N`
mit IRGENDEINEM bereits im selben Block oder einem nahen Vorgängerblock gelesenen Zähler
übereinstimmt (systematisch alle numerischen Felder der letzten 2-3 Blöcke vor dem
`0xFFFFFFFF`-Sentinel auflisten und mit 31/30/30/5 abgleichen); (2) falls keine Übereinstimmung
gefunden wird, den Sentinel/Flag/Array/Separator/Array/Flag-Rahmen versuchsweise als
EIGENSTÄNDIGEN, bisher nicht erkannten Block- oder Sub-Struktur-Typ behandeln (nicht als Teil
von NiNode/NiSourceTexture/NiPSysBoxEmitter selbst) und prüfen, ob eine plausible NIF-
Struktur mit genau diesem Aufbau (Sentinel+Flag+N-Array+3-Byte-Marker+N-Array+Flag)
existiert - Kandidaten: Bone-Gewichte, Partikel-Attribut-Paare, oder eine
Uninitialisiert-Speicher-Fülleigenschaft dieses Forks.

**Ergebnis:** weiterhin kein Massentest-Zuwachs (rein diagnostisch), aber die Signatur ist
jetzt vollständig, byte-exakt und reproduzierbar charakterisiert statt nur "beobachtet" - ein
deutlich besserer Ausgangspunkt für die nächste Sitzung. Massentest bleibt bei **2625/3436
(76.4%)**.

## 51. Wichtige Korrektur zu Abschnitt 50: die Nullregion nach dem Float-Array-Paar ist VIEL
    größer als ursprünglich vermessen (mind. 1,3 KB) - keine kleine, lokal reparierbare Lücke

Weiterverfolgung an `store.nif`: nach dem zweiten `N`-Floats-Array (Abschnitt 50) folgt NICHT
einfach das erwartete nächste Feld, sondern eine Nullregion von MINDESTENS 993 zusammen-
hängenden Nullbytes (13754 bis 14746), unterbrochen von einem einzelnen, bedeutungslos
wirkenden `0x01`-Byte, danach weitere Nullen mindestens bis 14850 (nicht vollständig
vermessen). Ein Test, ob an Position 14747 der laut Header als nächstes erwartete Blocktyp
(`NiTransformController`, 30-Byte-Struktur bereits vorhanden und byte-exakt verifiziert)
plausibel beginnt, ergab überwiegend Nullen (`frequency=0, phase=0, start=0, stop=0,
target=0, interp=0`) - kein klarer Treffer, aber auch kein eindeutiger Widerspruch, da eine
komplett auf Null initialisierte `NiTransformController`-Instanz technisch nicht unmöglich
wäre (nur unüblich).

**Wichtigste Korrektur:** Die in Abschnitt 49/50 aufgestellte Hypothese eines "kleinen,
fehlenden Feldes" ist zu bescheiden - es handelt sich um eine SUBSTANTIELLE, mindestens
kilobyte-große Datenregion, die der aktuelle Parser komplett unberücksichtigt lässt. Das
spricht eher für einen GANZEN, bisher nicht erkannten Block oder eine große
Sub-Struktur (z.B. ein großzügig vorallokierter Partikel-/Gewichtspuffer mit fester
Kapazität, von der nur `N` Einträge tatsächlich befüllt sind, Rest Null) als für ein
einzelnes vergessenes Feld.

**NifSkope-Quellcode (github.com/niftools/nifskope) prüft, hilft aber nicht weiter:**
NifSkope ist ein generisches, ausschließlich `nif.xml`-getriebenes Werkzeug (bestätigt schon
in Abschnitt 49/HANDOFF) - es hat KEINE Sonderbehandlung für unbekannte/proprietäre
Erweiterungen einzelner Spiele-Engines. Da dieser Fork (Fiesta Online, custom Engine)
nachweislich mehrfach von der Vanilla-Spezifikation abweicht (`NiPixelData`,
`ParseNiTriStripsHeader`, vermutlich auch diese Struktur), kann ein generisches Werkzeug
diese proprietäre Erweiterung nicht kennen - nur eine echte, spiel-spezifische Referenz
(z.B. eine andere Fiesta-Online-Reverse-Engineering-Quelle, oder das tatsächliche
Herstellerwerkzeug) könnte hier weiterhelfen.

**Ehrliche Einschätzung für eine Folgesession:** dieses Rätsel ist GRÖSSER und
AUFWÄNDIGER als die übrigen Funde dieser Sitzung - nicht mehr "eine Stunde Bytes zählen",
sondern eher vergleichbar mit den großen, mehrere Sitzungen umspannenden Durchbrüchen wie
`ParseNiTriStripsHeader` selbst. Realistische nächste Schritte: (1) die TATSÄCHLICHE Länge
dieser Nullregion in allen 4 Belegdateien bis zum jeweils nächsten zweifelsfrei plausiblen
Block exakt vermessen (nicht nur die ersten ~1KB wie in dieser Sitzung); (2) prüfen, ob die
Länge der Nullregion (nicht nur `N`) mit einem Zähler korreliert (z.B. `N` selbst, oder
`N × irgendeine feste Größe` wie 32 oder 64 Byte pro Eintrag - ein klassisches Muster für
vorallokierte, aber nur teilweise befüllte Puffer); (3) falls das nicht konvergiert, dieses
Rätsel als eigenständiges, GROSSES Reverse-Engineering-Projekt für eine dedizierte Sitzung
behandeln, nicht nebenbei.

**Ergebnis:** kein Massentest-Zuwachs, aber eine wichtige Korrektur der Problemgröße - vor
einem echten Fixversuch sollte die TATSÄCHLICHE Ausdehnung der Region bekannt sein, sonst
droht wieder ein durch Massentest widerlegter Blindversuch wie bei den drei Dead Ends dieser
Sitzung. Massentest bleibt bei **2625/3436 (76.4%)**. 7/7 Test-Suiten weiterhin grün, echter
Quellcode in diesem Abschnitt NICHT verändert (rein diagnostisch).


## 52. Portale/Teleport-Ziele: `TownPortal.shn`, `RecallCoord.txt`, `Gate_Town`, `MapInfo.shn`

(Stand v0.44.25, alles gegen die echten NA2016-Dateien verifiziert.)

**`TownPortal.shn`** (Client `ressystem/` und Server `9Data/Shine/`, byte-identisch, 748 Byte, 8
Zeilen, verschlüsselt): `Index`(u8) `MinLevel`(u8) `TP_GroupNo`(u8) `MapName`(String[32]) `X`(u32)
`Y`(u32). Die Spaltennamen `X`/`Y` haben nur EIN Zeichen - der SHN-Loader hat sie bis v0.44.24 zu
"Undefined 4/5" umbenannt (siehe Abschnitt-Ende).

| Index | MapName | X | Y | Gate_Town-NPC (NPC.txt) | Abstand |
|---|---|---|---|---|---|
| 0 | RouN | 5874 | 6530 | 5874 / 6530 | 0 |
| 1 | RouVal01 | 13658 | 7812 | 13660 / 7862 | 50 |
| 2 | Eld | 11802 | 10466 | 11788 / 10395 | 72 |
| 3 | EldGbl02 | 9069 | 9312 | 9039 / 9312 | 30 |
| 4 | Urg | 3960 | 5940 | 3882 / 5884 | 96 |
| 5 | Urg_Alruin | 5005 | 9427 | 4962 / 9227 | 205 |
| 6 | Adl | 11720 | 9467 | 11688 / 9379 | 94 |
| 7 | Bera | 7505 | 7279 | 7553 / 7365 | 98 |

Schluss: (X,Y) ist ein Ort auf der Karte `MapName` in denselben Einheiten wie `Coord-X/Coord-Y`
in `World/NPC.txt` (`ShineNPC`, MobName `Gate_Town`, Rolle `QuestNpc`), in unmittelbarer Nähe
des Gate_Town-NPCs (vermutlich Ankunftspunkt neben dem Stadt-Tor). Warum der Versatz variiert,
ist NICHT geklärt.

**`World/RecallCoord.txt`** (Tabelle `RecallPoint`): `ItemIndex` `ItemIdent` `MapName` `LinkX`
`LinkY` (ein Eintrag je Schriftrolle). Weltkoordinaten wie oben. RouN/Eld/Urg: LinkX/Y == RegenX/Y
aus `MapInfo.shn` (6445/8630, 17214/13445, 6293/5477); z.B. EldGbl02 (10119/8609 vs. 9129/8213)
und Urg_Alruin (8003/10596 vs. 6721/12036) weichen ab. Die Datei enthält doppelte Einträge
(z.B. `PriDn01Scroll`/`EchoScroll` doppelt mit gleichem Ziel; `GblDn01Scroll`, `CemDn01Scroll`,
`ElfDn01Scroll`, `ValDn01Scroll` doppelt mit VERSCHIEDENEN Zielen - welcher gilt, ist NICHT
verifiziert) und LF-Zeilenenden.

**`MapInfo.shn`** (Client+Server, 138 Zeilen): `ID` `MapName` `Name` `IsWMLink` `RegenX` `RegenY`
`KingdomMap` `MapFolderName` `InSide` `Sight`. `RegenX/Y` = Wiederbelebungspunkt.

**SHN-Spaltennamen mit 1 Zeichen:** die Namensspalte im SHN-Header ist 48 Byte, nullterminiert;
ein Name wie `X` ist gültig. `MapWayPoint.shn` (`MapID`, `X`, `Y`, `MWP_Gate`) ist ebenfalls
betroffen gewesen.


## 53. Achsenrahmen, BMP-Zeilenreihenfolge, eingebettete NiPixelData (Stand v0.44.27)

**Achsenrahmen (an Karte Rou verifiziert, Objekte zusätzlich an Adl/Cypian/H_Rou/BD_Rou):**
Heightmap (Zeile r = Legacy-Y/50), Objekt-Positionen (posX = Legacy-X, posZ = Legacy-Y), Walk-Grid
(Bitmaske 2048x2048, ein Bit je Zelle, 16 Bit je Wort, Zelle = Welt/6.25) und Server-Koordinaten
(NPC.txt, MapWayPoint.shn, RecallCoord.txt, MobRegen) liegen im selben Rahmen: Spalte = X, Zeile = Y
ohne Spiegelung. Objekt-Höhe posY vs. Heightmap(col=X,row=Y): Median-Fehler 4.9 (Rou) / 0.0 (Adl),
alle anderen Achsenabbildungen >= 64. Walk-Grid-Bits korrelieren mit Heightmap-Steigung nur bei
Identität (+0.21 vs. <= +0.12).

**BMP (Blend-Layer, Vertex-Color):** Dateizeile y == Gitterzeile z (Rohreihenfolge). Mit dem früher
üblichen Bottom-up-Umdrehen passten Blend-Layer nicht zur Heightmap (Layer "rock": Korrelation mit
Höhe -0.06 statt +0.80; Vertex-Color-Luminanz vs. Steigung +0.07 statt -0.32). Konsequenz: als
gewöhnliches Bild betrachtet (Zeile 0 oben) steht die Karte aufrecht mit +Y OBEN, d.h. Bild-unten
= Y=0. Die Ansicht des Editors zeigt deshalb Norden (größeres Y/Z) oben.

**Achsenvertauschung ist eine Spiegelung:** (x,y,z)->(x,z,y) hat det -1. Positionen/Vertices werden
so vertauscht (das ist eine korrekte Darstellung der gespiegelten Welt), Quaternionen müssen
konjugiert werden: (x,y,z,w) -> (-x,-z,-y,w). Zur Anzeige wird Z wieder gespiegelt (Kamera).

**NiPixelData (eingebettete Texturen, NIF >= 10.4.0.2):** ... Mipmaps[Num Mipmaps] (je Breite,
Höhe, Offset), `Num Pixels` (u32, = Summe der Mip-Größen), `Num Faces` (u32, hier immer 1), dann
die Pixeldaten. `Bytes Per Pixel` (u32 vor den Mipmaps): 0 = DXT-komprimiert (Format-Feld 6 =
DXT5 gilt oft NUR nominell: Top-Mip-Größe w*h/2 -> 8-Byte-Blöcke = DXT1-Layout), 3/4 = unkomprimiert
R,G,B[,A]. Bei NIF 10.1/10.2 (isOlderVersion) gibt es KEIN `Num Faces`.

**ShineText-Datei speichern:** Ersetzen in-place, Löschen (Records, die nicht mehr in
`ShineTable::records` stehen) und Einfügen (`#recordin <Tabelle>` hinter `ShineTable::lastLine`)
erst in einem Ausgabedurchlauf. `#record` und `#recordin` gemischt: neue Zeilen immer `#recordin`.


## 54. NIF-Szenengraph, Strip-Ausrichtung, Layer-Regionen, NPC/Mob-Datenmodell (Stand v0.44.28)

**NIF-Szenengraph:** AVObject-Basis = Translation (3 float), Rotation (9 float, ZEILENWEISE: R[0..2] erste
Zeile, v' = R*v), Skalierung. Weltmatrix einer Geometrie = Produkt von der Wurzel bis zum NiTriStrips/
NiTriShape (Kinderlisten der NiNode). Matrizen gelten im Legacy-Rahmen (Z-up); der Loader hält die Vertices
bereits als (x, legacyZ, legacyY) und tauscht für die Transformation zurück. NiLODNode zeichnet nur das
aktive Kind. Belegt an Objekt-Höhen (Adl: 95 % der Objekte auf dem Gelände bei zeilenweiser Konvention).

**NiTriStripsData bei NIF 10.1/10.2:** die Bytes zwischen den UV-Daten und `num_triangles` sind bei
manchen Dateien 4 (Legacy-Fork-Variante), bei anderen 2 (nif.xml: nur Consistency Flags). Prüfsumme:
`num_triangles == Σ Streifenlängen - 2*Streifenanzahl`.

**Blend-Layer-Region:** `#StartPos_X/#StartPos_Y/#Width/#Height` der .ini sind in Heightmap-VERTEX-Einheiten;
Blend-UV = (Welt - Start*Block)/(Größe*Block), Diffuse-UV = Blend-UV * UVScaleDiffuse. Adl: Layer 1-9 Region
(0,0,476,476), Layer 10 (476,0,476,476). Ohne Region = ganze Karte.

**NPC/Mob:** gleiche `ID`+`InxName` in MobInfo (Client+Server), MobInfoServer, MobViewInfo (Client;
Server unter Shine/View/), MobSpecies (`MobName`), QuestSpecies (`MobGroupName`), MobWeapon (mehrere Zeilen
je Mob: Grundangriff + Skills). NPC: `MobInfo.IsNPC=1`, Dialog in Client-NpcDialogData (`MobIDX`),
Platzierung/Rolle in World/NPC.txt (MobName, Map, Coord-X, Coord-Y, Direct, NPCMenu, Role, RoleArg0).
Händler: Role=Merchant mit RoleArg Item/Weapon/WeaponTitle/Skill/Guild -> NPCItemList/<MobName>.txt
(Tabellen Tab00.., Spalten Rec + 6 Slots, Werte = ItemInfo-InxNames; SoulStone-Händler haben keine Datei).
Spieler-Look: `MobViewInfo.NpcViewIndex` -> `NPCViewInfo.TypeIndex` (Class 0-5, Gender, FaceShape, HairType,
HairColor, Action-Codes, 19 Equ_*-Slots mit Item-InxNames).


## 55. Block&Walk-Gitter (`.shbd`): Zellen, Bit-Semantik, Kartenabdeckung (Stand v0.44.29)

Verifiziert an Rou (2048x2048 Zellen), Cypian (4096x4096), Adl (7600x7600) mit Serverpunkten (`NPC.txt`,
`MapWayPoint.shn`): Das Gitter besteht aus `Width` 16-Bit-Wörtern je Zeile (`Width*16` Zellen) und `Height`
Zeilen. Jedes Wort = 16 nebeneinanderliegende Zellen, **Bit 0 = linkeste Zelle (LSB-zuerst)**; **Bit gesetzt =
blockiert, Bit 0 = begehbar** (NPCs/Waypoints liegen zu 100 % auf Bit 0; MSB-zuerst nur 89-99 %). Eine Zelle
ist **immer 6.25 Welteinheiten** breit und hoch. Das Gitter ist QUADRATISCH und deckt die längere Kartenseite
ab (Adl: Spanne 47500 x 23750 -> 7600x7600 Zellen, die Karte belegt nur die Zeilen 0..3799, der Rest ist
"blockiert"); Zelle (cx,cz) liegt bei Welt (cx*6.25, cz*6.25), Zeile 0 = Weltz 0. Gebäude-Grundflächen
(konvexe Hülle der untersten Modellschicht) liegen bei Adl zu 85-97 % auf gesperrten Zellen. Neue Karten
(Editor): Wörter = Blöcke/2 je Zeile, Zeilen = Blöcke*8, alles blockiert (-1).


## 56. Charakter-Modelle `reschar/` und Avatar-Zusammensetzung (Stand v0.44.30)

`reschar/<Klasse>-<m|f>/` (Fighter, Cleric, Mage, Joker, Sentinel; Archer fehlt): Körper-NIF (NiTriShape/NiTriStrips +
NiSkinInstance/NiSkinData/NiSkinPartition, NiBoneLODController, NiMultiTargetTransformController, ~80 Knochen-NiNodes,
77 NiStringExtraData; drei Detailstufen je Teil als getrennte Geometrien OHNE NiLODNode), Bindepose = T-Pose,
Figur ~49 Einheiten hoch (Y nach oben, Vorderseite bei -z). Untexturierte Teile = Knochen-Hüllkörper.
Knochen (Weltposition im Editor-Rahmen, Fighter-m): `Bip01 Head` (-0.6, 45.2, -0.5), `Bip01 R Hand` (-8.0, 28.1, 1.7) -
die Hand-Knoten liegen NICHT an den Händen der Bindepose (Mesh-Hände bei x = ±16.8).
Slots über den Schwerpunkt der Teile: Schuhe < 12, Beine < 29, Körper darüber. `setNNN.nif` enthält Körper/Beine/
Schuhe eines Sets (Nummer = `ItemViewInfo.MSetNo` männlich / `FSetNo` weiblich), Texturname aus der NIF
(z.B. `BrigandineArmor.dds`) wird durch `ItemViewInfo.TextureFile` ersetzt. `Face00N.nif`: Haut + Augen, lokale
Koordinaten = Charakter-Weltrahmen am Kopfknoten (Augen bei -z, oberhalb der Haut); Haare aus `HairInfo`
(`acModelName_Front/Bottom/Top`, `FrontTex`; führendes "_S_"/"_C_" abschneiden).
NiTriShape-Header bei `has_shader=1`: has_shader(u8) + Shader-Name (SizedString) + Shader Extra Data (i32).


## 57. NiTriStrips-/NiTriShape-Daten: Grenzen und Felder (Stand v0.44.31)

`NiTriStripsData`/`NiTriShapeData` (20.0.0.4): Group ID (u32, 0), Num Vertices (u16, bis 65535), Keep Flags (u8, BITFELD -
0, 1 oder 0x33), Compress Flags (u8, 0/1), Has Vertices (u8), Vertices (Num*12), Data Flags (u16; untere 6 Bit = UV-Sets,
0x1000 = Tangenten), Has Normals ... Streifen: Num Triangles (u16), Num Strips (u16), Strip Lengths (u16 je Streifen, bis
65535), Has Points, Indizes. Große Modelle haben Streifen mit über 60000 Indizes (Mauern/Türme). NiTriStrips/NiTriShape-
Header nach den Property-Refs: Collision-Ref, Data-Ref, Skin-Instance-Ref, Has Shader (u8), dann bei 0: 4 Byte (meist
Stringlänge 0; 0xFFFFFFFF = "Active Material" -1), bei 1: Shader-Name (SizedString) + i32 (nur bei nicht leerem Namen).
