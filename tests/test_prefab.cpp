// test_prefab.cpp
// Prefabs: anlegen, speichern/laden, einsetzen mit Drehung; Höhen folgen dem Terrain.

#include "mapeditor/core/Prefab.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

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

bool Near(float a, float b, float eps = 1e-3f) { return std::abs(a - b) <= eps; }

} // namespace

int main() {
    Heightmap flat(17, 17, 50.0f, 50.0f);
    for (auto& v : flat.MutableData()) v = 100.0f;
    std::vector<PlacedObject> group = {
        {"resmap\\field\\Rou\\stall.nif", 300.0f, 100.0f, 400.0f},
        {"resmap\\field\\Rou\\box.nif", 340.0f, 105.0f, 400.0f},
        {"resmap\\field\\Rou\\lamp.nif", 300.0f, 100.0f, 440.0f},
    };
    group[1].rotY = std::sin(0.25f); group[1].rotW = std::cos(0.25f); group[1].scale = 1.5f;
    const Prefab prefab = MakePrefab("Marktstand", group, flat);
    Check(prefab.items.size() == 3, "Prefab mit drei Objekten");
    Check(Near(prefab.items[1].heightAboveGround, 5.0f), "Höhe über Boden gespeichert");

    const auto dir = std::filesystem::temp_directory_path() / "nextgen_prefab_test";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    Check(SavePrefab(prefab, dir / "Marktstand.prefab").has_value(), "gespeichert");
    auto loaded = LoadPrefab(dir / "Marktstand.prefab");
    Check(loaded && loaded->name == "Marktstand" && loaded->items.size() == 3 &&
              loaded->items[1].modelPath == "resmap\\field\\Rou\\box.nif" && Near(loaded->items[1].scale, 1.5f),
          "geladen");
    Check(ListPrefabs(dir).size() == 1, "im Ordner gefunden");

    // Ohne Drehung: Abstände bleiben, Höhe = Terrain + Versatz.
    Heightmap slope(17, 17, 50.0f, 50.0f);
    for (std::uint32_t z = 0; z < 17; ++z)
        for (std::uint32_t x = 0; x < 17; ++x) slope.Set(x, z, static_cast<float>(x) * 10.0f);
    auto placed = InstantiatePrefab(*loaded, slope, 500.0f, 500.0f, 0.0f);
    const float dxOrig = group[1].posX - group[0].posX;
    Check(Near(placed[1].posX - placed[0].posX, dxOrig) && Near(placed[1].posZ, placed[0].posZ),
          "Abstände erhalten");
    Check(Near(placed[1].posY, slope.SampleWorld(placed[1].posX, placed[1].posZ) + 5.0f), "Höhe folgt dem Hang");

    // 90° gedreht: Versatz in +x wird zu Versatz in -z (Drehung um +Y).
    auto rotated = InstantiatePrefab(*loaded, flat, 500.0f, 500.0f, 90.0f);
    Check(Near(rotated[1].posX - rotated[0].posX, 0.0f, 0.01f) && Near(rotated[1].posZ - rotated[0].posZ, -dxOrig, 0.01f),
          "90°: Anordnung mitgedreht");
    // Quaternion: Kiste hatte 0,5 rad, jetzt 0,5 rad + 90°
    const float angle = 2.0f * std::atan2(rotated[1].rotY, rotated[1].rotW);
    Check(Near(angle, 0.5f + 3.14159265f / 2.0f, 1e-3f), "90°: Objektdrehung addiert");
    Check(!LoadPrefab(dir / "fehlt.prefab").has_value(), "fehlende Datei -> Fehler");
    std::filesystem::remove_all(dir, ec);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
