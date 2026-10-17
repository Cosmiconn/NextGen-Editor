// NpcEditor.cpp
// NPC-Auswahl, KI-Skripte, Patrouillen, NPC-Modelle, Dialog- und Shop-Editor.

#include "EditorShared.hpp"

namespace nge {

bool FocusCurrentSceneSelection(EditorState& state) {
    if (state.editMode == EditMode::ObjectPlacement) {
        if (state.selectedObjects.empty()) return false;
        FocusSelectedObjects(state);
        return true;
    }

    if (state.editMode == EditMode::Npcs) {
        EnsureNpcTextLoaded(state);
        auto* table = state.npcTextFile.FindTable("ShineNPC");
        if (!table || state.selectedNpcRecordIdx < 0) return false;
        const auto idx = static_cast<std::size_t>(state.selectedNpcRecordIdx);
        if (idx >= table->records.size() || table->records[idx].values.size() < 4) return false;
        const float x = static_cast<float>(std::atof(table->records[idx].values[2].c_str()));
        const float z = static_cast<float>(std::atof(table->records[idx].values[3].c_str()));
        state.camera.SetTarget(x, state.heightmap.SampleWorld(x,z) + 25.0f, z);
        state.camera.Zoom(220.0f - state.camera.Distance());
        return true;
    }

    if (state.editMode == EditMode::Mobs) {
        EnsureMobRegenLoaded(state);
        auto* zones = state.mobRegenTextFile.FindTable("MobRegenGroup");
        if (!zones || state.selectedMobZoneIdx < 0) return false;
        const auto idx = static_cast<std::size_t>(state.selectedMobZoneIdx);
        if (idx >= zones->records.size() || zones->records[idx].values.size() < 4) return false;
        const auto& rec = zones->records[idx];
        const float x = static_cast<float>(std::atof(rec.values[2].c_str()));
        const float z = static_cast<float>(std::atof(rec.values[3].c_str()));
        float radius = 90.0f;
        if (rec.values.size() >= 7) {
            const float zw = std::abs(static_cast<float>(std::atof(rec.values[4].c_str())));
            const float zh = std::abs(static_cast<float>(std::atof(rec.values[5].c_str())));
            const float rangeVal = std::abs(static_cast<float>(std::atof(rec.values[6].c_str())));
            radius = std::max(radius, std::max({zw, zh, rangeVal}));
        }
        state.camera.SetTarget(x, state.heightmap.SampleWorld(x,z) + 20.0f, z);
        const float desired = std::clamp(radius * 2.8f, 260.0f, 6000.0f);
        state.camera.Zoom(desired - state.camera.Distance());
        return true;
    }

    if (state.editMode == EditMode::Portals) {
        EnsurePortalDataLoaded(state);
        for (const auto& marker : CollectPortalMarkers(state)) {
            if (marker.kind != state.selectedPortalKind || static_cast<int>(marker.idx) != state.selectedPortalIdx)
                continue;
            state.camera.SetTarget(marker.x, state.heightmap.SampleWorld(marker.x,marker.y) + 25.0f, marker.y);
            state.camera.Zoom(260.0f - state.camera.Distance());
            return true;
        }
    }

    return false;
}
void EnsureNpcDialogLoaded(EditorState& state) {
    if (state.npcDialogLoaded) return;
    if (state.npcDialogRessystemRoot.empty() && state.shnClientRoot.empty()) return;
    // Gemeinsames Dokument statt privater Kopie: der Kreaturen-Assistent legt seine Dialogzeile im
    // selben Dokument an, das der Dialog-Editor und der SHN-Editor bearbeiten.
    state.npcDialogDoc = SharedShnDoc(state, "NpcDialogData.shn", EditorState::ShnSource::Client,
                                      state.npcDialogRessystemRoot);
    state.npcDialogLoaded = state.npcDialogDoc >= 0;
    if (!state.npcDialogLoaded) state.statusMessage = L("NpcDialogData.shn nicht gefunden.", "NpcDialogData.shn not found.");
}
bool LoadAiScriptFile(EditorState& state, const std::filesystem::path& path,
                      const std::string& displayName, bool openPopup, bool discardDirty) {
    const auto workingPath = PreferProjectOverride(
        state.project, core::ProjectOutputSide::Server, path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(workingPath,ec)) {
        state.statusMessage = "KI-Skript nicht gefunden: " + path.string();
        return false;
    }
    if (!discardDirty && state.aiScriptDirty) {
        if (state.aiScriptEditorPath == path.string()) {
            // Dasselbe Skript erneut anzuklicken darf den geänderten Puffer nicht verwerfen.
            // Ein Kontextaufruf darf lediglich denselben Puffer zusätzlich im Popup zeigen.
            if (openPopup) state.aiScriptEditorOpen=true;
            return true;
        }
        state.statusMessage = "KI-Skript hat ungespeicherte Änderungen. Erst speichern oder neu laden.";
        return false;
    }
    std::ifstream in(workingPath,std::ios::binary);
    if (!in) {
        state.statusMessage = "KI-Skript konnte nicht geöffnet werden: " + workingPath.string();
        return false;
    }
    state.aiScriptEditorText.assign((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    state.aiScriptEditorPath=path.string();
    state.aiScriptEditorName=displayName.empty()?path.filename().string():displayName;
    state.aiScriptDirty=false;
    if(openPopup) state.aiScriptEditorOpen=true;
    return true;
}
bool SaveAiScript(EditorState& state) {
    if(state.aiScriptEditorPath.empty()) return false;
    std::filesystem::path target;
    std::string error;
    if (!PrepareProjectOutput(
            ProjectPathForSource(state.project, core::ProjectOutputSide::Server,
                                 std::filesystem::path(state.aiScriptEditorPath)),
            target, &error)) {
        state.statusMessage = "KI-Skript speichern blockiert: " + error;
        return false;
    }
    std::ofstream out(target,std::ios::binary|std::ios::trunc);
    if(!out) {
        state.statusMessage = "Fehler beim Speichern: " + target.string();
        return false;
    }
    out.write(state.aiScriptEditorText.data(),static_cast<std::streamsize>(state.aiScriptEditorText.size()));
    if(!out) {
        state.statusMessage = "Fehler beim Speichern: " + target.string();
        return false;
    }
    state.aiScriptDirty=false;
    state.statusMessage = "Projekt-Override gespeichert: " + target.string();
    return true;
}
void ScanAiWorkspace(EditorState& state) {
    const std::string key=state.shnServerRoot;
    if(key==state.aiWorkspaceScanKey) return;
    state.aiWorkspaceScanKey=key;
    state.aiWorkspaceFiles.clear();
    state.aiWorkspaceLabels.clear();
    state.aiWorkspaceKinds.clear();
    state.aiWorkspaceSelected=-1;
    if(key.empty()) return;

    struct RootDef { const char* rel; const char* ext; const char* prefix; int kind; };
    const RootDef roots[]={
        {"LuaScript/AIScript",".lua","Lua",0},
        {"MobBehaviorDescript",".ps","Pine",1}
    };
    struct Entry { std::filesystem::path path; std::string label; int kind; };
    std::vector<Entry> entries;
    std::error_code ec;
    for(const auto& rootDef:roots) {
        const auto root=std::filesystem::path(key)/rootDef.rel;
        if(!std::filesystem::is_directory(root,ec)) { ec.clear(); continue; }
        for(std::filesystem::recursive_directory_iterator it(
                root,std::filesystem::directory_options::skip_permission_denied,ec),end;
            it!=end;it.increment(ec)) {
            if(ec) { ec.clear(); continue; }
            if(!it->is_regular_file(ec)||ec) { ec.clear(); continue; }
            if(LowerAscii(it->path().extension().string())!=rootDef.ext) continue;
            auto rel=std::filesystem::relative(it->path(),root,ec);
            if(ec) { ec.clear(); rel=it->path().filename(); }
            entries.push_back({it->path(),std::string(rootDef.prefix)+" · "+rel.generic_string(),rootDef.kind});
        }
    }
    std::stable_sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){
        return LowerAscii(a.label)<LowerAscii(b.label);
    });
    for(auto& e:entries) {
        state.aiWorkspaceFiles.push_back(std::move(e.path));
        state.aiWorkspaceLabels.push_back(std::move(e.label));
        state.aiWorkspaceKinds.push_back(e.kind);
    }
}
// Öffnet ein Lua-KI-Skript (LuaScript/AIScript/<Name>.lua, unter state.shnServerRoot) als
// reinen Text-Editor - siehe CHANGELOG [0.44.22]. Echte, vollständige Lua-Dateien (require/
// function/Engine-Aufrufe wie cStaticDamage_smo) - bewusst KEINE Syntaxprüfung, nur
// Text-Bearbeitung, ein Lua-Parser wäre weit außerhalb des Editor-Umfangs.
void OpenAiScriptEditor(EditorState& state, const std::string& mobName) {
    if (state.shnServerRoot.empty()) { state.statusMessage = "Server-SHN-Ordner nötig (Single SHN Editor)."; return; }
    const auto path=std::filesystem::path(state.shnServerRoot)/"LuaScript"/"AIScript"/(mobName+".lua");
    LoadAiScriptFile(state,path,mobName+".lua",true);
}
// Dito für PineScript (MobBehaviorDescript/<relativer Pfad ohne .ps>) - relScriptPath kommt
// z.B. direkt aus World/PineScript.txt (Spalte ScriptName, siehe CHANGELOG [0.44.22]).
void OpenPineScriptEditor(EditorState& state, const std::string& relScriptPath) {
    if (state.shnServerRoot.empty()) { state.statusMessage = "Server-SHN-Ordner nötig (Single SHN Editor)."; return; }
    const auto path=std::filesystem::path(state.shnServerRoot)/"MobBehaviorDescript"/(relScriptPath+".ps");
    LoadAiScriptFile(state,path,relScriptPath+".ps",true);
}
// Lädt/öffnet die Patrouillenroute eines Mobs/NPCs (MobRoam/<Name>.txt, Server) - ShineText-
// Format, Tabelle "Roaming": ID/X/Y/EventIndex, letzter Wegpunkt trägt oft "return" als
// EventIndex (siehe CHANGELOG [0.44.23], byte-exakt gegen echte Daten wie BerValeDw04.txt
// verifiziert). Nicht jedes Mob/NPC hat eine Route - fehlende Datei ist normal, kein Fehler.
void OpenPatrolRouteEditor(EditorState& state, const std::string& mobName) {
    if (state.shnServerRoot.empty()) {
        state.statusMessage = L(
            "Server-SHN-Ordner nötig (Single SHN Editor).",
            "Server SHN folder required (Single SHN Editor).");
        return;
    }
    const auto sourcePath = std::filesystem::path(state.shnServerRoot) / "MobRoam" / (mobName + ".txt");
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        state.statusMessage =
            L("Keine Patrouillenroute gefunden: ","No patrol route found: ") + path.string();
        state.patrolRouteLoaded = false;
        return;
    }
    auto result = core::legacy::LoadShineTextFile(path);
    if (!result) { state.statusMessage = "MobRoam/" + mobName + ".txt: " + result.error(); return; }
    state.patrolRouteFile = std::move(*result);
    state.patrolRouteLoaded = true;
    state.patrolEditorName = mobName;
    state.patrolEditorOpen = true;
    state.roamOverlayKey.clear();
}
void DrawPatrolRouteEditorPopup(EditorState& state) {
    if (!state.patrolEditorOpen) return;
    const char* patrolPopupTitle =
        L("Patrouillenroute bearbeiten###patrolRouteEditor",
          "Edit patrol route###patrolRouteEditor");
    ImGui::OpenPopup(patrolPopupTitle);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 460.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(patrolPopupTitle, &state.patrolEditorOpen)) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", state.patrolEditorName.c_str());
        if (!state.patrolRouteLoaded) {
            ImGui::TextWrapped("%s",L(
                "Route konnte nicht geladen werden.",
                "Route could not be loaded."));
        } else {
            auto* table = state.patrolRouteFile.FindTable("Roaming");
            if (!table) {
                ImGui::TextDisabled("%s",L(
                    "Keine 'Roaming'-Tabelle in dieser Datei gefunden.",
                    "No 'Roaming' table was found in this file."));
            } else {
                ImGui::TextDisabled("%s",L(
                    "Wegpunkte werden der Reihe nach abgelaufen; 'return' = zurück zum Start.",
                    "Waypoints are followed in order; 'return' = return to the start."));
                int removeIdx = -1;
                for (std::size_t i = 0; i < table->records.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    auto& rec = table->records[i];
                    if (rec.values.size() < 4) { ImGui::PopID(); continue; }
                    int x = std::atoi(rec.values[1].c_str());
                    int y = std::atoi(rec.values[2].c_str());
                    ImGui::Text("#%s", rec.values[0].c_str());
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.0f);
                    if (UI::InputInt("X", &x)) rec.values[1] = std::to_string(x);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(90.0f);
                    if (UI::InputInt("Y", &y)) rec.values[2] = std::to_string(y);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(100.0f);
                    std::vector<char> evBuf(rec.values[3].begin(), rec.values[3].end());
                    evBuf.resize(std::max<std::size_t>(evBuf.size() + 1, 32));
                    if (UI::InputText("Event", evBuf.data(), evBuf.size())) rec.values[3].assign(evBuf.data());
                    ImGui::SameLine();
                    if (UI::SmallButton("X")) removeIdx = static_cast<int>(i);
                    ImGui::PopID();
                }
                if (removeIdx >= 0) table->records.erase(table->records.begin() + removeIdx);
                if (UI::Button(L("+ Wegpunkt hinzufügen","+ Add waypoint"))) {
                    std::vector<std::string> vals = {std::to_string(table->records.size()), "0", "0", "-"};
                    table->records.push_back(core::legacy::ShineRecord{vals, 0});
                }
                ImGui::Separator();
                if (UI::Button(L("Speichern","Save"))) {
                    auto saved = SaveProjectShineOverride(
                        state.patrolRouteFile,
                        ProjectServerShinePath(
                            state.project,
                            std::filesystem::path("MobRoam") / (state.patrolEditorName + ".txt")));
                    state.statusMessage = saved
                        ? L("Patrouillenroute als Projekt-Override gespeichert.","Patrol route saved as project override.")
                        : L("Fehler: ","Error: ") + saved.error();
                    if (saved) state.roamOverlayKey.clear();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button(L("Schließen","Close"))) {
            state.patrolEditorOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void DrawAiScriptEditorPopup(EditorState& state) {
    if (!state.aiScriptEditorOpen) return;
    ImGui::OpenPopup(L("KI-Skript bearbeiten","Edit AI script"));
    ImGui::SetNextWindowSize(ImVec2(720.0f, 540.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(L("KI-Skript bearbeiten","Edit AI script"), &state.aiScriptEditorOpen)) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s%s",
                           state.aiScriptEditorName.c_str(),state.aiScriptDirty?" *":"");
        ImGui::TextDisabled("%s",L("Reiner Text-Editor (Lua/PineScript) - keine Syntaxprüfung, Änderungen 1:1 gespeichert.",
                                    "Plain text editor (Lua/PineScript) - no syntax checking, changes are saved 1:1."));
        std::vector<char> buf(state.aiScriptEditorText.begin(), state.aiScriptEditorText.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 8192));
        if (ImGui::InputTextMultiline("##aiscript", buf.data(), buf.size(), ImVec2(-1.0f, 420.0f), ImGuiInputTextFlags_AllowTabInput)) {
            state.aiScriptEditorText.assign(buf.data());
            state.aiScriptDirty=true;
        }
        if (ShortcutPressed(state.shortcutSave) && state.aiScriptDirty) SaveAiScript(state);
        if (UI::Button(state.aiScriptDirty ? L("Speichern *","Save *") : L("Speichern","Save"))) SaveAiScript(state);
        ImGui::SameLine();
        if (UI::Button(L("Neu laden / verwerfen","Reload / discard")))
            LoadAiScriptFile(state,state.aiScriptEditorPath,state.aiScriptEditorName,true,true);
        ImGui::SameLine();
        if (UI::Button(L("Schließen","Close"))) { state.aiScriptEditorOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
}
void DrawAiWorkspace(EditorState& state) {
    ScanAiWorkspace(state);

    const std::string aiHeaderMeta = std::to_string(state.aiWorkspaceFiles.size()) +
                                     L(" Skripte · Lua + PineScript"," scripts · Lua + PineScript");
    DrawPanelHeader("aiWorkspaceHeader", "AI WORKSPACE", DrawIconCode,
                    "module.ai", aiHeaderMeta.c_str());
    ImGui::TextWrapped("%s",L(
        "Direkter Text-Editor für die bereits verifizierten Fiesta-KI-Pfade. Keine Syntaxinterpretation: Änderungen werden byte-nah als Text gespeichert.",
        "Direct text editor for the already verified Fiesta AI paths. No syntax interpretation: changes are saved as plain text."));
    ImGui::Separator();

    if(state.shnServerRoot.empty()) {
        ImGui::TextWrapped("%s",L(
            "Kein Server-Shine-Ordner bekannt. Zuerst im Projekt einen Server-Ordner konfigurieren.",
            "No server Shine folder is known. Configure a server folder in the project first."));
        return;
    }

    const float listW=std::clamp(ImGui::GetContentRegionAvail().x*0.30f,300.0f,420.0f);
    ImGui::BeginChild("##aiLibrary",ImVec2(listW,0),true);
    DrawInlineIcon("aiLibraryHeaderIcon",DrawIconCode,IM_COL32(100,205,255,245),nullptr,
                   ImVec2(18,18),"module.ai");
    ImGui::SameLine(0,5);
    ImGui::TextColored(UiTheme::AccentCyan,"%s",L("SKRIPTBIBLIOTHEK","SCRIPT LIBRARY"));
    ImGui::SameLine();
    if(UI::SmallButton(L("Neu scannen","Rescan"))) {
        state.aiWorkspaceScanKey.clear();
        ScanAiWorkspace(state);
    }
    DrawSearchInput("aiWorkspaceFilter",L("Lua/PineScript filtern...","Filter Lua/PineScript..."),
                    state.aiWorkspaceFilter,sizeof(state.aiWorkspaceFilter));
    const std::string needle=LowerAscii(state.aiWorkspaceFilter);
    std::vector<std::size_t> visible;
    visible.reserve(state.aiWorkspaceLabels.size());
    for(std::size_t i=0;i<state.aiWorkspaceLabels.size();++i)
        if(needle.empty()||LowerAscii(state.aiWorkspaceLabels[i]).find(needle)!=std::string::npos)
            visible.push_back(i);
    ImGui::TextDisabled(L("%zu / %zu sichtbar","%zu / %zu visible"),
                        visible.size(),state.aiWorkspaceFiles.size());
    ImGui::Separator();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while(clipper.Step()) {
        for(int vi=clipper.DisplayStart;vi<clipper.DisplayEnd;++vi) {
            const std::size_t idx=visible[static_cast<std::size_t>(vi)];
            ImGui::PushID(static_cast<int>(idx));
            const bool lua=idx<state.aiWorkspaceKinds.size()&&state.aiWorkspaceKinds[idx]==0;
            DrawInlineIcon("type",lua?DrawIconCode:DrawIconRoute,
                           lua?IM_COL32(95,195,255,245):IM_COL32(195,135,255,245),
                           lua?"Lua AIScript":"PineScript",ImVec2(18,18),
                           lua?"module.ai":"gameplay.path");
            ImGui::SameLine(0,4);
            const bool selected=state.aiWorkspaceSelected==static_cast<int>(idx) ||
                                (!state.aiScriptEditorPath.empty() &&
                                 state.aiWorkspaceFiles[idx].string()==state.aiScriptEditorPath);
            if(UI::Selectable((state.aiWorkspaceLabels[idx]+"##aiEntry").c_str(),selected)) {
                if(LoadAiScriptFile(state,state.aiWorkspaceFiles[idx],
                                    state.aiWorkspaceFiles[idx].filename().string(),false)) {
                    state.aiWorkspaceSelected=static_cast<int>(idx);
                }
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s",state.aiWorkspaceFiles[idx].string().c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##aiEditor",ImVec2(0,0),true);
    DrawInlineIcon("aiEditorHeaderIcon",DrawIconCode,IM_COL32(100,205,255,245),nullptr,
                   ImVec2(18,18),"module.ai");
    ImGui::SameLine(0,5);
    ImGui::TextColored(UiTheme::AccentCyan,"%s",L("EDITOR","EDITOR"));
    if(state.aiScriptEditorPath.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("%s",L("Links ein Skript auswählen.","Choose a script on the left."));
        ImGui::EndChild();
        return;
    }

    ImGui::SameLine();
    ImGui::Text("%s%s",state.aiScriptEditorName.c_str(),state.aiScriptDirty?" *":"");
    if(state.aiScriptDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.72f,0.30f,1.0f),"%s",L("UNGESPEICHERT","UNSAVED"));
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s",state.aiScriptEditorPath.c_str());

    if(ShortcutPressed(state.shortcutSave) && state.aiScriptDirty) SaveAiScript(state);
    if(UI::Button(state.aiScriptDirty?L("Speichern *","Save *"):L("Speichern","Save")))
        SaveAiScript(state);
    ImGui::SameLine();
    if(UI::Button(L("Neu laden / Änderungen verwerfen","Reload / discard changes"))) {
        LoadAiScriptFile(state,state.aiScriptEditorPath,state.aiScriptEditorName,false,true);
    }
    ImGui::SameLine();
    const std::size_t lineCount=state.aiScriptEditorText.empty()?0:
        1+static_cast<std::size_t>(std::count(state.aiScriptEditorText.begin(),state.aiScriptEditorText.end(),'\n'));
    ImGui::TextDisabled(L("%zu Zeilen · %zu Bytes","%zu lines · %zu bytes"),
                        lineCount,state.aiScriptEditorText.size());

    std::vector<char> buf(state.aiScriptEditorText.begin(),state.aiScriptEditorText.end());
    buf.resize(std::max<std::size_t>(buf.size()+4096,65536),0);
    if(ImGui::InputTextMultiline("##aiWorkspaceText",buf.data(),buf.size(),ImVec2(-1.0f,-1.0f),
                                 ImGuiInputTextFlags_AllowTabInput)) {
        state.aiScriptEditorText.assign(buf.data());
        state.aiScriptDirty=true;
    }
    ImGui::EndChild();
}
int FindDialogRowForNpc(EditorState& state, const std::string& npcName) {
    if (state.npcDialogDoc < 0) return -1;
    const auto& f = ShnDocFile(state, state.npcDialogDoc);
    for (std::size_t i = 0; i < f.rows.size(); ++i) {
        if (f.rows[i].values.empty()) continue;
        if (core::legacy::ShnValueToString(f.rows[i].values[0]) == npcName) return static_cast<int>(i);
    }
    return -1;
}
// Legt für einen NPC ohne Eintrag eine Dialogzeile an: Struktur der ersten Zeile, MobIDX = NPC-Name,
// FaceCutFile der Vorlage, leerer Dialog. Die Zeile ist ganz als geändert markiert (wie neue Zeilen
// im SHN-Editor) und wird mit dem Dokument gespeichert.
int CreateNpcDialogRow(EditorState& state, const std::string& npcName) {
    if (state.npcDialogDoc < 0 || npcName.empty()) return -1;
    auto& doc = state.shnFiles[static_cast<std::size_t>(state.npcDialogDoc)];
    auto& f = doc.file;
    if (f.columns.size() < 3) return -1;
    core::legacy::ShnRow row;
    if (!f.rows.empty()) row = f.rows.front();
    else for (const auto& c : f.columns) {
        auto v = core::legacy::ParseShnValue(c, DefaultShnValueText(c));
        row.values.push_back(v ? *v : core::legacy::ShnValue{});
    }
    f.rows.push_back(std::move(row));
    const std::size_t r = f.rows.size() - 1;
    SetShnCellText(f, r, "MobIDX", npcName);
    if (FindShnColumnByName(f, "MobIDX") < 0) {
        if (auto v = core::legacy::ParseShnValue(f.columns[0], npcName)) f.rows[r].values[0] = std::move(*v);
    }
    if (auto v = core::legacy::ParseShnValue(f.columns[2], "")) f.rows[r].values[2] = std::move(*v);
    EnsureCellStatusSize(doc);
    std::fill(doc.cellDirty[r].begin(), doc.cellDirty[r].end(), static_cast<std::uint8_t>(1));
    doc.dirty = true;
    ++state.shnEditCounter;
    RecordRowsInserted(state, L("NPC-Dialog angelegt: ", "NPC dialog created: ") + npcName, {{state.npcDialogDoc, static_cast<int>(r)}});
    return static_cast<int>(r);
}
// Öffnet den Dialog-Editor für den übergebenen NPC-Namen - lädt Root/Datei bei Bedarf nach,
// sucht die passende Zeile per MobIDX und befüllt die Bearbeitungs-Buffer.
void EnsureMobViewInfoLoaded(EditorState& state) {
    if (state.mobViewInfoLoaded || state.npcDialogRessystemRoot.empty()) return;
    state.mobViewInfoDoc = SharedShnDoc(state, "MobViewInfo.shn", EditorState::ShnSource::Client,
                                        state.npcDialogRessystemRoot);
    state.mobViewInfoLoaded = state.mobViewInfoDoc >= 0;
}
// Löst den Dateinamen (ohne Endung) des 3D-Modells für einen NPC auf, per InxName-Suche in
// MobViewInfo.shn (Spalte 2 = FileName). Gibt leer zurück, wenn nicht gefunden ODER wenn
// NpcViewIndex (Spalte 9) != 0 ist - dann ist die Erscheinung aus einzeln ausgerüsteten
// Gegenständen zusammengesetzt (siehe NPCViewInfo.shn), dieser komplexere Fall wird noch nicht
// unterstützt (siehe CHANGELOG [0.44.17]). Bei allen gegen echte NA2016-Daten geprüften
// normalen Shop-NPCs (RouSmithJames, RouSoulMctJulia, ...) ist NpcViewIndex==0 - eigenes
// Modell mit demselben Namen wie der NPC selbst.
std::string ResolveMobViewFileName(EditorState& state, const std::string& inxName) {
    if (!state.mobViewInfoLoaded) return {};
    for (auto& row : ShnDocFile(state, state.mobViewInfoDoc).rows) {
        if (row.values.size() < 10) continue;
        if (core::legacy::ShnValueToString(row.values[1]) != inxName) continue;
        long long npcViewIdx = 0;
        ShnValueAsInt(row.values[9], npcViewIdx);
        if (npcViewIdx != 0) return {};
        return core::legacy::ShnValueToString(row.values[2]);
    }
    return {};
}
// Baut npcRenderSet neu auf (alle NPCs der aktuellen Karte mit auflösbarem 3D-Modell) und lädt
// sie in npcMeshRenderer - nur wenn sich die Karte geändert hat oder npcRenderSetForMap manuell
// geleert wurde (siehe Coord-X/Y-Bearbeitung oben, DrawToolsContent).
// Sucht den "reschar"-Ordner (Client, Geschwister von "resmap"/"ressystem") - enthält pro
// Charakter einen eigenen Unterordner mit .nif (siehe CHANGELOG [0.44.18], vom Nutzer per
// Screenshot des echten Client-Ordners bestätigt: Client/reschar/<Name>/<Name>.nif).
void EnsureRescharRoot(EditorState& state) {
    if (!state.rescharRoot.empty()) return;
    if (auto found = FindNamedSubfolder(state.project.clientFolder, {"reschar"})) {
        state.rescharRoot = found->string();
    }
}
// Liest/Schreibt eine Zelle per Spaltenname (Text). false, wenn Spalte fehlt oder der Wert nicht passt.
std::string ShnCellText(const core::legacy::ShnFile& f, std::size_t row, const char* col) {
    const int c = ShnColumnIndexByName(f, col);
    if (c < 0 || row >= f.rows.size() || static_cast<std::size_t>(c) >= f.rows[row].values.size()) return {};
    return core::legacy::ShnValueToString(f.rows[row].values[static_cast<std::size_t>(c)]);
}
bool SetShnCellText(core::legacy::ShnFile& f, std::size_t row, const char* col, const std::string& text) {
    const int c = ShnColumnIndexByName(f, col);
    if (c < 0 || row >= f.rows.size() || static_cast<std::size_t>(c) >= f.rows[row].values.size()) return false;
    auto parsed = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(c)], text);
    if (!parsed) return false;
    f.rows[row].values[static_cast<std::size_t>(c)] = std::move(*parsed);
    return true;
}
// Blickwinkel (Radiant, Drehung um die Hochachse im Editor-Rahmen) eines NPCs aus seiner Richtung (Grad).
float NpcYawRadians(const EditorState& state, int directionDeg) {
    return (static_cast<float>(state.npcDirSign * directionDeg) + static_cast<float>(state.npcDirOffsetDeg)) * 3.14159265f / 180.0f;
}
// Schreibt Position und Drehung aus einem NPC.txt-Datensatz in ein Render-Objekt.
void ApplyNpcRecordToObject(const EditorState& state, const core::legacy::ShineRecord& rec, core::PlacedObject& obj) {
    obj.posX = static_cast<float>(std::atoi(rec.values[2].c_str()));
    obj.posZ = static_cast<float>(std::atoi(rec.values[3].c_str()));
    obj.posY = state.heightmap.SampleWorld(obj.posX, obj.posZ);
    const float half = NpcYawRadians(state, std::atoi(rec.values[4].c_str())) * 0.5f;
    obj.rotY = std::sin(half);
    obj.rotW = std::cos(half);
    obj.scale = 1.0f;
}
// Aktualisiert Position/Drehung aller NPC-Render-Objekte OHNE die Modelle neu zu laden (Coord-/Richtung-
// Eingaben sollen bei grossen Charaktermodellen nicht jedes Mal alles neu von der Platte laden).
void RefreshNpcTransforms(EditorState& state) {
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return;
    for (std::size_t i = 0; i < state.npcRenderSet.Count() && i < state.npcRenderRecordIdx.size(); ++i) {
        const std::size_t idx = state.npcRenderRecordIdx[i];
        if (idx >= table->records.size() || table->records[idx].values.size() < 5) continue;
        ApplyNpcRecordToObject(state, table->records[idx], state.npcRenderSet.At(i));
    }
}
// Schaetzt Vorzeichen/Versatz der NPC-Blickrichtung fuer die gerade offene Karte (CHANGELOG
// [0.44.34]): testet je Kandidat, wie oft ein NPC "vorne" (in Blickrichtung) auf einer begehbaren
// Zelle und "hinten" auf einer blockierten Zelle steht (Annahme: NPCs stehen mit dem Ruecken zur
// Wand) - dieselbe Methode, mit der der Standardwert ermittelt wurde, hier live auf EINE Karte
// angewendet. Reine Heuristik: Karten ohne nahe Waende (offene Plaetze) liefern kein Signal, kleine
// NPC-Zahlen sind verrauscht - das Ergebnis ersetzt keinen Blick ins echte Spiel.
void EstimateNpcOrientation(EditorState& state) {
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table || state.walkGrid.Width() == 0) {
        state.npcOrientEstimateStatus = L("Keine NPCs oder kein Block&Walk-Gitter geladen.", "No NPCs or no Block&Walk grid loaded.");
        return;
    }
    struct Cand { int sign; int offset; long long net = 0; };
    std::vector<Cand> cands;
    for (const int sign : {1, -1})
        for (int off = -180; off < 180; off += 10) cands.push_back({sign, off});

    const float dists[] = {18.75f, 37.5f, 62.5f}; // 3, 6, 10 Zellen (Zelle = 6.25 Einheiten)
    int sampledNpcs = 0;
    for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        const float x = static_cast<float>(std::atoi(rec.values[2].c_str()));
        const float z = static_cast<float>(std::atoi(rec.values[3].c_str()));
        const int direct = std::atoi(rec.values[4].c_str());
        bool any = false;
        for (const float dist : dists) {
            for (auto& c : cands) {
                const float yaw = static_cast<float>(c.sign * direct + c.offset) * 3.14159265f / 180.0f;
                const float fx = -std::sin(yaw), fz = -std::cos(yaw);
                auto cellOf = [](float wx, float wz) {
                    return std::pair<std::uint32_t, std::uint32_t>{
                        static_cast<std::uint32_t>(std::max(0.0f, wx / core::WalkGrid::kCellSize)),
                        static_cast<std::uint32_t>(std::max(0.0f, wz / core::WalkGrid::kCellSize))};
                };
                const auto [acx, acz] = cellOf(x + fx * dist, z + fz * dist);
                const auto [bcx, bcz] = cellOf(x - fx * dist, z - fz * dist);
                if (acx >= state.walkGrid.Cols() || acz >= state.walkGrid.Rows() ||
                    bcx >= state.walkGrid.Cols() || bcz >= state.walkGrid.Rows()) continue;
                const bool aheadBlocked = state.walkGrid.CellBlocked(acx, acz);
                const bool behindBlocked = state.walkGrid.CellBlocked(bcx, bcz);
                if (!aheadBlocked && behindBlocked) ++c.net;
                if (aheadBlocked && !behindBlocked) --c.net;
                any = true;
            }
        }
        if (any) ++sampledNpcs;
    }
    if (sampledNpcs < 3) {
        state.npcOrientEstimateStatus = L("Zu wenige NPCs mit Wand in der Naehe auf dieser Karte fuer eine Schaetzung.",
                                          "Too few NPCs with a nearby wall on this map for an estimate.");
        return;
    }
    const auto best = std::max_element(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.net < b.net; });
    const long long worst = std::min_element(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.net < b.net; })->net;
    state.npcDirSign = best->sign;
    state.npcDirOffsetDeg = best->offset;
    RefreshNpcTransforms(state);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  L("Geschätzt aus %d NPCs: Vorzeichen %+d, Versatz %d Grad (Signal %+lld, schlechtester Kandidat %+lld - je größer der Abstand, desto verlässlicher).",
                    "Estimated from %d NPCs: sign %+d, offset %d degrees (signal %+lld, worst candidate %+lld - the bigger the gap, the more reliable)."),
                  sampledNpcs, best->sign, best->offset, best->net, worst);
    state.npcOrientEstimateStatus = buf;
}
// Schreibweisen-unabhaengige Suche einer Datei/eines Ordners in einem Ordner.
std::optional<std::filesystem::path> FindChildCaseInsensitive(const std::filesystem::path& dir, const std::string& name) {
    std::error_code ec;
    const std::string want = LowerAscii(name);
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (LowerAscii(entry.path().filename().string()) == want) return entry.path();
    }
    return std::nullopt;
}
// Sucht das Modell eines NPCs (MobViewInfo.FileName) in ALLEN Modell-Ordnern des Clients: zuerst
// reschar/<Name>/<Name>.nif (Charaktere, siehe CHANGELOG [0.44.18]), dann jeder weitere "res*"-Ordner
// (z.B. resmob) als <Ordner>/<Name>/<Name>.nif bzw. <Ordner>/<Name>.nif, zuletzt die NIF-Bibliothek von
// resmap. Rueckgabe: Pfad relativ zum Client-Ordner (leer = nicht gefunden).
std::string FindNpcModelPath(EditorState& state, const std::string& fileName) {
    const std::filesystem::path clientRoot(state.project.clientFolder);
    std::vector<std::filesystem::path> roots;
    if (!state.rescharRoot.empty()) roots.emplace_back(state.rescharRoot);
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(clientRoot, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string n = LowerAscii(entry.path().filename().string());
            if (n.rfind("res", 0) != 0) continue;
            if (n == "ressystem" || n == "resmenu" || n == "resmap" || n == "resitem" || n == "reschar") continue;
            roots.push_back(entry.path());
        }
    }
    for (const auto& root : roots) {
        std::error_code ec;
        std::filesystem::path candidate;
        if (auto dir = FindChildCaseInsensitive(root, fileName); dir && std::filesystem::is_directory(*dir, ec)) {
            if (auto nif = FindChildCaseInsensitive(*dir, fileName + ".nif")) candidate = *nif;
        }
        if (candidate.empty()) {
            if (auto nif = FindChildCaseInsensitive(root, fileName + ".nif")) candidate = *nif;
        }
        if (candidate.empty()) continue;
        const auto rel = std::filesystem::relative(candidate, clientRoot, ec);
        if (!ec) return rel.string();
    }
    if (!state.nifAssetRoot.empty()) {
        for (auto& nif : state.availableNifFiles) {
            if (LowerAscii(std::filesystem::path(nif).stem().string()) != LowerAscii(fileName)) continue;
            std::error_code ec;
            auto rel = std::filesystem::relative(std::filesystem::path(state.nifAssetRoot) / nif, clientRoot, ec);
            if (!ec) return rel.string();
        }
    }
    return {};
}
NpcRenderData CollectNpcRenderData(EditorState& state) {
    NpcRenderData out;
    if (!state.npcTextLoaded) return out;
    EnsureMobViewInfoLoaded(state);
    if (!state.mobViewInfoLoaded) return out;
    EnsureRescharRoot(state);
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return out;
    auto& customModels = out.customModels;
    // Spieler-Avatar-NPCs (MobViewInfo.NpcViewIndex != 0): Aussehen aus NPCViewInfo.shn (Klasse, Geschlecht,
    // Gesicht, Frisur, Ausruestung) - werden mit dem Avatar-Baukasten zusammengesetzt (AvatarPreview).
    const int npcViewDoc = FindOrLoadShnDoc(state, "NPCViewInfo.shn", EditorState::ShnSource::Client);

    core::ObjectPlacementSet& newSet = out.set;
    std::vector<std::size_t>& recordIdx = out.recordIdx;
    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
        auto& rec = table->records[idx];
        if (rec.values.size() < 5) continue;
        // MobViewInfo-Zeile des NPCs (InxName = NPC.txt-Name)
        std::string fileName;
        long long npcViewIdx = 0;
        for (auto& row : ShnDocFile(state, state.mobViewInfoDoc).rows) {
            if (row.values.size() < 10) continue;
            if (core::legacy::ShnValueToString(row.values[1]) != rec.values[0]) continue;
            ShnValueAsInt(row.values[9], npcViewIdx);
            fileName = core::legacy::ShnValueToString(row.values[2]);
            break;
        }

        std::string modelPath;
        if (npcViewIdx != 0 && npcViewDoc >= 0 && !state.charRoot.empty()) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(npcViewDoc)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (std::atoll(ShnCellText(f, r, "TypeIndex").c_str()) != npcViewIdx) continue;
                const std::string key = "avatar:" + std::to_string(npcViewIdx);
                if (!customModels.count(key)) {
                    std::array<std::string, 19> equ;
                    for (int q = 0; q < 19; ++q) equ[static_cast<std::size_t>(q)] = ShnCellText(f, r, kAvatarSlotColumns[q]);
                    auto req = MakeAvatarRequest(state, std::atoi(ShnCellText(f, r, "Class").c_str()), std::atoi(ShnCellText(f, r, "Gender").c_str()) != 0,
                                                 std::atoi(ShnCellText(f, r, "FaceShape").c_str()), std::atoi(ShnCellText(f, r, "HairType").c_str()), equ);
                    if (auto built = core::BuildAvatarModel(req)) customModels.emplace(key, core::AvatarToNifModel(*built));
                }
                if (customModels.count(key)) modelPath = key;
                break;
            }
        } else if (!fileName.empty()) {
            modelPath = FindNpcModelPath(state, fileName);
        }
        if (modelPath.empty()) {
            ++out.missing;
            if (out.missingNames.size() < 12) out.missingNames.push_back(rec.values[0] + (fileName.empty() ? "" : " (" + fileName + ")"));
            continue;
        }
        if (modelPath.rfind("avatar:", 0) == 0) ++out.avatars;
        ++out.withModel;

        core::PlacedObject obj;
        obj.modelPath = modelPath;
        ApplyNpcRecordToObject(state, rec, obj);
        newSet.AddObject(std::move(obj));
        recordIdx.push_back(idx);
    }
    return out;
}
void EnsureNpcModelsLoaded(EditorState& state) {
    if (!state.npcTextLoaded) return;
    if (state.npcRenderSetForMap == state.legacySaveStem) return;
    NpcRenderData data = CollectNpcRenderData(state);
    // Fiktiver, zwei Ebenen tiefer "mapDir" - ResolveLegacyAssetPath leitet daraus per
    // parent_path().parent_path() automatisch clientRoot als Such-Basis her (Trick schon bei
    // der Asset-Vorschau in DrawToolsContent verwendet, siehe GetOrLoadAssetThumbnail).
    const std::filesystem::path fakeMapDir = std::filesystem::path(state.project.clientFolder) / "_npcresolve" / "_probe";
    state.npcRenderSet = std::move(data.set);
    state.npcRenderRecordIdx = std::move(data.recordIdx);
    state.npcModelsMissing = data.missing;
    state.npcModelsMissingNames = std::move(data.missingNames);
    state.npcMeshRenderer.LoadModelsForSet(state.npcRenderSet, fakeMapDir, &data.customModels);
    state.npcRenderSetForMap = state.legacySaveStem;
}
void OpenNpcDialogEditor(EditorState& state, const std::string& npcName) {
    state.dialogEditorNpc = npcName;
    EnsureNpcDialogRoot(state);
    EnsureNpcDialogLoaded(state);
    state.dialogEditorRowIdx = state.npcDialogLoaded ? FindDialogRowForNpc(state, npcName) : -1;
    if (state.dialogEditorRowIdx >= 0 && ShnDocFile(state, state.npcDialogDoc).columns.size() > 2) {
        auto& row = ShnDocFile(state, state.npcDialogDoc).rows[static_cast<std::size_t>(state.dialogEditorRowIdx)];
        if (row.values.size() > 2) {
            auto parsed = ParseNpcDialogText(core::legacy::ShnValueToString(row.values[2]));
            state.dialogGreetingBuf = parsed.greeting;
            state.dialogButtonsBuf = parsed.buttons;
        }
    } else {
        state.dialogGreetingBuf.clear();
        state.dialogButtonsBuf.clear();
    }
    state.dialogEditorOpen = true;
}
void DrawDialogEditorPopup(EditorState& state) {
    if (!state.dialogEditorOpen) return;
    const char* dialogPopupTitle =
        L("NPC-Dialog bearbeiten###npcDialogEditor","Edit NPC dialog###npcDialogEditor");
    ImGui::OpenPopup(dialogPopupTitle);
    ImGui::SetNextWindowSize(ImVec2(580.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(dialogPopupTitle, &state.dialogEditorOpen)) {
        if (state.npcDialogRessystemRoot.empty()) {
            ImGui::TextWrapped("%s",L(
                "Client-Ordner 'ressystem' (enthält NpcDialogData.shn) nicht gefunden - bitte manuell angeben:",
                "Client 'ressystem' folder (contains NpcDialogData.shn) was not found - choose it manually:"));
            std::vector<char> rootBuf(state.npcDialogRessystemRoot.begin(), state.npcDialogRessystemRoot.end());
            rootBuf.resize(std::max<std::size_t>(rootBuf.size() + 1, 512));
            ImGui::SetNextItemWidth(-1.0f);
            if (UI::InputText("##ressystemroot", rootBuf.data(), rootBuf.size())) {
                state.npcDialogRessystemRoot.assign(rootBuf.data()); state.npcDialogLoaded = false;
                if (state.shnClientRoot.empty()) state.shnClientRoot = state.npcDialogRessystemRoot;
            }
#ifdef _WIN32
            if (UI::Button(L("Ordner wählen...##ressystem","Choose folder...##ressystem"))) {
                if (auto p = BrowseForFolderWindows(
                        L("ressystem-Ordner wählen","Choose ressystem folder"))) {
                    state.npcDialogRessystemRoot = *p;
                    state.npcDialogLoaded = false;
                    if (state.shnClientRoot.empty()) state.shnClientRoot = state.npcDialogRessystemRoot;
                }
            }
#endif
        } else if (state.dialogEditorRowIdx < 0) {
            ImGui::TextWrapped("%s",L(
                "Kein Dialog-Eintrag für diesen NPC in NpcDialogData.shn gefunden (oder Datei konnte nicht geladen werden).",
                "No dialog entry for this NPC was found in NpcDialogData.shn (or the file could not be loaded)."));
            if (state.npcDialogLoaded && !state.dialogEditorNpc.empty() &&
                UI::Button(L("Dialog anlegen","Create dialog"))) {
                state.dialogEditorRowIdx = CreateNpcDialogRow(state, state.dialogEditorNpc);
                state.dialogGreetingBuf.clear();
                state.dialogButtonsBuf.clear();
            }
        } else {
            ImGui::TextDisabled("%s",L(
                "Begrüßungstext ([NAME] wird durch den Spielernamen ersetzt):",
                "Greeting text ([NAME] is replaced with the player name):"));
            std::vector<char> buf(state.dialogGreetingBuf.begin(), state.dialogGreetingBuf.end());
            buf.resize(std::max<std::size_t>(buf.size() + 1, 2048));
            if (ImGui::InputTextMultiline("##greeting", buf.data(), buf.size(), ImVec2(-1.0f, 100.0f))) {
                state.dialogGreetingBuf.assign(buf.data());
            }
            ImGui::Separator();
            ImGui::Text("%s",L("Buttons:","Buttons:"));
            int removeIdx = -1;
            for (std::size_t i = 0; i < state.dialogButtonsBuf.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& [label, action] = state.dialogButtonsBuf[i];
                std::vector<char> lbuf(label.begin(), label.end()); lbuf.resize(std::max<std::size_t>(lbuf.size() + 1, 128));
                std::vector<char> abuf(action.begin(), action.end()); abuf.resize(std::max<std::size_t>(abuf.size() + 1, 128));
                ImGui::SetNextItemWidth(180.0f);
                if (UI::InputText("##label", lbuf.data(), lbuf.size())) label.assign(lbuf.data());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(220.0f);
                if (UI::InputText("##action", abuf.data(), abuf.size())) action.assign(abuf.data());
                ImGui::SameLine();
                if (UI::Button(L("Entfernen","Remove"))) removeIdx = static_cast<int>(i);
                ImGui::PopID();
            }
            if (removeIdx >= 0) state.dialogButtonsBuf.erase(state.dialogButtonsBuf.begin() + removeIdx);
            if (UI::Button(L("+ Button hinzufügen","+ Add button")))
                state.dialogButtonsBuf.emplace_back("Neuer Button", "server_ack 1");
            ImGui::TextDisabled("%s",L(
                "Übliche Aktionen: 'server_ack <Zahl>' (Server-Callback, z.B. Kauf) oder\n"
                "'opendlg <Name>' (verketteter Dialog, z.B. zu einem Aufwertungs-Menü).",
                "Common actions: 'server_ack <number>' (server callback, e.g. purchase) or\n"
                "'opendlg <name>' (chained dialog, e.g. an upgrade menu)."));
            ImGui::Separator();
            if (UI::Button(L("Speichern","Save"))) {
                ParsedNpcDialog d; d.greeting = state.dialogGreetingBuf; d.buttons = state.dialogButtonsBuf;
                const auto text = SerializeNpcDialogText(d);
                // Über das gemeinsame Dokument: markiert die Zelle (Undo, Prüfung beim Speichern) und
                // speichert zusammen mit allen anderen Änderungen an NpcDialogData.shn (z. B. aus dem
                // Kreaturen-Assistenten), statt sie mit einer privaten Kopie zu überschreiben.
                const bool applied = ApplyShnCellText(state, state.npcDialogDoc, state.dialogEditorRowIdx, 2, text);
                const bool saved = applied && SaveShnDocument(state, state.npcDialogDoc);
                state.statusMessage = saved
                    ? L("NpcDialogData.shn als Projekt-Override gespeichert.","NpcDialogData.shn saved as project override.")
                    : L("Fehler: ","Error: ") + state.shnStatus;
                if (saved) { state.dialogEditorOpen = false; ImGui::CloseCurrentPopup(); }
            }
        }
        ImGui::SameLine();
        if (UI::Button(L("Schließen","Close"))) {
            state.dialogEditorOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
// Baut die Item-Auswahlliste aus ItemInfo.shn (Server bevorzugt, sonst Client).
void EnsureItemLookup(EditorState& state) {
    // Neu aufbauen, wenn ein Editor eine SHN-Zelle geändert hat (z. B. neues Item im SHN-Editor).
    if (state.itemLookupTried && state.itemLookupRevision == state.shnEditCounter) return;
    state.itemLookupTried = true;
    state.itemLookupRevision = state.shnEditCounter;
    state.itemEntries.clear();
    state.itemByInx.clear();
    state.itemInxById.clear();
    EnsureItemInfoLoadedForQuests(state);
    if (!state.itemInfoLoaded) return;
    const auto& f = ShnDocFile(state, state.itemInfoDoc);
    int cId = -1, cInx = -1, cName = -1;
    for (std::size_t i = 0; i < f.columns.size(); ++i) {
        if (f.columns[i].name == "ID") cId = static_cast<int>(i);
        else if (f.columns[i].name == "InxName") cInx = static_cast<int>(i);
        else if (f.columns[i].name == "Name") cName = static_cast<int>(i);
    }
    if (cInx < 0) return;
    for (const auto& row : f.rows) {
        if (static_cast<std::size_t>(cInx) >= row.values.size()) continue;
        EditorState::ShopItemEntry e;
        e.inx = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cInx)]);
        if (e.inx.empty()) continue;
        if (cName >= 0 && static_cast<std::size_t>(cName) < row.values.size()) e.name = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)]);
        long long id = 0;
        if (cId >= 0 && static_cast<std::size_t>(cId) < row.values.size() && ShnValueAsInt(row.values[static_cast<std::size_t>(cId)], id)) e.id = id;
        state.itemEntries.push_back(std::move(e));
    }
    std::sort(state.itemEntries.begin(), state.itemEntries.end(), [](const auto& a, const auto& b) { return a.inx < b.inx; });
    for (std::size_t i = 0; i < state.itemEntries.size(); ++i) state.itemByInx.emplace(state.itemEntries[i].inx, i);
}
// Nummeriert die Records einer Shop-Tabelle neu (Spalte "Rec": 0,1,2,... - im Original mit
// nachgestelltem Leerzeichen "0 ").
void RenumberShopRows(core::legacy::ShineTable& table) {
    for (std::size_t i = 0; i < table.records.size(); ++i) {
        if (table.records[i].values.empty()) table.records[i].values.resize(1);
        table.records[i].values[0] = std::to_string(i) + " ";
    }
}
core::legacy::ShineRecord MakeEmptyShopRow(const core::legacy::ShineTable& table) {
    core::legacy::ShineRecord row;
    row.sourceLine = 0;
    row.values.assign(std::max<std::size_t>(table.columns.size(), 2), "-");
    row.values[0] = "0 ";
    return row;
}
// Legt fuer einen NPC eine neue, leere Shop-Datei an (Kopfzeilen und Tabellenaufbau wie eine
// beliebige vorhandene NPCItemList-Datei). true bei Erfolg (Datei wird erst beim Speichern
// geschrieben).
bool CreateNewShopFile(EditorState& state, const std::string& npcName) {
    const auto dir = std::filesystem::path(state.shineTextRoot) / "NPCItemList";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        state.statusMessage = L("Ordner NPCItemList nicht gefunden: ",
                                "NPCItemList folder not found: ") + dir.string();
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".txt") continue;
        auto tmpl = core::legacy::LoadShineTextFile(entry.path());
        if (!tmpl || tmpl->tables.empty()) continue;
        core::legacy::ShineTextFile nf = core::legacy::MakeShineFileWithHeaderOf(*tmpl, dir / (npcName + ".txt"));
        core::legacy::ShineTable t;
        t.name = "Tab00";
        t.isNew = true;
        t.columns = tmpl->tables.front().columns;
        t.records.push_back(MakeEmptyShopRow(t));
        RenumberShopRows(t);
        nf.tables.push_back(std::move(t));
        state.shopTextFile = std::move(nf);
        state.shopTextLoaded = true;
        state.shopLoadedForNpc = npcName;
        state.shopFileIsNew = true;
        return true;
    }
    state.statusMessage = L("Keine Vorlage in NPCItemList gefunden.",
                            "No template found in NPCItemList.");
    return false;
}
void DrawShopEditorPopup(EditorState& state) {
    if (!state.shopEditorOpen) return;
    const char* shopPopupTitle =
        L("Händler-Inventar###shopInventory","Shop inventory###shopInventory");
    ImGui::OpenPopup(shopPopupTitle);
    ImGui::SetNextWindowSize(ImVec2(940.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(shopPopupTitle, &state.shopEditorOpen)) {
        EnsureItemLookup(state);
        const auto shopPath = std::filesystem::path(state.shineTextRoot) / "NPCItemList" / (state.shopLoadedForNpc + ".txt");
        ImGui::Text(L("Shop von: %s","Shop for: %s"), state.shopLoadedForNpc.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s%s)", shopPath.string().c_str(),
                            state.shopFileIsNew
                                ? L(" - NEU, noch nicht gespeichert"," - NEW, not saved yet")
                                : "");
        ImGui::TextDisabled("%s",L(
            "Jede Zeile = ein Regal mit bis zu 6 Items, jeder Tab = eine Kategorie im Shop-Fenster. "
            "Slot klicken = Item wählen, Rechtsklick = leeren.",
            "Each row is a shelf with up to 6 items; each tab is a shop category. "
            "Click a slot to choose an item; right-click to clear it."));
        ImGui::Separator();
        if (!state.shopTextLoaded) {
            ImGui::TextWrapped(
                L("Für diesen NPC gibt es noch keine Shop-Datei (NPCItemList/%s.txt).",
                  "There is no shop file for this NPC yet (NPCItemList/%s.txt)."),
                state.shopLoadedForNpc.c_str());
            if (UI::Button(L("Shop-Datei neu anlegen","Create shop file")))
                CreateNewShopFile(state, state.shopLoadedForNpc);
        } else {
            auto& file = state.shopTextFile;
            int deleteTable = -1;
            bool structureChanged = false;
            if (ImGui::BeginTabBar("##shoptabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
                for (std::size_t t = 0; t < file.tables.size(); ++t) {
                    auto& table = file.tables[t];
                    const std::string tabLabel = table.name + " (" + std::to_string(table.records.size()) + ")###shoptab" + std::to_string(t);
                    if (!ImGui::BeginTabItem(tabLabel.c_str())) continue;
                    const int slotCols = static_cast<int>(table.columns.size()) - 1;
                    if (ImGui::BeginTable("##shopgrid", slotCols + 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 380.0f))) {
                        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 32.0f);
                        for (int c = 0; c < slotCols; ++c) ImGui::TableSetupColumn(("Slot " + std::to_string(c + 1)).c_str());
                        ImGui::TableSetupColumn(L("Zeile","Row"), ImGuiTableColumnFlags_WidthFixed, 96.0f);
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableHeadersRow();
                        int moveRow = -1, moveDir = 0, removeRow = -1;
                        for (std::size_t ri = 0; ri < table.records.size(); ++ri) {
                            auto& rec = table.records[ri];
                            if (rec.values.size() < table.columns.size()) rec.values.resize(table.columns.size(), "-");
                            ImGui::PushID(static_cast<int>(ri));
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%zu", ri);
                            for (int c = 0; c < slotCols; ++c) {
                                ImGui::TableSetColumnIndex(c + 1);
                                ImGui::PushID(c);
                                std::string& v = rec.values[static_cast<std::size_t>(c) + 1];
                                const bool isEmpty = v.empty() || v == "-";
                                const auto found = state.itemByInx.find(v);
                                const bool known = isEmpty || found != state.itemByInx.end() || state.itemEntries.empty();
                                if (!known) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 40, 40, 255));
                                else if (isEmpty) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 50, 58, 255));
                                const std::string slotLabel = isEmpty ? L("(leer)","(empty)") : v;
                                if (!isEmpty) {
                                    DrawItemIconByInx(state, v, ImGui::GetFrameHeight());
                                    ImGui::SameLine(0.0f, 3.0f);
                                }
                                if (UI::Button(slotLabel.c_str(), ImVec2(-1.0f, 0.0f))) {
                                    state.shopPickTable = static_cast<int>(t);
                                    state.shopPickRow = static_cast<int>(ri);
                                    state.shopPickCol = c + 1;
                                    state.shopItemFilter[0] = '\0';
                                    state.shopPickRequested = true;
                                }
                                if (!known || isEmpty) ImGui::PopStyleColor();
                                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { v = "-"; }
                                if (ImGui::IsItemHovered()) {
                                    if (isEmpty) {
                                        ImGui::SetTooltip("%s",L(
                                            "Leerer Slot - klicken, um ein Item zu wählen",
                                            "Empty slot - click to choose an item"));
                                    } else if (found != state.itemByInx.end()) {
                                        const auto& e = state.itemEntries[found->second];
                                        ImGui::SetTooltip(
                                            L("%s\nID %lld\nRechtsklick = Slot leeren",
                                              "%s\nID %lld\nRight-click = clear slot"),
                                            e.name.c_str(), e.id);
                                    } else if (!state.itemEntries.empty()) {
                                        ImGui::SetTooltip(
                                            L("'%s' ist NICHT in ItemInfo.shn - Tippfehler oder fehlendes Item?",
                                              "'%s' is NOT in ItemInfo.shn - typo or missing item?"),
                                            v.c_str());
                                    }
                                }
                                ImGui::PopID();
                            }
                            ImGui::TableSetColumnIndex(slotCols + 1);
                            if (UI::SmallButton("^")) { moveRow = static_cast<int>(ri); moveDir = -1; }
                            ImGui::SameLine();
                            if (UI::SmallButton("v")) { moveRow = static_cast<int>(ri); moveDir = 1; }
                            ImGui::SameLine();
                            if (state.shopDeleteArmed && UI::SmallButton("x")) removeRow = static_cast<int>(ri);
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                        if (moveRow >= 0) {
                            const int other = moveRow + moveDir;
                            if (other >= 0 && other < static_cast<int>(table.records.size())) {
                                std::swap(table.records[static_cast<std::size_t>(moveRow)], table.records[static_cast<std::size_t>(other)]);
                                structureChanged = true;
                            }
                        }
                        if (removeRow >= 0) { table.records.erase(table.records.begin() + removeRow); structureChanged = true; }
                    }
                    if (UI::Button(L("+ Zeile","+ Row"))) {
                        table.records.push_back(MakeEmptyShopRow(table));
                        structureChanged = true;
                    }
                    if (state.shopDeleteArmed && file.tables.size() > 1) {
                        ImGui::SameLine();
                        if (UI::Button(L("Diesen Tab löschen","Delete this tab")))
                            deleteTable = static_cast<int>(t);
                    }
                    if (structureChanged) RenumberShopRows(table);
                    ImGui::EndTabItem();
                }
                if (UI::TabItemButton(L("+ Tab","+ Tab"), ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                    core::legacy::ShineTable nt;
                    int maxNo = -1;
                    for (const auto& t : file.tables) {
                        if (t.name.size() > 3 && t.name.compare(0, 3, "Tab") == 0) maxNo = std::max(maxNo, std::atoi(t.name.c_str() + 3));
                    }
                    char nm[16];
                    std::snprintf(nm, sizeof(nm), "Tab%02d", maxNo + 1);
                    nt.name = nm;
                    nt.isNew = true;
                    nt.columns = file.tables.empty() ? std::vector<core::legacy::ShineColumn>{} : file.tables.front().columns;
                    if (nt.columns.empty()) {
                        nt.columns.push_back({"Rec", "BYTE"});
                        for (int c = 0; c < 6; ++c) { char cn[16]; std::snprintf(cn, sizeof(cn), "Column%02d", c); nt.columns.push_back({cn, "String[33]"}); }
                    }
                    nt.records.push_back(MakeEmptyShopRow(nt));
                    RenumberShopRows(nt);
                    file.tables.push_back(std::move(nt));
                }
                ImGui::EndTabBar();
            }
            if (deleteTable >= 0) { file.tables.erase(file.tables.begin() + deleteTable); state.shopDeleteArmed = false; }

            // Item-Auswahl (Popup): Suche in InxName und Anzeigename.
            const char* itemPickerTitle =
                L("Item wählen###shopItemPicker","Choose item###shopItemPicker");
            if (state.shopPickRequested) {
                ImGui::OpenPopup(itemPickerTitle);
                state.shopPickRequested = false;
            }
            if (ImGui::BeginPopup(itemPickerTitle)) {
                const bool focusItemSearch = ImGui::IsWindowAppearing();
                DrawSearchInput("shopItemFilter",
                                L("Suche (Name oder Item-Bezeichnung)",
                                  "Search (name or item description)"),
                                state.shopItemFilter, sizeof(state.shopItemFilter),
                                490.0f, focusItemSearch);
                const std::string needle = LowerAscii(state.shopItemFilter);
                ImGui::BeginChild("##itemlist", ImVec2(520.0f, 320.0f), true);
                if (UI::Selectable(L("(leer) -","(empty) -"))) {
                    if (state.shopPickTable >= 0 && state.shopPickTable < static_cast<int>(file.tables.size())) {
                        auto& tb = file.tables[static_cast<std::size_t>(state.shopPickTable)];
                        if (state.shopPickRow >= 0 && state.shopPickRow < static_cast<int>(tb.records.size()))
                            tb.records[static_cast<std::size_t>(state.shopPickRow)].values[static_cast<std::size_t>(state.shopPickCol)] = "-";
                    }
                    ImGui::CloseCurrentPopup();
                }
                int shown = 0;
                for (const auto& e : state.itemEntries) {
                    if (!needle.empty() && LowerAscii(e.inx).find(needle) == std::string::npos && LowerAscii(e.name).find(needle) == std::string::npos) continue;
                    if (++shown > 300) {
                        ImGui::TextDisabled("%s",L(
                            "... weitere Treffer - Suche eingrenzen",
                            "... more matches - narrow the search"));
                        break;
                    }
                    const std::string label = e.inx + "   " + e.name + "   (#" + std::to_string(e.id) + ")##" + std::to_string(shown);
                    if (UI::Selectable(label.c_str())) {
                        if (state.shopPickTable >= 0 && state.shopPickTable < static_cast<int>(file.tables.size())) {
                            auto& tb = file.tables[static_cast<std::size_t>(state.shopPickTable)];
                            if (state.shopPickRow >= 0 && state.shopPickRow < static_cast<int>(tb.records.size()))
                                tb.records[static_cast<std::size_t>(state.shopPickRow)].values[static_cast<std::size_t>(state.shopPickCol)] = e.inx;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndChild();
                if (state.itemEntries.empty()) {
                    ImGui::TextDisabled("%s",L(
                        "ItemInfo.shn nicht geladen - Item-Bezeichnung von Hand eingeben ist im Raster nicht möglich.",
                        "ItemInfo.shn is not loaded - manual item entry is not available in the grid."));
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();
            UI::Checkbox(L("Löschen freigeben","Enable delete"), &state.shopDeleteArmed);
            ImGui::SameLine();
            if (UI::Button(L("Speichern","Save"))) {
                auto saved = SaveProjectShineOverride(
                    state.shopTextFile,
                    ProjectServerShinePath(
                        state.project,
                        std::filesystem::path("NPCItemList") / (state.shopLoadedForNpc + ".txt")));
                if (saved) {
                    state.statusMessage =
                        L("Händler-Inventar als Projekt-Override gespeichert: ","Shop inventory saved as project override: ") +
                        state.shopLoadedForNpc;
                    // Neu laden: Tabellen/Records bekommen ihre echten Quellzeilen, "neu"-Markierungen entfallen.
                    const std::string npc = state.shopLoadedForNpc;
                    state.shopTextLoaded = false;
                    state.shopFileIsNew = false;
                    EnsureShopTextLoaded(state, npc);
                } else {
                    state.statusMessage = L("Fehler: ","Error: ") + saved.error();
                }
            }
        }
        ImGui::SameLine();
        if (UI::Button(L("Schließen","Close"))) {
            state.shopEditorOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
// ============================================================================
// Custom NPC / Mob Assistent (SHN-Editor-Tab "Custom NPC/Mob") - CHANGELOG [0.44.28].
//
// Datengrundlage: nur die verifizierte Mob-ID-Pflichtfamilie wird automatisch geklont:
// MobInfo (Client+Server), MobInfoServer und MobViewInfo (Client+Server). Tabellen wie
// MobSpecies, QuestSpecies und MobWeapon sind laut docs/DATA_DEPENDENCY_MATRIX.md nur BEDINGT
// und werden deshalb NICHT pauschal mutiert. NPCs koennen zusaetzlich NpcDialogData (Dialog),
// World/NPC.txt (Platzierung/Rolle) und NPCViewInfo (Avatar-Look) benoetigen - jeweils nur wenn
// der entsprechende Wizard-Schalter/Funktionspfad verwendet wird.
// ============================================================================
// Generische Auswahl-Liste im Popup (Suche + Liste), fuer Item-Namen und Modell-Dateinamen.
bool StringPickerPopup(const char* popupId, const std::vector<std::string>& options, char* filterBuf, std::size_t filterSize,
                              std::string& chosen, const std::vector<std::string>* labels) {
    bool picked = false;
    if (ImGui::BeginPopup(popupId)) {
        // Popups sind AlwaysAutoResize: eine relative Kindhoehe (0/-x) schrumpfte das Fenster auf ein
        // Minimum ("minimiert"). Deshalb FESTE Breite/Hoehe (CHANGELOG [0.44.29]).
        const bool focusPickerSearch = ImGui::IsWindowAppearing();
        DrawSearchInput("stringPickerFilter",L("Suche","Search"),
                        filterBuf,filterSize,490.0f,focusPickerSearch);
        const std::string needle = LowerAscii(filterBuf);
        ImGui::BeginChild("##picklist", ImVec2(520.0f, 320.0f), true);
        if (UI::Selectable("(leer) -")) { chosen = "-"; picked = true; ImGui::CloseCurrentPopup(); }
        int shown = 0;
        for (std::size_t i = 0; i < options.size(); ++i) {
            const std::string& label = labels ? (*labels)[i] : options[i];
            if (!needle.empty() && LowerAscii(label).find(needle) == std::string::npos) continue;
            if (++shown > 300) { ImGui::TextDisabled("... weitere Treffer - Suche eingrenzen"); break; }
            if (UI::Selectable((label + "##opt" + std::to_string(i)).c_str())) { chosen = options[i]; picked = true; ImGui::CloseCurrentPopup(); }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    return picked;
}
// Klont die Vorlage-Zeilen (ID + Name) einer Tabelle unter neuer ID/neuem Namen ans Ende. Liefert
// die Indizes der neuen Zeilen (leer, wenn Datei fehlt oder keine Vorlage-Zeile existiert).
std::vector<std::size_t> CloneRowsById(EditorState& state, const char* fileName, EditorState::ShnSource src,
                                              const char* nameCol, long long templateId, const std::string& templateInx,
                                              long long newId, const std::string& newInx, std::vector<std::string>& report) {
    std::vector<std::size_t> created;
    const int di = FindOrLoadShnDoc(state, fileName, src);
    const std::string label = std::string(ShnSourceName(src)) + " " + fileName;
    if (di < 0) { report.push_back("- " + label + ": nicht geladen (übersprungen)"); return created; }
    auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
    auto& f = doc.file;
    const int cId = ShnColumnIndexByName(f, "ID");
    const int cName = ShnColumnIndexByName(f, nameCol);
    if (cId < 0 || cName < 0) { report.push_back("- " + label + ": Spalten ID/" + nameCol + " fehlen (übersprungen)"); return created; }
    std::vector<std::size_t> sources;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        long long id = -1;
        if (!ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(cId)], id) || id != templateId) continue;
        if (core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cName)]) != templateInx) continue;
        sources.push_back(r);
    }
    for (const std::size_t r : sources) {
        core::legacy::ShnRow row = f.rows[r];
        f.rows.push_back(std::move(row));
        const std::size_t nr = f.rows.size() - 1;
        SetShnCellText(f, nr, "ID", std::to_string(newId));
        SetShnCellText(f, nr, nameCol, newInx);
        created.push_back(nr);
    }
    if (!created.empty()) {
        doc.dirty = true;
        EnsureCellStatusSize(doc);
        // Eine neu geklonte Zeile ist vollständig ungespeichert. Alle Zellen markieren,
        // damit Rohgrid und semantische Editoren denselben Dirty-Zustand anzeigen.
        for (const std::size_t nr : created) {
            if (nr < doc.cellDirty.size())
                std::fill(doc.cellDirty[nr].begin(),doc.cellDirty[nr].end(),1);
        }
        ++state.shnEditCounter;
        std::vector<std::pair<int, int>> inserted;
        for (const std::size_t nr : created) inserted.emplace_back(di, static_cast<int>(nr));
        RecordRowsInserted(state, label + ": " + newInx + " angelegt", inserted);
        report.push_back("+ " + label + ": " + std::to_string(created.size()) + " Zeile(n) angelegt");
    } else {
        report.push_back("- " + label + ": keine Vorlage-Zeile gefunden (übersprungen)");
    }
    return created;
}

} // namespace nge
