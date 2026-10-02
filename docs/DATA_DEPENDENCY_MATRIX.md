# Data Dependency Matrix

Diese Matrix beschreibt, welche Fiesta-Dateien bei typischen Änderungen
**mitgeändert**, **bedingt geprüft/geändert** oder nur **als Referenz gelesen** werden.

Grundlage:

- reale NA2016-Struktur (`FiestaHeroes/NA2016`, Client `ressystem` und
  Server `9Data/Shine`);
- bestehende Spalten-/ID-Auswertung in `docs/SHN_DEPENDENCIES.md`;
- aktuelle Editor-Parser für SHN, QuestData und ShineText;
- konkrete Runtime-Pfade des Editors für Map, NPC, Mob, Quest, Shop, Portal und AI.

Im untersuchten NA2016-Bestand existieren 130 Client-SHN und 220 Server-SHN.
93 Dateinamen kommen auf beiden Seiten vor; **92/93 sind byte-identisch**.
Die einzige abweichende gleichnamige Datei ist `ColorInfo.shn`.
Daraus folgt ausdrücklich **nicht**, dass alle gleichnamigen Dateien automatisch
synchronisiert werden dürfen. Synchronisation wird nur für belegte Familien aktiviert.

## Implementierungs- und Strukturbeleg

Die Matrix ist zusätzlich als typisierter Katalog in
`include/mapeditor/core/DataDependency.hpp` / `src/core/DataDependency.cpp` hinterlegt.
Der Multi-SHN-Workspace verwendet diesen Katalog direkt; die frühere reine
Dateinamen-Keyword-Suche ist für diese Aufgabenprofile nicht mehr die Quelle der Wahrheit.

Am 02.10.2026 wurden **80 konkrete, nicht-parametrisierte Client-/Server-Pfade** aus
diesem Katalog direkt gegen die NA2016-Struktur
(`Client/ressystem` und `Server/9Data/Shine`) geprüft:

- 80/80 vorhanden
- 0 fehlende Pfade
- Platzhalterpfade wie `MobRegen/<Map>.txt`, `MobRoam/<Mob>.txt` und Assetordner
  wurden dabei bewusst nicht als konkrete Datei gewertet.

Wichtig: „Pfad existiert“ beweist noch keine fachliche Pflichtbeziehung. Die Einstufung
PFLICHT/BEDINGT/REFERENZ folgt weiterhin den unten dokumentierten ID-/Familien- und
Runtime-Belegen. Nur Item, Mob und ActiveSkill sind für automatische Mehrdatei-Propagation
freigegeben.

## Legende

- **PFLICHT**: gehört bei dieser Art neuer Identität/Funktion zum selben Datensatz.
- **BEDINGT**: nur ändern, wenn die konkrete Funktion/Felder diese Datei benötigen.
- **REFERENZ**: zur Validierung/Picker lesen; nicht verändern, nur weil darauf verwiesen wird.
- **ASSET**: Nicht-SHN-Datei/-Ordner, ebenfalls nur als Projekt-Override schreiben.

---

## Neue Karte

### PFLICHT

| Seite | Datei | Grund |
|---|---|---|
| Client | `resmap/<Map>/<Map>.ini` + zugehörige Karten-Companions | eigentliche Karte |
| Client | `MapInfo.shn` | Map-Stammdaten; Client/Server-Kopie im NA2016-Bestand identisch |
| Server | `9Data/Shine/MapInfo.shn` | Server-Stammdaten derselben Map |
| Client | `MapViewInfo.shn` | View-/Anzeigeinformationen |
| Server | `9Data/Shine/View/MapViewInfo.shn` | Server-seitige View-Kopie |

`MapInfo`/ `MapViewInfo` sind im untersuchten Bestand als gleich große,
korrespondierende Map-Familie belegt. Automatische Erstellung neuer Rows soll erst
nach expliziter Feldzuordnung erfolgen; bis dahin zeigt der Editor sie als Pflicht-
Checkliste statt plausible Default-Semantik zu erfinden.

### BEDINGT

- `MapLinkPoint.shn` — wenn Map-zu-Map/Gate-Links angelegt werden.
- `MapWayPoint.shn` — wenn Wegpunkt-/Navigationsdaten gebraucht werden.
- `TownPortal.shn` — wenn die Karte über TownPortal/Teleport-Auswahl erreichbar ist.
- `Server/9Data/Shine/World/RecallCoord.txt` — bei Recall-Ziel.
- `Server/9Data/Shine/World/NPC.txt` — bei NPCs/Gates auf der Karte.
- `Server/9Data/Shine/MobRegen/<Map>.txt` — bei Mob-Spawns.
- `WorldMapAvatarInfo.shn`, `MobCoordinate.shn` und weitere Map-Referenztabellen
  nur wenn deren konkrete Funktion verwendet wird.

---

## Neues Item (allgemein)

### PFLICHT – verifizierte ID-Familie

| Seite | Datei |
|---|---|
| Client | `ItemInfo.shn` |
| Server | `9Data/Shine/ItemInfo.shn` |
| Server | `9Data/Shine/ItemInfoServer.shn` |
| Client | `ItemViewInfo.shn` |
| Server | `9Data/Shine/View/ItemViewInfo.shn` |

Für `ItemInfo`, `ItemInfoServer` und `ItemViewInfo` wurden 14.999
Datensätze/IDs und identische ID-Mengen verifiziert. Row-Position ist **kein**
Join-Key; mindestens 115 IDs liegen in `ItemViewInfo` an anderer Position.
Verknüpft wird ausschließlich über ID.

### BEDINGT

Je nach Itemtyp/Funktion, niemals pauschal:

- `WeaponAttrib.shn`
- `GradeItemOption.shn`
- `ItemUpgrade.shn`
- `ItemServerEquipTypeInfo.shn` / `ItemViewEquipTypeInfo.shn`
- `SetItem.shn`, `SetItemEffect.shn`, `SetItemView.shn`
- `ItemAction*.shn`
- `ItemMix.shn`
- `ItemMoney.shn`
- `MoverItem.shn`
- Shop-/Drop-Tabellen
- weitere in `docs/SHN_DEPENDENCIES.md` belegte Item-Referenzen.

### ASSET

Neue Darstellung kann Client-Assets unter `resitem` und abhängig vom
Ausrüstungstyp `reschar` benötigen. Vorhandene Assets werden nur referenziert und
nicht kopiert/geändert.

---

## Neue Waffe / Rüstung / Ausrüstung

**PFLICHT:** dieselbe Item-ID-Familie wie oben.

**BEDINGT:**

- `WeaponAttrib.shn` nur wenn der Datensatz einen neuen/geänderten
  Waffenattribut-/Typ-Eintrag benötigt.
- Equip-Type-Tabellen nur wenn ein neuer/geänderter Equip-Type benötigt wird.
- Upgrade-, Set-, Option- und Action-Tabellen nur wenn die Funktion aktiviert wird.
- passende `resitem`/`reschar`-Assets nur bei neuer Grafik/Geometrie.

Ein normales neues Schwert mit einem **bereits existierenden** Waffentyp darf also
nicht automatisch neue `WeaponAttrib`-/Equip-Type-Zeilen erzeugen.

---

## Neuer Mob

### PFLICHT – verifizierte ID-Familie

| Seite | Datei |
|---|---|
| Client | `MobInfo.shn` |
| Server | `9Data/Shine/MobInfo.shn` |
| Server | `9Data/Shine/MobInfoServer.shn` |
| Client | `MobViewInfo.shn` |
| Server | `9Data/Shine/View/MobViewInfo.shn` |

Für `MobInfo`, `MobInfoServer`, `MobViewInfo` wurden 2.878 IDs als
identische ID-Menge verifiziert.

### BEDINGT

- `MobRegen/<Map>.txt` — nur wenn der Mob auf einer Karte spawnen soll.
- `MobSpecies.shn`, `MobWeapon.shn`, `MobResist.shn`,
  `MobLifeTime.shn`, `MobRegenAni.shn`, `MobAutoAction.shn`,
  `MobCondition*.shn` — nur wenn die konkrete Mob-Konfiguration einen
  entsprechenden Datensatz/Verweis benötigt.
- `MobRoam/<Mob>.txt` — nur bei Patrouillenroute.
- `LuaScript/AIScript/*.lua`, `MobBehaviorDescript/*.ps`,
  `MobAttackSequence/*` — nur wenn der Mob darauf verweist.
- Client-`reschar` — nur bei neuem Modell/Animation/Texture.

Questtabellen, die einen Mob als Ziel referenzieren, werden **nicht** geändert,
nur weil ein Mob existiert.

---

## Neuer NPC

Es sind zwei Fälle zu unterscheiden.

### Bestehende NPC-/Mob-Identität nur platzieren

**PFLICHT:**

- `Server/9Data/Shine/World/NPC.txt`

Die Mob-/View-SHN bleiben unverändert.

### Neue NPC-Identität / neuer MobIndex

**PFLICHT:**

- dieselbe verifizierte Mob-ID-Familie wie bei „Neuer Mob“;
- `World/NPC.txt`, wenn der NPC direkt auf einer Map platziert wird.

### BEDINGT

- `NPCViewInfo.shn` / Server-`View/NPCViewInfo.shn` —
  nur bei Avatar-NPC (`MobViewInfo.NpcViewIndex != 0`).
- `NpcDialogData.shn` — nur wenn der NPC einen Dialog erhält/ändert.
- `NPCItemList/<NPC>.txt` — nur für Merchant/Shop.
- Quest-Dateien — nur wenn zugleich Quest-/Questgeberdaten geändert werden.
- AI-/MobRoam-Dateien — nur wenn diese Rolle/Funktion verwendet wird.
- `reschar` — nur bei neuem Character-/Mob-Asset.

---

## Neue Quest

### PFLICHT

- `Client/ressystem/QuestData.shn`
- `Server/9Data/Shine/QuestData.shn`

Beide `QuestData.shn` sind im NA2016-Referenzbestand derselbe Git-Blob
(`c4a1464c04f423df5ff51404f161238c002a02b1`, 2.140.480 Bytes). Neue/geänderte
Quests werden deshalb im Projekt transaktional auf Client **und** Server gespiegelt.

### BEDINGT

- Client `QuestDialog.shn` **und** die vorhandene Server-Kopie — wenn neue/geänderte Text-IDs benötigt werden.
  Eine aus einer Vorlage geklonte Quest kann zunächst bestehende Text-IDs
  referenzieren; dann darf der Editor `QuestDialog` nicht ungefragt ändern.
- `QuestScript.shn` — nur wenn der verwendete Questpfad dort registrierte
  Scriptdaten benötigt.
- `QuestSpecies.shn` — nur bei tatsächlich benötigter Species-Semantik;
  keine automatische 1:1-Annahme.
- `NpcDialogData.shn` — wenn Questgeberdialog geändert wird.
- `World/NPC.txt` — nur wenn dafür ein neuer Questgeber platziert wird.

### REFERENZ

- `ItemInfo*` für benötigte/rewardete Items;
- `MobInfo*` für Kill-/Mobziele;
- NPC/Mob-Daten für Questgeber/Ziele.

Eine Referenz auf Item/Mob/NPC ist **kein Grund**, den referenzierten Datensatz zu
verändern.

---

## Neuer ActiveSkill

Bereits verifizierte ID-Familie:

- `ActiveSkill.shn`
- `ActiveSkillInfoServer.shn`
- `ActiveSkillView.shn`

2.791 Zeilen / 2.790 IDs; ID 9034 ist als Duplikat bekannt und muss beim Join
explizit als mehrdeutig behandelt werden.

Skillbuch-Items sind eine zusätzliche **bedingte** Item-Familie, wenn der Skill
über ein Item gelernt wird.

---

## Shop / Drop / Portal

Diese Dateien sind keine pauschalen Nebenwirkungen von Item/NPC/Map:

- Shop: `Server/9Data/Shine/NPCItemList/<NPC>.txt`
- Drop: `Server/9Data/Shine/World/ItemDropTable.txt`
- Town-Portal: Client `TownPortal.shn` + Server `TownPortal.shn`, wenn beide
  Quellkopien existieren
- Recall: `Server/9Data/Shine/World/RecallCoord.txt`

Sie werden nur geändert, wenn der jeweilige Editorbereich ausdrücklich bearbeitet
wird.

---

## Implementierungsregel im Editor

1. Picker/Validierung darf beliebig viele abhängige Dateien **lesen**.
2. Automatische Mutation nur bei hier als verifiziert markierten Pflichtfamilien.
3. Bedingte Dateien werden erst verändert, wenn der Nutzer die zugehörige Funktion
   aktiviert oder einen konkreten Datensatz darin bearbeitet.
4. Heuristische Kandidaten (Dateiname/Zeilenzahl) sind nur Warnung.
5. Jeder Save geht durch `docs/PROJECT_OUTPUT_POLICY.md`.
