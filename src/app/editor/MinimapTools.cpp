// MinimapTools.cpp
// Client-Bilder der Karte: Spiel-Minimap (MapViewInfo-Zeile, Client-/Projektbild finden, Draufsicht
// offscreen rendern, resmenu\minimap\<Karte>.dds schreiben) und Stadtportal-Bilder
// (resmenu\TownPortal\<MapName>.dds und _disable.dds, perspektivische Aufnahme des Ankunftsorts).

#include "EditorShared.hpp"

#include "mapeditor/core/DdsWrite.hpp"
#include "mapeditor/core/legacy/Minimap.hpp"
#include "mapeditor/core/legacy/TownPortalImage.hpp"

namespace nge {

namespace {
int MapViewInt(const core::legacy::ShnFile& f, std::size_t row, const char* col, int fallback) {
    const std::string v = TrimAscii(ShnCellText(f, row, col));
    if (v.empty()) return fallback;
    return std::atoi(v.c_str());
}

float MapSpanX(const EditorState& state) {
    return static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 0) * state.heightmap.BlockWidth();
}
float MapSpanZ(const EditorState& state) {
    return static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 0) * state.heightmap.BlockHeight();
}
} // namespace

std::optional<MapViewRow> FindMapViewRow(EditorState& state, EditorState::ShnSource source) {
    const std::string stem = state.legacySaveStem;
    if (stem.empty()) return std::nullopt;
    const int di = FindOrLoadShnDoc(state, "MapViewInfo.shn", source);
    if (di < 0) return std::nullopt;
    const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
    const std::string serverName = CurrentMapServerName(state);
    std::optional<std::size_t> byFolder;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        const std::string name = TrimAscii(ShnCellText(f, r, "MapName"));
        if (SameMapName(name, serverName) || SameMapName(name, stem)) {
            byFolder = r;
            break;
        }
        if (!byFolder && SameMapName(TrimAscii(ShnCellText(f, r, "MapFolderName")), stem)) byFolder = r;
    }
    if (!byFolder) return std::nullopt;
    MapViewRow out;
    out.document = di;
    out.row = *byFolder;
    out.folder = TrimAscii(ShnCellText(f, out.row, "MapFolderName"));
    if (out.folder.empty() || out.folder == "-") out.folder = stem;
    out.minimapView = MapViewInt(f, out.row, "MinimapView", 0) != 0;
    out.view = {MapViewInt(f, out.row, "StartX", 0), MapViewInt(f, out.row, "StartY", 0), MapViewInt(f, out.row, "EndX", 511),
                MapViewInt(f, out.row, "EndY", 511)};
    return out;
}

std::filesystem::path GameMinimapPath(EditorState& state, const std::string& folder, bool* fromProject) {
    if (fromProject) *fromProject = false;
    if (folder.empty()) return {};
    return GameResmenuPath(state, core::legacy::MinimapRelativePath(folder), fromProject);
}

std::vector<std::uint8_t> CaptureSceneRgba(EditorState& state, const app::OrbitCamera& cam, std::uint32_t size,
                                           bool withObjects, bool withSky, bool withFog) {
    if (size == 0 || g_disableGlUploads) return {};
    const int previousTarget = state.renderer.CurrentRenderTarget();
    const bool previousGlow = state.renderer.GlowEnabled();
    const auto lighting = state.renderer.GetSceneLighting();
    auto capture = lighting;
    capture.fogEnabled = withFog && lighting.fromMapData;
    state.renderer.SelectRenderTarget(app::HeightmapRenderer::kCaptureTarget);
    state.renderer.SetGlowEnabled(false);
    state.renderer.SetSceneLighting(capture);
    state.nifMeshRenderer.SetSceneLighting(capture);
    state.shmdCategoryMeshRenderer.SetSceneLighting(capture);

    const int px = static_cast<int>(size);
    state.renderer.BeginScene(cam, px, px, false);
    if (withObjects) {
        state.nifMeshRenderer.Draw(state.placementSet, cam, px, px, &state.objectHidden);
        // Wasser und Bodenobjekte immer, die Himmelskuppel nur auf Wunsch (von oben läge sie über allem).
        std::vector<char> hidden = state.shmdCategoryHidden;
        hidden.resize(state.shmdCategoryRenderSet.Count(), 0);
        if (!withSky)
            for (std::size_t i = 0; i < hidden.size() && i < state.shmdCategoryRenderKind.size(); ++i)
                if (state.shmdCategoryRenderKind[i] == 0) hidden[i] = 1;
        state.shmdCategoryMeshRenderer.Draw(state.shmdCategoryRenderSet, cam, px, px, &hidden);
    }
    const GLuint tex = state.renderer.EndScene();

    std::vector<std::uint8_t> glPixels(static_cast<std::size_t>(size) * size * 4);
    GLint prevTex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, glPixels.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTex));

    state.renderer.SetSceneLighting(lighting);
    state.nifMeshRenderer.SetSceneLighting(lighting);
    state.shmdCategoryMeshRenderer.SetSceneLighting(lighting);
    state.renderer.SetGlowEnabled(previousGlow);
    state.renderer.SelectRenderTarget(previousTarget);

    // OpenGL liefert die unterste Zeile zuerst; Ergebnis: Zeile 0 = obere Bildkante.
    std::vector<std::uint8_t> image(glPixels.size());
    const std::size_t rowBytes = static_cast<std::size_t>(size) * 4;
    for (std::uint32_t y = 0; y < size; ++y)
        std::memcpy(&image[y * rowBytes], &glPixels[(size - 1 - y) * rowBytes], rowBytes);
    for (std::size_t i = 3; i < image.size(); i += 4) image[i] = 255;
    return image;
}

std::vector<std::uint8_t> RenderMinimapRgba(EditorState& state, std::uint32_t size, bool withObjects) {
    const float spanX = MapSpanX(state), spanZ = MapSpanZ(state);
    if (spanX <= 0.0f || spanZ <= 0.0f) return {};
    const float half = 0.5f * std::max(spanX, spanZ);

    // Orthografische Kamera fast senkrecht von oben (Pitch-Grenze 1.5 rad), von Süden: oben = Norden.
    app::OrbitCamera cam;
    cam.SetOrthographic(true);
    cam.SetTarget(spanX * 0.5f, 0.0f, spanZ * 0.5f);
    cam.SetDistance(half / std::tan(app::OrbitCamera::kFovY * 0.5f));
    cam.SetOrientation(0.0f, 1.5f);
    auto square = CaptureSceneRgba(state, cam, size, withObjects, false, false);
    if (square.empty() || std::abs(spanX - spanZ) < 1e-3f) return square;
    // Nicht quadratische Karte: das Kartenrechteck aus dem Quadrat um die Mitte schneiden.
    const float x0 = (half - spanX * 0.5f) / (2.0f * half);
    const float z0 = (half - spanZ * 0.5f) / (2.0f * half);
    return core::legacy::CropResampleRgba(square, size, size, {x0, z0, 1.0f - x0, 1.0f - z0}, size, size);
}

std::vector<std::uint8_t> MinimapWalkMask(const EditorState& state, std::uint32_t size) {
    const float spanX = MapSpanX(state), spanZ = MapSpanZ(state);
    if (state.walkGrid.Cols() == 0 || spanX <= 0.0f || spanZ <= 0.0f) return {};
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(size) * size, 1);
    for (std::uint32_t y = 0; y < size; ++y)
        for (std::uint32_t x = 0; x < size; ++x) {
            const float wx = (x + 0.5f) / size * spanX;
            const float wz = (1.0f - (y + 0.5f) / size) * spanZ;
            mask[static_cast<std::size_t>(y) * size + x] = core::level::WalkBlockedAtWorld(state.walkGrid, wx, wz) ? 0 : 1;
        }
    return mask;
}

bool GenerateMinimap(EditorState& state, std::vector<std::string>& report) {
    if (state.project.projectFolder[0] == '\0') {
        report.push_back(L("- Minimap: kein Projektordner.", "- Minimap: no project folder."));
        return false;
    }
    const std::string stem = state.legacySaveStem;
    if (stem.empty()) {
        report.push_back(L("- Minimap: keine Karte geöffnet.", "- Minimap: no map open."));
        return false;
    }
    constexpr std::uint32_t kSize = 512;
    auto rgba = RenderMinimapRgba(state, kSize, state.minimapWithObjects);
    if (rgba.empty()) {
        report.push_back(L("- Minimap: Draufsicht konnte nicht gerendert werden.", "- Minimap: top-down view could not be rendered."));
        return false;
    }
    if (state.minimapStyled) {
        const auto mask = MinimapWalkMask(state, kSize);
        rgba = core::legacy::StylizeMinimap(rgba, mask, kSize);
    }
    const auto row = FindMapViewRow(state, EditorState::ShnSource::Client);
    const std::string folder = row ? row->folder : stem;
    const auto target = InterfaceProjectOverridePath(state, core::legacy::MinimapRelativePath(folder));
    if (target.empty()) {
        report.push_back(L("- Minimap: Zielpfad im Projekt nicht bestimmbar.", "- Minimap: project target path unavailable."));
        return false;
    }
    if (auto written = core::WriteDdsFile(target, rgba, kSize, kSize, core::DdsFormat::Dxt1, false); !written) {
        report.push_back("- " + written.error());
        return false;
    }
    report.push_back("+ Client resmenu/minimap/" + folder + ".dds (512x512 DXT1)");
    if (auto it = state.assetThumbnails.find(target.string()); it != state.assetThumbnails.end()) {
        if (it->second.tex != 0) {
            const GLuint t = it->second.tex;
            glDeleteTextures(1, &t);
        }
        state.assetThumbnails.erase(it);
    }

    // MapViewInfo: Minimap an, Ausschnitt = ganze Karte (das erzeugte Bild zeigt die ganze Karte).
    for (const auto src : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        const auto r = FindMapViewRow(state, src);
        if (!r) {
            report.push_back(std::string("- ") + ShnSourceName(src) + L(" MapViewInfo: keine Zeile für die Karte.",
                                                                          " MapViewInfo: no row for the map."));
            continue;
        }
        const auto& f = state.shnFiles[static_cast<std::size_t>(r->document)].file;
        const auto old = r->view;
        // Über ApplyShnCellText, damit die Zellen als Änderung markiert sind (SHN-Sicherheitscheck).
        bool cellsOk = true;
        for (const auto& [col, value] : {std::pair<const char*, const char*>{"MinimapView", "1"}, {"StartX", "0"}, {"StartY", "0"},
                                          {"EndX", "511"}, {"EndY", "511"}}) {
            const int c = ShnColumnIndexByName(f, col);
            cellsOk = c >= 0 && ApplyShnCellText(state, r->document, static_cast<int>(r->row), c, value, false) && cellsOk;
        }
        const bool saved = cellsOk && SaveShnDocument(state, r->document);
        if (!saved) report.push_back("- " + state.shnStatus);
        char buf[160];
        std::snprintf(buf, sizeof(buf), " MapViewInfo: MinimapView 1, Ausschnitt 0,0-511,511 (vorher %d,%d-%d,%d)", old.startX,
                      old.startY, old.endX, old.endY);
        report.push_back(std::string(saved ? "+ " : "- FEHLER ") + ShnSourceName(src) + buf);
    }
    state.minimapShowGame = true;
    return true;
}

void ProcessPendingMinimap(EditorState& state) {
    if (!state.minimapPendingGenerate || state.meshDirty) return;
    state.minimapPendingGenerate = false;
    std::vector<std::string> report;
    const bool ok = GenerateMinimap(state, report);
    for (const auto& line : report) state.newMapReport.push_back(line);
    state.statusMessage = ok ? L("Minimap erzeugt.", "Minimap generated.") : L("Minimap konnte nicht erzeugt werden.", "Minimap could not be generated.");
}

std::filesystem::path GameResmenuPath(EditorState& state, const std::string& rel, bool* fromProject) {
    if (fromProject) *fromProject = false;
    const auto project = InterfaceProjectOverrideRoot(state);
    if (!project.empty())
        if (auto hit = core::legacy::ResolveCaseInsensitivePath(project, rel)) {
            if (fromProject) *fromProject = true;
            return *hit;
        }
    if (!state.interfaceRoot.empty())
        if (auto hit = core::legacy::ResolveCaseInsensitivePath(state.interfaceRoot, rel)) return *hit;
    return {};
}

bool GenerateTownPortalImages(EditorState& state, const std::string& mapName, float x, float y,
                              std::vector<std::string>& report) {
    if (state.project.projectFolder[0] == '\0' || mapName.empty()) {
        report.push_back(L("- Portalbilder: kein Projektordner oder Kartenname.", "- Portal images: no project folder or map name."));
        return false;
    }
    // Wie die Originale: Blick über den Ankunftsort, leicht erhöht, mit Himmel und Nebel.
    const float ground = state.heightmap.Width() > 1 ? state.heightmap.SampleWorld(x, y) : 0.0f;
    app::OrbitCamera cam;
    cam.SetTarget(x, ground + 90.0f, y);
    cam.SetDistance(950.0f);
    cam.SetOrientation(state.townPortalImageYaw, 0.32f);
    constexpr std::uint32_t kSize = 256;
    auto rgba = CaptureSceneRgba(state, cam, kSize, true, true, true);
    if (rgba.empty()) {
        report.push_back(L("- Portalbilder: Aufnahme fehlgeschlagen.", "- Portal images: capture failed."));
        return false;
    }
    core::legacy::ApplyRoundedCorners(rgba, kSize, 25.0f);
    const auto disabled = core::legacy::MakeTownPortalDisabled(rgba);
    bool ok = true;
    for (const bool grey : {false, true}) {
        const auto rel = core::legacy::TownPortalImageRelativePath(mapName, grey);
        const auto target = InterfaceProjectOverridePath(state, rel);
        auto written = target.empty() ? std::expected<void, std::string>(std::unexpected(std::string("kein Zielpfad")))
                                      : core::WriteDdsFile(target, grey ? disabled : rgba, kSize, kSize, core::DdsFormat::Dxt5, true);
        if (!written) {
            report.push_back("- " + rel + ": " + written.error());
            ok = false;
            continue;
        }
        report.push_back("+ Client resmenu/" + rel + " (256x256 DXT5, Mipmaps)");
        if (auto it = state.assetThumbnails.find(target.string()); it != state.assetThumbnails.end()) {
            if (it->second.tex != 0) {
                const GLuint t = it->second.tex;
                glDeleteTextures(1, &t);
            }
            state.assetThumbnails.erase(it);
        }
    }
    return ok;
}

} // namespace nge
