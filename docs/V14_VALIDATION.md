# v14 · KFM-Codec und Animationskatalog

Datum: 24.09.2026. Basis: v13-Commit `2597dc6`.
Der Emulator wurde nicht geändert. App, Codec und Prüfwerkzeuge sind C++23.

## Implementiert

KFM 1.2.4b und 2.0.0.0b: strukturierter Reader und symmetrischer Writer,
einschließlich alter Animationsnamen, Textschlüsselpaaren, Zwischenanimationen,
Sequenzindizes und unverändert erhaltener unbekannter Werte. Kein bloßes Kopieren
gespeicherter Originalbytes. Details und Grenzen: [KFM_FORMAT.md](KFM_FORMAT.md).

Neuer DE/EN-Katalog im Hauptmenü: Datei öffnen, Animationen filtern, Übergänge
ansehen, Verweise prüfen und eine neue KFM-Kopie exportieren. Vorhandene Ziele
werden nicht überschrieben. Der C++-Auditor unterstützt `.kfm` mit Roundtrip- und
Verweisprüfung; Originaldateien wurden nur gelesen.

## Erfolgreich kompiliert

Windows Release, MSVC 19.51, C++23: vollständige GUI, Core, Auditor und Tests.
Der vorhandene GUI-Buildordner `build-v13` wurde mit dem v14-Quellstand neu gebaut;
der Verzeichnisname bezeichnet keinen älteren Binärstand. [Buildprotokoll](v14-results/build.log).

## Tests bestanden

**19/19 CTest-Tests.** [Protokoll](v14-results/ctest.log).
Der neue KFM-Test prüft unabhängig festgelegte Binärdaten für beide Versionen,
jede einzelne Trunkierungsgrenze, ungültige Längen/Zähler, fremde Versionen und
unerwartete Restbytes. Dazu Änderungen mit mehreren Textschlüsseln/Zwischenanimationen,
Float-Biterhaltung, Ablehnung verlustbehafteter Schreibversuche, bestehende Exportziele
sowie fehlende/mehrfach vorhandene Event-IDs und relative Dateiverweise.

**1.380/1.380 echte KFM-Dateien bytegenau zurückgeschrieben**, null Parserfehler,
null Timeouts, null Infrastrukturfehler und null Warnungen. Prozessisolation:
vier Arbeiter, 5.000 ms hartes Limit je Datei. [Summen](v14-results/kfm-summary.tsv).

| Inhalt | Anzahl |
|---|---:|
| Version 2.0.0.0b | 1.378 |
| Version 1.2.4b | 2 |
| Animationen | 11.457 |
| Übergänge | 965.302 |
| Textschlüsselpaare | 125 in 33 Dateien |
| Zwischenanimationen | 1.205 in 263 Dateien |

## Befunde im Bestand

- 12 nicht aufgelöste NIF-Verweise in 12 KFM-Dateien.
- 250 nicht aufgelöste KF-Verweise in 15 KFM-Dateien.
- Drei Zwischenanimations-IDs ohne entsprechendes Event in ihrer jeweiligen KFM.
- Keine doppelten Event-IDs, keine fehlenden regulären Übergangsziele.

Dies sind Verweisbefunde, keine strukturellen Codecfehler. Es werden keine Verweise
geraten oder Originaldateien repariert. Auf GitHub stehen nur Summen und bereinigte
Build-/Testprotokolle. Pfadgenaue Ergebnisse, Inventar und Screenshots liegen unter
`docs/v14-results/local` ausschließlich im lokalen vollständigen ZIP-Paket.

## Tatsächlich zur Laufzeit/OpenGL getestet

Der **wirkliche KFM-Panel-Code** wurde mit drei echten Dateien (größter Katalog,
Textschlüsselvariante, ältere Version) in einem versteckten GLFW/OpenGL-3.3-Fenster
gerendert, jeweils auf Deutsch und Englisch. NVIDIA GeForce RTX 3060, Treiber 591.86.
Alle sechs Renderfälle lieferten sichtbare Text-/Tabellenpixel und null GL-Fehler.
Der große Katalog enthält 290 Animationen und 83.791 Übergänge. Screenshots wurden
visuell geprüft. Dies ist kein manueller Durchlauf sämtlicher App-Dialoge und kein
Test von Skelettanimations-Playback.

Messung: eine Aufwärmrunde, dann 30 Lese-/Schreibrunden; Median mit warmem Dateicache.
Panelmessung: 60 Frames bei 1.280 × 900 mit abschließendem GPU-Warten, ohne VSync.
Die Einzelwerte stehen in den Protokollen
[großer Katalog](v14-results/largest-runtime.log),
[Textschlüssel](v14-results/textkeys-runtime.log),
[ältere Version](v14-results/legacy-runtime.log).
Diese Werte sind isolierte Codec-/Panelmessungen, keine FPS-Zusage für ganze Karten.

## Offen

Der KFM-Katalog liest und exportiert; interaktive Feldbearbeitung und vollständige
Wiedergabe von KF-Sequenzen/Übergängen sind noch nicht implementiert. Auch die
unbekannten Headerwerte und Runtime-Semantik der Zwischenanimationswerte bleiben
als solche benannt. NIF-Writer, vollständige Partikelsimulation und die übrigen
Formatlücken aus [FIESTA_FORMAT_STATUS.md](FIESTA_FORMAT_STATUS.md) bestehen weiter.
