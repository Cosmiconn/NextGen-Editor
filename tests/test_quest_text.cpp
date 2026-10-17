// test_quest_text.cpp
// SAY-Verweise in Quest-Skripten, freie Text-ID-Blöcke und das Umnummerieren der Texte einer
// geklonten Quest. Mit Pfad zu QuestData.shn: zusätzlich die gemessenen Zahlen der Fixture.

#include "mapeditor/core/legacy/QuestText.hpp"

#include <cstdio>

using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}

void Synthetic() {
    const std::string script = "SAY 202 NPC\r\n  SAY\t203 ME\r\n; SAY 999 NPC\r\nIF RESULT == 1 GOTO MARK1\r\nSAYX 5\r\nSAY 7x\r\nSAY 204, NPC";
    const auto refs = FindSayReferences(script);
    Check(refs.size() == 3 && refs[0].id == 202 && refs[1].id == 203 && refs[2].id == 204,
          "SAY am Zeilenanfang (eingerückt, mit Komma), Kommentar und ungültige Tokens ignoriert");
    Check(script.substr(refs[1].offset, refs[1].length) == "203", "Position der Ziffern");

    const std::string remapped = RemapSayIds(script, {{202, 9000}, {204, 9002}});
    Check(remapped == "SAY 9000 NPC\r\n  SAY\t203 ME\r\n; SAY 999 NPC\r\nIF RESULT == 1 GOTO MARK1\r\nSAYX 5\r\nSAY 7x\r\nSAY 9002, NPC",
          "nur gemappte SAY-IDs ersetzt, Rest unverändert");
    Check(RemapSayIds(script, {}) == script, "leere Zuordnung lässt den Text unverändert");

    std::set<std::uint32_t> used = {1, 2, 3, 7, 8, 20};
    Check(FindFreeTextBlock(used, 3, 1) == 4u, "erster freier Block der Länge 3 ab 1 ist 4");
    Check(FindFreeTextBlock(used, 4, 1) == 9u, "Block der Länge 4 passt erst ab 9");
    Check(FindFreeTextBlock(used, 2, 21) == 21u, "hinter der höchsten ID");
    Check(!FindFreeTextBlock(used, 5, 18, 22).has_value(), "Obergrenze wird beachtet");

    QuestRecord q;
    q.title = 200;
    q.description = 201;
    q.start.text = "SAY 202 NPC\nSAY 201 NPC\nACCEPT\nEND";
    q.action.text = "SAY 205 NPC\nEND";
    q.finish.text = "SAY 204 ME\nDONE\nEND";
    Check(QuestTextIds(q) == std::vector<std::uint32_t>{200, 201, 202, 205, 204}, "Text-IDs ohne Duplikate in fester Reihenfolge");
    std::set<std::uint32_t> dialog = {0, 200, 201, 202, 203, 204, 205, 300};
    const auto remap = AssignNewQuestTextIds(q, dialog);
    Check(remap.size() == 5 && remap.at(200) == 301 && remap.at(201) == 302 && remap.at(204) == 305,
          "neuer Block hinter der höchsten belegten ID");
    Check(q.title == 301 && q.description == 302, "Titel und Beschreibung umgestellt (Beschreibung = Titel + 1 bleibt)");
    Check(q.start.text == "SAY 303 NPC\nSAY 302 NPC\nACCEPT\nEND" && q.action.text == "SAY 304 NPC\nEND" &&
          q.finish.text == "SAY 305 ME\nDONE\nEND", "SAY-Verweise aller drei Skripte umgestellt");

    QuestRecord empty;
    Check(AssignNewQuestTextIds(empty, dialog).empty(), "Quest ohne Texte bleibt unverändert");
}

void Fixture(const char* path) {
    auto file = LoadQuestData(path);
    Check(file.has_value(), "QuestData.shn geladen");
    if (!file) return;
    std::size_t says = 0, descPlusOne = 0, unchanged = 0;
    for (const auto& q : file->records) {
        if (q.description == q.title + 1) ++descPlusOne;
        bool same = true;
        for (const auto* s : {&q.start, &q.action, &q.finish}) {
            says += FindSayReferences(s->text).size();
            std::map<std::uint32_t, std::uint32_t> identity;
            for (const auto& r : FindSayReferences(s->text)) identity[r.id] = r.id;
            same = same && RemapSayIds(s->text, identity) == s->text;
        }
        if (same) ++unchanged;
    }
    std::printf("         %zu Quests, %zu SAY-Verweise, Beschreibung = Titel + 1 bei %zu\n", file->records.size(), says, descPlusOne);
    Check(file->records.size() == 2304 && says == 19924, "Fixture: 2304 Quests mit 19924 SAY-Verweisen");
    Check(descPlusOne == 2295, "Fixture: Beschreibung = Titel + 1 bei 2295 Quests");
    Check(unchanged == file->records.size(), "Identitäts-Zuordnung lässt jedes Skript byte-gleich");
}
} // namespace

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) Fixture(argv[1]);
    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle Quest-Text-Tests bestanden.\n");
    return 0;
}
