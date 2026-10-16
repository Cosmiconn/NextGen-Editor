// test_object_scatter.cpp
// Objekte malen: Mindestabstand, Sättigung bei wiederholtem Malen, Neigungsgrenze, Skalierung,
// Ausrichtung am Hang und Löschen im Pinselkreis.

#include "mapeditor/core/ObjectScatter.hpp"

#include <cmath>
#include <cstdio>

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

// Hochachse (0,1,0) mit dem Quaternion drehen.
void RotateUp(const PlacedObject& o, float out[3]) {
    const float x = o.rotX, y = o.rotY, z = o.rotZ, w = o.rotW;
    out[0] = 2.0f * (x * y - w * z);
    out[1] = 1.0f - 2.0f * (x * x + z * z);
    out[2] = 2.0f * (y * z + w * x);
}

} // namespace

int main() {
    Heightmap flat(33, 33, 50.0f, 50.0f); // 1600 x 1600
    ObjectPlacementSet set;
    ScatterSettings s;
    s.models = {"resmap\\nifs\\common\\grass3.nif", "resmap\\nifs\\common\\flowers1.nif"};
    s.radius = 300.0f;
    s.density = 5.0f;
    s.minDistance = 60.0f;
    std::mt19937 rng(42);
    std::size_t lastCount = 0, added = 0;
    for (int stroke = 0; stroke < 40; ++stroke) {
        auto objs = ScatterObjects(flat, set, s, 800.0f, 800.0f, rng);
        added = objs.size();
        for (auto& o : objs) set.AddObject(std::move(o));
        if (stroke == 0) lastCount = set.Count();
    }
    Check(lastCount > 0, "erste Anwendung setzt Objekte");
    Check(added <= 2, "nach 40 Anwendungen kaum noch Platz (Sättigung)");
    float minD = 1e9f;
    for (std::size_t i = 0; i < set.Count(); ++i)
        for (std::size_t j = i + 1; j < set.Count(); ++j) {
            const float dx = set.At(i).posX - set.At(j).posX, dz = set.At(i).posZ - set.At(j).posZ;
            minD = std::min(minD, std::sqrt(dx * dx + dz * dz));
        }
    std::printf("         %zu Objekte, kleinster Abstand %.1f\n", set.Count(), minD);
    Check(minD >= 60.0f, "Mindestabstand eingehalten");
    bool inside = true, scaled = true, models = true;
    for (const auto& o : set.Objects()) {
        inside &= std::hypot(o.posX - 800.0f, o.posZ - 800.0f) <= 300.0f + 1e-3f;
        scaled &= o.scale >= 0.8f && o.scale <= 1.25f;
        models &= o.modelPath == s.models[0] || o.modelPath == s.models[1];
    }
    Check(inside && scaled && models, "im Kreis, Skalierung im Bereich, nur gewählte Modelle");

    // Löschen im Kreis: absteigende Indizes, nur gewählte Modelle.
    set.AddObject(PlacedObject{"resmap\\field\\Rou\\House.nif", 800.0f, 0.0f, 800.0f});
    const auto erase = ScatteredObjectsInRadius(set, s.models, 800.0f, 800.0f, 120.0f);
    bool descending = true, notHouse = true;
    for (std::size_t k = 0; k < erase.size(); ++k) {
        if (k > 0 && erase[k] >= erase[k - 1]) descending = false;
        if (set.At(erase[k]).modelPath.find("House") != std::string::npos) notHouse = false;
    }
    Check(!erase.empty() && descending && notHouse, "Löschen: nur gewählte Modelle, absteigend");

    // Neigungsgrenze und Ausrichtung auf einer 45°-Rampe (rechte Hälfte).
    Heightmap ramp(33, 33, 50.0f, 50.0f);
    for (std::uint32_t z = 0; z < 33; ++z)
        for (std::uint32_t x = 0; x < 33; ++x) ramp.Set(x, z, x >= 16 ? static_cast<float>(x - 16) * 50.0f : 0.0f);
    ObjectPlacementSet empty;
    ScatterSettings steep = s;
    steep.maxSlope = 30.0f;
    Check(ScatterObjects(ramp, empty, steep, 1200.0f, 800.0f, rng).empty(), "45°-Hang über 30°: nichts gesetzt");
    steep.maxSlope = 60.0f;
    steep.alignToSlope = true;
    steep.randomYaw = true;
    auto onSlope = ScatterObjects(ramp, empty, steep, 1200.0f, 800.0f, rng);
    bool aligned = !onSlope.empty();
    for (const auto& o : onSlope) {
        float up[3];
        RotateUp(o, up);
        // Normale der Rampe (Steigung 1 in +x): (-1, 1, 0) / sqrt(2)
        aligned &= std::abs(up[0] + 0.7071f) < 0.03f && std::abs(up[1] - 0.7071f) < 0.03f && std::abs(up[2]) < 0.03f;
        aligned &= std::abs(o.posY - ramp.SampleWorld(o.posX, o.posZ)) < 1e-3f;
    }
    Check(aligned, "am Hang ausgerichtet (Hochachse = Normale), auf dem Terrain");

    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
