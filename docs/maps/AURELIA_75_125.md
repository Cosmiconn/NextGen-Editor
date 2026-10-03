# Aurelia75 – Stadt für Stufe 75–125

## Ziel

`Aurelia75` ist eine neue, sichere High-Level-Stadt für den Bereich **Stufe 75 bis 125**. Sie wird nicht als Kopie einer bestehenden Stadt ausgeliefert: Der Generator übernimmt nur den technisch verifizierten `Rou`-Terrainunterbau (HTD/HTDG und die außerhalb des Stadtplateaus gültigen Block&Walk-Daten) und erzeugt darauf einen **neuen Stadtgrundriss, neue Objektplatzierungen und ein neu gestempeltes Stadt-Kollisionsraster**.

Die Quellinstallation bleibt strikt read-only. Das Ergebnis wird ausschließlich als NextGen-Projekt-Override unter

`<Project>/Client/resmap/field/Aurelia75/`

geschrieben.

## Stadtbild

Aurelia ist als befestigtes Reise- und Progressionszentrum aufgebaut. Der zentrale Platz trennt die Wege bewusst klar, damit die Stadt trotz vieler Gebäude schnell lesbar bleibt.

```text
                         NORDTOR 120+
                    [Veteranen / Expedition]
                       T       T
                           |
                 +------ Runenwerk ------+
                 |      95–105           |
          Park --+                       +-- Veteranenbastion 105–115
                 |          G            |           T  T
WESTTOR 75  ---> | Shops -- Platz -- Shops| ----------------> OSTTOR 105
                 |       Markt 85–95     |
                 |          W            |
                 +------ Hafen ----------+
                        85–100
                     T           T
                         SÜDTOR 90
```

`G` = Haupt-Gilden-/Questgebäude, `W` = Brunnen, `T` = Wachturm.

## Progressionszonen

| Bereich | Stufen | Funktion |
|---|---:|---|
| Ankunftsviertel / Westtor | 75–85 | Einstieg, frühe Quests, Reise-NPCs |
| Kronmarkt | 85–95 | Händler, Lager, soziale Fläche |
| Runenwerk | 95–105 | Schmied, Crafting, Upgrades |
| Veteranenbastion | 105–115 | Veteranenquests, High-Level-Services |
| Morgenwacht / Nordtor | 115–125 | Endbereich, Expeditionen ab 120 |
| Südhafen | 85–100 | Reise-/Expeditionsviertel |

Die Stadt selbst bleibt eine **Safe City**. Die Stufenbereiche definieren Service-/Quest-Handoffs und Ausgangstore; sie erzeugen keine künstlichen Mobs innerhalb der Stadt.

## Verwendete echte ResMap-Assets

Der Generator verwendet ausschließlich Assetpfade, die im echten `Rou.shmd` des Referenzkorpus belegt sind, darunter:

- `GuildHall.nif`
- `ItemShop02.nif`
- `Rou_M_Shop00.nif` bis `Rou_M_Shop07.nif`
- `rou_market.nif`
- `smithsmith.nif`
- `rou_watchTower.nif`
- `rou_lighthouse.nif`
- `rou_Bridge01.nif`
- `waterhall.nif`
- `rou_waterwell.nif`
- Banner-, Store-, Wagon-, Tree- und Woods-Assets
- `field_sky_01.nif`, `sea.nif` und `wool5.nif`

Vor dem Schreiben wird jede Abhängigkeit gegen den angegebenen Client geprüft. Fehlt ein benötigtes Asset, bricht der Generator ab, anstatt einen kaputten Pfad zu erzeugen.

## Technische Basis

Die verifizierte Referenzkarte `Rou` besitzt:

- 257 × 257 Heightmap-Punkte
- 50 Welteinheiten Punktabstand
- 64 × 64 INI-Quads
- 12.800 × 12.800 Welteinheiten Terrainfläche
- Block&Walk-Zellen mit 6,25 Welteinheiten Kantenlänge
- 16 Walk-Zellen pro 16-Bit-Wort; gesetztes Bit = blockiert

Für Aurelia wird das `Rou.shbd` als Ausgangsbasis gelesen. Nur das neue Stadtplateau wird zunächst freigeräumt und anschließend mit konservativen Kollisionsrechtecken der neu platzierten massiven Gebäude neu gestempelt. Die außerhalb der Stadt liegenden verifizierten Rou-Wasser-/Terrainblockierungen bleiben erhalten.

## Erzeugen

Unter Windows aus dem Repository:

```powershell
py tools/maps/generate_aurelia_75_125.py `
  --source-client "D:\Fiesta\Client" `
  --project-root "D:\NextGenProjects\Aurelia"
```

Nur Abhängigkeiten prüfen:

```powershell
py tools/maps/generate_aurelia_75_125.py `
  --source-client "D:\Fiesta\Client" `
  --project-root "D:\NextGenProjects\Aurelia" `
  --validate-only
```

Eine vorhandene generierte Version bewusst ersetzen:

```powershell
py tools/maps/generate_aurelia_75_125.py `
  --source-client "D:\Fiesta\Client" `
  --project-root "D:\NextGenProjects\Aurelia" `
  --force
```

Danach im NextGen-Editor öffnen:

`D:\NextGenProjects\Aurelia\Client\resmap\field\Aurelia75\Aurelia75.ini`

## Generierte Dateien

- `Aurelia75.ini` – Terrain- und Texturlayer-Definition
- `Aurelia75.HTD` – verifizierte Rou-Terrainbasis, unverändert kopiert
- `Aurelia75.HTDG` – passende Ground-Height-Basis
- `Aurelia75.shmd` – **neue Aurelia-Objektierung**
- `Aurelia75.shbd` – Rou-Außenbereich + **neu gestempeltes Aurelia-Stadtplateau**
- `Aurelia75.layout.json` – Districts, Levelbereiche, Safe-Spawn, NPC-/Portalanker, exakte Objektliste
- `README-Aurelia75.txt` – Kurzbeschreibung direkt im generierten Mapordner

## Server-/SHN-Bindings

Der Generator erfindet absichtlich keine NPC-, Mob- oder `TownPortal`-IDs. `Aurelia75.layout.json` enthält stattdessen konkrete Positionen und Rollen für:

- Questmaster 75–95, 95–110 und 110–125
- Schmied
- Item-Händler
- Lager
- Portalmeister
- Gildenmeister
- vier Progressionstore bei 75 / 90 / 105 / 120+

Diese Anker werden anschließend im Editor mit **real vorhandenen SHN-/Servereinträgen** verbunden. So bleibt die Karte technisch sauber und wir vermeiden nicht belegte IDs oder erfundene Serversemantik.

## Abnahme

Für eine erste Kartenabnahme gilt:

1. Generator läuft ohne fehlende Assets durch.
2. `Aurelia75.ini` öffnet im Editor.
3. Alle platzierten NIFs rendern mit Texturen.
4. Zentralplatz, Händler-Ring, Schmiedeviertel, Hafen und Veteranenbastion sind visuell getrennt erkennbar.
5. Hauptwege und vier Tore bleiben begehbar.
6. Massive Gebäude sind im Block&Walk-Raster gesperrt.
7. `Aurelia75.layout.json` stimmt mit der sichtbaren Objektierung überein.
8. NPC-/Portalbindungen werden erst nach Auswahl realer Datensätze vorgenommen.

Die Minimap wird vorerst über die editorinterne Top-Down-Preview geprüft; ein Fiesta-Minimap-Export wird nicht erfunden, solange dessen Zielformat nicht abschließend verifiziert ist.
