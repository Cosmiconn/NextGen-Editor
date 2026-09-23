#include "mapeditor/core/legacy/QuestData.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

using namespace theseed::mapeditor::core::legacy;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: test_questdata QuestData.shn (Client oder Server)\n"; return 2; }
    auto loaded = LoadQuestData(argv[1]);
    assert(loaded);
    auto& f = *loaded;
    assert(f.header == 0x0006);
    assert(!f.records.empty());
    for (auto& q : f.records) {
        assert(q.mobs.size() == 5);
        assert(q.items.size() == 10);
        assert(q.itemDropPadding.size() == 28u * (11u - q.drops.size()) + 12u);
        assert(q.rewardsRaw.size() == 144);
    }

    // Unveraendertes Speichern muss byte-identisch zum Original sein.
    auto out1 = std::filesystem::temp_directory_path() / "nextgen_questdata_roundtrip.shn";
    auto saved1 = SaveQuestData(f, out1);
    assert(saved1);
    {
        std::ifstream a(argv[1], std::ios::binary), b(out1, std::ios::binary);
        std::vector<char> ba((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
        std::vector<char> bb((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
        assert(ba == bb && "unveraendertes Speichern muss byte-identisch zum Original sein");
    }

    // Skript bearbeiten, speichern, neu laden - Aenderung muss ankommen, alle anderen Quests
    // unveraendert bleiben.
    auto edited = f;
    edited.records[0].start.text += "\r\n; test_questdata Marker";
    auto out2 = std::filesystem::temp_directory_path() / "nextgen_questdata_edit.shn";
    auto saved2 = SaveQuestData(edited, out2);
    assert(saved2);
    auto reloaded = LoadQuestData(out2);
    assert(reloaded);
    assert(reloaded->records.size() == f.records.size());
    assert(reloaded->records[0].start.text == edited.records[0].start.text);
    for (std::size_t i = 1; i < f.records.size(); ++i) {
        assert(reloaded->records[i].id == f.records[i].id);
        assert(reloaded->records[i].start.text == f.records[i].start.text);
        assert(reloaded->records[i].action.text == f.records[i].action.text);
        assert(reloaded->records[i].finish.text == f.records[i].finish.text);
    }

    std::filesystem::remove(out1);
    std::filesystem::remove(out2);
    std::cout << "0 Fehler.\n";
    return 0;
}
