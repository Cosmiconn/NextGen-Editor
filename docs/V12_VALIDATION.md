# NextGen-Editor v0.44.35 / v12 — Prüfbericht

Stand: 23.09.2026. Änderungen ausschließlich am Editor; NextGen-Emulator/Server unverändert.

## Ausgangsbasis

Verwendet wurde `NextGen-Editor-v0_44_35-nif-texture-animation-palette-v11.zip` aus dem
Downloads-Ordner. Vor Änderungen wurden die vorhandenen C++-Quellen, Header und die
CMake-Datei mit dem separat entpackten Archiv verglichen: identisch.

SHA-256 der v11-ZIP:
`83e0b0da8893918003b103b538a859d1362068eddad56480114a993104f436dd`

## Implementiert

- Versionsgerechte Kette `NiGeometryData -> NiParticlesData -> NiRotatingParticlesData -> NiPSysData`.
  Group-ID/unknownInt vor der Vertex-Anzahl ab 10.2.0.0; ParticleDesc 40 statt 28 Bytes
  in 10.x; Rotation-Angles/-Axes und weitere PSys-Felder erst in 20.0.0.4.
- `NiMeshPSysData`: gezählte Integerliste und abschließender Link, `13 + 4*N` Bytes ab 10.2.0.0.
- Die angeforderten Particle-Controller, Cylinder-/SphereEmitter und BombModifier;
  weitere im Massentest identifizierte Controller und SphericalCollider.
  `NiPSysMeshUpdateModifier` liest weiterhin eine gezählte Referenzliste.
- Struktureller Standardpfad für Material, Geometry, SourceTexture, PixelData, Palette und LOD.
  Materialien lesen genau ihre 14 Floats, GeometryData liest die Group-ID selbst;
  SourceTexture liest die Formatpräferenzen selbst; PixelData liest Faces vor den Pixeldaten.
  LOD liest seinen echten Mittelpunkt und die gezählte Bereichsliste.
- Alte Block-Marker vor **jedem** 10.1-Block und PS2-Felder von TextureEffect in 10.x.
  NiCamera, NiShadeProperty, Keyframe-, ControllerManager-/Sequence- und B-Spline-Strukturen.
- Exakte Footer-/Root-Link-Prüfung im Standardpfad. Alte Versatz-/Namens-/Trailerheuristiken
  verbleiben im getrennten Recovery-Pfad. Rückgaben kennzeichnen `recovered` und `partial`.

Partikelsimulation, B-Spline-Auswertung zur Animation und vollständige Shadersemantik der
zusätzlich strukturell gelesenen Blöcke sind damit **nicht** implementiert.

## Erfolgreich kompiliert

MSVC 19.51, Windows x64 Release: gesamte `mapeditor_core`, vollständige `map_editor`-GUI
einschließlich `NifMeshRenderer.cpp`, sämtliche vorhandenen Testprogramme sowie die neuen
Parser-/Scan-/OpenGL-Testziele. Keine GL-Stubs.

Die ZIP enthält den Windows-Build in `bin/map_editor.exe`, `bin/glfw3.dll` und das
Kommandozeilen-Prüfprogramm `bin/nif_probe.exe`. Buildverzeichnisse, Download-Referenzquellen,
extrahierter Massentest-Cache und temporäre Dateien sind nicht enthalten.

## Tests bestanden

**13/13 CTest**, siehe [vollständiges Protokoll](v12-results/ctest.log).

Der ursprüngliche NIF-Test erhält jetzt eine echte Fixture statt sich ohne Argumente zu
überspringen. Der neue Particle-Test prüft alle drei unterstützten Versionen, optionale
Arrays einschließlich Tangenten und zweier UV-Sets, Mesh-Listen mit N=0/1/3/19, jede
Trunkierungsposition, übergroße Zähler, Material-/SourceTexture-Grenzen, Palette-Inhalte,
fehlende/zusätzliche Footerbytes sowie echte Particle- und ältere 10.x-Dateien.

Der Harness-Selbsttest prüft verschachtelte ZIPs, getrennte Archivfehler, Duplikatzuordnung
und einen echten schlafenden Kindprozess: nach 0,2 Sekunden hart beendet; die nachfolgende
Datei wird anschließend erfolgreich geprüft. Damit ist der Timeout nicht nur ein
kooperatives Zeitbudget im Parser.

## Tatsächlich zur Laufzeit/OpenGL getestet

Separater Renderer-Smoke-Test mit einem unsichtbaren GLFW-Fenster und echtem GL-Kontext:
NVIDIA GeForce RTX 3060, OpenGL 3.3.0, Treiber 591.86. Geprüft wurden Shadererstellung,
Mesh-/Textur-Upload, Draw, Framebuffer-Pixel und `glGetError`. Ein zweiter Durchlauf mit
ausgeblendeten Objekten prüft, dass die Pixel tatsächlich vom Modell stammen.

| Modell | Parts | Vertices | Dreiecke | sichtbare Pixel | GL-Fehler |
|---|---:|---:|---:|---:|---:|
| machine.nif | 13 | 2.978 | 1.760 | 3.989 | 0 |
| machine_Urg.nif | 13 | 2.978 | 1.760 | 3.989 | 0 |
| Yak_VaporPower.nif | 34 | 1.762 | 2.386 | 28.608 | 0 |
| Back_Shamrock.nif | 31 | 304 | 274 | 5.083 | 0 |
| StaXReward01.nif | 3 | 12 | 6 | 0 | 0 |

Beide Machine-Modelle besitzen je 12 eingebettete Diffuse-Texturen. StaXReward01 enthält
drei Mesh-Materialien mit Alpha 0; die transparente Ausgabe wird ausdrücklich erwartet
und geprüft. Das ist kein Nachweis einer laufenden Partikelsimulation.

Die ausgegebenen PNGs liegen neben diesem Bericht unter `v12-results/`. Dies ist ein echter
Renderer-Test, aber kein vollständiger interaktiver GUI-Test und kein visueller
Gleichheitstest gegen NifSkope. Eine Szene allein belegt außerdem nicht sämtliche
Blend-, UV-, LOD-, Billboard- oder Animationsvarianten.

## Vollständiger Massentest

Inventar: **63.984 Vorkommen**, **11.418 unterschiedliche SHA-256-Inhalte**. Jedes Vorkommen
bleibt mit Archiv und Dateipfad erhalten; byteidentische Inhalte werden einmal pro Phase
in einem eigenen Prozess geprüft. Der NIF-Lader nutzt nur den Dateiinhalt; externe
Texturpfade werden erst beim Rendern aufgelöst. Daher ist diese Deduplizierung für den
Parsertest geeignet, ersetzt aber keinen Renderer-Test an jedem ursprünglichen Ort.

Enthalten: loser Client-Bestand einschließlich reschar/resmap/resitem/reseffect/ressystem,
`Client.zip`, darin und im Client enthaltene ZIPs rekursiv, Fixtures, `fixtures.zip` und
Downloads `resmap.zip`, `resmap__2_.zip` bis `resmap__5_.zip`.

**63 Archivprüfungen lesbar**, keine untestbaren Archive/Einträge. Insbesondere war
`resmap__3_.zip` hier gültig und wurde einschließlich NIF-Dekompression/CRC geprüft.
Die früheren 5.807 Dateien sind deshalb kein unmittelbar vergleichbarer Nenner.

| Standardpfad | v11, identischer Inhaltssatz | v12 |
|---|---:|---:|
| OK_GEOMETRY | 8.035 | 11.325 |
| OK_NO_GEOMETRY | 70 | 87 |
| ERROR | 3.313 | 6 |
| TIMEOUT | 0 | 0 |
| Summe | 11.418 | 11.418 |

Zwischenstände der Standardfehler: 3.313 → 1.976 (Particle-Fixes) → 89
(strukturelle Material-/Geometrie-/Texturgrenzen) → 15 (weitere Felder/Footer) → **6**.
Keine v11-Standard-OK-Datei ist im finalen v12-Standardpfad fehlgeschlagen.

Erst nach Ende des Standardlaufs wurden dessen sechs Fehler separat mit Recovery geprüft.
Harter Timeout: **5 Sekunden pro Datei**, vier parallele Einzelprozesse; jede abgeschlossene
Zeile wurde unmittelbar in TSV gespeichert.

| Endkategorie | unterschiedliche Inhalte | alle Vorkommen |
|---|---:|---:|
| OK_GEOMETRY | 11.325 | 63.612 |
| OK_NO_GEOMETRY | 87 | 346 |
| RECOVERY_OK (vollständig) | 0 | 0 |
| RECOVERY_PARTIAL | 2 | 9 |
| ERROR | 4 | 17 |
| TIMEOUT | 0 | 0 |
| Summe | 11.418 | 63.984 |

`RECOVERY_PARTIAL` bedeutet ausdrücklich: Die Datei wurde nicht strukturell vollständig
gelesen. Diese Ergebnisse werden weder als Standard-OK noch als vollständiges Recovery-OK gezählt.

## Verbleibende Fehler nach Ursache

| Blocktyp | Standardfehler | Datei / Block | Recovery |
|---|---:|---|---|
| NPTR_ISShader_v2 | 3 | KingdomC00 / 5; Hat_Antler00 Female / 10; Hat_Antler00 male / 10 | alle ERROR |
| NiBlendAccumTransformInterpolator | 3 | EglackMad / 130; Helga / 219; M_MajesticLion / 279 | EglackMad ERROR; übrige zwei PARTIAL |

Für diese beiden Typen ist in den verwendeten Referenzen kein ausreichend belegtes
Layout vorhanden. Es wurde kein geratenes Skip-Layout hinzugefügt.

Zusätzlich **drei Standard-OK-Dateien mit Texturdekodierwarnungen**:
LegelFairy, LegelFeatherDemon und BirthdayConf. Es handelt sich um eingebettete 16-Bit-
Rohtexturen (BytesPerPixel=2), die der vorhandene Bilddecoder noch nicht unterstützt.
Strukturelles OK behauptet daher keine vollständige Texturdarstellung dieser Dateien.
Die aus den drei betroffenen PixelData-Blöcken gelesenen Formatwerte stehen in
`v12-results/texture-warnings.tsv`.

## Berichte und Wiederholung

- `v12-results/inventory.tsv`: jedes NIF-Vorkommen mit Archiv, Name, Version und SHA-256.
- `v12-results/archives.tsv`: separat erfasster Archivstatus.
- `v12-results/standard.tsv` und `recovery.tsv`: vollständige Einzelprozess-Ergebnisse;
  Fehler mit Blockindex/-typ, Meldung, Laufzeit und Dekodierwarnungen.
- `v12-results/standard-groups.tsv`: gruppierte Ursachen statt Dateinamensflut.
- `v12-results/results.tsv`: Endergebnis pro Inhalt mit erhaltener Standarddiagnose.
- `v12-results/occurrence-results.tsv`: Endergebnis für jedes ursprüngliche Vorkommen.
- `v12-results/summary.json`: maschinenlesbare Summen; `baseline-v11.tsv` dokumentiert den Vergleich.

```powershell
# Standardpfad ohne Recovery
.\bin\nif_probe.exe --standard C:\Pfad\machine.nif

# Vollscan; --root darf mehrfach angegeben werden, ZIPs dürfen verschachtelt sein
python tools/scan_nifs.py --probe bin/nif_probe.exe --out scan-results `
  --root D:/SERVER_FIESTA/Client --root C:/Pfad/resmap.zip --jobs 4 --timeout 5

# Nur Recovery nach einem abgeschlossenen Standardlauf
python tools/scan_nifs.py --probe bin/nif_probe.exe --out scan-results --phase recovery --timeout 5

# Build und Tests; GUI benötigt die in vcpkg.json angegebenen Bibliotheken
cmake -S . -B build -DNEXTGEN_EDITOR_BUILD_GUI=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release -- /m:1 /nr:false
ctest --test-dir build -C Release --output-on-failure
.\build\Release\test_nif_opengl.exe tests/fixtures/particles build/gl-output
```

Der GL-Test wird nur mit `NEXTGEN_EDITOR_ENABLE_GL_TESTS=ON` zusätzlich in CTest registriert.
`NEXTGEN_NIF_TRACE=1` aktiviert bei Bedarf Blocktyp-/Offset-Ausgaben auf stderr.

## Formatquellen

Semantik eigenständig implementiert; keine übernommenen Implementierungsblöcke.

- [Niflib GeometryData](https://github.com/niftools/niflib/blob/develop/src/obj/NiGeometryData.cpp),
  [ParticlesData](https://github.com/niftools/niflib/blob/develop/src/obj/NiParticlesData.cpp),
  [PSysData](https://github.com/niftools/niflib/blob/develop/src/obj/NiPSysData.cpp),
  [MeshPSysData](https://github.com/niftools/niflib/blob/develop/src/obj/NiMeshPSysData.cpp).
- [Niflib Objektstrom / alte Block-Marker](https://github.com/niftools/niflib/blob/develop/src/niflib.cpp).
- [NifTools NIF XML](https://github.com/niftools/nifxml/blob/develop/nif.xml):
  SourceTexture, PixelFormat, Palette, LOD, zusätzliche Controller-/Emitterstrukturen.
