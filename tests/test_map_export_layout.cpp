// test_map_export_layout.cpp
// Kartenexport im echten Client-Layout mit den NA2016-Roumen-Dateien (Fixtures):
//   Quelle  <tmp>/src/Client/resmap/field/Rou/Rou.ini   (read-only behandelt)
//   Projekt <tmp>/proj/Client/resmap/field/Rou/...
// Prüft: unverändertes Speichern ist für alle geschriebenen Dateien bytegleich, Ini-Pfade
// bleiben client-relativ (".\resmap\field\Rou\Rou.HTD"), Blend-BMPs landen an ihrem
// client-relativen Ziel (".\resmap\fieldtexture\..." -> resmap/fieldtexture), eine bemalte
// Maske ändert nur die bemalten Pixel und behält Header/Füllbytes/Nicht-Grau-Pixel, und eine
// beim Laden fehlende, unbemalte Maske wird nicht als leere Datei angelegt.

#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/LegacyMapProject.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}
std::vector<char> Bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path fixtures = argc > 1 ? argv[1] : "tests/fixtures";
    const auto root = std::filesystem::temp_directory_path() / "nextgen_map_export_layout";
    std::filesystem::remove_all(root);
    const auto srcMap = root / "src" / "Client" / "resmap" / "field" / "Rou";
    const auto outMap = root / "proj" / "Client" / "resmap" / "field" / "Rou";
    std::filesystem::create_directories(srcMap);
    for (const char* f : {"Rou.HTD", "Rou.HTDG", "Rou.aid", "Rou.conf", "Rou.idm", "Rou.ini", "Rou.shbd", "Rou.shmd"})
        std::filesystem::copy_file(fixtures / f, srcMap / f);
    // Ini schreibt die Masken als "block.Bmp" usw.; die Fixtures heißen block.BMP.
    for (const char* f : {"block", "rock", "grass"})
        std::filesystem::copy_file(fixtures / (std::string(f) + ".BMP"), srcMap / (std::string(f) + ".Bmp"));

    legacy::LegacyMapOpenReport report;
    auto opened = legacy::OpenLegacyMap(srcMap / "Rou.ini", &report);
    Check(opened.has_value(), "Roumen aus Client-Layout geöffnet");
    if (!opened) return 1;
    auto project = *opened;
    Check(project.textureStack.LayerCount() == 4, "4 Textur-Layer (L1_A fehlt in den Fixtures)");

    // --- unverändert speichern ---
    auto saved = legacy::SaveLegacyMap(project, outMap, "Rou");
    Check(saved.has_value(), "unverändert gespeichert");
    for (const char* f : {"Rou.HTD", "Rou.HTDG", "Rou.aid", "Rou.conf", "Rou.idm", "Rou.ini", "Rou.shbd", "Rou.shmd",
                          "block.Bmp", "rock.Bmp", "grass.Bmp"})
        Check(std::filesystem::exists(outMap / f) && Bytes(outMap / f) == Bytes(srcMap / f),
              std::string("bytegleich am Client-Pfad resmap/field/Rou/") + f);
    Check(!std::filesystem::exists(outMap / "field"), "kein falsch verschachtelter Ordner resmap/field/Rou/field/");
    Check(!std::filesystem::exists(root / "proj" / "Client" / "resmap" / "fieldtexture" / "L1_A.BMP") &&
          !std::filesystem::exists(outMap / "fieldtexture"),
          "fehlende, unbemalte Maske L1_A.BMP wird nicht als leere Datei angelegt");

    // --- Maske bemalen ---
    std::size_t blockLayer = 99;
    for (std::size_t i = 0; i < project.ini.layers.size(); ++i)
        if (project.ini.layers[i].blendFileName.find("block") != std::string::npos) blockLayer = i;
    Check(blockLayer < project.textureStack.LayerCount(), "Layer mit block.Bmp gefunden");
    if (blockLayer < project.textureStack.LayerCount()) {
        auto& blend = project.textureStack.Layer(blockLayer).blend;
        for (std::uint32_t z = 10; z < 20; ++z)
            for (std::uint32_t x = 30; x < 40; ++x) blend.Set(x, z, 1.0f);
        saved = legacy::SaveLegacyMap(project, outMap, "Rou");
        Check(saved.has_value(), "bemalt gespeichert");
        const auto a = Bytes(srcMap / "block.Bmp"), b = Bytes(outMap / "block.Bmp");
        Check(a.size() == b.size() && std::equal(a.begin(), a.begin() + 54, b.begin()),
              "bemalte Maske: Grösse und Header unverändert (inkl. 2 Füllbytes am Ende)");
        std::size_t changedBytes = 0, outside = 0;
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
            if (a[i] == b[i]) continue;
            ++changedBytes;
            if (i < 54) { ++outside; continue; }
            const std::size_t px = (i - 54) / 3, x = px % 256, y = px / 256;
            if (!(x >= 30 && x < 40 && y >= 10 && y < 20)) ++outside;
        }
        std::printf("         %zu Bytes geändert\n", changedBytes);
        Check(changedBytes > 0 && outside == 0, "nur Bytes der bemalten Pixel geändert");
        auto reread = legacy::ReadBlendMapBmp(outMap / "block.Bmp");
        Check(reread && reread->At(35, 15) == 1.0f, "bemalter Wert wird wieder gelesen");
        for (const char* f : {"rock.Bmp", "grass.Bmp", "Rou.ini"})
            Check(Bytes(outMap / f) == Bytes(srcMap / f), std::string("andere Dateien weiter bytegleich: ") + f);
        auto reopened = legacy::OpenLegacyMap(outMap / "Rou.ini");
        Check(reopened && reopened->textureStack.Layer(blockLayer).blend.At(35, 15) == 1.0f,
              "Projektkarte öffnet die bemalte Maske vom Client-Pfad");
    }
    // --- Ini-Wert ändern: nur diese Zeile darf sich ändern (Leerzeilen bleiben an ihrer Stelle) ---
    {
        auto edited = *legacy::OpenLegacyMap(srcMap / "Rou.ini");
        edited.textureStack.Layer(1).uvScaleDiffuse = 7.0f;
        const auto iniOut = root / "proj2" / "Client" / "resmap" / "field" / "Rou";
        Check(legacy::SaveLegacyMap(edited, iniOut, "Rou").has_value(), "Ini mit geändertem UVScaleDiffuse gespeichert");
        auto lines = [](const std::filesystem::path& p) {
            std::vector<std::string> out; std::ifstream in(p, std::ios::binary); std::string l;
            while (std::getline(in, l)) out.push_back(l);
            return out;
        };
        const auto before = lines(srcMap / "Rou.ini"), after = lines(iniOut / "Rou.ini");
        std::size_t differing = 0;
        for (std::size_t i = 0; i < before.size() && i < after.size(); ++i) if (before[i] != after[i]) ++differing;
        Check(before.size() == after.size() && differing == 1, "Ini: gleiche Zeilenzahl, genau eine Zeile geändert");
        bool heightKept = false;
        for (const auto& l : after) if (l.find("#HeightFileName") != std::string::npos && l.find(".\\resmap\\field\\Rou\\Rou.HTD") != std::string::npos) heightKept = true;
        Check(heightKept, "Ini: #HeightFileName bleibt client-relativ (.\\resmap\\field\\Rou\\Rou.HTD)");
    }

    std::filesystem::remove_all(root);
    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle Kartenexport-Layout-Tests bestanden.\n");
    return 0;
}
