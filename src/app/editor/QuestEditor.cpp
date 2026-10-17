// QuestEditor.cpp
// NPC-/Quest-Texte, Dialoge, Quest-Ablauf und Quest-Editor.

#include "EditorShared.hpp"

#include "mapeditor/core/legacy/QuestText.hpp"

namespace nge {

// Werkzeug-Inhalt für den aktuell aktiven Modus (siehe EditMode) - der Moduswechsel selbst
// erfolgt jetzt über den Tab-Balken des Arbeitsbereichs (siehe DrawWorkspaceTabBar), nicht
// mehr über Radio-Buttons hier. Wird OHNE eigenes Fenster aufgerufen (eingebettet in die
// "Tools/etc"-Spalte, siehe DrawMapEditorWorkspace) - daher kein ImGui::Begin/End mehr.
// Lädt World/NPC.txt (alle Karten in einer Datei) einmalig aus state.shineTextRoot - dieselbe
// Datei bleibt für die ganze Sitzung geladen, gefiltert wird erst bei der Anzeige (siehe
// NpcRecordsForCurrentMap). Speichern erfolgt minimal-invasiv über SaveShineTextFile.
void EnsureNpcTextLoaded(EditorState& state) {
    if (state.npcTextLoaded || state.shineTextRoot.empty()) return;
    const auto sourcePath = std::filesystem::path(state.shineTextRoot) / "World" / "NPC.txt";
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) { state.npcTextFile = std::move(*result); state.npcTextLoaded = true; }
    else state.statusMessage = "NPC.txt: " + result.error();
}
bool SameMapName(const std::string& a, const std::string& b) {
    return LowerAscii(TrimAscii(a)) == LowerAscii(TrimAscii(b));
}
std::vector<std::size_t> NpcRecordsForCurrentMap(EditorState& state) {
    std::vector<std::size_t> out;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return out;
    for (std::size_t i = 0; i < table->records.size(); ++i) {
        if (table->records[i].values.size() > 1 && SameMapName(table->records[i].values[1], state.legacySaveStem)) {
            out.push_back(i);
        }
    }
    return out;
}
// Lädt MobRegen/<Karte>.txt neu, wenn die Karte gewechselt hat (Dateiname = Kartenname, 1:1,
// anders als bei NPC.txt wo alle Karten in einer Datei stehen).
void EnsureMobRegenLoaded(EditorState& state) {
    if (state.shineTextRoot.empty()) return;
    if (state.mobRegenTextLoaded && state.mobRegenLoadedForMap == state.legacySaveStem) return;
    const std::filesystem::path regenDir = std::filesystem::path(state.shineTextRoot) / "MobRegen";
    const std::string regenFile = CurrentMapServerName(state) + ".txt";
    const auto sourcePath = core::legacy::ResolveCaseInsensitivePath(regenDir, regenFile).value_or(regenDir / regenFile);
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) {
        state.mobRegenTextFile = std::move(*result);
        state.mobRegenTextLoaded = true;
        state.mobRegenLoadedForMap = state.legacySaveStem;
    } else {
        state.mobRegenTextLoaded = false;
        state.statusMessage = std::string("MobRegen/") + state.legacySaveStem + ".txt: " + result.error();
    }
}
void EnsureShopTextLoaded(EditorState& state, const std::string& npcName) {
    if (state.shopTextLoaded && state.shopLoadedForNpc == npcName) return;
    const auto sourcePath = std::filesystem::path(state.shineTextRoot) / "NPCItemList" / (npcName + ".txt");
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    state.shopFileIsNew = false;
    state.shopLoadedForNpc = npcName;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        // Noch keine Shop-Datei - der Editor bietet an, eine anzulegen.
        state.shopTextLoaded = false;
        return;
    }
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) {
        state.shopTextFile = std::move(*result);
        state.shopTextLoaded = true;
    } else {
        state.shopTextLoaded = false;
        state.statusMessage = "NPCItemList/" + npcName + ".txt: " + result.error();
    }
}
std::vector<std::string> ExtractBracketed(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '[') {
            std::size_t end = s.find(']', i);
            if (end == std::string::npos) break;
            out.push_back(s.substr(i + 1, end - i - 1));
            i = end + 1;
        } else {
            ++i;
        }
    }
    return out;
}
ParsedNpcDialog ParseNpcDialogText(const std::string& raw) {
    ParsedNpcDialog out;
    std::istringstream iss(raw);
    std::string line;
    std::vector<std::string> greetingLines;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("[BUTTON_NPC]=", 0) == 0) {
            auto parts = ExtractBracketed(line.substr(13));
            if (parts.size() >= 2) out.buttons.emplace_back(parts[0], parts[1]);
            else if (parts.size() == 1) out.buttons.emplace_back("", parts[0]); // nur Aktion, kein Label (Client zeigt vermutlich einen Standard-Button wie "Weiter") - byte-exakt gegen echte Daten verifiziert (z.B. RouGaianMaria)
        } else {
            greetingLines.push_back(line);
        }
    }
    while (!greetingLines.empty() && greetingLines.back().empty()) greetingLines.pop_back();
    for (std::size_t i = 0; i < greetingLines.size(); ++i) {
        if (i) out.greeting += "\n";
        out.greeting += greetingLines[i];
    }
    return out;
}
std::string SerializeNpcDialogText(const ParsedNpcDialog& d) {
    std::string out = d.greeting;
    for (auto& [label, action] : d.buttons) {
        if (label.empty()) out += "\n[BUTTON_NPC]=[" + action + "]"; // siehe ParseNpcDialogText
        else out += "\n[BUTTON_NPC]=[" + label + "][" + action + "]";
    }
    return out;
}
// Sucht den ressystem-Ordner (enthält NpcDialogData.shn) unter dem Projekt-Client-Ordner, falls
// noch nicht gesetzt - derselbe Ordner, den auch der SHN-Editor für Client-Dateien nutzen würde,
// hier aber unabhängig ermittelt (siehe EditorState::npcDialogRessystemRoot).
void EnsureNpcDialogRoot(EditorState& state) {
    if (!state.npcDialogRessystemRoot.empty()) return;
    if (auto found = FindNamedSubfolder(state.project.clientFolder, {"ressystem"})) {
        state.npcDialogRessystemRoot = found->string();
    }
}
// Lädt QuestData.shn (Server) einmalig aus state.shnServerRoot - eigenes Format, siehe
// QuestData.hpp/CHANGELOS [0.44.19].
void EnsureQuestDataLoaded(EditorState& state) {
    if (state.questDataLoaded || state.shnServerRoot.empty()) return;
    auto path = std::filesystem::path(state.shnServerRoot) / "QuestData.shn";
    {
        // Ordner war evtl. ein Elternordner von Shine (z.B. Server/9Data) - dann rekursiv suchen.
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (auto shine = FindServerShineRoot(state.shnServerRoot)) path = *shine / "QuestData.shn";
        }
    }
    path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, path);
    auto result = core::legacy::LoadQuestData(path);
    if (result) { state.questDataFile = std::move(*result); state.questDataLoaded = true; }
    else state.statusMessage = "QuestData.shn: " + result.error();
}
// Lädt QuestDialog.shn (Client, ressystem/) zur Auflösung der Text-IDs in title/description
// und der SAY-Referenzen in den Skripten - selber Ordner wie NpcDialogData.shn.
void EnsureQuestDialogLoaded(EditorState& state) {
    if (state.questDialogLoaded) return;
    EnsureNpcDialogRoot(state);
    if (state.npcDialogRessystemRoot.empty()) return;
    state.questDialogDoc = SharedShnDoc(state, "QuestDialog.shn", EditorState::ShnSource::Client,
                                        state.npcDialogRessystemRoot);
    state.questDialogLoaded = state.questDialogDoc >= 0;
    state.questTextMapBuilt = false;
}
// Löst eine Text-ID über QuestDialog.shn auf (Spalte 0 = ID, Spalte 1 = Dialog) - byte-exakt
// gegen echte Daten geprüft, siehe CHANGELOG [0.44.19] (z.B. ID 200 = Quest-1-Titel "Baby
// Steps", ID 202 = die zugehörige NPC-Begrüßungszeile mit [BUTTON]=[...]-Markup).
std::string ResolveQuestText(EditorState& state, int textId) {
    if (!state.questDialogLoaded) return {};
    for (auto& row : ShnDocFile(state, state.questDialogDoc).rows) {
        if (row.values.empty()) continue;
        long long id = 0;
        if (!ShnValueAsInt(row.values[0], id)) continue;
        if (id == textId) return row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : std::string();
    }
    return {};
}
// Lädt ItemInfo.shn (Server bevorzugt, sonst Client) für die Item-Namensauflösung im Quest-
// Editor - siehe CHANGELOG [0.44.21].
void EnsureItemInfoLoadedForQuests(EditorState& state) {
    if (state.itemInfoLoaded) return;
    for (auto src : {EditorState::ShnSource::Server, EditorState::ShnSource::Client}) {
        const std::string& root = src == EditorState::ShnSource::Client ? state.shnClientRoot : state.shnServerRoot;
        if (root.empty()) continue;
        state.itemInfoDoc = SharedShnDoc(state, "ItemInfo.shn", src, root);
        if (state.itemInfoDoc >= 0) { state.itemInfoLoaded = true; return; }
    }
}
// Liefert "<InxName> (#<ID>)" wenn gefunden, sonst "?? ID <id> nicht gefunden" (rot markiert
// vom Aufrufer) - dient zugleich als einfache Validierung von Item-Referenzen. id==0 gilt als
// "kein Item" (nicht als Fehler gewertet), da 0 in den Quest-Feldern durchgängig "leer/
// deaktiviert" bedeutet.
std::pair<std::string, bool> ResolveItemNameForQuest(EditorState& state, int id) {
    if (id == 0) return {"-", true};
    EnsureItemInfoLoadedForQuests(state);
    if (state.itemInfoLoaded) {
        for (auto& row : ShnDocFile(state, state.itemInfoDoc).rows) {
            if (row.values.empty()) continue;
            long long rid = 0;
            if (ShnValueAsInt(row.values[0], rid) && rid == id) {
                std::string name = row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : "?";
                return {name, true};
            }
        }
    }
    return {L("nicht gefunden", "not found"), false};
}
// Dito für Mob-/NPC-IDs über das ohnehin für die NPC-3D-Auflösung geladene MobViewInfo.shn
// (siehe EnsureMobViewInfoLoaded) - selbe ID-Basis für Monster UND benannte NPCs.
std::pair<std::string, bool> ResolveMobNameForQuest(EditorState& state, int id) {
    if (id == 0) return {"-", true};
    EnsureMobViewInfoLoaded(state);
    if (state.mobViewInfoLoaded) {
        for (auto& row : ShnDocFile(state, state.mobViewInfoDoc).rows) {
            if (row.values.empty()) continue;
            long long rid = 0;
            if (ShnValueAsInt(row.values[0], rid) && rid == id) {
                std::string name = row.values.size() > 1 ? core::legacy::ShnValueToString(row.values[1]) : "?";
                return {name, true};
            }
        }
    }
    return {L("nicht gefunden", "not found"), false};
}
// Zeigt eine ID + ihren aufgelösten Namen (grün=gefunden, rot=nicht gefunden) direkt hinter
// einem InputInt - siehe CHANGELOG [0.44.21] ("Validierung" + "echten Namen zeigen" in einem).
void DrawResolvedIdLabel(const std::pair<std::string, bool>& resolved) {
    ImGui::SameLine();
    if (resolved.second) ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "-> %s", resolved.first.c_str());
    else ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "-> %s", resolved.first.c_str());
}
// Text zu einer Quest-Text-ID; leer, wenn unbekannt oder nur ein Platzhalter aus Strichen ("-----").
std::string QuestTextOf(EditorState& state, int textId) {
    if (!state.questDialogLoaded) return {};
    if (!state.questTextMapBuilt || state.questTextMapRevision != state.shnEditCounter) {
        state.questTextMapBuilt = true;
        state.questTextMapRevision = state.shnEditCounter;
        state.questTextMap.clear();
        state.questTextRow.clear();
        const auto& rows = ShnDocFile(state, state.questDialogDoc).rows;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto& row = rows[r];
            if (row.values.size() < 2) continue;
            long long id = 0;
            if (!ShnValueAsInt(row.values[0], id)) continue;
            state.questTextMap.emplace(static_cast<int>(id), core::legacy::ShnValueToString(row.values[1]));
            state.questTextRow.emplace(static_cast<int>(id), static_cast<int>(r));
        }
    }
    const auto it = state.questTextMap.find(textId);
    if (it == state.questTextMap.end()) return {};
    const std::string& t = it->second;
    if (t.find_first_not_of("- \t") == std::string::npos) return {}; // nur Striche/Leerraum
    return t;
}
void DrawQuestFlowView(EditorState& state,
                       const std::vector<core::legacy::QuestRecord>& quests,
                       std::size_t currentIndex) {
    if (currentIndex >= quests.size()) return;

    // Only the explicitly decoded QuestData fields needPred/predecessor create graph edges.
    // Script GOTO/ACCEPT text is intentionally NOT interpreted as a quest relationship.
    std::unordered_map<std::uint16_t,std::size_t> byId;
    std::unordered_set<std::uint16_t> duplicateIds;
    byId.reserve(quests.size());
    for (std::size_t i=0;i<quests.size();++i) {
        const auto [it,inserted]=byId.emplace(quests[i].id,i);
        if (!inserted) duplicateIds.insert(quests[i].id);
    }

    const auto titleOf=[&](const core::legacy::QuestRecord& quest) {
        std::string title=QuestTextOf(state,quest.title);
        if (title.empty()) title=QuestTextOf(state,quest.description);
        if (title.empty()) title=L("(ohne Text)","(no text)");
        return title;
    };
    const auto questButton=[&](const char* prefix,std::size_t index,const char* suffix=nullptr) {
        if (index>=quests.size()) return false;
        const auto& quest=quests[index];
        std::string label=std::string(prefix)+" #"+std::to_string(quest.id)+"  "+titleOf(quest);
        if (suffix && *suffix) label+="  "+std::string(suffix);
        return UI::Button((label+"##flowQuest"+std::to_string(index)).c_str(),ImVec2(-1.0f,0.0f));
    };

    const auto& current=quests[currentIndex];
    std::vector<std::size_t> ancestors;
    std::unordered_set<std::uint16_t> chainSeen;
    chainSeen.insert(current.id);
    bool cycle=false;
    std::uint16_t missingPredecessor=0;
    std::size_t cursor=currentIndex;
    for (int depth=0;depth<12;++depth) {
        const auto& q=quests[cursor];
        if (q.needPred==0 || q.predecessor==0) break;
        if (chainSeen.contains(q.predecessor)) {
            cycle=true;
            break;
        }
        chainSeen.insert(q.predecessor);
        const auto it=byId.find(q.predecessor);
        if (it==byId.end() || duplicateIds.contains(q.predecessor)) {
            missingPredecessor=q.predecessor;
            break;
        }
        ancestors.push_back(it->second);
        cursor=it->second;
    }
    std::reverse(ancestors.begin(),ancestors.end());

    std::vector<std::size_t> successors;
    for (std::size_t i=0;i<quests.size();++i) {
        if (i==currentIndex) continue;
        if (quests[i].needPred!=0 && quests[i].predecessor==current.id)
            successors.push_back(i);
    }

    DrawPanelHeader("questFlowHeader", L("QUEST FLOW","QUEST FLOW"),
                    DrawIconBook, "module.quest",
                    L("nur verifizierte predecessor-Beziehungen",
                      "verified predecessor relationships only"));
    if (duplicateIds.contains(current.id)) {
        ImGui::TextColored(UiTheme::Error,"%s",
            L("Diese Quest-ID kommt mehrfach vor; eingehende/ausgehende Links sind dadurch mehrdeutig.",
              "This quest ID occurs more than once; incoming/outgoing links are ambiguous."));
    }

    const float avail=ImGui::GetContentRegionAvail().x;
    if (!ImGui::BeginTable("##questFlowLayout",3,
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV)) return;
    ImGui::TableSetupColumn("##ancestors",ImGuiTableColumnFlags_WidthStretch,0.31f);
    ImGui::TableSetupColumn("##current",ImGuiTableColumnFlags_WidthStretch,0.38f);
    ImGui::TableSetupColumn("##successors",ImGuiTableColumnFlags_WidthStretch,0.31f);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(UiTheme::TextSecondary,"%s",L("VORGÄNGER-KETTE","PREDECESSOR CHAIN"));
    ImGui::Separator();
    if (ancestors.empty() && missingPredecessor==0 && !cycle) {
        ImGui::TextDisabled("%s",L("Kein Vorgänger.","No predecessor."));
    } else {
        const std::size_t first=ancestors.size()>6 ? ancestors.size()-6 : 0;
        if (first>0) ImGui::TextDisabled(L("... %zu frühere Quest(s)","... %zu earlier quest(s)"),first);
        for (std::size_t ai=first;ai<ancestors.size();++ai) {
            const std::size_t idx=ancestors[ai];
            if (questButton("<",idx)) state.selectedQuestIdx=static_cast<int>(idx);
            if (ai+1<ancestors.size()) {
                const float x=ImGui::GetCursorPosX()+18.0f;
                ImGui::SetCursorPosX(x);
                ImGui::TextColored(UiTheme::TextSecondary,"v");
            }
        }
        if (missingPredecessor!=0)
            ImGui::TextColored(UiTheme::Error,L("× Vorgänger #%u fehlt oder ist mehrdeutig",
                                                "× predecessor #%u missing or ambiguous"),
                               static_cast<unsigned>(missingPredecessor));
        if (cycle)
            ImGui::TextColored(UiTheme::Error,"%s",
                L("× Zyklus in der predecessor-Kette erkannt","× cycle detected in predecessor chain"));
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::TextColored(UiTheme::AccentCyan,"%s",L("AKTUELLE QUEST","CURRENT QUEST"));
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,IM_COL32(8,29,45,220));
    ImGui::BeginChild("##questFlowCurrent",ImVec2(0.0f,0.0f),true);
    ImGui::TextColored(UiTheme::AccentCyan,"#%u",static_cast<unsigned>(current.id));
    ImGui::TextWrapped("%s",titleOf(current).c_str());
    ImGui::Separator();

    if (current.needLevel!=0)
        ImGui::Text(L("Level: %u-%u","Level: %u-%u"),
                    static_cast<unsigned>(current.minLevel),static_cast<unsigned>(current.maxLevel));
    else
        ImGui::TextDisabled("%s",L("Keine Level-Bedingung","No level requirement"));

    if (current.needNpc!=0 && current.startingNpc!=0) {
        const auto npc=ResolveMobNameForQuest(state,current.startingNpc);
        ImGui::TextColored(npc.second?UiTheme::Success:UiTheme::Error,
            L("Start-NPC: #%u · %s","Start NPC: #%u · %s"),
            static_cast<unsigned>(current.startingNpc),npc.first.c_str());
    } else {
        ImGui::TextDisabled("%s",L("Kein fester Start-NPC","No fixed starting NPC"));
    }

    if (current.needItem!=0 && current.itemId!=0) {
        const auto item=ResolveItemNameForQuest(state,current.itemId);
        ImGui::TextColored(item.second?UiTheme::Success:UiTheme::Error,
            L("Voraussetzungs-Item: #%u · %s","Required item: #%u · %s"),
            static_cast<unsigned>(current.itemId),item.first.c_str());
    }

    int mobObjectives=0,itemObjectives=0,activeDrops=0;
    for (const auto& m:current.mobs) if (m.active!=0) ++mobObjectives;
    for (const auto& item:current.items) if (item.active!=0) ++itemObjectives;
    for (const auto& drop:current.drops) if (drop.active!=0) ++activeDrops;
    ImGui::SeparatorText(L("Ziele","Objectives"));
    ImGui::Text(L("%d Monster · %d Items · %d Drops",
                  "%d monsters · %d items · %d drops"),
                mobObjectives,itemObjectives,activeDrops);

    ImGui::SeparatorText(L("Flags","Flags"));
    ImGui::Text("%s%s%s",
        current.enableQuest!=0?L("Aktiv","Enabled"):L("Deaktiviert","Disabled"),
        current.dailyQuest!=0?L(" · Täglich"," · Daily"):"",
        current.multiQuest!=0?L(" · Multi"," · Multi"):"");

    ImGui::SeparatorText(L("Scripts","Scripts"));
    ImGui::TextDisabled("Start %s · Action %s · Finish %s",
        current.start.text.empty()?"-":"OK",
        current.action.text.empty()?"-":"OK",
        current.finish.text.empty()?"-":"OK");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::TableSetColumnIndex(2);
    ImGui::TextColored(UiTheme::TextSecondary,"%s",L("DIREKTE FOLGEQUESTS","DIRECT SUCCESSORS"));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu",successors.size());
    ImGui::Separator();
    if (successors.empty()) {
        ImGui::TextDisabled("%s",L("Keine Quest verweist direkt auf diese ID.",
                                   "No quest directly references this ID."));
    } else {
        ImGui::BeginChild("##questFlowSuccessors",ImVec2(0.0f,0.0f),false);
        for (const std::size_t idx:successors) {
            if (duplicateIds.contains(quests[idx].id)) ImGui::PushStyleColor(ImGuiCol_Button,IM_COL32(112,70,24,210));
            if (questButton(">",idx,duplicateIds.contains(quests[idx].id)?L("! ID mehrfach","! duplicate ID"):nullptr))
                state.selectedQuestIdx=static_cast<int>(idx);
            if (duplicateIds.contains(quests[idx].id)) ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    ImGui::EndTable();

    ImGui::Separator();
    ImGui::TextDisabled("%s",
        L("Der Flow ist absichtlich read-only. Kanten stammen ausschließlich aus needPred/predecessor; "
          "Skriptbefehle und unbekannte Raw-/Reward-Felder werden nicht als Graph-Semantik interpretiert.",
          "The flow is intentionally read-only. Edges come only from needPred/predecessor; "
          "script commands and unknown raw/reward fields are not interpreted as graph semantics."));
    (void)avail;
}
// Existiert path als reguläre Datei? Eine fehlende Datei ist kein Fehler - libstdc++ (Linux,
// MinGW) setzt bei is_regular_file(p, ec) für ENOENT trotzdem ec, MSVC nicht.
std::expected<bool, std::string> RegularFileState(const std::filesystem::path& path) {
    std::error_code ec;
    const auto status = std::filesystem::status(path, ec);
    if (status.type() == std::filesystem::file_type::not_found) return false;
    if (ec) return std::unexpected(ec.message());
    return status.type() == std::filesystem::file_type::regular;
}
bool SaveQuestDataProject(EditorState& state) {
    std::filesystem::path clientPath, serverPath;
    std::string error;
    if (!PrepareProjectOutput(
            ProjectClientRessystemPath(state.project, "QuestData.shn"),
            clientPath, &error)) {
        state.statusMessage = "QuestData.shn Client-Speichern blockiert: " + error;
        return false;
    }
    if (!PrepareProjectOutput(
            ProjectServerShinePath(state.project, "QuestData.shn"),
            serverPath, &error)) {
        state.statusMessage = "QuestData.shn Server-Speichern blockiert: " + error;
        return false;
    }

    struct StagedQuestCopy {
        std::filesystem::path finalPath;
        std::filesystem::path tempPath;
        std::filesystem::path backupPath;
        bool hadOriginal = false;
        bool committed = false;
    };
    auto makeStage = [](const std::filesystem::path& finalPath) {
        StagedQuestCopy out;
        out.finalPath = finalPath;
        out.tempPath = finalPath;
        out.tempPath += ".nextgen-tmp";
        out.backupPath = finalPath;
        out.backupPath += ".nextgen-bak";
        return out;
    };
    auto client = makeStage(clientPath);
    auto server = makeStage(serverPath);

    auto cleanupTemp = [](const StagedQuestCopy& item) {
        std::error_code ec;
        std::filesystem::remove(item.tempPath, ec);
    };
    auto resetStaging = [&](const StagedQuestCopy& item) -> std::expected<void,std::string> {
        cleanupTemp(item);
        std::error_code ec;
        const auto backupState = RegularFileState(item.backupPath);
        if (!backupState) return std::unexpected("Backup-Status konnte nicht gelesen werden: " + backupState.error());
        const auto finalState = RegularFileState(item.finalPath);
        if (!finalState) return std::unexpected("Ziel-Status konnte nicht gelesen werden: " + finalState.error());
        const bool backupExists = *backupState;
        const bool finalExists = *finalState;
        if (!backupExists) return {};
        if (finalExists) {
            std::filesystem::remove(item.backupPath, ec);
            if (ec) return std::unexpected("Veraltetes Backup konnte nicht entfernt werden: " + ec.message());
            return {};
        }
        std::filesystem::rename(item.backupPath, item.finalPath, ec);
        if (ec) return std::unexpected(
            "Vorheriges Projekt-Backup konnte nicht wiederhergestellt werden: " + ec.message() +
            " (" + item.backupPath.string() + ")");
        return {};
    };
    if (auto recovered = resetStaging(client); !recovered) {
        state.statusMessage = "QuestData.shn Client-Recovery: " + recovered.error();
        return false;
    }
    if (auto recovered = resetStaging(server); !recovered) {
        state.statusMessage = "QuestData.shn Server-Recovery: " + recovered.error();
        return false;
    }

    auto clientStaged = core::legacy::SaveQuestData(state.questDataFile, client.tempPath);
    if (!clientStaged) {
        cleanupTemp(client);
        cleanupTemp(server);
        state.statusMessage = "QuestData.shn Client-Staging: " + clientStaged.error();
        return false;
    }
    auto serverStaged = core::legacy::SaveQuestData(state.questDataFile, server.tempPath);
    if (!serverStaged) {
        cleanupTemp(client);
        cleanupTemp(server);
        state.statusMessage = "QuestData.shn Server-Staging: " + serverStaged.error();
        return false;
    }

    auto commitStage = [](StagedQuestCopy& item) -> std::expected<void,std::string> {
        std::error_code ec;
        std::filesystem::remove(item.backupPath, ec);
        ec.clear();
        const auto finalState = RegularFileState(item.finalPath);
        if (!finalState) return std::unexpected("Status des Projektziels konnte nicht gelesen werden: " + finalState.error());
        item.hadOriginal = *finalState;
        if (item.hadOriginal) {
            std::filesystem::rename(item.finalPath, item.backupPath, ec);
            if (ec) return std::unexpected("Bestehender Projektstand konnte nicht gesichert werden: " + ec.message());
        }
        std::filesystem::rename(item.tempPath, item.finalPath, ec);
        if (ec) {
            const std::string message = "Staging-Datei konnte nicht aktiviert werden: " + ec.message();
            if (item.hadOriginal) {
                std::error_code restoreEc;
                std::filesystem::rename(item.backupPath, item.finalPath, restoreEc);
                if (restoreEc)
                    return std::unexpected(message + " | Backup-Restore fehlgeschlagen: " + restoreEc.message() +
                                           " (Backup bleibt erhalten: " + item.backupPath.string() + ")");
            }
            return std::unexpected(message);
        }
        item.committed = true;
        return {};
    };
    auto rollbackStage = [](StagedQuestCopy& item) -> std::string {
        if (!item.committed) return {};
        std::error_code ec;
        std::filesystem::remove(item.finalPath, ec);
        ec.clear();
        if (item.hadOriginal) {
            std::filesystem::rename(item.backupPath, item.finalPath, ec);
            if (ec) return "Rollback fehlgeschlagen: " + ec.message();
        }
        item.committed = false;
        return {};
    };

    auto clientCommit = commitStage(client);
    if (!clientCommit) {
        cleanupTemp(client);
        cleanupTemp(server);
        state.statusMessage = "QuestData.shn Client-Commit: " + clientCommit.error();
        return false;
    }
    auto serverCommit = commitStage(server);
    if (!serverCommit) {
        const std::string rollbackError = rollbackStage(client);
        cleanupTemp(client);
        cleanupTemp(server);
        state.statusMessage = "QuestData.shn Server-Commit: " + serverCommit.error();
        if (!rollbackError.empty()) state.statusMessage += " | " + rollbackError;
        return false;
    }

    std::error_code ec;
    std::filesystem::remove(client.backupPath, ec);
    ec.clear();
    std::filesystem::remove(server.backupPath, ec);
    state.questDirty = false;
    state.statusMessage =
        "QuestData.shn transaktional als Client+Server Projekt-Override gespeichert.";
    return true;
}
// Katalog aller Mobs/NPCs aus MobInfo.shn (Client) mit ihren Platzierungen aus World/NPC.txt.
// Belegt an NA2016: alle 137 Start-NPCs der Originalquests stehen in NPC.txt (Rollen QuestNpc,
// Merchant, Guard, ...); ein Start-NPC ohne Platzierung kann im Spiel nicht angesprochen werden.
void EnsureQuestNpcCatalog(EditorState& state) {
    if (state.questNpcCatalogBuilt) return;
    state.questNpcCatalogBuilt = true;
    state.questNpcCatalog.clear();
    EnsureNpcTextLoaded(state);
    std::unordered_map<std::string, std::vector<std::string>> placements;
    if (state.npcTextLoaded) {
        if (const auto* table = state.npcTextFile.FindTable("ShineNPC")) {
            for (const auto& rec : table->records) {
                if (rec.values.size() < 7) continue;
                placements[LowerAscii(rec.values[0])].push_back(
                    rec.values[1] + " " + rec.values[2] + "/" + rec.values[3] + " · " + rec.values[6]);
            }
        }
    }
    int doc = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client);
    if (doc < 0) doc = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Server);
    if (doc < 0) return;
    const auto& file = state.shnFiles[static_cast<std::size_t>(doc)].file;
    const int cId = ExactShnColumnIndex(file, "ID"), cInx = ExactShnColumnIndex(file, "InxName");
    const int cName = ExactShnColumnIndex(file, "Name"), cNpc = ExactShnColumnIndex(file, "IsNPC");
    if (cId < 0 || cInx < 0) return;
    for (const auto& row : file.rows) {
        EditorState::QuestNpcEntry e;
        long long id = 0;
        if (!ShnValueAsInt(row.values[static_cast<std::size_t>(cId)], id)) continue;
        e.id = static_cast<int>(id);
        e.inx = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cInx)]);
        if (cName >= 0) e.name = core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)]);
        long long npcFlag = 0;
        e.isNpc = cNpc >= 0 && ShnValueAsInt(row.values[static_cast<std::size_t>(cNpc)], npcFlag) && npcFlag != 0;
        if (const auto it = placements.find(LowerAscii(e.inx)); it != placements.end()) e.placements = it->second;
        state.questNpcCatalog.push_back(std::move(e));
    }
}
const EditorState::QuestNpcEntry* FindQuestNpc(EditorState& state, int id) {
    EnsureQuestNpcCatalog(state);
    for (const auto& e : state.questNpcCatalog) if (e.id == id) return &e;
    return nullptr;
}
// Platzierung eines NPCs als eine Zeile; leer = steht auf keiner Karte.
std::string QuestNpcPlacementText(const EditorState::QuestNpcEntry& e) {
    std::string out;
    for (std::size_t i = 0; i < e.placements.size() && i < 3; ++i) out += (i ? ",  " : "") + e.placements[i];
    if (e.placements.size() > 3) out += ",  +" + std::to_string(e.placements.size() - 3);
    return out;
}
// Auswahl-Popup; gibt die gewählte ID zurück (oder -1, solange nichts gewählt wurde).
int DrawQuestNpcPickerPopup(EditorState& state) {
    int chosen = -1;
    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(L("NPC wählen###questNpcPicker", "Choose NPC###questNpcPicker"), nullptr)) {
        EnsureQuestNpcCatalog(state);
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        UI::InputTextWithHint("##questNpcFilter", L("Name, InxName oder ID...", "Name, InxName or ID..."),
                              state.questNpcFilter, sizeof(state.questNpcFilter));
        ImGui::SameLine();
        UI::Checkbox(L("Nur NPCs mit Platzierung", "Only NPCs with a placement"), &state.questNpcOnlyPlaced);
        const std::string needle = LowerAscii(state.questNpcFilter);
        std::vector<const EditorState::QuestNpcEntry*> rows;
        for (const auto& e : state.questNpcCatalog) {
            if (state.questNpcOnlyPlaced && e.placements.empty()) continue;
            if (!needle.empty() && LowerAscii(e.name).find(needle) == std::string::npos &&
                LowerAscii(e.inx).find(needle) == std::string::npos && std::to_string(e.id).find(needle) == std::string::npos)
                continue;
            rows.push_back(&e);
        }
        ImGui::TextDisabled(L("%zu Treffer", "%zu matches"), rows.size());
        if (ImGui::BeginTable("##questNpcRows", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                              ImVec2(0.0f, ImGui::GetContentRegionAvail().y - 36.0f))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn(L("Name", "Name"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("InxName", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(L("Platzierung (NPC.txt)", "Placement (NPC.txt)"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows.size()));
            while (clipper.Step()) {
                for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
                    const auto& e = *rows[static_cast<std::size_t>(r)];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable((std::to_string(e.id) + "##npcPick" + std::to_string(r)).c_str(), false,
                                          ImGuiSelectableFlags_SpanAllColumns))
                        chosen = e.id;
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(e.name.c_str());
                    ImGui::TableSetColumnIndex(2); ImGui::TextDisabled("%s", e.inx.c_str());
                    ImGui::TableSetColumnIndex(3);
                    const std::string place = QuestNpcPlacementText(e);
                    if (place.empty()) ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "%s", L("keine", "none"));
                    else ImGui::TextUnformatted(place.c_str());
                }
            }
            ImGui::EndTable();
        }
        if (UI::Button(L("Abbrechen##questNpcPicker", "Cancel##questNpcPicker")) || chosen >= 0) {
            state.questNpcPickerTarget = chosen >= 0 ? state.questNpcPickerTarget : -2;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return chosen;
}
void DrawQuestEditor(EditorState& state) {
    EnsureQuestDataLoaded(state);
    EnsureQuestDialogLoaded(state);
    if (!state.questDataLoaded) {
        if (state.shnServerRoot.empty()) {
            ImGui::TextWrapped("%s", L("Kein Server-Ordner bekannt - in den Projekt-Einstellungen den Server-Ordner setzen (Server/9Data/Shine wird automatisch gefunden).", "No server folder is known - set the server folder in project settings (Server/9Data/Shine is detected automatically)."));
        } else {
            ImGui::TextWrapped(L("QuestData.shn konnte unter '%s' nicht geladen werden.", "QuestData.shn could not be loaded from '%s'."), state.shnServerRoot.c_str());
        }
        return;
    }
    auto& quests = state.questDataFile.records;
    auto saveQuestData = [&]() {
        if (state.questDirty || !QuestDialogDirty(state)) {
            if (!SaveQuestDataProject(state)) return;
        }
        if (QuestDialogDirty(state)) {
            const std::string questStatus = state.questDirty ? std::string() : state.statusMessage;
            SaveQuestDialogTexts(state);
            if (!questStatus.empty()) state.statusMessage = questStatus + " · " + state.statusMessage;
        }
    };
    auto reloadQuestData = [&]() {
        const auto sourcePath=std::filesystem::path(state.shnServerRoot)/"QuestData.shn";
        const auto path=PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
        const int selectedId=(state.selectedQuestIdx>=0 &&
            static_cast<std::size_t>(state.selectedQuestIdx)<quests.size())
            ? static_cast<int>(quests[static_cast<std::size_t>(state.selectedQuestIdx)].id) : -1;
        auto loaded=core::legacy::LoadQuestData(path);
        if (!loaded) {
            state.statusMessage="QuestData.shn: "+loaded.error();
            return;
        }
        state.questDataFile=std::move(*loaded);
        state.questDirty=false;
        state.selectedQuestIdx=-1;
        if (QuestDialogDirty(state)) ReloadShnDocument(state, state.questDialogDoc); // Textänderungen ebenfalls verwerfen
        state.dataHistory.Clear(); // Indizes im Verlauf gelten nach dem Verwerfen nicht mehr
        if (selectedId>=0) {
            for (std::size_t i=0;i<state.questDataFile.records.size();++i)
                if (state.questDataFile.records[i].id==selectedId) {
                    state.selectedQuestIdx=static_cast<int>(i);
                    break;
                }
        }
        ++state.questRevision;
        state.questListKey.clear();
        state.statusMessage=L("QuestData.shn neu geladen.","QuestData.shn reloaded.");
    };
    auto duplicateSelectedQuest = [&]() {
        if (state.selectedQuestIdx < 0 ||
            static_cast<std::size_t>(state.selectedQuestIdx) >= quests.size()) return;
        std::array<bool,65536> used{};
        int maxId=0;
        for (const auto& rec:quests) {
            used[rec.id]=true;
            maxId=std::max(maxId,static_cast<int>(rec.id));
        }
        int newId=-1;
        if (maxId < 65535 && !used[static_cast<std::size_t>(maxId+1)]) newId=maxId+1;
        if (newId < 0) {
            for (int id=1;id<=65535;++id) {
                if (!used[static_cast<std::size_t>(id)]) { newId=id; break; }
            }
        }
        if (newId < 0) {
            state.statusMessage=L("Keine freie Quest-ID verfügbar.","No free quest ID available.");
            return;
        }
        auto copy=quests[static_cast<std::size_t>(state.selectedQuestIdx)];
        copy.id=static_cast<std::uint16_t>(newId);
        copy.dataLen=0; // wird von SaveQuestData aus dem aktuellen Record neu berechnet
        TrackQuestEdits(state);
        // Ausdrückliche Option: Texte kopieren und auf einen freien Text-ID-Block umstellen (sonst teilen
        // sich Vorlage und Klon Titel, Beschreibung und SAY-Texte).
        std::map<std::uint32_t,std::uint32_t> textRemap;
        if (state.questCloneNewTexts && state.questDialogLoaded) {
            QuestTextRow(state, 0); // Text-Index aufbauen
            std::set<std::uint32_t> usedTextIds;
            for (const auto& [textId,row] : state.questTextRow) usedTextIds.insert(static_cast<std::uint32_t>(textId));
            textRemap=core::legacy::AssignNewQuestTextIds(copy, usedTextIds);
        }
        DataHistoryBegin(state, std::string(L("Quest geklont #","Quest cloned #"))+std::to_string(newId));
        quests.push_back(std::move(copy));
        state.selectedQuestIdx=static_cast<int>(quests.size()-1);
        state.questDirty=true;
        ++state.questRevision;
        state.questListKey.clear();
        RecordQuestInserted(state, state.selectedQuestIdx);
        if (!textRemap.empty()) CopyQuestTexts(state, textRemap);
        DataHistoryCommit(state);
        if (!textRemap.empty())
            state.statusMessage=std::string(L("Quest geklont · neue ID #","Quest cloned · new ID #"))+std::to_string(newId)+
                                L(" · ", " · ")+std::to_string(textRemap.size())+
                                L(" Texte auf neue Text-IDs ab "," texts copied to new text IDs from ")+
                                std::to_string(textRemap.begin()->second)+L(" kopiert. Reward-Referenzen aus der Vorlage übernommen.",
                                                                            ". Reward references were copied from the template.");
        else
            state.statusMessage=std::string(L("Quest geklont · neue ID #","Quest cloned · new ID #"))+
                                std::to_string(newId)+
                                L(". Text-/Reward-Referenzen wurden bewusst aus der Vorlage übernommen.",
                                  ". Text/reward references were intentionally copied from the template.");
    };
    const std::string questHeaderMeta = std::to_string(quests.size()) +
        L(" Quests"," quests") +
        (state.questDialogLoaded ? std::string{} : std::string(L(" · QuestDialog fehlt"," · QuestDialog missing")));
    DrawPanelHeader("questEditorHeader", "QUEST EDITOR", DrawIconBook,
                    "module.quest", questHeaderMeta.c_str());
    const bool questAnyDirty = state.questDirty || QuestDialogDirty(state);
    if (questAnyDirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f), "%s",
                           state.questDirty ? L("* geändert","* modified") : L("* Texte geändert","* texts modified"));
    }
    const float questSaveW = 190.0f;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8.0f,
                             ImGui::GetWindowContentRegionMax().x - questSaveW));
    if (UI::Button(questAnyDirty ? L("QuestData speichern *","Save QuestData *")
                                 : L("QuestData speichern","Save QuestData"),
                   ImVec2(questSaveW,0))) {
        saveQuestData();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", L("Speichert QuestData.shn (Client + Server) und geänderte Texte in QuestDialog.shn als Projekt-Override.",
                                  "Saves QuestData.shn (client + server) and changed texts in QuestDialog.shn as project override."));
    if (ShortcutPressed(state.shortcutSave) && questAnyDirty) saveQuestData();
    ImGui::SameLine();
    ImGui::BeginDisabled(!questAnyDirty);
    if (UI::Button(L("Neu laden / verwerfen","Reload / discard"))) reloadQuestData();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(state.selectedQuestIdx < 0 ||
                         static_cast<std::size_t>(state.selectedQuestIdx) >= quests.size());
    if (UI::Button(L("Aus Auswahl klonen","Clone selected"))) duplicateSelectedQuest();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s",L(
            "Erzeugt eine vollständige Kopie mit freier Quest-ID. Unbekannte Reward-/Raw-Felder bleiben bytegetreu aus der Vorlage erhalten.",
            "Creates a full copy with a free quest ID. Unknown reward/raw fields are preserved from the template."));
    ImGui::SameLine();
    UI::Checkbox(L("neue Text-IDs","new text IDs"), &state.questCloneNewTexts);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s",L(
            "Beim Klonen Titel, Beschreibung und alle SAY-Texte in QuestDialog.shn kopieren und der neuen Quest einen freien Text-ID-Block geben. Aus: der Klon nutzt die Texte der Vorlage mit.",
            "When cloning, copy title, description and all SAY texts in QuestDialog.shn and give the new quest a free block of text IDs. Off: the clone shares the template's texts."));
    ImGui::SameLine();
    ImGui::BeginDisabled(state.selectedQuestIdx < 0 ||
                         static_cast<std::size_t>(state.selectedQuestIdx) >= quests.size());
    if (UI::Button(L("Quest löschen","Delete quest"))) {
        state.questDeleteIdx = state.selectedQuestIdx;
        state.questDeleteOpen = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    DrawDataHistoryButtons(state, "questHistory");
    if (state.questDeleteOpen) {
        const char* title = L("Quest löschen###questDelete","Delete quest###questDelete");
        ImGui::OpenPopup(title);
        if (ImGui::BeginPopupModal(title, &state.questDeleteOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            const bool valid = state.questDeleteIdx >= 0 && static_cast<std::size_t>(state.questDeleteIdx) < quests.size();
            if (valid) {
                const auto& q = quests[static_cast<std::size_t>(state.questDeleteIdx)];
                ImGui::Text(L("Quest #%u \"%s\" löschen?","Delete quest #%u \"%s\"?"), q.id, QuestTextOf(state, static_cast<int>(q.title)).c_str());
                // Folgequests, die diese Quest als Vorgänger verlangen, verlören ihre Bedingung.
                std::vector<std::uint16_t> followers;
                for (const auto& other : quests)
                    if (other.needPred && other.predecessor == q.id && other.id != q.id) followers.push_back(other.id);
                if (followers.empty()) {
                    ImGui::TextColored(ImVec4(0.5f,0.9f,0.5f,1.0f), "%s", L("Keine Quest verlangt diese als Vorgänger.","No quest requires this one as predecessor."));
                } else {
                    ImGui::TextColored(ImVec4(1.0f,0.75f,0.3f,1.0f), "%s", L("Diese Quests verlangen sie als Vorgänger:","These quests require it as predecessor:"));
                    std::string list;
                    for (const auto id : followers) list += (list.empty() ? "#" : ", #") + std::to_string(id);
                    ImGui::TextWrapped("%s", list.c_str());
                }
                ImGui::TextDisabled("%s", L("Texte in QuestDialog.shn bleiben erhalten. Strg+Z macht das Löschen rückgängig.",
                                            "Texts in QuestDialog.shn are kept. Ctrl+Z undoes the deletion."));
            }
            if (UI::Button(L("Löschen","Delete")) && valid) {
                DeleteQuestWithHistory(state, state.questDeleteIdx);
                state.statusMessage = L("Quest gelöscht (noch nicht gespeichert).","Quest deleted (not saved yet).");
                state.questDeleteOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (UI::Button(L("Abbrechen","Cancel"))) { state.questDeleteOpen = false; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    ImGui::Separator();
    DrawSearchInput("questSearch",
                    L("Quest-ID oder Titeltext suchen...", "Search quest ID or title text..."),
                    state.questSearch, sizeof(state.questSearch), 405.0f);
    ImGui::SameLine();
    if (SceneQuickFilterButton("questAll",L("Alle","All"),state.questQuickFilter==0)) state.questQuickFilter=0;
    ImGui::SameLine();
    if (SceneQuickFilterButton("questEnabled",L("Aktiv","Enabled"),state.questQuickFilter==1)) state.questQuickFilter=1;
    ImGui::SameLine();
    if (SceneQuickFilterButton("questDaily",L("Täglich","Daily"),state.questQuickFilter==2)) state.questQuickFilter=2;
    ImGui::SameLine();
    if (SceneQuickFilterButton("questProblems",L("Probleme","Problems"),state.questQuickFilter==3)) state.questQuickFilter=3;

    // Listen-Beschriftungen und Filter nur bei Aenderung neu berechnen.
    const std::string key = std::string(state.questSearch) + "|" + std::to_string(quests.size()) + "|" +
                            (state.questDialogLoaded ? "d" : "-") + "|" + std::to_string(state.questRevision) +
                            "|qf=" + std::to_string(state.questQuickFilter);
    if (key != state.questListKey) {
        state.questListKey = key;
        state.questLabels.assign(quests.size(), std::string());
        state.questVisible.clear();
        const std::string needle = LowerAscii(state.questSearch);

        std::unordered_set<int> knownMobs;
        std::unordered_set<int> knownItems;
        std::unordered_set<int> knownQuests;
        if (state.questQuickFilter == 3) {
            EnsureMobViewInfoLoaded(state);
            EnsureItemInfoLoadedForQuests(state);
            if (state.mobViewInfoLoaded) {
                for (const auto& row : ShnDocFile(state, state.mobViewInfoDoc).rows) {
                    if (row.values.empty()) continue;
                    long long id = 0;
                    if (ShnValueAsInt(row.values[0], id)) knownMobs.insert(static_cast<int>(id));
                }
            }
            if (state.itemInfoLoaded) {
                for (const auto& row : ShnDocFile(state, state.itemInfoDoc).rows) {
                    if (row.values.empty()) continue;
                    long long id = 0;
                    if (ShnValueAsInt(row.values[0], id)) knownItems.insert(static_cast<int>(id));
                }
            }
            for (const auto& q : quests) knownQuests.insert(q.id);
        }

        auto hasReferenceProblem = [&](const core::legacy::QuestRecord& q) {
            if (q.title != 0 && QuestTextOf(state,q.title).empty()) return true;
            if (q.description != 0 && QuestTextOf(state,q.description).empty()) return true;
            if (q.needLevel != 0 && q.minLevel > q.maxLevel && q.maxLevel != 0) return true;
            if (q.needNpc != 0 && (q.startingNpc == 0 || !knownMobs.contains(q.startingNpc))) return true;
            if (q.needItem != 0 && (q.itemId == 0 || !knownItems.contains(q.itemId))) return true;
            if (q.needPred != 0 &&
                (q.predecessor == 0 || q.predecessor == q.id || !knownQuests.contains(q.predecessor))) return true;
            for (const auto& m : q.mobs) {
                if (m.active == 0) continue;
                // "NPC aufsuchen" (hasToBeKilled 0) hat im Original Anzahl 0 (2289 von 2291 Zielen).
                if (m.id == 0 || (m.hasToBeKilled != 0 && m.amount == 0) || !knownMobs.contains(m.id)) return true;
            }
            for (const auto& item : q.items) {
                if (item.active == 0) continue;
                if (item.id == 0 || item.amount == 0 || !knownItems.contains(item.id)) return true;
            }
            for (const auto& drop : q.drops) {
                if (drop.active == 0) continue;
                if (drop.mobId == 0 || drop.itemId == 0 || drop.amount == 0) return true;
                if (!knownMobs.contains(static_cast<int>(drop.mobId))) return true;
                if (!knownItems.contains(static_cast<int>(drop.itemId))) return true;
            }
            return false;
        };

        for (std::size_t i = 0; i < quests.size(); ++i) {
            const auto& q = quests[i];
            std::string title = QuestTextOf(state, q.title);
            if (title.empty()) title = QuestTextOf(state, q.description); // viele Quests haben nur eine Beschreibung
            if (title.empty()) title = L("(ohne Text)", "(no text)");
            state.questLabels[i] = "#" + std::to_string(q.id) + "  " + title;
            if (!needle.empty() && LowerAscii(state.questLabels[i]).find(needle) == std::string::npos) continue;
            if (state.questQuickFilter == 1 && q.enableQuest == 0) continue;
            if (state.questQuickFilter == 2 && q.dailyQuest == 0) continue;
            if (state.questQuickFilter == 3 && !hasReferenceProblem(q)) continue;
            state.questVisible.push_back(i);
        }
    }

    const float listWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.30f, 310.0f, 430.0f);
    ImGui::BeginChild("QuestList", ImVec2(listWidth, 0.0f), true);
    const std::string questListMeta = std::to_string(state.questVisible.size()) +
                                      L(" sichtbar"," visible");
    DrawPanelHeader("questListHeader", "QUESTS", DrawIconBook, "module.quest",
                    questListMeta.c_str());
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.questVisible.size()));
        while (clipper.Step()) {
            for (int vi = clipper.DisplayStart; vi < clipper.DisplayEnd; ++vi) {
                const std::size_t i = state.questVisible[static_cast<std::size_t>(vi)];
                if (UI::Selectable((state.questLabels[i] + "##q" + std::to_string(i)).c_str(), state.selectedQuestIdx == static_cast<int>(i))) {
                    state.selectedQuestIdx = static_cast<int>(i);
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("QuestDetail", ImVec2(0.0f, 0.0f), true);
    DrawPanelHeader("questPropertiesHeader", L("EIGENSCHAFTEN","PROPERTIES"),
                    DrawIconGear, "panel.properties");
    if (state.selectedQuestIdx < 0 || static_cast<std::size_t>(state.selectedQuestIdx) >= quests.size()) {
        ImGui::TextDisabled("%s", L("Quest links auswählen.", "Choose a quest on the left."));
        ImGui::EndChild();
        return;
    }
    auto& q = quests[static_cast<std::size_t>(state.selectedQuestIdx)];
    const ImVec4 kOk(0.50f, 0.90f, 0.50f, 1.0f), kBad(1.0f, 0.40f, 0.40f, 1.0f), kDim(0.60f, 0.66f, 0.74f, 1.0f);

    if (SceneQuickFilterButton("questFormView",L("Formular","Form"),!state.questShowFlow))
        state.questShowFlow=false;
    ImGui::SameLine();
    if (SceneQuickFilterButton("questFlowView",L("Flow","Flow"),state.questShowFlow))
        state.questShowFlow=true;
    ImGui::SameLine();
    ImGui::TextDisabled("%s",state.questShowFlow
        ? L("Vorgänger/Folgequests aus QuestData","predecessor/successors from QuestData")
        : L("Felder und Referenzen bearbeiten","edit fields and references"));
    ImGui::Separator();

    if (state.questShowFlow) {
        DrawQuestFlowView(state,quests,static_cast<std::size_t>(state.selectedQuestIdx));
        ImGui::EndChild();
        return;
    }

    // Kleine Bausteine fuer eine ordentliche Formular-Optik: feste Labelspalte, kompakte Felder,
    // aufgeloeste Namen in der dritten Spalte (Umbruch innerhalb der Spalte).
    auto beginForm = [&](const char* id) {
        if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingFixedFit)) return false;
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 210.0f);
        ImGui::TableSetupColumn("##field", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("##info", ImGuiTableColumnFlags_WidthStretch);
        return true;
    };
    auto rowLabel = [&](const char* label) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(110.0f);
    };
    auto infoText = [&](const std::string& text, bool ok, bool known = true) {
        ImGui::TableSetColumnIndex(2);
        ImGui::AlignTextToFramePadding();
        if (text.empty()) ImGui::TextColored(kDim, "%s", known ? "-" : "?");
        else ImGui::TextColored(ok ? kOk : kBad, "%s", text.c_str());
    };
    auto markQuestChanged = [&]() {
        state.questDirty = true;
        ++state.questRevision;
    };
    auto u16Row = [&](const char* label, const char* id, std::uint16_t& value, int maxValue = 65535) {
        rowLabel(label);
        int v = value;
        if (UI::InputInt(id, &v, 0, 0)) {
            value = static_cast<std::uint16_t>(std::clamp(v, 0, maxValue));
            markQuestChanged();
        }
    };

    auto u32Row = [&](const char* label, const char* id, std::uint32_t& value) {
        rowLabel(label);
        if (UI::InputScalar(id, ImGuiDataType_U32, &value)) markQuestChanged();
    };

    ImGui::Text(L("Quest #%d", "Quest #%d"), q.id);
    ImGui::SameLine();
    ImGui::TextColored(kDim, "%s", QuestTextOf(state, q.title).c_str());
    ImGui::Separator();

    if (UI::CollapsingHeader(L("Allgemein", "General"), ImGuiTreeNodeFlags_DefaultOpen) &&
        beginForm("##qGeneral")) {
        u16Row(L("Quest-ID", "Quest ID"), "##id", q.id);
        // Text-ID in der Feldspalte, der Text selbst daneben direkt bearbeitbar (QuestDialog.shn).
        u32Row(L("Titel-Text-ID", "Title text ID"), "##title", q.title);
        ImGui::TableSetColumnIndex(2);
        DrawQuestTextField(state, "questTitle", q.title, false);
        u32Row(L("Beschreibung-Text-ID", "Description text ID"), "##desc", q.description);
        ImGui::TableSetColumnIndex(2);
        DrawQuestTextField(state, "questDescription", q.description, true);
        rowLabel(L("Start-NPC erforderlich","Starting NPC required"));
        { bool need = q.needNpc != 0; if (UI::Checkbox("##needNpc",&need)) { q.needNpc = need ? 1 : 0; markQuestChanged(); } }
        u16Row(L("Start-NPC (Mob-ID)", "Starting NPC (mob ID)"), "##startnpc", q.startingNpc);
        {
            const auto* npc = q.startingNpc != 0 ? FindQuestNpc(state, q.startingNpc) : nullptr;
            ImGui::TableSetColumnIndex(2);
            ImGui::AlignTextToFramePadding();
            if (UI::SmallButton(L("Auswählen...##startNpcPick", "Choose...##startNpcPick")) || state.automationQuestSection == 3) {
                state.automationQuestSection = 0;
                state.questNpcPickerTarget = -1;
                state.questNpcOnlyPlaced = true;
                ImGui::OpenPopup(L("NPC wählen###questNpcPicker", "Choose NPC###questNpcPicker"));
            }
            ImGui::SameLine();
            if (q.startingNpc == 0) {
                ImGui::TextColored(kDim, "%s", L("kein Start-NPC", "no starting NPC"));
            } else if (!npc) {
                ImGui::TextColored(kBad, "%s", L("ID nicht in MobInfo.shn", "ID not in MobInfo.shn"));
            } else {
                ImGui::TextColored(kOk, "%s (%s)", npc->name.c_str(), npc->inx.c_str());
                ImGui::SameLine();
                if (UI::SmallButton(L("Öffnen##startNpcRef", "Open##startNpcRef")))
                    OpenShnRecordById(state, {"MobInfo.shn","MobViewInfo.shn"},
                                      EditorState::ShnSource::Client, q.startingNpc);
            }
            if (npc) {
                rowLabel(L("Platzierung", "Placement"));
                ImGui::TableSetColumnIndex(1);
                ImGui::TableSetColumnIndex(2);
                const std::string place = QuestNpcPlacementText(*npc);
                if (place.empty())
                    ImGui::TextColored(kBad, "%s", L("steht auf keiner Karte (World/NPC.txt) - die Quest kann im Spiel nicht angenommen werden",
                                                     "not placed on any map (World/NPC.txt) - the quest cannot be accepted in the game"));
                else ImGui::TextWrapped("%s", place.c_str());
            }
            if (q.needNpc == 0 && q.startingNpc != 0)
                ImGui::TextColored(kDim, "%s", L("(Start-NPC erforderlich ist aus - die ID wird nicht verwendet)",
                                                 "(starting NPC required is off - the ID is not used)"));
            const int chosen = DrawQuestNpcPickerPopup(state);
            if (chosen >= 0 && state.questNpcPickerTarget == -1) {
                q.startingNpc = static_cast<std::uint16_t>(chosen);
                q.needNpc = 1;
                markQuestChanged();
                state.questNpcPickerTarget = -2;
            }
        }
        rowLabel(L("Aktiviert", "Enabled"));
        { bool enable = q.enableQuest != 0; if (UI::Checkbox("##enable", &enable)) { q.enableQuest = enable ? 1 : 0; markQuestChanged(); } }
        rowLabel(L("Tägliche Quest", "Daily quest"));
        { bool daily = q.dailyQuest != 0; if (UI::Checkbox("##daily", &daily)) { q.dailyQuest = daily ? 1 : 0; markQuestChanged(); } }
        ImGui::EndTable();
    }

    if (UI::CollapsingHeader(L("Voraussetzungen", "Requirements"), ImGuiTreeNodeFlags_DefaultOpen) &&
        beginForm("##qRequirements")) {
        rowLabel(L("Level-Bedingung aktiv","Level requirement enabled"));
        { bool need = q.needLevel != 0; if (UI::Checkbox("##needLevel",&need)) { q.needLevel = need ? 1 : 0; markQuestChanged(); } }
        if (q.needLevel != 0) {
            rowLabel(L("Mindest-Level", "Minimum level"));
            { int v = q.minLevel; if (UI::InputInt("##minlv", &v, 0, 0)) { q.minLevel = static_cast<std::uint8_t>(std::clamp(v, 0, 255)); markQuestChanged(); } }
            rowLabel(L("Maximal-Level", "Maximum level"));
            { int v = q.maxLevel; if (UI::InputInt("##maxlv", &v, 0, 0)) { q.maxLevel = static_cast<std::uint8_t>(std::clamp(v, 0, 255)); markQuestChanged(); } }
        }

        rowLabel(L("Item-Bedingung aktiv","Item requirement enabled"));
        { bool need = q.needItem != 0; if (UI::Checkbox("##needItem",&need)) { q.needItem = need ? 1 : 0; markQuestChanged(); } }
        if (q.needItem != 0) {
            u16Row(L("Benötigtes Item (ID)", "Required item (ID)"), "##reqitem", q.itemId);
            auto r = ResolveItemNameForQuest(state, q.itemId);
            infoText(r.first == "-" ? std::string() : r.first, r.second);
            if (r.second && q.itemId != 0) {
                ImGui::SameLine();
                if (UI::SmallButton(L("Öffnen##requiredItemRef", "Open##requiredItemRef")))
                    OpenShnRecordById(state, {"ItemInfo.shn"}, EditorState::ShnSource::Server, q.itemId);
            }
            rowLabel(L("Item bei Annahme verbrauchen","Consume item on accept"));
            { bool vanish = q.itemVanish != 0; if (UI::Checkbox("##itemVanish",&vanish)) { q.itemVanish = vanish ? 1 : 0; markQuestChanged(); } }
        }

        rowLabel(L("Vorgänger-Quest nötig","Predecessor quest required"));
        { bool need = q.needPred != 0; if (UI::Checkbox("##needPred",&need)) { q.needPred = need ? 1 : 0; markQuestChanged(); } }
        if (q.needPred != 0) {
            u16Row(L("Vorgänger-Quest (ID)", "Predecessor quest (ID)"), "##pred", q.predecessor);
            bool found = false;
            std::size_t predecessorIndex = 0;
            std::string predTitle;
            for (std::size_t oi = 0; oi < quests.size(); ++oi) {
                if (quests[oi].id != q.predecessor) continue;
                found = true;
                predecessorIndex = oi;
                predTitle = QuestTextOf(state, quests[oi].title);
                break;
            }
            infoText(found ? (predTitle.empty() ? std::string(L("gefunden", "found")) : predTitle)
                           : std::string(L("Quest-ID nicht gefunden", "Quest ID not found")), found);
            if (found) {
                ImGui::SameLine();
                if (UI::SmallButton(L("Öffnen##predecessorQuestRef", "Open##predecessorQuestRef")))
                    state.selectedQuestIdx = static_cast<int>(predecessorIndex);
            }
        }

        rowLabel(L("Klassen-Bedingung aktiv","Class requirement enabled"));
        { bool need = q.needClass != 0; if (UI::Checkbox("##needClass",&need)) { q.needClass = need ? 1 : 0; markQuestChanged(); } }
        if (q.needClass != 0) {
            rowLabel(L("Klassen-Typ (Rohwert)","Class type (raw value)"));
            int classType = q.classType;
            if (UI::InputInt("##classType",&classType,0,0)) {
                q.classType = static_cast<std::uint8_t>(std::clamp(classType,0,255));
                markQuestChanged();
            }
            infoText(L("Enum noch nicht semantisch kartiert","Enum not semantically mapped yet"),true,false);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (state.automationQuestSection == 4) { ImGui::SetScrollHereY(0.0f); state.automationQuestSection = 0; }
    if (UI::CollapsingHeader(L("Ziele · Monster", "Objectives · Monsters"), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##qmobs", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn(L("Aktiv", "Active"), ImGuiTableColumnFlags_WidthFixed, 46.0f);
            ImGui::TableSetupColumn(L("Art", "Kind"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
            ImGui::TableSetupColumn(L("Mob-ID", "Mob ID"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn(L("Anzahl", "Amount"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(L("Name", "Name"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##sp", ImGuiTableColumnFlags_WidthFixed, 1.0f);
            ImGui::TableHeadersRow();
            for (std::size_t mi = 0; mi < q.mobs.size(); ++mi) {
                ImGui::PushID(static_cast<int>(mi));
                auto& m = q.mobs[mi];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); bool active = m.active != 0; if (UI::Checkbox("##a", &active)) { m.active = active ? 1 : 0; markQuestChanged(); }
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(140.0f);
                {
                    // hasToBeKilled: 0 = NPC aufsuchen (Anzahl immer 0), 1 = Monster besiegen.
                    const char* kinds[] = {L("NPC aufsuchen", "Visit NPC"), L("Monster besiegen", "Defeat monster")};
                    int kind = m.hasToBeKilled != 0 ? 1 : 0;
                    if (m.hasToBeKilled > 1) ImGui::Text("%s %u", L("Rohwert", "Raw value"), static_cast<unsigned>(m.hasToBeKilled));
                    else if (UI::Combo("##kind", &kind, kinds, 2)) { m.hasToBeKilled = static_cast<std::uint8_t>(kind); markQuestChanged(); }
                }
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(80.0f); int mid = m.id; if (UI::InputInt("##id", &mid, 0, 0)) { m.id = static_cast<std::uint16_t>(std::clamp(mid, 0, 65535)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(70.0f); int amt = m.amount; if (UI::InputInt("##n", &amt, 0, 0)) { m.amount = static_cast<std::uint8_t>(std::clamp(amt, 0, 255)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(4);
                if (m.active != 0 || m.id != 0) {
                    auto r = ResolveMobNameForQuest(state, m.id);
                    ImGui::TextColored(r.second ? kOk : kBad, "%s", r.first.c_str());
                    if (r.second && m.id != 0) {
                        ImGui::SameLine();
                        if (UI::SmallButton(L("Öffnen##mobQuestRef", "Open##mobQuestRef")))
                            OpenShnRecordById(state, {"MobInfo.shn","MobViewInfo.shn"},
                                              EditorState::ShnSource::Client, m.id);
                    }
                    // Ein aufzusuchender NPC muss auf einer Karte stehen, sonst ist das Ziel unerfüllbar.
                    if (m.hasToBeKilled == 0 && m.id != 0) {
                        const auto* npc = FindQuestNpc(state, m.id);
                        if (npc && npc->placements.empty()) {
                            ImGui::SameLine();
                            ImGui::TextColored(kBad, "%s", L("(auf keiner Karte)", "(not on any map)"));
                        } else if (npc) {
                            ImGui::SameLine();
                            ImGui::TextColored(kDim, "%s", npc->placements.front().c_str());
                        }
                    }
                }
                if (m.active != 0 || m.id != 0) ImGui::SameLine();
                if (UI::SmallButton(L("Auswählen...##mobQuestPick", "Choose...##mobQuestPick"))) {
                    state.questNpcPickerTarget = static_cast<int>(mi);
                    state.questNpcPickerOpenRequest = true;
                    state.questNpcOnlyPlaced = m.hasToBeKilled == 0; // Monster stehen nicht in NPC.txt
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (state.questNpcPickerOpenRequest) {
            ImGui::OpenPopup(L("NPC wählen###questNpcPicker", "Choose NPC###questNpcPicker"));
            state.questNpcPickerOpenRequest = false;
        }
        const int chosen = DrawQuestNpcPickerPopup(state);
        const int target = state.questNpcPickerTarget;
        if (chosen >= 0 && target >= 0 && static_cast<std::size_t>(target) < q.mobs.size()) {
            auto& m = q.mobs[static_cast<std::size_t>(target)];
            m.id = static_cast<std::uint16_t>(chosen);
            m.active = 1;
            if (m.hasToBeKilled == 0) m.amount = 0; // "NPC aufsuchen" hat im Original immer Anzahl 0
            markQuestChanged();
            state.questNpcPickerTarget = -2;
        }
    }
    if (UI::CollapsingHeader(L("Ziele · Items", "Objectives · Items"), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##qitems", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn(L("Aktiv", "Active"), ImGuiTableColumnFlags_WidthFixed, 46.0f);
            ImGui::TableSetupColumn(L("Item-ID", "Item ID"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn(L("Anzahl", "Amount"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(L("Name", "Name"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##sp", ImGuiTableColumnFlags_WidthFixed, 1.0f);
            ImGui::TableHeadersRow();
            for (std::size_t ii = 0; ii < q.items.size(); ++ii) {
                ImGui::PushID(static_cast<int>(ii) + 100);
                auto& it = q.items[ii];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); bool active = it.active != 0; if (UI::Checkbox("##a", &active)) { it.active = active ? 1 : 0; markQuestChanged(); }
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(80.0f); int itemId = it.id; if (UI::InputInt("##id", &itemId, 0, 0)) { it.id = static_cast<std::uint16_t>(std::clamp(itemId, 0, 65535)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(70.0f); int amt = it.amount; if (UI::InputInt("##n", &amt, 0, 0)) { it.amount = static_cast<std::uint16_t>(std::clamp(amt, 0, 65535)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(3);
                if (it.active != 0 || it.id != 0) {
                    auto r = ResolveItemNameForQuest(state, it.id);
                    DrawItemIconById(state, it.id, ImGui::GetTextLineHeight());
                    ImGui::SameLine();
                    ImGui::TextColored(r.second ? kOk : kBad, "%s", r.first.c_str());
                    if (r.second && it.id != 0) {
                        ImGui::SameLine();
                        if (UI::SmallButton(L("Öffnen##itemQuestRef", "Open##itemQuestRef")))
                            OpenShnRecordById(state, {"ItemInfo.shn"}, EditorState::ShnSource::Server, it.id);
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if (state.automationQuestSection == 1) { ImGui::SetScrollHereY(0.0f); state.automationQuestSection = 0; }
    if (UI::CollapsingHeader(L("Ziele · Drops", "Objectives · Drops"), ImGuiTreeNodeFlags_DefaultOpen)) {
        int removeIdx = -1;
        if (ImGui::BeginTable("##qdrops", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn(L("Aktiv","Active"), ImGuiTableColumnFlags_WidthFixed, 48.0f);
            ImGui::TableSetupColumn(L("Mob-ID", "Mob ID"), ImGuiTableColumnFlags_WidthFixed, 86.0f);
            ImGui::TableSetupColumn(L("Item-ID", "Item ID"), ImGuiTableColumnFlags_WidthFixed, 86.0f);
            ImGui::TableSetupColumn(L("Menge", "Amount"), ImGuiTableColumnFlags_WidthFixed, 66.0f);
            ImGui::TableSetupColumn(L("Rate", "Rate"), ImGuiTableColumnFlags_WidthFixed, 76.0f);
            ImGui::TableSetupColumn(L("Min?", "Min?"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn(L("Max?", "Max?"), ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn(L("Mob", "Mob"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(L("Item", "Item"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableHeadersRow();
            for (std::size_t di = 0; di < q.drops.size(); ++di) {
                ImGui::PushID(static_cast<int>(di) + 200);
                auto& d = q.drops[di];
                int mobId = static_cast<int>(d.mobId), itemId = static_cast<int>(d.itemId), amount = static_cast<int>(d.amount), rate = static_cast<int>(d.rate);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                { bool active=d.active!=0; if(UI::Checkbox("##active",&active)) { d.active=active?1u:0u; markQuestChanged(); } }
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(76.0f); if (UI::InputInt("##m", &mobId, 0, 0)) { d.mobId = static_cast<std::uint32_t>(std::max(0, mobId)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(76.0f); if (UI::InputInt("##i", &itemId, 0, 0)) { d.itemId = static_cast<std::uint32_t>(std::max(0, itemId)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(56.0f); if (UI::InputInt("##a", &amount, 0, 0)) { d.amount = static_cast<std::uint32_t>(std::max(0, amount)); markQuestChanged(); }
                ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(66.0f); if (UI::InputInt("##r", &rate, 0, 0)) { d.rate = static_cast<std::uint32_t>(std::max(0, rate)); markQuestChanged(); }
                // unk1/unk2: in NA2016 immer unk1 <= unk2 (1..20) - vermutlich Mindest-/Höchstanzahl, unbelegt.
                ImGui::TableSetColumnIndex(5); ImGui::SetNextItemWidth(40.0f); if (UI::InputScalar("##u1", ImGuiDataType_U32, &d.unk1)) markQuestChanged();
                ImGui::TableSetColumnIndex(6); ImGui::SetNextItemWidth(40.0f); if (UI::InputScalar("##u2", ImGuiDataType_U32, &d.unk2)) markQuestChanged();
                auto mobName = ResolveMobNameForQuest(state, mobId);
                auto itemName = ResolveItemNameForQuest(state, itemId);
                ImGui::TableSetColumnIndex(7);
                ImGui::TextColored(mobName.second ? kOk : kBad, "%s", mobName.first.c_str());
                if (mobName.second && mobId != 0) {
                    ImGui::SameLine();
                    if (UI::SmallButton(L("Öffnen##dropMobRef", "Open##dropMobRef")))
                        OpenShnRecordById(state, {"MobInfo.shn","MobViewInfo.shn"},
                                          EditorState::ShnSource::Client, mobId);
                }
                ImGui::TableSetColumnIndex(8);
                ImGui::TextColored(itemName.second ? kOk : kBad, "%s", itemName.first.c_str());
                if (itemName.second && itemId != 0) {
                    ImGui::SameLine();
                    if (UI::SmallButton(L("Öffnen##dropItemRef", "Open##dropItemRef")))
                        OpenShnRecordById(state, {"ItemInfo.shn"}, EditorState::ShnSource::Server, itemId);
                }
                ImGui::TableSetColumnIndex(9); if (UI::SmallButton("X")) removeIdx = static_cast<int>(di);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (removeIdx >= 0) { q.drops.erase(q.drops.begin() + removeIdx); markQuestChanged(); }
        if (q.drops.size() < core::legacy::kQuestDropSlots && UI::Button(L("+ Drop hinzufügen", "+ Add drop"))) {
            core::legacy::QuestDrop d;
            d.active = 1; d.amount = 1; d.unk1 = 1; d.unk2 = 1; // häufigste Werte in NA2016
            q.drops.push_back(d);
            markQuestChanged();
        }
    }

    ImGui::Spacing();
    if (state.automationQuestSection == 2) { ImGui::SetScrollHereY(0.0f); state.automationQuestSection = 0; }
    if (UI::CollapsingHeader(L("Belohnungen", "Rewards"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s", L("12 Einträge. Struktur an allen 2304 NA2016-Quests belegt; die Bedeutung von Verwendung "
                                   "und der Typen EXP/Geld/Ruhm ist aus Wertebereichen und Quest-Texten abgeleitet (mit * bzw. ? markiert).",
                                   "12 entries. Structure verified on all 2304 NA2016 quests; the meaning of usage and of "
                                   "the EXP/money/fame types is inferred from value ranges and quest texts (marked with * or ?)."));
        constexpr int kRewardTypes[] = {0, 1, 2, 4};
        const char* typeNames[] = {L("EXP*", "EXP*"), L("Geld*", "Money*"), L("Item", "Item"), L("Ruhm?", "Fame?")};
        const char* useNames[] = {L("unbelegt", "unused"), L("fest*", "fixed*"), L("Auswahl*", "choice*")};
        int clearIdx = -1;
        if (ImGui::BeginTable("##qrewards", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.0f);
            ImGui::TableSetupColumn(L("Verwendung", "Usage"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn(L("Wert", "Value"), ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn(L("Name", "Name"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, 26.0f);
            ImGui::TableHeadersRow();
            for (std::size_t ri = 0; ri < q.rewards.size(); ++ri) {
                auto& e = q.rewards[ri];
                if (e.Empty()) continue;
                ImGui::PushID(static_cast<int>(ri) + 300);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", ri + 1);
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(100.0f);
                if (e.use <= 2) {
                    int use = e.use;
                    if (UI::Combo("##use", &use, useNames, 3)) { e.use = static_cast<std::uint8_t>(use); markQuestChanged(); }
                } else ImGui::Text("%s %u", L("Rohwert", "Raw value"), static_cast<unsigned>(e.use));
                ImGui::TableSetColumnIndex(2); ImGui::SetNextItemWidth(100.0f);
                const auto typeIt = std::find(std::begin(kRewardTypes), std::end(kRewardTypes), static_cast<int>(e.type));
                if (typeIt != std::end(kRewardTypes)) {
                    int typeIdx = static_cast<int>(typeIt - std::begin(kRewardTypes));
                    if (UI::Combo("##type", &typeIdx, typeNames, 4)) {
                        const bool wasItem = e.type == 2;
                        e.type = static_cast<std::uint8_t>(kRewardTypes[typeIdx]);
                        if (wasItem != (e.type == 2)) e.value = e.type == 2 ? (std::uint64_t{1} << 16) : 0;
                        markQuestChanged();
                    }
                } else ImGui::Text("%s %u", L("Typ", "Type"), static_cast<unsigned>(e.type));
                ImGui::TableSetColumnIndex(3);
                if (e.type == 2) {
                    int itemId = e.ItemId(), count = e.ItemCount();
                    ImGui::SetNextItemWidth(80.0f);
                    if (UI::InputInt("##rid", &itemId, 0, 0)) { e.SetItem(static_cast<std::uint16_t>(std::clamp(itemId, 0, 65535)), e.ItemCount()); markQuestChanged(); }
                    ImGui::SameLine(); ImGui::TextUnformatted("×"); ImGui::SameLine();
                    ImGui::SetNextItemWidth(60.0f);
                    if (UI::InputInt("##rcount", &count, 0, 0)) { e.SetItem(e.ItemId(), static_cast<std::uint16_t>(std::clamp(count, 0, 65535))); markQuestChanged(); }
                } else {
                    ImGui::SetNextItemWidth(180.0f);
                    if (UI::InputScalar("##rvalue", ImGuiDataType_U64, &e.value)) markQuestChanged();
                }
                ImGui::TableSetColumnIndex(4);
                if (e.type == 2) {
                    const auto r = ResolveItemNameForQuest(state, e.ItemId());
                    DrawItemIconById(state, e.ItemId(), ImGui::GetTextLineHeight());
                    ImGui::SameLine();
                    ImGui::TextColored(r.second ? kOk : kBad, "%s", r.first.c_str());
                    if (r.second) {
                        ImGui::SameLine();
                        if (UI::SmallButton(L("Öffnen##rewardItemRef", "Open##rewardItemRef")))
                            OpenShnRecordById(state, {"ItemInfo.shn"}, EditorState::ShnSource::Server, e.ItemId());
                    }
                } else if (e.pad != 0 || (e.value >> 32) != 0) {
                    ImGui::TextColored(kDim, "%s", L("ungewöhnliche Rohbytes (in NA2016 nie belegt)", "unusual raw bytes (never set in NA2016)"));
                }
                ImGui::TableSetColumnIndex(5); if (UI::SmallButton("X")) clearIdx = static_cast<int>(ri);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (clearIdx >= 0) { q.rewards[static_cast<std::size_t>(clearIdx)] = {}; markQuestChanged(); }
        const auto freeSlot = std::find_if(q.rewards.begin(), q.rewards.end(), [](const auto& e) { return e.Empty(); });
        if (freeSlot != q.rewards.end()) {
            if (UI::Button(L("+ EXP", "+ EXP"))) { *freeSlot = {1, 0, 0, 0}; markQuestChanged(); }
            ImGui::SameLine();
            if (UI::Button(L("+ Geld", "+ Money"))) { *freeSlot = {1, 1, 0, 0}; markQuestChanged(); }
            ImGui::SameLine();
            if (UI::Button(L("+ Item", "+ Item"))) { *freeSlot = {1, 2, 0, std::uint64_t{1} << 16}; markQuestChanged(); }
        } else {
            ImGui::TextDisabled("%s", L("Alle 12 Einträge belegt.", "All 12 entries in use."));
        }
    }

    if (state.automationQuestSection == 5) { ImGui::SetNextItemOpen(true); ImGui::SetScrollHereY(0.0f); state.automationQuestSection = 0; }
    if (UI::CollapsingHeader(L("Dialoge", "Dialogs"))) {
        ImGui::TextDisabled("%s", L("Alle Texte der Quest aus QuestDialog.shn - Titel, Beschreibung und jede SAY-Zeile der Skripte.",
                                    "All texts of the quest from QuestDialog.shn - title, description and every SAY line of the scripts."));
        DrawQuestTextList(state, q);
        ImGui::Separator();
        static char lookupBuf[16] = "";
        ImGui::SetNextItemWidth(120.0f);
        UI::InputText(L("SAY-Text-ID", "SAY text ID"), lookupBuf, sizeof(lookupBuf), ImGuiInputTextFlags_CharsDecimal);
        if (lookupBuf[0] != '\0') {
            const std::string t = QuestTextOf(state, std::atoi(lookupBuf));
            ImGui::TextWrapped("%s", t.empty() ? L("(kein Text)", "(no text)") : t.c_str());
        }
    }

    if (UI::CollapsingHeader(L("Scripts", "Scripts"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("SAY / IF / GOTO / ACCEPT / CREATE_ITEM / ...");
        auto scriptEditor = [&](const char* label, core::legacy::QuestScript& script) {
            ImGui::TextColored(kDim, "%s", label);
            std::vector<char> buf(script.text.begin(), script.text.end());
            buf.resize(std::max<std::size_t>(buf.size() + 1, 4096));
            if (ImGui::InputTextMultiline((std::string("##script") + label).c_str(), buf.data(), buf.size(),
                                          ImVec2(-1.0f, 110.0f))) {
                script.text.assign(buf.data());
                markQuestChanged();
            }
        };
        scriptEditor(L("Start", "Start"), q.start);
        scriptEditor(L("Action", "Action"), q.action);
        scriptEditor(L("Finish", "Finish"), q.finish);
    }
    ImGui::EndChild();
    TrackQuestEdits(state);
}

} // namespace nge
