// Deliberately exercise the byte reader as well as public loading: a valid model
// alone cannot detect two compensating block-offset errors.
#include "../src/core/NifModel.cpp"
#include <iostream>

using namespace theseed::mapeditor::core;

namespace {
int failures = 0;
void check(bool ok, const char* name) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
struct Bytes {
    std::vector<std::uint8_t> data;
    void u8(std::uint8_t n) { data.push_back(n); }
    void u16(std::uint16_t n) { u8(n & 255); u8(n >> 8); }
    void u32(std::uint32_t n) { u16(n & 65535); u16(n >> 16); }
    void zero(std::size_t n) { data.insert(data.end(), n, 0); }
    void str(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); data.insert(data.end(), s.begin(), s.end()); }
};
Bytes particles(std::uint32_t version, bool arrays, bool system, int meshCount = -1) {
    Bytes b;
    constexpr std::uint16_t n = 2;
    if (version >= 0x0A020000u) b.u32(0x12345678); // not a zero padding assumption
    b.u16(n); b.u8(1); b.u8(0); b.u8(arrays);
    if (arrays) b.zero(n * 12);
    b.u16(arrays ? 0x1002 : 0); // tangents + TWO UV sets
    b.u8(arrays);
    if (arrays) b.zero(n * 36);
    b.zero(16); b.u8(arrays);
    if (arrays) b.zero(n * 16 + n * 8 * 2);
    b.u16(0x4000);
    if (version >= 0x14000004u) b.u32(0xFFFFFFFF);
    b.u8(arrays); if (arrays) b.zero(n * 4); // radii
    b.u16(n);
    b.u8(arrays); if (arrays) b.zero(n * 4); // sizes
    b.u8(arrays); if (arrays) b.zero(n * 16); // quaternions
    if (version >= 0x14000004u) {
        b.u8(arrays); if (arrays) b.zero(n * 4);
        b.u8(arrays); if (arrays) b.zero(n * 12);
    }
    if (system) {
        b.zero(n * (version <= 0x0A040001u ? 40 : 28));
        if (version >= 0x14000004u) { b.u8(arrays); if (arrays) b.zero(n * 4); }
        b.u16(1); b.u16(2);
    }
    if (meshCount >= 0) {
        if (version >= 0x0A020000u) {
            b.u32(37); b.u8(1); b.u32(meshCount);
            for (int i = 0; i < meshCount; ++i) b.u32(i + 7);
        }
        b.u32(123); // final link
    }
    return b;
}
void checkLayout(const Bytes& b, std::uint32_t version, bool system, bool mesh) {
    auto bytes = b.data;
    const auto end = bytes.size();
    bytes.insert(bytes.end(), {0x12, 0x34, 0x56, 0x78});
    ByteReader r(bytes);
    if (system) SkipNiPSysData(r, version, mesh);
    else SkipNiParticlesData(r, version);
    check(r.Ok() && r.Pos() == end && r.U32() == 0x78563412, "exact next-block boundary");
    // Every truncation must fail, including truncated counted arrays and links.
    for (std::size_t length = 0; length < end; ++length) {
        std::vector<std::uint8_t> cut(bytes.begin(), bytes.begin() + length);
        ByteReader shortReader(cut);
        if (system) SkipNiPSysData(shortReader, version, mesh);
        else SkipNiParticlesData(shortReader, version);
        check(!shortReader.Ok(), "truncated particle block rejected");
    }
}
}

int main(int argc, char** argv) {
    // Material and SourceTexture own their fields; neither may consume bytes
    // from a following GeometryData or PixelData block.
    for (auto version : {0x0A010000u, 0x0A020000u, 0x14000004u}) {
        for (int external : {0, 1}) {
            Bytes b; b.str("named texture"); b.u32(1); b.u32(7); b.u32(0xFFFFFFFF);
            b.u8(external); b.str("texture.dds"); b.u32(9);
            b.u32(0); b.u32(1); b.u32(2); b.u8(1);
            if (version >= 0x0A01006Au) b.u8(1);
            const auto end = b.data.size(); b.u32(0x12345678);
            ByteReader r(b.data); r.SetVersion(version);
            const auto tex = ParseNiSourceTexture(r);
            check(r.Ok() && r.Pos() == end && tex.filename == "texture.dds" &&
                  tex.pixelDataRef == 9 && tex.useExternal == external, "source texture owns format preferences");
        }
    }
    {
        Bytes b; b.str("named material"); b.u32(0); b.u32(0xFFFFFFFF); b.zero(14 * 4);
        const auto end = b.data.size(); b.u32(0x12345678);
        ByteReader r(b.data); ParseNiMaterialProperty(r, true);
        check(r.Ok() && r.Pos() == end && r.U32() == 0x12345678, "material never reads geometry group ID");
    }
    {
        Bytes b; b.u8(1); b.u32(16);
        for (int i = 0; i < 16; ++i) { b.u8(i); b.u8(5); b.u8(6); b.u8(7); }
        ByteReader r(b.data); const auto palette = ParseNiPalette(r);
        check(r.Ok() && r.Remaining() == 0 && palette.rgba[15*4] == 15 && palette.rgba[3] == 7,
              "palette entry count and RGBA contents");
    }
    for (const auto version : {0x0A010000u, 0x0A020000u, 0x14000004u}) {
        for (bool arrays : {false, true}) {
            checkLayout(particles(version, arrays, false), version, false, false);
            checkLayout(particles(version, arrays, true), version, true, false);
            for (int count : {0, 1, 3, 19})
                checkLayout(particles(version, arrays, true, count), version, true, true);
        }
    }
    {
        auto b = particles(0x14000004u, false, true, 0);
        const auto countOffset = b.data.size() - 8;
        for (int i = 0; i < 4; ++i) b.data[countOffset + i] = 255;
        ByteReader r(b.data);
        SkipNiPSysData(r, 0x14000004u, true);
        check(!r.Ok(), "oversized NiMeshPSysData array rejected");
    }
    // Modifier base with nonempty name, plus independently specified suffix sizes.
    for (const auto& [parser, suffix] : std::vector<std::pair<void(*)(ByteReader&), int>>{
             {SkipNiPSysCylinderEmitter, 68}, {SkipNiPSysSphereEmitter, 64}, {SkipNiPSysBombModifier, 32}}) {
        Bytes b; b.str("Particle modifier"); b.u32(0); b.u32(0); b.u8(1); b.zero(suffix);
        ByteReader r(b.data); parser(r);
        check(r.Ok() && r.Pos() == b.data.size(), "modifier/emitter field layout");
    }
    if (argc != 2) { std::cerr << "Fixture directory required\n"; return 2; }
    const std::filesystem::path fixtures(argv[1]);
    for (const char* name : {"machine.nif", "machine_Urg.nif", "Yak_VaporPower.nif", "Back_Shamrock.nif", "StaXReward01.nif"}) {
        auto model = LoadNifMesh(fixtures / name, false);
        check(model.has_value(), name);
        if (!model) { std::cerr << model.error() << '\n'; continue; }
        std::size_t vertices = 0, triangles = 0, textures = 0;
        for (const auto& p : model->parts) {
            vertices += p.positions.size(); triangles += p.triangleIndices.size() / 3;
            if (p.embeddedDiffuseTexture) ++textures;
            for (const auto i : p.triangleIndices) check(i < p.positions.size(), "valid triangle index");
        }
        if (std::string(name).starts_with("machine")) {
            check(model->parts.size() == 13 && vertices == 2978 && triangles == 1760 && textures == 12,
                  "machine: 13 parts / 2978 vertices / 1760 triangles / 12 textures");
        }
        if (std::string(name) == "DarkVally_Frog.nif") {
            check(model->textureEffectBlocks > 0, "real fixture contains NiTextureEffect");
            check(model->textureEffectEnvironmentSphereBlocks > 0,
                  "real fixture contains verified environment/sphere effect");
            const auto effectPart = std::find_if(model->parts.begin(), model->parts.end(), [](const auto& part) {
                return std::any_of(part.textureEffects.begin(), part.textureEffects.end(), [](const auto& effect) {
                    return effect.enabled && effect.textureType == 2u && effect.coordGenType == 2u &&
                           effect.sourceTextureRef >= 0;
                });
            });
            check(effectPart != model->parts.end(),
                  "NiTextureEffect node binding reaches affected real mesh part");
            if (effectPart != model->parts.end()) {
                const auto effect = std::find_if(effectPart->textureEffects.begin(), effectPart->textureEffects.end(),
                    [](const auto& candidate) {
                        return candidate.enabled && candidate.textureType == 2u &&
                               candidate.coordGenType == 2u;
                    });
                check(effect != effectPart->textureEffects.end() &&
                      (!effect->texture.empty() || effect->embeddedTexture || effect->sourceUsesEmbeddedPixelData),
                      "NiTextureEffect source is preserved/resolved");
            }
        }
        std::cout << name << ": " << model->parts.size() << " parts, " << vertices << " vertices, " << triangles << " triangles\n";
    }
    for (const char* name : {"RouTempDn01_ground.nif", "treeThin.nif", "DarkVally_Frog.nif", "MapLinkGate2.nif"}) {
        const auto model = LoadNifMesh(fixtures / name, false);
        check(model && !model->parts.empty() && !model->recovered && !model->partial, name);
        if (!model) std::cerr << model.error() << '\n';
    }
    {
        const auto gate = LoadNifMesh(fixtures / "MapLinkGate2.nif", false);
        check(gate.has_value(), "MapLinkGate2 particle metadata loads");
        if (gate) {
            check(gate->particleSystemBlocks == gate->particleSystems.size(),
                  "particle block count matches preserved particle system descriptors");
            check(!gate->particleSystems.empty(),
                  "MapLinkGate2 exposes authored particle systems instead of silently skipping them");
            std::size_t systemsWithData = 0;
            std::size_t authoredParticles = 0;
            for (const auto& system : gate->particleSystems) {
                check(system.dataRef >= 0, "particle system keeps data block reference");
                check(!system.modifierRefs.empty(), "particle system keeps modifier wiring");
                check(system.modifierTypes.size() == system.modifierRefs.size(),
                      "particle modifier references resolve to explicit block types");
                for (const auto& modifierType : system.modifierTypes)
                    check(modifierType != "<invalid>", "particle modifier type reference is valid");
                if (system.hasParticleData) {
                    ++systemsWithData;
                    check(system.particleData.activeCount <= system.particleData.capacity,
                          "particle active count never exceeds authored capacity");
                    check(system.particleData.particles.size() == system.particleData.capacity,
                          "particle state array preserves every authored slot");
                    authoredParticles += system.particleData.particles.size();
                }
            }
            check(systemsWithData == gate->particleSystems.size(),
                  "every MapLinkGate2 particle system resolves its NiPSysData block");
            check(authoredParticles > 0u,
                  "MapLinkGate2 preserves authored particle positions/state for renderer bootstrap");
        }
    }
    // Strict standard loading must reject both missing and trailing footer bytes.
    std::ifstream input(fixtures / "machine.nif", std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    bytes.push_back(0);
    check(!LoadNifMeshData(bytes, 0, 0, false, nullptr, nullptr, nullptr, false, false, false), "trailing byte rejected");
    bytes.resize(bytes.size() - 2);
    check(!LoadNifMeshData(bytes, 0, 0, false, nullptr, nullptr, nullptr, false, false, false), "truncated footer rejected");
    return failures ? 1 : 0;
}
