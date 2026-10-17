// ImageReplace.cpp
// Bild ersetzen mit Konvertierung: beliebige Quelle (PNG/JPG/BMP/TGA/DDS) wird auf Größe und Format
// des Originals gebracht (core::BuildReplacement) und als Projektkopie unter
// <Projekt>/Client/resmenu/<rel> abgelegt. Vorher/Nachher auf Schachbrett, Strg+Z stellt den
// vorherigen Stand der Projektkopie wieder her. Genutzt vom Interface-Bereich und den Bild-Editoren
// (Ladebilder, Icons, Porträts).

#include "EditorShared.hpp"

namespace nge {

namespace {
void DeleteTexture(std::uint32_t& tex) {
    if (tex == 0) return;
    GLuint t = tex;
    glDeleteTextures(1, &t);
    tex = 0;
}

std::optional<std::vector<std::uint8_t>> ReadResmenuBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool WriteFileBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = L("Datei kann nicht geschrieben werden", "cannot write file");
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        error = L("Schreibfehler", "write error");
        return false;
    }
    return true;
}

// Vorschau-Texturen je Datei; neu geladen, wenn sich Größe oder Änderungszeit der Datei ändern.
struct PreviewEntry {
    std::uint32_t tex = 0;
    float aspect = 1.0f;
    std::filesystem::file_time_type mtime{};
    std::uintmax_t size = 0;
    bool failed = false;
};
std::unordered_map<std::string, PreviewEntry>& PreviewCache() {
    static std::unordered_map<std::string, PreviewEntry> cache;
    return cache;
}
void ForgetPreview(const std::filesystem::path& path) {
    auto& cache = PreviewCache();
    if (auto it = cache.find(path.string()); it != cache.end()) {
        DeleteTexture(it->second.tex);
        cache.erase(it);
    }
}

// Nach jeder Änderung einer Projektkopie: Vorschauen und Override-Status auffrischen; eine neue
// Datei taucht nach dem nächsten Scan in der Interface-Liste auf.
void ResmenuFileChanged(EditorState& state, const std::string& rel) {
    const auto sourcePath = ResmenuSourceRoot(state) / std::filesystem::path(rel);
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    InvalidateInterfaceAssetPreview(state, sourcePath, projectPath);
    ForgetPreview(sourcePath);
    ForgetPreview(projectPath);
    RefreshInterfaceOverrideState(state, rel);
    const std::string lower = LowerAscii(rel);
    const bool listed = std::any_of(state.interfaceAssets.begin(), state.interfaceAssets.end(),
                                    [&](const std::string& a) { return LowerAscii(a) == lower; });
    if (!listed || (!IsRegularFileNoThrow(projectPath) && !IsRegularFileNoThrow(sourcePath))) {
        state.interfaceAssetsScanned = false;
        state.interfaceAssets.clear();
    }
    ++state.resmenuRevision;
}

// Setzt die Projektkopie auf "content" (nullopt = Datei entfernen).
bool ApplyResmenuContent(EditorState& state, const std::string& rel,
                         const std::shared_ptr<const std::vector<std::uint8_t>>& content) {
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    if (projectPath.empty()) {
        state.statusMessage = L("Kein Projektordner konfiguriert.", "No project folder configured.");
        return false;
    }
    bool ok = true;
    if (content) {
        std::string error;
        ok = WriteFileBytes(projectPath, *content, error);
        if (!ok) state.statusMessage = L("Projektkopie konnte nicht geschrieben werden: ", "Could not write project copy: ") + error;
    } else {
        std::error_code ec;
        std::filesystem::remove(projectPath, ec);
        ok = !ec;
        if (!ok) state.statusMessage = L("Projektkopie konnte nicht entfernt werden: ", "Could not remove project copy: ") + ec.message();
    }
    ResmenuFileChanged(state, rel);
    return ok;
}

void RecordResmenuChange(EditorState& state, const std::string& rel,
                         std::shared_ptr<const std::vector<std::uint8_t>> before,
                         std::shared_ptr<const std::vector<std::uint8_t>> after, const std::string& label) {
    EditorState* s = &state;
    state.dataHistory.Record({[s, rel, after]() { ApplyResmenuContent(*s, rel, after); },
                              [s, rel, before]() { ApplyResmenuContent(*s, rel, before); }},
                             label);
}

std::shared_ptr<const std::vector<std::uint8_t>> CurrentProjectBytes(EditorState& state, const std::string& rel) {
    const auto projectPath = InterfaceProjectOverridePath(state, rel);
    if (!IsRegularFileNoThrow(projectPath)) return nullptr;
    auto bytes = ReadResmenuBytes(projectPath);
    return bytes ? std::make_shared<const std::vector<std::uint8_t>>(std::move(*bytes)) : nullptr;
}

// Bild in ein Rechteck einpassen (Seitenverhältnis bleibt), mittig.
void FitRect(ImVec2 areaMin, ImVec2 areaSize, float aspect, ImVec2& outMin, ImVec2& outMax) {
    ImVec2 size = areaSize;
    if (aspect <= 0.0f) aspect = 1.0f;
    if (size.x / std::max(size.y, 1.0f) > aspect) size.x = size.y * aspect;
    else size.y = size.x / aspect;
    outMin = ImVec2(areaMin.x + (areaSize.x - size.x) * 0.5f, areaMin.y + (areaSize.y - size.y) * 0.5f);
    outMax = ImVec2(outMin.x + size.x, outMin.y + size.y);
}

void DrawComparePane(const char* id, const char* caption, std::uint32_t tex, float aspect, ImVec2 size,
                     const std::string& info) {
    ImGui::BeginChild(id, size, true);
    ImGui::TextUnformatted(caption);
    ImGui::TextDisabled("%s", info.c_str());
    const ImVec2 area = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (tex != 0 && area.x > 8 && area.y > 8) {
        ImVec2 mn, mx;
        FitRect(origin, area, aspect, mn, mx);
        DrawCheckerboard(mn, mx);
        ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), mn, mx);
        ImGui::Dummy(area);
    }
    ImGui::EndChild();
}
} // namespace

std::filesystem::path ResmenuSourceRoot(EditorState& state) {
    if (state.interfaceRoot.empty() && state.project.clientFolder[0] != '\0') {
        if (auto found = FindDirBreadthFirst(state.project.clientFolder, "resmenu", 3, nullptr))
            state.interfaceRoot = found->string();
    }
    return state.interfaceRoot;
}

std::filesystem::path ResmenuEffectivePath(EditorState& state, const std::string& rel) {
    return InterfaceEffectiveAssetPath(state, ResmenuSourceRoot(state), rel);
}

bool WriteResmenuOverride(EditorState& state, const std::string& rel, const std::vector<std::uint8_t>& bytes,
                          const std::string& label) {
    if (InterfaceProjectOverridePath(state, rel).empty()) {
        state.statusMessage = L("Kein Projektordner konfiguriert.", "No project folder configured.");
        return false;
    }
    auto before = CurrentProjectBytes(state, rel);
    auto after = std::make_shared<const std::vector<std::uint8_t>>(bytes);
    if (!ApplyResmenuContent(state, rel, after)) return false;
    RecordResmenuChange(state, rel, std::move(before), std::move(after), label);
    state.statusMessage = label + " -> " + InterfaceProjectOverridePath(state, rel).string();
    return true;
}

bool RemoveResmenuOverride(EditorState& state, const std::string& rel) {
    auto before = CurrentProjectBytes(state, rel);
    if (!before) return false;
    if (!ApplyResmenuContent(state, rel, nullptr)) return false;
    RecordResmenuChange(state, rel, std::move(before), nullptr,
                        L("Projektkopie entfernt: ", "Project copy removed: ") + rel);
    state.statusMessage = IsRegularFileNoThrow(ResmenuSourceRoot(state) / std::filesystem::path(rel))
        ? L("Projektkopie entfernt; das Original gilt wieder (Strg+Z stellt sie wieder her).",
            "Project copy removed; the original applies again (Ctrl+Z restores it).")
        : L("Nur-Projekt-Datei entfernt (Strg+Z stellt sie wieder her).",
            "Project-only file removed (Ctrl+Z restores it).");
    return true;
}

std::uint32_t ResmenuPreviewTexture(EditorState& state, const std::string& rel, float* aspect) {
    const auto path = ResmenuEffectivePath(state, rel);
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return 0;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    auto& entry = PreviewCache()[path.string()];
    if (entry.size != size || entry.mtime != mtime || (entry.tex == 0 && !entry.failed)) {
        DeleteTexture(entry.tex);
        entry.size = size;
        entry.mtime = mtime;
        entry.failed = true;
        if (auto image = core::LoadImageFile(path)) {
            entry.tex = UploadThumbnailTexture(image->rgba, image->width, image->height);
            entry.aspect = static_cast<float>(image->width) / static_cast<float>(std::max<std::uint32_t>(image->height, 1));
            entry.failed = entry.tex == 0;
        }
    }
    if (aspect) *aspect = entry.aspect;
    return entry.tex;
}

void DrawCheckerboard(ImVec2 min, ImVec2 max) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    constexpr float cell = 8.0f;
    dl->AddRectFilled(min, max, IM_COL32(150, 150, 150, 255));
    dl->PushClipRect(min, max, true);
    for (float y = min.y; y < max.y; y += cell) {
        const int row = static_cast<int>((y - min.y) / cell);
        for (float x = min.x + ((row & 1) ? cell : 0.0f); x < max.x; x += cell * 2.0f)
            dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + cell, max.x), std::min(y + cell, max.y)),
                              IM_COL32(205, 205, 205, 255));
    }
    dl->PopClipRect();
}

std::filesystem::path ExportResmenuAsPng(EditorState& state, const std::string& rel) {
    if (state.project.projectFolder[0] == '\0') {
        state.statusMessage = L("Kein Projektordner konfiguriert.", "No project folder configured.");
        return {};
    }
    const auto source = ResmenuEffectivePath(state, rel);
    auto image = core::LoadImageFile(source);
    if (!image) {
        state.statusMessage = L("Bild nicht lesbar: ", "Image not readable: ") + image.error();
        return {};
    }
    core::ImageFormat png;
    png.container = core::ImageContainer::Png;
    png.width = image->width;
    png.height = image->height;
    auto bytes = core::EncodeImage(*image, png);
    if (!bytes) {
        state.statusMessage = bytes.error();
        return {};
    }
    std::filesystem::path target = std::filesystem::path(state.project.projectFolder) / "Export" / "resmenu" /
                                   std::filesystem::path(rel);
    target.replace_extension(".png");
    std::string error;
    if (!WriteFileBytes(target, *bytes, error)) {
        state.statusMessage = L("Export fehlgeschlagen: ", "Export failed: ") + error;
        return {};
    }
    state.statusMessage = L("Als PNG exportiert: ", "Exported as PNG: ") + target.string();
    return target;
}

void OpenImageReplaceDialog(EditorState& state, const std::string& targetRel, const std::string& title,
                            const std::string& templateRel) {
    auto& r = state.imageReplace;
    r.open = true;
    r.targetRel = targetRel;
    r.templateRel = templateRel;
    r.title = title.empty() ? targetRel : title;
    r.error.clear();
    r.resultKey.clear();
    r.result.reset();
    r.originalTexFor.clear();
    r.originalFormat.reset();
    r.originalError.clear();
    r.applied = false;
    r.extraTargets.clear();
    r.onApply = nullptr;
    r.cellMode = false;
}

bool ApplyImageReplacement(EditorState& state) {
    auto& r = state.imageReplace;
    if (!r.result) return false;
    const std::string label = L("Bild ersetzt: ", "Image replaced: ") + r.targetRel;
    DataHistoryBegin(state, label);
    bool ok = WriteResmenuOverride(state, r.targetRel, r.result->bytes, label);
    core::ReplaceOptions options;
    options.fit = static_cast<core::FitMode>(std::clamp(r.fit, 0, 2));
    options.keepOriginalSize = r.keepSize;
    for (const auto& extra : r.extraTargets) {
        if (!ok) break;
        auto format = core::InspectImageFile(ResmenuEffectivePath(state, extra));
        if (!format) continue;
        auto built = core::BuildReplacement(r.sourceImage, *format, options);
        ok = built && WriteResmenuOverride(state, extra, built->bytes, label);
    }
    if (ok && r.onApply) r.onApply(state);
    DataHistoryCommit(state);
    if (ok) {
        r.applied = true;
        r.open = false;
        state.statusMessage = label + (r.extraTargets.empty() ? std::string() : " (+" + std::to_string(r.extraTargets.size()) + ")");
    }
    return ok;
}

void DrawImageReplaceDialog(EditorState& state) {
    auto& r = state.imageReplace;
    r.applied = false;
    const char* popupId = "###imageReplace";
    if (r.open && !ImGui::IsPopupOpen(popupId)) ImGui::OpenPopup(popupId);
    const auto release = [&r]() {
        DeleteTexture(r.previewTex);
        DeleteTexture(r.originalTex);
        r.originalTexFor.clear();
        r.sourceImage = {};
        r.loadedSource.clear();
        r.result.reset();
        r.resultKey.clear();
    };
    ImGui::SetNextWindowSize(ImVec2(940, 640), ImGuiCond_Appearing);
    const std::string windowTitle = L("Bild ersetzen", "Replace image") + std::string(" - ") + r.title + popupId;
    if (!ImGui::BeginPopupModal(windowTitle.c_str(), &r.open)) {
        if (!r.open && (r.previewTex || r.originalTex || r.sourceImage.Valid())) release();
        return;
    }

    // Original (bzw. Vorlage für ein neues Ziel): Format und Vorschau.
    const auto targetPath = ResmenuEffectivePath(state, r.targetRel);
    const bool targetExists = IsRegularFileNoThrow(targetPath);
    const auto formatPath = targetExists || r.templateRel.empty() ? targetPath : ResmenuEffectivePath(state, r.templateRel);
    if (r.originalTexFor != formatPath.string()) {
        DeleteTexture(r.originalTex);
        r.originalTexFor = formatPath.string();
        r.originalFormat.reset();
        r.originalError.clear();
        r.resultKey.clear();
        if (auto fmt = core::InspectImageFile(formatPath)) r.originalFormat = *fmt;
        else r.originalError = fmt.error();
        if (auto img = core::LoadImageFile(formatPath)) {
            if (r.cellMode) *img = core::CropImage(*img, r.cellX, r.cellY, r.cellW, r.cellH);
            r.originalTex = UploadThumbnailTexture(img->rgba, img->width, img->height);
            r.originalW = static_cast<float>(img->width);
            r.originalH = static_cast<float>(img->height);
        }
    }
    const bool projectCopy = IsRegularFileNoThrow(InterfaceProjectOverridePath(state, r.targetRel));
    ImGui::Text("%s %s", L("Ziel:", "Target:"), r.targetRel.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", !targetExists ? L("(neu)", "(new)")
                             : projectCopy  ? L("(Projektkopie)", "(project copy)")
                                            : L("(Original aus dem Client)", "(client original)"));
    if (r.originalFormat) {
        ImGui::TextDisabled("%s %s%s", targetExists ? L("Format des Originals:", "Original format:")
                                                    : L("Format der Vorlage:", "Template format:"),
                            core::DescribeImageFormat(*r.originalFormat).c_str(),
                            targetExists ? "" : (" (" + r.templateRel + ")").c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s %s", L("Format unbekannt:", "Unknown format:"),
                           r.originalError.c_str());
    }
    ImGui::Separator();

    // Quelle: Pfadfeld (überall) und Dateidialog (Windows); geladen wird, sobald die Datei existiert.
    ImGui::TextUnformatted(L("Quelle (PNG, JPG, BMP, TGA oder DDS):", "Source (PNG, JPG, BMP, TGA or DDS):"));
    ImGui::SetNextItemWidth(-130.0f);
    UI::InputText("##imageReplaceSource", r.sourcePath, sizeof(r.sourcePath));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button(L("Durchsuchen...##imageReplace", "Browse...##imageReplace"))) {
        if (auto picked = BrowseForInterfaceAssetWindows(L("Quellbild wählen", "Choose source image"), ""))
            std::snprintf(r.sourcePath, sizeof(r.sourcePath), "%s", picked->c_str());
    }
#endif
    const std::string sourcePath = TrimAscii(r.sourcePath);
    if (sourcePath != r.loadedSource) {
        r.sourceImage = {};
        r.error.clear();
        if (!sourcePath.empty() && IsRegularFileNoThrow(sourcePath)) {
            r.loadedSource = sourcePath;
            if (auto img = core::LoadImageFile(sourcePath)) r.sourceImage = std::move(*img);
            else r.error = img.error();
        } else if (sourcePath.empty()) {
            r.loadedSource.clear();
        }
    }
    if (!sourcePath.empty() && !IsRegularFileNoThrow(sourcePath))
        ImGui::TextDisabled("%s", L("Datei nicht gefunden.", "File not found."));

    // Optionen
    const bool german = app::CurrentLanguage() == app::Language::German;
    const char* fitNames[] = {core::FitModeName(core::FitMode::Stretch, german), core::FitModeName(core::FitMode::Fit, german),
                              core::FitModeName(core::FitMode::Fill, german)};
    ImGui::SetNextItemWidth(220.0f);
    UI::Combo(L("Anpassung##imageReplace", "Fitting##imageReplace"), &r.fit, fitNames, 3);
    ImGui::SameLine();
    if (r.cellMode) {
        ImGui::SameLine();
        ImGui::TextDisabled(L("Zelle %ux%u bei %u,%u - der übrige Atlas bleibt unverändert", "Cell %ux%u at %u,%u - the rest of the atlas stays unchanged"),
                            r.cellW, r.cellH, r.cellX, r.cellY);
    } else {
        UI::Checkbox(L("Größe des Originals behalten##imageReplaceKeep", "Keep original size##imageReplaceKeep"), &r.keepSize);
    }
    if (r.originalFormat && !r.cellMode) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%ux%u)", r.originalFormat->width, r.originalFormat->height);
    }

    // Ergebnis bauen, sobald sich Quelle, Optionen oder Vorlage ändern; die Vorschau zeigt die
    // tatsächlich geschriebenen Bytes (DXT-Kompression, 16-Bit-TGA, JPG sind also sichtbar).
    const std::string key = r.loadedSource + "|" + std::to_string(r.fit) + "|" + (r.keepSize ? "1" : "0") + "|" +
                            r.originalTexFor + (r.cellMode ? "|cell" + std::to_string(r.cellX) + "," + std::to_string(r.cellY) : "");
    if (key != r.resultKey) {
        r.resultKey = key;
        r.result.reset();
        r.resultUnchanged = false;
        DeleteTexture(r.previewTex);
        if (r.sourceImage.Valid() && r.originalFormat && r.cellMode) {
            // Atlas laden, Zelle einsetzen, im Originalformat kodieren; Vorschau = Zelle aus den Bytes.
            auto atlas = core::LoadImageFile(r.originalTexFor);
            if (atlas) {
                core::PasteImage(*atlas, core::ResizeImage(r.sourceImage, r.cellW, r.cellH,
                                                           static_cast<core::FitMode>(std::clamp(r.fit, 0, 2))),
                                 r.cellX, r.cellY);
                auto bytes = core::EncodeImage(*atlas, *r.originalFormat);
                // DDS: nur die Blöcke der Zelle übernehmen, die übrigen Icons bleiben bitgleich.
                if (bytes && r.originalFormat->container == core::ImageContainer::Dds)
                    if (auto original = ReadResmenuBytes(r.originalTexFor))
                        if (auto spliced = core::SpliceDdsRegion(*original, *bytes, r.cellX, r.cellY, r.cellW, r.cellH))
                            bytes = std::move(*spliced);
                if (bytes) {
                    core::ReplaceResult cell;
                    cell.bytes = std::move(*bytes);
                    cell.format = *r.originalFormat;
                    cell.image = std::move(*atlas);
                    auto decoded = core::DecodeImage(cell.bytes);
                    const auto shown = core::CropImage(decoded ? *decoded : cell.image, r.cellX, r.cellY, r.cellW, r.cellH);
                    r.previewTex = UploadThumbnailTexture(shown.rgba, shown.width, shown.height);
                    r.previewW = static_cast<float>(shown.width);
                    r.previewH = static_cast<float>(shown.height);
                    r.result = std::move(cell);
                } else {
                    r.error = bytes.error();
                }
            } else {
                r.error = atlas.error();
            }
        } else if (r.sourceImage.Valid() && r.originalFormat) {
            core::ReplaceOptions options;
            options.fit = static_cast<core::FitMode>(std::clamp(r.fit, 0, 2));
            options.keepOriginalSize = r.keepSize;
            // Hat die Quelle schon exakt Format und Größe des Originals, wird sie unverändert
            // übernommen (keine erneute verlustbehaftete Kompression).
            auto sourceFormat = core::InspectImageFile(r.loadedSource);
            const auto& o = *r.originalFormat;
            const bool identical = sourceFormat && sourceFormat->container == o.container &&
                                   sourceFormat->width == o.width && sourceFormat->height == o.height &&
                                   (o.container != core::ImageContainer::Dds ||
                                    (sourceFormat->dds == o.dds && sourceFormat->mipmaps == o.mipmaps)) &&
                                   (o.container != core::ImageContainer::Tga || sourceFormat->tgaBits == o.tgaBits) &&
                                   (o.container != core::ImageContainer::Png || sourceFormat->pngAlpha == o.pngAlpha);
            auto rawBytes = identical ? ReadResmenuBytes(r.loadedSource) : std::nullopt;
            if (rawBytes) {
                core::ReplaceResult raw;
                raw.bytes = std::move(*rawBytes);
                raw.format = *sourceFormat;
                raw.image = r.sourceImage;
                r.result = std::move(raw);
                r.resultUnchanged = true;
                r.previewTex = UploadThumbnailTexture(r.sourceImage.rgba, r.sourceImage.width, r.sourceImage.height);
                r.previewW = static_cast<float>(r.sourceImage.width);
                r.previewH = static_cast<float>(r.sourceImage.height);
            } else if (auto built = core::BuildReplacement(r.sourceImage, *r.originalFormat, options)) {
                r.result = std::move(*built);
                auto decoded = core::DecodeImage(r.result->bytes);
                const core::RgbaImage& shown = decoded ? *decoded : r.result->image;
                r.previewTex = UploadThumbnailTexture(shown.rgba, shown.width, shown.height);
                r.previewW = static_cast<float>(shown.width);
                r.previewH = static_cast<float>(shown.height);
            } else {
                r.error = built.error();
            }
        }
    }
    if (!r.error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s", r.error.c_str());

    // Vorher / Nachher
    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.2f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 paneSize((avail.x - ImGui::GetStyle().ItemSpacing.x) * 0.5f, std::max(120.0f, avail.y - footer));
    const std::string originalInfo = r.originalTex
        ? std::to_string(static_cast<int>(r.originalW)) + "x" + std::to_string(static_cast<int>(r.originalH))
        : std::string(L("keine Vorschau", "no preview"));
    DrawComparePane("##imageReplaceBefore", targetExists ? L("Vorher", "Before") : L("Vorlage", "Template"), r.originalTex,
                    r.originalH > 0 ? r.originalW / r.originalH : 1.0f, paneSize, originalInfo);
    ImGui::SameLine();
    std::string resultInfo = L("Quelle wählen", "Choose a source");
    if (r.result)
        resultInfo = (r.cellMode ? std::to_string(r.cellW) + "x" + std::to_string(r.cellH) + L(" in ", " in ") : std::string()) +
                     core::DescribeImageFormat(r.result->format) + " · " + std::to_string(r.result->bytes.size()) + " Bytes" +
                     (r.resultUnchanged ? L(" · Quelle unverändert übernommen", " · source taken over unchanged") : "");
    else if (r.sourceImage.Valid())
        resultInfo = std::to_string(r.sourceImage.width) + "x" + std::to_string(r.sourceImage.height);
    DrawComparePane("##imageReplaceAfter", L("Nachher", "After"), r.previewTex, r.previewH > 0 ? r.previewW / r.previewH : 1.0f,
                    paneSize, resultInfo);

    if (!r.extraTargets.empty()) {
        std::string extras;
        for (const auto& e : r.extraTargets) extras += (extras.empty() ? "" : ", ") + e;
        ImGui::TextDisabled("%s %s", L("Ebenfalls geschrieben (im eigenen Format):", "Also written (in its own format):"), extras.c_str());
    }
    const bool hasProject = state.project.projectFolder[0] != '\0';
    ImGui::BeginDisabled(!r.result || !hasProject);
    if (UI::Button(L("Übernehmen##imageReplace", "Apply##imageReplace"))) {
        ApplyImageReplacement(state);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (UI::Button(L("Abbrechen##imageReplace", "Cancel##imageReplace"))) r.open = false;
    ImGui::SameLine();
    ImGui::TextDisabled("%s", hasProject
        ? L("Schreibt nach <Projekt>/Client/resmenu/... - das Original bleibt unverändert, Strg+Z nimmt es zurück.",
            "Writes to <Project>/Client/resmenu/... - the original stays untouched, Ctrl+Z reverts it.")
        : L("Projektordner konfigurieren, um Bilder zu ersetzen.", "Configure a project folder to replace images."));
    if (!r.open) {
        release();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace nge
