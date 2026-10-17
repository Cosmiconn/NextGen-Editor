#include "mapeditor/core/NifPatch.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace theseed::mapeditor::core {

namespace {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

float ReadFloat(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    float f = 0.0f;
    std::memcpy(&f, bytes.data() + offset, sizeof(f));
    return f;
}

void WriteFloat(std::vector<std::uint8_t>& bytes, std::size_t offset, float value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

// NiAVObject-Transform im Dateirahmen (wie NifModel::applySceneTransform): p' = s * R * p + t,
// R zeilenweise.
Vec3 Apply(const NifBlockLayout& b, const Vec3& p) {
    const auto& R = b.rotation;
    return {(R[0] * p.x + R[1] * p.y + R[2] * p.z) * b.scale + b.translation.x,
            (R[3] * p.x + R[4] * p.y + R[5] * p.z) * b.scale + b.translation.y,
            (R[6] * p.x + R[7] * p.y + R[8] * p.z) * b.scale + b.translation.z};
}

// Umkehrung (R orthonormal): p = R^T * (p' - t) / s.
Vec3 Unapply(const NifBlockLayout& b, const Vec3& q) {
    const auto& R = b.rotation;
    const double s = b.scale;
    const Vec3 d{(q.x - b.translation.x) / s, (q.y - b.translation.y) / s, (q.z - b.translation.z) / s};
    return {R[0] * d.x + R[3] * d.y + R[6] * d.z, R[1] * d.x + R[4] * d.y + R[7] * d.z, R[2] * d.x + R[5] * d.y + R[8] * d.z};
}

bool Orthonormal(const std::array<float, 9>& R) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double dot = 0.0;
            for (int k = 0; k < 3; ++k) dot += static_cast<double>(R[i * 3 + k]) * R[j * 3 + k];
            if (std::abs(dot - (i == j ? 1.0 : 0.0)) > 1e-3) return false;
        }
    return true;
}

// Transformkette des Geometrie-Knotens bis zur Wurzel (erst der Knoten selbst, dann die Eltern).
std::vector<const NifBlockLayout*> Chain(const NifByteLayout& layout, std::int32_t geometryBlock) {
    std::vector<const NifBlockLayout*> chain;
    for (std::int32_t b = geometryBlock; b >= 0 && static_cast<std::size_t>(b) < layout.blocks.size() && chain.size() < 64;
         b = layout.blocks[static_cast<std::size_t>(b)].parent) {
        if (layout.blocks[static_cast<std::size_t>(b)].transformOffset != 0) chain.push_back(&layout.blocks[static_cast<std::size_t>(b)]);
    }
    return chain;
}

std::unexpected<std::string> Fail(const std::string& message) { return std::unexpected(message); }

} // namespace

NifRect NifPartRect(const NifMeshPart& part) {
    if (part.positions.empty()) return {};
    float minX = part.positions.front().x, maxX = minX, minY = part.positions.front().z, maxY = minY;
    for (const auto& p : part.positions) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.z);
        maxY = std::max(maxY, p.z);
    }
    return {minX, minY, maxX - minX, maxY - minY};
}

NifPartPatchCheck CheckNifPartPatchable(const NifByteLayout& layout, std::size_t partIndex) {
    NifPartPatchCheck check;
    auto refuse = [&](const char* de, const char* en) {
        check.patchable = false;
        check.reason = de;
        check.reasonEn = en;
        return check;
    };
    if (partIndex >= layout.model.parts.size()) return refuse("Element nicht gefunden.", "Element not found.");
    const auto& part = layout.model.parts[partIndex];
    const auto g = part.geometryBlock, d = part.dataBlock;
    if (g < 0 || d < 0 || static_cast<std::size_t>(g) >= layout.blocks.size() || static_cast<std::size_t>(d) >= layout.blocks.size())
        return refuse("Herkunft des Elements in der Datei unbekannt.", "The element's origin in the file is unknown.");
    const auto& data = layout.blocks[static_cast<std::size_t>(d)];
    if (data.type != "NiTriShapeData" || data.vertexOffset == 0 || data.boundOffset == 0)
        return refuse("Nur NiTriShapeData-Elemente lassen sich verschieben.", "Only NiTriShapeData elements can be moved.");
    if (data.vertexCount != part.positions.size())
        return refuse("Vertexzahl passt nicht zum Datenblock.", "Vertex count does not match the data block.");
    if (part.skinned || part.billboard)
        return refuse("Geskinnte oder Billboard-Elemente werden nicht verschoben.", "Skinned or billboard elements are not moved.");
    check.sharedUsers = 0;
    for (const auto& other : layout.model.parts)
        if (other.dataBlock == d) ++check.sharedUsers;
    if (check.sharedUsers > 1) {
        check.patchable = false;
        check.reason = "Die Geometrie wird von " + std::to_string(check.sharedUsers) + " Elementen geteilt - Verschieben würde alle bewegen.";
        check.reasonEn = "The geometry is shared by " + std::to_string(check.sharedUsers) + " elements - moving it would move all of them.";
        return check;
    }
    for (const auto* b : Chain(layout, g)) {
        if (!Orthonormal(b->rotation) || std::abs(b->scale) < 1e-6f)
            return refuse("Transformkette nicht umkehrbar (Rotation/Skalierung).", "Transform chain is not invertible (rotation/scale).");
    }
    check.patchable = true;
    return check;
}

std::expected<std::vector<std::uint8_t>, std::string> PatchNifPartRect(const std::vector<std::uint8_t>& bytes, std::size_t partIndex,
                                                                      const NifRect& target) {
    auto layout = LoadNifByteLayout(bytes);
    if (!layout) return Fail("Datei lädt nicht im strikten Standardpfad: " + layout.error());
    const auto check = CheckNifPartPatchable(*layout, partIndex);
    if (!check.patchable) return Fail(check.reason);
    const auto& part = layout->model.parts[partIndex];
    const auto& data = layout->blocks[static_cast<std::size_t>(part.dataBlock)];
    const auto chain = Chain(*layout, part.geometryBlock);
    if (data.vertexOffset + std::size_t{12} * data.vertexCount > bytes.size() || data.boundOffset + 16 > bytes.size())
        return Fail("Datenblock liegt außerhalb der Datei.");

    // Lokale Vertices (Dateirahmen) lesen und mit der Kette gegen die geladenen Positionen prüfen.
    std::vector<Vec3> local(data.vertexCount);
    for (std::uint32_t i = 0; i < data.vertexCount; ++i) {
        const std::size_t o = data.vertexOffset + std::size_t{12} * i;
        local[i] = {ReadFloat(bytes, o), ReadFloat(bytes, o + 4), ReadFloat(bytes, o + 8)};
        Vec3 w = local[i];
        for (const auto* b : chain) w = Apply(*b, w);
        // Editor-Rahmen = (Datei-X, Datei-Z, Datei-Y).
        const auto& p = part.positions[i];
        const double tol = 1e-3 * (1.0 + std::abs(w.x) + std::abs(w.y) + std::abs(w.z));
        if (std::abs(w.x - p.x) > tol || std::abs(w.y - p.z) > tol || std::abs(w.z - p.y) > tol)
            return Fail("Transformkette passt nicht zu den geladenen Positionen - nichts geschrieben.");
    }

    // Abbildung altes Rechteck -> Zielrechteck (Editor X/Z = Datei X/Y), Datei-Z bleibt.
    const NifRect old = NifPartRect(part);
    const double sx = old.width > 1e-6f ? static_cast<double>(target.width) / old.width : 1.0;
    const double sy = old.height > 1e-6f ? static_cast<double>(target.height) / old.height : 1.0;
    std::vector<std::uint8_t> out = bytes;
    float minX = 0, maxX = 0, minY = 0, maxY = 0, minZ = 0, maxZ = 0;
    std::vector<Vec3> newLocal(data.vertexCount);
    for (std::uint32_t i = 0; i < data.vertexCount; ++i) {
        Vec3 w = local[i];
        for (const auto* b : chain) w = Apply(*b, w);
        w.x = target.x + (w.x - old.x) * sx;
        w.y = target.y + (w.y - old.y) * sy;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) w = Unapply(**it, w);
        newLocal[i] = w;
        const std::size_t o = data.vertexOffset + std::size_t{12} * i;
        WriteFloat(out, o, static_cast<float>(w.x));
        WriteFloat(out, o + 4, static_cast<float>(w.y));
        WriteFloat(out, o + 8, static_cast<float>(w.z));
        const float fx = static_cast<float>(w.x), fy = static_cast<float>(w.y), fz = static_cast<float>(w.z);
        if (i == 0) { minX = maxX = fx; minY = maxY = fy; minZ = maxZ = fz; }
        minX = std::min(minX, fx); maxX = std::max(maxX, fx);
        minY = std::min(minY, fy); maxY = std::max(maxY, fy);
        minZ = std::min(minZ, fz); maxZ = std::max(maxZ, fz);
    }
    // Bounding-Sphere des Datenblocks neu: Mittelpunkt der Box, Radius bis zum fernsten Vertex.
    const Vec3 center{(minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5};
    double radius = 0.0;
    for (const auto& v : newLocal)
        radius = std::max(radius, std::sqrt((v.x - center.x) * (v.x - center.x) + (v.y - center.y) * (v.y - center.y) +
                                            (v.z - center.z) * (v.z - center.z)));
    WriteFloat(out, data.boundOffset, static_cast<float>(center.x));
    WriteFloat(out, data.boundOffset + 4, static_cast<float>(center.y));
    WriteFloat(out, data.boundOffset + 8, static_cast<float>(center.z));
    WriteFloat(out, data.boundOffset + 12, static_cast<float>(radius));

    // Gegenprobe: nur Vertex- und Bound-Bytes geändert, strikt neu ladbar, Ziel erreicht, Rest gleich.
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (out[i] == bytes[i]) continue;
        const bool inVerts = i >= data.vertexOffset && i < data.vertexOffset + std::size_t{12} * data.vertexCount;
        const bool inBound = i >= data.boundOffset && i < data.boundOffset + 16;
        if (!inVerts && !inBound) return Fail("Gegenprobe: unerwartete Byte-Änderung - nichts geschrieben.");
    }
    auto reloaded = LoadNifByteLayout(out);
    if (!reloaded) return Fail("Gegenprobe: geänderte Datei lädt nicht mehr - nichts geschrieben (" + reloaded.error() + ")");
    if (reloaded->model.parts.size() != layout->model.parts.size()) return Fail("Gegenprobe: Elementzahl geändert - nichts geschrieben.");
    const NifRect now = NifPartRect(reloaded->model.parts[partIndex]);
    const float tol = 0.01f + 1e-5f * (std::abs(target.x) + std::abs(target.y) + target.width + target.height);
    if (std::abs(now.x - target.x) > tol || std::abs(now.y - target.y) > tol ||
        (old.width > 1e-6f && std::abs(now.width - target.width) > tol) || (old.height > 1e-6f && std::abs(now.height - target.height) > tol))
        return Fail("Gegenprobe: Element liegt nicht im Zielrechteck - nichts geschrieben.");
    for (std::size_t q = 0; q < reloaded->model.parts.size(); ++q) {
        if (q == partIndex) continue;
        const auto& a = layout->model.parts[q].positions;
        const auto& b = reloaded->model.parts[q].positions;
        if (a.size() != b.size()) return Fail("Gegenprobe: anderes Element verändert - nichts geschrieben.");
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) return Fail("Gegenprobe: anderes Element verändert - nichts geschrieben.");
    }
    return out;
}

} // namespace theseed::mapeditor::core
