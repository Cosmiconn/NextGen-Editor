# TheSeed Map-Editor — Standalone-Tool

**NextGen-Editor v0.44.35 / v12:** Der aktuelle NIF-Parser-, Build- und Teststand steht in
[docs/V12_VALIDATION.md](docs/V12_VALIDATION.md). Die ZIP enthält vollständige Quellen,
Fixtures, Prüfberichte und unter `bin/` den Windows-Release-Build samt GLFW-DLL.
Der Emulator-Server wurde in diesem Stand nicht verändert.

Eigenständiges Editor-Tool (nicht ins TheSeed-Editor-Modul integriert), Kernlogik aber als
eigenständige, GUI-freie Bibliothek (`mapeditor_core`) gebaut — spätere Integration ins
Haupt-Editor-Modul bleibt dadurch ohne Rewrite möglich.

**Zielplattform: Windows UND Linux** (wie das TheSeed-Hauptprojekt). Der komplette Code ist auf
reiner C++23-Standardbibliothek + `std::filesystem` aufgebaut, keine POSIX-spezifischen Aufrufe.
Details und offene Punkte siehe Abschnitt "Windows" unten.

## Core-Bibliothek + Tests bauen (keine externen Abhängigkeiten, plattformunabhängig)

```bash
g++ -std=c++23 -Wall -Wextra -O2 -Iinclude \
  src/core/Heightmap.cpp src/core/HeightmapIO.cpp src/core/EditOps.cpp \
  tests/test_heightmap_core.cpp -o test_heightmap_core
./test_heightmap_core /pfad/zu/Rou.HTD /pfad/zu/Rou.HTDG   # optional: Legacy-Test gegen echte Datei
```

Die vollständigen Testziele und ihre Quelldateien stehen in `CMakeLists.txt`.
Der Windows-v12-Build besteht **13/13 CTest**, einschließlich echter NIF-Fixtures und
des Scan-Harness mit hartem Prozess-Timeout. Prüfdetails stehen in `docs/V12_VALIDATION.md`;
die Formatgeschichte dokumentieren `docs/MAP_FORMAT.md` und `CHANGELOG.md`.

## Vollständige App bauen (vcpkg + CMake) — Linux

```bash
cmake -B build -S . -DNEXTGEN_EDITOR_BUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/map_editor
```

## Vollständige App bauen — Windows

```powershell
cmake -B build -S . -DNEXTGEN_EDITOR_BUILD_GUI=ON -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
.\build\Release\map_editor.exe
```

**Compiler-Anforderung:** `std::expected` (C++23) braucht Visual Studio 2022 **17.9 oder neuer**
(älterer MSVC-Toolset bricht mit einem Fehler zu `std::expected` ab — falls das passiert, VS
über den Visual Studio Installer aktualisieren, nicht den C++-Standard herunterstufen, da
`std::expected` durchgängig für Fehlerbehandlung verwendet wird).

**UTF-8-Handling:** `CMakeLists.txt` setzt automatisch `/utf-8` für MSVC (`src/app/main.cpp`
enthält direkt eingebettete deutsche Umlaute in ImGui-Labels; MSVC interpretiert Quelldateien
sonst nach der System-Codepage statt UTF-8, was zu falsch dargestelltem Text zur Laufzeit führen
würde). Sollte automatisch greifen, keine manuelle Einstellung nötig.

**Windows-Kompatibilität** (v12 wurde unter Windows mit MSVC gebaut):
- Kein `strnlen`/POSIX-Code mehr (durch reine Standardbibliotheks-Alternative ersetzt)
- Alle Binärformat-Dateien öffnen konsequent mit `std::ios::binary` (verhindert die
  automatische CRLF-Übersetzung des Windows-Textmodus, die Binärdaten korrumpieren würde)
- Alle Pfade laufen über `std::filesystem::path` (kein manuelles String-Concat mit
  hartkodierten `/`-Trennzeichen)
- Case-insensitive Pfadauflösung (`ResolveCaseInsensitivePath` in `LegacyTextureSetIO.cpp`)
  wurde ursprünglich für Linux gebraucht (Windows-authored Pfade auf case-sensitivem
  Dateisystem) - unter Windows selbst harmlos, da dort ohnehin case-insensitiv aufgelöst wird
  (die Fallback-Suche greift dort einfach nie)

**Verifiziert in v12:** Core und vollständige GUI wurden gegen echte GLFW-/ImGui-/glad-
Bibliotheken kompiliert. Der NIF-Renderer wurde separat mit fünf Modellen in einem echten
OpenGL-Kontext auf einer RTX 3060 getestet. Ein vollständiger interaktiver GUI-Durchlauf,
ein visueller Vergleich mit NifSkope und ein erneuter Linux-Build stehen noch aus.

## Bedienung

- **Datei-Menü, ganz oben:** "Asset-Ordner wählen..." öffnet einen Ordnerdialog (Windows) - das
  Tool scannt automatisch nach Karten (`.ini`-Dateien) darunter und zeigt sie als anklickbare
  Liste. Karte anklicken → "Karte öffnen" lädt alle vier Module auf einmal. "Karte speichern"
  (Ausgabeverzeichnis + Kartenname, ebenfalls per Ordnerdialog wählbar) schreibt alles zurück.
  Darunter folgen die granularen Einzel-Modul-Importe für fortgeschrittene/Teilaufgaben
- **Werkzeuge-Panel**: Moduswahl (Heightmap/Textur malen/Block&Walk/Objekte), zugehörige
  Werkzeuge, Undo/Redo je Modul
- **Editor (2D)**: Linke Maustaste = malen (Heightmap/Textur/Walk) bzw. platzieren/auswählen
  (Objekte)
- **3D-Vorschau**: Linke Maustaste + ziehen = Kamera drehen, Mausrad = Zoom; zeigt Heightmap
  UND platzierte Objekte (als Platzhalter-Marker, siehe unten) gemeinsam

## Struktur

```
include/mapeditor/core/   Heightmap, Texturing, Block&Walk, Objekt-Placement (GUI-frei)
  legacy/                 Legacy-Format-Parser (ini, BMP, idm, aid) - reiner Import/Export
src/core/                 Implementierung dazu
src/app/                  main.cpp, Renderer (Terrain), ObjectMarkerRenderer, Camera (Orbit)
tests/                    GUI-freie Tests, direkt mit g++ baubar, je Modul eine Datei
docs/MAP_FORMAT.md        native Formate + vollständige Legacy-Format-Referenzdokumentation
CHANGELOG.md              laufendes Changelog
```

## Aktueller Stand & nächste Ausbaustufen

Die ursprünglichen Module Heightmap, Texturing, Block&Walk und Objekt-Placement besitzen
Legacy-Import/-Export. Der Editor unterstützt inzwischen echtes NIF-Mesh-Rendering;
v12 ergänzt strukturelle Particle-Parser und einen vollständigen NIF-Massentest.
Die aktuellen Ergebnisse und Grenzen stehen in `docs/V12_VALIDATION.md`.

Offene Punkte, siehe Diskussion in `docs/MAP_FORMAT.md`:
1. Vollständige interaktive GUI-Prüfung und visueller Vergleich mit NifSkope
2. Verbleibende NIF-Blocktypen, eingebettete 16-Bit-Texturen und Partikelsimulation
3. `Eld`/`.sbi`-Format (andere/neuere Formatgeneration, nicht untersucht)
4. `.idm`-Zellzuordnung (welche der 1178 Gruppen zu welcher Rasterzelle gehört) bleibt Hypothese
