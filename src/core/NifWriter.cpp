// NifWriter.cpp - siehe NifWriter.hpp.

#include "mapeditor/core/NifWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace theseed::mapeditor::core {
namespace {

struct Span { std::size_t begin = 0, end = 0; };

// Bytebereich eines Blocks inklusive des Markers davor (bis 10.1.0.106); der letzte Block endet am Fuß.
Span BlockSpan(const NifByteLayout& layout, std::size_t i) {
    const std::size_t marker = layout.blockMarkers ? 4 : 0;
    const std::size_t begin = layout.blocks[i].offset - marker;
    const std::size_t end = i + 1 < layout.blocks.size() ? layout.blocks[i + 1].offset - marker : layout.footerOffset;
    return {begin, end};
}

bool SameBytes(const std::vector<std::uint8_t>& a, Span sa, const std::vector<std::uint8_t>& b, Span sb) {
    return sa.end - sa.begin == sb.end - sb.begin && sa.end <= a.size() && sb.end <= b.size() &&
           std::equal(a.begin() + static_cast<std::ptrdiff_t>(sa.begin), a.begin() + static_cast<std::ptrdiff_t>(sa.end),
                      b.begin() + static_cast<std::ptrdiff_t>(sb.begin));
}

float ReadF32(const std::vector<std::uint8_t>& b, std::size_t at) {
    float v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}
void WriteF32(std::vector<std::uint8_t>& b, std::size_t at, float v) { std::memcpy(b.data() + at, &v, 4); }
void WriteU32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v) {
    for (int k = 0; k < 4; ++k) b[at + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(v >> (8 * k));
}

std::expected<NifByteLayout, std::string> Layout(const std::vector<std::uint8_t>& bytes, std::uint32_t block, const char* what) {
    auto layout = LoadNifByteLayout(bytes);
    if (!layout) return std::unexpected("NIF nicht strikt lesbar (nur strikt geparste Dateien werden geschrieben): " + layout.error());
    if (block >= layout->blocks.size()) return std::unexpected(std::string(what) + ": Block " + std::to_string(block) + " existiert nicht");
    if (layout->footerOffset == 0) return std::unexpected("NIF-Fuß nicht gefunden");
    return layout;
}

// Gegenprobe: neue Datei strikt neu lesen, Blöcke vergleichen, Wert prüfen.
std::expected<std::vector<std::uint8_t>, std::string> Verify(const std::vector<std::uint8_t>& oldBytes, const NifByteLayout& oldLayout,
                                                            std::vector<std::uint8_t> newBytes, std::uint32_t edited,
                                                            const std::function<bool(const NifByteLayout&, const std::vector<std::uint8_t>&)>& valueOk) {
    auto fresh = LoadNifByteLayout(newBytes);
    if (!fresh) return std::unexpected("Gegenprobe: neue Datei nicht strikt lesbar: " + fresh.error());
    if (fresh->blocks.size() != oldLayout.blocks.size()) return std::unexpected("Gegenprobe: Blockzahl geändert");
    if (!SameBytes(oldBytes, {0, BlockSpan(oldLayout, 0).begin}, newBytes, {0, BlockSpan(*fresh, 0).begin}))
        return std::unexpected("Gegenprobe: Dateikopf geändert");
    if (!SameBytes(oldBytes, {oldLayout.footerOffset, oldBytes.size()}, newBytes, {fresh->footerOffset, newBytes.size()}))
        return std::unexpected("Gegenprobe: Fuß geändert");
    for (std::size_t i = 0; i < oldLayout.blocks.size(); ++i) {
        if (fresh->blocks[i].type != oldLayout.blocks[i].type) return std::unexpected("Gegenprobe: Blocktyp " + std::to_string(i) + " geändert");
        if (i == edited) continue;
        if (!SameBytes(oldBytes, BlockSpan(oldLayout, i), newBytes, BlockSpan(*fresh, i)))
            return std::unexpected("Gegenprobe: Block " + std::to_string(i) + " (" + oldLayout.blocks[i].type + ") geändert");
    }
    if (!valueOk(*fresh, newBytes)) return std::unexpected("Gegenprobe: neuer Wert kommt nicht an");
    return newBytes;
}

bool Near(float a, float b) { return std::fabs(a - b) <= 1e-6f * std::max(1.0f, std::fabs(b)); }

} // namespace

std::vector<NifTextureEntry> NifTextureSources(const NifByteLayout& layout) {
    std::vector<NifTextureEntry> out;
    for (std::size_t i = 0; i < layout.blocks.size(); ++i)
        if (layout.blocks[i].textureNameOffset != 0)
            out.push_back({static_cast<std::uint32_t>(i), layout.blocks[i].textureFile, layout.blocks[i].textureExternal != 0});
    return out;
}

std::expected<NifMaterialColors, std::string> ReadNifMaterial(const std::vector<std::uint8_t>& bytes, const NifByteLayout& layout,
                                                             std::uint32_t block) {
    if (block >= layout.blocks.size() || layout.blocks[block].materialOffset == 0) return std::unexpected("kein NiMaterialProperty-Block");
    const std::size_t at = layout.blocks[block].materialOffset;
    if (at + 14 * 4 > bytes.size()) return std::unexpected("Material außerhalb der Datei");
    NifMaterialColors c;
    std::size_t p = at;
    for (auto* arr : {&c.ambient, &c.diffuse, &c.specular, &c.emissive})
        for (float& v : *arr) { v = ReadF32(bytes, p); p += 4; }
    c.glossiness = ReadF32(bytes, p);
    c.alpha = ReadF32(bytes, p + 4);
    return c;
}

std::expected<std::vector<std::uint8_t>, std::string> RewriteNifTextureFile(const std::vector<std::uint8_t>& bytes, std::uint32_t block,
                                                                           const std::string& file) {
    auto layout = Layout(bytes, block, "Textur");
    if (!layout) return std::unexpected(layout.error());
    const auto& b = layout->blocks[block];
    if (b.textureNameOffset == 0) return std::unexpected("Block " + std::to_string(block) + " (" + b.type + ") ist keine NiSourceTexture");
    if (!b.textureExternal) return std::unexpected("Eingebettete Textur - der Name hat keine Wirkung, das Bild steckt in der NIF");
    if (file.empty() || file.size() > 1024 || std::any_of(file.begin(), file.end(), [](char ch) {
            const auto c = static_cast<unsigned char>(ch);
            return c < 0x20 || c >= 0x7F;
        }))
        return std::unexpected("Dateiname leer, zu lang oder mit Sonderzeichen (nur ASCII)");
    const std::size_t lenAt = b.textureNameOffset;
    const std::size_t oldEnd = lenAt + 4 + b.textureFile.size();
    std::vector<std::uint8_t> out;
    out.reserve(bytes.size() + file.size());
    out.insert(out.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(lenAt));
    out.resize(out.size() + 4);
    WriteU32(out, lenAt, static_cast<std::uint32_t>(file.size()));
    out.insert(out.end(), file.begin(), file.end());
    out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(oldEnd), bytes.end());
    return Verify(bytes, *layout, std::move(out), block,
                  [&](const NifByteLayout& fresh, const std::vector<std::uint8_t>&) { return fresh.blocks[block].textureFile == file; });
}

std::expected<std::vector<std::uint8_t>, std::string> RewriteNifTransform(const std::vector<std::uint8_t>& bytes, std::uint32_t block,
                                                                         const NifTransform& t) {
    auto layout = Layout(bytes, block, "Transformation");
    if (!layout) return std::unexpected(layout.error());
    const auto& b = layout->blocks[block];
    if (b.transformOffset == 0) return std::unexpected("Block " + std::to_string(block) + " (" + b.type + ") hat keine Transformation");
    if (!std::isfinite(t.scale) || t.scale <= 0.0f) return std::unexpected("Skalierung muss größer als 0 sein");
    std::vector<std::uint8_t> out = bytes;
    std::size_t p = b.transformOffset;
    for (const float v : {t.translation.x, t.translation.y, t.translation.z}) { WriteF32(out, p, v); p += 4; }
    for (const float v : t.rotation) { WriteF32(out, p, v); p += 4; }
    WriteF32(out, p, t.scale);
    return Verify(bytes, *layout, std::move(out), block, [&](const NifByteLayout& fresh, const std::vector<std::uint8_t>&) {
        const auto& f = fresh.blocks[block];
        bool ok = Near(f.translation.x, t.translation.x) && Near(f.translation.y, t.translation.y) &&
                  Near(f.translation.z, t.translation.z) && Near(f.scale, t.scale);
        for (std::size_t k = 0; k < 9; ++k) ok = ok && Near(f.rotation[k], t.rotation[k]);
        return ok;
    });
}

std::expected<std::vector<std::uint8_t>, std::string> RewriteNifMaterial(const std::vector<std::uint8_t>& bytes, std::uint32_t block,
                                                                        const NifMaterialColors& c) {
    auto layout = Layout(bytes, block, "Material");
    if (!layout) return std::unexpected(layout.error());
    const auto& b = layout->blocks[block];
    if (b.materialOffset == 0) return std::unexpected("Block " + std::to_string(block) + " (" + b.type + ") ist kein NiMaterialProperty");
    std::vector<std::uint8_t> out = bytes;
    std::size_t p = b.materialOffset;
    for (const auto* arr : {&c.ambient, &c.diffuse, &c.specular, &c.emissive})
        for (const float v : *arr) { WriteF32(out, p, v); p += 4; }
    WriteF32(out, p, c.glossiness);
    WriteF32(out, p + 4, c.alpha);
    return Verify(bytes, *layout, std::move(out), block, [&](const NifByteLayout& fresh, const std::vector<std::uint8_t>& written) {
        const auto read = ReadNifMaterial(written, fresh, block);
        if (!read) return false;
        bool ok = Near(read->glossiness, c.glossiness) && Near(read->alpha, c.alpha);
        for (std::size_t k = 0; k < 3; ++k)
            ok = ok && Near(read->ambient[k], c.ambient[k]) && Near(read->diffuse[k], c.diffuse[k]) &&
                 Near(read->specular[k], c.specular[k]) && Near(read->emissive[k], c.emissive[k]);
        // Das geparste Modell muss die Farbe ebenfalls sehen (Diffuse eines Teils mit diesem Material).
        return ok;
    });
}

} // namespace theseed::mapeditor::core
