// LoadingScreens.cpp
// Ladebildschirm-Editor im Interface-Bereich. Der Client zeigt beim Betreten einer Karte
// resmenu/loading/<MapViewInfo.Loading>.png (bzw. .tga); mehrere Karten teilen sich Bilder
// (NA2016: 138 Karten, 129 Namen, BFGate01 viermal). Hier: Kartenliste, Vorschau, "verwendet von",
// anderes Bild zuweisen (MapViewInfo in Client und Server, ein Undo-Schritt), Bild ersetzen (alle
// vorhandenen Varianten PNG/TGA im jeweils eigenen Format), eigenes Bild nur für diese Karte und die
// gemeinsamen Dateien (NowLoading, ProgressBar, NowLoadingTextColor, Tipps).

#include "EditorShared.hpp"

namespace nge {

namespace {
// Alle Bilder in resmenu/loading (Client und Projekt) nach Name ohne Endung, Groß/Klein egal.
struct LoadingImage {
    std::string stem;              // Schreibweise der ersten gefundenen Datei
    std::vector<std::string> rels; // "loading/<Datei>", PNG zuerst, dann TGA, dann der Rest
};
struct LoadingCatalog {
    std::string key;
    std::map<std::string, LoadingImage> byLowerStem;
};

int ExtensionRank(const std::string& ext) {
    if (ext == ".png") return 0;
    if (ext == ".tga") return 1;
    if (ext == ".dds") return 2;
    return 3;
}

const LoadingCatalog& Catalog(EditorState& state) {
    static LoadingCatalog catalog;
    const auto sourceRoot = ResmenuSourceRoot(state);
    const auto projectRoot = InterfaceProjectOverrideRoot(state);
    const std::string key = sourceRoot.string() + "|" + projectRoot.string() + "|" + std::to_string(state.resmenuRevision);
    if (catalog.key == key) return catalog;
    catalog.key = key;
    catalog.byLowerStem.clear();
    for (const auto& root : {sourceRoot, projectRoot}) {
        if (root.empty()) continue;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(root / "loading", ec)) {
            if (!entry.is_regular_file(ec)) continue;
            const std::string ext = LowerAscii(entry.path().extension().string());
            if (ext != ".png" && ext != ".tga" && ext != ".dds" && ext != ".jpg" && ext != ".bmp") continue;
            const std::string stem = entry.path().stem().string();
            auto& image = catalog.byLowerStem[LowerAscii(stem)];
            if (image.stem.empty()) image.stem = stem;
            const std::string rel = "loading/" + entry.path().filename().string();
            const bool known = std::any_of(image.rels.begin(), image.rels.end(),
                                           [&](const std::string& r) { return LowerAscii(r) == LowerAscii(rel); });
            if (!known) image.rels.push_back(rel);
        }
    }
    for (auto& [lower, image] : catalog.byLowerStem)
        std::stable_sort(image.rels.begin(), image.rels.end(), [](const std::string& a, const std::string& b) {
            return ExtensionRank(LowerAscii(std::filesystem::path(a).extension().string())) <
                   ExtensionRank(LowerAscii(std::filesystem::path(b).extension().string()));
        });
    return catalog;
}

const LoadingImage* FindImage(EditorState& state, const std::string& stem) {
    const auto& c = Catalog(state);
    const auto it = c.byLowerStem.find(LowerAscii(TrimAscii(stem)));
    return it == c.byLowerStem.end() ? nullptr : &it->second;
}

struct MapEntry {
    long long id = -1;
    std::size_t row = 0;
    std::string mapName, displayName, loading, loadLocation;
};

long long CellId(const core::legacy::ShnFile& f, std::size_t row) {
    return std::atoll(TrimAscii(ShnCellText(f, row, "ID")).c_str());
}

int RowById(const core::legacy::ShnFile& f, long long id) {
    for (std::size_t r = 0; r < f.rows.size(); ++r)
        if (CellId(f, r) == id) return static_cast<int>(r);
    return -1;
}

std::vector<MapEntry> CollectMaps(EditorState& state, int clientDoc) {
    std::vector<MapEntry> out;
    const auto& f = ShnDocFile(state, clientDoc);
    std::unordered_map<std::string, std::string> displayNames;
    if (const int mi = SharedShnDoc(state, "MapInfo.shn", EditorState::ShnSource::Client); mi >= 0) {
        const auto& m = ShnDocFile(state, mi);
        for (std::size_t r = 0; r < m.rows.size(); ++r)
            displayNames[LowerAscii(TrimAscii(ShnCellText(m, r, "MapName")))] = TrimAscii(ShnCellText(m, r, "Name"));
    }
    out.reserve(f.rows.size());
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        MapEntry e;
        e.id = CellId(f, r);
        e.row = r;
        e.mapName = TrimAscii(ShnCellText(f, r, "MapName"));
        e.loading = TrimAscii(ShnCellText(f, r, "Loading"));
        e.loadLocation = TrimAscii(ShnCellText(f, r, "LoadLocation"));
        if (const auto it = displayNames.find(LowerAscii(e.mapName)); it != displayNames.end()) e.displayName = it->second;
        out.push_back(std::move(e));
    }
    return out;
}

// Setzt eine MapViewInfo-Spalte der Karte in Client und Server (ein Undo-Schritt).
bool SetMapViewCell(EditorState& state, long long id, const char* column, const std::string& value, const std::string& label) {
    bool any = false, ok = true;
    DataHistoryBegin(state, label);
    for (const auto source : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
        const int doc = SharedShnDoc(state, "MapViewInfo.shn", source);
        if (doc < 0) continue;
        auto& f = ShnDocFile(state, doc);
        const int row = RowById(f, id);
        const int col = ShnColumnIndexByName(f, column);
        if (row < 0 || col < 0) {
            ok = false;
            continue;
        }
        if (TrimAscii(ShnCellText(f, static_cast<std::size_t>(row), column)) == value) {
            any = true;
            continue;
        }
        const bool written = ApplyShnCellText(state, doc, row, col, value, true);
        ok = ok && written;
        any = any || written;
    }
    DataHistoryCommit(state);
    if (any && ok) state.statusMessage = label;
    return any && ok;
}

// Vorschau in einem Rechteck (Seitenverhältnis bleibt, Schachbrett hinter transparenten Stellen).
void DrawFittedPreview(EditorState& state, const std::string& rel, ImVec2 area) {
    float aspect = 4.0f / 3.0f;
    const std::uint32_t tex = rel.empty() ? 0 : ResmenuPreviewTexture(state, rel, &aspect);
    ImVec2 size = area;
    if (size.x / std::max(size.y, 1.0f) > aspect) size.x = size.y * aspect;
    else size.y = size.x / aspect;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (tex) {
        DrawCheckerboard(origin, ImVec2(origin.x + size.x, origin.y + size.y));
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), size);
    } else {
        ImGui::GetWindowDrawList()->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(200, 90, 70, 255));
        ImGui::Dummy(size);
    }
}

std::string FormatOf(EditorState& state, const std::string& rel) {
    auto fmt = core::InspectImageFile(ResmenuEffectivePath(state, rel));
    return fmt ? core::DescribeImageFormat(*fmt) : std::string(L("unlesbar", "unreadable"));
}

// Freier Name für ein eigenes Bild der Karte: MapName, sonst MapName_2, _3 ...
std::string SuggestImageName(EditorState& state, const std::string& mapName) {
    const std::string base = mapName.empty() ? std::string("Loading") : mapName;
    if (!FindImage(state, base)) return base;
    for (int n = 2; n < 1000; ++n) {
        const std::string candidate = base + "_" + std::to_string(n);
        if (!FindImage(state, candidate)) return candidate;
    }
    return base + "_new";
}

bool ValidImageName(const std::string& name) {
    if (name.empty() || name.size() > 60) return false;
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    });
}

void DrawSharedFiles(EditorState& state) {
    // Gemeinsame Dateien des Ladevorgangs (aus Fiesta.bin belegt: NowLoading, ProgressBar,
    // NowLoadingTextColor) und die Tipp-Bilder unter loading/Tips.
    std::vector<std::string> rels;
    for (const char* name : {"NowLoading", "ProgressBar", "NowLoadingTextColor"})
        if (const auto* image = FindImage(state, name))
            for (const auto& rel : image->rels) rels.push_back(rel);
    std::error_code ec;
    std::vector<std::string> tips;
    for (const auto& root : {ResmenuSourceRoot(state), InterfaceProjectOverrideRoot(state)}) {
        if (root.empty()) continue;
        for (const auto& entry : std::filesystem::directory_iterator(root / "loading" / "Tips", ec)) {
            if (!entry.is_regular_file(ec)) continue;
            const std::string rel = "loading/Tips/" + entry.path().filename().string();
            if (std::none_of(tips.begin(), tips.end(), [&](const std::string& t) { return LowerAscii(t) == LowerAscii(rel); }))
                tips.push_back(rel);
        }
    }
    std::sort(tips.begin(), tips.end(), [](const std::string& a, const std::string& b) { return LowerAscii(a) < LowerAscii(b); });
    rels.insert(rels.end(), tips.begin(), tips.end());
    if (rels.empty()) {
        ImGui::TextDisabled("%s", L("Keine gemeinsamen Ladebild-Dateien gefunden.", "No shared loading files found."));
        return;
    }
    ImGui::TextDisabled("%s", L("Wo der Client diese Dateien auf dem Ladebildschirm platziert, ist nicht belegt; die Vorschau zeigt sie einzeln.",
                                "Where the client places these files on the loading screen is not proven; the preview shows them one by one."));
    for (const auto& rel : rels) {
        ImGui::PushID(rel.c_str());
        DrawFittedPreview(state, rel, ImVec2(160.0f, 60.0f));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(rel.c_str());
        ImGui::TextDisabled("%s%s", FormatOf(state, rel).c_str(),
                            CachedInterfaceOverrideState(state, rel) != InterfaceOverrideState::None ? L(" · Projektkopie", " · project copy") : "");
        if (UI::SmallButton(L("Ersetzen...##loadingShared", "Replace...##loadingShared"))) OpenImageReplaceDialog(state, rel, rel);
        ImGui::EndGroup();
        ImGui::PopID();
    }
}

void DrawAssignPicker(EditorState& state, const MapEntry& map) {
    if (!ImGui::BeginPopup("##loadingPick")) return;
    ImGui::TextUnformatted(L("Vorhandenes Ladebild zuweisen", "Assign an existing loading image"));
    ImGui::SetNextItemWidth(260.0f);
    UI::InputTextWithHint("##loadingPickFilter", L("Name filtern...", "Filter name..."), state.loadingPickFilter, sizeof(state.loadingPickFilter));
    const std::string needle = LowerAscii(state.loadingPickFilter);
    ImGui::BeginChild("##loadingPickList", ImVec2(260.0f, 380.0f), true);
    for (const auto& [lower, image] : Catalog(state).byLowerStem) {
        if (!needle.empty() && lower.find(needle) == std::string::npos) continue;
        if (lower == "nowloading" || lower == "progressbar" || lower == "nowloadingtextcolor") continue;
        if (UI::Selectable((image.stem + "##pick").c_str(), LowerAscii(state.loadingPickChoice) == lower))
            state.loadingPickChoice = image.stem;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginGroup();
    const auto* chosen = FindImage(state, state.loadingPickChoice);
    DrawFittedPreview(state, chosen ? chosen->rels.front() : std::string(), ImVec2(400.0f, 300.0f));
    ImGui::BeginDisabled(!chosen);
    if (UI::Button(L("Zuweisen##loadingPick", "Assign##loadingPick")) && chosen) {
        SetMapViewCell(state, map.id, "Loading", chosen->stem,
                       std::string(L("Ladebild von ", "Loading image of ")) + map.mapName + " -> " + chosen->stem);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();
    ImGui::EndPopup();
}
} // namespace

bool SetMapViewLoading(EditorState& state, long long mapId, const std::string& image) {
    return SetMapViewCell(state, mapId, "Loading", image, std::string(L("Ladebild -> ", "Loading image -> ")) + image);
}

std::vector<std::string> LoadingImageFiles(EditorState& state, const std::string& stem) {
    const auto* image = FindImage(state, stem);
    return image ? image->rels : std::vector<std::string>{};
}

void DrawLoadingScreenEditor(EditorState& state) {
    if (ResmenuSourceRoot(state).empty()) {
        ImGui::TextDisabled("%s", L("Kein Client/resmenu gefunden.", "No Client/resmenu found."));
        return;
    }
    const int clientDoc = SharedShnDoc(state, "MapViewInfo.shn", EditorState::ShnSource::Client);
    if (clientDoc < 0) {
        ImGui::TextDisabled("%s", L("MapViewInfo.shn (Client) nicht gefunden - Client-Ordner im Projekt prüfen.",
                                    "MapViewInfo.shn (client) not found - check the client folder in the project."));
        return;
    }
    const int serverDoc = SharedShnDoc(state, "MapViewInfo.shn", EditorState::ShnSource::Server);
    const auto maps = CollectMaps(state, clientDoc);
    std::unordered_map<std::string, int> useCount;
    std::vector<std::string> missing;
    for (const auto& m : maps) {
        ++useCount[LowerAscii(m.loading)];
        if (!FindImage(state, m.loading)) missing.push_back(m.mapName + " (" + m.loading + ")");
    }

    DrawDataHistoryButtons(state, "loadingHistory");
    ImGui::SameLine();
    const std::size_t dirty = DirtyShnDocumentCount(state);
    ImGui::BeginDisabled(dirty == 0);
    if (UI::Button(L("Alle geänderten SHN speichern##loading", "Save all changed SHN##loading"))) SaveAllDirtyShnDocuments(state);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu %s · %zu %s", maps.size(), L("Karten", "maps"), useCount.size(), L("Ladebilder", "loading images"));
    if (serverDoc < 0)
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), "%s",
                           L("Server-MapViewInfo nicht gefunden: Zuweisungen gehen nur in den Client.",
                             "Server MapViewInfo not found: assignments only go to the client."));
    if (!missing.empty()) {
        std::string list;
        for (const auto& m : missing) list += (list.empty() ? "" : ", ") + m;
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s %s", L("Ohne Ladebild:", "Without loading image:"), list.c_str());
    }

    const float listW = std::clamp(ImGui::GetContentRegionAvail().x * 0.32f, 260.0f, 420.0f);
    ImGui::BeginChild("##loadingList", ImVec2(listW, 0.0f), true);
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputTextWithHint("##loadingFilter", L("Karte oder Bild filtern...", "Filter map or image..."), state.loadingFilter, sizeof(state.loadingFilter));
    const std::string needle = LowerAscii(state.loadingFilter);
    const MapEntry* selected = nullptr;
    for (const auto& m : maps) {
        if (m.id == state.loadingSelectedId) selected = &m;
        if (!needle.empty() && LowerAscii(m.mapName + " " + m.displayName + " " + m.loading).find(needle) == std::string::npos) continue;
        ImGui::PushID(static_cast<int>(m.row));
        const bool has = FindImage(state, m.loading) != nullptr;
        const int uses = useCount[LowerAscii(m.loading)];
        std::string label = m.mapName + (m.displayName.empty() ? "" : "  " + m.displayName);
        if (UI::Selectable((label + "##loadingMap").c_str(), m.id == state.loadingSelectedId, 0, ImVec2(std::max(60.0f, listW - 178.0f), 0.0f)))
            state.loadingSelectedId = m.id;
        if (state.loadingScrollToSelected && m.id == state.loadingSelectedId) {
            ImGui::SetScrollHereY(0.3f);
            state.loadingScrollToSelected = false;
        }
        ImGui::SameLine(listW - 160.0f);
        ImGui::TextColored(has ? ImVec4(0.55f, 0.75f, 0.9f, 1.0f) : ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s%s", m.loading.c_str(),
                           uses > 1 ? (" x" + std::to_string(uses)).c_str() : "");
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##loadingDetail", ImVec2(0.0f, 0.0f), true);
    if (!selected) {
        ImGui::TextDisabled("%s", L("Karte auswählen.", "Select a map."));
        ImGui::EndChild();
        return;
    }
    const MapEntry map = *selected; // Kopie: Schreiben kann die Liste ändern
    if (state.loadingAutomationAssign) {
        state.loadingAutomationAssign = false;
        if (const auto* chosen = FindImage(state, state.loadingPickChoice))
            SetMapViewCell(state, map.id, "Loading", chosen->stem,
                           std::string(L("Ladebild von ", "Loading image of ")) + map.mapName + " -> " + chosen->stem);
    }
    const auto* image = FindImage(state, map.loading);
    const int uses = useCount[LowerAscii(map.loading)];
    ImGui::Text("%s  #%lld", map.mapName.c_str(), map.id);
    if (!map.displayName.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", map.displayName.c_str());
    }
    ImGui::Text("%s %s", L("Ladebild (MapViewInfo.Loading):", "Loading image (MapViewInfo.Loading):"), map.loading.c_str());
    if (serverDoc >= 0) {
        const auto& sf = ShnDocFile(state, serverDoc);
        const int sr = RowById(sf, map.id);
        const std::string serverValue = sr >= 0 ? TrimAscii(ShnCellText(sf, static_cast<std::size_t>(sr), "Loading")) : std::string();
        if (sr < 0 || serverValue != map.loading) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "! %s %s", L("Server:", "Server:"),
                               sr < 0 ? L("keine Zeile", "no row") : serverValue.c_str());
        }
    }
    if (UI::Button(L("Anderes Bild zuweisen...", "Assign other image..."))) {
        state.loadingPickChoice = map.loading;
        state.loadingPickFilter[0] = '\0';
        ImGui::OpenPopup("##loadingPick");
    }
    DrawAssignPicker(state, map);
    ImGui::SameLine();
    ImGui::BeginDisabled(!image || state.project.projectFolder[0] == '\0');
    if (UI::Button(L("Bild ersetzen...##loading", "Replace image...##loading")) && image) {
        OpenImageReplaceDialog(state, image->rels.front(), image->stem);
        state.imageReplace.extraTargets.assign(image->rels.begin() + 1, image->rels.end());
    }
    ImGui::EndDisabled();
    if (image && uses > 1) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.3f, 1.0f), L("betrifft %d Karten", "affects %d maps"), uses);
    }

    // Eigenes Bild nur für diese Karte: neue Datei loading/<Name>.png (Format der bisherigen PNG bzw.
    // eines anderen Ladebilds), Loading zeigt danach darauf - beides ein Undo-Schritt.
    // Neu vorschlagen bei Kartenwechsel und wenn der eigene Vorschlag inzwischen vergeben ist.
    if (state.loadingNewName[0] == '\0' || state.loadingNewNameFor != map.id ||
        (state.loadingNewNameSuggested == state.loadingNewName && FindImage(state, state.loadingNewName))) {
        std::snprintf(state.loadingNewName, sizeof(state.loadingNewName), "%s", SuggestImageName(state, map.mapName).c_str());
        state.loadingNewNameFor = map.id;
        state.loadingNewNameSuggested = state.loadingNewName;
    }
    ImGui::SetNextItemWidth(200.0f);
    UI::InputText("##loadingNewName", state.loadingNewName, sizeof(state.loadingNewName));
    ImGui::SameLine();
    const std::string newName = TrimAscii(state.loadingNewName);
    const bool nameFree = ValidImageName(newName) && !FindImage(state, newName);
    ImGui::BeginDisabled(!nameFree || state.project.projectFolder[0] == '\0');
    const bool automationOwn = !state.loadingAutomationOwnSource.empty() && nameFree;
    if (UI::Button(L("Eigenes Bild für diese Karte...", "Own image for this map...")) || automationOwn) {
        std::string templateRel;
        if (image)
            for (const auto& rel : image->rels)
                if (LowerAscii(std::filesystem::path(rel).extension().string()) == ".png") templateRel = rel;
        if (templateRel.empty())
            for (const auto& [lower, other] : Catalog(state).byLowerStem)
                if (templateRel.empty() && lower != "nowloading")
                    for (const auto& rel : other.rels)
                        if (templateRel.empty() && LowerAscii(std::filesystem::path(rel).extension().string()) == ".png") templateRel = rel;
        OpenImageReplaceDialog(state, "loading/" + newName + ".png", newName, templateRel);
        const long long id = map.id;
        const std::string mapName = map.mapName;
        state.imageReplace.onApply = [id, newName, mapName](EditorState& s) {
            SetMapViewCell(s, id, "Loading", newName, std::string(L("Ladebild von ", "Loading image of ")) + mapName + " -> " + newName);
        };
        if (automationOwn)
            std::snprintf(state.imageReplace.sourcePath, sizeof(state.imageReplace.sourcePath), "%s",
                          state.loadingAutomationOwnSource.c_str());
        state.loadingAutomationOwnSource.clear();
    }
    ImGui::EndDisabled();
    if (!nameFree && !newName.empty())
        ImGui::TextDisabled("%s", ValidImageName(newName) ? L("Name ist schon vergeben.", "Name is already taken.")
                                                           : L("Nur Buchstaben, Ziffern, _ und -.", "Only letters, digits, _ and -."));

    std::string loadLocation;
    if (DeferredInputText("##loadLocation", map.loadLocation, loadLocation, 60.0f))
        SetMapViewCell(state, map.id, "LoadLocation", TrimAscii(loadLocation),
                       std::string("LoadLocation ") + map.mapName + " = " + TrimAscii(loadLocation));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", L("LoadLocation (0/1, Bedeutung nicht belegt; 29 Karten haben 1)",
                                "LoadLocation (0/1, meaning not proven; 29 maps use 1)"));
    ImGui::Separator();

    if (image) {
        for (const auto& rel : image->rels) {
            const bool projectCopy = CachedInterfaceOverrideState(state, rel) != InterfaceOverrideState::None ||
                                     IsRegularFileNoThrow(InterfaceProjectOverridePath(state, rel));
            ImGui::TextDisabled("%s · %s%s", rel.c_str(), FormatOf(state, rel).c_str(), projectCopy ? L(" · Projektkopie", " · project copy") : "");
        }
        if (uses > 1) {
            ImGui::TextUnformatted(L("Verwendet von:", "Used by:"));
            for (const auto& m : maps) {
                if (LowerAscii(m.loading) != LowerAscii(map.loading)) continue;
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(m.row));
                if (m.id == map.id) ImGui::TextDisabled("%s", m.mapName.c_str());
                else if (ImGui::SmallButton(m.mapName.c_str())) state.loadingSelectedId = m.id;
                ImGui::PopID();
            }
        }
        ImVec2 area = ImGui::GetContentRegionAvail();
        area.y = state.loadingSharedOpen ? 220.0f : std::max(160.0f, area.y - ImGui::GetFrameHeightWithSpacing() * 1.6f);
        DrawFittedPreview(state, image->rels.front(), area);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.4f, 1.0f), "%s loading/%s.png", L("Datei fehlt:", "File missing:"), map.loading.c_str());
    }
    if (state.loadingSharedOpenRequest) {
        ImGui::SetNextItemOpen(true);
        state.loadingSharedOpenRequest = false;
    }
    state.loadingSharedOpen = UI::CollapsingHeader(L("Gemeinsame Ladebild-Dateien", "Shared loading files"));
    if (state.loadingSharedOpen) DrawSharedFiles(state);
    ImGui::EndChild();
}

} // namespace nge
