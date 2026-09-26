# NextGen-Editor

C++23-Editor für die Dateiformate von Fiesta Online. Aktueller Entwicklungsstand: **v0.44.35 / v14**.
Der Emulator ist ein separates Projekt und wurde in diesem Editor-Release nicht geändert.

[Formatstatus und verbleibende Lücken](docs/FIESTA_FORMAT_STATUS.md) ·
[v14: KFM, Tests und OpenGL-Nachweise](docs/V14_VALIDATION.md) ·
[v13: NIF- und Fiesta-Gesamtprüfung](docs/V13_VALIDATION.md) ·
[v12-Referenzstand](docs/V12_VALIDATION.md)

## Bauen

GUI-freier Kern und Prüfwerkzeuge, ohne externe Bibliotheken:

```powershell
cmake -S . -B build -DNEXTGEN_EDITOR_CORE_ONLY=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Vollständige Windows-App mit vcpkg. Unter Windows ist die GUI standardmäßig aktiviert; wenn
`VCPKG_ROOT` oder `VCPKG_INSTALLATION_ROOT` gesetzt ist, wird der Toolchain-Pfad automatisch
erkannt:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\Editor.exe
```

Ohne gesetzte vcpkg-Umgebungsvariable kann der Toolchain-Pfad weiterhin explizit angegeben werden:

```powershell
cmake -S . -B build "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

Ältere Revisionen konnten in einem vorhandenen `build`-Ordner
`NEXTGEN_EDITOR_BUILD_GUI=OFF` hinterlassen. Unter Windows wird dieser alte Cachewert jetzt
automatisch korrigiert: ein normaler Configure/Build erzeugt wieder `Editor.exe`.

```powershell
cmake -S . -B build
cmake --build build --config Release --target map_editor
.\\build\\Release\\Editor.exe
```

Nur wenn bewusst **ohne** Desktop-Editor gebaut werden soll, wird der neue explizite
Core-only-Schalter verwendet:

```powershell
cmake -S . -B build-core -DNEXTGEN_EDITOR_CORE_ONLY=ON
cmake --build build-core --config Release
```

C++23 einschließlich `std::expected` ist erforderlich. Lokal geprüft mit MSVC 19.51.
Für Linux ist beispielsweise GCC 14 vorgesehen; die CI-Konfiguration enthält Windows- und
Linux-Core-Builds. Ein vorhandener Workflow ist noch kein Nachweis eines erfolgreichen CI-Laufs.
Der OS-Prozessstarter des Auditors nutzt unter Windows Job Objects, unter POSIX Prozessgruppen.
Es gibt keine Python-Abhängigkeit für Build, App oder Tests.

## Prüfen

```powershell
.\build\Release\fiesta_audit.exe --root D:\Fiesta\Client --root D:\Fiesta\Server\9Data --out audit --jobs 4 --timeout-ms 5000
.\build\Release\fiesta_audit.exe --root extracted-nifs --out nif-audit --nif-only
.\build\Release\fiesta_audit.exe --root D:\Fiesta\Client --out kf-audit --extension .kf
.\build\Release\fiesta_audit.exe --root D:\Fiesta\Client --out kfm-audit --extension .kfm
.\build\Release\nif_benchmark.exe tests\fixtures\nif-extensions 30
.\build\Release\test_nif_opengl.exe tests\fixtures\nif-extensions gl-output
```

`fiesta_audit` inventarisiert entpackte Verzeichnisse. ZIP-Archive müssen vorher extrahiert werden.
Für v13 wurde zusätzlich der vollständige, in v12 aus Archiven extrahierte und nach SHA-256
deduplizierte NIF-Bestand erneut geprüft; Herkunftszuordnungen stehen im v12-Inventar.
Das Ausgabeverzeichnis des Auditors muss außerhalb seiner Eingabeverzeichnisse liegen.
Jede Datei läuft in einem eigenen Prozess mit hartem Zeitlimit. Nur NIF-Standardfehler erhalten
einen getrennten Recovery-Versuch. TSV-Ergebnisse werden nach jeder Datei geschrieben.
`UNRESEARCHED` im Inventar bedeutet: kein Codec im Auditor; dies ist keine Aussage über
Windows-WIC oder sonstige separate Anzeigefunktionen des Editors.

## Umfang

Karten: INI, HTD/HTDG, Blend-BMP, SHBD, SHMD, IDM und AID. Zusätzlich SHN,
QuestData und Shine-Text-Tabellen. Modelle: NIF einschließlich eingebetteter Texturen,
Materialien und unterstützter Texturanimationen. KF wird strukturell geprüft; vollständiges
Skelettanimations-Playback ist damit nicht zugesagt. Der neue KFM-Katalog liest beide
Fiesta-Versionen, prüft Dateiverweise und exportiert byteerhaltende Kopien. Im Hauptmenü
unter **KFM-Animationen**, siehe [Bedienung und Format](docs/KFM_FORMAT.md). DDS/TGA und unter Windows WIC-Rasterbilder
bleiben erhalten, weil Fiesta diese Formate selbst verwendet.

Die eigenen Zwischenformate TSHM/TSTEX/TSWALK/TSOBJ wurden aus API und Oberfläche entfernt.
Begleitdateien werden unverändert mitgeführt; der Formatbericht benennt, welche abgeleiteten
Daten nach Änderungen noch nicht neu erzeugt werden. Vollständige Spiel- oder NifSkope-
Darstellungsgleichheit, Partikelsimulation und ein NIF-Writer sind noch nicht nachgewiesen bzw. umgesetzt.

Die ZIP im Verzeichnis `releases` enthält Quellen, Fixtures, Berichte und den Windows-Build in
`bin/`. Historische Berichte sind ausdrücklich versionsgebunden; neue Ergebnisse stehen in
`docs/v14-results`. Das Handbuch wird direkt in C++ gepflegt, siehe
[MANUAL_MAINTENANCE.md](docs/MANUAL_MAINTENANCE.md).
