// test_texture_rules.cpp
// Texturregeln (Neigung/Höhe), Regel als Pinsel, Layer-Regionen, Pinselform aus Bild; mit der
// echten Rou.HTD: steile Hänge bekommen den Fels-Layer, andere Layer bleiben unverändert.

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/TextureRules.hpp"
#include "mapeditor/core/WalkGenerate.hpp"

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

// 17x17 à 50: links flach, rechts 45°-Rampe.
Heightmap RampMap() {
    Heightmap h(17, 17, 50.0f, 50.0f);
    for (std::uint32_t z = 0; z < 17; ++z)
        for (std::uint32_t x = 0; x < 17; ++x) h.Set(x, z, x >= 8 ? static_cast<float>(x - 8) * 50.0f : 0.0f);
    return h;
}

void TestWeights() {
    const Heightmap h = RampMap();
    TextureRule rock;
    rock.minSlope = 35.0f;
    Check(TextureRuleWeight(h, rock, 600.0f, 400.0f) > 0.99f, "45°: Fels-Regel voll");
    Check(TextureRuleWeight(h, rock, 200.0f, 400.0f) == 0.0f, "flach: Fels-Regel 0");
    rock.minSlope = 47.0f;
    rock.slopeFeather = 4.0f;
    const float soft = TextureRuleWeight(h, rock, 600.0f, 400.0f);
    Check(soft > 0.4f && soft < 0.6f, "weicher Übergang (2° unter der Grenze, 4° Übergang)");
    TextureRule high;
    high.useSlope = false;
    high.useHeight = true;
    high.minHeight = 200.0f;
    high.maxHeight = 1000.0f;
    high.heightFeather = 0.0f;
    Check(TextureRuleWeight(h, high, 750.0f, 400.0f) == 1.0f && TextureRuleWeight(h, high, 500.0f, 400.0f) == 0.0f,
          "Höhenregel");
}

void TestApply() {
    const Heightmap h = RampMap();
    TextureLayerStack stack(64, 64);
    stack.AddLayer("grass", "g.dds");
    stack.AddLayer("rock", "r.dds");
    for (std::uint32_t z = 0; z < 64; ++z)
        for (std::uint32_t x = 0; x < 64; ++x) stack.Layer(0).blend.Set(x, z, 1.0f);
    TextureRule rock;
    rock.minSlope = 35.0f;
    auto patch = ApplyTextureRule(stack, 1, h, rock, {});
    Check(!patch.entries.empty(), "Regel auf ganze Karte angewendet");
    Check(stack.Layer(1).blend.At(48, 32) > 0.99f && stack.Layer(1).blend.At(16, 32) == 0.0f, "Rampe Fels, Ebene nicht");
    Check(stack.Layer(0).blend.At(48, 32) == 1.0f, "anderer Layer unverändert");
    RevertTexturePatch(stack, patch);
    Check(stack.Layer(1).blend.At(48, 32) == 0.0f, "Undo stellt den Layer wieder her");

    // Als Pinsel: nur im Kreis.
    TextureRuleArea area;
    area.wholeMap = false;
    area.centerX = 600.0f;
    area.centerZ = 200.0f;
    area.radius = 60.0f;
    ApplyTextureRule(stack, 1, h, rock, area);
    Check(stack.Layer(1).blend.At(48, 16) > 0.99f && stack.Layer(1).blend.At(48, 48) == 0.0f, "Regel als Pinsel: nur im Kreis");

    // Replace überschreibt, Raise nicht.
    stack.Layer(1).blend.Set(16, 32, 0.7f);
    ApplyTextureRule(stack, 1, h, rock, {});
    Check(std::abs(stack.Layer(1).blend.At(16, 32) - 0.7f) < 1e-4f, "Raise lässt vorhandenes Gewicht stehen");
    rock.mode = TextureRuleMode::Replace;
    ApplyTextureRule(stack, 1, h, rock, {});
    Check(stack.Layer(1).blend.At(16, 32) == 0.0f, "Replace überschreibt");

    // Layer mit Region (rechte Hälfte): Pixel 0 liegt bei x = 400.
    TextureLayerStack half(32, 32);
    half.AddLayer("rock", "r.dds");
    half.Layer(0).regionStartX = 8.0f;
    half.Layer(0).regionWidth = 8.0f;
    half.Layer(0).regionHeight = 16.0f;
    rock.mode = TextureRuleMode::Raise;
    ApplyTextureRule(half, 0, h, rock, {});
    Check(half.Layer(0).blend.At(16, 16) > 0.99f, "Layer-Region: Rampe in der rechten Hälfte erkannt");
}

void TestAlpha() {
    BrushAlpha alpha;
    alpha.width = 3;
    alpha.height = 3;
    alpha.values = {0, 0, 0, 0, 1, 0, 0, 0, 0.5f};
    Check(BrushAlphaWeight(alpha, 0.0f, 0.0f) == 1.0f, "Pinselform: Mitte");
    Check(BrushAlphaWeight(alpha, 1.0f, -1.0f) == 0.5f, "Pinselform: Südost-Ecke (Zeile 2)");
    Heightmap h(17, 17, 50.0f, 50.0f);
    BrushSettings s;
    s.radius = 200.0f;
    s.strength = 10.0f;
    s.alpha = &alpha;
    ApplyBrush(h, BrushMode::Raise, s, 400.0f, 400.0f);
    Check(std::abs(h.At(8, 8) - 10.0f) < 1e-4f, "Terrain-Pinsel mit Bildform: volle Wirkung in der Mitte");
    Check(h.At(8, 12) == 0.0f, "Bildform 0 am Nordrand: keine Wirkung");
}

void TestReal(const char* htd) {
    auto rou = ImportLegacyHtd(htd, 257, 257);
    Check(rou.has_value(), "Rou.HTD geladen");
    if (!rou) return;
    TextureLayerStack stack(256, 256);
    stack.AddLayer("rock", "r.dds");
    TextureRule rock;
    rock.minSlope = 35.0f;
    rock.slopeFeather = 0.0f;
    ApplyTextureRule(stack, 0, *rou, rock, {});
    std::size_t painted = 0, steep = 0;
    for (std::uint32_t z = 0; z < 256; z += 2)
        for (std::uint32_t x = 0; x < 256; x += 2) {
            const bool isSteep = TerrainSlopeDegrees(*rou, static_cast<float>(x) * 50.0f, static_cast<float>(z) * 50.0f) > 35.0f;
            steep += isSteep;
            painted += isSteep && stack.Layer(0).blend.At(x, z) > 0.99f;
        }
    std::printf("         Rou: %zu von %zu steilen Proben bemalt\n", painted, steep);
    Check(steep > 0 && painted == steep, "Rou: alle Stellen über 35° haben den Fels-Layer");
}

} // namespace

int main(int argc, char** argv) {
    TestWeights();
    TestApply();
    TestAlpha();
    if (argc > 1) TestReal(argv[1]);
    if (g_failures != 0) {
        std::fprintf(stderr, "%d Prüfung(en) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Prüfungen bestanden\n");
    return 0;
}
