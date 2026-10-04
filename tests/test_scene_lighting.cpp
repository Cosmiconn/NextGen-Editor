// test_scene_lighting.cpp
// Prüft die Ableitung von Szenenlicht, Nebel und Hintergrund aus der echten NA2016-Datei Rou.shmd
// (GlobalLight / DirectionLightAmbient / DirectionLightDiffuse / Fog / BackGroundColor / Frustum).

#include "SceneLighting.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace theseed::mapeditor;

namespace {
int g_failures = 0;
void Check(bool ok, const char* what) {
    if (ok) std::printf("[ok]     %s\n", what);
    else { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
}
bool Near(float a, float b, float eps = 1.0e-4f) { return std::abs(a - b) < eps; }
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "Aufruf: test_scene_lighting <Rou.shmd>\n"); return 2; }
    const auto set = core::legacy::ParseLegacyShmd(argv[1]);
    Check(set.has_value(), "Rou.shmd geladen");
    if (!set) return 1;
    const auto& env = set->environment;
    Check(Near(env.globalLight[0], 0.792157f) && Near(env.frustumFar, 5000.0f), "Rou.shmd: GlobalLight 0.792157, Frustum 5000");

    const auto l = app::SceneLightingFromEnvironment(env, set->hasLightingFooter);
    Check(l.fromMapData && l.backgroundFromMapData, "Kartenlicht und Hintergrund aus Daten");
    Check(Near(l.ambient[0], 0.792157f) && Near(l.ambient[2], 0.792157f),
          "Umgebungslicht = GlobalLight + DirectionLightAmbient (0)");
    Check(Near(l.sun[0], 1.0f) && Near(l.sun[1], 0.976471f) && Near(l.sun[2], 0.901961f),
          "Sonnenfarbe = DirectionLightDiffuse (warm)");
    Check(Near(l.background[0], 0.0f) && Near(l.background[1], 0.501961f) && Near(l.background[2], 1.0f),
          "Hintergrund = BackGroundColor");
    Check(Near(l.fogColor[0], 0.070588f) && Near(l.fogColor[1], 0.541176f) && Near(l.fogColor[2], 0.929412f),
          "Nebelfarbe = Fog-Werte 2..4 (Himmelblau)");
    Check(Near(l.fogEnd, 5000.0f) && Near(l.fogStart, 5000.0f * (1.0f - 0.57f), 0.05f),
          "Nebel von Frustum*(1-Tiefe) bis Frustum");
    Check(!l.fogEnabled, "Nebel standardmäßig aus (Editor schaltet ihn je Modus)");

    // Ohne Licht-Fußteil (SHMD-Variante): Sonne neutral, nur GlobalLight.
    const auto noFooter = app::SceneLightingFromEnvironment(env, false);
    Check(Near(noFooter.sun[1], 1.0f) && Near(noFooter.ambient[1], 0.792157f), "Variante ohne Licht-Fußteil");

    // Weitere echte NA2016-Karten (optional als weitere Argumente): Nebelfarbe = Hintergrundfarbe
    // bei SwaDn01 exakt und bei Bera bis auf eine 8-Bit-Stufe (Beleg, dass Fog-Werte 2..4 eine Horizontfarbe sind); SwaDn01 ist die
    // SHMD-Variante ohne Licht-Fußteil.
    for (int a = 2; a < argc; ++a) {
        const auto other = core::legacy::ParseLegacyShmd(argv[a]);
        Check(other.has_value(), "weitere SHMD geladen");
        if (!other) continue;
        const auto ol = app::SceneLightingFromEnvironment(other->environment, other->hasLightingFooter);
        const std::string name = std::filesystem::path(argv[a]).stem().string();
        std::printf("         %s: Fog-Farbe %.3f %.3f %.3f | Hintergrund %.3f %.3f %.3f | Nebel %.0f..%.0f | Fußteil %s\n",
                    name.c_str(), ol.fogColor[0], ol.fogColor[1], ol.fogColor[2], ol.background[0], ol.background[1],
                    ol.background[2], ol.fogStart, ol.fogEnd, other->hasLightingFooter ? "ja" : "nein");
        if (name == "bera" || name == "SwaDn01") {
            bool same = true;
            // Bera: 199/255 gegen 198/255 im Rotkanal - gleich bis auf eine 8-Bit-Stufe.
            for (int c = 0; c < 3; ++c) same = same && Near(ol.fogColor[c], ol.background[c], 1.5f / 255.0f);
            Check(same, (name + ": Nebelfarbe = BackGroundColor (höchstens eine 8-Bit-Stufe Abweichung)").c_str());
        }
        if (name == "SwaDn01") Check(!other->hasLightingFooter && Near(ol.sun[0], 1.0f), "SwaDn01: ohne Licht-Fußteil -> neutrale Sonne");
    }

    // Grenzfälle: ungültiges Frustum, Tiefe 0.
    core::SceneEnvironment odd;
    odd.frustumFar = 0.0f;
    odd.fog[0] = 0.0f;
    const auto o = app::SceneLightingFromEnvironment(odd, true);
    Check(Near(o.fogEnd, 5000.0f) && o.fogEnd - o.fogStart >= 1.0f, "ungültiges Frustum -> 5000, Nebelbereich nie leer");

    if (g_failures) { std::fprintf(stderr, "%d Fehler\n", g_failures); return 1; }
    std::printf("Alle Szenenlicht-Tests bestanden.\n");
    return 0;
}
