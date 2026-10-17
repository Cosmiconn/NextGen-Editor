// Automation.cpp
// Automatisierung für Screenshots und visuelle Regression.

#include "EditorShared.hpp"

namespace nge {

GLFWwindow* g_automationWindow = nullptr;
 // nur für die Maus-Befehle der Automatisierung

AutomationScript LoadAutomationScriptFromEnvironment() {
    AutomationScript script;
    const char* env = std::getenv("NEXTGEN_EDITOR_SCRIPT");
    if (!env) return script;
    std::string text = env;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t sep = text.find(';', pos);
        std::string cmd = text.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
        pos = sep == std::string::npos ? text.size() + 1 : sep + 1;
        const auto first = cmd.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        cmd = cmd.substr(first, cmd.find_last_not_of(" \t\r\n") - first + 1);
        script.commands.push_back(cmd);
    }
    return script;
}
void RunAutomationStep(EditorState& state, AutomationScript& script) {
    if (script.waitFrames > 0) { --script.waitFrames; return; }
    if (script.commands.empty() || !script.pendingScreenshot.empty()) return;
    std::istringstream in(script.commands.front());
    script.commands.pop_front();
    std::string cmd;
    in >> cmd;
    auto onOff = [&](bool& target) { std::string v; in >> v; target = v != "off" && v != "0"; };
    if (cmd == "wait") { in >> script.waitFrames; }
    else if (cmd == "open") {
        std::string path; std::getline(in >> std::ws, path);
        if (OpenMapPathIntoState(state, std::filesystem::path(path), true)) state.screen = AppScreen::MapEditorWorkspace;
    }
    else if (cmd == "newmaploading") {
        // newmaploading <Pfad>: eigenes Ladebild für den nächsten "newmap" (leer = Kopie der Vorlage);
        // zeigt dazu den Assistenten "Neue Karte".
        std::string path; std::getline(in >> std::ws, path);
        std::snprintf(state.newMapLoadingSource, sizeof(state.newMapLoadingSource), "%s", path.c_str());
        state.screen = AppScreen::MapEditorLauncher;
        state.mapLauncherView = EditorState::MapLauncherView::NewMap;
    }
    else if (cmd == "newmap") {
        // newmap <Name> <Vorlage> <129|257|513> [Gate-Karte X Y]: Assistent "Neue Karte" ausführen.
        std::string name, templ, from; int size = 257; in >> name >> templ >> size;
        std::snprintf(state.newMapName, sizeof(state.newMapName), "%s", name.c_str());
        EnsureDiscoveredMaps(state);
        state.newMapTemplate = FindDiscoveredMapByName(state, templ);
        state.newMapSize = size <= 129 ? 0 : size >= 513 ? 2 : 1;
        state.newMapGate = static_cast<bool>(in >> from >> state.newMapGateX >> state.newMapGateY);
        std::snprintf(state.newMapGateFrom, sizeof(state.newMapGateFrom), "%s", from.c_str());
        const bool ok = CreateNewMapFromWizard(state);
        std::printf("[newmap] %s ok=%d\n", name.c_str(), ok ? 1 : 0);
        for (const auto& line : state.newMapReport) std::printf("[newmap] %s\n", line.c_str());
        std::fflush(stdout);
    }
    else if (cmd == "heightexport" || cmd == "heightimport") {
        // heightexport <Datei> | heightimport <Datei> <min> <max>
        std::string file; in >> file;
        if (cmd == "heightexport") {
            auto r = core::ExportHeightmapImage(state.heightmap, file);
            std::printf("[height] export %s %s\n", file.c_str(), r ? "ok" : r.error().c_str());
        } else {
            float lo = 0, hi = 1000; in >> lo >> hi;
            auto img = core::ImportHeightImage(file);
            if (img) {
                auto patch = core::ApplyHeightImage(state.heightmap, *img, lo, hi);
                std::printf("[height] import %s %ux%u changed %zu\n", file.c_str(), img->width, img->height, patch.entries.size());
                if (!patch.entries.empty()) { PushHeightmapHistory(state, std::move(patch)); state.meshDirty = true; state.mapDirty = true; }
            } else std::printf("[height] import %s %s\n", file.c_str(), img.error().c_str());
        }
        std::fflush(stdout);
    }
    else if (cmd == "walkgen") {
        // walkgen changes|all [Neigung] | apply | discard: Block&Walk neu berechnen (Vorschau/Übernehmen).
        std::string mode; in >> mode;
        if (mode == "apply") ApplyWalkProposal(state);
        else if (mode == "discard") state.walkProposal.reset();
        else {
            float slope = state.walkGenSettings.maxSlopeDegrees;
            if (in >> slope) state.walkGenSettings.maxSlopeDegrees = slope;
            state.walkGenScope = mode == "all" ? 1 : 0;
            ComputeWalkProposal(state);
        }
        std::printf("[walkgen] %s: blocked+%zu free+%zu region %zu vsOriginal %.4f | %s | %s\n", mode.c_str(),
                    state.walkProposalDiff.newlyBlocked, state.walkProposalDiff.newlyFree, state.walkProposalRegionCells,
                    state.walkProposalVsOriginal.Agreement(), state.walkGenStatus.c_str(), state.statusMessage.c_str());
        std::fflush(stdout);
    }
    else if (cmd == "spline") {
        // spline add <x> <z> | spline clear | spline flatten <breite> | spline paint <layer> | spline objects <modell> <abstand> <versatz>
        std::string what; in >> what;
        const auto samples = core::SampleSpline(state.splinePoints, 25.0f);
        std::size_t n = 0;
        if (what == "add") { core::SplinePoint p; in >> p.x >> p.z; state.splinePoints.push_back(p); }
        else if (what == "clear") state.splinePoints.clear();
        else if (what == "flatten") {
            in >> state.splineWidth.width;
            auto patch = core::FlattenAlongSpline(state.heightmap, samples, state.splineWidth, state.splineSmooth);
            n = patch.entries.size();
            if (n) { PushHeightmapHistory(state, std::move(patch)); state.meshDirty = true; state.mapDirty = true; }
        } else if (what == "paint") {
            int layer = 0; in >> layer;
            auto patch = core::PaintAlongSpline(state.textureStack, static_cast<std::size_t>(layer), state.heightmap, samples, state.splineWidth);
            n = patch.entries.size();
            if (n) { PushTexturePaintHistory(state, std::move(patch)); state.renderer.UpdateBlendTextures(state.textureStack); state.layerPreviewDirty = true; state.mapDirty = true; }
        } else if (what == "objects") {
            in >> state.splineObjects.model >> state.splineObjects.spacing >> state.splineObjects.sideOffset;
            auto objs = core::ObjectsAlongSpline(state.splinePoints, state.heightmap, state.splineObjects);
            n = objs.size();
            auto historyBefore = BeginObjectMutation(state);
            for (auto& o : objs) state.placementSet.AddObject(std::move(o));
            SyncObjectEditorMetadata(state);
            state.nifMeshRenderer.LoadModelsForSet(state.placementSet, CurrentObjectAssetMapDir(state));
            state.objectVisKey.clear();
            FinishObjectMutation(state, std::move(historyBefore), n > 0);
        }
        std::printf("[spline] %s points %zu changed %zu length %.0f\n", what.c_str(), state.splinePoints.size(), n,
                    samples.empty() ? 0.0f : samples.back().distance);
        std::fflush(stdout);
    }
    else if (cmd == "prefab") {
        // prefab save <Name> (aktuelle Auswahl) | prefab place <Name> <x> <z> [Grad]
        std::string what, name; in >> what >> name;
        const auto file = PrefabDir(state) / (name + ".prefab");
        if (what == "save") {
            std::vector<core::PlacedObject> objects;
            for (const int id : state.selectedObjects)
                if (id >= 0 && static_cast<std::size_t>(id) < state.placementSet.Count()) objects.push_back(state.placementSet.At(static_cast<std::size_t>(id)));
            auto r = core::SavePrefab(core::MakePrefab(name, objects, state.heightmap), file);
            std::printf("[prefab] save %s %zu %s\n", name.c_str(), objects.size(), r ? "ok" : r.error().c_str());
        } else if (what == "place") {
            float x = 0, z = 0; in >> x >> z;
            float yaw = 0; if (in >> yaw) state.newObjectRotDeg = yaw;
            state.prefabSelected = file.string();
            state.editMode = EditMode::ObjectPlacement;
            state.objectPlaceMode = 3;
            const bool ok = PlacePrefabAtWorld(state, x, z);
            std::printf("[prefab] place %s %d objects %zu | %s\n", name.c_str(), ok ? 1 : 0, state.placementSet.Count(), state.prefabStatus.c_str());
        }
        std::fflush(stdout);
    }
    else if (cmd == "scatter") {
        // scatter model <pfad> | scatter set <radius> <dichte> <abstand> <maxNeigung> | scatter erase on|off
        std::string what; in >> what;
        if (what == "model") { std::string m; in >> m; state.scatter.models.push_back(m); }
        else if (what == "set") in >> state.scatter.radius >> state.scatter.density >> state.scatter.minDistance >> state.scatter.maxSlope;
        else if (what == "erase") { std::string v; in >> v; state.scatterErase = v == "on"; }
        state.editMode = EditMode::ObjectPlacement;
        state.objectPlaceMode = 2;
        std::printf("[scatter] objects %zu\n", state.placementSet.Count());
        std::fflush(stdout);
    }
    else if (cmd == "texrule") {
        // texrule <minSlope> <maxSlope> [replace] : Regel auf den gewählten Layer (ganze Karte)
        std::string mode; in >> state.texRule.minSlope >> state.texRule.maxSlope >> mode;
        state.texRule.useSlope = true;
        state.texRule.mode = mode == "replace" ? core::TextureRuleMode::Replace : core::TextureRuleMode::Raise;
        std::size_t changed = 0;
        if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount() &&
            !TextureLayerLocked(state, state.selectedLayer)) {
            auto patch = core::ApplyTextureRule(state.textureStack, static_cast<std::size_t>(state.selectedLayer), state.heightmap, state.texRule, {});
            changed = patch.entries.size();
            if (changed) { PushTexturePaintHistory(state, std::move(patch)); state.renderer.UpdateBlendTextures(state.textureStack); state.layerPreviewDirty = true; state.mapDirty = true; }
        }
        std::printf("[texrule] layer %d changed %zu\n", state.selectedLayer, changed);
        std::fflush(stdout);
    }
    else if (cmd == "brushalpha") {
        std::string file; in >> file;
        const bool ok = file == "off" ? (state.brushAlphaEnabled = false, true) : LoadBrushAlpha(state, file);
        std::printf("[brushalpha] %s %d %s\n", file.c_str(), ok ? 1 : 0, state.brushAlphaStatus.c_str());
        std::fflush(stdout);
    }
    else if (cmd == "layerlock") {
        int l = 0; std::string v; in >> l >> v;
        if (l >= 0 && static_cast<std::size_t>(l) < state.textureStack.LayerCount()) {
            const auto& name = state.textureStack.Layer(static_cast<std::size_t>(l)).name;
            if (v == "off") state.lockedTextureLayers.erase(name); else state.lockedTextureLayers.insert(name);
        }
    }
    else if (cmd == "stroke") {
        // stroke <x> <z> [n]: wendet den aktiven Pinsel n-mal an einer Weltposition an (wie ein Klick).
        float x = 0, z = 0; int n = 1; in >> x >> z >> n;
        for (int i = 0; i < std::max(1, n); ++i) ApplyBrushAtWorld(state, x, z);
    }
    else if (cmd == "mode") {
        std::string m; in >> m;
        if (m == "terrain") state.editMode = EditMode::Heightmap;
        else if (m == "texture") state.editMode = EditMode::TexturePaint;
        else if (m == "walk") state.editMode = EditMode::BlockWalk;
        else if (m == "objects") state.editMode = EditMode::ObjectPlacement;
        else if (m == "npcs") state.editMode = EditMode::Npcs;
        else if (m == "portals") state.editMode = EditMode::Portals;
        else if (m == "mobs") state.editMode = EditMode::Mobs;
    }
    else if (cmd == "project") {
        // project <Client-Ordner>|<Server-Ordner>: setzt die Projekt-Quellordner (read-only Quellen).
        std::string rest; std::getline(in >> std::ws, rest);
        const auto bar = rest.find('|');
        const std::string client = rest.substr(0, bar);
        const std::string server = bar == std::string::npos ? std::string() : rest.substr(bar + 1);
        std::snprintf(state.project.clientFolder, sizeof(state.project.clientFolder), "%s", client.c_str());
        std::snprintf(state.project.serverFolder, sizeof(state.project.serverFolder), "%s", server.c_str());
    }
    else if (cmd == "shn") {
        // shn <Unterreiter> <Server-Shine-Ordner>: öffnet den SHN-Editor (4 = Quest-Editor).
        int tab = 0; in >> tab;
        std::string root; std::getline(in >> std::ws, root);
        if (!root.empty()) state.shnServerRoot = root;
        state.shnSubTab = tab;
        state.screen = AppScreen::ShnEditor;
    }
    else if (cmd == "quest") {
        int id = 0; in >> id;
        EnsureQuestDataLoaded(state);
        for (std::size_t i = 0; i < state.questDataFile.records.size(); ++i)
            if (state.questDataFile.records[i].id == id) state.selectedQuestIdx = static_cast<int>(i);
        std::string section; in >> section;
        state.automationQuestSection = section == "drops" ? 1 : section == "rewards" ? 2 : section == "npcpick" ? 3 : section == "objectives" ? 4 : section == "dialogs" ? 5 : 0;
        state.questShowFlow = section == "flow";
        if (section == "npcpick") { // quest <id> npcpick [Filter]: öffnet die Start-NPC-Auswahl
            std::string filter; std::getline(in >> std::ws, filter);
            std::snprintf(state.questNpcFilter, sizeof(state.questNpcFilter), "%s", filter.c_str());
        }
    }
    else if (cmd == "dropgroup") {
        // dropgroup <Gruppe>: öffnet den Drop-Editor im Reiter Drop-Gruppen mit dieser Gruppe.
        std::string group; in >> group;
        state.shnSubTab = 10;
        state.screen = AppScreen::ShnEditor;
        state.dropEditorTabRequest = 1;
        state.dropGroupSelected = group;
    }
    else if (cmd == "questtext") {
        // questtext <Text-ID> <Text...>: setzt einen Quest-Text (wie das Eingabefeld, mit Undo).
        std::uint32_t id = 0; in >> id;
        std::string text; std::getline(in >> std::ws, text);
        EnsureQuestDialogLoaded(state);
        std::printf("[questtext] %u %s\n", id, SetQuestText(state, id, text) ? "ok" : "FEHLER");
        std::fflush(stdout);
    }
    else if (cmd == "interface") {
        // interface <resmenu-Pfad>: öffnet den Interface-Bereich mit dieser Datei.
        std::string rel; std::getline(in >> std::ws, rel);
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = 9;
        state.interfacePendingAsset = rel;
    }
    else if (cmd == "loadingscreen") {
        // loadingscreen <MapName> [shared]: öffnet den Ladebildschirm-Editor mit dieser Karte
        // (shared: gemeinsame Dateien aufgeklappt).
        std::string name; in >> name;
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = 9;
        state.interfaceMode = 1;
        if (const int doc = SharedShnDoc(state, "MapViewInfo.shn", EditorState::ShnSource::Client); doc >= 0) {
            const auto& f = ShnDocFile(state, doc);
            for (std::size_t r = 0; r < f.rows.size(); ++r)
                if (LowerAscii(TrimAscii(ShnCellText(f, r, "MapName"))) == LowerAscii(name))
                    state.loadingSelectedId = std::atoll(TrimAscii(ShnCellText(f, r, "ID")).c_str());
        }
        state.loadingScrollToSelected = true;
        std::string extra; in >> extra;
        state.loadingSharedOpenRequest = extra == "shared";
    }
    else if (cmd == "loadingassign") {
        // loadingassign <Bildname>: weist der gewählten Karte ein vorhandenes Ladebild zu (wie "Zuweisen").
        std::string image; in >> image;
        state.loadingPickChoice = image;
        state.loadingAutomationAssign = true;
    }
    else if (cmd == "iconeditor") {
        // iconeditor <IconFile> [Index]: öffnet den Icon-Atlas-Editor mit dieser Zelle.
        std::string atlas; int index = -1; in >> atlas >> index;
        OpenIconInAtlasEditor(state, atlas, index);
    }
    else if (cmd == "iconreplace") {
        // iconreplace <Quelldatei>: "Zelle ersetzen" für die gewählte Zelle mit dieser Quelle.
        std::string source; std::getline(in >> std::ws, source);
        state.iconAutomationReplaceSource = source;
    }
    else if (cmd == "uilayout") {
        // uilayout <resmenu-NIF> [Element]: öffnet die Fenster-Ansicht mit diesem NIF (und Element).
        std::string rel; int part = -1; in >> rel >> part;
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = 9;
        state.interfaceMode = 4;
        state.layoutSelected = rel;
        state.layoutPart = part;
        state.layoutScrollToSelected = true;
    }
    else if (cmd == "uilayoutreplace") {
        // uilayoutreplace <Quelldatei>: "Ausschnitt ersetzen" für das gewählte Element mit dieser Quelle.
        std::string source; std::getline(in >> std::ws, source);
        state.layoutAutomationReplaceSource = source;
    }
    else if (cmd == "portrait") {
        // portrait <mob|npc> <Name>: öffnet den Porträt-Bereich mit diesem Porträt.
        std::string kind, name; in >> kind >> name;
        OpenPortraitEditor(state, kind == "npc" ? 1 : 0, name);
    }
    else if (cmd == "loadingown") {
        // loadingown <Quelldatei>: "Eigenes Bild für diese Karte" mit dieser Quelle (Dialog öffnet sich).
        std::string source; std::getline(in >> std::ws, source);
        state.loadingAutomationOwnSource = source;
    }
    else if (cmd == "imagereplace") {
        // imagereplace <resmenu-Pfad>|<Quelldatei>[|<Anpassung 0-2>]: öffnet den Ersetzen-Dialog.
        std::string rest; std::getline(in >> std::ws, rest);
        std::vector<std::string> parts;
        for (std::size_t start = 0;;) {
            const auto bar = rest.find('|', start);
            parts.push_back(rest.substr(start, bar - start));
            if (bar == std::string::npos) break;
            start = bar + 1;
        }
        OpenImageReplaceDialog(state, parts[0], parts[0]);
        if (parts.size() > 1) std::snprintf(state.imageReplace.sourcePath, sizeof(state.imageReplace.sourcePath), "%s", parts[1].c_str());
        if (parts.size() > 2) state.imageReplace.fit = std::clamp(std::atoi(parts[2].c_str()), 0, 2);
    }
    else if (cmd == "imageapply") {
        // imageapply: übernimmt das Ergebnis des offenen Ersetzen-Dialogs (wie der Knopf).
        auto& r = state.imageReplace;
        const bool ok = ApplyImageReplacement(state);
        std::printf("[imageapply] %s %s\n", r.targetRel.c_str(), ok ? "ok" : "FEHLER");
        std::fflush(stdout);
    }
    else if (cmd == "undo") DataUndo(state);
    else if (cmd == "itemeditor" || cmd == "mobeditor" || cmd == "passiveeditor" || cmd == "stateeditor") {
        // itemeditor/mobeditor/passiveeditor/stateeditor <ID>: öffnet den Formular-Editor mit diesem Eintrag.
        long long id = -1; in >> id;
        state.screen = AppScreen::ShnEditor;
        state.shnSubTab = cmd == "itemeditor" ? 11 : cmd == "mobeditor" ? 12 : cmd == "passiveeditor" ? 13 : 14;
        (cmd == "itemeditor" ? state.itemForm : cmd == "mobeditor" ? state.mobForm : cmd == "passiveeditor" ? state.passiveForm : state.stateForm).selectedId = id;
    }
    else if (cmd == "autosave") {
        // autosave now | wait | restore | discard | minutes <n>: automatisches Speichern testen.
        std::string mode; in >> mode;
        if (mode == "now") StartMapAutosave(state);
        else if (mode == "wait") FinishMapAutosave(state, true);
        else if (mode == "restore") RestoreMapAutosave(state);
        else if (mode == "discard") { core::RemoveAutosave(state.project.projectFolder, state.legacySaveStem); state.autosaveRecoveryPrompt = false; }
        else if (mode == "minutes") { int m = 5; in >> m; state.autosaveMinutes = std::clamp(m, 1, 60); }
        std::printf("[autosave] %s prompt=%d dirty=%d status=%s\n", mode.c_str(), state.autosaveRecoveryPrompt ? 1 : 0,
                    state.mapDirty ? 1 : 0, state.autosaveStatus.c_str());
        std::fflush(stdout);
    }
    else if (cmd == "mapcheck") {
        // mapcheck [print]: Kartenprüfung ausführen und Fenster öffnen; "print" schreibt alle Meldungen.
        std::string mode; in >> mode;
        RunMapCheck(state);
        state.mapCheckOpen = true;
        std::printf("[mapcheck] %s: %s\n", state.mapCheckForMap.c_str(), state.mapCheckSummary.c_str());
        if (mode == "print")
            for (const auto& i : state.mapCheckIssues)
                std::printf("[mapcheck] %d %s | %s | %s\n", static_cast<int>(i.severity), MapCheckCategoryLabel(i.code),
                            i.subject.c_str(), MapCheckMessage(i).c_str());
        std::fflush(stdout);
    }
    else if (cmd == "projectfolder") {
        // projectfolder <Ordner>: beschreibbarer Projektordner (Ausgabe für alle Speicherpfade).
        std::string folder; std::getline(in >> std::ws, folder);
        std::snprintf(state.project.projectFolder, sizeof(state.project.projectFolder), "%s", folder.c_str());
        state.project.hasProject = true;
    }
    else if (cmd == "shnset") {
        // shnset <Client|Server> <Datei.shn> <Zeile> <Spalte> <Wert>: setzt eine Zelle wie das Grid.
        std::string side, file, value; int row = -1, column = -1;
        in >> side >> file >> row >> column; std::getline(in >> std::ws, value);
        const auto src = side == "Client" ? EditorState::ShnSource::Client : EditorState::ShnSource::Server;
        for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
            const auto& doc = state.shnFiles[static_cast<std::size_t>(i)];
            if (doc.source == src && doc.file.FileName() == file) { ApplyShnCellText(state, i, row, column, value); break; }
        }
    }
    else if (cmd == "saveall") {
        // saveall [all]: speichert alle geänderten (mit "all": alle geladenen) Module über die
        // normalen Speicherfunktionen - für den End-to-End-Test der Projektausgabe.
        std::string mode; in >> mode;
        const bool everything = mode == "all";
        std::string report;
        std::size_t shnSaved = 0, shnFailed = 0;
        for (int i = 0; i < static_cast<int>(state.shnFiles.size()); ++i) {
            if (!everything && !state.shnFiles[static_cast<std::size_t>(i)].dirty) continue;
            if (SaveShnDocument(state, i)) ++shnSaved; else { ++shnFailed; report += " SHN-Fehler: " + state.shnStatus; }
        }
        report += "SHN " + std::to_string(shnSaved) + " gespeichert, " + std::to_string(shnFailed) + " Fehler";
        if (!everything && state.questDirty)
            report += SaveQuestDataProject(state) ? " · Quest ok" : " · Quest FEHLER " + state.statusMessage;
        if (everything) {
            EnsureQuestDataLoaded(state);
            if (state.questDataLoaded) report += SaveQuestDataProject(state) ? " · Quest ok" : " · Quest FEHLER " + state.statusMessage;
            EnsureDropTableLoaded(state);
            if (state.dropTableLoaded) report += SaveDropTable(state) ? " · Drops ok" : " · Drops FEHLER " + state.statusMessage;
            EnsureTownPortalLoaded(state);
            if (state.townPortalLoaded) { SaveTownPortalFiles(state); report += " · " + state.statusMessage; }
            EnsureRecallCoordLoaded(state);
            if (state.recallCoordLoaded) report += SaveRecallCoordFile(state) ? " · Recall ok" : " · Recall FEHLER " + state.statusMessage;
            EnsureNpcTextLoaded(state);
            if (state.npcTextLoaded) {
                auto saved = SaveProjectShineOverride(state.npcTextFile,
                    ProjectServerShinePath(state.project, std::filesystem::path("World") / "NPC.txt"));
                report += saved ? " · NPC ok" : " · NPC FEHLER " + saved.error();
            }
            if (state.legacySaveStem[0] != '\0') {
                EnsureMobRegenLoaded(state);
                if (state.mobRegenTextLoaded) {
                    auto saved = SaveProjectShineOverride(state.mobRegenTextFile,
                        ProjectServerShinePath(state.project, std::filesystem::path("MobRegen") / (CurrentMapServerName(state) + ".txt")));
                    report += saved ? " · MobRegen ok" : " · MobRegen FEHLER " + saved.error();
                }
                report += SaveLegacyMapProject(state) ? " · Karte ok" : " · Karte FEHLER " + state.statusMessage;
            }
        }
        std::printf("[saveall] %s\n", report.c_str());
        std::fflush(stdout);
    }
    else if (cmd == "skill") {
        // skill <ID> [picker]: Skill im Skill-Editor wählen, optional den Icon-Atlas öffnen.
        long long id = 0; std::string opt; in >> id >> opt;
        state.skill.selectedId = id;
        state.skillIconPickerOpen = opt == "picker";
    }
    else if (cmd == "shop") {
        // shop <NPC-InxName>: öffnet den Händler-Inventar-Editor (Karte im NPC-Modus nötig).
        std::string npc; in >> npc;
        EnsureShopTextLoaded(state, npc);
        state.shopEditorOpen = true;
    }
    else if (cmd == "mouse") {
        // mouse <x> <y>: Mauszeiger im Fenster setzen (Fensterkoordinaten).
        double x = 0, y = 0; in >> x >> y;
        if (g_automationWindow) glfwSetCursorPos(g_automationWindow, x, y);
        ImGui::GetIO().AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
    }
    else if (cmd == "lmb") {
        std::string v; in >> v;
        ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, v == "down");
    }
    else if (cmd == "probe") {
        // probe <x> <z>: Terrainhöhe, Gewicht des aktiven Layers und Walk-Bit an einer Weltposition.
        float x = 0, z = 0;
        std::string first; in >> first;
        if (first == "cursor" && state.viewportCursorWorld) { x = (*state.viewportCursorWorld)[0]; z = (*state.viewportCursorWorld)[2]; }
        else { x = std::strtof(first.c_str(), nullptr); in >> z; }
        float weight = -1.0f;
        if (state.selectedLayer >= 0 && static_cast<std::size_t>(state.selectedLayer) < state.textureStack.LayerCount()) {
            const auto& blend = state.textureStack.Layer(static_cast<std::size_t>(state.selectedLayer)).blend;
            const float sx = static_cast<float>(state.heightmap.Width() - 1) * state.heightmap.BlockWidth();
            const float sz = static_cast<float>(state.heightmap.Height() - 1) * state.heightmap.BlockHeight();
            const auto bx = static_cast<std::uint32_t>(std::clamp(x / sx * blend.Width(), 0.0f, blend.Width() - 1.0f));
            const auto bz = static_cast<std::uint32_t>(std::clamp(z / sz * blend.Height(), 0.0f, blend.Height() - 1.0f));
            weight = blend.At(bx, bz);
        }
        std::printf("[probe] %.0f %.0f height %.2f layer %d weight %.3f blocked %d dirty %d cursor %s\n", x, z,
                    state.heightmap.SampleWorld(x, z), state.selectedLayer, weight,
                    core::level::WalkBlockedAtWorld(state.walkGrid, x, z) ? 1 : 0, state.mapDirty ? 1 : 0,
                    state.viewportCursorWorld ? (std::to_string(static_cast<int>((*state.viewportCursorWorld)[0])) + "/" +
                                                 std::to_string(static_cast<int>((*state.viewportCursorWorld)[2]))).c_str() : "-");
        std::fflush(stdout);
    }
    else if (cmd == "brush") {
        // brush <raise|lower|smooth|flatten> <radius> <strength>
        std::string m; float r = 200, st = 10; in >> m >> r >> st;
        if (m == "raise") state.brushMode = core::BrushMode::Raise;
        else if (m == "lower") state.brushMode = core::BrushMode::Lower;
        else if (m == "smooth") state.brushMode = core::BrushMode::Smooth;
        else if (m == "flatten") state.brushMode = core::BrushMode::Flatten;
        else if (m == "noise") state.brushMode = core::BrushMode::Noise;
        else if (m == "terrace") state.brushMode = core::BrushMode::Terrace;
        else if (m == "erode") state.brushMode = core::BrushMode::Erode;
        else if (m == "sharpen") state.brushMode = core::BrushMode::Sharpen;
        else if (m == "ramp") { state.brushMode = core::BrushMode::Ramp; state.rampStart.reset(); }
        state.brush.radius = r; state.brush.strength = st;
        state.paintSettings.radius = r; state.walkSettings.radius = r;
    }
    else if (cmd == "layer") { int l = 0; in >> l; state.selectedLayer = l; }
    else if (cmd == "nifpreview") {
        // nifpreview <out.ppm> <nif> [<nif> ...]: gerenderte Vorschauen nebeneinander als PPM.
        std::string out; in >> out;
        std::vector<std::string> nifs; std::string n;
        while (in >> n) nifs.push_back(n);
        const int size = 192;
        std::vector<std::uint8_t> sheet(static_cast<std::size_t>(size) * size * 3 * nifs.size(), 40);
        for (std::size_t k = 0; k < nifs.size(); ++k) {
            std::vector<std::uint8_t> px;
            const auto tex = RenderNifPreviewThumbnail(nifs[k], size, &px);
            std::printf("[nifpreview] %s -> %s\n", nifs[k].c_str(), tex ? "ok" : "kein Modell");
            for (int y = 0; y < size && !px.empty(); ++y)
                for (int x = 0; x < size; ++x)
                    for (int c = 0; c < 3; ++c)
                        sheet[(static_cast<std::size_t>(y) * size * nifs.size() + k * size + x) * 3 + c] =
                            px[(static_cast<std::size_t>(y) * size + x) * 4 + c];
        }
        std::ofstream f(out, std::ios::binary);
        f << "P6\n" << size * nifs.size() << " " << size << "\n255\n";
        f.write(reinterpret_cast<const char*>(sheet.data()), static_cast<std::streamsize>(sheet.size()));
        std::fflush(stdout);
    }
    else if (cmd == "footprintstats") {
        // Vergleicht jede Objekt-Grundfläche mit dem geladenen (Original-)SHBD: Anteil gesperrter
        // Zellen innerhalb der Grundfläche, je Modell aggregiert.
        struct Agg { std::size_t objects = 0, cells = 0, blocked = 0; bool fallback = false; };
        std::map<std::string, Agg> byModel;
        std::size_t totalCells = 0, totalBlocked = 0, noPolygon = 0;
        const std::uint32_t cols = state.walkGrid.Cols(), rows = state.walkGrid.Rows();
        std::vector<char> covered(static_cast<std::size_t>(cols) * rows, 0);
        core::WalkGrid scratch(state.walkGrid.Width(), state.walkGrid.Height(), 0);
        RefreshObjectVisibility(state);
        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
            const auto& obj = state.placementSet.At(i);
            const auto polygon = ObjectFootprintWorldPolygon(state, obj);
            if (polygon.size() < 3) { ++noPolygon; continue; }
            core::WalkUndoPatch patch; std::vector<std::uint64_t> seen;
            core::ApplyWalkConvexPolygon(scratch, polygon, true, patch, seen);
            float minX = 1e30f, minZ = 1e30f, maxX = -1e30f, maxZ = -1e30f;
            for (const auto& [px, pz] : polygon) { minX = std::min(minX, px); maxX = std::max(maxX, px); minZ = std::min(minZ, pz); maxZ = std::max(maxZ, pz); }
            const auto cx0 = static_cast<std::uint32_t>(std::clamp(minX / core::WalkGrid::kCellSize, 0.0f, cols - 1.0f));
            const auto cx1 = static_cast<std::uint32_t>(std::clamp(maxX / core::WalkGrid::kCellSize, 0.0f, cols - 1.0f));
            const auto cz0 = static_cast<std::uint32_t>(std::clamp(minZ / core::WalkGrid::kCellSize, 0.0f, rows - 1.0f));
            const auto cz1 = static_cast<std::uint32_t>(std::clamp(maxZ / core::WalkGrid::kCellSize, 0.0f, rows - 1.0f));
            auto& agg = byModel[obj.modelPath];
            ++agg.objects;
            agg.fallback = agg.fallback || !state.nifMeshRenderer.HasRealMesh(i);
            for (std::uint32_t cz = cz0; cz <= cz1; ++cz)
                for (std::uint32_t cx = cx0; cx <= cx1; ++cx) {
                    if (!scratch.CellBlocked(cx, cz)) continue;
                    ++agg.cells; ++totalCells;
                    covered[static_cast<std::size_t>(cz) * cols + cx] = 1;
                    if (state.walkGrid.CellBlocked(cx, cz)) { ++agg.blocked; ++totalBlocked; }
                }
            core::RevertWalkPatch(scratch, patch);
        }
        std::size_t gridBlocked = 0, gridBlockedCovered = 0;
        for (std::uint32_t cz = 0; cz < rows; ++cz)
            for (std::uint32_t cx = 0; cx < cols; ++cx)
                if (state.walkGrid.CellBlocked(cx, cz)) {
                    ++gridBlocked;
                    if (covered[static_cast<std::size_t>(cz) * cols + cx]) ++gridBlockedCovered;
                }
        std::printf("[footprint] objects %zu, without polygon %zu, cells in footprints %zu, of which blocked %zu (%.1f%%); "
                    "blocked cells in SHBD %zu, covered by footprints %zu (%.1f%%)\n",
                    state.placementSet.Count(), noPolygon, totalCells, totalBlocked,
                    totalCells ? 100.0 * totalBlocked / totalCells : 0.0, gridBlocked, gridBlockedCovered,
                    gridBlocked ? 100.0 * gridBlockedCovered / gridBlocked : 0.0);
        {
            std::size_t keptCells = 0, keptBlocked = 0, keptModels = 0, droppedModels = 0;
            for (const auto& [m, a] : byModel) {
                if (ModelWalkableInSource(state, m)) { ++droppedModels; continue; }
                ++keptModels; keptCells += a.cells; keptBlocked += a.blocked;
            }
            std::printf("[footprint] mit Filter: %zu Modelle gesperrt, %zu ausgenommen; %zu Zellen, davon im Original gesperrt %.1f%%\n",
                        keptModels, droppedModels, keptCells, keptCells ? 100.0 * keptBlocked / keptCells : 0.0);
        }
        std::vector<std::pair<std::string, Agg>> sorted(byModel.begin(), byModel.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.cells > b.second.cells; });
        for (std::size_t k = 0; k < sorted.size() && k < 25; ++k)
            std::printf("[footprint] %-48s n=%3zu cells=%6zu blocked=%5.1f%%%s\n", sorted[k].first.c_str(), sorted[k].second.objects,
                        sorted[k].second.cells, sorted[k].second.cells ? 100.0 * sorted[k].second.blocked / sorted[k].second.cells : 0.0,
                        sorted[k].second.fallback ? " (ohne NIF)" : "");
        std::fflush(stdout);
    }
    else if (cmd == "selectall") SelectAllNormalObjects(state);
    else if (cmd == "select") {
        // select <id> [id ...]
        std::vector<int> ids;
        for (int id = 0; in >> id;) ids.push_back(id);
        if (ids.empty()) ids.push_back(-1);
        state.selectedObjects = ids; state.selectedObject = ids.front(); state.objectGizmoMatrixValid = false;
    }
    else if (cmd == "selectmodel") {
        // selectmodel <Teil des Modellpfads> [n]: das n-te passende Objekt auswählen (ohne n alle).
        std::string needle; int nth = -1; in >> needle >> nth;
        for (char& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::vector<int> ids;
        for (std::size_t i = 0; i < state.placementSet.Count(); ++i) {
            std::string m = state.placementSet.At(i).modelPath;
            for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (m.find(needle) != std::string::npos) ids.push_back(static_cast<int>(i));
        }
        if (nth >= 0) ids = nth < static_cast<int>(ids.size()) ? std::vector<int>{ids[static_cast<std::size_t>(nth)]} : std::vector<int>{};
        state.selectedObjects = ids;
        state.selectedObject = ids.empty() ? kNoObjectSelection : ids.front();
        state.objectGizmoMatrixValid = false;
        std::printf("[automation] selectmodel %s -> %zu\n", needle.c_str(), ids.size());
        std::fflush(stdout);
    }
    else if (cmd == "focus") FocusCurrentSceneSelection(state);
    else if (cmd == "hide") HideSelectedObjects(state);
    else if (cmd == "isolate") HideUnselectedObjects(state);
    else if (cmd == "showall") ShowAllObjects(state);
    else if (cmd == "playtest") TogglePlaytest(state);
    else if (cmd == "preset") { int p = 0; in >> p; ApplyViewportPreset(state, p); }
    else if (cmd == "workspace") { int p = 0; in >> p; RequestMapWorkspacePreset(state, p); }
    else if (cmd == "view2d") {
        // view2d <zoom> <weltX> <weltZ>: 2D-Ansicht zoomen und auf eine Weltposition zentrieren.
        float zoom = 1.0f, x = 0.0f, z = 0.0f; in >> zoom >> x >> z;
        const float spanX = static_cast<float>(std::max<std::uint32_t>(state.heightmap.Width(), 2) - 1) * state.heightmap.BlockWidth();
        const float spanZ = static_cast<float>(std::max<std::uint32_t>(state.heightmap.Height(), 2) - 1) * state.heightmap.BlockHeight();
        state.view2dZoom = zoom;
        state.view2dCenterU = x / spanX;
        state.view2dCenterV = 1.0f - z / spanZ;
    }
    else if (cmd == "camera") {
        float tx = 0, ty = 0, tz = 0, yaw = 0, pitch = 0, dist = 1000;
        in >> tx >> ty >> tz >> yaw >> pitch >> dist;
        state.camera.SetTarget(tx, ty, tz); state.camera.SetDistance(dist); state.camera.SetOrientation(yaw, pitch);
    }
    else if (cmd == "layout") { int l = 1; in >> l; SetViewportLayout(state, l); }
    else if (cmd == "activeviewport") { int k = 0; in >> k; SetActiveViewport(state, k); }
    else if (cmd == "grid") onOff(state.showViewportGrid);
    else if (cmd == "stats") onOff(state.showViewportStats);
    else if (cmd == "collision") onOff(state.showWalkCollision3D);
    else if (cmd == "gameview") onOff(state.gameView);
    else if (cmd == "surfacesnap") onOff(state.surfaceSnap);
    else if (cmd == "glow") onOff(state.viewportGlow);
    else if (cmd == "objectanim") onOff(state.viewportObjectAnimation);
    else if (cmd == "capture") {
        // capture <datei.ppm> <größe|BxH> <x> <y(server)> <yaw> <pitch> <abstand> [höhe über Boden]:
        // Offscreen-Aufnahme wie die Portalbilder (mit Himmel und Nebel), für Vergleiche mit Client-Bildern.
        std::string file, sizeText; float x = 0, y = 0, yaw = 0, pitch = 0.3f, dist = 950, lift = 90;
        in >> file >> sizeText >> x >> y >> yaw >> pitch >> dist;
        in >> lift;
        std::uint32_t width = 256, height = 256;
        if (const auto cross = sizeText.find('x'); cross != std::string::npos) {
            width = static_cast<std::uint32_t>(std::max(1, std::atoi(sizeText.substr(0, cross).c_str())));
            height = static_cast<std::uint32_t>(std::max(1, std::atoi(sizeText.substr(cross + 1).c_str())));
        } else if (!sizeText.empty()) {
            width = height = static_cast<std::uint32_t>(std::max(1, std::atoi(sizeText.c_str())));
        }
        const float ground = state.heightmap.Width() > 1 ? state.heightmap.SampleWorld(x, y) : 0.0f;
        app::OrbitCamera cam;
        cam.SetTarget(x, ground + lift, y);
        cam.SetDistance(dist);
        cam.SetOrientation(yaw, pitch);
        const auto rgba = CaptureSceneRgba(state, cam, width, height, true, true, true);
        std::ofstream out(file, std::ios::binary);
        out << "P6\n" << width << " " << height << "\n255\n";
        for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) out.write(reinterpret_cast<const char*>(&rgba[i]), 3);
        std::printf("[capture] %s %zu\n", file.c_str(), rgba.size());
        std::fflush(stdout);
    }
    else if (cmd == "townportal") {
        // townportal add | images [yaw]: Ziel für die offene Karte anlegen bzw. Bilder des ersten Ziels erzeugen.
        std::string sub; in >> sub;
        EnsureTownPortalLoaded(state);
        if (sub == "add") {
            AddTownPortalForCurrentMap(state);
            std::printf("[townportal] add: %s\n", state.statusMessage.c_str());
        } else if (sub == "images") {
            float yaw = state.townPortalImageYaw;
            if (in >> yaw) state.townPortalImageYaw = yaw;
            bool done = false;
            for (const auto& m : CollectPortalMarkers(state)) {
                if (m.kind != kPortalKindTown) continue;
                std::vector<std::string> report;
                const bool ok = GenerateTownPortalImages(state, TownPortalMapName(state, m.idx), m.x, m.y, report);
                std::printf("[townportal] images ok=%d at %.0f/%.0f\n", ok ? 1 : 0, m.x, m.y);
                for (const auto& line : report) std::printf("[townportal] %s\n", line.c_str());
                done = true;
                break;
            }
            if (!done) std::printf("[townportal] kein TownPortal-Ziel auf dieser Karte\n");
        }
        std::fflush(stdout);
    }
    else if (cmd == "minimap") {
        // minimap generate | game on|off | style on|off | objects on|off | export <datei.ppm>
        std::string sub; in >> sub;
        if (sub == "generate") {
            std::vector<std::string> report;
            const bool ok = GenerateMinimap(state, report);
            std::printf("[minimap] generate ok=%d\n", ok ? 1 : 0);
            for (const auto& line : report) std::printf("[minimap] %s\n", line.c_str());
        } else if (sub == "game") onOff(state.minimapShowGame);
        else if (sub == "style") onOff(state.minimapStyled);
        else if (sub == "objects") onOff(state.minimapWithObjects);
        else if (sub == "export") {
            // Rohe Draufsicht (ohne Stil) als PPM, für den Vergleich mit den Originalen.
            std::string file; in >> file;
            const auto rgba = RenderMinimapRgba(state, 512, state.minimapWithObjects);
            std::ofstream out(file, std::ios::binary);
            out << "P6\n512 512\n255\n";
            for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) out.write(reinterpret_cast<const char*>(&rgba[i]), 3);
            std::printf("[minimap] export %s %zu\n", file.c_str(), rgba.size());
        }
        std::fflush(stdout);
    }
    else if (cmd == "culling") {
        bool on = true; onOff(on);
        state.nifMeshRenderer.SetFrustumCullingEnabled(on);
        state.shmdCategoryMeshRenderer.SetFrustumCullingEnabled(on);
    }
    else if (cmd == "renderstats") {
        const auto& a = state.nifMeshRenderer.LastFrameStats();
        std::printf("[renderstats] drawn=%zu culled=%zu cpu=%.2fms\n", a.drawnParts, a.culledParts, a.cpuMs);
        std::fflush(stdout);
    }
    else if (cmd == "animtime") {
        // animtime <Sekunden>|off: Animationszeit für reproduzierbare Bilder festhalten.
        std::string v; in >> v;
        const std::optional<float> t = v == "off" ? std::nullopt : std::optional<float>(std::strtof(v.c_str(), nullptr));
        state.nifMeshRenderer.SetAnimationTimeOverrideForTesting(t);
        state.shmdCategoryMeshRenderer.SetAnimationTimeOverrideForTesting(t);
    }
    else if (cmd == "vertexcolors") onOff(state.viewportVertexColors);
    else if (cmd == "maplighting") onOff(state.viewportMapLighting);
    else if (cmd == "mapbackground") onOff(state.viewportMapBackground);
    else if (cmd == "fog") onOff(state.viewportFog);
    else if (cmd == "viewmode") { in >> state.viewportViewMode; state.viewportViewMode = std::clamp(state.viewportViewMode, 0, 7); }
    else if (cmd == "msaa") { in >> state.viewportMsaaSamples; state.viewportMsaaSamples = std::clamp(state.viewportMsaaSamples, 1, 8); }
    else if (cmd == "status") { std::string t; std::getline(in >> std::ws, t); state.statusMessage = t; }
    else if (cmd == "marquee") {
        // marquee <x0> <y0> <x1> <y1> [inside|crossing] - Anteile 0..1 des 3D-Bilds
        float x0 = 0, y0 = 0, x1 = 1, y1 = 1; std::string mode;
        in >> x0 >> y0 >> x1 >> y1 >> mode;
        if (mode == "inside") state.marqueeMode = core::level::MarqueeMode::Inside;
        else if (mode == "crossing") state.marqueeMode = core::level::MarqueeMode::Crossing;
        const ImVec2 p = state.viewport3dPos, sz = state.viewport3dSize;
        const auto rect = core::level::ScreenRect::FromCorners({p.x + x0 * sz.x, p.y + y0 * sz.y},
                                                               {p.x + x1 * sz.x, p.y + y1 * sz.y});
        state.selectedObjects = CollectMarqueeHits(state, p, static_cast<int>(sz.x), static_cast<int>(sz.y), rect);
        state.selectedObject = state.selectedObjects.empty() ? kNoObjectSelection : state.selectedObjects.back();
        state.objectGizmoMatrixValid = false;
        state.statusMessage = "marquee " + std::to_string(state.selectedObjects.size());
        std::printf("[automation] marquee %s -> %zu\n", mode.c_str(), state.selectedObjects.size());
    }
    else if (cmd == "snapprobe") {
        // snapprobe <fx> <fy> - Vertex-Snap-Ziel an einer Bildposition (Anteile 0..1) ausgeben
        float fx = 0.5f, fy = 0.5f;
        in >> fx >> fy;
        const ImVec2 p = state.viewport3dPos, sz = state.viewport3dSize;
        const ImVec2 mouse(p.x + fx * sz.x, p.y + fy * sz.y);
        const auto t = VertexSnapTarget(state, p, static_cast<int>(sz.x), static_cast<int>(sz.y), mouse);
        const auto g = PickViewportGround(state, p, static_cast<int>(sz.x), static_cast<int>(sz.y), mouse);
        if (t) std::printf("[automation] snap %.3f %.3f %.3f", t->x, t->y, t->z);
        else std::printf("[automation] snap none");
        if (g) std::printf(" | ground %.1f %.1f %.1f", g->x, g->y, g->z);
        std::printf("\n");
    }
    else if (cmd == "screenshot") { in >> script.pendingScreenshot; }
    else if (cmd == "quit") script.quitRequested = true;
}
// Schreibt den aktuellen Backbuffer als binäres PPM (P6), oben = erste Zeile.
void WriteFramebufferPpm(const std::string& file, int w, int h) {
    if (w <= 0 || h <= 0) return;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << "P6\n" << w << " " << h << "\n255\n";
    const std::size_t row = static_cast<std::size_t>(w) * 3u;
    for (int y = h - 1; y >= 0; --y)
        out.write(reinterpret_cast<const char*>(pixels.data() + static_cast<std::size_t>(y) * row),
                  static_cast<std::streamsize>(row));
}

} // namespace nge
