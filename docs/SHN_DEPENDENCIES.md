# SHN-Abhängigkeiten — Stand 19.09.2026

Diese Datei dokumentiert die tatsächlich gegen alle 350 echten NA2016-SHN-Dateien (Client +
Server, aus `NA2016.zip`) verifizierten Cross-Datei-Beziehungen. Sie ersetzt keine vollständige,
für jede einzelne der 350 Dateien manuell geprüfte Matrix (das wäre ein eigenes, mehrsitzungs-
großes Vorhaben) - sie fasst zusammen, was diese Sitzung mit zwei unabhängigen, automatisierten
Signalen gefunden wurde, plus eine Einschätzung der Zuverlässigkeit jedes Signals.

## Methodik (zwei Signale, kombiniert)

1. **Namens-Stamm-Gruppierung** (`ShnNameStem` in `main.cpp`): bekannte Suffixe abtrennen
   (`InfoServer`, `ViewInfo`, `Info`, `View`, `Server`, `Desc`, `Group`, `Data`, `List`, `Rate`,
   `Count`, `Type`, `State`) und nach übereinstimmendem Rest gruppieren. **Deutlich
   zuverlässiger** als reine Zeilenanzahl - findet auch Teilmengen-Beziehungen, bei denen die
   Zeilenanzahl bewusst NICHT übereinstimmt.
2. **Zeilenanzahl-Gleichschritt** (`FindRowCountPeers`): exakte Übereinstimmung der Zeilenzahl
   zwischen zwei geladenen Dateien, Schwelle >20 Zeilen. Nützlich als zusätzliche Bestätigung,
   aber bei kleinen Tabellen (<100 Zeilen) statistisch unzuverlässig (viele zufällige
   Übereinstimmungen zwischen thematisch unabhängigen kleinen Konfigurationstabellen).
3. **Spalten-Namen-Suche**: Dateien, die eine Spalte mit einem Entitäts-Schlüsselwort im Namen
   haben (z.B. "Item", "Mob", "Skill", "Map"), referenzieren diese Entität sehr wahrscheinlich
   (meist per Namens-String wie `InxName`, nicht per numerischer ID).

Reine Werte-Überlappung (gemeinsame numerische IDs) wurde NICHT verwendet - bei `ItemInfo.shn`s
15000 dichten IDs erzeugte das über 500 falsche Positive (siehe CHANGELOG [0.44.13]).

## Verifizierte "Xxx / XxxInfoServer / XxxView"-Familien (hohe Konfidenz)

Diese drei folgen exakt demselben Muster: eine Basis-Tabelle, eine Server-Erweiterung, eine
View-Tabelle, alle mit identischer Zeilenanzahl UND übereinstimmendem Namens-Stamm:

| Familie | Dateien | Zeilen |
|---|---|---|
| **Item** | `ItemInfo.shn`, `ItemInfoServer.shn`, `ItemViewInfo.shn` | 14999 |
| **Mob** | `MobInfo.shn`, `MobInfoServer.shn`, `MobViewInfo.shn` (+ `MobSpecies.shn`, `QuestSpecies.shn` - gleiche Zeilenzahl, aber Namens-Stamm passt nicht exakt, unklar ob echt oder Zufall) | 2878 |
| **ActiveSkill** | `ActiveSkill.shn`, `ActiveSkillInfoServer.shn`, `ActiveSkillView.shn` | 2791 |

Ein neuer Eintrag in einer dieser Basis-Dateien braucht mit sehr hoher Wahrscheinlichkeit eine
passende Zeile in den anderen Familienmitgliedern.

### Verifizierte ID-Mengen und Cross-Link-Regel (26.09.2026)

Die drei Familien wurden zusätzlich direkt gegen `NA2016.zip` auf ihre `ID`-Spalten geprüft.
Das Ergebnis ist stärker als die frühere reine Zeilenzahl-/Namens-Stamm-Evidenz:

| Familie | Zeilen | eindeutige IDs | ID-Mengen zwischen Familienmitgliedern | wichtige Besonderheit |
|---|---:|---:|---|---|
| Item | 14.999 | 14.999 | exakt identisch | `ItemViewInfo` hat 115 IDs an anderer Zeilenposition als `ItemInfo`; Cross-Link MUSS per ID suchen, niemals per Row-Index |
| Mob | 2.878 | 2.878 | exakt identisch | IDs liegen in allen drei geprüften Dateien zeilenweise gleich |
| ActiveSkill | 2.791 | 2.790 | exakt identisch | ID **9034** kommt zweimal vor; ein ID-only Cross-Link ist dort absichtlich als mehrdeutig zu behandeln |

Daraus folgt für den Editor:
- grün: ID in allen **geladenen** verifizierten Familienmitgliedern vorhanden;
- rot: ID fehlt in mindestens einem geladenen Familienmitglied;
- gelb: ID ist im aktuellen oder Ziel-Dokument mehrfach vorhanden und daher als ID-only-Link mehrdeutig;
- ein nicht geladenes Familienmitglied ist **kein Fehler** und wird nur als „nicht geladen“ angezeigt;
- Cross-Links navigieren per `ID`, nicht per Zeilennummer.

## Weitere Namens-Stamm-Paare (mittlere bis hohe Konfidenz)

Zeilenzahl-Differenzen sind hier bewusst vermerkt - sie deuten auf Teilmengen- oder
1:n-Beziehungen hin, nicht auf einen Fehler:

- `PassiveSkill.shn` (503) / `PassiveSkillView.shn` (503) - exakter Gleichschritt.
- `MapInfo.shn` (138) / `MapViewInfo.shn` (138) - exakter Gleichschritt.
- `CharacterTitleStateServer.shn` (823) / `CharacterTitleStateView.shn` (823) - exakter Gleichschritt.
- `CollectCard.shn` (334) / `CollectCardView.shn` (334) - exakter Gleichschritt; `CollectCardDropRate.shn` (334, gleiche Zeilenzahl, anderer Stamm) vermutlich auch dazugehörig.
- `ItemShop.shn` (3930) / `ItemShopView.shn` (3559) - **Teilmenge**, nicht jeder Shop-Eintrag hat einen View-Eintrag.
- `SetItem.shn` (442) / `SetItemView.shn` (187) - **Teilmenge**.
- `RandomOption.shn` (11456) / `RandomOptionCount.shn` (5928) - ungefähr 2:1, vermutlich mehrere RandomOption-Zeilen pro Count-Eintrag.
- `ItemActionEffect.shn` (202) / `ItemActionEffectDesc.shn` (1025) - **1:n** (mehrere Beschreibungen pro Effekt).
- `Produce.shn` (216) / `ProduceView.shn` (5) - **starke Teilmenge**, nur 5 View-Einträge für 216 Produce-Rezepte, wirkt unfertig/experimentell - vor Verlass auf diese Beziehung manuell gegenprüfen.
- `PupCase.shn` (40) / `PupCaseDesc.shn` (153) - **1:n**.
- `KingdomQuest.shn` (57) / `KingdomQuestDesc.shn` (39) - Teilmenge, nicht jede Quest hat eine Desc-Zeile.

## Spalten-Referenzen (welche Dateien referenzieren welche Entität, aus dem Client-SHN-Satz)

Referenz meist per Namens-String (`InxName`/`ItemIDX`-Spalten), nicht per numerischer ID -
diese Dateien sollten geprüft werden, wenn sich der interne Name eines Eintrags in der
Basis-Datei ändert (nicht nur bei neuen Einträgen):

- **Item**: `ActionEffectItem`, `ActiveSkill`, `ActiveSkillView`, `AttendReward`, `ChargedEffect`, `CollectCard`, `CollectCardReward`, `CollectCardTitle`, `Gather`, `GradeItemOption`, `ItemAction`, `ItemActionEffectDesc`, `ItemMix`, `ItemMoney`, `MiniHouse`, `MiniHouseFurniture`, `MiniHouseFurnitureObjEffect`, `MiniHouseObjAni`, `MinimonAutoUseItem`, `MinimonInfo`, `MoverItem`, `PupMain`, `Riding`, `SetEffect`, `SetItemName`, `TermExtendMatch`, `TermExtendMatchGroupDesc`
- **Mob**: `CollectCard`, `CollectCardGroupDesc`, `CollectCardView`, `MobConditionView`, `MobCoordinate`, `MobNoFadeIn`, `MobRandomIdleAni`, `NpcDialogData`, `WeaponTitleData`
- **Skill**: `ActiveSkillGroup`, `ItemInfo`, `MoverUseSkill`
- **Map**: `DamageSoundInfo`, `GradeItemOption`, `GuildTournamentSkill`, `GuildTournamentSkillDesc`, `MapWayPoint`, `MobCoordinate`, `MobViewInfo`, `TownPortal`, `WorldMapAvatarInfo`

## Bewusst NICHT geprüft / offen

- **Quest**: keine Spalte mit "quest" im Namen im Client-Satz gefunden (nur "Quest" bereits im
  Dateinamen, z.B. `QuestDialog.shn`) - die Referenzierung läuft vermutlich über eine anders
  benannte Spalte (`ID`, `Index` o.ä.), noch nicht identifiziert.
- **Guild**: keine Spalten-Treffer gefunden, obwohl `GuildTournament*`-Dateien existieren -
  vermutlich referenziert über generische `ID`/`GroupID`-Spalten ohne "Guild" im Namen.
- Kleine Zeilenanzahl-Übereinstimmungen (<100 Zeilen, z.B. 21/24/34/36/37/39/41/52/57/60/64) NICHT
  in diese Doku übernommen - bei so wenigen Zeilen ist die Übereinstimmungs-Wahrscheinlichkeit
  zwischen thematisch unabhängigen Tabellen zu hoch, um daraus verlässlich etwas abzuleiten.
- Eine vollständige, für jede der 350 Dateien einzeln manuell verifizierte Matrix bleibt ein
  offenes, größeres Vorhaben.

## Wo das im Editor ankommt

`FindDependencyPeers` (main.cpp, vereint `FindRowCountPeers` + `FindNameStemPeers`) markiert
im Datei-Browser des Single-SHN-Editors automatisch alle hier per Namens-Stamm ODER
Zeilenanzahl gefundenen Familienmitglieder mit einem gelben ⚠-Symbol + Tooltip.

Zusätzlich besitzt der Single-SHN-Grid jetzt eine **streng begrenzte semantische Referenzprüfung** für die drei oben verifizierten ID-Familien Item/Mob/ActiveSkill. ID-Zellen erhalten Grün/Rot/Gelb-Status, der ausgewählte Datensatz zeigt die geladenen Familienziele, und eindeutige Treffer sind direkt anklickbar. Für andere Dateien/Spalten wird bewusst keine Referenzsemantik erfunden.
