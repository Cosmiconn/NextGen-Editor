// test_nif_client_render_state.cpp
// Render-Zustände, die der Fiesta-Client anders setzt als ein NIF-Betrachter, belegt an Rou-Modellen:
// APP_CULLED-Geometrie bleibt unsichtbar, ohne NiSpecularProperty gibt es keinen Glanz, und
// eingebettete NiPixelData liegt zeilenweise von oben nach unten (Direct3D-UVs, keine Spiegelung).

#include "mapeditor/core/NifModel.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

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

const NifMeshPart* PartWithTexture(const NifModel& model, const std::string& texture) {
    for (const auto& part : model.parts)
        if (part.textureSlots[0].texture == texture) return &part;
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Aufruf: test_nif_client_render_state <fixtures>\n");
        return 2;
    }
    const std::filesystem::path fixtures = argv[1];

    // H_rou_Light.nif: eine untexturierte Hilfsgeometrie unter einem APP_CULLED-Knoten.
    if (const auto light = LoadNifMesh(fixtures / "H_rou_Light.nif")) {
        std::size_t culled = 0;
        for (const auto& part : light->parts) culled += part.appCulled ? 1u : 0u;
        Check(culled == 1, "H_rou_Light.nif: genau ein APP_CULLED-Part");
        bool culledIsUntextured = false;
        for (const auto& part : light->parts)
            if (part.appCulled) culledIsUntextured = part.textureSlots[0].texture.empty();
        Check(culledIsUntextured, "H_rou_Light.nif: der APP_CULLED-Part ist untexturiert");
    } else {
        Check(false, "H_rou_Light.nif ladbar");
    }

    // house02.nif hat keine NiSpecularProperty: im Client ohne Glanzlicht (Dächer matt).
    if (const auto house = LoadNifMesh(fixtures / "house02.nif")) {
        bool anySpecular = false;
        for (const auto& part : house->parts) anySpecular = anySpecular || part.specularEnabled;
        Check(!house->parts.empty() && !anySpecular, "house02.nif: ohne NiSpecularProperty kein Glanz");
    } else {
        Check(false, "house02.nif ladbar");
    }

    // rou_ground.nif: roundtile.dds ist ein Viertel des Magiekreises am Brunnen. Die Vertices
    // legen UV (0,0) an die äußere Ecke und UV (1,1) an die Kreismitte; im Spiel ist die äußere
    // Ecke durchsichtig. Ohne Spiegelung muss Zeile 0/Spalte 0 also transparent sein.
    if (const auto ground = LoadNifMesh(fixtures / "rou_ground.nif")) {
        const auto* tile = PartWithTexture(*ground, "roundtile.dds");
        const auto texture = tile ? tile->textureSlots[0].embeddedTexture : nullptr;
        Check(texture != nullptr && texture->width == 512 && texture->height == 512,
              "rou_ground.nif: roundtile.dds eingebettet (512x512)");
        if (texture && texture->rgba.size() == 512u * 512u * 4u) {
            const auto alphaAt = [&](std::size_t x, std::size_t y) { return texture->rgba[(y * 512u + x) * 4u + 3u]; };
            Check(alphaAt(0, 0) < 64, "roundtile.dds: Zeile 0, Spalte 0 (UV 0,0, äußere Ecke) transparent");
            Check(alphaAt(511, 511) > 192, "roundtile.dds: letzte Zeile/Spalte (UV 1,1, Kreismitte) deckend");
        }
        std::size_t culled = 0;
        for (const auto& part : ground->parts) culled += part.appCulled ? 1u : 0u;
        Check(culled >= 1, "rou_ground.nif: APP_CULLED-Hilfsgeometrie erkannt");
    } else {
        Check(false, "rou_ground.nif ladbar");
    }

    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
