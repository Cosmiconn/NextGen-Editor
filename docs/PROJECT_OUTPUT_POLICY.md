# Project Output Policy

## Ziel

Die konfigurierte Fiesta-Installation ist **read-only Quelle**. Der NextGen-Editor darf
Spiel-/Client-/Serverdaten niemals direkt in diesen Quellordnern überschreiben.

Jede vom Editor erzeugte oder geänderte Datei wird ausschließlich unter dem
konfigurierten Projektordner gespeichert:

```text
<Project>/
  project.tsproj
  Client/
    ressystem/
    resmap/
    resmenu/
    resitem/       # wenn ein Workflow neue/ersetzte Item-Assets erzeugt
    reschar/       # wenn ein Workflow neue/ersetzte Character-Assets erzeugt
  Server/
    9Data/
      Shine/
        World/
        MobRegen/
        MobRoam/
        NPCItemList/
        LuaScript/
        ...
```

Die Unterordner werden nur angelegt, wenn ein Workflow sie benötigt.

## Copy-on-write

Beim Lesen gilt:

1. Ermittle die Originaldatei aus dem konfigurierten Client-/Server-Quellbaum.
2. Ermittle den dazugehörigen Projektpfad.
3. Existiert die Projektdatei, wird **sie** als Arbeitsfassung geladen.
4. Sonst wird die Originaldatei gelesen.
5. Beim ersten Speichern wird ausschließlich die Projektdatei angelegt.

Dadurch bleibt die ursprüngliche Installation unverändert und ein Projekt kann
jederzeit als Patch-/Override-Satz betrachtet werden.

Diese Priorität gilt nicht nur für die Haupteditoren, sondern auch für
Hilfs-/Referenz-Lader: dynamisch nachgeladene SHNs, Quest-Item-Lookups,
`MapInfo.shn`, `MobViewInfo.shn` und `MobRoam/*.txt` lesen vorhandene
Projekt-Overrides vor der Basisinstallation. So sieht jede Editoransicht denselben
Projektstand.

## Pfadregeln

- Client-Quelle `<Client>/ressystem/ItemInfo.shn`
  -> `<Project>/Client/ressystem/ItemInfo.shn`
- Server-Quelle `<Server>/9Data/Shine/QuestData.shn`
  -> `<Project>/Server/9Data/Shine/QuestData.shn`
- `World/NPC.txt`
  -> `<Project>/Server/9Data/Shine/World/NPC.txt`
- `MobRegen/Rou.txt`
  -> `<Project>/Server/9Data/Shine/MobRegen/Rou.txt`
- vorhandene Karte `<Client>/resmap/Rou/Rou.ini`
  -> `<Project>/Client/resmap/Rou/Rou.ini`
- neue Karte `MyMap`
  -> `<Project>/Client/resmap/MyMap/MyMap.ini`

Absolute Pfade, `..`-Escapes und Quelldateien außerhalb des konfigurierten
Client-/Server-Roots werden als Save-Ziel abgewiesen.

Zusätzlich muss der **Projektordner physisch von beiden read-only Quellen getrennt**
sein. Ein Projektordner, der mit dem konfigurierten Client-/Server-Root identisch ist
oder darunter liegt, wird bereits beim Konfigurations-Save und erneut in der zentralen
Output-Auflösung hart abgewiesen. Dadurch kann auch ein formal gültiger
`<Project>/Client/...`-Pfad niemals versehentlich innerhalb der Fiesta-Quellinstallation
liegen.

Auch die fortgeschrittenen Einzel-Import/Export-Wege der Kartenmodule folgen
dieser Policy. HTD/HTDG, Textur-Set, SHBD, SHMD, IDM und AID dürfen beim Export
nicht mehr direkt in den eingegebenen Client-Quellpfad schreiben: Ein Quellpfad wird
auf den korrespondierenden `<Project>/Client/...`-Pfad gemappt, ein bereits valides
Projektziel bleibt erhalten, Fremdziele und explizite `..`-Traversierungen werden
abgewiesen. Beim erneuten Import derselben Clientdatei gewinnt ein vorhandener
Projekt-Override.

Für `resmenu` werden relative Assetpfade zusätzlich vor dem Join geprüft; absolute
Pfade und `..`-Komponenten können den Projekt-`resmenu`-Baum nicht verlassen.

Die Ausgabe bleibt auch dann **kanonisch**, wenn als read-only Quelle ein tieferer
Unterordner gewählt wurde:

- gewählte Quelle `<Client>/ressystem` → weiterhin `<Project>/Client/ressystem/...`
- gewählte Quelle `<Server>/9Data` → weiterhin `<Project>/Server/9Data/Shine/...`
- gewählte Quelle `<Server>/9Data/Shine` → weiterhin `<Project>/Server/9Data/Shine/...`

Der Projektbaum hängt damit nicht davon ab, wie tief der Nutzer beim Konfigurieren
der Originalquelle eingestiegen ist. Diese Fälle sind durch `test_project_output`
abgedeckt.

## Minimal-invasive Änderungen

### Binäre SHN

Der SHN-Editor hält den ursprünglichen Quellpfad separat von der Arbeitsfassung.

Vor jedem Save wird die auf Platte liegende Baseline erneut geladen. Der Save wird
abgewiesen, wenn:

- Version, Verschlüsselungszustand oder Spaltenschema geändert wurden;
- bestehende Zeilen gelöscht/umgeordnet wurden;
- eine bestehende Zelle geändert wurde, die nicht als editierte Zelle markiert ist;
- eine neue Zeile nicht vollständig als explizit neu markiert ist.

Erlaubt sind damit ausschließlich:

- explizit bearbeitete Zellen bestehender Records;
- explizit angehängte neue Records.

Ein unverändertes SHN bleibt laut bestehendem Roundtrip-Test byte-identisch.

**Client/Server-Kopplung:** Liegt dieselbe Tabelle in Client und Server (NA2016: 74 Dateien,
73 davon bytegleich; Ausnahme `ColorInfo.shn`), speichert der Editor die Gegenseite mit, wenn
ihr aktueller Stand bytegleich zum bisherigen Stand der bearbeiteten Datei ist und sie keine
eigenen ungespeicherten Änderungen hat. Sonst wird nur die bearbeitete Seite geschrieben und
die Statuszeile nennt den Grund.
Bei längenändernden Strings oder angehängten Records kann sich wegen Dateilänge/
Verschlüsselung die Binärposition nachfolgender Bytes verschieben; fachlich
unveränderte Records und das Schema bleiben trotzdem unverändert.

### Shine-Textdateien

`ShineTextFile` bewahrt die Originalzeilen, das Zeilenende jeder einzelnen Zeile,
Kommentare und unbekannte Direktiven. Bestehende Records besitzen ihre
`sourceLine`.

Beim Save wird nur die betroffene Record-Zeile ersetzt. Neue Records werden an der
zugehörigen Tabelle ergänzt. Unveränderte Dateien sind im Regressionstest
byte-identisch.

Das gilt unter anderem für:

- `World/NPC.txt`
- `MobRegen/*.txt`
- `MobRoam/*.txt`
- `NPCItemList/*.txt`
- `World/RecallCoord.txt`
- `World/ItemDropTable.txt`

### QuestData

`QuestData.shn` besitzt einen eigenen Roundtrip-Test. Eine unveränderte Datei bleibt
byte-identisch; beim Test einer bearbeiteten Quest bleiben alle anderen Quest-
Datensätze logisch unverändert.

Im NA2016-Referenzbestand sind Client- und Server-`QuestData.shn` byte-identisch.
Der Quest-Editor staged deshalb beide Projektkopien und aktiviert sie gemeinsam:
`<Project>/Client/ressystem/QuestData.shn` und
`<Project>/Server/9Data/Shine/QuestData.shn`. Schlägt das zweite Commit fehl,
wird die erste Projektkopie zurückgerollt; die Originalinstallation wird zu keinem
Zeitpunkt beschrieben.

### Karten

`SaveLegacyMap` schreibt den vollständigen bearbeiteten Karten-Dateisatz in den
Projekt-Clientbaum. Originale unbekannte/erhaltene Companion-Dateien werden in den
Projektbaum übernommen, nicht in der Quelle verändert.

Ini-Pfade wie `.\resmap\field\Rou\block.Bmp` oder `.\resmap\fieldtexture\L1_A.BMP`
sind relativ zum Client-Ordner. Blend-BMPs werden deshalb unter `<Project>/Client/resmap/...`
an genau diesem Pfad abgelegt, `#HeightFileName` bleibt client-relativ. Unveränderte
Blend-BMPs werden mit ihren Originalbytes geschrieben; bemalte nur an den geänderten Pixeln
gepatcht (Header, Füllbytes und Nicht-Grau-Pixel bleiben erhalten). Eine beim Laden fehlende,
unbemalte Maske wird nicht angelegt.

### End-to-End-Prüfung (NA2016)

Mit den echten NA2016-Daten (Client-`ressystem`, Server-`Shine`) und Roumen im echten
Client-Layout wurden alle Module über die normalen Speicherfunktionen in einen leeren
Projektordner gespeichert (Automatisierung `saveall all`): 348 SHN, QuestData (Client +
Server), ItemDropTable, TownPortal (Client + Server), RecallCoord, NPC.txt, MobRegen und die
Karte. Ergebnis: 365 Projektdateien, alle bytegleich zur Quelle und alle am kanonischen Pfad
(`Client/ressystem`, `Client/resmap/...`, `Server/9Data/Shine/...`); die Prüfsummen aller
Quelldateien sind unverändert. Eine geänderte SHN-Zelle ändert genau ihre Bytes und wird nach
einem Neustart aus dem Projekt geladen.

## Automatische Mehrdatei-Änderungen

Automatische SHN-Propagation darf nur auf **verifizierten** Familien basieren.
Aktuell sind dafür Item, Mob und ActiveSkill freigegeben.

Die alte Heuristik „ähnlicher Dateiname + gleiche Zeilenzahl“ bleibt ausschließlich
als Diagnosehinweis sichtbar und darf keine Datei verändern.

Siehe `docs/DATA_DEPENDENCY_MATRIX.md` und `docs/SHN_DEPENDENCIES.md`.

## Deployment

Der Projektordner ist absichtlich **kein Live-Patch der Installation**. Ein späterer
Deployment-/Merge-Schritt muss separat und ausdrücklich vom Benutzer gestartet
werden. Damit kann normales Bearbeiten niemals versehentlich die Arbeitsinstallation
beschädigen.
