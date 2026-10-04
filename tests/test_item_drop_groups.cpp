// test_item_drop_groups.cpp
// Drop-Kette ItemDropTable -> ItemDropGroup -> ItemInfoServer.DropGroupA/B bzw. ItemInfo.
// Ohne Argument: synthetische Daten. Mit <NA2016-Datenordner> (enthält Shine/ bzw. Server-
// Shine-Inhalt): zusätzlich die gemessenen Zahlen der echten Dateien.

#include "mapeditor/core/legacy/ItemDropGroups.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>

using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
ShnColumn Col(const char* name) { ShnColumn c; c.name = name; c.kind = ShnValueKind::String; return c; }
ShnRow Row(std::initializer_list<const char*> v) { ShnRow r; for (const char* s : v) r.values.emplace_back(std::string(s)); return r; }

void Synthetic() {
    const auto path = std::filesystem::temp_directory_path() / "nextgen_itemdropgroup_test.txt";
    {
        std::ofstream out(path, std::ios::binary);
        out << "#TABLE\tItemDropGroup\r\n"
               "#COLUMNTYPE\tIndex\tString[40]\tWord\tWord\r\n"
               "#COLUMNNAME\tItemGroupIdx\tItemID\tMinQtty\tMaxQtty\r\n"
               "#RECORD\tPotion01\tPotion01\t1\t2\r\n"
               "#RECORD\tRing\tRingA\t1\t1\r\n"
               "#RECORD\tDead\tNoSuchGroup\t1\t1\r\n"
               "#RECORD\tMixed\tNoSuchGroup\t1\t1\r\n"
               "#RECORD\tMixed\tRingB\t1\t1\r\n";
    }
    auto file = LoadShineTextFile(path);
    Check(file.has_value() && file->FindTable("ItemDropGroup"), "synthetische ItemDropGroup geladen");
    if (!file) return;
    DropGroupCatalog cat;
    Check(cat.Check("Potion01") == DropSlotStatus::Unknown, "ohne Gruppen: Unknown");
    Check(cat.SetGroups(*file->FindTable("ItemDropGroup")), "Gruppen übernommen");
    ShnFile server;
    server.columns = {Col("InxName"), Col("DropGroupA"), Col("DropGroupB")};
    server.rows = {Row({"RedPotion", "Potion01", "-"}), Row({"BluePotion", "Potion01", "-"}), Row({"Sword", "-", "Potion01"})};
    cat.AddItemInfoServer(server);
    cat.AddItem("RingA", "Ring of A");
    cat.AddItem("RingB", "Ring of B");
    Check(cat.ItemsForGroup("Potion01") == std::vector<std::string>{"BluePotion", "RedPotion", "Sword"},
          "DropGroupA und DropGroupB liefern die Mitglieder");
    Check(cat.Check("Potion01") == DropSlotStatus::Resolved && cat.Check("Ring") == DropSlotStatus::Resolved,
          "Gruppe über DropGroup bzw. direkten InxName aufgelöst");
    Check(cat.Check("Dead") == DropSlotStatus::GroupWithoutItems, "Gruppe ohne auflösbare ItemID erkannt");
    Check(cat.Check("Mixed") == DropSlotStatus::Resolved && cat.ItemsForGroup("Mixed") == std::vector<std::string>{"RingB"},
          "mehrere Zeilen je Gruppe: eine auflösbare genügt");
    Check(cat.Check("Potion06") == DropSlotStatus::MissingGroup, "unbekannter Gruppenname erkannt");
    Check(cat.Check("-") == DropSlotStatus::Empty && cat.Check("0") == DropSlotStatus::Empty, "'-' und '0' sind leer");
    Check(cat.DisplayName("RingA") == "Ring of A" && cat.DisplayName("RedPotion") == "RedPotion", "Anzeigenamen");
    std::filesystem::remove(path);
}

void RealData(const std::filesystem::path& root) {
    std::filesystem::path shine = root / "Shine";
    if (!std::filesystem::exists(shine / "World" / "ItemDropGroup.txt")) shine = root;
    const auto groups = LoadShineTextFile(shine / "World" / "ItemDropGroup.txt");
    const auto table = LoadShineTextFile(shine / "World" / "ItemDropTable.txt");
    const auto server = LoadShnFile(shine / "ItemInfoServer.shn");
    const auto items = LoadShnFile(shine / "ItemInfo.shn");
    Check(groups && table && server && items, "NA2016: ItemDropGroup, ItemDropTable, ItemInfoServer, ItemInfo geladen");
    if (!groups || !table || !server || !items) return;
    DropGroupCatalog cat;
    Check(cat.SetGroups(*groups->FindTable("ItemDropGroup")), "NA2016: Gruppen übernommen");
    cat.AddItemInfoServer(*server);
    int cInx = -1;
    for (std::size_t i = 0; i < items->columns.size(); ++i) if (items->columns[i].name == "InxName") cInx = static_cast<int>(i);
    for (const auto& row : items->rows) cat.AddItem(ShnValueToString(row.values[static_cast<std::size_t>(cInx)]), {});

    const auto* dropTable = table->FindTable("ItemGroup");
    Check(dropTable && dropTable->records.size() == 1485, "NA2016: 1485 Mob-Drop-Einträge");
    if (!dropTable) return;
    const std::regex slotName(R"(DrItem\d+)");
    std::size_t slots = 0, missing = 0, resolved = 0, empty = 0;
    for (std::size_t c = 0; c < dropTable->columns.size(); ++c) {
        if (!std::regex_match(dropTable->columns[c].name, slotName)) continue;
        for (const auto& r : dropTable->records) {
            const std::string v = c < r.values.size() ? r.values[c] : std::string();
            if (DropGroupCatalog::IsEmptySlot(v)) continue;
            ++slots;
            switch (cat.Check(v)) {
            case DropSlotStatus::MissingGroup: ++missing; break;
            case DropSlotStatus::Resolved: ++resolved; break;
            case DropSlotStatus::GroupWithoutItems: ++empty; break;
            default: break;
            }
        }
    }
    std::printf("         %zu Slots: %zu aufgelöst, %zu Gruppe ohne Items, %zu Gruppe fehlt\n", slots, resolved, empty, missing);
    Check(slots == 29402 && missing == 393, "NA2016: 29402 belegte Slots, 393 nennen eine fehlende Gruppe");
    Check(resolved == 24159 && empty == 4850,
          "NA2016: 24159 Slots lösen bis zu Items auf, 4850 nennen leere Gruppen (v. a. EventItem01-08, Choco01)");
    Check(cat.Check("EventItem01") == DropSlotStatus::GroupWithoutItems, "NA2016: EventItem01 ist eine leere Event-Gruppe");
    Check(!cat.ItemsForGroup("Shoes1").empty(), "NA2016: Gruppe Shoes1 hat Items (LeatherBoots u. a.)");
}
} // namespace

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1 && argv[1][0] != '\0') RealData(argv[1]);
    else std::printf("         (ohne NA2016-Datenordner: nur synthetische Prüfungen)\n");
    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle ItemDropGroup-Tests bestanden.\n");
    return 0;
}
