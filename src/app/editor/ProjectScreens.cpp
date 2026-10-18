// ProjectScreens.cpp
// Projekt-Hub, neues Projekt, Karten-Startseite.

#include "EditorShared.hpp"

namespace nge {

void DrawProjectHub(EditorState& state) {
    DrawTopNav(state, L("Übersicht","Overview"));

    // Zwei Bereiche: links alles, was auf einer Karte liegt, rechts alle Spieldaten-Dateien
    // (SHN, Shine-Text, Interface, Animationen). Schnellknöpfe führen direkt in Unterbereiche.
    struct QuickLink {
        const char* label;   // eindeutige ImGui-ID über ##, Tooltip in ManualData
        int shnSubTab;       // >= 0: Unterreiter des Datei Editors
        AppScreen screen;
        EditorState::MapLauncherView launcherView;
    };
    struct HubCard {
        const char* id;
        const char* title;
        const char* semanticIcon;
        IconDrawFn icon;
        const char* summary;
        std::vector<const char*> covers;
        std::vector<QuickLink> links;
        AppScreen screen;
        int shnSubTab;
    };
    using LV = EditorState::MapLauncherView;
    const HubCard cards[] = {
        {"hub.map", L("Map Editor","Map Editor"), "nav.world", DrawIconTerrain,
         L("Alles, was auf einer Karte liegt: Gelände, Begehbarkeit und alles, was darauf platziert ist. "
           "Jede Karte wird in einer 2D-Draufsicht bearbeitet und in 3D geprüft.",
           "Everything that lives on a map: terrain, walkability and everything placed on it. "
           "Every map is edited in a 2D top view and checked in 3D."),
         {L("Karten anlegen (Assistent) und öffnen","Create maps (wizard) and open them"),
          L("Terrain formen und texturieren, Foliage","Sculpt and texture terrain, foliage"),
          L("Objekte, Prefabs, Sky/Water/GroundObject","Objects, prefabs, Sky/Water/GroundObject"),
          L("Block & Walk, Begehbarkeit neu berechnen","Block & Walk, recompute walkability"),
          L("NPCs, Mob-Spawns, Routen und Portale","NPCs, mob spawns, routes and portals"),
          L("Minimap und Kartenprüfung","Minimap and map check")},
         {{L("Neue Karte##hubNewMap","New map##hubNewMap"), -1, AppScreen::MapEditorLauncher, LV::NewMap},
          {L("Karte öffnen##hubOpenMap","Open map##hubOpenMap"), -1, AppScreen::MapEditorLauncher, LV::Browse}},
         AppScreen::MapEditorLauncher, -1},
        {"hub.data", L("Datei Editor","File Editor"), "module.shn.single", DrawIconTable,
         L("Alle Spieldaten-Dateien aus Client (ressystem, resmenu) und Server (9Data), unabhängig von einer Karte. "
           "Client und Server werden gemeinsam bearbeitet, Strg+Z nimmt Änderungen zurück.",
           "All game data files from client (ressystem, resmenu) and server (9Data), independent of a map. "
           "Client and server are edited together, Ctrl+Z reverts changes."),
         {L("SHN-Tabellen einzeln und mehrere gleichzeitig","SHN tables one at a time or several at once"),
          L("Items, Mobs, Skills, Passive, Zustände","Items, mobs, skills, passives, states"),
          L("Item-Systeme: Sets, Aufwertung, Mix, Zufallsoptionen, Pakete","Item systems: sets, upgrade, mix, random options, packages"),
          L("Reittiere, Haustiere, MiniHaus, Sammelkarten, Gilde, Kingdom Quests, Glücksspiel","Mounts, pets, mini house, collect cards, guild, kingdom quests, gambling"),
          L("Quests, Drops, Preise, XP, KI-Skripte, Angriffsfolgen der Mobs","Quests, drops, prices, XP, AI scripts, mob attack sequences"),
          L("Interface: Ladebilder, Icons, Porträts, Fenster","Interface: loading screens, icons, portraits, windows"),
          L("Texte und Lokalisierung (CSV für Übersetzungen)","Texts and localization (CSV for translations)"),
          L("Animationen (KFM)","Animations (KFM)")},
         {{L("Tabellen##hubShn","Tables##hubShn"), 0, AppScreen::ShnEditor, LV::Browse},
          {L("Items##hubItems","Items##hubItems"), 11, AppScreen::ShnEditor, LV::Browse},
          {L("Mobs##hubMobs","Mobs##hubMobs"), 12, AppScreen::ShnEditor, LV::Browse},
          {L("Skills##hubSkills","Skills##hubSkills"), 7, AppScreen::ShnEditor, LV::Browse},
          {L("Quests##hubQuests","Quests##hubQuests"), 4, AppScreen::ShnEditor, LV::Browse},
          {L("Drops##hubDrops","Drops##hubDrops"), 10, AppScreen::ShnEditor, LV::Browse},
          {L("Interface##hubInterface","Interface##hubInterface"), 9, AppScreen::ShnEditor, LV::Browse},
          {L("Texte##hubTexts","Texts##hubTexts"), 15, AppScreen::ShnEditor, LV::Browse},
          {L("Item-Systeme##hubItemSystems","Item systems##hubItemSystems"), 16, AppScreen::ShnEditor, LV::Browse},
          {L("Angriffsfolgen##hubAttackSeq","Attack sequences##hubAttackSeq"), 17, AppScreen::ShnEditor, LV::Browse},
          {L("Reittiere & Pets##hubMounts","Mounts & pets##hubMounts"), 18, AppScreen::ShnEditor, LV::Browse},
          {L("MiniHaus & Karten##hubHousing","Mini house & cards##hubHousing"), 19, AppScreen::ShnEditor, LV::Browse},
          {L("Gilde, KQ & Spiel##hubGuild","Guild, KQ & games##hubGuild"), 20, AppScreen::ShnEditor, LV::Browse},
          {L("Animationen##hubKfm","Animations##hubKfm"), -1, AppScreen::KfmBrowser, LV::Browse}},
         AppScreen::ShnEditor, 0},
    };

    const float gap = 12.0f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float cardW = std::max(300.0f, (avail.x - gap) * 0.5f);
    const float cardH = std::clamp(avail.y - 150.0f, 330.0f, 470.0f);
    const auto open = [&state](AppScreen screen, int subTab) {
        if (subTab >= 0) state.shnSubTab = subTab;
        state.screen = screen;
    };

    for (int i = 0; i < static_cast<int>(std::size(cards)); ++i) {
        const auto& card = cards[i];
        if (i > 0) ImGui::SameLine(0.0f, gap);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(8, 24, 36, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::BeginChild(card.id, ImVec2(cardW, cardH), true);

        constexpr float iconSize = 64.0f;
        const ImVec2 iconPos = ImGui::GetCursorScreenPos();
        if (const std::uint32_t tex = gUiIcons.Texture(card.semanticIcon, 64); tex != 0) {
            ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(iconSize, iconSize),
                         ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImGui::Dummy(ImVec2(iconSize, iconSize));
            card.icon(ImGui::GetWindowDrawList(), ImVec2(iconPos.x + iconSize * 0.5f, iconPos.y + iconSize * 0.5f),
                      iconSize * 0.4f, IM_COL32(255, 255, 255, 235));
        }
        ImGui::SameLine(0.0f, 14.0f);
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextColored(UiTheme::AccentCyan, "%s", card.title);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::TextWrapped("%s", card.summary);
        ImGui::Spacing();
        for (const char* line : card.covers) {
            ImGui::Bullet();
            ImGui::TextWrapped("%s", line);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("%s", L("Direkt zu:", "Go directly to:"));
        const float rowRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        for (std::size_t l = 0; l < card.links.size(); ++l) {
            const auto& link = card.links[l];
            const float w = ImGui::CalcTextSize(link.label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            if (l > 0) {
                ImGui::SameLine();
                if (ImGui::GetCursorScreenPos().x + w > rowRight) ImGui::NewLine();
            }
            if (UI::Button(link.label)) {
                if (link.screen == AppScreen::MapEditorLauncher) state.mapLauncherView = link.launcherView;
                open(link.screen, link.shnSubTab);
            }
        }

        // Start-Knopf unten in der Karte.
        const float buttonH = 36.0f;
        const float bottom = ImGui::GetWindowContentRegionMax().y - buttonH;
        if (ImGui::GetCursorPosY() < bottom) ImGui::SetCursorPosY(bottom);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(55, 125, 195, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 145, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 95, 155, 255));
        if (UI::Button(T("card.start"), ImVec2(-1.0f, buttonH))) open(card.screen, card.shnSubTab);
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    DrawInlineIcon("recentProjectsHeader", DrawIconTable, IM_COL32(100,205,255,245), nullptr,
                   ImVec2(18.0f,18.0f), "system.recent_projects");
    ImGui::SameLine(0.0f, 5.0f);
    ImGui::TextColored(UiTheme::AccentCyan, "%s", L("ZULETZT VERWENDETE PROJEKTE","RECENT PROJECTS"));
    ImGui::Separator();
    if (state.recentProjects.empty()) {
        ImGui::TextDisabled("%s",L("Noch keine Projekte geöffnet.","No projects opened yet."));
    } else {
        const float recentW = std::max(220.0f, (ImGui::GetContentRegionAvail().x - 16.0f) / 3.0f);
        for (std::size_t i = 0; i < state.recentProjects.size(); ++i) {
            const auto& path = state.recentProjects[i];
            std::error_code ec;
            const bool exists = std::filesystem::is_regular_file(std::filesystem::path(path) / "project.tsproj", ec);
            const std::string leaf = std::filesystem::path(path).filename().string();
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginDisabled(!exists);
            if (UI::Button((leaf.empty() ? path : leaf).c_str(), ImVec2(recentW, 34.0f)))
                LoadProjectFolderIntoState(state, path);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s%s", path.c_str(),
                                  exists ? "" : L("\n(nicht mehr gefunden)","\n(no longer found)"));
            ImGui::PopID();
            if (i % 3 != 2 && i + 1 < state.recentProjects.size()) ImGui::SameLine();
        }
    }

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}
// (Hinweis: die gelben Klebezettel aus den Mockups waren Umsetzungs-Hinweise, keine
// echten UI-Elemente - daher gibt es hier bewusst keine "Sticky Note"-Komponente mehr.)

void DrawNewProjectConfig(EditorState& state) {
    DrawTopNav(state, L("Projekt","Project"));

    DrawPanelHeader("projectConfigHeader", L("PROJEKT KONFIGURIEREN","CONFIGURE PROJECT"),
                    DrawIconPencilPaper, "panel.project",
                    L("Client, Server und Arbeitsordner einmal zentral festlegen",
                      "Configure client, server and workspace folders in one place"));
    ImGui::Dummy(ImVec2(0,4));

    const float cardW = std::min(820.0f, std::max(560.0f, ImGui::GetContentRegionAvail().x * 0.68f));
    ImGui::BeginChild("##projectConfigCard", ImVec2(cardW, 0), true);

    ImGui::TextDisabled("%s",L("PROJEKTNAME","PROJECT NAME"));
    ImGui::SetNextItemWidth(-1.0f);
    UI::InputText("##projname", state.project.name, sizeof(state.project.name));
    ImGui::TextDisabled("%s",L("Interner Name des NextGen-Editor-Projekts.","Internal name of the NextGen Editor project."));

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("%s",L("PROJEKTORDNER","PROJECT FOLDER"));
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##projfolder", state.project.projectFolder, sizeof(state.project.projectFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##pf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.projectfolder")))
            std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("%s",L("Hier liegen Projektkonfiguration und später die bearbeiteten Ausgabedateien.",
                                    "Project configuration and edited output files are stored here."));

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("CLIENT");
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##clientfolder", state.project.clientFolder, sizeof(state.project.clientFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##cf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.clientfolder")))
            std::snprintf(state.project.clientFolder, sizeof(state.project.clientFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("%s",L("Quelle für resmap, ressystem, reschar, resitem und Client-SHN.",
                                    "Source for resmap, ressystem, reschar, resitem and client SHN."));

    ImGui::Dummy(ImVec2(0,6));
    ImGui::TextDisabled("SERVER");
    ImGui::SetNextItemWidth(-44.0f);
    UI::InputText("##serverfolder", state.project.serverFolder, sizeof(state.project.serverFolder));
#ifdef _WIN32
    ImGui::SameLine();
    if (UI::Button("...##sf", ImVec2(36,0))) {
        if (auto picked = BrowseForFolderWindows(T("newproject.serverfolder")))
            std::snprintf(state.project.serverFolder, sizeof(state.project.serverFolder), "%s", picked->c_str());
    }
#endif
    ImGui::TextDisabled("%s",L("Quelle für 9Data/Shine, World, MobRegen, Quest- und Serverdaten.",
                                    "Source for 9Data/Shine, World, MobRegen, quest and server data."));

    ImGui::Separator();
    const float actionW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    if (UI::Button(T("newproject.createsave"), ImVec2(actionW, 38.0f))) {
        if (state.project.name[0] == '\0') {
            state.statusMessage = T("newproject.namemissing");
        } else if (state.project.projectFolder[0] == '\0') {
            state.statusMessage = T("newproject.folderMissing");
        } else {
            std::string err;
            if (SaveProjectConfig(state.project, &err)) {
                state.project.hasProject = true;
                TouchRecentProject(state, state.project.projectFolder);
                state.statusMessage = T("newproject.saved");
                state.screen = AppScreen::ProjectHub;
            } else {
                state.statusMessage = std::string(T("newproject.savefailed")) + err;
            }
        }
    }
    ImGui::SameLine();
    if (UI::Button(T("nav.back"), ImVec2(actionW, 38.0f)))
        state.screen = AppScreen::ProjectHub;

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }

    ImGui::EndChild();
}
void DrawMapEditorLauncher(EditorState& state) {
    DrawTopNav(state, L("Karte","Map"));

    DrawPanelHeader("mapLauncherHeader", L("KARTEN","MAPS"), DrawIconGlobe, "nav.world",
                    L("Neue Karte anlegen oder vorhandene Fiesta-Karte öffnen",
                      "Create a new map or open an existing Fiesta map"));

    const bool onNewMap = state.mapLauncherView == EditorState::MapLauncherView::NewMap;
    const bool onBrowse = state.mapLauncherView == EditorState::MapLauncherView::Browse;
    if (DrawIconButton("launcher.new", L("Neue Karte","New map"), DrawIconTerrain, onNewMap, ImVec2(108,58), true, "file.new"))
        state.mapLauncherView = EditorState::MapLauncherView::NewMap;
    ImGui::SameLine();
    if (DrawIconButton("launcher.open", L("Karte öffnen","Open map"), DrawIconGlobe, onBrowse, ImVec2(108,58), true, "file.open"))
        state.mapLauncherView = EditorState::MapLauncherView::Browse;
    ImGui::SameLine();
    if (DrawIconButton("launcher.project", L("Projekt","Project"), DrawIconPencilPaper, false, ImVec2(92,58), true, "panel.project"))
        state.screen = AppScreen::ProjectHub;
    ImGui::Separator();

    // Kartenwurzel nur neu scannen, wenn sich der Client-Ordner geändert hat oder der Nutzer
    // explizit einen Rescan anstößt.
    if (state.project.clientFolder[0] != '\0') {
        const std::string clientFolderStr = state.project.clientFolder;
        if (clientFolderStr != state.lastScannedMapRoot) {
            const auto resolution = ResolveMapSearchRootAndScan(state.project.clientFolder);
            state.discoveredMaps = resolution.maps;
            MergeProjectOnlyMaps(state.project, state.discoveredMaps);
            state.lastResmapCandidateCount = resolution.candidateCount;
            state.lastResmapFound = resolution.root.has_value();
            state.lastResmapResolvedPath = resolution.root
                ? (resolution.candidateCount > 1
                       ? (resolution.root->string() + "  (" + std::to_string(resolution.candidateCount) +
                          L(" resmap-Ordner gefunden; dieser enthält die meisten Karten)",
                            " resmap folders found; this one contains the most maps)"))
                       : resolution.root->string())
                : (std::string(L("Kein resmap-Ordner unter ","No resmap folder found under ")) +
                   clientFolderStr + L(" gefunden",""));
            state.lastScannedMapRoot = clientFolderStr;
            state.selectedMapIndex = -1;
            if (resolution.root) StartNifThumbnailPrecache(state, *resolution.root);
        }
    }

    const std::string& resolvedRoot = state.lastResmapResolvedPath;
    const bool resmapFound = state.lastResmapFound;

    if (state.nifPrecacheActive) {
        AdvanceNifPrecache(state, 8);
        const std::size_t totalQ = state.nifPrecacheQueue.size();
        const float frac = totalQ == 0 ? 1.0f
            : static_cast<float>(state.nifPrecacheCursor) / static_cast<float>(totalQ);
        ImGui::BeginChild("##assetPreparation", ImVec2(std::min(680.0f, ImGui::GetContentRegionAvail().x), 150.0f), true);
        DrawPanelHeader("assetPreparationHeader", L("ASSET-BIBLIOTHEK","ASSET LIBRARY"),
                        DrawIconCube, "panel.asset_browser");
        ImGui::TextWrapped("%s",L(
            "NIF-Vorschaubilder werden einmalig vorbereitet. Danach öffnet sich der Asset Browser ohne Erstlade-Ruckler.",
            "NIF thumbnails are prepared once. Afterwards the Asset Browser opens without the initial loading hitch."));
        ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f));
        ImGui::TextDisabled(L("%zu / %zu Modelle","%zu / %zu models"), state.nifPrecacheCursor, totalQ);
        ImGui::EndChild();
        return;
    }

    const float panelW = std::min(760.0f, std::max(520.0f, ImGui::GetContentRegionAvail().x * 0.66f));

    if (onNewMap) {
        ImGui::BeginChild("##createNewMap", ImVec2(panelW, 0), true);
        DrawPanelHeader("newMapHeader", L("NEUE KARTE","NEW MAP"), DrawIconTerrain, "file.new",
                        L("Grunddaten festlegen","Configure base data"));

        ImGui::TextDisabled("%s",L("KARTENNAME (Ordner, Dateien, Server)","MAP NAME (folder, files, server)"));
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputText("##newmapname", state.newMapName, sizeof(state.newMapName));
        ImGui::TextDisabled("%s",L("ANZEIGENAME","DISPLAY NAME"));
        ImGui::SetNextItemWidth(-1.0f);
        UI::InputText("##newmapdisplay", state.newMapDisplayName, sizeof(state.newMapDisplayName));

        ImGui::Dummy(ImVec2(0,4));
        ImGui::TextDisabled("%s",L("VORLAGE (Texturen, Licht, Himmel)","TEMPLATE (textures, light, sky)"));
        if (state.newMapTemplate < 0 || static_cast<std::size_t>(state.newMapTemplate) >= state.discoveredMaps.size()) {
            state.newMapTemplate = -1;
            for (std::size_t i = 0; i < state.discoveredMaps.size(); ++i)
                if (!state.discoveredMaps[i].standaloneNif && (state.newMapTemplate < 0 || state.discoveredMaps[i].name == "Rou"))
                    state.newMapTemplate = static_cast<int>(i);
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##newmaptemplate", state.newMapTemplate >= 0 ? state.discoveredMaps[static_cast<std::size_t>(state.newMapTemplate)].name.c_str()
                                                                         : L("(keine Karte gefunden)","(no map found)"))) {
            for (std::size_t i = 0; i < state.discoveredMaps.size(); ++i) {
                if (state.discoveredMaps[i].standaloneNif) continue;
                if (ImGui::Selectable(state.discoveredMaps[i].name.c_str(), state.newMapTemplate == static_cast<int>(i)))
                    state.newMapTemplate = static_cast<int>(i);
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s",L("GRÖSSE","SIZE"));
        ImGui::RadioButton("129 x 129##nms", &state.newMapSize, 0); ImGui::SameLine();
        ImGui::RadioButton("257 x 257##nms", &state.newMapSize, 1); ImGui::SameLine();
        ImGui::RadioButton("513 x 513##nms", &state.newMapSize, 2);
        ImGui::TextDisabled(L("Höhenpunkte; 257 = 12800 x 12800 Einheiten wie Roumen.","Height points; 257 = 12800 x 12800 units like Roumen."));
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputFloat(L("Grundhöhe##newmapbase","Base height##newmapbase"), &state.newMapBaseHeight, 0.0f, 0.0f, "%.0f");

        ImGui::Dummy(ImVec2(0,4));
        ImGui::TextDisabled("%s",L("SPIELDATEN","GAME DATA"));
        UI::Checkbox(L("MapInfo + MapViewInfo (Client und Server)##nm","MapInfo + MapViewInfo (client and server)##nm"), &state.newMapWriteMapInfo);
        if (state.newMapWriteMapInfo) {
            ImGui::Indent();
            ImGui::TextDisabled("%s", L("Ladebild (leer = Kopie des Vorlagen-Ladebilds)", "Loading image (empty = copy of the template's image)"));
            ImGui::SetNextItemWidth(-130.0f);
            UI::InputText("##newmaploading", state.newMapLoadingSource, sizeof(state.newMapLoadingSource));
#ifdef _WIN32
            ImGui::SameLine();
            if (UI::Button(L("Durchsuchen...##newmaploading", "Browse...##newmaploading"))) {
                if (auto picked = BrowseForInterfaceAssetWindows(L("Ladebild wählen", "Choose loading image"), ""))
                    std::snprintf(state.newMapLoadingSource, sizeof(state.newMapLoadingSource), "%s", picked->c_str());
            }
#endif
            // Größe/Format der Quelle (nur bei geändertem Pfad neu gelesen).
            static std::string inspectedPath, inspectedInfo;
            static bool inspectedOk = false;
            const std::string sourcePath = TrimAscii(state.newMapLoadingSource);
            if (sourcePath != inspectedPath) {
                inspectedPath = sourcePath;
                const auto format = sourcePath.empty() ? std::expected<core::ImageFormat, std::string>(std::unexpected(std::string()))
                                                       : core::InspectImageFile(sourcePath);
                inspectedOk = format.has_value();
                inspectedInfo = format ? core::DescribeImageFormat(*format) : format.error();
            }
            if (!sourcePath.empty()) {
                if (inspectedOk)
                    ImGui::TextDisabled("%s %s", inspectedInfo.c_str(), L("- wird auf Größe und Format des Vorlagen-Ladebilds umgerechnet",
                                                                          "- converted to the size and format of the template's image"));
                else
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s", inspectedInfo.c_str());
            }
            ImGui::Unindent();
        }
        UI::Checkbox(L("World/Field.txt (Kartenliste des Servers)##nm","World/Field.txt (server map list)##nm"), &state.newMapWriteField);
        UI::Checkbox(L("Gate von einer vorhandenen Karte (mit Rückweg)##nm","Gate from an existing map (with way back)##nm"), &state.newMapGate);
        if (state.newMapGate) {
            ImGui::SetNextItemWidth(140.0f);
            UI::InputText(L("Karte##nmgate","Map##nmgate"), state.newMapGateFrom, sizeof(state.newMapGateFrom));
            ImGui::SetNextItemWidth(90.0f);
            UI::InputInt("X##nmgate", &state.newMapGateX, 0, 0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            UI::InputInt("Y##nmgate", &state.newMapGateY, 0, 0);
            ImGui::TextDisabled("%s",L("Servername der Ausgangskarte und Position des Gates dort (z.B. Rou 5311 4428).",
                                       "Server name of the source map and gate position there (e.g. Rou 5311 4428)."));
        }

        ImGui::Separator();
        ImGui::BeginDisabled(state.newMapName[0] == '\0' || state.project.projectFolder[0] == '\0');
        if (UI::Button(L("Karte erstellen und ins Projekt schreiben","Create map and write to project"), ImVec2(-1.0f, 40.0f)))
            CreateNewMapFromWizard(state);
        ImGui::EndDisabled();
        if (state.project.projectFolder[0] == '\0')
            ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1.0f), "%s", L("Erst einen Projektordner festlegen.","Set a project folder first."));
        for (const auto& line : state.newMapReport)
            ImGui::TextColored(line.rfind("- ",0)==0 ? ImVec4(1.0f,0.5f,0.4f,1.0f) : ImVec4(0.75f,0.85f,0.95f,1.0f), "%s", line.c_str());
        ImGui::EndChild();
    }

    if (onBrowse) {
        ImGui::BeginChild("##browseMaps", ImVec2(panelW, 0), true);
        const std::string browseMapMeta = std::to_string(state.discoveredMaps.size()) +
                                          L(" gefunden"," found");
        DrawPanelHeader("browseMapsHeader", L("KARTE ÖFFNEN","OPEN MAP"), DrawIconGlobe,
                        "file.open", browseMapMeta.c_str());

        if (state.project.clientFolder[0] == '\0') {
            ImGui::TextWrapped("%s", T("mapeditor.noclientfolder"));
            if (UI::Button(L("Projekt konfigurieren","Configure project"), ImVec2(-1,0)))
                state.screen = AppScreen::NewProjectConfig;
        } else {
            if (!state.recentMaps.empty()) {
                DrawInlineIcon("recentMapsHeader", DrawIconGlobe, IM_COL32(100,205,255,245), nullptr,
                               ImVec2(18.0f,18.0f), "file.open");
                ImGui::SameLine(0.0f, 5.0f);
                ImGui::TextColored(UiTheme::AccentCyan, "%s", L("ZULETZT GEÖFFNET","RECENTLY OPENED"));
                const std::size_t showCount = std::min<std::size_t>(5, state.recentMaps.size());
                for (std::size_t i = 0; i < showCount; ++i) {
                    const auto& recent = state.recentMaps[i];
                    std::error_code ec;
                    const bool exists = std::filesystem::is_regular_file(recent, ec);
                    const std::string label = std::filesystem::path(recent).stem().string();
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::BeginDisabled(!exists);
                    if (UI::SmallButton(label.empty() ? recent.c_str() : label.c_str())) {
                        if (OpenMapPathIntoState(state, recent, true))
                            state.screen = AppScreen::MapEditorWorkspace;
                    }
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s%s", recent.c_str(),
                                          exists ? "" : L("\n(nicht mehr gefunden)","\n(no longer found)"));
                    ImGui::PopID();
                    if (i + 1 < showCount) ImGui::SameLine();
                }
                ImGui::Separator();
            }

            ImGui::TextDisabled("%s",L("SUCHWURZEL","SEARCH ROOT"));
            ImGui::TextWrapped("%s", resolvedRoot.empty() ? L("(noch nicht gescannt)","(not scanned yet)") : resolvedRoot.c_str());
            if (UI::SmallButton(L("Neu durchsuchen","Rescan"))) state.lastScannedMapRoot.clear();

            ImGui::Separator();
            ImGui::BeginChild("##mapList", ImVec2(0.0f, std::max(230.0f, ImGui::GetContentRegionAvail().y - 84.0f)), true);
            if (state.discoveredMaps.empty()) {
                ImGui::TextDisabled("%s", resmapFound
                    ? L("Keine .ini- oder eigenständigen NIF-Karten im gefundenen resmap-Ordner.",
                        "No .ini or standalone NIF maps in the discovered resmap folder.")
                    : L("resmap nicht gefunden - Client-Pfad im Projekt prüfen.",
                        "resmap not found - check the client path in the project."));
            }
            for (int i = 0; i < static_cast<int>(state.discoveredMaps.size()); ++i) {
                const bool selected = state.selectedMapIndex == i;
                const auto& map = state.discoveredMaps[static_cast<std::size_t>(i)];
                const std::string mapLabel = map.name +
                    (map.projectOnly ? L("  [Projekt]","  [Project]") : "") +
                    (map.standaloneNif ? L("  [NIF-only]##map","  [NIF-only]##map") : "##map");
                if (UI::Selectable(mapLabel.c_str(), selected)) {
                    state.selectedMapIndex = i;
                    std::snprintf(state.legacyMapIniPath, sizeof(state.legacyMapIniPath), "%s", map.iniPath.c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s%s", map.iniPath.c_str(),
                    map.standaloneNif ? L("\nEigenständige NIF-Karte ohne HTD/HTDG",
                                          "\nStandalone NIF map without HTD/HTDG") : "");
            }
            ImGui::EndChild();

            ImGui::BeginDisabled(state.selectedMapIndex < 0);
            if (UI::Button(L("Ausgewählte Karte öffnen","Open selected map"), ImVec2(-1.0f, 40.0f))) {
                if (OpenMapPathIntoState(state, state.legacyMapIniPath, true))
                    state.screen = AppScreen::MapEditorWorkspace;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
    }

    if (!state.statusMessage.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", state.statusMessage.c_str());
    }
}

} // namespace nge
