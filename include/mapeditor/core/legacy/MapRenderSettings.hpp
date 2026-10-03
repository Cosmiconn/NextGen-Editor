#pragma once
// MapRenderSettings.hpp
// Karten-lokale Render-Daten des Fiesta-Clients (NA2016), getrennt von den vier Kernmodulen:
//
//  1. Terrain-Vertex-Color-Bitmap (`#VerTexColorTexture` in <Karte>.ini, z.B.
//     resmap/field/Rou/Rouvertexcolor2.bmp): 24-bit-BMP mit exakt einer Farbe je HTD-Vertex
//     (Rou: 257x257 zu 257x257 Heightmap). Gleiche Zeilenkonvention wie die Blend-BMPs
//     (CHANGELOG [0.44.27]): Dateizeile y == Gitterzeile z, KEIN Bottom-up-Flip.
//  2. <Karte>.conf (Klartext-INI), z.B. Rou.conf:
//        [WorldSetting]       Ground_DL_Enable=TRUE
//        [GlowScreenEffect]   Glowness / BlendFactor / GaussFactor / DownScaling / NumBlurring
//     Alle Abschnitte/Schlüssel bleiben zusätzlich roh erhalten, damit unbekannte Einträge
//     sichtbar sind statt still verworfen zu werden.
//
// Die genaue Client-Formel, mit der Vertexfarbe, Bodenlicht und Glow verrechnet werden, ist
// NICHT aus Client-Code belegt. Der Editor-Renderer nutzt dokumentierte Näherungen (siehe
// docs/RENDERER.md); diese Datei liest nur die Daten.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace theseed::mapeditor::core::legacy {

struct RgbImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb; // Zeile z, Spalte x -> rgb[(z*width + x)*3 + {0,1,2}] = R,G,B

    [[nodiscard]] bool Empty() const noexcept { return width == 0 || height == 0; }
    [[nodiscard]] const std::uint8_t* At(std::uint32_t x, std::uint32_t z) const noexcept {
        return rgb.data() + (static_cast<std::size_t>(z) * width + x) * 3u;
    }
};

// Liest unkomprimierte 24-bit-BMPs (BI_RGB). Zeile y der Datei wird Zeile z des Bildes.
std::expected<RgbImage, std::string> ReadBmpRgb(const std::filesystem::path& file);

struct GlowScreenEffect {
    bool present = false;
    float glowness = 0.0f;
    float blendFactor = 0.0f;
    float gaussFactor = 1.0f;
    float downScaling = 8.0f;
    int numBlurring = 0;
};

struct MapRenderConfig {
    bool loaded = false;
    // [WorldSetting] Ground_DL_Enable: ob das gerichtete Licht auf den Boden wirkt.
    // Fehlt der Eintrag, gilt true (Standardbeleuchtung des Editors bleibt aktiv).
    bool groundDirectionalLight = true;
    bool groundDirectionalLightPresent = false;
    GlowScreenEffect glow;
    // Alle Einträge roh: Abschnitt -> (Schlüssel -> Wert), Reihenfolge alphabetisch.
    std::map<std::string, std::map<std::string, std::string>> sections;
    std::vector<std::string> warnings;
};

// Robust gegen CRLF, Leerzeichen um '=', Kommentare (';' oder '#'), Groß-/Kleinschreibung der
// Bool-Werte (TRUE/true/1). Ungültige Zahlen erzeugen eine Warnung und behalten den Default.
[[nodiscard]] MapRenderConfig ParseMapRenderConfig(std::string_view text);
std::expected<MapRenderConfig, std::string> LoadMapRenderConfig(const std::filesystem::path& file);

} // namespace theseed::mapeditor::core::legacy
