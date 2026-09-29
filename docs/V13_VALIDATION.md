# v13 · Build- und Validierungsbericht

Datum: 24.09.2026. Basis: v12-Commit `8163829`; Implementierungs-Commit `727793f`.
Die anschließende Auditor-Korrektur betrifft ausschließlich die Zuordnung von HTD/HTDG zu
INI-Metadaten. Quellen und Fixtures sind zusätzlich im [SHA-256-Manifest](v13-results/source-manifest.tsv) erfasst.

## Implementiert

- Strukturelle Fiesta-NIF-Erweiterungen für Accumulation-Interpolator, proprietäre Shaderreferenz
  und Toon-ExtraData; B-Spline-Float/Point3 sowie große Control-Point-Arrays.
- Maskenbasiertes Decoding für rohe eingebettete RGB/RGBA-Texturen und DDS; 16-Bit-TGA,
  korrekte horizontale TGA-Ausrichtung, begrenzte Bildgrößen.
- AID mit sämtlichen Zonen und beiden Datensatzgrößen; Auswahl/Bearbeitung jeder Zone.
- INI- und SHMD-Originalerhalt, einschließlich unbekannter INI-Felder und SHMD ohne Licht-Fußteil.
  NaN-Transformationen werden erhalten, aber nicht als NIF-Mesh gerendert.
- Opaque Karten-Begleitdateien bleiben beim Export erhalten; entfernte eigene Zwischenformate.
- C++23-Auditor mit isolierten Prozessen, fortlaufenden TSVs und hartem Timeout; C++23-Benchmark.
- Shader-Locations werden einmal pro Programm statt 98-mal pro Bild abgefragt;
  Draw-Listen werden wiederverwendet, Modell-/Texturpfadauflösungen je Ladevorgang gecacht.
  NIF-Dateien werden blockweise gelesen; NiPixelData nutzt Sichten statt zweier Kopien.
  SHMD-Gruppierung verwendet eine Hashtabelle; unkomprimierte TGA-Pixel werden gesammelt gelesen.

## Erfolgreich kompiliert

Lokaler Windows-Release-Build mit MSVC 19.51, CMake und den vorhandenen vcpkg-Abhängigkeiten:
`map_editor`, `fiesta_audit`, `nif_probe`, `nif_benchmark` und alle Testziele.
C++23 ist erforderlich, Compiler-Erweiterungen sind ausgeschaltet.

[GitHub-Actions-Lauf für 727793f](https://github.com/Cosmiconn/NextGen-Editor/actions/runs/35943292538):
Linux-Core, Windows-Core und vollständige Windows-GUI jeweils **success**. Dies belegt Build
und die jeweils registrierten CTest-Tests; CI prüft keinen echten GPU-Kontext.
Die spätere kleine Heightmap-Zuordnungskorrektur wurde lokal gebaut und separat gegen alle
176 HTD/HTDG-Dateien geprüft.

## Tests bestanden

**18/18 lokale CTest-Tests**, siehe [Protokoll](v13-results/ctest.log).
Tests erhalten echte Fixture-Pfade; Assertions sind auch im Release-Build aktiv.
Neu geprüft: abgeschnittene NIF-Blöcke, RGB565/RGB5A1/BGRA, DDS-Zeilenabstand,
16-Bit-TGA mit Alpha/RLE/Ursprungsrichtung, mehrere AID-Zonen, alle Trunkierungsgrenzen,
INI-Kommentare/Änderungen/Layer-Zuwachs/-Entfernung, SHMD-Varianten, opaque Begleitdateien
und harte Prozess-Zeitlimits einschließlich paralleler Handle-Isolation.

Die vorherige Python-Harness-Prüfung wurde durch den C++23-Prozesstest ersetzt; die Zahl 18
ist deshalb nicht mit der zwischenzeitlichen Zahl 19 inklusive Python-Harness zu verwechseln.

## NIF-Gesamtbestand

**11.418 SHA-256-eindeutige Dateien:** 11.331 `OK_GEOMETRY`, 87 `OK_NO_GEOMETRY`,
**0 ERROR, 0 TIMEOUT, 0 Warnungen, 0 Dateien mit undekodierten eingebetteten Texturen**.
Kein Recovery-Lauf war nötig. Die Einzelprotokolle `nif-standard.tsv` und `nif-recovery.tsv`
liegen unter `docs/v13-results` in der lokalen vollständigen ZIP.

Der Bestand umfasst die bereits in v12 extrahierten Archive und losen Dateien.
Die `sha256`-Spalte lässt sich mit [dem Herkunftsinventar](v12-results/inventory.tsv) verbinden;
Archivprüfung und ausgeschlossene Pakete sind im [v12-Bericht](V12_VALIDATION.md) dokumentiert.
Nicht jede gültige NIF enthält renderbare Geometrie. Parsererfolg beweist keine vollständige
Partikel- oder Animationssimulation.

## Client- und Server-Dateiprüfung

52.476 lose Dateien inventarisiert; davon **41.067 mit verfügbaren Auditor-Codecs geprüft**.
Quellen: `D:/SERVER_FIESTA/Client` und `D:/SERVER_FIESTA/Server/9Data`.
Originaldateien nur gelesen; Roundtrip-Ausgaben im Audit-Arbeitsverzeichnis.
**0 Infrastrukturfehler, 0 Timeouts.**

| Typ | Ergebnis | Anzahl |
|---|---|---:|
| .aid | ROUNDTRIP_EXACT | 72 |
| .dds | DECODED | 10032 |
| .dds | ERROR | 1 |
| .htd | NEEDS_MAP_INI | 8 |
| .htd | ROUNDTRIP_EXACT | 91 |
| .htdg | NEEDS_MAP_INI | 11 |
| .htdg | ROUNDTRIP_EXACT | 66 |
| .idm | ROUNDTRIP_EXACT | 108 |
| .ini | NOT_MAP_INI | 10 |
| .ini | ROUNDTRIP_EXACT | 109 |
| .kf | OK_NO_GEOMETRY | 12590 |
| .nif | OK_GEOMETRY | 15340 |
| .nif | OK_NO_GEOMETRY | 104 |
| .shbd | ROUNDTRIP_EXACT | 394 |
| .shmd | ROUNDTRIP_EXACT | 236 |
| .shn | ERROR | 1 |
| .shn | ROUNDTRIP_EXACT | 398 |
| .tga | DECODED | 809 |
| .txt | ROUNDTRIP_EXACT | 687 |

Die Einzelberichte `format-results.tsv`, `format-issues.tsv`, `inventory.tsv` und
`extensions.tsv` sind unter `docs/v13-results` in der lokalen ZIP enthalten. Auf GitHub
werden dafür nur die zusammengefassten Zahlen veröffentlicht, ohne neue Pfad-/Dateiinventare.
NIFs in diesem losen Bestand enthalten Duplikate; die Zahl 15.444 ist daher kein Widerspruch
zu 11.418 unterschiedlichen NIFs im Gesamtbestand.

`ROUNDTRIP_EXACT` bedeutet bytegleicher Export einer unveränderten, eingelesenen Datei.
Bei TXT ist zusätzlich ausgewiesen, ob Tabellen erkannt wurden; bloßer Texterhalt ist keine
vollständige Semantik. `NOT_MAP_INI` umfasst acht leere Karten-INIs und zwei andere INI-Typen.

Die zwei verbleibenden echten Decoder-Ablehnungen sind eine **0 Byte große DDS-Datei** und
**QuestData-Header 2**, für den kein vollständiger Codec besteht. 19 HTD/HTDG-Dateien besitzen
keine in ihrem Verzeichnis passend zuordenbare Karten-INI. Eine INI, die ausdrücklich eine
andere Heightmap referenziert, darf nicht zur Ableitung ihrer Dimensionen verwendet werden
(z.B. EventF -> DarkVally). Der ursprüngliche volle Lauf und die korrigierte Zuordnungsprüfung
wurden für die abschließende Tabelle zusammengeführt; die 176 Rohresultate des zweiten Laufs
stehen separat in `docs/v13-results/height-recheck.tsv` in der lokalen ZIP.

Verwendete Auditor-Binärdateien (SHA-256):
- Vollständiger Lauf: `79fd009501d50e33664850ba26ee33f2c1c13639081e99b633be8d149f9f14e4`.
- Korrigierte Heightmap-Zuordnung: `bcdf48ba65ff48a407749ab1f7138effd3a75f838c008575fc7121fd886eb14e`.

## Tatsächlich mit OpenGL geprüft

**18 Modelle** in echtem OpenGL 3.3 auf NVIDIA GeForce RTX 3060, Treiber 591.86.
Alle Läufe beendet mit Exitcode 0 und GL-Fehler 0. Sichtbare Pixel wurden geprüft;
Ausblenden jedes Objekts muss als Negativkontrolle ein leeres Bild liefern.
StaXReward01 bleibt erwartungsgemäß transparent, weil sämtliche Material-Alpha-Werte 0 sind.
KingdomC00 wurde im abschließenden Lauf mit seiner DDS-Fixture getestet.

[Partikel-Fixtures: Log](v13-results/opengl-particles.log) ·
[Erweiterungs-Fixtures: Log](v13-results/opengl-extensions.log).
PNG-Renderbilder liegen im selben Verzeichnis der lokalen ZIP. Dies ist ein Renderer-Test mit unsichtbarem
GLFW-Fenster, kein vollständiger interaktiver GUI-Durchlauf und kein NifSkope-Bildvergleich.

## Performance-Messung

`nif_benchmark` führt nach einem Warm-up 30 vollständige CPU-Ladevorgänge je Fixture aus,
einschließlich Texturdecoding. Median/P95 stehen in
[benchmark-particles.tsv](v13-results/benchmark-particles.tsv) und
[benchmark-extensions.tsv](v13-results/benchmark-extensions.tsv).
Beispiele: machine.nif 2,60 ms Median, EglackMad 8,70 ms, LegelFeatherDemon 23,20 ms.
Dies misst warmen Dateisystem-Cache, keinen kalten Datenträger und keinen GPU-Upload.

Die OpenGL-Logs enthalten den Mittelwert aus 30 Bildern je einzelnem Modell bei 512×512 Pixeln
mit abschließendem `glFinish`. Die abschließenden Messungen liefen nach Ende der parallelen
Dateiprüfung. Daraus folgt keine Framerate-Zusage für komplette Karten und keine prozentuale
Beschleunigung gegenüber v12; dafür fehlt ein kontrollierter Szenenvergleich.

## Weiter offen

Der [Formatstatus](FIESTA_FORMAT_STATUS.md) trennt nutzbare Codecs, reine Erhaltung und
Forschungsbedarf. Insbesondere KFM/Playback, Partikelsimulation, NIF-Export, QuestData v2,
SBI/BDT/SHAB-Semantik und Neuberechnung abgeleiteter Kartendaten sind nicht fertig.
Dieses Release ist deshalb ein verifizierter Fortschritt und keine Zusage, dass alle
Fiesta-Dateitypen bereits vollständig verstanden sind.
