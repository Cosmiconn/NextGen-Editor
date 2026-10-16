// test_new_map.cpp
// Neue Karte aus Vorlage (echte NA2016-Karte Rou): anlegen, im Original-Format speichern, wieder
// öffnen und prüfen, dass alle Teile zueinander passen.

#include "mapeditor/core/legacy/NewMapFromTemplate.hpp"
#include "mapeditor/core/LevelEditorTools.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace theseed::mapeditor::core;
using namespace theseed::mapeditor::core::legacy;

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

int main(int argc, char** argv) {
    Check(IsValidNewMapName("MyField01"), "gültiger Name");
    Check(!IsValidNewMapName("1abc") && !IsValidNewMapName("a/b") && !IsValidNewMapName("") &&
              !IsValidNewMapName("ABCDEFGHIJKLMNOP"), "ungültige Namen abgelehnt");
    if (argc < 2) {
        std::fprintf(stderr, "Aufruf: test_new_map <Rou.ini>\n");
        return 1;
    }
    auto tmpl = OpenLegacyMap(argv[1]);
    Check(tmpl.has_value(), "Vorlage Rou geladen");
    if (!tmpl) return 1;

    Check(!CreateMapFromTemplate(*tmpl, {"Bad/Name", 129, 0.0f}).has_value(), "ungültiger Name -> Fehler");
    auto created = CreateMapFromTemplate(*tmpl, {"TestField", 129, 120.0f});
    Check(created.has_value(), "Karte aus Vorlage angelegt");
    if (!created) return 1;
    Check(created->textureStack.LayerCount() == tmpl->textureStack.LayerCount(), "gleiche Layer wie die Vorlage");
    Check(created->textureStack.Layer(0).diffuseFileName == tmpl->textureStack.Layer(0).diffuseFileName,
          "Diffuse-Texturen der Vorlage (existieren im Client)");
    Check(created->walkGrid.Rows() == 128 * 8 && created->walkGrid.Cols() >= 128 * 8, "SHBD: Blöcke * 8 Zeilen");
    Check(created->objects.Count() == 0, "keine Objekte");

    const auto root = std::filesystem::temp_directory_path() / "nextgen_newmap_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    const auto outDir = root / "Client" / "resmap" / "field" / "TestField";
    auto saved = SaveLegacyMap(*created, outDir, "TestField");
    Check(saved.has_value(), "im Original-Format gespeichert");
    if (!saved) std::fprintf(stderr, "         %s\n", saved.error().c_str());

    Check(std::filesystem::exists(outDir / "TestField.ini") && std::filesystem::exists(outDir / "TestField.HTD") &&
              std::filesystem::exists(outDir / "TestField.shbd") && std::filesystem::exists(outDir / "TestField.shmd"),
          "INI, HTD, SHBD und SHMD geschrieben");
    Check(std::filesystem::exists(outDir / "TestField_1.bmp"), "Masken im Kartenordner");
    Check(std::filesystem::file_size(outDir / "TestField_VertexColor.bmp") == 54 + 129u * ((129u * 3 + 3) & ~3u),
          "weiße Vertex-Color-Bitmap 129x129x24");
    Check(std::filesystem::file_size(outDir / "TestField.HTD") == 4 + 129u * 129u * 4u, "HTD-Größe 4 + 129*129*4");
    Check(std::filesystem::file_size(outDir / "TestField.shbd") == 8 + 1024u * 1024u / 8u, "SHBD-Größe 8 + 1024*1024/8");

    auto header = PeekLegacyShbdHeader(outDir / "TestField.shbd");
    Check(header && header->quadCount == 128 && header->height == 1024, "SHBD-Kopf (Blöcke, Zeilen) wie im Original");

    auto reopened = OpenLegacyMap(outDir / "TestField.ini");
    Check(reopened.has_value(), "neue Karte wieder geöffnet");
    if (reopened) {
        Check(reopened->hasHeightmap && reopened->heightmap.Width() == 129 && reopened->heightmap.At(64, 64) == 120.0f,
              "Heightmap 129x129, Höhe 120");
        Check(reopened->textureStack.LayerCount() == tmpl->textureStack.LayerCount(), "Layer wieder gelesen");
        Check(reopened->textureStack.Layer(0).blend.At(10, 10) > 0.99f &&
                  (reopened->textureStack.LayerCount() < 2 || reopened->textureStack.Layer(1).blend.At(10, 10) < 0.01f),
              "Layer 1 voll, Layer 2 leer");
        Check(reopened->hasWalkGrid && reopened->walkGrid.Rows() == 1024 &&
                  !level::WalkBlockedAtWorld(reopened->walkGrid, 3200.0f, 3200.0f),
              "SHBD gelesen, Kartenmitte begehbar");
        Check(reopened->hasObjects && reopened->objects.Count() == 0, "SHMD ohne Objekte gelesen");
        Check(reopened->ini.heightFileName == ".\\resmap\\field\\TestField\\TestField.HTD", "HTD-Pfad relativ zum Client");
        Check(reopened->ini.vertexColorTexture == ".\\resmap\\field\\TestField\\TestField_VertexColor.bmp", "Vertex-Color-Pfad");
    }
    std::filesystem::remove_all(root, ec);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
