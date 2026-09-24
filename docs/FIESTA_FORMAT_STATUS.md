# Fiesta-Formatstatus · v14

Stand: 24.09.2026. „Bytegenau erhalten“ beweist den Rückschreibpfad für die geprüften Dateien,
nicht automatisch die Bedeutung aller Felder oder das Verhalten des Spiels.
Messzahlen: [v13 Gesamtprüfung](V13_VALIDATION.md) und [v14 KFM-Prüfung](V14_VALIDATION.md).

| Format | Implementierter Umfang | Grenze / nächste Forschungsarbeit |
|---|---|---|
| NIF | Alle 11.418 eindeutigen Dateien des vorhandenen Gesamtbestands strukturell im Standardmodus geladen; Meshes, Materialien, Texturen, Scene-Graph und vorhandene Texturcontroller | Kein NIF-Writer, keine vollständige Partikelsimulation; Spiel-/NifSkope-Gleichheit nicht belegt. Proprietäre Shader-/Accumulation-Blöcke sind strukturell gelesen, nicht vollständig semantisch umgesetzt. |
| KF | NIF-Animationscontainer einschließlich B-Spline-Transform-, Float- und Point3-Interpolatoren strukturell gelesen | Struktureller Erfolg ist kein vollständiges Skelettanimations-Playback. |
| KFM | Strukturierter Reader/Writer für 1.2.4b und 2.0.0.0b; 1.380/1.380 bytegleiche Roundtrips; Katalog, Übergänge, Verweisprüfung und Kopie-Export | Unbekannte Werte erhalten, Runtime-Semantik teilweise offen. Keine Feldbearbeitung oder vollständige Skelettwiedergabe. 12 NIF- und 250 KF-Verweise nicht aufgelöst; drei Zwischenziele fehlen. |
| DDS | BC1/BC2/BC3 sowie gepacktes RGB/RGBA mit Masken und Zeilenabstand; Top-Mip | Kein DDS-Writer, keine vollständige DDS-Formatfamilie. Die leere `_C_FloofBoots copy.dds` ist ein Eingabefehler. |
| TGA | 8-Bit-Grau, 16-Bit-BGR5A1, 24-/32-Bit-Farbe; unterstützte RLE-Pakete und beide Ursprungsachsen | Keine Palette/alle historischen TGA-Varianten; getestet gegen den vorhandenen Bestand. |
| INI | Kartenmetadaten und Layer; Originalbytes bei unverändertem Modell; bekannte Änderungen werden gepatcht, unbekannte Zeilen/Kommentare erhalten | Kein allgemeiner INI-Parser. Leere Karten-INIs bzw. andere INI-Typen bleiben separat ausgewiesen. |
| HTD/HTDG | Höhenraster, Header und Trailer mit explizit zugeordneten INI-Dimensionen | Ohne passende INI keine geratenen Dimensionen; Header-/Trailer-Bedeutung und genaue HTDG-Funktion nicht vollständig geklärt. |
| SHBD | Raster und beide Headerfelder gelesen und bytegenau zurückgeschrieben | Bedeutung aller Bits und spielseitige Navigation nicht vollständig verifiziert. |
| SHMD | Kategorien, Umgebung, alle Objektgruppen und Transformationen; Variante ohne Licht-Fußteil; unveränderte Texte bytegenau erhalten | Bestehende NaN-Transformationen bleiben erhalten und werden vom NIF-Renderer ausgelassen. Bearbeitete Dateien werden kanonisch geschrieben. |
| IDM | Gruppen und Objektindizes gelesen und bytegenau zurückgeschrieben; begrenzte Zähler vor Speicherallokation | Index wird nach Verschieben/Löschen von Objekten noch nicht neu berechnet. |
| AID | Alle Zonen, 48-/56-Byte-Datensätze, Original-Namenspuffer; jede Zone auswählbar und editierbar | Geometrische Bedeutung der Werte über den belegten Datensatzaufbau hinaus nicht abschließend erforscht. |
| SHN | Vorhandener Tabellen-Codec; Testasserts auch in Release aktiv | Unbekannte Feldbedeutungen je Tabelle bleiben Forschungsarbeit. |
| QuestData.shn | Eigenständiger Quest-Codec für Header 6 mit Größen-/Slotprüfungen | Der ältere Header 2 in `Client/shader/ressystem/QuestData.shn` bleibt ausdrücklich nicht unterstützt. |
| Shine-TXT | Tabellen sowie übrige Textbestandteile; bytegenauer unveränderter Export | Dateien ohne erkannte Tabellen sind lediglich textuell erhalten, nicht semantisch vollständig verstanden. |
| BMP | Vorhandener Blendmap-Codec; Windows-WIC für weitere Rastertexturen | Kein vollständiger BMP-Massentest in diesem Auditor. Unterschiedliche Blendmap-Auflösungen werden beim Import weiterhin resampelt. |
| PNG/JPEG | Vorhandener Windows-WIC-Anzeigepfad für Fiesta-Texturen | Kein neuer formatweiter Massentest; keine Zusage für andere Plattformen über WIC. |
| CONF/SBI/SBISSS/SHAB/SHAD/BDT | Begleitdateien im Kartenordner werden unverändert eingelesen und beim Speichern mitgeführt | Opaque Erhaltung, keine vollständige Semantik oder Neuberechnung nach Änderungen. Dateinamen werden beibehalten. |
| NPZ/NSB/NSF/M3D und weitere proprietäre Dateien | Inventarisiert, noch kein vollständiger Codec | Containeraufbau und Zusammenhang mit Karten/Animationen untersuchen. |

## Verifizierte neue Strukturen

`NiBlendAccumTransformInterpolator`: Für Fiesta-Version 20.0.0.4 nach dem
`NiBlendInterpolator`-Teil zwei Gruppen aus je zwei 8-Float-Arrays und einer 3×3-Matrix,
anschließend acht Floats. Die 58 Werte werden einzeln mit Bereichs-/Endprüfung gelesen.
Der Aufbau ist an EglackMad, Helga und M_MajesticLion belegt; die volle Playback-Semantik ist offen.

`NPTR_ISShader_v2`: Zwei SizedStrings mit den Signaturen `NPTR_IS` und `PTSEV2`, danach ein
Boolean. `NsPgToonExtraData`: SizedString, vier Floats, ein undurchsichtiges uint32-Feld,
ein Float, zwei Booleans und zwei Floats. Die Grenzen sind an KingdomC00 und beiden
Antler-Hüten sowie synthetischen Trunkierungsfällen geprüft. Unbekannte Werte werden
nicht als erfundene Links interpretiert.

Die B-Spline-Float-/Point3-Strukturen entsprechen der lokalen Niftools-XML-Referenz
([nifxml](https://github.com/niftools/nifxml/blob/develop/nif.xml)). Große Control-Point-Arrays
werden gegen die verbleibende Dateigröße begrenzt; das frühere pauschale 200.000er-Limit
verwarf valide KF-Dateien.

Die drei früheren NiPixelData-Warnungen sind durch den maskenbasierten 16-Bit-Decoder behoben.
Der Pixelheader wird ausgewertet; ein vorher verschobener Block wird nicht durch Recovery kaschiert.

## Grenzen des vollständigen Kartenexports

Erhaltene Begleitdateien und IDM-Indizes können nach inhaltlichen Kartenänderungen veraltet sein.
Der Export ist deshalb noch kein Generator sämtlicher vom Spiel benötigter abgeleiteter Daten.
Externe Asset-Verweise, Vertexcolor-Dateien und resampelte Blendmaps verhindern außerdem eine
pauschale Zusage, dass jede exportierte Karte eigenständig oder vollständig byteidentisch ist.
Die Roundtrip-Zahlen beziehen sich auf einzelne Codec-Dateien, nicht auf diese weitergehende Zusage.

## Bereinigter Umfang

TSHM, TSTEX, TSWALK und TSOBJ sind aus öffentlichen APIs und Oberfläche entfernt.
App, Prüfwerkzeuge und Tests werden als C++23 gebaut; Python-Scanner und Handbuchgeneratoren
wurden entfernt. DDS/TGA/BMP/PNG/JPEG bleiben als tatsächlich verwendete Fiesta-Texturformate.
Client- und Server-Originaldateien wurden während der Prüfung nur gelesen.
