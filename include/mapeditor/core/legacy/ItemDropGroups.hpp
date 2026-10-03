#pragma once
// ItemDropGroups.hpp
// Auflösung der Drop-Kette der NA2016-Serverdaten:
//   World/ItemDropTable.txt  (Tabelle ItemGroup, je Mob: DrItemN = Gruppenname)
//     -> World/ItemDropGroup.txt  (ItemGroupIdx -> ItemID, MinQtty/MaxQtty, Upgrade-Raten)
//       -> ItemInfoServer.shn DropGroupA/DropGroupB == ItemID  (Menge konkreter Items)
//          oder ItemInfo.shn InxName == ItemID                  (ein einzelnes Item)
// Belegt an den echten Dateien: 29009 von 29402 belegten DrItem-Slots nennen eine vorhandene
// ItemGroupIdx (die übrigen 393 verteilen sich auf 20 in den Daten fehlende Gruppen), 724 von
// 970 Gruppenzeilen lösen über DropGroupA/B auf, 103 direkt über einen InxName. 4850 Slots nennen
// Gruppen ohne auflösbares Item - fast nur Event-Platzhalter (EventItem01-08, Choco01), deren
// Items im NA2016-Stand fehlen; das ist ein Hinweis, kein Fehler.

#include "mapeditor/core/legacy/ShineText.hpp"
#include "mapeditor/core/legacy/ShnFile.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct DropGroupRow {
    std::string itemId;
    std::string minQty;
    std::string maxQty;
};

enum class DropSlotStatus {
    Empty,             // "-", "" oder "0"
    Resolved,          // Gruppe vorhanden, mindestens ein konkretes Item
    GroupWithoutItems, // Gruppe vorhanden, ihre ItemIDs lösen auf kein Item auf
    MissingGroup,      // Name ist keine ItemGroupIdx in ItemDropGroup.txt
    Unknown            // ItemDropGroup.txt nicht geladen
};

class DropGroupCatalog {
public:
    // Liest die Tabelle ItemDropGroup (Spalten ItemGroupIdx, ItemID, MinQtty, MaxQtty).
    // false, wenn Pflichtspalten fehlen.
    bool SetGroups(const ShineTable& itemDropGroup);
    // ItemInfoServer.shn: InxName + DropGroupA/DropGroupB.
    void AddItemInfoServer(const ShnFile& itemInfoServer);
    // Ein Item aus ItemInfo.shn (InxName, Anzeigename).
    void AddItem(const std::string& inxName, const std::string& displayName);

    [[nodiscard]] bool HasGroups() const { return hasGroups_; }
    [[nodiscard]] const std::vector<DropGroupRow>* Group(const std::string& name) const;
    // Konkrete InxNames für die ItemID einer Gruppenzeile (Mitglieder der DropGroup, sonst
    // das Item selbst, wenn die ItemID ein InxName ist).
    [[nodiscard]] std::vector<std::string> ItemsForGroupItemId(const std::string& itemId) const;
    // Alle konkreten InxNames einer Gruppe (über alle Zeilen, ohne Duplikate, sortiert).
    [[nodiscard]] std::vector<std::string> ItemsForGroup(const std::string& name) const;
    [[nodiscard]] DropSlotStatus Check(const std::string& slotValue) const;
    [[nodiscard]] std::string DisplayName(const std::string& inxName) const;

    static bool IsEmptySlot(const std::string& value) { return value.empty() || value == "-" || value == "0"; }

private:
    bool hasGroups_ = false;
    std::unordered_map<std::string, std::vector<DropGroupRow>> groups_;
    std::unordered_map<std::string, std::vector<std::string>> membersByDropGroup_;
    std::unordered_map<std::string, std::string> itemNames_;
};

} // namespace theseed::mapeditor::core::legacy
