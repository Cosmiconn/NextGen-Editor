// test_mob_attack_sequence.cpp
// Angriffsfolgen (MobAttackSequence/<Mob>.txt): Lesen, Einfügen/Löschen mit Neunummerierung,
// neue SkillChange-Tabelle, Kommentare und unveränderte Zeilen bleiben erhalten. Optional:
// Verzeichnis mit echten Dateien - unverändertes Anwenden muss jede Datei bytegleich lassen.

#include "mapeditor/core/legacy/MobAttackSequence.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace theseed::mapeditor::core::legacy;

namespace {

void Require(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "[FEHLER] %s\n", what);
    std::exit(1);
}

std::string ReadAll(const std::filesystem::path& p) {
    std::string out;
    {
        std::ifstream in(p, std::ios::binary);
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return out;
}

void WriteAll(const std::filesystem::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << text;
}

const char* kSample =
    ";\t\tMob Attack Sequence\t\t\t\r\n"
    "#Table\tAttSeq\t\t\t\t\r\n"
    "#ColumnType\tWORD\tSTRING[33]\t\t\t\r\n"
    "#ColumnName\tOrder\tAttack\t\t\t\r\n"
    "#Record\t0\t-\t\t\t\r\n"
    "#Record\t1\tBoss_Skill01\t\t\t; erster Angriff\r\n"
    "#Record\t2\tBoss_Skill02\t\t\t\r\n"
    "\t\t\t\t\t\r\n"
    "#End\r\n";

void Synthetic() {
    const auto dir = std::filesystem::temp_directory_path() / "test_mob_attack_sequence";
    std::filesystem::create_directories(dir);
    const auto path = dir / "Boss.txt";
    WriteAll(path, kSample);

    auto file = LoadShineTextFile(path);
    Require(file.has_value(), "Datei lädt");
    auto seq = ReadAttackSequence(*file);
    Require(seq.has_value() && seq->attacks.size() == 3 && seq->attacks[1] == "Boss_Skill01" && seq->ordersSequential,
            "drei Schritte gelesen");
    Require(!seq->hasRuleTable && seq->rules.empty(), "keine Regeltabelle");

    // Unverändert anwenden: Datei bleibt bytegleich.
    Require(ApplyAttackSequence(*file, *seq).has_value(), "unverändert anwenden");
    const auto same = dir / "same.txt";
    Require(SaveShineTextFile(*file, same).has_value() && ReadAll(same) == kSample, "unverändert bytegleich");

    // Einfügen in der Mitte, letzten Schritt löschen, Regel hinzufügen.
    seq->attacks.insert(seq->attacks.begin() + 1, kAttackRepeatMarker);
    seq->attacks.push_back("Boss_Skill03");
    seq->attacks.erase(seq->attacks.begin()); // "-" am Anfang weg
    seq->rules.push_back({"HPLow_ChangeOrder", "400", "0", "Boss_Skill04"});
    Require(ApplyAttackSequence(*file, *seq).has_value(), "geänderte Folge anwenden");
    const auto changed = dir / "changed.txt";
    Require(SaveShineTextFile(*file, changed).has_value(), "geänderte Datei gespeichert");

    auto reloaded = LoadShineTextFile(changed);
    Require(reloaded.has_value(), "geänderte Datei lädt");
    auto again = ReadAttackSequence(*reloaded);
    Require(again.has_value() && again->ordersSequential, "Order nach dem Speichern 0..n-1");
    Require(again->attacks == std::vector<std::string>({"*RepeatStart", "Boss_Skill01", "Boss_Skill02", "Boss_Skill03"}),
            "Schritte in neuer Reihenfolge");
    Require(again->hasRuleTable && again->rules.size() == 1 && again->rules[0].to == "Boss_Skill04" &&
                again->rules[0].value == "400",
            "neue SkillChange-Tabelle mit Regel");
    const std::string text = ReadAll(changed);
    Require(text.find("; erster Angriff") != std::string::npos, "Kommentar bleibt erhalten");
    Require(text.find(";\t\tMob Attack Sequence") == 0, "Dateikopf bleibt erhalten");

    // Regel wieder entfernen: keine leere neue Tabelle.
    auto file2 = LoadShineTextFile(path);
    auto seq2 = ReadAttackSequence(*file2);
    seq2->rules.push_back({"OutOfRange", "0", "-", "Boss_Skill01"});
    Require(ApplyAttackSequence(*file2, *seq2).has_value(), "Regel anlegen");
    seq2->rules.clear();
    Require(ApplyAttackSequence(*file2, *seq2).has_value(), "Regel wieder entfernen");
    Require(file2->FindTable("SkillChange") == nullptr, "leere neue Regeltabelle entfällt");

    // Ungültige Werte werden abgelehnt.
    seq2->attacks.push_back("");
    Require(!ApplyAttackSequence(*file2, *seq2).has_value(), "leerer Angriff abgelehnt");

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void RealFiles(const std::filesystem::path& dir) {
    std::size_t files = 0, unchanged = 0, renumbered = 0;
    const auto out = std::filesystem::temp_directory_path() / "test_mob_attack_sequence_real.txt";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".txt") continue;
        ++files;
        auto file = LoadShineTextFile(entry.path());
        Require(file.has_value(), "echte Datei lädt");
        auto seq = ReadAttackSequence(*file);
        Require(seq.has_value(), "echte Datei hat AttSeq");
        if (!seq->ordersSequential) { ++renumbered; continue; }
        Require(ApplyAttackSequence(*file, *seq).has_value(), "echte Datei unverändert anwenden");
        Require(SaveShineTextFile(*file, out).has_value(), "echte Datei speichern");
        if (ReadAll(out) == ReadAll(entry.path())) ++unchanged;
        else std::fprintf(stderr, "abweichend: %s\n", entry.path().filename().string().c_str());
    }
    std::printf("%zu Dateien, %zu bytegleich, %zu mit abweichender Order (übersprungen)\n", files, unchanged, renumbered);
    Require(unchanged + renumbered == files, "alle echten Dateien bleiben unverändert bytegleich");
    std::error_code ec;
    std::filesystem::remove(out, ec);
}

} // namespace

int main(int argc, char** argv) {
    Synthetic();
    if (argc > 1) RealFiles(argv[1]);
    std::printf("Angriffsfolgen: ok\n");
    return 0;
}
