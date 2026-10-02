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
