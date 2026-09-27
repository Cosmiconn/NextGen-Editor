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
    {
        Bytes b;
        b.u8(0);  // not manager-controlled, not highest-weight-only
        b.u8(1);  // one blend item
        b.zero(4); // weight threshold
        b.u8(1); b.u8(0); b.u8(3); b.u8(2); // count/index/priorities
        b.zero(16); // single time + weight sums + ease
        b.u32(17); b.zero(8); b.u8(4); b.zero(4); // blend item
        b.zero(4); // typed float value
        ByteReader r(b.data);
        r.SetVersion(0x14000004u);
        const auto blend = ParseNiBlendFloatInterpolator(r);
        check(r.Ok() && r.Remaining() == 0 && blend.blend.items.size() == 1 &&
              blend.blend.items[0].interpolatorRef == 17 &&
              blend.blend.highPriority == 3 && blend.blend.nextHighPriority == 2,
              "20.0 NiBlendFloatInterpolator payload preserved");
    }
    {
        NifParticleModifierInfo emitter;
        emitter.type = "NiPSysBoxEmitter";
        emitter.name = "Emitter";
        emitter.active = true;
        emitter.emitter = true;
        emitter.hasEmitterToParticleSystemTransform = true;
        emitter.speed = 2.0f;
        emitter.initialColor = {0.25f, 0.5f, 0.75f, 0.8f};
        emitter.initialRadius = 3.0f;
        emitter.lifeSpan = 4.0f;

        NifParticleModifierInfo grow;
        grow.type = "NiPSysGrowFadeModifier";
        grow.active = true;
        grow.growTime = 1.0f;
        grow.growGeneration = 0;

        NifParticleModifierInfo rotation;
        rotation.type = "NiPSysRotationModifier";
        rotation.active = true;
        rotation.initialRotationAngle = 0.25f;
        rotation.initialRotationSpeed = 0.5f;
        rotation.randomInitialAxis = false;
        rotation.initialAxis = {1.0f, 0.0f, 0.0f};

        std::vector<NifParticleState> emittedState(4);
        std::uint16_t active = 0;
        std::uint32_t rng = 1;
        const auto count = EmitNifParticles(
            emittedState, active, 4, emitter, {grow, rotation}, {0.25f}, 10.0f, true, true, rng);
        check(count == 1 && active == 1, "Gamebryo box emitter creates requested particle");
        check(std::abs(emittedState[0].position.x) < 1.0e-6f &&
              std::abs(emittedState[0].position.y) < 1.0e-6f &&
              std::abs(emittedState[0].position.z) < 1.0e-6f &&
              std::abs(emittedState[0].velocity.y - 2.0f) < 1.0e-6f,
              "zero-volume emitter uses legacy +Z/editor +Y velocity");
        check(std::abs(emittedState[0].age - 0.25f) < 1.0e-6f &&
              std::abs(emittedState[0].lifeSpan - 4.0f) < 1.0e-6f &&
              std::abs(emittedState[0].lastUpdate - 9.75f) < 1.0e-6f &&
              std::abs(emittedState[0].size - 0.25f) < 1.0e-6f,
              "emitter age lifespan lastUpdate and InitializeParticle grow semantics");
        check(std::abs(emittedState[0].rotationAngle - 0.25f) < 1.0e-6f &&
              std::abs(emittedState[0].rotationSpeed - 0.5f) < 1.0e-6f &&
              std::abs(emittedState[0].rotationAxis.x - 1.0f) < 1.0e-6f,
              "rotation modifier initializes emitted particles");
    }
    {
        NifParticleModifierInfo emitter;
        emitter.type = "NiPSysMeshEmitter";
        emitter.active = true;
        emitter.emitter = true;
        emitter.speed = 3.0f;
        emitter.lifeSpan = 2.0f;
        emitter.initialRadius = 1.0f;
        emitter.initialVelocityType = 0;
        emitter.emissionType = 1;
        NifParticleEmitterMesh mesh;
        mesh.positions = {{0,0,0},{3,0,0},{0,3,0}};
        mesh.normals = {{0,1,0},{0,1,0},{0,1,0}};
        mesh.triangleIndices = {0,1,2};
        emitter.emitterMeshes.push_back(mesh);
        std::vector<NifParticleState> emittedState(1);
        std::uint16_t active = 0;
        std::uint32_t rng = 7;
        const auto count = EmitNifParticles(
            emittedState, active, 1, emitter, {}, {0.0f}, 1.0f, false, false, rng);
        check(count == 1 && std::abs(emittedState[0].position.x - 1.0f) < 1.0e-6f &&
              std::abs(emittedState[0].position.y - 1.0f) < 1.0e-6f &&
              std::abs(emittedState[0].velocity.y - 3.0f) < 1.0e-6f,
              "mesh face-center emitter uses averaged face position and normal");
    }
    {
        NifParticleState particle;
        particle.velocity = {1.0f, 0.0f, 0.0f};
        particle.lifeSpan = 10.0f;

        NifParticleModifierInfo gravity;
        gravity.type = "NiPSysGravityModifier";
        gravity.active = true;
        gravity.hasForceToParticleSystemTransform = true;
        gravity.forceAxis = {0.0f, 1.0f, 0.0f};
        gravity.forceStrength = 2.0f;
        gravity.forceType = 0;

        std::vector<NifParticleState> state{particle};
        std::uint16_t active = 1;
        std::uint32_t rng = 1;
        AdvanceNifParticleState(state, active, {gravity}, 0.5f, &rng);
        check(active == 1 &&
              std::abs(state[0].velocity.x - 1.0f) < 1.0e-6f &&
              std::abs(state[0].velocity.y - 1.6f) < 1.0e-5f,
              "planar gravity uses Gamebryo strength*1.6 and force-step delta");
    }
    {
        NifParticleState particle;
        particle.velocity = {2.0f, 3.0f, 0.0f};
        particle.lifeSpan = 10.0f;

        NifParticleModifierInfo drag;
        drag.type = "NiPSysDragModifier";
        drag.active = true;
        drag.hasForceToParticleSystemTransform = true;
        drag.forceAxis = {1.0f, 0.0f, 0.0f};
        drag.dragPercentage = 0.5f;
        drag.dragRange = 100.0f;
        drag.dragRangeFalloff = 200.0f;

        std::vector<NifParticleState> state{particle};
        std::uint16_t active = 1;
        AdvanceNifParticleState(state, active, {drag}, 0.0333333f);
        check(active == 1 &&
              std::abs(state[0].velocity.x - 1.0f) < 1.0e-5f &&
              std::abs(state[0].velocity.y - 3.0f) < 1.0e-5f,
              "drag removes only projected axis velocity with 30fps-normalized percentage");
    }
    {
        std::vector<NifParticleState> state(3);
        state[0].position = {1.0f, 2.0f, 3.0f};
        state[0].velocity = {2.0f, -1.0f, 0.5f};
        state[0].age = 0.25f;
        state[0].lifeSpan = 2.0f;
        state[0].lastUpdate = 7.0f;
        state[0].spawnGeneration = 0;

        state[1].position = {10.0f, 0.0f, 0.0f};
        state[1].velocity = {1.0f, 0.0f, 0.0f};
        state[1].age = 1.9f;
        state[1].lifeSpan = 2.0f;
        state[1].spawnGeneration = 0;

        state[2].age = 0.25f;
        state[2].lifeSpan = 2.0f;
        state[2].spawnGeneration = 1;

        NifParticleModifierInfo grow;
        grow.type = "NiPSysGrowFadeModifier";
        grow.active = true;
        grow.growTime = 1.0f;
        grow.growGeneration = 0;
        grow.fadeTime = 0.5f;
        grow.fadeGeneration = 0;

        NifParticleModifierInfo color;
        color.type = "NiPSysColorModifier";
        color.active = true;
        color.hasColorTrack = true;
        color.colorTrack.interpolation = 1;
        color.colorTrack.keys = {
            NifColorKey{.time = 0.0f, .value = {1.0f, 0.0f, 0.0f, 0.0f}},
            NifColorKey{.time = 1.0f, .value = {0.0f, 0.0f, 1.0f, 1.0f}},
        };

        std::uint16_t active = 3;
        AdvanceNifParticleState(state, active, {grow, color}, 0.5f);
        check(active == 2, "particle FinalKernel removes age-expired active particles");
        check(std::abs(state[0].position.x - 2.0f) < 1.0e-6f &&
              std::abs(state[0].position.y - 1.5f) < 1.0e-6f &&
              std::abs(state[0].position.z - 3.25f) < 1.0e-6f &&
              std::abs(state[0].age - 0.75f) < 1.0e-6f &&
              std::abs(state[0].lastUpdate - 7.5f) < 1.0e-6f,
              "particle FinalKernel position age and lastUpdate semantics");
        check(std::abs(state[0].size - 0.25f) < 1.0e-6f,
              "particle GeneralKernel grow uses pre-step age");
        check(std::abs(state[0].color.r - 0.875f) < 1.0e-6f &&
              std::abs(state[0].color.b - 0.125f) < 1.0e-6f &&
              std::abs(state[0].color.a - 0.125f) < 1.0e-6f,
              "particle GeneralKernel color uses normalized pre-step lifetime");
        check(state[1].spawnGeneration == 1 && std::abs(state[1].size - 1.0f) < 1.0e-6f,
              "grow fade applies only to matching particle generation");
    }
    {
        Bytes b = particles(0x14000004u, true, true, -1);
        ByteReader r(b.data);
        const auto parsed = ParseNiPSysData(r, 0x14000004u, false);
        check(r.Ok() && parsed.info.hasRotationSpeeds &&
              parsed.info.particles.size() == 2 &&
              parsed.info.particles[0].rotationSpeed == 0.0f,
              "NiPSysData rotation-speed array is preserved");
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
                check(!system.controllers.empty(),
                      "particle ObjectNET controller chain resolves to runtime controller data");
                check(std::any_of(system.controllers.begin(), system.controllers.end(), [](const auto& controller) {
                          return controller.type == "NiPSysEmitterCtlr" && controller.hasFloatTrack;
                      }),
                      "particle system preserves its emitter rate controller track");
                check(std::any_of(system.controllers.begin(), system.controllers.end(), [](const auto& controller) {
                          return controller.type == "NiPSysUpdateCtlr";
                      }),
                      "particle system preserves its update controller");
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
            std::size_t parsedModifiers = 0;
            for (const auto& system : gate->particleSystems) {
                parsedModifiers += system.modifiers.size();
                for (const auto& modifier : system.modifiers) {
                    check(!modifier.type.empty(), "parsed particle modifier keeps its concrete type");
                    check(modifier.targetRef >= 0, "parsed particle modifier keeps target system reference");
                    if (modifier.type == "NiPSysColorModifier" && modifier.colorDataRef >= 0) {
                        check(modifier.hasColorTrack, "particle color modifier resolves NiColorData");
                        if (modifier.hasColorTrack && !modifier.colorTrack.keys.empty()) {
                            const auto first = EvaluateNifColorTrack(modifier.colorTrack,
                                                                   modifier.colorTrack.keys.front().time);
                            check(first.has_value(), "particle color track is CPU-evaluable");
                        }
                    }
                }
            }
            check(parsedModifiers > 0u, "MapLinkGate2 preserves particle modifier parameters");
            check(systemsWithData == gate->particleSystems.size(),
                  "every MapLinkGate2 particle system resolves its NiPSysData block");
            check(authoredParticles > 0u,
                  "MapLinkGate2 preserves authored particle positions/state for renderer bootstrap");
            std::size_t texturedSystems = 0;
            for (const auto& system : gate->particleSystems)
                if (system.textureSlots[0].present &&
                    (!system.textureSlots[0].texture.empty() || system.textureSlots[0].embeddedTexture))
                    ++texturedSystems;
            check(texturedSystems > 0u,
                  "MapLinkGate2 particle materials resolve their authored base textures");
            for (const auto& system : gate->particleSystems) {
                for (const auto& animation : system.textureTransformAnimations)
                    check(animation.slot < system.textureSlots.size() && animation.operation <= 4u,
                          "particle texture-transform controller targets supported slot/operation");
                for (const auto& animation : system.textureFlipAnimations)
                    check(animation.slot < system.textureSlots.size() && !animation.frames.empty(),
                          "particle flip controller resolves authored frame textures");
                for (const auto& shaderSlot : system.shaderTextureSlots)
                    check(shaderSlot.texture.present,
                          "particle ShaderTexDesc stays attached to its particle material");
            }
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
