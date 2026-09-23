#include "mapeditor/core/TextureLayerIO.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

namespace {

constexpr char kTstexMagic[4] = {'T', 'S', 'T', 'X'};
constexpr std::uint32_t kTstexVersion = 1;

template <typename T>
void WriteRaw(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

void WriteString(std::ofstream& out, const std::string& s) {
    const auto len = static_cast<std::uint32_t>(s.size());
    WriteRaw(out, len);
    out.write(s.data(), static_cast<std::streamsize>(s.size()));
}

bool ReadString(std::ifstream& in, std::string& s) {
    std::uint32_t len = 0;
    if (!ReadRaw(in, len)) return false;
    s.resize(len);
    if (len > 0) {
        in.read(s.data(), static_cast<std::streamsize>(len));
    }
    return static_cast<bool>(in) || len == 0;
}

} // namespace

std::expected<TextureLayerStack, std::string> LoadTsTex(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte Datei nicht \u00f6ffnen: " + file.string());
    }

    char magic[4]{};
    in.read(magic, sizeof(magic));
    if (!in || std::memcmp(magic, kTstexMagic, sizeof(magic)) != 0) {
        return std::unexpected("Ung\u00fcltige .tstex-Datei (Magic stimmt nicht): " + file.string());
    }

    std::uint32_t version = 0, width = 0, height = 0, layerCount = 0;
    if (!ReadRaw(in, version) || !ReadRaw(in, width) || !ReadRaw(in, height) || !ReadRaw(in, layerCount)) {
        return std::unexpected("Unerwartetes Dateiende im Header: " + file.string());
    }
    if (version != kTstexVersion) {
        return std::unexpected("Nicht unterst\u00fctzte .tstex-Version: " + std::to_string(version));
    }

    TextureLayerStack stack(width, height);
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;

    for (std::uint32_t i = 0; i < layerCount; ++i) {
        std::string name, diffuse;
        float uvScale = 1.0f;
        if (!ReadString(in, name) || !ReadString(in, diffuse) || !ReadRaw(in, uvScale)) {
            return std::unexpected("Unerwartetes Dateiende in Layer-Metadaten (Index " + std::to_string(i) + "): " + file.string());
        }

        std::vector<std::uint8_t> raw(pixelCount);
        in.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(pixelCount));
        if (!in) {
            return std::unexpected("Unerwartetes Dateiende in Blend-Daten (Layer " + std::to_string(i) + "): " + file.string());
        }

        const std::size_t idx = stack.AddLayer(std::move(name), std::move(diffuse), uvScale);
        auto data = stack.Layer(idx).blend.MutableData();
        for (std::size_t p = 0; p < pixelCount; ++p) {
            data[p] = static_cast<float>(raw[p]) / 255.0f;
        }
    }

    return stack;
}

std::expected<void, std::string> SaveTsTex(const TextureLayerStack& stack, const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte Datei nicht zum Schreiben \u00f6ffnen: " + file.string());
    }

    out.write(kTstexMagic, sizeof(kTstexMagic));
    WriteRaw(out, kTstexVersion);
    WriteRaw(out, stack.Width());
    WriteRaw(out, stack.Height());
    WriteRaw(out, static_cast<std::uint32_t>(stack.LayerCount()));

    const std::size_t pixelCount = static_cast<std::size_t>(stack.Width()) * stack.Height();
    std::vector<std::uint8_t> raw(pixelCount);

    for (std::size_t i = 0; i < stack.LayerCount(); ++i) {
        const auto& layer = stack.Layer(i);
        WriteString(out, layer.name);
        WriteString(out, layer.diffuseFileName);
        WriteRaw(out, layer.uvScaleDiffuse);

        const auto data = layer.blend.Data();
        for (std::size_t p = 0; p < pixelCount; ++p) {
            raw[p] = static_cast<std::uint8_t>(std::clamp(data[p], 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    }

    if (!out) {
        return std::unexpected("Fehler beim Schreiben: " + file.string());
    }
    return {};
}

TextureLayerStack BuildTextureLayerStackFromLegacyIni(const legacy::LegacyMapIni& ini) {
    // WICHTIG: Auflösung NICHT von der Heightmap übernehmen - echte Referenzkarten zeigen
    // Blend-Bitmaps in fester, von der Heightmap-Auflösung unabhängiger Größe (z.B. Bera:
    // 257x257-Heightmap, aber 512x512-Blend-BMPs), siehe docs/MAP_FORMAT.md. Ohne echte BMP-Daten
    // (reiner ini-Metadaten-Import) ist die tatsächliche Auflösung unbekannt - 512x512 ist der in
    // allen bisher gesichteten Kartensets beobachtete Wert, aber eine reine Verlegenheitslösung,
    // kein aus der ini hergeleiteter Wert. Für eine belastbare Auflösung siehe
    // legacy::ImportLegacyTextureSet, das die echten BMP-Dateien liest.
    constexpr std::uint32_t kPlaceholderResolution = 512;
    TextureLayerStack stack(kPlaceholderResolution, kPlaceholderResolution);
    for (const auto& layerDef : ini.layers) {
        stack.AddLayer(layerDef.name, layerDef.diffuseFileName, layerDef.uvScaleDiffuse);
    }
    return stack;
}

} // namespace theseed::mapeditor::core
