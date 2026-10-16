// AssetThumbnails.cpp
// NIF-/Textur-Vorschaubilder und Asset-Auswahl.

#include "EditorShared.hpp"

namespace nge {

// Findet den "resmap"-Ordner (wiederverwendet dieselbe mehrdeutigkeitstolerante Suche wie
// beim Karten-Scan, siehe ResolveMapSearchRootAndScan) - Basis für die Asset-Picker unten.
std::optional<std::filesystem::path> FindResmapRootForAssets(const std::string& clientFolder) {
    if (clientFolder.empty()) return std::nullopt;
    std::string selfName = std::filesystem::path(clientFolder).filename().string();
    std::transform(selfName.begin(), selfName.end(), selfName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (selfName == "resmap") return std::filesystem::path(clientFolder);
    std::vector<std::filesystem::path> candidates;
    FindAllResmapCandidates(clientFolder, 3, candidates);
    if (candidates.empty()) return std::nullopt;
    // Den mit den meisten Karten nehmen, wie beim Karten-Scan - derselbe "richtige" resmap-
    // Ordner ist auch hier die richtige Basis für fieldTexture/nif.
    std::size_t bestIdx = 0;
    std::size_t bestCount = ScanForMaps(candidates[0]).size();
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        const std::size_t count = ScanForMaps(candidates[i]).size();
        if (count > bestCount) { bestIdx = i; bestCount = count; }
    }
    return candidates[bestIdx];
}
// Lädt rgba (RGBA8, Top-Down, wie von LoadDdsImage/NifEmbeddedTexture geliefert) als kleine,
// nicht gemipmapte GL-Textur hoch - für 32x32-Vorschaubilder lohnt sich Mipmapping nicht.
std::uint32_t UploadThumbnailTexture(const std::vector<std::uint8_t>& rgba, std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || rgba.size() != static_cast<std::size_t>(width) * height * 4) return 0;
    std::uint32_t tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<int>(width), static_cast<int>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}
NifPreviewRenderTarget& NifPreviewTarget() {
    static NifPreviewRenderTarget target;
    return target;
}
std::uint32_t RenderNifPreviewThumbnail(const std::filesystem::path& nifPath, int size,
                                        std::vector<std::uint8_t>* pixelsOut) {
    auto& t = NifPreviewTarget();
    if (!t.initialized) {
        t.renderer.Init();
        glGenFramebuffers(1, &t.fbo);
        glGenTextures(1, &t.color);
        glGenRenderbuffers(1, &t.depth);
        t.initialized = true;
    }
    if (t.size != size) {
        glBindTexture(GL_TEXTURE_2D, t.color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindRenderbuffer(GL_RENDERBUFFER, t.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        t.size = size;
    }
    core::ObjectPlacementSet set;
    core::PlacedObject obj;
    obj.modelPath = nifPath.filename().string();
    obj.rotW = 1.0f;
    obj.scale = 1.0f;
    set.AddObject(obj);
    t.renderer.LoadModelsForSet(set, nifPath.parent_path());
    const auto bounds = t.renderer.ObjectWorldBounds(set, 0);
    if (!bounds) return 0;
    const auto& [lo, hi] = *bounds;
    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    const float radius = std::max(0.5f * std::sqrt(dx * dx + dy * dy + dz * dz), 1.0f);
    app::OrbitCamera camera;
    camera.SetTarget((lo[0] + hi[0]) * 0.5f, (lo[1] + hi[1]) * 0.5f, (lo[2] + hi[2]) * 0.5f);
    camera.SetOrientation(0.75f, 0.42f);
    camera.SetDistance(radius / std::sin(app::OrbitCamera::kFovY * 0.5f) * 0.8f);

    GLint previousFbo = 0, viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depth);
    std::uint32_t result = 0;
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        glViewport(0, 0, size, size);
        glClearColor(0.11f, 0.13f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        t.renderer.Draw(set, camera, size, size);
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4);
        glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        // OpenGL liest von unten nach oben; Thumbnails erwarten die erste Zeile oben.
        std::vector<std::uint8_t> flipped(pixels.size());
        const std::size_t row = static_cast<std::size_t>(size) * 4;
        for (int y = 0; y < size; ++y)
            std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(y * row), row,
                        flipped.begin() + static_cast<std::ptrdiff_t>((size - 1 - y) * row));
        for (std::size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
        if (pixelsOut) *pixelsOut = flipped;
        result = UploadThumbnailTexture(flipped, static_cast<std::uint32_t>(size), static_cast<std::uint32_t>(size));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return result;
}
// Lädt (einmalig, gecacht in state.assetThumbnails) ein kleines Vorschaubild für einen
// Asset-Picker-Eintrag oder Layer: entweder direkt eine Textur-Datei (isNif=false, z.B. .dds)
// oder die erste Diffuse-Textur eines .nif-Modells (isNif=true - eingebettet über NiPixelData
// ODER extern über den "fieldTexture"-Ordner aufgelöst, siehe ResolveLegacyAssetPath). Gibt ein
// AssetThumbnail zurück (tex==0: kein Vorschaubild, z.B. Mesh ohne Textur oder Ladefehler).
// resmapRoot wird nur für den NIF-Fall gebraucht (externe Textur-Auflösung).
EditorState::AssetThumbnail GetOrLoadAssetThumbnail(EditorState& state,
                                                      const std::filesystem::path& resolvedPath,
                                                      bool isNif,
                                                      const std::filesystem::path& resmapRoot) {
    const std::string key = resolvedPath.string();
    if (auto it = state.assetThumbnails.find(key); it != state.assetThumbnails.end()) {
        return it->second;
    }

    EditorState::AssetThumbnail thumb;
    const auto loadTextureThumbnail = [&](const std::filesystem::path& texturePath) {
        const std::string ext = LowerAscii(texturePath.extension().string());
        if (ext == ".tga") {
            if (auto image = core::LoadTgaImage(texturePath)) {
                thumb.tex = UploadThumbnailTexture(image->rgba, image->width, image->height);
                if (thumb.tex && image->height > 0)
                    thumb.aspect = static_cast<float>(image->width) / static_cast<float>(image->height);
                return thumb.tex != 0;
            }
            return false;
        }
        if (ext == ".dds") {
            if (auto image = core::LoadDdsImage(texturePath)) {
                thumb.tex = UploadThumbnailTexture(image->rgba, image->width, image->height);
                if (thumb.tex && image->height > 0)
                    thumb.aspect = static_cast<float>(image->width) / static_cast<float>(image->height);
                return thumb.tex != 0;
            }
            return false;
        }
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
            if (auto image = app::LoadPlatformRasterImage(texturePath)) {
                thumb.tex = UploadThumbnailTexture(image->rgba, image->width, image->height);
                if (thumb.tex && image->height > 0)
                    thumb.aspect = static_cast<float>(image->width) / static_cast<float>(image->height);
                return thumb.tex != 0;
            }
            return false;
        }
        return false;
    };

    if (!isNif) {
        loadTextureThumbnail(resolvedPath);
    } else if (const auto rendered = RenderNifPreviewThumbnail(resolvedPath); rendered != 0) {
        // Gerendertes Modell statt nur seiner ersten Textur.
        thumb.tex = rendered;
        thumb.aspect = 1.0f;
    } else if (auto model = core::LoadNifMesh(resolvedPath)) {
        // Zwei-Ebenen-tiefer "Fake"-Kartenordner unter resmapRoot: ResolveLegacyAssetPath leitet
        // den Asset-Root aus mapDir.parent_path().parent_path() her (Layout <AssetRoot>/field/
        // <Karte>/...) - der letzte Pfadbestandteil muss dafür gar nicht existieren, siehe
        // src/core/legacy/LegacyPathResolve.cpp.
        const std::filesystem::path fakeMapDir = resmapRoot / "field" / "_thumbnail";
        for (const auto& part : model->parts) {
            if (part.embeddedDiffuseTexture) {
                thumb.tex = UploadThumbnailTexture(part.embeddedDiffuseTexture->rgba,
                                                    part.embeddedDiffuseTexture->width,
                                                    part.embeddedDiffuseTexture->height);
                if (thumb.tex && part.embeddedDiffuseTexture->height > 0) {
                    thumb.aspect = static_cast<float>(part.embeddedDiffuseTexture->width) /
                                   static_cast<float>(part.embeddedDiffuseTexture->height);
                }
                break;
            }
            if (!part.diffuseTexture.empty()) {
                if (auto texPath = core::legacy::ResolveLegacyAssetPath(fakeMapDir, part.diffuseTexture)) {
                    if (loadTextureThumbnail(*texPath)) break;
                }
            }
        }
    }
    return state.assetThumbnails.emplace(key, thumb).first->second;
}
// Startet die einmalige Vorlade-Sequenz aller NIF-Vorschaubilder der GESAMTEN Asset-
// Bibliothek unter resmapRoot (nicht nur der auf einer Karte platzierten Objekte) - siehe
// EditorState::nifPrecacheActive und DrawMapEditorLauncher. Erfasst bewusst ALLE .nif unter
// resmap (field/IDField/KDField/nif(s)/...), weil echte SHMDs diese Quellen mischen.
// Macht nichts, wenn exakt dieser resmapRoot schon einmal komplett durchgelaufen ist.
void StartNifThumbnailPrecache(EditorState& state, const std::filesystem::path& resmapRoot) {
    if (state.nifPrecacheDoneForRoot == resmapRoot.string()) return;
    // SHMD-Verweise liegen nicht nur unter resmap/nif(s), sondern u.a. auch unter field,
    // IDField und KDField. Deshalb die komplette resmap-Hierarchie als Modellbibliothek
    // inventarisieren. Die Vorschaubilder werden weiterhin stückweise pro Frame erzeugt.
    state.availableNifFiles = ListFilesByExtension(resmapRoot, {".nif"});
    state.nifAssetRoot = resmapRoot;
    state.resmapRootForThumbnails = resmapRoot;
    state.nifListScanned = true;
    state.nifPrecacheQueue = state.availableNifFiles;
    state.nifPrecacheCursor = 0;
    state.nifPrecacheActive = !state.nifPrecacheQueue.empty();
    if (!state.nifPrecacheActive) state.nifPrecacheDoneForRoot = resmapRoot.string();
}
// Verarbeitet bis zu filesPerFrame Einträge der Vorlade-Warteschlange (siehe
// StartNifThumbnailPrecache) - bewusst in kleinen Häppchen pro Frame statt einer einzigen
// blockierenden Schleife, damit sich der Fortschrittsbalken sichtbar bewegt. Die
// Gesamtdauer ändert sich dadurch nicht (~4ms/Datei im Schnitt für einen vollen
// LoadNifMesh-Aufruf, siehe CHANGELOG [0.44.8]) - nur, wie fein sie über Frames verteilt
// wird. Bei z.B. 3436 Dateien und 8/Frame sind das ~14s Gesamtdauer, verteilt auf ~430
// Frames statt eines einzigen ~14s-Frames (der wie ein Absturz aussähe).
void AdvanceNifPrecache(EditorState& state, int filesPerFrame) {
    if (!state.nifPrecacheActive) return;
    for (int i = 0; i < filesPerFrame && state.nifPrecacheCursor < state.nifPrecacheQueue.size(); ++i) {
        const std::filesystem::path resolvedPath = state.nifAssetRoot / state.nifPrecacheQueue[state.nifPrecacheCursor];
        GetOrLoadAssetThumbnail(state, resolvedPath, true, state.resmapRootForThumbnails);
        ++state.nifPrecacheCursor;
    }
    if (state.nifPrecacheCursor >= state.nifPrecacheQueue.size()) {
        state.nifPrecacheActive = false;
        state.nifPrecacheDoneForRoot = state.resmapRootForThumbnails.string();
    }
}
// Popup mit Textfilter zur Auswahl einer Datei aus files (relative Pfade). Gibt true zurück
// UND setzt outSelected, wenn der Nutzer in diesem Frame etwas ausgewählt hat. Muss von
// ImGui::OpenPopup(popupId) aus geöffnet werden - zeichnet nur, wenn das Popup offen ist.
// assetRoot: Basis für die relativen Pfade in files (zum Auflösen der Vorschaubilder).
// isNif: steuert, ob ein Eintrag als Textur-Datei oder als .nif-Modell geladen wird.
// resmapRoot: nur für isNif=true gebraucht (externe Textur-Auflösung, siehe GetOrLoadAssetThumbnail).
bool DrawAssetPickerPopup(const char* popupId, const std::vector<std::string>& files,
                           std::string& filterBuf, std::string& outSelected,
                           EditorState& state, const std::filesystem::path& assetRoot, bool isNif,
                           const std::filesystem::path& resmapRoot) {
    bool picked = false;
    ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(popupId)) {
        char filterCstr[128];
        std::snprintf(filterCstr, sizeof(filterCstr), "%s", filterBuf.c_str());
        if (DrawSearchInput("assetPickerFilter",L("Filtern...","Filter..."),
                            filterCstr,sizeof(filterCstr),455.0f)) {
            filterBuf = filterCstr;
        }

        // Gefilterte Indizes VORAB bestimmen (reine String-Vergleiche, kein Datei-I/O) - erst
        // danach per ImGuiListClipper nur die tatsächlich sichtbaren Zeilen zeichnen. Nötig,
        // damit Vorschaubilder unten NUR für sichtbare Einträge geladen werden, nicht für die
        // komplette (ggf. tausende Einträge lange) Liste auf einen Schlag.
        std::vector<std::size_t> matching;
        matching.reserve(files.size());
        std::string filterLower = filterBuf;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (std::size_t i = 0; i < files.size(); ++i) {
            if (filterLower.empty()) { matching.push_back(i); continue; }
            std::string fLower = files[i];
            std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (fLower.find(filterLower) != std::string::npos) matching.push_back(i);
        }

        ImGui::Text("%zu Datei(en)", matching.size());
        ImGui::Separator();
        ImGui::BeginChild("##assetList", ImVec2(0.0f, 340.0f), true);
        constexpr float kThumbSize = 32.0f;
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(matching.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const std::string& f = files[matching[static_cast<std::size_t>(row)]];
                ImGui::PushID(row);
                const std::filesystem::path resolvedPath = assetRoot / f;
                const auto thumb = GetOrLoadAssetThumbnail(state, resolvedPath, isNif, resmapRoot);
                if (thumb.tex) {
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)), ImVec2(kThumbSize, kThumbSize));
                } else {
                    ImGui::Dummy(ImVec2(kThumbSize, kThumbSize)); // Platzhalter - hält die Zeilen bündig
                }
                ImGui::SameLine();
                if (UI::Selectable(f.c_str(), false, 0, ImVec2(0.0f, kThumbSize))) {
                    outSelected = f;
                    picked = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        if (UI::Button(L("Abbrechen","Cancel"))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return picked;
}

} // namespace nge
