// test_quest_layout.cpp
// Belegt die Byte-Lage der QuestData-Records an der echten NA2016-QuestData.shn (2304 Quests):
// Querbezüge zwischen Feldern, die nur bei korrekter Lage stimmen können (Titel/Beschreibung,
// Vorgänger, Levelbereich, Drop-Items = Item-Ziele), Füllfelder = 0, Belohnungsstruktur,
// sowie Bearbeiten + Speichern + Neuladen von Belohnungen und Drops.

#include "mapeditor/core/legacy/QuestData.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
const QuestRecord* FindQuest(const QuestDataFile& f, std::uint16_t id) {
    for (const auto& q : f.records) if (q.id == id) return &q;
    return nullptr;
}
std::vector<char> ReadAll(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
bool AllZero(const auto& bytes) {
    return std::all_of(bytes.begin(), bytes.end(), [](std::uint8_t b) { return b == 0; });
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: test_quest_layout QuestData.shn\n"); return 2; }
    const std::filesystem::path source = argv[1];
    auto loaded = LoadQuestData(source);
    Check(loaded.has_value(), "QuestData.shn geladen");
    if (!loaded) return 1;
    const auto& f = *loaded;
    std::printf("         %zu Quests\n", f.records.size());
    Check(f.records.size() == 2304, "2304 Quests (NA2016)");

    std::set<std::uint16_t> ids;
    for (const auto& q : f.records) ids.insert(q.id);

    // --- Querbezüge, die nur bei korrekter Byte-Lage stimmen ---
    std::size_t descIsTitlePlusOne = 0, withPred = 0, predKnown = 0, withLevel = 0, levelOk = 0;
    std::size_t drops = 0, dropInObjectives = 0, npcTargets = 0, killTargets = 0;
    bool padsZero = true, freeSlotsZero = true, isMobZero = true, itemTypeZero = true;
    for (const auto& q : f.records) {
        if (q.description == q.title + 1) ++descIsTitlePlusOne;
        if (q.needPred) { ++withPred; if (ids.contains(q.predecessor)) ++predKnown; }
        if (q.needLevel) { ++withLevel; if (q.minLevel <= q.maxLevel) ++levelOk; }
        padsZero = padsZero && q.idPad == 0 && q.unk2Pad == 0 && AllZero(q.unk4);
        freeSlotsZero = freeSlotsZero && AllZero(q.unusedDropSlots);
        std::set<std::uint16_t> objectiveItems;
        for (const auto& it : q.items)
            if (it.active) { objectiveItems.insert(it.id); itemTypeZero = itemTypeZero && it.type == 0; }
        for (const auto& d : q.drops) { ++drops; if (objectiveItems.contains(static_cast<std::uint16_t>(d.itemId))) ++dropInObjectives; }
        for (const auto& m : q.mobs) {
            if (!m.active) continue;
            isMobZero = isMobZero && m.isMob == 0;
            if (m.hasToBeKilled) ++killTargets; else if (m.amount == 0) ++npcTargets;
        }
    }
    std::printf("         Beschreibung = Titel+1: %zu · Vorgänger %zu/%zu · Level %zu/%zu · Drop-Items in Zielen %zu/%zu\n",
                descIsTitlePlusOne, predKnown, withPred, levelOk, withLevel, dropInObjectives, drops);
    std::printf("         Ziele: %zu NPC (Anzahl 0), %zu Monster\n", npcTargets, killTargets);
    Check(descIsTitlePlusOne == 2295, "Beschreibung = Titel + 1 bei 2295 Quests");
    Check(withPred == 1390 && predKnown == withPred, "alle 1390 Vorgänger sind vorhandene Quest-IDs");
    Check(withLevel == 2099 && levelOk == withLevel, "alle 2099 Levelbereiche: min <= max");
    Check(drops == 960 && dropInObjectives == 941, "941 von 960 Drop-Items sind auch Item-Ziel der Quest");
    Check(padsZero, "Füllfelder nach id und unk2 sowie unk4 überall 0");
    Check(freeSlotsZero, "freie Drop-Slots überall 0");
    Check(isMobZero && itemTypeZero, "isMob und Item-Ziel-Typ überall 0");
    if (const auto* q40 = FindQuest(f, 40)) {
        Check(q40->items[0].active == 1 && q40->items[0].id == 3108 && q40->items[0].amount == 1 &&
              q40->drops.size() == 3 && q40->drops[0].itemId == 3108 && q40->drops[0].mobId == 13 &&
              q40->drops[0].rate == 400000,
              "Quest 40: Item-Ziel 3108 ×1, drei Drops von Mob 13/11/12 mit Rate 400000");
    } else Check(false, "Quest 40 vorhanden");

    // --- Belohnungen ---
    bool rewardPad = true, unusedZero = true, useOk = true, typeOk = true, highZero = true;
    std::size_t used = 0, exp = 0, money = 0, items = 0, type4 = 0;
    for (const auto& q : f.records)
        for (const auto& e : q.rewards) {
            rewardPad = rewardPad && e.pad == 0;
            highZero = highZero && (e.value >> 32) == 0;
            if (e.use == 0) { unusedZero = unusedZero && e.Empty(); continue; }
            ++used;
            useOk = useOk && (e.use == 1 || e.use == 2);
            switch (e.type) {
            case 0: ++exp; break;
            case 1: ++money; break;
            case 2: ++items; break;
            case 4: ++type4; break;
            default: typeOk = false;
            }
        }
    std::printf("         %zu Belohnungen: %zu EXP, %zu Geld, %zu Items, %zu Typ 4\n", used, exp, money, items, type4);
    Check(rewardPad && unusedZero && useOk && typeOk && highZero,
          "Belohnungen: pad 0, unbelegte Einträge 0, use 1/2, type 0/1/2/4, obere 4 Byte 0");
    Check(used == 5161 && items == 893 && type4 == 17, "5161 Belohnungen, davon 893 Items und 17 vom Typ 4");
    if (const auto* q251 = FindQuest(f, 251)) {
        const auto& r = q251->rewards;
        Check(r[0].use == 1 && r[0].type == 0 && r[0].value == 6200 && r[1].use == 1 && r[1].type == 1 &&
              r[1].value == 4400 && r[5].use == 2 && r[5].type == 2 && r[5].ItemId() == 13 && r[5].ItemCount() == 1,
              "Quest 251: 6200 EXP, 4400 Geld, Auswahl-Item 13 ×1");
    } else Check(false, "Quest 251 vorhanden");
    if (const auto* q7 = FindQuest(f, 7)) {
        std::vector<std::uint16_t> choice;
        for (const auto& e : q7->rewards)
            if (e.use == 2 && e.type == 2 && e.ItemCount() == 1) choice.push_back(e.ItemId());
        Check(choice == std::vector<std::uint16_t>{0, 500, 1000, 1500, 57390}, "Quest 7: Auswahl-Items 0/500/1000/1500/57390");
    }

    // --- Belohnung bearbeiten, speichern, neu laden ---
    const auto tmp = std::filesystem::temp_directory_path();
    auto edited = f;
    QuestRecord* q251 = nullptr;
    for (auto& q : edited.records) if (q.id == 251) q251 = &q;
    if (q251) {
        q251->rewards[5].SetItem(13, 7);
        q251->rewards[11] = QuestRewardEntry{1, 1, 0, 12345};
        q251->drops.push_back(QuestDrop{1, 6, 1, 7, 100, 1, 1, 0}); // 10 -> 11 nicht erlaubt
        Check(!SaveQuestData(edited, tmp / "nextgen_quest_layout_bad.shn").has_value(), "11 Drops werden abgelehnt");
        q251->drops.pop_back();
        q251->drops.erase(q251->drops.begin() + 2, q251->drops.end()); // 10 -> 2 Drops
        const auto out = tmp / "nextgen_quest_layout_edit.shn";
        Check(SaveQuestData(edited, out).has_value(), "bearbeitete Datei gespeichert");
        const auto re = LoadQuestData(out);
        Check(re.has_value() && re->records.size() == f.records.size(), "bearbeitete Datei neu geladen");
        if (re) {
            const auto* r = FindQuest(*re, 251);
            Check(r->rewards[5].ItemCount() == 7 && r->rewards[5].ItemId() == 13, "geänderte Item-Anzahl kommt an");
            Check(r->rewards[11].type == 1 && r->rewards[11].value == 12345, "neuer Geld-Eintrag kommt an");
            Check(r->drops.size() == 2 && r->unusedDropSlots.size() == 8 * kQuestDropBytes && AllZero(r->unusedDropSlots) &&
                  r->rewards[0].value == 6200 && r->title == FindQuest(f, 251)->title,
                  "Drops 10 -> 2: freie Slots 0, Belohnungen und Kopf unverändert");
            bool othersSame = true;
            for (std::size_t i = 0; i < f.records.size(); ++i)
                if (f.records[i].id != 251)
                    othersSame = othersSame && re->records[i].dataLen == f.records[i].dataLen &&
                                 re->records[i].title == f.records[i].title;
            Check(othersSame, "alle anderen Quests unverändert");

            auto restored = *re;
            for (auto& q : restored.records) if (q.id == 251) q = *FindQuest(f, 251);
            const auto out2 = tmp / "nextgen_quest_layout_restored.shn";
            Check(SaveQuestData(restored, out2).has_value() && ReadAll(out2) == ReadAll(source),
                  "Original zurück -> Datei bytegleich zum Original");
            std::filesystem::remove(out2);
        }
        std::filesystem::remove(out);
    } else Check(false, "Quest 251 bearbeitbar");

    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle QuestData-Layout-Tests bestanden.\n");
    return 0;
}
