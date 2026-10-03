// test_autosave.cpp
// Automatische Sicherung: Ablageort außerhalb der Projektausgabe, autosave.txt hin und zurück,
// Löschen nur im Sicherungsordner, Fälligkeit.

#include "mapeditor/core/Autosave.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace theseed::mapeditor::core;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what);
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what);
    }
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "nextgen_autosave_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto project = root / "Projekt";

    const auto dir = AutosaveMapDir(project, "Rou");
    Check(dir == project / "Autosave" / "Client" / "resmap" / "field" / "Rou", "Ablage unter <Projekt>/Autosave");
    Check(dir.string().find((project / "Client").string() + "/") != 0, "nicht in der Projektausgabe <Projekt>/Client");

    Check(!ReadAutosaveMeta(dir).has_value(), "ohne Sicherung keine Metadaten");
    AutosaveMeta meta{"/client/resmap/field/Rou/Rou.ini", "Rou", 1790000000};
    Check(WriteAutosaveMeta(dir, meta).has_value(), "autosave.txt geschrieben");
    const auto read = ReadAutosaveMeta(dir);
    Check(read && read->sourceIni == meta.sourceIni && read->mapStem == "Rou" && read->savedAtUnix == meta.savedAtUnix,
          "autosave.txt gelesen");
    Check(!std::filesystem::exists(dir / "autosave.txt.tmp"), "keine Zwischendatei zurückgelassen");

    // Projektausgabe daneben darf vom Löschen nicht berührt werden.
    std::filesystem::create_directories(project / "Client" / "resmap" / "field" / "Rou");
    { std::ofstream(project / "Client" / "resmap" / "field" / "Rou" / "Rou.ini") << "x"; }
    RemoveAutosave(project, "../Client");
    RemoveAutosave(project, "Rou");
    Check(!std::filesystem::exists(dir), "Sicherung entfernt");
    Check(std::filesystem::exists(project / "Client" / "resmap" / "field" / "Rou" / "Rou.ini"), "Projektausgabe unberührt");

    Check(!AutosaveDue(true, false, 1000.0, 0.0, 300.0), "ohne Änderungen nicht fällig");
    Check(!AutosaveDue(true, true, 200.0, 0.0, 300.0), "vor Ablauf nicht fällig");
    Check(AutosaveDue(true, true, 300.0, 0.0, 300.0), "nach Ablauf fällig");
    Check(!AutosaveDue(false, true, 1000.0, 0.0, 300.0), "abgeschaltet nicht fällig");

    std::filesystem::remove_all(root, ec);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
