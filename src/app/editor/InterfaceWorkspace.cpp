// InterfaceWorkspace.cpp
// Interface-Arbeitsbereich, Asset-Browser und Ausgabe-Log.

#include "EditorShared.hpp"

namespace nge {

std::filesystem::path InterfaceProjectOverrideRoot(const EditorState& state) {
    if (state.project.projectFolder[0] == '\0') return {};
    auto root = ProjectPathForRelative(
        state.project, core::ProjectOutputSide::Client, std::filesystem::path("resmenu"));
    return root ? *root : std::filesystem::path{};
}
std::filesystem::path InterfaceProjectOverridePath(const EditorState& state, const std::string& rel) {
    const std::filesystem::path relative(rel);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory() || HasParentTraversal(relative)) return {};
    auto target = ProjectPathForRelative(
        state.project, core::ProjectOutputSide::Client,
        std::filesystem::path("resmenu") / relative);
    return target ? *target : std::filesystem::path{};
}
bool IsRegularFileNoThrow(const std::filesystem::path& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}
bool BinaryFilesEqualNoThrow(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ecA, ecB;
    const auto sizeA = std::filesystem::file_size(a, ecA);
    const auto sizeB = std::filesystem::file_size(b, ecB);
    if (ecA || ecB || sizeA != sizeB) return false;

    std::ifstream left(a, std::ios::binary);
    std::ifstream right(b, std::ios::binary);
    if (!left || !right) return false;

    std::array<char, 64 * 1024> leftBuf{};
    std::array<char, 64 * 1024> rightBuf{};
    while (true) {
        left.read(leftBuf.data(), static_cast<std::streamsize>(leftBuf.size()));
        right.read(rightBuf.data(), static_cast<std::streamsize>(rightBuf.size()));
        const std::streamsize leftCount = left.gcount();
        const std::streamsize rightCount = right.gcount();
        if (leftCount != rightCount) return false;
        if (leftCount == 0) return true;
        if (!std::equal(leftBuf.data(), leftBuf.data() + leftCount, rightBuf.data())) return false;
    }
}
InterfaceOverrideState InspectInterfaceOverride(const EditorState& state,
                                                const std::filesystem::path& sourceRoot,
                                                const std::string& rel) {
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    if (!IsRegularFileNoThrow(projectPath)) return InterfaceOverrideState::None;

    const auto sourcePath = sourceRoot / std::filesystem::path(rel);
    if (!IsRegularFileNoThrow(sourcePath)) return InterfaceOverrideState::ProjectOnly;
    return BinaryFilesEqualNoThrow(sourcePath, projectPath)
        ? InterfaceOverrideState::Identical
        : InterfaceOverrideState::Modified;
}
InterfaceOverrideState CachedInterfaceOverrideState(const EditorState& state,
                                                    const std::string& rel) {
    const auto it = state.interfaceOverrideStateByAsset.find(LowerAscii(rel));
    return it == state.interfaceOverrideStateByAsset.end()
        ? InterfaceOverrideState::None
        : static_cast<InterfaceOverrideState>(it->second);
}
void RefreshInterfaceOverrideState(EditorState& state, const std::string& rel) {
    if (state.interfaceRoot.empty()) {
        state.interfaceOverrideStateByAsset.erase(LowerAscii(rel));
        return;
    }
    const auto status = InspectInterfaceOverride(state, std::filesystem::path(state.interfaceRoot), rel);
    if (status == InterfaceOverrideState::None)
        state.interfaceOverrideStateByAsset.erase(LowerAscii(rel));
    else
        state.interfaceOverrideStateByAsset[LowerAscii(rel)] = static_cast<int>(status);
}
void RebuildInterfaceOverrideStates(EditorState& state, const std::filesystem::path& sourceRoot) {
    state.interfaceOverrideStateByAsset.clear();
    for (const auto& rel : state.interfaceAssets) {
        const auto status = InspectInterfaceOverride(state, sourceRoot, rel);
        if (status != InterfaceOverrideState::None)
            state.interfaceOverrideStateByAsset[LowerAscii(rel)] = static_cast<int>(status);
    }
}
std::filesystem::path InterfaceEffectiveAssetPath(const EditorState& state,
                                                  const std::filesystem::path& sourceRoot,
                                                  const std::string& rel) {
    const auto projectCopy = InterfaceProjectOverridePath(state, rel);
    if (IsRegularFileNoThrow(projectCopy)) return projectCopy;
    return sourceRoot / std::filesystem::path(rel);
}
void InvalidateInterfaceAssetPreview(EditorState& state,
                                     const std::filesystem::path& sourcePath,
                                     const std::filesystem::path& projectPath) {
    if (!sourcePath.empty()) state.assetThumbnails.erase(sourcePath.string());
    if (!projectPath.empty()) state.assetThumbnails.erase(projectPath.string());
    state.nifInspectorAsset.clear();
    state.nifInspectorModel.reset();
    state.nifInspectorError.clear();
    state.nifInspectorResolvedTextureCache.clear();
}
bool CreateInterfaceProjectOverride(EditorState& state,
                                    const std::filesystem::path& sourcePath,
                                    const std::string& rel) {
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    if (projectPath.empty()) {
        state.statusMessage = L("Kein Projektordner konfiguriert.","No project folder configured.");
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(projectPath.parent_path(), ec);
    if (ec) {
        state.statusMessage = L("Projektordner konnte nicht angelegt werden: ",
                                "Could not create project folder: ") + ec.message();
        return false;
    }
    if (!std::filesystem::copy_file(sourcePath, projectPath,
                                    std::filesystem::copy_options::overwrite_existing, ec) || ec) {
        state.statusMessage = L("Interface-Asset konnte nicht ins Projekt kopiert werden: ",
                                "Could not copy interface asset into project: ") + ec.message();
        return false;
    }
    InvalidateInterfaceAssetPreview(state, sourcePath, projectPath);
    RefreshInterfaceOverrideState(state, rel);
    state.statusMessage = L("Projekt-Override erstellt: ","Project override created: ") + projectPath.string();
    return true;
}
bool RemoveInterfaceProjectOverride(EditorState& state,
                                    const std::filesystem::path& sourcePath,
                                    const std::string& rel) {
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    if (projectPath.empty() || !IsRegularFileNoThrow(projectPath)) return false;
    std::error_code ec;
    if (!std::filesystem::remove(projectPath, ec) || ec) {
        state.statusMessage = L("Projekt-Override konnte nicht entfernt werden: ",
                                "Could not remove project override: ") + ec.message();
        return false;
    }
    InvalidateInterfaceAssetPreview(state, sourcePath, projectPath);
    state.interfaceOverrideStateByAsset.erase(LowerAscii(rel));
    if (!IsRegularFileNoThrow(sourcePath)) {
        // War die Datei nur im Projekt vorhanden, darf der inzwischen gelöschte Eintrag nicht
        // als tote Zeile bis zum nächsten manuellen Rescan im Katalog stehen bleiben.
        state.interfaceAssetsScanned = false;
        state.interfaceAssets.clear();
        state.interfaceSelectedAsset = -1;
    }
    state.statusMessage = IsRegularFileNoThrow(sourcePath)
        ? L("Projekt-Override entfernt; Original wird wieder verwendet.",
            "Project override removed; source asset is used again.")
        : L("Projekt-only Asset entfernt.","Project-only asset removed.");
    return true;
}
std::pair<std::size_t, std::size_t> RemoveIdenticalInterfaceOverrides(
    EditorState& state, const std::filesystem::path& sourceRoot) {
    std::vector<std::string> identical;
    for (const auto& rel : state.interfaceAssets) {
        if (CachedInterfaceOverrideState(state, rel) == InterfaceOverrideState::Identical)
            identical.push_back(rel);
    }

    std::size_t removed = 0;
    std::size_t failed = 0;
    for (const auto& rel : identical) {
        const auto sourcePath = sourceRoot / std::filesystem::path(rel);
        if (RemoveInterfaceProjectOverride(state, sourcePath, rel)) ++removed;
        else ++failed;
    }

    if (failed == 0) {
        state.statusMessage =
            L("Unveränderte Interface-Overrides bereinigt: ",
              "Removed unchanged interface overrides: ") + std::to_string(removed);
    } else {
        state.statusMessage =
            L("Interface-Override-Bereinigung: ",
              "Interface override cleanup: ") +
            std::to_string(removed) + L(" entfernt, "," removed, ") +
            std::to_string(failed) + L(" fehlgeschlagen."," failed.");
    }
    return {removed, failed};
}
void DrawInterfaceWorkspace(EditorState& state) {
    if (state.interfaceRoot.empty() && state.project.clientFolder[0] != '\0') {
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "resmenu", 3, nullptr))
            state.interfaceRoot = found->string();
    }

    DrawPanelHeader("interfaceWorkspaceHeader", "INTERFACE / RESMENU", DrawIconMonitorEye,
                    "module.interface",
                    L("Originale nur lesen · Projekt-Overrides aktiv",
                      "sources read-only · project overrides enabled"));
    DrawImageReplaceDialog(state);

    if (state.interfaceRoot.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s",L(
            "Kein Client/resmenu unter dem Projekt-Clientpfad gefunden. Originaldateien werden "
            "weiterhin nie verändert; schreibende Arbeit erfolgt nur als Projekt-Override.",
            "No Client/resmenu was found below the configured client path. Source files are never "
            "modified; writable work is stored only as project overrides."));
        return;
    }

    const std::filesystem::path root = state.interfaceRoot;
    if (!state.interfaceAssetsScanned) {
        const std::vector<std::string> extensions =
            {".tga", ".dds", ".png", ".jpg", ".jpeg", ".bmp", ".nif"};
        state.interfaceAssets = ListFilesByExtension(root, extensions);

        // Projekt-only Dateien mit aufnehmen: so erscheinen auch extern bearbeitete oder neu
        // angelegte Overrides nach einem Rescan im selben Katalog wie die Client-Originale.
        const auto overrideRoot = InterfaceProjectOverrideRoot(state);
        if (!overrideRoot.empty()) {
            std::error_code overrideEc;
            if (std::filesystem::is_directory(overrideRoot, overrideEc) && !overrideEc) {
                const auto projectAssets = ListFilesByExtension(overrideRoot, extensions);
                std::unordered_set<std::string> known;
                known.reserve(state.interfaceAssets.size() + projectAssets.size());
                for (const auto& rel : state.interfaceAssets) known.insert(LowerAscii(rel));
                for (const auto& rel : projectAssets) {
                    if (known.insert(LowerAscii(rel)).second) state.interfaceAssets.push_back(rel);
                }
                std::stable_sort(state.interfaceAssets.begin(), state.interfaceAssets.end(),
                    [](const std::string& a, const std::string& b) {
                        return LowerAscii(a) < LowerAscii(b);
                    });
            }
        }

        RebuildInterfaceOverrideStates(state, root);
        state.interfaceAssetsScanned = true;
        if (state.interfaceSelectedAsset >= static_cast<int>(state.interfaceAssets.size()))
            state.interfaceSelectedAsset = -1;
    }
    static bool scrollToSelected = false;
    if (!state.interfacePendingAsset.empty()) {
        const std::string wanted = LowerAscii(state.interfacePendingAsset);
        for (std::size_t i = 0; i < state.interfaceAssets.size(); ++i)
            if (LowerAscii(state.interfaceAssets[i]) == wanted) state.interfaceSelectedAsset = static_cast<int>(i);
        state.interfacePendingAsset.clear();
        scrollToSelected = true;
    }

    ImGui::SameLine();
    if (UI::SmallButton(L("Neu scannen##interface","Rescan##interface"))) {
        state.interfaceAssetsScanned = false;
        state.interfaceAssets.clear();
        state.interfaceSelectedAsset = -1;
        state.interfaceAssetFilter[0] = '\0';
        state.interfaceOverrideStateByAsset.clear();
        state.nifInspectorAsset.clear();
        state.nifInspectorModel.reset();
        state.nifInspectorError.clear();
        state.nifInspectorResolvedTextureCache.clear();
        return;
    }

    ImGui::TextDisabled("%s", state.interfaceRoot.c_str());
    DrawSearchInput("interfaceFilter", L("Interface-Assets filtern...","Filter interface assets..."),
                    state.interfaceAssetFilter, sizeof(state.interfaceAssetFilter));
    DrawInlineIcon("interfaceFilterIcon", nullptr, IM_COL32(100,205,255,245),
                   L("Filter","Filter"), ImVec2(18,18), "panel.filter");
    ImGui::SameLine(0.0f,5.0f);
    UI::Checkbox(L("Nur Projekt-Overrides##interface","Project overrides only##interface"),
                 &state.interfaceOverridesOnly);
    ImGui::SameLine();
    UI::Checkbox(L("Nur Abweichungen##interface","Changed only##interface"),
                 &state.interfaceChangedOverridesOnly);

    std::size_t imageCount = 0;
    std::size_t nifCount = 0;
    std::size_t identicalOverrideCount = 0;
    std::size_t modifiedOverrideCount = 0;
    std::size_t projectOnlyCount = 0;
    for (const auto& rel : state.interfaceAssets) {
        const std::string ext = LowerAscii(std::filesystem::path(rel).extension().string());
        if (ext == ".nif") ++nifCount;
        else ++imageCount;
        switch (CachedInterfaceOverrideState(state, rel)) {
            case InterfaceOverrideState::Identical: ++identicalOverrideCount; break;
            case InterfaceOverrideState::Modified: ++modifiedOverrideCount; break;
            case InterfaceOverrideState::ProjectOnly: ++projectOnlyCount; break;
            default: break;
        }
    }
    const std::size_t overrideCount =
        identicalOverrideCount + modifiedOverrideCount + projectOnlyCount;

    const std::string needle = LowerAscii(state.interfaceAssetFilter);
    std::vector<std::size_t> matching;
    matching.reserve(state.interfaceAssets.size());
    for (std::size_t i = 0; i < state.interfaceAssets.size(); ++i) {
        const std::string& rel = state.interfaceAssets[i];
        const auto overrideState = CachedInterfaceOverrideState(state, rel);
        if (state.interfaceOverridesOnly && overrideState == InterfaceOverrideState::None) continue;
        if (state.interfaceChangedOverridesOnly &&
            overrideState != InterfaceOverrideState::Modified &&
            overrideState != InterfaceOverrideState::ProjectOnly) continue;
        if (needle.empty() || LowerAscii(rel).find(needle) != std::string::npos)
            matching.push_back(i);
    }

    ImGui::TextDisabled("%zu / %zu Assets · %zu Bilder · %zu NIF · %zu Overrides",
                        matching.size(), state.interfaceAssets.size(), imageCount, nifCount, overrideCount);
    if (overrideCount > 0) {
        ImGui::TextDisabled("%zu geändert · %zu identisch · %zu nur im Projekt",
                            modifiedOverrideCount, identicalOverrideCount, projectOnlyCount);
        if (identicalOverrideCount > 0) {
            ImGui::SameLine();
            const std::string cleanupLabel =
                L("Identische bereinigen##interface","Clean identical##interface") +
                std::string(" (") + std::to_string(identicalOverrideCount) + ")";
            if (UI::SmallButton(cleanupLabel.c_str()))
                RemoveIdenticalInterfaceOverrides(state, root);
        }
    }
    ImGui::Separator();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float leftW = std::clamp(avail.x * 0.36f, 280.0f, 430.0f);
    ImGui::BeginChild("##interfaceAssetList", ImVec2(leftW, 0), true);

    if (matching.empty()) {
        ImGui::TextDisabled("%s",L("Keine passenden resmenu-Assets.","No matching resmenu assets."));
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(matching.size()));
        int scrollRow = -1;
        if (scrollToSelected) {
            for (std::size_t m = 0; m < matching.size(); ++m)
                if (static_cast<int>(matching[m]) == state.interfaceSelectedAsset) scrollRow = static_cast<int>(m);
            if (scrollRow >= 0) clipper.IncludeItemByIndex(scrollRow);
            scrollToSelected = false;
        }
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const std::size_t assetIndex = matching[static_cast<std::size_t>(row)];
                const std::string& rel = state.interfaceAssets[assetIndex];
                const std::string ext = LowerAscii(std::filesystem::path(rel).extension().string());
                ImGui::PushID(static_cast<int>(assetIndex));

                const char* badge = ext == ".nif" ? "NIF" :
                                    ext == ".tga" ? "TGA" :
                                    ext == ".dds" ? "DDS" :
                                    ext == ".png" ? "PNG" :
                                    (ext == ".jpg" || ext == ".jpeg") ? "JPG" :
                                    ext == ".bmp" ? "BMP" : "FILE";
                ImGui::TextColored(ext == ".nif"
                                       ? ImVec4(0.35f,0.78f,1.0f,1.0f)
                                       : ImVec4(0.55f,0.86f,0.66f,1.0f),
                                   "%s", badge);
                const auto rowOverrideState = CachedInterfaceOverrideState(state, rel);
                if (rowOverrideState != InterfaceOverrideState::None) {
                    ImGui::SameLine(0, 4);
                    const char* overrideBadge =
                        rowOverrideState == InterfaceOverrideState::Identical ? "OVR=" :
                        rowOverrideState == InterfaceOverrideState::Modified ? "OVR*" : "NEW";
                    const ImVec4 overrideColor =
                        rowOverrideState == InterfaceOverrideState::Identical
                            ? ImVec4(0.50f,0.66f,0.76f,1.0f)
                            : rowOverrideState == InterfaceOverrideState::Modified
                                ? ImVec4(1.0f,0.72f,0.30f,1.0f)
                                : ImVec4(0.42f,0.86f,0.62f,1.0f);
                    ImGui::TextColored(overrideColor, "%s", overrideBadge);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s",
                            rowOverrideState == InterfaceOverrideState::Identical
                                ? L("Projektkopie ist byte-identisch zum Original",
                                    "Project copy is byte-identical to source")
                                : rowOverrideState == InterfaceOverrideState::Modified
                                    ? L("Projektkopie weicht vom Original ab",
                                        "Project copy differs from source")
                                    : L("Asset existiert nur im Projekt",
                                        "Asset exists only in the project"));
                    }
                }
                ImGui::SameLine(0, 6);

                const bool selected = state.interfaceSelectedAsset == static_cast<int>(assetIndex);
                if (row == scrollRow) ImGui::SetScrollHereY(0.3f);
                if (UI::Selectable((rel + "##interfaceAsset").c_str(), selected)) {
                    state.interfaceSelectedAsset = static_cast<int>(assetIndex);
                    if (ext == ".nif") {
                        LoadNifAssetInspector(state, InterfaceEffectiveAssetPath(state, root, rel), rel);
                    } else {
                        state.nifInspectorAsset.clear();
                        state.nifInspectorModel.reset();
                        state.nifInspectorError.clear();
                        state.nifInspectorResolvedTextureCache.clear();
                    }
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##interfacePreview", ImVec2(0,0), true);
    if (state.interfaceSelectedAsset < 0 ||
        state.interfaceSelectedAsset >= static_cast<int>(state.interfaceAssets.size())) {
        ImGui::TextDisabled("%s",L("Asset auswählen, um Vorschau und Metadaten anzuzeigen.","Select an asset to show preview and metadata."));
        ImGui::EndChild();
        return;
    }

    const std::string& rel = state.interfaceAssets[static_cast<std::size_t>(state.interfaceSelectedAsset)];
    const std::filesystem::path sourcePath = root / rel;
    const std::filesystem::path projectPath = InterfaceProjectOverridePath(state, rel);
    const bool sourceExists = IsRegularFileNoThrow(sourcePath);
    const bool hasProjectOverride = IsRegularFileNoThrow(projectPath);
    const auto overrideState = CachedInterfaceOverrideState(state, rel);
    const std::filesystem::path path = hasProjectOverride ? projectPath : sourcePath;
    const std::string ext = LowerAscii(path.extension().string());

    ImGui::TextWrapped("%s", rel.c_str());
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (!ec) ImGui::TextDisabled("%llu Bytes", static_cast<unsigned long long>(bytes));
    ImGui::SameLine();
    if (hasProjectOverride) {
        const ImVec4 statusColor =
            overrideState == InterfaceOverrideState::Identical
                ? ImVec4(0.50f,0.66f,0.76f,1.0f)
                : overrideState == InterfaceOverrideState::Modified
                    ? ImVec4(1.0f,0.72f,0.30f,1.0f)
                    : ImVec4(0.42f,0.86f,0.62f,1.0f);
        const char* statusText =
            overrideState == InterfaceOverrideState::Identical
                ? L("· Projekt-Override · byte-identisch","· project override · byte-identical")
                : overrideState == InterfaceOverrideState::Modified
                    ? L("· Projekt-Override · geändert","· project override · modified")
                    : L("· nur im Projekt","· project only");
        ImGui::TextColored(statusColor, "%s", statusText);
    } else {
        ImGui::TextDisabled("%s", L("· Original","· source"));
    }

    const bool canCreateOverride = sourceExists &&
                                   state.project.projectFolder[0] != '\0' &&
                                   !hasProjectOverride;
    ImGui::BeginDisabled(!canCreateOverride);
    if (UI::SmallButton(L("In Projekt übernehmen##interfaceOverride",
                          "Create project override##interfaceOverride"))) {
        CreateInterfaceProjectOverride(state, sourcePath, rel);
    }
    ImGui::EndDisabled();
    const bool isImage = ext == ".tga" || ext == ".dds" || ext == ".png" ||
                         ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
    ImGui::SameLine();
    ImGui::BeginDisabled(state.project.projectFolder[0] == '\0');
    if (isImage) {
        // Beliebiges Bild wählen; es wird auf Größe und Format des Originals umgerechnet.
        if (UI::SmallButton(L("Ersetzen...##interfaceOverride", "Replace...##interfaceOverride")))
            OpenImageReplaceDialog(state, rel, rel);
    }
#ifdef _WIN32
    else if (UI::SmallButton(L("Asset ersetzen...##interfaceOverride",
                               "Replace asset...##interfaceOverride"))) {
        if (auto picked = BrowseForInterfaceAssetWindows(
                L("Interface-Asset als Projekt-Override wählen",
                  "Choose interface asset for project override"), ext)) {
            const std::filesystem::path replacement = *picked;
            if (LowerAscii(replacement.extension().string()) != ext) {
                state.statusMessage = L(
                    "Ersatz abgelehnt: Dateiendung muss dem ausgewählten Asset entsprechen.",
                    "Replacement rejected: file extension must match the selected asset.");
            } else {
                CreateInterfaceProjectOverride(state, replacement, rel);
            }
        }
    }
#endif
    if (isImage) {
        ImGui::SameLine();
        if (UI::SmallButton(L("Als PNG exportieren##interface", "Export as PNG##interface")))
            ExportResmenuAsPng(state, rel);
    }
    ImGui::EndDisabled();
    if (hasProjectOverride) {
        ImGui::SameLine();
        if (UI::SmallButton(L("Projektkopie entfernen##interfaceOverride",
                              "Remove project copy##interfaceOverride"))) {
            RemoveResmenuOverride(state, rel);
        }
    }
    if (state.project.projectFolder[0] == '\0') {
        ImGui::TextDisabled("%s",L("Projektordner konfigurieren, um bearbeitbare Overrides anzulegen.",
                                   "Configure a project folder to create editable overrides."));
    } else if (hasProjectOverride) {
        ImGui::TextDisabled("%s", projectPath.string().c_str());
    } else {
        ImGui::TextDisabled("%s",L(
            "Beim Übernehmen wird das Asset unverändert nach <Projekt>/Client/resmenu/... kopiert.",
            "Creating an override copies the asset unchanged to <Project>/Client/resmenu/... ."));
    }
    ImGui::Separator();

    if (ext == ".nif") {
        if (state.nifInspectorAsset != rel)
            LoadNifAssetInspector(state, path, rel);
        // Der Inspector behandelt <Projekt>/Client/resmenu als Overlay: sowohl das NIF selbst
        // als auch externe Textur-/Flipbook-Referenzen bevorzugen vorhandene Projektkopien.
        DrawNifAssetInspector(state, root);
    } else if (isImage) {
        float aspect = 1.0f;
        if (const auto tex = ResmenuPreviewTexture(state, rel, &aspect)) {
            if (auto fmt = core::InspectImageFile(path)) ImGui::TextDisabled("%s", core::DescribeImageFormat(*fmt).c_str());
            const ImVec2 area(std::max(1.0f, ImGui::GetContentRegionAvail().x - 12.0f),
                              std::max(1.0f, ImGui::GetContentRegionAvail().y - 12.0f));
            ImVec2 size = area;
            if (size.x / size.y > aspect) size.x = size.y * aspect;
            else size.y = size.x / aspect;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            DrawCheckerboard(origin, ImVec2(origin.x + size.x, origin.y + size.y));
            ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), size);
        } else {
            ImGui::TextDisabled("%s",L("Bild konnte nicht dekodiert werden.","Image could not be decoded."));
        }
    } else {
        ImGui::TextDisabled("%s",L("Für diesen Dateityp ist noch keine Vorschau verfügbar.","No preview is available for this file type yet."));
    }
    ImGui::EndChild();
}
void DrawWorkspaceAssetBrowser(EditorState& state) {
    const bool objectMode = state.editMode == EditMode::ObjectPlacement;
    const bool textureMode = state.editMode == EditMode::TexturePaint;

    if (objectMode && !state.nifListScanned) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            state.availableNifFiles = ListFilesByExtension(*resmapRoot, {".nif"});
            state.nifAssetRoot = *resmapRoot;
            state.resmapRootForThumbnails = *resmapRoot;
            state.nifListScanned = true;
        }
    }
    if (textureMode && !state.textureListScanned) {
        if (const auto resmapRoot = FindResmapRootForAssets(state.project.clientFolder)) {
            if (const auto texRoot = FindNamedSubfolder(*resmapRoot, {"fieldtexture"})) {
                state.availableTextureFiles = ListFilesByExtension(*texRoot, {".dds"});
                state.textureAssetRoot = *texRoot;
                state.textureListScanned = true;
            }
        }
    }

    const char* assetContext = objectMode ? L("NIF Modelle","NIF models")
                                           : textureMode ? L("Texturen","Textures")
                                                         : L("kontextsensitiv","context-sensitive");
    DrawPanelHeader("assetBrowserHeader", "ASSET BROWSER", DrawIconCube,
                    "panel.asset_browser", assetContext);

    if (!objectMode && !textureMode) {
        ImGui::TextWrapped("%s",L("Der Asset Browser wird bei 'Objekte' und 'Textur' aktiv. Weitere Asset-Typen können später hier ergänzt werden.",
                                "The Asset Browser becomes active for 'Objects' and 'Texture'. Additional asset types can be added here later."));
        return;
    }

    DrawSearchInput("workspaceAssetFilter", L("Assets filtern...","Filter assets..."),
                    state.workspaceAssetFilter, sizeof(state.workspaceAssetFilter));

    const auto& files = objectMode ? state.availableNifFiles : state.availableTextureFiles;
    const auto& root = objectMode ? state.nifAssetRoot : state.textureAssetRoot;
    if (objectMode && !state.nifInspectorAsset.empty() &&
        std::find(files.begin(), files.end(), state.nifInspectorAsset) == files.end()) {
        // Ein zuvor im Interface-Browser inspiziertes resmenu-NIF darf im Karten-Asset-Browser
        // nicht versehentlich relativ zu resmap weiterverwendet werden.
        state.nifInspectorAsset.clear();
        state.nifInspectorModel.reset();
        state.nifInspectorError.clear();
        state.nifInspectorResolvedTextureCache.clear();
    }
    std::string needle = LowerAscii(state.workspaceAssetFilter);
    std::vector<std::size_t> matching;
    matching.reserve(files.size());
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (needle.empty() || LowerAscii(files[i]).find(needle) != std::string::npos) matching.push_back(i);
    }
    ImGui::TextDisabled("%zu / %zu Assets", matching.size(), files.size());

    const bool showNifInspector = objectMode && !state.nifInspectorAsset.empty();
    const float assetListHeight = showNifInspector
        ? std::max(190.0f, ImGui::GetContentRegionAvail().y * 0.52f)
        : 0.0f;
    ImGui::BeginChild("##workspaceAssetList", ImVec2(0,assetListHeight), true);

    constexpr float cardW = 108.0f;
    constexpr float cardH = 104.0f;
    constexpr float cardGap = 8.0f;
    constexpr float thumbSize = 68.0f;
    const float availableW = std::max(cardW, ImGui::GetContentRegionAvail().x);
    const int columns = std::max(1, static_cast<int>((availableW + cardGap) / (cardW + cardGap)));
    const int rowCount = static_cast<int>((matching.size() + static_cast<std::size_t>(columns) - 1) /
                                          static_cast<std::size_t>(columns));
    ImGuiListClipper clipper;
    clipper.Begin(rowCount, cardH + cardGap);
    while (clipper.Step()) {
        for (int gridRow = clipper.DisplayStart; gridRow < clipper.DisplayEnd; ++gridRow) {
            for (int col = 0; col < columns; ++col) {
                const std::size_t matchPos = static_cast<std::size_t>(gridRow * columns + col);
                if (matchPos >= matching.size()) break;
                const std::size_t index = matching[matchPos];
                const std::string& rel = files[index];
                const std::string filename = std::filesystem::path(rel).filename().string();
                std::string shortName = filename;
                if (shortName.size() > 17) shortName = shortName.substr(0,14) + "...";

                ImGui::PushID(static_cast<int>(index));
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const bool inspected = objectMode ? state.nifInspectorAsset == rel
                                                  : std::string(state.newLayerDiffuse) == rel;
                const bool clicked = ImGui::InvisibleButton("##assetCard", ImVec2(cardW,cardH));
                const bool hovered = ImGui::IsItemHovered();
                ImDrawList* dl=ImGui::GetWindowDrawList();
                dl->AddRectFilled(ImVec2(p.x,p.y+2.0f),ImVec2(p.x+cardW,p.y+cardH+2.0f),
                                  IM_COL32(0,3,10,100),7.0f);
                dl->AddRectFilled(p,ImVec2(p.x+cardW,p.y+cardH),
                                  inspected?IM_COL32(9,55,101,245):hovered?IM_COL32(14,47,75,245):IM_COL32(9,25,40,245),7.0f);
                dl->AddRect(p,ImVec2(p.x+cardW,p.y+cardH),
                            inspected?IM_COL32(32,221,242,245):hovered?IM_COL32(19,140,255,190):IM_COL32(31,62,88,235),
                            7.0f,0,inspected?1.6f:1.0f);

                const auto thumb = GetOrLoadAssetThumbnail(
                    state, root / rel, objectMode, objectMode ? state.resmapRootForThumbnails : std::filesystem::path{});
                const ImVec2 imageMin(p.x+(cardW-thumbSize)*0.5f,p.y+8.0f);
                const ImVec2 imageMax(imageMin.x+thumbSize,imageMin.y+thumbSize);
                dl->AddRectFilled(imageMin,imageMax,IM_COL32(5,15,24,255),5.0f);
                if (thumb.tex) {
                    dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)),imageMin,imageMax);
                } else {
                    const ImVec2 cc((imageMin.x+imageMax.x)*0.5f,(imageMin.y+imageMax.y)*0.5f);
                    DrawIconCube(dl,cc,14.0f,IM_COL32(78,116,145,210));
                }

                const ImVec2 ts=ImGui::CalcTextSize(shortName.c_str());
                dl->AddText(ImVec2(p.x+(cardW-ts.x)*0.5f,p.y+82.0f),
                            inspected?IM_COL32(238,249,255,255):IM_COL32(181,205,223,245),shortName.c_str());
                if (inspected)
                    dl->AddRectFilled(ImVec2(p.x+14.0f,p.y+cardH-3.0f),ImVec2(p.x+cardW-14.0f,p.y+cardH-1.0f),
                                      IM_COL32(32,221,242,255),1.0f);

                if (clicked) {
                    if (objectMode) {
                        const std::string legacy = ToLegacyResmapModelPath(rel);
                        std::snprintf(state.newObjectModelPath, sizeof(state.newObjectModelPath), "%s", legacy.c_str());
                        LoadNifAssetInspector(state, root / rel, rel);
                        state.statusMessage = L("Objekt-Asset gewählt: ","Object asset selected: ") + legacy;
                    } else {
                        std::snprintf(state.newLayerDiffuse, sizeof(state.newLayerDiffuse), "%s", rel.c_str());
                        state.statusMessage = L("Textur-Asset gewählt: ","Texture asset selected: ") + rel;
                    }
                }
                if (hovered) ImGui::SetTooltip("%s",rel.c_str());
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    if (objectMode) {
                        const std::string legacy=ToLegacyResmapModelPath(rel);
                        ImGui::SetDragDropPayload("NEXTGEN_NIF_ASSET",legacy.c_str(),legacy.size()+1);
                        ImGui::Text("%s",L("NIF platzieren","Place NIF"));
                        ImGui::TextDisabled("%s",legacy.c_str());
                    } else {
                        ImGui::SetDragDropPayload("NEXTGEN_DDS_ASSET",rel.c_str(),rel.size()+1);
                        ImGui::Text("%s",L("Textur zuweisen","Assign texture"));
                        ImGui::TextDisabled("%s",rel.c_str());
                    }
                    ImGui::EndDragDropSource();
                }
                ImGui::PopID();
                if (col + 1 < columns && matchPos + 1 < matching.size()) ImGui::SameLine(0.0f,cardGap);
            }
        }
    }
    ImGui::EndChild();
    if (objectMode && !state.nifInspectorAsset.empty())
        DrawNifAssetInspector(state, root);
}
// Der eigentliche Arbeitsbereich (siehe Mockup, zweites/rechtes Bild): Tab-Leiste oben,
// darunter drei Spalten - "Datei"+"Tools/etc" links, "2D View" Mitte, "3D View" rechts.
// Ausgabeprotokoll (Output Log) wie in Unreal: alle Statusmeldungen mit Zeit, Kategorie und
// Schweregrad, filterbar, kopierbar. Rein editorseitig, nichts wird in Fiesta-Dateien geschrieben.
void DrawOutputLogPanel(EditorState& state) {
    using core::level::LogSeverity;
    DrawPanelHeader("outputLogHeader", L("AUSGABE", "OUTPUT LOG"), DrawIconBook, "panel.properties",
                    L("Statusmeldungen", "Status messages"));
    const auto& log = state.editorLog;
    char infoLabel[48], warnLabel[48], errLabel[48];
    std::snprintf(infoLabel, sizeof(infoLabel), "Info %zu##logInfo", log.Count(LogSeverity::Info));
    std::snprintf(warnLabel, sizeof(warnLabel), "%s %zu##logWarn", L("Warnungen", "Warnings"), log.Count(LogSeverity::Warning));
    std::snprintf(errLabel, sizeof(errLabel), "%s %zu##logErr", L("Fehler", "Errors"), log.Count(LogSeverity::Error));
    ImGui::Checkbox(infoLabel, &state.outputLogShowInfo);
    ImGui::SameLine();
    ImGui::Checkbox(warnLabel, &state.outputLogShowWarnings);
    ImGui::SameLine();
    ImGui::Checkbox(errLabel, &state.outputLogShowErrors);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 220.0f));
    ImGui::InputTextWithHint("##logFilter", L("Filter...", "Filter..."), state.outputLogFilter, sizeof(state.outputLogFilter));
    ImGui::SameLine();
    if (ImGui::SmallButton(L("Kopieren", "Copy"))) ImGui::SetClipboardText(log.ExportText().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(L("Leeren", "Clear"))) state.editorLog.Clear();
    ImGui::SameLine();
    ImGui::Checkbox(L("Folgen", "Follow"), &state.outputLogAutoScroll);

    std::string filter = state.outputLogFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    ImGui::BeginChild("##outputLogLines", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& e : log.Entries()) {
        if ((e.severity == LogSeverity::Info && !state.outputLogShowInfo) ||
            (e.severity == LogSeverity::Warning && !state.outputLogShowWarnings) ||
            (e.severity == LogSeverity::Error && !state.outputLogShowErrors)) continue;
        if (!filter.empty()) {
            std::string hay = e.category + " " + e.text;
            std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(filter) == std::string::npos) continue;
        }
        const ImVec4 col = e.severity == LogSeverity::Error ? ImVec4(1.0f, 0.45f, 0.40f, 1.0f)
                         : e.severity == LogSeverity::Warning ? ImVec4(1.0f, 0.80f, 0.35f, 1.0f)
                         : ImVec4(0.80f, 0.86f, 0.92f, 1.0f);
        ImGui::TextDisabled("%8.1fs", e.timeSeconds);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "[%s]", e.category.c_str());
        ImGui::SameLine();
        if (e.repeat > 1) ImGui::TextColored(col, "%s  (x%d)", e.text.c_str(), e.repeat);
        else ImGui::TextColored(col, "%s", e.text.c_str());
    }
    if (state.outputLogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

} // namespace nge
