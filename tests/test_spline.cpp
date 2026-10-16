// test_spline.cpp
// Weg-Werkzeug: Abtastung, Terrain ebnen, Layer malen, Objekte entlang des Wegs.

#include "mapeditor/core/Spline.hpp"

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

} // namespace

int main() {
    const SplinePoint straight[] = {{100.0f, 800.0f}, {800.0f, 800.0f}, {1500.0f, 800.0f}};
    const auto samples = SampleSpline(straight, 50.0f);
    Check(samples.size() >= 28 && samples.size() <= 30, "1400 Einheiten mit Abstand 50 abgetastet");
    bool onLine = true;
    for (const auto& s : samples) onLine &= std::abs(s.z - 800.0f) < 0.5f && std::abs(s.dirX - 1.0f) < 0.01f;
    Check(onLine, "gerade Punkte: gerade Kurve, Richtung +X");
    Check(std::abs(samples.back().x - 1500.0f) < 0.5f, "endet am letzten Punkt");

    // Gelände: Rauschen um 100; Weg ebnen -> Wegmitte fast eben, außerhalb unverändert.
    Heightmap h(33, 33, 50.0f, 50.0f);
    for (std::uint32_t z = 0; z < 33; ++z)
        for (std::uint32_t x = 0; x < 33; ++x) h.Set(x, z, 100.0f + ((x * 7 + z * 13) % 5) * 20.0f);
    const Heightmap before = h;
    auto patch = FlattenAlongSpline(h, samples, {100.0f, 50.0f}, 200.0f);
    Check(!patch.entries.empty(), "Terrain entlang des Wegs geändert");
    float maxStep = 0.0f;
    for (std::uint32_t x = 4; x < 28; ++x) maxStep = std::max(maxStep, std::abs(h.At(x + 1, 16) - h.At(x, 16)));
    Check(maxStep < 15.0f, "Wegmitte deutlich ebener (Stufen < 15 statt bis 80)");
    Check(h.At(16, 4) == before.At(16, 4), "weit neben dem Weg unverändert");
    for (const auto& e : patch.entries) h.Set(e.x, e.z, e.oldValue);
    bool restored = true;
    for (std::uint32_t z = 0; z < 33; ++z)
        for (std::uint32_t x = 0; x < 33; ++x) restored &= h.At(x, z) == before.At(x, z);
    Check(restored, "Undo-Patch stellt alles wieder her");

    // Layer malen: 64x64-Maske über 1600x1600.
    TextureLayerStack stack(64, 64);
    stack.AddLayer("path", "p.dds");
    for (std::uint32_t z = 0; z < 64; ++z)
        for (std::uint32_t x = 0; x < 64; ++x) stack.Layer(0).blend.Set(x, z, 0.0f); // erster Layer startet voll
    PaintAlongSpline(stack, 0, h, samples, {100.0f, 50.0f});
    Check(stack.Layer(0).blend.At(32, 32) > 0.99f && stack.Layer(0).blend.At(32, 50) == 0.0f, "Weg-Layer nur auf dem Weg");

    // Objekte: beidseitig alle 200 Einheiten, 150 neben der Mitte.
    SplineObjectSettings fence;
    fence.model = "resmap\\nifs\\Common\\FenceWood1_1.nif";
    fence.spacing = 200.0f;
    fence.sideOffset = 150.0f;
    auto objs = ObjectsAlongSpline(straight, h, fence);
    Check(objs.size() == 16, "8 Positionen x 2 Seiten");
    bool sides = true;
    for (const auto& o : objs) sides &= std::abs(std::abs(o.posZ - 800.0f) - 150.0f) < 0.5f;
    Check(sides, "150 Einheiten links und rechts der Mitte");
    Check(std::abs(objs[0].rotY) < 1e-4f && std::abs(objs[0].rotW - 1.0f) < 1e-4f, "entlang +X ohne zusätzliche Drehung");

    // Kurve: Richtung folgt dem Weg.
    const SplinePoint corner[] = {{100.0f, 100.0f}, {800.0f, 100.0f}, {800.0f, 800.0f}};
    const auto bent = SampleSpline(corner, 50.0f);
    Check(std::abs(bent.back().dirZ) > 0.9f, "nach der Kurve Richtung +Z");
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
