// test_object_placement.cpp
// GUI-freier Test. Prüft ObjectPlacementSet-Grundfunktionen, .tsobj-Roundtrip, und den
// Legacy-Import/Export für .shmd, .idm und .aid - jeweils Byte-für-Byte gegen die echten
// Referenzdateien verglichen.

#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

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

std::vector<char> ReadAllBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void TestBasicObjectPlacement() {
    ObjectPlacementSet set;
    PlacedObject obj;
    obj.modelPath = "test.nif";
    obj.posX = 1.0f; obj.posY = 2.0f; obj.posZ = 3.0f;
    const auto idx = set.AddObject(obj);
    Check(set.Count() == 1, "AddObject erh\u00f6ht Count");
    Check(set.At(idx).modelPath == "test.nif", "At() liefert korrektes Objekt");
    set.RemoveObject(idx);
    Check(set.Count() == 0, "RemoveObject entfernt Objekt");
}

void TestTsObjRoundtrip() {
    ObjectPlacementSet set;
    ObjectCategoryList sky;
    sky.name = "Sky";
    sky.modelPaths.push_back("resmap\\nifs\\Common\\field_sky_01.nif");
    set.categories.push_back(sky);
    set.environment.frustumFar = 5000.0f;

    PlacedObject obj;
    obj.modelPath = "GuildHall.nif";
    obj.posX = 6769.816406f; obj.posY = 4131.159668f; obj.posZ = 480.796509f;
    obj.rotZ = -0.493918f; obj.rotW = 0.869508f;
    set.AddObject(obj);

    const auto path = std::filesystem::temp_directory_path() / "map_editor_tsobj_test.tsobj";
    auto saveResult = SaveTsObj(set, path);
    Check(saveResult.has_value(), "SaveTsObj erfolgreich");

    auto loadResult = LoadTsObj(path);
    Check(loadResult.has_value(), "LoadTsObj erfolgreich");
    if (loadResult) {
        Check(loadResult->categories.size() == 1 && loadResult->categories[0].name == "Sky", "Kategorie nach Roundtrip identisch");
        Check(loadResult->Count() == 1, "Objekt-Anzahl nach Roundtrip identisch");
        Check(loadResult->At(0).modelPath == "GuildHall.nif", "Modellpfad nach Roundtrip identisch");
        Check(std::abs(loadResult->At(0).posX - 6769.816406f) < 0.01f, "Position nach Roundtrip identisch");
    }
    std::filesystem::remove(path);
}

void TestLegacyShmdByteExactRoundtrip(const std::filesystem::path& shmdPath) {
    auto parsed = legacy::ParseLegacyShmd(shmdPath);
    Check(parsed.has_value(), "ParseLegacyShmd(Rou.shmd) erfolgreich");
    if (!parsed) {
        std::fprintf(stderr, "         Fehler: %s\n", parsed.error().c_str());
        return;
    }

    std::size_t totalCategoryPaths = 0;
    for (const auto& c : parsed->categories) totalCategoryPaths += c.modelPaths.size();
    std::printf("         Kategorien: %zu (zusammen %zu Pfade), Objekt-Instanzen: %zu\n",
                parsed->categories.size(), totalCategoryPaths, parsed->Count());
    Check(parsed->categories.size() == 3, "Alle 3 Kategorie-Bl\u00f6cke gefunden (Sky/Water/GroundObject)");
    Check(parsed->Count() == 1580, "Alle 1580 Objekt-Instanzen gefunden");

    const auto exportPath = std::filesystem::temp_directory_path() / "map_editor_legacy_shmd_export_test.shmd";
    auto exportResult = legacy::SerializeLegacyShmd(*parsed, exportPath);
    Check(exportResult.has_value(), "SerializeLegacyShmd erfolgreich");

    const auto originalBytes = ReadAllBytes(shmdPath);
    const auto exportedBytes = ReadAllBytes(exportPath);
    Check(originalBytes.size() == exportedBytes.size(), "Exportierte .shmd hat identische Gr\u00f6\u00dfe wie Original");
    Check(originalBytes == exportedBytes, "Exportierte .shmd ist BYTE-F\u00dcR-BYTE IDENTISCH zum Original (Rou.shmd)");

    std::filesystem::remove(exportPath);
}

void TestLegacyIdmByteExactRoundtrip(const std::filesystem::path& idmPath) {
    auto parsed = legacy::ParseLegacyIdm(idmPath);
    Check(parsed.has_value(), "ParseLegacyIdm(Rou.idm) erfolgreich");
    if (!parsed) {
        std::fprintf(stderr, "         Fehler: %s\n", parsed.error().c_str());
        return;
    }
    std::printf("         Hash: %s, F\u00fchrender Wert: %d, Gruppen: %zu\n",
                parsed->hash.c_str(), parsed->headerValue, parsed->groups.size());
    Check(parsed->groups.size() == 1178, "Alle 1178 Gruppen gefunden");

    const auto exportPath = std::filesystem::temp_directory_path() / "map_editor_legacy_idm_export_test.idm";
    auto exportResult = legacy::SerializeLegacyIdm(*parsed, exportPath);
    Check(exportResult.has_value(), "SerializeLegacyIdm erfolgreich");

    const auto originalBytes = ReadAllBytes(idmPath);
    const auto exportedBytes = ReadAllBytes(exportPath);
    Check(originalBytes.size() == exportedBytes.size(), "Exportierte .idm hat identische Gr\u00f6\u00dfe wie Original");
    Check(originalBytes == exportedBytes, "Exportierte .idm ist BYTE-F\u00dcR-BYTE IDENTISCH zum Original (Rou.idm)");

    std::filesystem::remove(exportPath);
}

void TestLegacyAidByteExactRoundtrip(const std::filesystem::path& aidPath) {
    auto parsed = legacy::ParseLegacyAid(aidPath);
    Check(parsed.has_value(), "ParseLegacyAid(Rou.aid) erfolgreich");
    if (!parsed) {
        std::fprintf(stderr, "         Fehler: %s\n", parsed.error().c_str());
        return;
    }
    std::printf("         Zonenname: '%s', recordType=%d, flag=%d\n", parsed->name.c_str(), parsed->recordType, parsed->flag);
    Check(parsed->name == "MH_Zone1", "Zonenname korrekt geparst");

    const auto exportPath = std::filesystem::temp_directory_path() / "map_editor_legacy_aid_export_test.aid";
    auto exportResult = legacy::SerializeLegacyAid(*parsed, exportPath);
    Check(exportResult.has_value(), "SerializeLegacyAid erfolgreich");

    const auto originalBytes = ReadAllBytes(aidPath);
    const auto exportedBytes = ReadAllBytes(exportPath);
    Check(originalBytes.size() == exportedBytes.size(), "Exportierte .aid hat identische Gr\u00f6\u00dfe wie Original");
    Check(originalBytes == exportedBytes, "Exportierte .aid ist BYTE-F\u00dcR-BYTE IDENTISCH zum Original (Rou.aid)");

    std::filesystem::remove(exportPath);
}

} // namespace

int main(int argc, char** argv) {
    std::printf("== ObjectPlacement Core Tests ==\n");
    TestBasicObjectPlacement();
    TestTsObjRoundtrip();

    if (argc >= 4) {
        std::printf("\n== Legacy-shmd-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyShmdByteExactRoundtrip(argv[1]);
        std::printf("\n== Legacy-idm-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyIdmByteExactRoundtrip(argv[2]);
        std::printf("\n== Legacy-aid-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyAidByteExactRoundtrip(argv[3]);
    } else {
        std::printf("\n(Legacy-Tests \u00fcbersprungen - Aufruf mit: %s <Rou.shmd> <Rou.idm> <Rou.aid>)\n", argv[0]);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
