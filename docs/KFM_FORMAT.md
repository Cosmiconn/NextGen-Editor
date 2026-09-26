# KFM in Fiesta Online

Implementiert in `KfmFile.hpp/.cpp`, ausschließlich C++23. Unterstützt werden die
beiden im vorhandenen Client gefundenen Versionen **1.2.4b** und **2.0.0.0b**.
Andere Versionen werden ausdrücklich abgelehnt. Die Strings sind byteerhaltend;
der Codec nimmt keine Zeichensatzkonvertierung vor.

## Binärer Aufbau

Alle Zahlen sind little endian, Integer/Float-Felder vier Bytes breit. Ein
SizedString besteht aus einer uint32-Länge und genau dieser Anzahl Bytes.

1. ASCII-Zeile `;Gamebryo KFM File Version <Version>`, LF oder CRLF.
2. Nur 2.0.0.0b: ein erhaltenes, semantisch unbekanntes Byte.
3. NIF-Dateiname und Master-/Root-Name als SizedStrings.
4. Zwei int32- und zwei float32-Felder, deren Werte erhalten werden.
5. int32-Animationsanzahl, gefolgt von den Animationen.
6. Abschließendes int32-Feld. Danach sind keine Bytes zulässig.

Eine Animation enthält Event-ID, nur in 1.2.4b einen Namen, KF-Dateiname,
int32-Sequenzindex, int32-Übergangsanzahl und die Übergänge. Ein Übergang enthält
Ziel-Event-ID und Typ. Bei Typ 5 endet er dort. Bei anderen Typen folgen:

1. float32-Dauer.
2. int32-Anzahl der Textschlüsselpaare; pro Paar zwei SizedStrings.
3. int32-Anzahl der Zwischenanimationen; pro Eintrag int32-Event-ID und float32-Wert.

Die historische [Niftools-KFM-XML, Commit 91eff92d](https://github.com/niftools/kfmxml/blob/91eff92daf197f0c5376740ed4b000d064138ec2/kfm.xml)
dient als Ausgangsreferenz für Header und Basisstruktur. **Ihre Bezeichnungen und
Unterstrukturen der beiden Übergangsarrays stimmen mit diesem Fiesta-Bestand
nicht überein.** Der oben dokumentierte Arrayaufbau ist eine eigene Ableitung aus
den Dateibytes: Er konsumiert alle 1.380 Dateien exakt bis zum Footer. Unabhängige
Testbytes decken beide Arrays gleichzeitig ab; der C++-Writer rekonstruiert sämtliche
Originaldateien bytegleich. Es gibt keinen undurchsichtigen Originalbytes-Fallback.

Im Bestand: 125 Textschlüsselpaare `end` → `start`, 1.205 Zwischenanimationen,
Übergangstypen 0, 2, 3 und 5. Die genaue Runtime-Semantik der Typen, des
Zwischenanimations-Floats und der unbekannten Headerwerte wird nicht behauptet.
Werte bleiben erhalten; unbekannte Typnummern mit dem gleichen belegten Aufbau
werden nicht in erfundene Laufzeitaktionen übersetzt.

## Grenzen und Erhaltung

Vor jeder Leseoperation wird die verbleibende Dateilänge geprüft. Zähler sind
nichtnegativ, durch die minimale Datensatzgröße und ein 256-MiB-Budget für die
angeforderten Daten begrenzt. Das Budget umfasst Strings und Arrayelemente,
nicht den Verwaltungsaufwand des Allokators. Dateien sind auf 64 MiB begrenzt.
Der Writer erhält Float-Bitmuster einschließlich signed zero/NaN und Reihenfolgen.
Nicht darstellbare Zusatzdaten in Typ 5 bzw. alte Animationsnamen in einer neuen
Dateiversion führen zu Fehlern statt stiller Datenverluste.

`SaveKfmFile` erstellt ausschließlich eine **neue** Datei mit C++23 `noreplace`.
Bestehende Ziele werden nicht geöffnet oder überschrieben. Für Einbettung in eine
andere Speichertransaktion steht `EncodeKfm` als reiner Speicher-Serializer bereit.

## Verweise und Oberfläche

Im Projekt-Hauptmenü öffnet **KFM-Animationen** den Katalog. Eine Datei lässt sich
per Pfad oder Windows-Dateidialog laden, nach KF-Datei/Name/Event-ID filtern und als
unveränderte Kopie exportieren. Die ausgewählte Animation zeigt ihre Übergänge;
Hover über die Arrayanzahlen zeigt die Textschlüssel und Zwischenanimationen.

**Dateiverweise prüfen** löst explizite relative Pfade gegen das KFM-Verzeichnis
auf, einschließlich `..`, Windows-Trennzeichen und eindeutiger abweichender
Groß-/Kleinschreibung. Absolute Autorenpfade werden nicht verfolgt. Keine rekursive
Basename-Suche, kein Entfernen bedeutungstragender Leerzeichen. Nicht aufgelöst
bedeutet deshalb nicht zwingend, dass die Datei im gesamten Client fehlt.
Zusätzlich werden doppelte Event-IDs und nicht vorhandene Übergangs-/Zwischenziele
gezählt. Dateiexistenz bestätigt noch keinen gültigen KF-Sequenzindex.

Parser/Writer lesen bzw. schreiben zusammenhängende Puffer. Die Verweisprüfung
verwendet Pfad-Caches und eine Event-ID-Hashtabelle; sie läuft nur auf Anforderung.
Filter werden bei Änderungen berechnet, beide Tabellen zeichnen nur sichtbare
Zeilen. Es gibt keine Datei-/Verzeichnissuche pro Frame.

Der Katalog besitzt inzwischen eine echte KF-Playback-Stufe:

- ausgewählte, aufgelöste KF-Datei laden;
- Play/Pause, Loop, Geschwindigkeitsfaktor und Scrub;
- echte `NiTextKeyExtraData`-Marker auf einer klick-/dragbaren Timeline;
- Live-Sampling der verifizierten Pose-/Linear-/Constant-/XYZ-Transformtracks;
- Trackliste mit aktuell gesampelter Translation/Scale und klarer Kennzeichnung nicht
  unterstützter Interpolationen;
- **NIF-/Skeleton-Viewport**: die explizite KFM-NIF-Referenz wird über denselben sicheren
  Referenzresolver geladen. Die im NIF gelesene Parent-/Local-Bind-Hierarchie wird mit den
  nach Knotennamen gematchten KF-Local-Transforms zusammengesetzt und als bewegtes Skelett
  gerendert. Drag dreht die Ansicht, Mausrad zoomt, Doppelklick setzt die Kamera zurück.
- **echte Skinned-Mesh-Deformation**: der NIF-Parser bewahrt zusätzlich zu seiner bisherigen
  Bind-Pose die originalen Source-Vertices, NiSkin-Weights, Bone-Refs, NiSkinData-Bind-Transforms
  und die nachgelagerte Geometrie-/Parent-Matrix. Der Preview-Sampler wertet diese Daten mit
  exakt derselben Transform-Reihenfolge wie der verifizierte Bind-Pose-Pfad erneut aus.
  Samplebare KF-Tracks deformieren dadurch die echten NIF-Dreiecke synchron zur Timeline.
- Der Preview-Viewport zeichnet die deformierte NIF-Geometrie bewusst als budgetiertes
  Wireframe hinter dem Skelett. Das ist tatsächliche Vertex-/Skin-Deformation, aber noch kein
  separater material-/texturierter Character-Renderer.

Komprimierte Fiesta-B-Splines, TBC/Quadratic-Interpolation und mehrdeutige Node-/Track-Namen
werden weiterhin nicht geraten. Betroffene Nodes/Einflüsse bleiben in Bind-Pose und werden
sichtbar als offen bzw. mehrdeutig markiert. Schreibende Transition-/KFM-Bearbeitung darf nur auf den
bereits verlustfrei belegten Codec-Feldern aufbauen; unbekannte Laufzeitsemantik bleibt
uninterpretiert.
