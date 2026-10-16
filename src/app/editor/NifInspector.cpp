// NifInspector.cpp
// NIF-Asset-Inspektor.

#include "EditorShared.hpp"

namespace nge {

const char* NifTextureSlotLabel(std::size_t slot) {
    static constexpr const char* kLabels[] = {
        "Base", "Dark", "Detail", "Gloss", "Glow",
        "Bump", "Decal 0", "Decal 1", "Decal 2", "Decal 3"
    };
    return slot < std::size(kLabels) ? kLabels[slot] : L("Textur","Texture");
}
const char* NifTextureTransformOperationLabel(std::uint32_t operation) {
    switch (operation) {
        case 0: return "U-Offset";
        case 1: return "V-Offset";
        case 2: return "Rotation";
        case 3: return L("U-Skalierung","U scale");
        case 4: return L("V-Skalierung","V scale");
        default: return "Transform";
    }
}
const char* NifTextureInterpolationLabel(std::uint32_t interpolation) {
    switch (interpolation) {
        case 1: return "Linear";
        case 2: return L("Quadratisch","Quadratic");
        case 3: return "TBC";
        case 5: return L("Konstant","Constant");
        default: return L("Unbekannt","Unknown");
    }
}
const char* NifTextureExtrapolationLabel(std::uint8_t extrapolation) {
    switch (extrapolation) {
        case 0: return L("Zyklus","Cycle");
        case 1: return "Ping-Pong";
        case 2: return "Clamp";
        default: return L("Unbekannt","Unknown");
    }
}
void LoadNifAssetInspector(EditorState& state, const std::filesystem::path& path,
                           const std::string& relativeAsset) {
    state.nifInspectorAsset = relativeAsset;
    state.nifInspectorModel.reset();
    state.nifInspectorError.clear();
    state.nifInspectorResolvedTextureCache.clear();
    auto loaded = core::LoadNifMesh(path);
    if (loaded) {
        state.nifInspectorModel = std::move(*loaded);
    } else {
        state.nifInspectorError = loaded.error();
    }
}
std::optional<std::filesystem::path> ResolveNifInspectorTexturePath(
    const std::filesystem::path& root, const std::string& relativeAsset,
    const std::string& textureName) {
    if (textureName.empty()) return std::nullopt;

    // Gleiches Legacy-Layout wie der Renderer: aus einem fiktiven Feld-Verzeichnis
    // kann ResolveLegacyAssetPath auf fieldTexture und andere resmap-Bereiche aufloesen.
    const std::filesystem::path fakeMapDir = root / "field" / "_nif_inspector";
    if (auto resolved = core::legacy::ResolveLegacyAssetPath(fakeMapDir, textureName))
        return resolved;

    // Manche NIFs referenzieren nur einen Dateinamen neben dem Modell. Dieser Fallback
    // arbeitet absichtlich case-insensitiv, da die Originaldaten haeufig Windows-Pfade sind.
    const std::filesystem::path modelDir = (root / relativeAsset).parent_path();
    std::string leaf = textureName;
    if (const auto slash = leaf.find_last_of("\\/"); slash != std::string::npos)
        leaf = leaf.substr(slash + 1);
    const std::string wanted = LowerAscii(leaf);
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(modelDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (LowerAscii(entry.path().filename().string()) == wanted) return entry.path();
    }
    return std::nullopt;
}
std::optional<std::filesystem::path> ResolveNifInspectorTexturePathCached(
    EditorState& state, const std::filesystem::path& root,
    const std::string& textureName) {
    if (textureName.empty()) return std::nullopt;
    const std::string key = LowerAscii(textureName);
    if (const auto it = state.nifInspectorResolvedTextureCache.find(key);
        it != state.nifInspectorResolvedTextureCache.end()) {
        if (it->second.empty()) return std::nullopt;
        return std::filesystem::path(it->second);
    }

    std::optional<std::filesystem::path> resolved;

    // Im Interface-Workspace bildet <Projekt>/Client/resmenu einen Overlay-Layer über
    // dem read-only Client/resmenu. Dadurch sieht der NIF-Inspector nicht nur das
    // ausgewählte NIF-Override, sondern bevorzugt auch projektseitig ersetzte oder
    // projekt-only Texturabhängigkeiten. Im Karten-Asset-Browser bleibt das Verhalten
    // unverändert, weil dort root != interfaceRoot ist.
    const bool interfaceContext =
        !state.interfaceRoot.empty() &&
        std::filesystem::path(state.interfaceRoot).lexically_normal() == root.lexically_normal();
    if (interfaceContext && state.project.projectFolder[0] != '\0') {
        const std::filesystem::path overrideRoot =
            std::filesystem::path(state.project.projectFolder) / "Client" / "resmenu";
        resolved = ResolveNifInspectorTexturePath(
            overrideRoot, state.nifInspectorAsset, textureName);
    }
    if (!resolved) {
        resolved = ResolveNifInspectorTexturePath(
            root, state.nifInspectorAsset, textureName);
    }

    state.nifInspectorResolvedTextureCache.emplace(
        key, resolved ? resolved->string() : std::string{});
    return resolved;
}
EditorState::AssetThumbnail GetOrLoadEmbeddedNifInspectorThumbnail(
    EditorState& state, const core::NifEmbeddedTexture& image,
    std::size_t partIndex, std::size_t slotIndex) {
    const std::string key = "nif-embedded://" + state.nifInspectorAsset + "#" +
                            std::to_string(partIndex) + "/" + std::to_string(slotIndex);
    if (const auto it = state.assetThumbnails.find(key); it != state.assetThumbnails.end())
        return it->second;

    EditorState::AssetThumbnail thumb;
    thumb.tex = UploadThumbnailTexture(image.rgba, image.width, image.height);
    if (thumb.tex && image.height > 0)
        thumb.aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    return state.assetThumbnails.emplace(key, thumb).first->second;
}
void DrawNifAssetInspector(EditorState& state, const std::filesystem::path& root) {
    if (state.nifInspectorAsset.empty()) return;

    ImGui::Separator();
    DrawInlineIcon("nifMaterialHeaderIcon",DrawIconCube,IM_COL32(100,205,255,245),nullptr,
                   ImVec2(18,18),"panel.asset_browser");
    ImGui::SameLine(0,5);
    ImGui::TextColored(UiTheme::AccentCyan, "NIF / MATERIAL");
    ImGui::SameLine();
    ImGui::TextDisabled("%s",L("nur lesen","read-only"));
    ImGui::SameLine();
    if (UI::SmallButton(L("Neu laden##nifInspector","Reload##nifInspector"))) {
        std::filesystem::path reloadPath = root / state.nifInspectorAsset;
        const bool interfaceContext =
            !state.interfaceRoot.empty() &&
            std::filesystem::path(state.interfaceRoot).lexically_normal() == root.lexically_normal();
        if (interfaceContext && state.project.projectFolder[0] != '\0') {
            const std::filesystem::path projectCandidate =
                std::filesystem::path(state.project.projectFolder) /
                "Client" / "resmenu" / state.nifInspectorAsset;
            std::error_code projectEc;
            if (std::filesystem::is_regular_file(projectCandidate, projectEc) && !projectEc)
                reloadPath = projectCandidate;
        }
        LoadNifAssetInspector(state, reloadPath, state.nifInspectorAsset);
    }

    ImGui::TextWrapped("%s", state.nifInspectorAsset.c_str());
    if (!state.nifInspectorError.empty()) {
        ImGui::TextWrapped(L("NIF konnte nicht gelesen werden: %s","NIF could not be read: %s"), state.nifInspectorError.c_str());
        return;
    }
    if (!state.nifInspectorModel) {
        ImGui::TextDisabled("%s",L("Kein NIF ausgewählt.","No NIF selected."));
        return;
    }

    const auto& model = *state.nifInspectorModel;
    std::size_t triangleCount = 0;
    std::size_t textureCount = 0;
    std::size_t externalTextureCount = 0;
    std::size_t embeddedTextureCount = 0;
    std::size_t unresolvedEmbeddedTextureCount = 0;
    std::size_t uvFallbackTextureCount = 0;
    std::size_t missingUvTextureCount = 0;
    std::size_t missingTextureCount = 0;
    std::size_t animatedTextureCount = 0;
    std::size_t flipbookFrameCount = 0;
    std::size_t missingFlipbookFrameCount = 0;
    std::size_t unresolvedEmbeddedFlipbookFrameCount = 0;
    std::size_t genericShaderFallbackParts = 0;
    std::size_t genericShaderDescriptorCount = 0;
    std::size_t missingGenericShaderTextureCount = 0;
    std::size_t unresolvedEmbeddedGenericShaderTextureCount = 0;
    std::size_t unresolvedGenericShaderSourceCount = 0;
    std::map<std::uint32_t,std::size_t> textureApplyFallbackModes;
    std::set<std::string> missingTextureRefs;
    std::set<std::string> shaderNames;
    std::set<std::pair<std::string,std::uint32_t>> genericShaderMaps;
    for (const auto& part : model.parts) {
        if (!part.shaderName.empty()) {
            shaderNames.insert(part.shaderName);
            if (part.shaderName != "VCAlphaTextureBlender")
                ++genericShaderFallbackParts;
        }
        triangleCount += part.triangleIndices.size() / 3;
        if (part.textureApplyMode != 0u &&
            part.textureApplyMode != 1u &&
            part.textureApplyMode != 2u)
            ++textureApplyFallbackModes[part.textureApplyMode];
        for (const auto& slot : part.textureSlots) {
            if (!slot.present) continue;
            ++textureCount;
            const bool hasTextureSource =
                slot.sourceUsesEmbeddedPixelData || slot.embeddedTexture || !slot.texture.empty();
            if (hasTextureSource) {
                const bool hasSlotUvs =
                    slot.uvSet < part.uvSets.size() &&
                    part.uvSets[slot.uvSet].size() == part.positions.size();
                const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
                if (!hasSlotUvs) {
                    if (hasBaseFallbackUvs) ++uvFallbackTextureCount;
                    else ++missingUvTextureCount;
                }
            }
            if (slot.sourceUsesEmbeddedPixelData) {
                if (slot.embeddedTexture) ++embeddedTextureCount;
                else ++unresolvedEmbeddedTextureCount;
            } else if (!slot.texture.empty()) {
                ++externalTextureCount;
                if (!ResolveNifInspectorTexturePathCached(state, root, slot.texture)) {
                    ++missingTextureCount;
                    missingTextureRefs.insert(slot.texture);
                }
            }
        }

        // ShaderTexDesc ausserhalb des verifizierten VCAlphaTextureBlender-Pfads bleibt
        // bewusst Diagnose: Quellen werden exakt aufgeloest, aber mapId bekommt noch keine
        // spekulative Renderer-Semantik.
        if (!part.shaderName.empty() && part.shaderName != "VCAlphaTextureBlender") {
            for (const auto& shaderSlot : part.shaderTextureSlots) {
                ++genericShaderDescriptorCount;
                genericShaderMaps.emplace(part.shaderName, shaderSlot.mapId);
                const auto& texture = shaderSlot.texture;
                if (texture.sourceUsesEmbeddedPixelData) {
                    if (!texture.embeddedTexture)
                        ++unresolvedEmbeddedGenericShaderTextureCount;
                } else if (!texture.texture.empty()) {
                    if (!ResolveNifInspectorTexturePathCached(state, root, texture.texture)) {
                        ++missingGenericShaderTextureCount;
                        missingTextureRefs.insert(texture.texture);
                    }
                } else {
                    ++unresolvedGenericShaderSourceCount;
                }
            }
        }

        animatedTextureCount += part.textureTransformAnimations.size();
        animatedTextureCount += part.textureFlipAnimations.size();
        for (const auto& animation : part.textureFlipAnimations) {
            for (const auto& frame : animation.frames) {
                ++flipbookFrameCount;
                if (frame.sourceUsesEmbeddedPixelData) {
                    if (!frame.embeddedTexture) {
                        ++missingFlipbookFrameCount;
                        ++unresolvedEmbeddedFlipbookFrameCount;
                    }
                } else if (frame.texture.empty() ||
                           !ResolveNifInspectorTexturePathCached(state, root, frame.texture)) {
                    ++missingFlipbookFrameCount;
                    missingTextureRefs.insert(frame.texture.empty() ? "(leerer Flipbook-Frame)" : frame.texture);
                }
            }
        }
    }

    ImGui::Text("Root: %s", model.rootName.empty() ? L("(unbenannt)","(unnamed)") : model.rootName.c_str());
    ImGui::TextDisabled("%zu Mesh-Teile · %zu Dreiecke · %zu Textur-Slots",
                        model.parts.size(), triangleCount, textureCount);
    const std::size_t totalEmbeddedFailures =
        unresolvedEmbeddedTextureCount + unresolvedEmbeddedFlipbookFrameCount +
        unresolvedEmbeddedGenericShaderTextureCount;
    ImGui::TextDisabled(L("%zu extern · %zu eingebettet · %zu Embedded-Fehler · %zu Textur-Animationen",
                          "%zu external · %zu embedded · %zu embedded failures · %zu texture animations"),
                        externalTextureCount, embeddedTextureCount, totalEmbeddedFailures,
                        animatedTextureCount);
    if (uvFallbackTextureCount > 0 || missingUvTextureCount > 0) {
        ImGui::TextDisabled(
            L("%zu Texturslot(s) mit UV0-Fallback · %zu ohne brauchbare UVs",
              "%zu texture slot(s) using UV0 fallback · %zu without usable UVs"),
            uvFallbackTextureCount, missingUvTextureCount);
    }
    if (missingUvTextureCount > 0) {
        ImGui::TextColored(
            ImVec4(1.0f,0.48f,0.34f,1.0f),
            L("%zu Texturslot(s) können wegen fehlender UV-Daten nicht gerendert werden",
              "%zu texture slot(s) cannot render because usable UV data is missing"),
            missingUvTextureCount);
    }
    if (flipbookFrameCount > 0)
        ImGui::TextDisabled(L("%zu Flipbook-Frames · %zu nicht auflösbar","%zu flipbook frames · %zu unresolved"),
                            flipbookFrameCount, missingFlipbookFrameCount);
    const std::size_t totalMissingTextureRefs =
        missingTextureCount + missingFlipbookFrameCount + missingGenericShaderTextureCount;
    if (totalMissingTextureRefs > 0) {
        ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f), L("%zu Textur-Referenz(en) nicht gefunden","%zu texture reference(s) not found"),
                           totalMissingTextureRefs);
        ImGui::SameLine();
        if (UI::SmallButton(L("Fehlende kopieren##nifInspector","Copy missing##nifInspector"))) {
            std::string report;
            for (const auto& ref : missingTextureRefs) {
                if (!report.empty()) report += "\n";
                report += ref;
            }
            ImGui::SetClipboardText(report.c_str());
            state.statusMessage = std::to_string(missingTextureRefs.size()) +
                                  L(" fehlende NIF-Texturpfade kopiert."," missing NIF texture paths copied.");
        }
    } else if (externalTextureCount > 0 || flipbookFrameCount > 0) {
        ImGui::TextColored(ImVec4(0.42f,0.86f,0.62f,1.0f), "%s",L("Alle externen Textur-Referenzen gefunden","All external texture references found"));
    }
    if (totalEmbeddedFailures > 0 || model.undecodedEmbeddedTextures > 0) {
        ImGui::TextColored(
            ImVec4(1.0f,0.48f,0.34f,1.0f),
            L("%zu Material-/Flipbook-Slot(s) mit nicht dekodierter eingebetteter PixelData",
              "%zu material/flipbook slot(s) with undecoded embedded PixelData"),
            totalEmbeddedFailures);
    }
    ImGui::TextDisabled(
        L("%zu Nodes · Embedded PixelData dekodiert/nicht dekodiert: %u/%u",
          "%zu nodes · embedded PixelData decoded/undecoded: %u/%u"),
        model.nodes.size(), model.decodedEmbeddedTextures, model.undecodedEmbeddedTextures);
    if (!shaderNames.empty()) {
        std::string shaderList;
        for (const auto& shader : shaderNames) {
            if (!shaderList.empty()) shaderList += ", ";
            shaderList += shader;
        }
        ImGui::TextDisabled(L("Shadernamen: %s","Shader names: %s"), shaderList.c_str());
    }
    if (!textureApplyFallbackModes.empty()) {
        std::string applyModeList;
        for (const auto& [mode, count] : textureApplyFallbackModes) {
            if (!applyModeList.empty()) applyModeList += ", ";
            const char* modeName =
                mode == 3u ? "APPLY_HILIGHT" :
                mode == 4u ? "APPLY_HILIGHT2" : "UNKNOWN";
            applyModeList += std::string(modeName) + " (" + std::to_string(mode) + "): " +
                             std::to_string(count);
        }
        ImGui::TextColored(
            ImVec4(1.0f,0.72f,0.30f,1.0f),
            L("NiTexturingProperty ApplyMode noch nicht materialisiert: %s · Renderer nutzt Modulate-Fallback",
              "NiTexturingProperty ApplyMode not materialized yet: %s · renderer uses modulate fallback"),
            applyModeList.c_str());
    }

    if (genericShaderFallbackParts > 0 || model.textureEffectBlocks > 0) {
        ImGui::TextColored(
            ImVec4(1.0f,0.72f,0.30f,1.0f),
            "%s",L("Materialdateien können vollständig gefunden sein, obwohl unbekannte Shader-/Effect-Semantik noch generisch bleibt. Verifiziertes Env/Sphere-TextureEffect wird gerendert.",
                   "All material files can be resolved while unknown shader/effect semantics remain generic. Verified env/sphere TextureEffect is rendered."));
        ImGui::TextDisabled(
            L("%zu Mesh-Part(s) mit generischem Shader-Fallback · %zu ShaderTexDesc · TextureEffect %u",
              "%zu mesh part(s) using generic shader fallback · %zu ShaderTexDesc · TextureEffect %u"),
            genericShaderFallbackParts, genericShaderDescriptorCount, model.textureEffectBlocks);
        if (!genericShaderMaps.empty()) {
            std::string shaderMapList;
            for (const auto& [shader, mapId] : genericShaderMaps) {
                if (!shaderMapList.empty()) shaderMapList += ", ";
                shaderMapList += shader + "#" + std::to_string(mapId);
            }
            ImGui::TextWrapped(
                L("Shader-Maps ohne verifizierte Rendersemantik: %s",
                  "Shader maps without verified render semantics: %s"),
                shaderMapList.c_str());
        }
        if (unresolvedGenericShaderSourceCount > 0 ||
            missingGenericShaderTextureCount > 0 ||
            unresolvedEmbeddedGenericShaderTextureCount > 0) {
            ImGui::TextColored(
                ImVec4(1.0f,0.48f,0.34f,1.0f),
                L("ShaderTexDesc-Quellen: %zu ohne auflösbare SourceTexture · %zu externe Datei(en) fehlen · %zu Embedded PixelData nicht dekodiert",
                  "ShaderTexDesc sources: %zu without a resolvable SourceTexture · %zu external file(s) missing · %zu embedded PixelData undecoded"),
                unresolvedGenericShaderSourceCount,
                missingGenericShaderTextureCount,
                unresolvedEmbeddedGenericShaderTextureCount);
        }
        if (model.textureEffectBlocks > 0) {
            ImGui::TextDisabled(
                L("TextureEffect klassifiziert: Env/Sphere %u · andere/aus %u · Node-Bindings %u · Env/Sphere-Rendering aktiv",
                  "TextureEffect classified: env/sphere %u · other/off %u · node bindings %u · env/sphere rendering active"),
                model.textureEffectEnvironmentSphereBlocks,
                model.textureEffectUnsupportedBlocks,
                model.textureEffectNodeBindings);
        }
    }
    if (model.vertexColorPropertyBlocks > 0) {
        ImGui::TextDisabled(
            L("NiVertexColorProperty: %u Block/Blöcke · Ignore/Emission/Ambient+Diffuse werden pro Mesh gerendert",
              "NiVertexColorProperty: %u block(s) · ignore/emission/ambient+diffuse rendered per mesh"),
            model.vertexColorPropertyBlocks);
    }
    if (model.zBufferPropertyBlocks > 0) {
        ImGui::TextDisabled(
            L("NiZBufferProperty: %u Block/Blöcke · Z-Test/Z-Write/Funktion werden pro Mesh gerendert",
              "NiZBufferProperty: %u block(s) · Z test/write/function rendered per mesh"),
            model.zBufferPropertyBlocks);
    }
    if (model.inheritedPropertyBindings > 0) {
        ImGui::TextDisabled(
            L("Vererbte NiProperty-Bindings: %u · Parent-Node-Material/Texture/Renderstate aktiv",
              "Inherited NiProperty bindings: %u · parent-node material/texture/render state active"),
            model.inheritedPropertyBindings);
    }
    if (model.recovered || model.partial) {
        ImGui::TextDisabled("%s%s",
                            model.recovered ? L("Kompatibilitäts-Recovery aktiv","Compatibility recovery active") : "",
                            model.partial ? (model.recovered ? L(" · partiell dekodiert"," · partially decoded") : L("Partiell dekodiert","Partially decoded")) : "");
    }

    DrawSearchInput("nifInspectorFilter", L("Mesh oder Textur filtern...","Filter mesh or texture..."),
                    state.nifInspectorFilter, sizeof(state.nifInspectorFilter));
    DrawInlineIcon("nifInspectorFilterIcon", nullptr, IM_COL32(100,205,255,245),
                   L("Filter","Filter"), ImVec2(18,18), "panel.filter");
    ImGui::SameLine(0.0f,5.0f);
    UI::Checkbox(L("Nur fehlende Texturen##nifInspector","Missing textures only##nifInspector"), &state.nifInspectorMissingOnly);
    ImGui::SameLine();
    if (UI::SmallButton(L("Filter zurücksetzen##nifInspector","Reset filter##nifInspector"))) {
        state.nifInspectorFilter[0] = '\0';
        state.nifInspectorMissingOnly = false;
    }
    const std::string inspectorNeedle = LowerAscii(state.nifInspectorFilter);

    ImGui::BeginChild("##nifMaterialInspectorBody", ImVec2(0,0), true);
    std::size_t visiblePartCount = 0;
    for (std::size_t pi = 0; pi < model.parts.size(); ++pi) {
        const auto& part = model.parts[pi];
        const std::string partName = part.name.empty() ? ("Mesh " + std::to_string(pi + 1)) : part.name;

        bool partHasMissingTexture = false;
        std::string partSearch = LowerAscii(partName);
        if (!part.shaderName.empty()) partSearch += " " + LowerAscii(part.shaderName);
        for (const auto& slot : part.textureSlots) {
            if (!slot.present) continue;
            if (!slot.texture.empty()) partSearch += " " + LowerAscii(slot.texture);
            if (slot.sourceUsesEmbeddedPixelData) partSearch += " embedded pixeldata";
            const bool hasTextureSource =
                slot.sourceUsesEmbeddedPixelData || slot.embeddedTexture || !slot.texture.empty();
            const bool hasSlotUvs =
                slot.uvSet < part.uvSets.size() &&
                part.uvSets[slot.uvSet].size() == part.positions.size();
            const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
            if (hasTextureSource && !hasSlotUvs && !hasBaseFallbackUvs) {
                partHasMissingTexture = true;
            }
            if (slot.sourceUsesEmbeddedPixelData && !slot.embeddedTexture) {
                partHasMissingTexture = true;
            } else if (!slot.embeddedTexture && !slot.texture.empty() &&
                       !ResolveNifInspectorTexturePathCached(state, root, slot.texture)) {
                partHasMissingTexture = true;
            }
        }
        for (const auto& shaderSlot : part.shaderTextureSlots) {
            const auto& slot = shaderSlot.texture;
            partSearch += " shadertexdesc map" + std::to_string(shaderSlot.mapId);
            if (!slot.texture.empty()) partSearch += " " + LowerAscii(slot.texture);
            if (slot.sourceUsesEmbeddedPixelData) partSearch += " embedded pixeldata";

            const bool hasTextureSource =
                slot.sourceUsesEmbeddedPixelData || slot.embeddedTexture || !slot.texture.empty();
            const bool hasSlotUvs =
                slot.uvSet < part.uvSets.size() &&
                part.uvSets[slot.uvSet].size() == part.positions.size();
            const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
            if (hasTextureSource && !hasSlotUvs && !hasBaseFallbackUvs)
                partHasMissingTexture = true;

            if (slot.sourceUsesEmbeddedPixelData) {
                if (!slot.embeddedTexture) partHasMissingTexture = true;
            } else if (slot.texture.empty() ||
                       !ResolveNifInspectorTexturePathCached(state, root, slot.texture)) {
                partHasMissingTexture = true;
            }
        }
        for (const auto& animation : part.textureFlipAnimations) {
            for (const auto& frame : animation.frames) {
                if (!frame.texture.empty()) partSearch += " " + LowerAscii(frame.texture);
                if (frame.sourceUsesEmbeddedPixelData) partSearch += " embedded pixeldata";
                if (frame.sourceUsesEmbeddedPixelData && !frame.embeddedTexture) {
                    partHasMissingTexture = true;
                } else if (!frame.sourceUsesEmbeddedPixelData &&
                           (frame.texture.empty() ||
                            !ResolveNifInspectorTexturePathCached(state, root, frame.texture))) {
                    partHasMissingTexture = true;
                }
            }
        }
        if (!inspectorNeedle.empty() && partSearch.find(inspectorNeedle) == std::string::npos)
            continue;
        if (state.nifInspectorMissingOnly && !partHasMissingTexture)
            continue;

        ++visiblePartCount;
        ImGui::PushID(static_cast<int>(pi));
        const std::string header = partName + "  (" + std::to_string(part.positions.size()) +
                                   " V / " + std::to_string(part.triangleIndices.size()/3) + " T)";
        if (UI::CollapsingHeader(header.c_str())) {
            const auto& m = part.material;
            ImGui::TextDisabled("Material");
            ImGui::Text("Diffuse  %.2f  %.2f  %.2f · Alpha %.2f",
                        m.diffuse[0],m.diffuse[1],m.diffuse[2],m.alpha);
            ImGui::Text("Ambient  %.2f  %.2f  %.2f",m.ambient[0],m.ambient[1],m.ambient[2]);
            ImGui::Text("Specular %.2f  %.2f  %.2f · Gloss %.2f",
                        m.specular[0],m.specular[1],m.specular[2],m.glossiness);
            ImGui::Text("Emissive %.2f  %.2f  %.2f",m.emissive[0],m.emissive[1],m.emissive[2]);
            ImGui::TextDisabled("UV-Sets: %zu · ApplyMode: %u · %s%s",
                                part.uvSets.size(), part.textureApplyMode,
                                part.alphaBlend ? "AlphaBlend " : "",
                                part.alphaTest ? "AlphaTest" : "");
            const bool dedicatedShaderPath = part.shaderName == "VCAlphaTextureBlender";
            const bool genericNamedShader = !part.shaderName.empty() && !dedicatedShaderPath;
            ImGui::TextDisabled(
                L("Shader: %s · Renderer-Pfad: %s","Shader: %s · renderer path: %s"),
                part.shaderName.empty() ? L("(NIF Fixed Function)","(NIF fixed function)") : part.shaderName.c_str(),
                dedicatedShaderPath
                    ? "VCAlphaTextureBlender"
                    : (genericNamedShader
                        ? L("generischer NIF-Material-Fallback","generic NIF material fallback")
                        : L("klassischer NIF-Materialpfad","classic NIF material path")));
            if (genericNamedShader) {
                ImGui::TextColored(
                    ImVec4(1.0f,0.72f,0.30f,1.0f), "%s",
                    L("Shader-spezifische Semantik ist für diesen Namen noch nicht separat modelliert.",
                      "Shader-specific semantics for this name are not modeled separately yet."));
            }
            ImGui::TextDisabled(
                L("Z-State: Test %s · Write %s · Func %u",
                  "Z state: test %s · write %s · func %u"),
                part.depthTest ? L("an","on") : L("aus","off"),
                part.depthWrite ? L("an","on") : L("aus","off"),
                part.depthFunction);
            if (part.hasVertexColorProperty || !part.vertexColors.empty()) {
                const char* vertexMode =
                    !part.hasVertexColorProperty ? L("Ambient+Diffuse (NIF-Standard)","Ambient+Diffuse (NIF default)") :
                    part.vertexColorMode == 0 ? L("Ignorieren","Ignore") :
                    part.vertexColorMode == 1 ? L("Emission","Emission") :
                    part.vertexColorMode == 2
                        ? (part.vertexLightingMode == 0
                            ? L("Ambient+Diffuse / LightMode Emissive -> ignoriert",
                                "Ambient+Diffuse / emissive light mode -> ignored")
                            : L("Ambient+Diffuse","Ambient+Diffuse"))
                        : L("Unbekannter Modus -> Ambient+Diffuse-Fallback",
                            "Unknown mode -> Ambient+Diffuse fallback");
                ImGui::TextDisabled(
                    L("Vertexfarben: %zu · %s","Vertex colors: %zu · %s"),
                    part.vertexColors.size(), vertexMode);
            }

            bool anyTexture=false;
            for (std::size_t si=0; si<part.textureSlots.size(); ++si) {
                const auto& slot=part.textureSlots[si];
                if (!slot.present) continue;
                anyTexture=true;
                ImGui::PushID(static_cast<int>(si));
                ImGui::SeparatorText(NifTextureSlotLabel(si));

                EditorState::AssetThumbnail preview;
                std::optional<std::filesystem::path> resolvedTexture;
                if (slot.embeddedTexture) {
                    if (state.nifInspectorMissingOnly) {
                        ImGui::PopID();
                        continue;
                    }
                    preview = GetOrLoadEmbeddedNifInspectorThumbnail(
                        state, *slot.embeddedTexture, pi, si);
                } else if (slot.sourceUsesEmbeddedPixelData) {
                    // Fehlgeschlagene eingebettete PixelData ist selbst ein "Missing"-Fall und
                    // muss im entsprechenden Inspector-Filter sichtbar bleiben.
                } else if (!slot.texture.empty()) {
                    resolvedTexture = ResolveNifInspectorTexturePathCached(
                        state, root, slot.texture);
                    if (state.nifInspectorMissingOnly && resolvedTexture) {
                        ImGui::PopID();
                        continue;
                    }
                    if (resolvedTexture)
                        preview = GetOrLoadAssetThumbnail(state, *resolvedTexture, false);
                } else if (state.nifInspectorMissingOnly) {
                    ImGui::PopID();
                    continue;
                }

                if (preview.tex) {
                    constexpr float kPreviewMax = 64.0f;
                    ImVec2 previewSize(kPreviewMax, kPreviewMax);
                    if (preview.aspect > 1.0f) previewSize.y /= preview.aspect;
                    else if (preview.aspect > 0.0f) previewSize.x *= preview.aspect;
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(preview.tex)),
                                 previewSize);
                    ImGui::SameLine();
                }
                ImGui::BeginGroup();
                if (slot.embeddedTexture) {
                    ImGui::Text(L("Eingebettet · PixelData #%d · %u × %u",
                                  "Embedded · PixelData #%d · %u × %u"),
                                slot.sourcePixelDataRef,
                                slot.embeddedTexture->width, slot.embeddedTexture->height);
                    if (!slot.texture.empty()) {
                        ImGui::TextDisabled(
                            L("NIF-Quellname: %s · Metadatum, nicht extern geladen",
                              "NIF source name: %s · metadata, not externally loaded"),
                            slot.texture.c_str());
                    }
                } else if (slot.sourceUsesEmbeddedPixelData) {
                    ImGui::TextColored(
                        ImVec4(1.0f,0.48f,0.34f,1.0f),
                        L("Eingebettete PixelData #%d konnte nicht dekodiert werden",
                          "Embedded PixelData #%d could not be decoded"),
                        slot.sourcePixelDataRef);
                } else {
                    ImGui::TextWrapped("%s", slot.texture.empty() ? L("(keine externe Datei)","(no external file)") : slot.texture.c_str());
                    if (!slot.texture.empty()) {
                        if (resolvedTexture) {
                            ImGui::TextDisabled(L("Datei: %s","File: %s"), resolvedTexture->filename().string().c_str());
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s", resolvedTexture->string().c_str());
                        } else {
                            ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f),
                                               L("Texturdatei nicht gefunden","Texture file not found"));
                        }
                    }
                }
                const bool slotHasRequestedUvs =
                    slot.uvSet < part.uvSets.size() &&
                    part.uvSets[slot.uvSet].size() == part.positions.size();
                const bool slotHasBaseFallbackUvs = part.uvs.size() == part.positions.size();
                if (slotHasRequestedUvs) {
                    ImGui::TextDisabled("UV %u · Clamp %u · Filter %u",
                                        slot.uvSet, slot.clampMode, slot.filterMode);
                } else if (slotHasBaseFallbackUvs) {
                    ImGui::TextDisabled(
                        L("UV %u nicht verfügbar -> UV0-Fallback · Clamp %u · Filter %u",
                          "UV %u unavailable -> UV0 fallback · Clamp %u · Filter %u"),
                        slot.uvSet, slot.clampMode, slot.filterMode);
                } else {
                    ImGui::TextColored(
                        ImVec4(1.0f,0.48f,0.34f,1.0f),
                        L("UV %u nicht verfügbar · kein brauchbarer UV0-Fallback",
                          "UV %u unavailable · no usable UV0 fallback"),
                        slot.uvSet);
                }
                if (slot.hasTransform) {
                    ImGui::TextDisabled("Transform: Offset %.3f / %.3f · Scale %.3f / %.3f · Rot %.3f",
                                        slot.translation.u,slot.translation.v,
                                        slot.scale.u,slot.scale.v,slot.rotation);
                }
                ImGui::EndGroup();
                ImGui::PopID();
            }
            if (!anyTexture)
                ImGui::TextDisabled("%s",L("Keine klassischen NiTexturingProperty-Slots.",
                                           "No classic NiTexturingProperty slots."));

            if (!part.shaderTextureSlots.empty()) {
                ImGui::SeparatorText("ShaderTexDesc");
                ImGui::TextDisabled(
                    "%s",
                    dedicatedShaderPath
                        ? L("Rohdescriptoren; VCAlphaTextureBlender Map 0/1/2 ist separat verifiziert und auf die klassischen Slots abgebildet.",
                            "Raw descriptors; VCAlphaTextureBlender maps 0/1/2 are separately verified and mapped to the classic slots.")
                        : L("Shader-spezifische Rohdescriptoren; Map-ID bleibt bis zur Verifikation reine Diagnose.",
                            "Shader-specific raw descriptors; map ID remains diagnostic until verified."));

                for (std::size_t sdi=0; sdi<part.shaderTextureSlots.size(); ++sdi) {
                    const auto& shaderSlot = part.shaderTextureSlots[sdi];
                    const auto& slot = shaderSlot.texture;
                    const bool hasTextureSource =
                        slot.sourceUsesEmbeddedPixelData || slot.embeddedTexture || !slot.texture.empty();
                    const bool hasSlotUvs =
                        slot.uvSet < part.uvSets.size() &&
                        part.uvSets[slot.uvSet].size() == part.positions.size();
                    const bool hasBaseFallbackUvs = part.uvs.size() == part.positions.size();
                    const bool missingUvs = hasTextureSource && !hasSlotUvs && !hasBaseFallbackUvs;

                    std::optional<std::filesystem::path> resolvedTexture;
                    bool missingSource = false;
                    if (slot.sourceUsesEmbeddedPixelData) {
                        missingSource = !slot.embeddedTexture;
                    } else if (!slot.texture.empty()) {
                        resolvedTexture = ResolveNifInspectorTexturePathCached(
                            state, root, slot.texture);
                        missingSource = !resolvedTexture.has_value();
                    } else {
                        missingSource = true;
                    }
                    if (state.nifInspectorMissingOnly && !missingSource && !missingUvs)
                        continue;

                    ImGui::PushID(static_cast<int>(1000 + sdi));
                    std::string shaderDescLabel =
                        std::string("Map ") + std::to_string(shaderSlot.mapId) +
                        " · Source #" + std::to_string(shaderSlot.sourceTextureRef);
                    ImGui::SeparatorText(shaderDescLabel.c_str());

                    EditorState::AssetThumbnail preview;
                    if (slot.embeddedTexture && !state.nifInspectorMissingOnly) {
                        preview = GetOrLoadEmbeddedNifInspectorThumbnail(
                            state, *slot.embeddedTexture, pi, 1000 + sdi);
                    } else if (resolvedTexture && !state.nifInspectorMissingOnly) {
                        preview = GetOrLoadAssetThumbnail(state, *resolvedTexture, false);
                    }
                    if (preview.tex) {
                        constexpr float kPreviewMax = 64.0f;
                        ImVec2 previewSize(kPreviewMax, kPreviewMax);
                        if (preview.aspect > 1.0f) previewSize.y /= preview.aspect;
                        else if (preview.aspect > 0.0f) previewSize.x *= preview.aspect;
                        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(preview.tex)),
                                     previewSize);
                        ImGui::SameLine();
                    }

                    ImGui::BeginGroup();
                    if (slot.embeddedTexture) {
                        ImGui::Text(
                            L("Eingebettet · PixelData #%d · %u × %u",
                              "Embedded · PixelData #%d · %u × %u"),
                            slot.sourcePixelDataRef,
                            slot.embeddedTexture->width, slot.embeddedTexture->height);
                        if (!slot.texture.empty())
                            ImGui::TextDisabled(
                                L("NIF-Quellname: %s · Metadatum, nicht extern geladen",
                                  "NIF source name: %s · metadata, not externally loaded"),
                                slot.texture.c_str());
                    } else if (slot.sourceUsesEmbeddedPixelData) {
                        ImGui::TextColored(
                            ImVec4(1.0f,0.48f,0.34f,1.0f),
                            L("Eingebettete PixelData #%d konnte nicht dekodiert werden",
                              "Embedded PixelData #%d could not be decoded"),
                            slot.sourcePixelDataRef);
                    } else if (slot.texture.empty()) {
                        ImGui::TextColored(
                            ImVec4(1.0f,0.48f,0.34f,1.0f),
                            "%s",L("SourceTexture konnte nicht aufgelöst werden.",
                                   "SourceTexture could not be resolved."));
                    } else {
                        ImGui::TextWrapped("%s",slot.texture.c_str());
                        if (resolvedTexture) {
                            ImGui::TextDisabled(L("Datei: %s","File: %s"),
                                                resolvedTexture->filename().string().c_str());
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("%s",resolvedTexture->string().c_str());
                        } else {
                            ImGui::TextColored(
                                ImVec4(1.0f,0.48f,0.34f,1.0f),
                                "%s",L("Datei nicht gefunden","File not found"));
                        }
                    }
                    ImGui::TextDisabled(
                        "UV %u · Clamp %u · Filter %u%s",
                        slot.uvSet,slot.clampMode,slot.filterMode,
                        slot.hasTransform ? " · Transform" : "");
                    if (missingUvs) {
                        ImGui::TextColored(
                            ImVec4(1.0f,0.48f,0.34f,1.0f),
                            L("UV %u nicht verfügbar · kein brauchbarer UV0-Fallback",
                              "UV %u unavailable · no usable UV0 fallback"),
                            slot.uvSet);
                    }
                    if (slot.hasTransform) {
                        ImGui::TextDisabled(
                            "Transform: Offset %.3f / %.3f · Scale %.3f / %.3f · Rot %.3f · Method %u",
                            slot.translation.u,slot.translation.v,
                            slot.scale.u,slot.scale.v,slot.rotation,slot.transformType);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                }
            }

            if (!part.textureTransformAnimations.empty() || !part.textureFlipAnimations.empty()) {
                ImGui::SeparatorText(L("Textur-Animationen","Texture animations"));
                for (std::size_t ai = 0; ai < part.textureTransformAnimations.size(); ++ai) {
                    if (state.nifInspectorMissingOnly) continue;
                    const auto& animation = part.textureTransformAnimations[ai];
                    ImGui::PushID(static_cast<int>(ai));
                    ImGui::BulletText("%s · %s",
                                      NifTextureSlotLabel(animation.slot),
                                      NifTextureTransformOperationLabel(animation.operation));
                    ImGui::Indent();
                    const auto& track = animation.track;
                    ImGui::TextDisabled("%.3f - %.3f s · %zu Keys · %s · %s",
                                        track.startTime, track.stopTime, track.keys.size(),
                                        NifTextureInterpolationLabel(track.interpolation),
                                        NifTextureExtrapolationLabel(track.extrapolation));
                    ImGui::TextDisabled("Frequenz %.3f · Phase %.3f%s",
                                        track.frequency, track.phase,
                                        track.active ? L(" · aktiv"," · active") : "");
                    ImGui::Unindent();
                    ImGui::PopID();
                }

                for (std::size_t ai = 0; ai < part.textureFlipAnimations.size(); ++ai) {
                    const auto& animation = part.textureFlipAnimations[ai];
                    ImGui::PushID(static_cast<int>(part.textureTransformAnimations.size() + ai));
                    std::size_t embeddedFrames = 0;
                    std::size_t embeddedFailedFrames = 0;
                    std::size_t externalFrames = 0;
                    std::size_t missingFrames = 0;
                    for (const auto& frame : animation.frames) {
                        if (frame.sourceUsesEmbeddedPixelData) {
                            if (frame.embeddedTexture) ++embeddedFrames;
                            else {
                                ++embeddedFailedFrames;
                                ++missingFrames;
                            }
                        } else {
                            ++externalFrames;
                            if (frame.texture.empty() ||
                                !ResolveNifInspectorTexturePathCached(state, root, frame.texture)) {
                                ++missingFrames;
                            }
                        }
                    }
                    if (state.nifInspectorMissingOnly && missingFrames == 0) {
                        ImGui::PopID();
                        continue;
                    }

                    ImGui::BulletText(L("%s · Flipbook · %zu Frames","%s · Flipbook · %zu frames"),
                                      NifTextureSlotLabel(animation.slot), animation.frames.size());
                    ImGui::Indent();
                    const auto& track = animation.track;
                    ImGui::TextDisabled("%.3f - %.3f s · %zu Keys · %s · %s",
                                        track.startTime, track.stopTime, track.keys.size(),
                                        NifTextureInterpolationLabel(track.interpolation),
                                        NifTextureExtrapolationLabel(track.extrapolation));
                    ImGui::TextDisabled(
                        L("%zu eingebettet · %zu Embedded-Fehler · %zu extern%s",
                          "%zu embedded · %zu embedded failures · %zu external%s"),
                        embeddedFrames, embeddedFailedFrames, externalFrames,
                        missingFrames ? L(" · nicht auflösbare Frames vorhanden",
                                          " · unresolved frames present") : "");
                    if (missingFrames)
                        ImGui::TextColored(ImVec4(1.0f,0.48f,0.34f,1.0f),
                                           L("%zu Flipbook-Frame(s) nicht auflösbar","%zu flipbook frame(s) unresolved"), missingFrames);

                    const std::size_t framePreviewCount = std::min<std::size_t>(animation.frames.size(), 6);
                    for (std::size_t fi = 0; fi < framePreviewCount; ++fi) {
                        const auto& frame = animation.frames[fi];
                        std::string frameLabel = "Frame " + std::to_string(fi + 1);
                        if (frame.sourceUsesEmbeddedPixelData) {
                            frameLabel += frame.embeddedTexture
                                ? L(" · eingebettet"," · embedded")
                                : (std::string(L(" · Embedded PixelData #"," · embedded PixelData #")) +
                                   std::to_string(frame.sourcePixelDataRef) +
                                   L(" nicht dekodiert"," undecoded"));
                        } else {
                            frameLabel += " · " +
                                (frame.texture.empty() ? std::string(L("(leer)","(empty)")) : frame.texture);
                        }
                        ImGui::TextDisabled("%s", frameLabel.c_str());
                    }
                    if (animation.frames.size() > framePreviewCount)
                        ImGui::TextDisabled(L("... %zu weitere Frames","... %zu more frames"),
                                            animation.frames.size() - framePreviewCount);
                    ImGui::Unindent();
                    ImGui::PopID();
                }
            }

            if (part.skinned)
                ImGui::TextDisabled(L("Skinning: %u Bones · max. %u Einflüsse","Skinning: %u bones · max. %u influences"),
                                    part.skinBoneCount, part.maxSkinInfluences);
            if (part.billboard) ImGui::TextDisabled("Billboard-Modus: %u", part.billboardMode);
            if (part.lodControlled) ImGui::TextDisabled("LOD: %.1f - %.1f", part.lodNear, part.lodFar);
        }
        ImGui::PopID();
    }
    if (visiblePartCount == 0)
        ImGui::TextDisabled(state.nifInspectorMissingOnly
                                ? L("Keine Mesh-Teile mit fehlenden Texturen.","No mesh parts with missing textures.")
                                : L("Keine passenden Mesh-Teile.","No matching mesh parts."));
    ImGui::EndChild();
}

} // namespace nge
