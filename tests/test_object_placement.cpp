// test_object_placement.cpp
// GUI-freier Test. Prüft ObjectPlacementSet-Grundfunktionen, .shmd-Roundtrip, und den
// Legacy-Import/Export für .shmd, .idm und .aid - jeweils Byte-für-Byte gegen die echten
// Referenzdateien verglichen.

#include "mapeditor/core/ObjectPlacement.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"

#include <cstdio>
#include <algorithm>
#include <cmath>
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

    ObjectCategoryList sky;
    sky.name = "Sky";
    sky.modelPaths.push_back("sky.nif");
    set.categories.push_back(sky);
    set.AddObject(obj);
    set.AddObject(obj);
    set.ClearObjects();
    Check(set.Count() == 0, "ClearObjects entfernt alle normalen Placements");
    Check(set.categories.size() == 1 && set.categories[0].modelPaths.size() == 1,
          "ClearObjects erhält SHMD-Kategorien");
}

void TestShmdRoundtrip() {
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

    const auto path = std::filesystem::temp_directory_path() / "map_editor_tsobj_test.shmd";
    auto saveResult = legacy::SerializeLegacyShmd(set, path);
    Check(saveResult.has_value(), "legacy::SerializeLegacyShmd erfolgreich");

    auto loadResult = legacy::ParseLegacyShmd(path);
    Check(loadResult.has_value(), "legacy::ParseLegacyShmd erfolgreich");
    if (loadResult) {
        Check(loadResult->categories.size() == 1 && loadResult->categories[0].name == "Sky", "Kategorie nach Roundtrip identisch");
        Check(loadResult->Count() == 1, "Objekt-Anzahl nach Roundtrip identisch");
        Check(loadResult->At(0).modelPath == "GuildHall.nif", "Modellpfad nach Roundtrip identisch");
        Check(std::abs(loadResult->At(0).posX - 6769.816406f) < 0.01f, "Position nach Roundtrip identisch");
    }
    std::filesystem::remove(path);
}

void TestShmdCategoryEditing() {
    ObjectPlacementSet set;

    ObjectCategoryList sky;
    sky.name = "Sky";
    sky.modelPaths = {
        "resmap\\nifs\\Common\\field_sky_01.nif",
        "resmap\\field\\Test\\sky_alt.nif"
    };
    ObjectCategoryList water;
    water.name = "Water";
    water.modelPaths = {"resmap\\field\\Test\\water.nif"};
    ObjectCategoryList ground;
    ground.name = "GroundObject";
    ground.modelPaths = {
        "resmap\\field\\Test\\ground_a.nif",
        "resmap\\field\\Test\\ground_b.nif"
    };
    set.categories = {sky, water, ground};

    // Entspricht den nativen Editor-Operationen für SHMD-Szenenmodelle:
    // Modellpfad austauschen und einen Eintrag löschen.
    set.categories[0].modelPaths[0] = "resmap\\field\\Test\\sky_replaced.nif";
    set.categories[1].modelPaths[0] = "resmap\\field\\Test\\water_replaced.nif";
    set.categories[2].modelPaths.erase(set.categories[2].modelPaths.begin());

    const auto path = std::filesystem::temp_directory_path() / "nextgen-shmd-category-edit.shmd";
    Check(legacy::SerializeLegacyShmd(set, path).has_value(), "SHMD-Kategorieänderung schreiben");

    const auto loaded = legacy::ParseLegacyShmd(path);
    Check(loaded.has_value(), "SHMD-Kategorieänderung lesen");
    if (loaded) {
        Check(loaded->categories.size() == 3, "Sky/Water/GroundObject nach Bearbeitung erhalten");
        Check(loaded->categories[0].modelPaths.size() == 2 &&
              loaded->categories[0].modelPaths[0] == "resmap\\field\\Test\\sky_replaced.nif",
              "Sky-Modellpfad nach Bearbeitung erhalten");
        Check(loaded->categories[1].modelPaths.size() == 1 &&
              loaded->categories[1].modelPaths[0] == "resmap\\field\\Test\\water_replaced.nif",
              "Water-Modellpfad nach Bearbeitung erhalten");
        Check(loaded->categories[2].modelPaths.size() == 1 &&
              loaded->categories[2].modelPaths[0] == "resmap\\field\\Test\\ground_b.nif",
              "GroundObject-Löschung nach Bearbeitung erhalten");
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


void TestMultipleAidAreasAndInvalidInput() {
    using namespace legacy;
    const auto path = std::filesystem::temp_directory_path() / "nextgen-aid-multiple.aid";
    ZoneMetadata zones;
    zones.name = "circle"; zones.flag = 0;
    zones.bounds[0] = 1.25f; zones.bounds[1] = -7.0f; zones.bounds[2] = 42.0f;
    zones.additionalAreas.resize(2);
    zones.Area(1).name = "rectangle"; zones.Area(1).bounds[4] = 123.5f;
    zones.Area(2).name = std::string(32, 'x'); zones.Area(2).flag = 0;
    Check(SerializeLegacyAid(zones, path).has_value(), "Write mixed AID areas");
    const auto bytes = ReadAllBytes(path);
    Check(bytes.size() == 4 + 48 + 56 + 48, "Mixed AID record sizes");
    const auto loaded = ParseLegacyAid(path);
    Check(loaded && loaded->AreaCount() == 3 && loaded->Area(1).bounds[4] == 123.5f
          && loaded->Area(2).name.size() == 32, "All AID areas retained");
    if (loaded) {
        Check(SerializeLegacyAid(*loaded, path).has_value() && ReadAllBytes(path) == bytes, "Mixed AID exact roundtrip");
        auto edited = *loaded;
        edited.Area(1).name = "edited";
        Check(SerializeLegacyAid(edited, path).has_value(), "Edit second AID area");
        const auto again = ParseLegacyAid(path);
        Check(again && again->Area(1).name == "edited" && again->Area(2).name == zones.Area(2).name,
              "Editing second area preserves third area");
    }
    const auto write = [&](const std::vector<char>& data) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    };
    bool rejectsTruncation = true;
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        write(std::vector<char>(bytes.begin(), bytes.begin() + length));
        if (ParseLegacyAid(path)) rejectsTruncation = false;
    }
    Check(rejectsTruncation, "Reject every truncated mixed AID boundary");
    auto damaged = bytes; damaged.push_back(0); write(damaged);
    Check(!ParseLegacyAid(path), "Reject undeclared AID trailing bytes");
    damaged = bytes; damaged[36] = 2; write(damaged);
    Check(!ParseLegacyAid(path), "Reject unknown AID shape");
    damaged = bytes; std::fill_n(damaged.begin(), 4, static_cast<char>(0xff)); write(damaged);
    Check(!ParseLegacyAid(path), "Reject impossible AID count before allocation");
    zones.recordType = 0; zones.additionalAreas.clear();
    Check(SerializeLegacyAid(zones, path).has_value(), "Write empty AID");
    const auto empty = ParseLegacyAid(path);
    Check(empty && empty->AreaCount() == 0 && std::filesystem::file_size(path) == 4, "Empty AID roundtrip");
    std::filesystem::remove(path);
}


void TestShmdVariants(const std::filesystem::path& fixtures) {
    const auto output = std::filesystem::temp_directory_path() / "nextgen-shmd-variants.shmd";
    for (const char* name : {"Mem_UA.shmd", "SwaDn01.shmd", "bera.shmd"}) {
        const auto source = fixtures / name;
        auto parsed = legacy::ParseLegacyShmd(source);
        Check(parsed.has_value(), name);
        if (!parsed) continue;
        Check(legacy::SerializeLegacyShmd(*parsed, output).has_value() && ReadAllBytes(source) == ReadAllBytes(output),
              "SHMD variant byte-exact roundtrip");
        if (parsed->Count()) {
            parsed->At(0).posX = 17.5f;
            Check(legacy::SerializeLegacyShmd(*parsed, output).has_value(), "SHMD variant edited export");
            const auto again = legacy::ParseLegacyShmd(output);
            Check(again && again->Count() == parsed->Count() && again->At(0).posX == 17.5f
                && again->hasLightingFooter == parsed->hasLightingFooter, "SHMD variant edit and footer retained");
            if (std::string(name) == "Mem_UA.shmd" && again) {
                Check(std::any_of(again->Objects().begin(), again->Objects().end(), [](const auto& obj) {
                    return std::isnan(obj.rotW);
                }), "Existing NaN transforms retained without inventing rotations");
            }
        }
    }
    // A missing footer is accepted only at a complete record boundary.
    { std::ofstream out(output, std::ios::binary); out << "shmd0_5\nGlobalLight 1 1 1\nFog 0 0 0 0\nBackGroundColor 0 0 0\nFrustum 5000\na.nif 1\n1 2 3 0 0 0 1"; }
    Check(!legacy::ParseLegacyShmd(output), "Incomplete SHMD instance is rejected");
    std::filesystem::remove(output);
}

} // namespace

int main(int argc, char** argv) {
    std::printf("== ObjectPlacement Core Tests ==\n");
    TestBasicObjectPlacement();
    TestShmdRoundtrip();
    TestShmdCategoryEditing();
    TestMultipleAidAreasAndInvalidInput();

    if (argc >= 4) {
        std::printf("\n== Legacy-shmd-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyShmdByteExactRoundtrip(argv[1]);
        std::printf("\n== Legacy-idm-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyIdmByteExactRoundtrip(argv[2]);
        std::printf("\n== Legacy-aid-Roundtrip (Byte-f\u00fcr-Byte) ==\n");
        TestLegacyAidByteExactRoundtrip(argv[3]);
        TestShmdVariants(std::filesystem::path(argv[1]).parent_path() / "data");
    } else {
        std::printf("\n(Legacy-Tests \u00fcbersprungen - Aufruf mit: %s <Rou.shmd> <Rou.idm> <Rou.aid>)\n", argv[0]);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
