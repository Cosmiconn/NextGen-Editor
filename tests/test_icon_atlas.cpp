// test_icon_atlas.cpp
// Item-/Skill-Icon-Atlanten: IconIndex -> Zelle, Pfade, DDS-Größe. Optional mit den NA2016-Daten
// (Client/resmenu/Icon/ItemEqu000.dds): Größe 256x256 und Zellen des Original-Atlas.

#include "mapeditor/core/DdsWrite.hpp"
#include "mapeditor/core/legacy/IconAtlas.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace theseed::mapeditor::core;
using namespace theseed::mapeditor::core::legacy;

namespace {
int g_failures = 0;
void Check(bool condition, const std::string& what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what.c_str());
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what.c_str());
    }
}
bool Near(float a, float b) { return std::abs(a - b) < 1e-6f; }
} // namespace

int main(int argc, char** argv) {
    const auto c1 = IconAtlasCell(256, 256, 1);
    Check(c1.valid && Near(c1.u0, 32 / 256.0f) && Near(c1.v0, 0) && Near(c1.u1, 64 / 256.0f) && Near(c1.v1, 32 / 256.0f),
          "Index 1 = zweite Zelle der ersten Zeile (LeatherBoots in ItemEqu000)");
    const auto c8 = IconAtlasCell(256, 256, 8);
    Check(c8.valid && Near(c8.u0, 0) && Near(c8.v0, 32 / 256.0f), "Index 8 = erste Zelle der zweiten Zeile");
    const auto c63 = IconAtlasCell(256, 256, 63);
    Check(c63.valid && Near(c63.u1, 1) && Near(c63.v1, 1), "Index 63 = letzte Zelle");
    Check(!IconAtlasCell(256, 256, 64).valid && !IconAtlasCell(256, 256, -1).valid, "Außerhalb des Atlas ungültig");
    Check(IconAtlasCapacity(256, 256) == 64 && IconAtlasCapacity(128, 128) == 16, "Kapazität");
    Check(IconAtlasRelativePath("ItemEqu000") == "Icon/ItemEqu000.dds" && IconAtlasRelativePath("-").empty(), "Pfad relativ zu resmenu");

    const auto tmp = std::filesystem::temp_directory_path() / "nextgen_icon_atlas_test.dds";
    std::vector<std::uint8_t> img(64 * 32 * 4, 200);
    (void)WriteDdsFile(tmp, img, 64, 32, DdsFormat::Dxt3, false);
    const auto size = ReadDdsSize(tmp);
    Check(size && size->first == 64 && size->second == 32, "DDS-Größe aus dem Kopf (64x32)");
    std::filesystem::remove(tmp);
    Check(!ReadDdsSize(tmp).has_value(), "Fehlende Datei -> keine Größe");

    if (argc > 1 && argv[1][0] != '\0') {
        const auto atlas = std::filesystem::path(argv[1]) / "Client" / "resmenu" / "Icon" / "ItemEqu000.dds";
        if (std::filesystem::exists(atlas)) {
            const auto s = ReadDdsSize(atlas);
            Check(s && s->first == 256 && s->second == 256 && IconAtlasCapacity(s->first, s->second) == 64,
                  "Original ItemEqu000.dds: 256x256, 64 Icons");
        } else {
            std::printf("[skip]   %s fehlt\n", atlas.string().c_str());
        }
    }
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
