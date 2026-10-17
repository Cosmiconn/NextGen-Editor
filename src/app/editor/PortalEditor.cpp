// PortalEditor.cpp
// Portale: Stadtportale, Rückkehrpunkte, Gates und LinkTable.

#include "EditorShared.hpp"

namespace nge {

// Lädt TownPortal.shn (Client, ressystem/) - die Skill-/Schriftrollen-Schnellreiseliste mit
// auswählbarem Ziel (siehe CHANGELOG [0.44.20], Nutzer-Klarstellung zur Portal-Taxonomie).
// Server-Ordner (Server/9Data/Shine) fuer Portal-Daten: derselbe Ordner wie shineTextRoot (Map-
// Editor-Tabs NPCs/Mobs) bzw. shnServerRoot (SHN-Editor) - je nachdem, welcher gesetzt ist.
std::string PortalServerRoot(const EditorState& state) {
    return !state.shineTextRoot.empty() ? state.shineTextRoot : state.shnServerRoot;
}
void EnsureTownPortalLoaded(EditorState& state) {
    if (state.townPortalLoaded) return;
    EnsureNpcDialogRoot(state);
    // Client- und Server-Kopie von TownPortal.shn sind byte-identisch (NA2016 geprueft) - die
    // Server-Kopie dient als Rueckfall, falls nur ein Server-Ordner bekannt ist.
    std::vector<std::pair<std::filesystem::path, core::ProjectOutputSide>> candidates;
    if (!state.npcDialogRessystemRoot.empty())
        candidates.emplace_back(std::filesystem::path(state.npcDialogRessystemRoot) / "TownPortal.shn",
                                core::ProjectOutputSide::Client);
    if (const auto srv = PortalServerRoot(state); !srv.empty())
        candidates.emplace_back(std::filesystem::path(srv) / "TownPortal.shn",
                                core::ProjectOutputSide::Server);
    for (const auto& [sourcePath, side] : candidates) {
        const auto source = side == core::ProjectOutputSide::Client ? EditorState::ShnSource::Client
                                                                     : EditorState::ShnSource::Server;
        state.townPortalDoc = SharedShnDoc(state, "TownPortal.shn", source, sourcePath.parent_path());
        if (state.townPortalDoc >= 0) {
            state.townPortalLoaded = true;
            state.townPortalBaseline = ShnDocFile(state, state.townPortalDoc);
            state.townPortalDirty = state.shnFiles[static_cast<std::size_t>(state.townPortalDoc)].dirty;
            return;
        }
    }
}
// Lädt RecallCoord.txt (Server, World/) - Schriftrollen mit FESTEM Ziel (ein Eintrag pro
// Schriftrollen-Item), ShineText-Format wie NPC.txt/MobRegen.
void EnsureRecallCoordLoaded(EditorState& state) {
    const std::string serverRoot = PortalServerRoot(state);
    if (state.recallCoordLoaded || serverRoot.empty()) return;
    const auto sourcePath = std::filesystem::path(serverRoot) / "World" / "RecallCoord.txt";
    const auto path = PreferProjectOverride(state.project, core::ProjectOutputSide::Server, sourcePath);
    auto result = core::legacy::LoadShineTextFile(path);
    if (result) {
        state.recallCoordFile = std::move(*result);
        state.recallCoordLoaded = true;
        state.recallCoordDirty = false;
    }
}
// Speichert TownPortal.shn in die Client-Kopie (ressystem/) UND - falls dort vorhanden - in die
// Server-Kopie (Server/9Data/Shine/). Beide Dateien sind im Original byte-identisch (NA2016
// geprueft); nur eine davon zu aendern wuerde Client und Server auseinanderlaufen lassen.
// Es wird nie eine NEUE Datei im Server-Baum angelegt, nur eine vorhandene ueberschrieben.
// Änderungen des Portal-Editors (direkte Zellzugriffe) als geänderte Zellen im gemeinsamen
// Dokument markieren - so sieht der SHN-Editor sie und kann das Dokument ebenfalls speichern.
void SyncTownPortalMarks(EditorState& state) {
    if (!state.townPortalLoaded || !state.townPortalDirty || state.townPortalDoc < 0) return;
    MarkShnChangesSince(state.shnFiles[static_cast<std::size_t>(state.townPortalDoc)], state.townPortalBaseline);
}
void SaveTownPortalFiles(EditorState& state) {
    EnsureNpcDialogRoot(state);
    if (state.townPortalDoc < 0) { state.statusMessage = "TownPortal.shn ist nicht geladen."; return; }
    SyncTownPortalMarks(state);
    std::string done, failed;
    std::filesystem::path clientOutput, serverOutput;

    auto saveProjectCopy = [&](const std::filesystem::path& sourcePath,
                               core::ProjectOutputSide side,
                               const char* label) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(sourcePath, ec)) return;
        auto target = ProjectPathForSource(state.project, side, sourcePath);
        std::filesystem::path output;
        std::string error;
        if (!PrepareProjectOutput(target, output, &error)) {
            failed += std::string(label) + ": " + error + " ";
            return;
        }
        auto saved = core::legacy::SaveShnFile(ShnDocFile(state, state.townPortalDoc), output);
        if (saved) {
            done += std::string(done.empty() ? "" : " + ") + label;
            (side == core::ProjectOutputSide::Client ? clientOutput : serverOutput) = output;
        }
        else failed += std::string(label) + ": " + saved.error() + " ";
    };

    if (!state.npcDialogRessystemRoot.empty())
        saveProjectCopy(std::filesystem::path(state.npcDialogRessystemRoot) / "TownPortal.shn",
                        core::ProjectOutputSide::Client, "Client");
    if (const auto srv = PortalServerRoot(state); !srv.empty())
        saveProjectCopy(std::filesystem::path(srv) / "TownPortal.shn",
                        core::ProjectOutputSide::Server, "Server");

    if (!failed.empty()) state.statusMessage = "Fehler beim Projekt-Speichern von TownPortal.shn - " + failed;
    else if (done.empty()) state.statusMessage = "TownPortal.shn: keine vorhandene Client/Server-Quellkopie gefunden.";
    else {
        state.townPortalDirty = false;
        state.statusMessage = "TownPortal.shn als Projekt-Override gespeichert (" + done + ").";
        // Gemeinsame Dokumente beider Seiten auf den gespeicherten Stand bringen und als sauber markieren.
        const auto saved = ShnDocFile(state, state.townPortalDoc);
        state.townPortalBaseline = saved;
        for (auto& d : state.shnFiles) {
            if (LowerAscii(d.file.FileName()) != "townportal.shn") continue;
            const auto& out = d.source == EditorState::ShnSource::Client ? clientOutput : serverOutput;
            if (out.empty()) continue;
            d.file = saved;
            d.file.path = out;
            d.dirty = false;
            ClearShnDirtyCells(d);
        }
        ++state.shnEditCounter;
    }
}
bool SaveRecallCoordFile(EditorState& state) {
    const auto root=PortalServerRoot(state);
    if (root.empty()) {
        state.statusMessage=L("RecallCoord.txt: kein Server-Shine-Ordner bekannt.","RecallCoord.txt: no server Shine folder is known.");
        return false;
    }
    auto saved=SaveProjectShineOverride(
        state.recallCoordFile,
        ProjectServerShinePath(state.project, std::filesystem::path("World") / "RecallCoord.txt"));
    if (saved) {
        state.recallCoordDirty=false;
        state.statusMessage=L("RecallCoord.txt als Projekt-Override gespeichert.","RecallCoord.txt saved as project override.");
        return true;
    }
    state.statusMessage=L("Fehler: ","Error: ")+saved.error();
    return false;
}
// Gemeinsamer Editor für die beiden übrigen Portal-/Teleport-Tabellen (siehe Nutzer-Taxonomie:
// 1. TownPortal = Skill mit auswählbarem Ziel, 4a. RecallCoord = Schriftrolle mit festem Ziel).
// Die "normalen Portale" (2./3./5. - Map-zu-Map, NPC-förmige Gates, Instanz-Tore) sind bereits
// über World/NPC.txt im NPC-Tab des Map-Editors abgedeckt, siehe Rollen Gate/IDGate/
// ModeIDGate/RandomGate.
void DrawPortalEditor(EditorState& state) {
    EnsureTownPortalLoaded(state);
    EnsureRecallCoordLoaded(state);

    DrawPanelHeader("portalDataHeader", L("PORTAL-DATEN","PORTAL DATA"), DrawIconPortal,
                    nullptr, L("Client- und Server-Ziele","Client and server destinations"));
    ImGui::SeparatorText("TownPortal · Skill/Menü");
    if (!state.townPortalLoaded) {
        ImGui::TextWrapped("TownPortal.shn konnte nicht geladen werden (Client-ressystem-Ordner nötig).");
    } else if (ImGui::BeginTable("##townportal", static_cast<int>(ShnDocFile(state, state.townPortalDoc).columns.size()),
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (auto& c : ShnDocFile(state, state.townPortalDoc).columns) ImGui::TableSetupColumn(c.name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < ShnDocFile(state, state.townPortalDoc).rows.size(); ++ri) {
            ImGui::TableNextRow();
            for (std::size_t ci = 0; ci < ShnDocFile(state, state.townPortalDoc).columns.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci));
                ImGui::SetNextItemWidth(-1.0f);
                std::string cur = ShnShortValue(ShnDocFile(state, state.townPortalDoc).rows[ri].values[ci]);
                std::vector<char> buf(cur.begin(), cur.end());
                buf.resize(std::max<std::size_t>(buf.size() + 1, 32));
                if (UI::InputText(("##tp" + std::to_string(ri) + "_" + std::to_string(ci)).c_str(), buf.data(), buf.size())) {
                    auto parsed = core::legacy::ParseShnValue(ShnDocFile(state, state.townPortalDoc).columns[ci], buf.data());
                    if (parsed) {
                        ShnDocFile(state, state.townPortalDoc).rows[ri].values[ci] = std::move(*parsed);
                        state.townPortalDirty = true;
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::BeginDisabled(!state.townPortalDirty);
    if (UI::Button(state.townPortalDirty ? L("TownPortal.shn speichern *","Save TownPortal.shn *")
                                         : L("TownPortal.shn speichern","Save TownPortal.shn"))) {
        SaveTownPortalFiles(state);
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("RecallCoord · Schriftrollen");
    if (!state.recallCoordLoaded) {
        ImGui::TextWrapped("RecallCoord.txt konnte nicht geladen werden (Server-World-Ordner nötig).");
        return;
    }
    auto* table = state.recallCoordFile.FindTable("RecallPoint");
    if (!table) { ImGui::TextDisabled("Keine 'RecallPoint'-Tabelle gefunden."); return; }
    if (ImGui::BeginTable("##recallcoord", static_cast<int>(table->columns.size()), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        for (auto& c : table->columns) ImGui::TableSetupColumn(c.name.c_str());
        ImGui::TableHeadersRow();
        for (std::size_t ri = 0; ri < table->records.size(); ++ri) {
            ImGui::TableNextRow();
            for (std::size_t ci = 0; ci < table->records[ri].values.size(); ++ci) {
                ImGui::TableSetColumnIndex(static_cast<int>(ci));
                ImGui::SetNextItemWidth(-1.0f);
                std::string& v = table->records[ri].values[ci];
                std::vector<char> buf(v.begin(), v.end());
                buf.resize(std::max<std::size_t>(buf.size() + 1, 40));
                if (UI::InputText(("##rc" + std::to_string(ri) + "_" + std::to_string(ci)).c_str(), buf.data(), buf.size())) {
                    v.assign(buf.data());
                    state.recallCoordDirty = true;
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::BeginDisabled(!state.recallCoordDirty);
    if (UI::Button(state.recallCoordDirty ? L("RecallCoord.txt speichern *","Save RecallCoord.txt *")
                                          : L("RecallCoord.txt speichern","Save RecallCoord.txt"))) {
        SaveRecallCoordFile(state);
    }
    ImGui::EndDisabled();
    if (ShortcutPressed(state.shortcutSave)) {
        if (state.townPortalDirty) SaveTownPortalFiles(state);
        if (state.recallCoordDirty) SaveRecallCoordFile(state);
    }
}
int FindShnColumnByName(const core::legacy::ShnFile& f, const std::string& name) {
    for (std::size_t i = 0; i < f.columns.size(); ++i) {
        if (f.columns[i].name == name) return static_cast<int>(i);
    }
    return -1;
}
long long ShnCellInt(const core::legacy::ShnFile& f, std::size_t row, int col) {
    long long v = 0;
    if (col < 0 || row >= f.rows.size() || static_cast<std::size_t>(col) >= f.rows[row].values.size()) return 0;
    ShnValueAsInt(f.rows[row].values[static_cast<std::size_t>(col)], v);
    return v;
}
// Schreibt einen Ganzzahlwert in eine Zelle; Werte ausserhalb des Spaltentyps (z.B. > 255 bei
// UInt8) lehnt ParseShnValue ab - die Zelle bleibt dann unveraendert.
void SetShnCellInt(core::legacy::ShnFile& f, std::size_t row, int col, long long v) {
    if (col < 0 || row >= f.rows.size() || static_cast<std::size_t>(col) >= f.rows[row].values.size()) return;
    v = std::max(0LL, v);
    auto parsed = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(col)], std::to_string(v));
    if (parsed) f.rows[row].values[static_cast<std::size_t>(col)] = std::move(*parsed);
}
std::string TrimAscii(const std::string& in) {
    std::size_t a = 0, b = in.size();
    while (a < b && (in[a] == ' ' || in[a] == '\t' || in[a] == '\r')) ++a;
    while (b > a && (in[b - 1] == ' ' || in[b - 1] == '\t' || in[b - 1] == '\r')) --b;
    return in.substr(a, b - a);
}
void EnsureMapInfoLoaded(EditorState& state) {
    if (state.mapInfoLoaded) return;
    EnsureNpcDialogRoot(state);
    std::vector<std::pair<std::filesystem::path, core::ProjectOutputSide>> candidates;
    if (!state.npcDialogRessystemRoot.empty())
        candidates.emplace_back(std::filesystem::path(state.npcDialogRessystemRoot) / "MapInfo.shn",
                                core::ProjectOutputSide::Client);
    if (const auto srv = PortalServerRoot(state); !srv.empty())
        candidates.emplace_back(std::filesystem::path(srv) / "MapInfo.shn",
                                core::ProjectOutputSide::Server);
    for (const auto& [sourcePath, side] : candidates) {
        const auto source = side == core::ProjectOutputSide::Client ? EditorState::ShnSource::Client
                                                                     : EditorState::ShnSource::Server;
        state.mapInfoDoc = SharedShnDoc(state, "MapInfo.shn", source, sourcePath.parent_path());
        if (state.mapInfoDoc >= 0) { state.mapInfoLoaded = true; return; }
    }
}
// Laedt alle Daten des Portale-Tabs - genau einmal pro Ordner-Kombination (sonst wuerde eine
// fehlende Datei in jedem Frame erneut von der Platte gelesen). Aendert der Nutzer einen
// Ordner, wird neu geladen; ungespeicherte Aenderungen werden dabei verworfen.
void EnsurePortalDataLoaded(EditorState& state) {
    if (state.shineTextRoot.empty() && !state.shnServerRoot.empty()) state.shineTextRoot = state.shnServerRoot;
    EnsureNpcDialogRoot(state);
    const std::string sig = state.npcDialogRessystemRoot + "|" + PortalServerRoot(state);
    if (state.portalDataSig == sig) return;
    state.portalDataSig = sig;
    state.townPortalLoaded = false;
    state.recallCoordLoaded = false;
    state.mapInfoLoaded = false;
    state.selectedPortalKind = kPortalKindNone;
    state.selectedPortalIdx = -1;
    EnsureTownPortalLoaded(state);
    EnsureRecallCoordLoaded(state);
    EnsureMapInfoLoaded(state);
    EnsureNpcTextLoaded(state);
}
// Erster aktiver "Gate_Town"-NPC der offenen Karte in World/NPC.txt - Referenzmarker.
bool FindGateTownNpc(EditorState& state, float& x, float& y) {
    if (!state.npcTextLoaded) return false;
    auto* table = state.npcTextFile.FindTable("ShineNPC");
    if (!table) return false;
    for (std::size_t idx : NpcRecordsForCurrentMap(state)) {
        const auto& rec = table->records[idx];
        if (rec.values.size() > 3 && rec.values[0] == "Gate_Town") {
            x = static_cast<float>(std::atoi(rec.values[2].c_str()));
            y = static_cast<float>(std::atoi(rec.values[3].c_str()));
            return true;
        }
    }
    return false;
}
std::string CurrentMapServerName(EditorState& state) {
    const std::string stem = state.legacySaveStem;
    EnsureMapInfoLoaded(state);
    if (!state.mapInfoLoaded || stem.empty()) return stem;
    const auto& f = ShnDocFile(state, state.mapInfoDoc);
    const int cName = FindShnColumnByName(f, "MapName");
    const int cFolder = FindShnColumnByName(f, "MapFolderName");
    if (cName < 0) return stem;
    std::string byName;
    for (const auto& row : f.rows) {
        if (static_cast<std::size_t>(cName) >= row.values.size()) continue;
        const std::string name = TrimAscii(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cName)]));
        if (cFolder >= 0 && static_cast<std::size_t>(cFolder) < row.values.size() &&
            SameMapName(core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(cFolder)]), stem) && SameMapName(name, stem))
            return name; // Ordner und Name passen: eindeutig
        if (byName.empty() && SameMapName(name, stem)) byName = name;
    }
    return byName.empty() ? stem : byName;
}
// Wiederbelebungspunkt der offenen Karte (MapInfo.shn: MapName == Kartenname).
bool FindMapRegen(EditorState& state, float& x, float& y) {
    if (!state.mapInfoLoaded) return false;
    const auto& f = ShnDocFile(state, state.mapInfoDoc);
    const int cName = FindShnColumnByName(f, "MapName");
    const int cX = FindShnColumnByName(f, "RegenX");
    const int cY = FindShnColumnByName(f, "RegenY");
    if (cName < 0 || cX < 0 || cY < 0) return false;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        if (static_cast<std::size_t>(cName) >= f.rows[r].values.size()) continue;
        if (SameMapName(core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cName)]), CurrentMapServerName(state))) {
            x = static_cast<float>(ShnCellInt(f, r, cX));
            y = static_cast<float>(ShnCellInt(f, r, cY));
            return true;
        }
    }
    return false;
}
LinkTableColumns FindLinkTableColumns(const core::legacy::ShineTable& table) {
    LinkTableColumns c;
    const auto find = [&](const char* name, int fallback) {
        for (std::size_t i = 0; i < table.columns.size(); ++i)
            if (LowerAscii(table.columns[i].name) == LowerAscii(name)) return static_cast<int>(i);
        return fallback;
    };
    c.argument = find("argument", c.argument);
    const int base = c.argument; // Fallback: die übrigen Spalten folgen in fester Reihenfolge
    c.mapServer = find("MapServer", base + 1);
    c.mapClient = find("MapClient", base + 2);
    c.x = find("Coord-X", base + 3);
    c.y = find("Coord-Y", base + 4);
    c.direct = find("Direct", base + 5);
    c.party = find("Party", base + 6);
    return c;
}
const std::string& LinkValue(const core::legacy::ShineRecord& record, int column) {
    static const std::string empty;
    return column >= 0 && static_cast<std::size_t>(column) < record.values.size() ? record.values[static_cast<std::size_t>(column)] : empty;
}
// Alle Teleport-Ziele (TownPortal + Schriftrollen), die auf der offenen Karte liegen.
std::vector<PortalMarker> CollectPortalMarkers(EditorState& state) {
    std::vector<PortalMarker> out;
    if (state.legacySaveStem[0] == '\0') return out;
    const std::string stem = state.legacySaveStem;
    if (state.townPortalLoaded) {
        const auto& f = ShnDocFile(state, state.townPortalDoc);
        const int cMap = FindShnColumnByName(f, "MapName");
        const int cX = FindShnColumnByName(f, "X");
        const int cY = FindShnColumnByName(f, "Y");
        const int cIdx = FindShnColumnByName(f, "Index");
        if (cMap >= 0 && cX >= 0 && cY >= 0) {
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (static_cast<std::size_t>(cMap) >= f.rows[r].values.size()) continue;
                if (!SameMapName(core::legacy::ShnValueToString(f.rows[r].values[static_cast<std::size_t>(cMap)]), stem)) continue;
                PortalMarker m;
                m.kind = kPortalKindTown;
                m.idx = r;
                m.x = static_cast<float>(ShnCellInt(f, r, cX));
                m.y = static_cast<float>(ShnCellInt(f, r, cY));
                m.label = "TownPortal #" + std::to_string(cIdx >= 0 ? ShnCellInt(f, r, cIdx) : static_cast<long long>(r));
                out.push_back(std::move(m));
            }
        }
    }
    if (state.recallCoordLoaded) {
        auto* table = state.recallCoordFile.FindTable("RecallPoint");
        if (table && table->columns.size() >= 5) {
            for (std::size_t r = 0; r < table->records.size(); ++r) {
                const auto& rec = table->records[r];
                if (rec.values.size() < 5 || !SameMapName(rec.values[2], stem)) continue;
                PortalMarker m;
                m.kind = kPortalKindRecall;
                m.idx = r;
                m.x = static_cast<float>(std::atoi(rec.values[3].c_str()));
                m.y = static_cast<float>(std::atoi(rec.values[4].c_str()));
                m.label = TrimAscii(rec.values[0]);
                out.push_back(std::move(m));
            }
        }
    }

    // Ausgehende Karten-Gates sind in World/NPC.txt zweistufig verknüpft:
    // ShineNPC.RoleArg0 (z.B. GateRou1) == LinkTable.argument.
    // LinkTable liefert MapServer/MapClient, Ziel-X/Y, Richtung und Party-Flag.
    if (state.npcTextLoaded) {
        auto* npcs = state.npcTextFile.FindTable("ShineNPC");
        auto* links = state.npcTextFile.FindTable("LinkTable");
        if (npcs && links) {
            const LinkTableColumns lc = FindLinkTableColumns(*links);
            std::unordered_map<std::string,const core::legacy::ShineRecord*> byArgument;
            for (const auto& link : links->records) {
                if (!LinkValue(link, lc.argument).empty())
                    byArgument.emplace(LinkValue(link, lc.argument), &link);
            }
            for (const std::size_t idx : NpcRecordsForCurrentMap(state)) {
                if (idx >= npcs->records.size()) continue;
                const auto& npc = npcs->records[idx];
                if (npc.values.size() < 8 || npc.values[7].empty()) continue;
                const auto it = byArgument.find(npc.values[7]);
                if (it == byArgument.end()) continue;
                const auto& link = *it->second;
                PortalMarker m;
                m.kind = kPortalKindGateLink;
                m.idx = idx;
                m.x = static_cast<float>(std::atof(npc.values[2].c_str()));
                m.y = static_cast<float>(std::atof(npc.values[3].c_str()));
                m.linkKey = npc.values[7];
                m.sourceRole = npc.values.size() > 6 ? npc.values[6] : std::string();
                m.targetMapServer = TrimAscii(LinkValue(link, lc.mapServer));
                m.targetMapClient = TrimAscii(LinkValue(link, lc.mapClient));
                m.targetX = static_cast<float>(std::atof(LinkValue(link, lc.x).c_str()));
                m.targetY = static_cast<float>(std::atof(LinkValue(link, lc.y).c_str()));
                m.targetDirect = std::atoi(LinkValue(link, lc.direct).c_str());
                m.targetParty = std::atoi(LinkValue(link, lc.party).c_str()) != 0;
                const std::string target = !m.targetMapClient.empty() ? m.targetMapClient : m.targetMapServer;
                m.label = (npc.values[0].empty() ? m.linkKey : npc.values[0]) + " -> " + target;
                out.push_back(std::move(m));
            }
        }
    }
    return out;
}
// Setzt die Position des aktuell gewaehlten Ziels (Klick im 2D-View bzw. Eingabefelder).
std::string PortalTargetMapName(const PortalMarker& marker) {
    return !marker.targetMapClient.empty() ? marker.targetMapClient : marker.targetMapServer;
}
// Kartenliste aus dem Client-Ordner des Projekts, falls noch nicht gescannt (z.B. bevor der
// Karten-Startbildschirm geöffnet wurde).
void EnsureDiscoveredMaps(EditorState& state) {
    if (!state.discoveredMaps.empty() || state.project.clientFolder[0] == '\0') return;
    const auto resolution = ResolveMapSearchRootAndScan(state.project.clientFolder);
    state.discoveredMaps = resolution.maps;
    MergeProjectOnlyMaps(state.project, state.discoveredMaps);
    state.lastScannedMapRoot = state.project.clientFolder;
}
int FindDiscoveredMapByName(const EditorState& state, const std::string& mapName) {
    const std::string wanted = LowerAscii(TrimAscii(mapName));
    if (wanted.empty()) return -1;
    for (std::size_t i = 0; i < state.discoveredMaps.size(); ++i) {
        if (LowerAscii(state.discoveredMaps[i].name) == wanted ||
            LowerAscii(std::filesystem::path(state.discoveredMaps[i].iniPath).stem().string()) == wanted)
            return static_cast<int>(i);
    }
    return -1;
}
bool NavigateToPortalTarget(EditorState& state, const PortalMarker& marker) {
    if (marker.kind != kPortalKindGateLink) return false;
    const std::string targetMap = PortalTargetMapName(marker);
    if (targetMap.empty()) {
        state.statusMessage = "Gate-Link '" + marker.linkKey + "' enthält keine Zielkarte.";
        return false;
    }

    // Link auf dieselbe Karte: kein Reload nötig, nur direkt zum Ziel springen.
    if (LowerAscii(targetMap) == LowerAscii(state.legacySaveStem)) {
        state.camera.SetTarget(marker.targetX,
                               state.heightmap.SampleWorld(marker.targetX,marker.targetY) + 25.0f,
                               marker.targetY);
        state.camera.Zoom(260.0f - state.camera.Distance());
        state.statusMessage = "Gate-Ziel fokussiert: " + targetMap + " (" +
                              std::to_string(static_cast<int>(marker.targetX)) + ", " +
                              std::to_string(static_cast<int>(marker.targetY)) + ").";
        return true;
    }

    int mapIndex = FindDiscoveredMapByName(state,targetMap);
    if (mapIndex < 0 && state.project.clientFolder[0] != '\0') {
        const auto resolution = ResolveMapSearchRootAndScan(state.project.clientFolder);
        state.discoveredMaps = resolution.maps;
        MergeProjectOnlyMaps(state.project, state.discoveredMaps);
        state.lastResmapCandidateCount = resolution.candidateCount;
        state.lastResmapFound = resolution.root.has_value();
        state.lastResmapResolvedPath = resolution.root ? resolution.root->string() : std::string();
        state.lastScannedMapRoot = state.project.clientFolder;
        mapIndex = FindDiscoveredMapByName(state,targetMap);
    }

    if (mapIndex < 0) {
        state.mapLauncherView = EditorState::MapLauncherView::Browse;
        state.screen = AppScreen::MapEditorLauncher;
        state.statusMessage = "Zielkarte '" + targetMap + "' wurde im Client-resmap nicht gefunden.";
        return false;
    }

    state.selectedMapIndex = mapIndex;
    std::snprintf(state.legacyMapIniPath,sizeof(state.legacyMapIniPath),"%s",
                  state.discoveredMaps[static_cast<std::size_t>(mapIndex)].iniPath.c_str());

    // Niemals ungespeicherte Kartenänderungen durch einen Navigations-Klick verwerfen.
    if (state.mapDirty) {
        state.mapLauncherView = EditorState::MapLauncherView::Browse;
        state.screen = AppScreen::MapEditorLauncher;
        state.statusMessage = "Zielkarte '" + targetMap +
                              "' ist vorausgewählt. Aktuelle Karte hat ungespeicherte Änderungen und wurde nicht geschlossen.";
        return true;
    }

    if (!OpenMapPathIntoState(state,state.discoveredMaps[static_cast<std::size_t>(mapIndex)].iniPath,true))
        return false;
    state.screen = AppScreen::MapEditorWorkspace;
    state.editMode = EditMode::Portals;
    state.camera.SetTarget(marker.targetX,
                           state.heightmap.SampleWorld(marker.targetX,marker.targetY) + 25.0f,
                           marker.targetY);
    state.camera.Zoom(260.0f - state.camera.Distance());
    state.statusMessage = "Gate-Ziel geöffnet: " + targetMap + " (" +
                          std::to_string(static_cast<int>(marker.targetX)) + ", " +
                          std::to_string(static_cast<int>(marker.targetY)) + ").";
    return true;
}
void SetSelectedPortalPosition(EditorState& state, long long x, long long y) {
    x = std::max(0LL, x);
    y = std::max(0LL, y);
    if (state.selectedPortalKind == kPortalKindTown && state.selectedPortalIdx >= 0) {
        auto& f = ShnDocFile(state, state.townPortalDoc);
        SetShnCellInt(f, static_cast<std::size_t>(state.selectedPortalIdx), FindShnColumnByName(f, "X"), x);
        SetShnCellInt(f, static_cast<std::size_t>(state.selectedPortalIdx), FindShnColumnByName(f, "Y"), y);
        state.townPortalDirty = true;
    } else if (state.selectedPortalKind == kPortalKindRecall && state.selectedPortalIdx >= 0) {
        auto* table = state.recallCoordFile.FindTable("RecallPoint");
        if (!table || static_cast<std::size_t>(state.selectedPortalIdx) >= table->records.size()) return;
        auto& rec = table->records[static_cast<std::size_t>(state.selectedPortalIdx)];
        if (rec.values.size() < 5) return;
        rec.values[3] = std::to_string(x);
        rec.values[4] = std::to_string(y);
        state.recallCoordDirty = true;
    }
}
// Legt einen neuen TownPortal-Eintrag fuer die offene Karte an (Kopie der letzten Zeile als
// Strukturvorlage, naechster freier Index). Startposition: Gate_Town-NPC, sonst Regen-Punkt,
// sonst Kartenmitte. Ob der Client fuer NEUE Ziele weitere Daten braucht (Menuetext o.ae.),
// ist NICHT geprueft - siehe CHANGELOG [0.44.25].
// MapName-Spalte einer TownPortal-Zeile (= Dateiname der Portalbilder).
std::string TownPortalMapName(EditorState& state, std::size_t row) {
    const auto& f = ShnDocFile(state, state.townPortalDoc);
    const int cMap = FindShnColumnByName(f, "MapName");
    if (cMap < 0 || row >= f.rows.size() || static_cast<std::size_t>(cMap) >= f.rows[row].values.size()) return CurrentMapServerName(state);
    const std::string name = TrimAscii(core::legacy::ShnValueToString(f.rows[row].values[static_cast<std::size_t>(cMap)]));
    return name.empty() ? CurrentMapServerName(state) : name;
}
void AddTownPortalForCurrentMap(EditorState& state) {
    auto& f = ShnDocFile(state, state.townPortalDoc);
    const int cIdx = FindShnColumnByName(f, "Index");
    const int cMap = FindShnColumnByName(f, "MapName");
    const int cX = FindShnColumnByName(f, "X");
    const int cY = FindShnColumnByName(f, "Y");
    if (f.rows.empty() || cMap < 0 || cX < 0 || cY < 0) { state.statusMessage = "TownPortal.shn: Spalten MapName/X/Y nicht gefunden."; return; }
    long long maxIdx = -1;
    if (cIdx >= 0) for (std::size_t r = 0; r < f.rows.size(); ++r) maxIdx = std::max(maxIdx, ShnCellInt(f, r, cIdx));
    core::legacy::ShnRow row = f.rows.back();
    f.rows.push_back(std::move(row));
    const std::size_t r = f.rows.size() - 1;
    if (auto pv = core::legacy::ParseShnValue(f.columns[static_cast<std::size_t>(cMap)], CurrentMapServerName(state))) {
        f.rows[r].values[static_cast<std::size_t>(cMap)] = std::move(*pv);
    }
    if (cIdx >= 0) SetShnCellInt(f, r, cIdx, maxIdx + 1);
    float px = 0.0f, py = 0.0f;
    if (!FindGateTownNpc(state, px, py) && !FindMapRegen(state, px, py)) {
        px = static_cast<float>(state.heightmap.Width() > 1 ? state.heightmap.Width() - 1 : 1) * state.heightmap.BlockWidth() * 0.5f;
        py = static_cast<float>(state.heightmap.Height() > 1 ? state.heightmap.Height() - 1 : 1) * state.heightmap.BlockHeight() * 0.5f;
    }
    SetShnCellInt(f, r, cX, static_cast<long long>(std::lround(px)));
    SetShnCellInt(f, r, cY, static_cast<long long>(std::lround(py)));
    state.selectedPortalKind = kPortalKindTown;
    state.selectedPortalIdx = static_cast<int>(r);
    state.townPortalDirty = true;
    // Ohne <MapName>.dds/_disable.dds zeigt der Client das Ziel nicht richtig an: gleich erzeugen.
    const std::string mapName = CurrentMapServerName(state);
    if (GameResmenuPath(state, core::legacy::TownPortalImageRelativePath(mapName, false)).empty() ||
        GameResmenuPath(state, core::legacy::TownPortalImageRelativePath(mapName, true)).empty()) {
        std::vector<std::string> report;
        GenerateTownPortalImages(state, mapName, px, py, report);
        std::string summary = L("TownPortal-Ziel angelegt", "TownPortal target added");
        for (const auto& line : report) summary += "  ·  " + line;
        state.statusMessage = summary;
    }
}
// Generischer Editor fuer einen ShineText-Record: Zahlenspalten (DWRD/BYTE/WORD) als Zahlenfeld,
// alles andere als Textfeld. `firstEditable` = erste bearbeitbare Spalte (z.B. 1, wenn die erste
// der Verknuepfungsname ist). Fehlen hinten getrimmte Leerwerte, wird NUR bei einer Aenderung
// auf die volle Spaltenzahl aufgefuellt (unveraenderte Records bleiben byte-identisch).
bool DrawShineRecordFields(std::vector<std::string>& values, const std::vector<core::legacy::ShineColumn>& columns,
                           std::size_t firstEditable) {
    std::vector<std::string> edited = values;
    if (edited.size() < columns.size()) edited.resize(columns.size(), "0");
    bool changed = false;
    for (std::size_t i = firstEditable; i < columns.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const std::string& type = columns[i].type;
        const std::string label = columns[i].name.empty() ? ("Spalte " + std::to_string(i)) : columns[i].name;
        if (type == "DWRD" || type == "BYTE" || type == "WORD") {
            int v = std::atoi(edited[i].c_str());
            ImGui::SetNextItemWidth(150.0f);
            if (UI::InputInt(label.c_str(), &v, 1, 50)) {
                if (type == "BYTE") v = std::clamp(v, 0, 255);
                edited[i] = std::to_string(v);
                changed = true;
            }
        } else {
            std::vector<char> buf(edited[i].begin(), edited[i].end());
            buf.resize(std::max<std::size_t>(buf.size() + 1, 64));
            ImGui::SetNextItemWidth(150.0f);
            if (UI::InputText(label.c_str(), buf.data(), buf.size())) { edited[i].assign(buf.data()); changed = true; }
        }
        ImGui::PopID();
    }
    if (changed) values = std::move(edited);
    return changed;
}
// Textfeld (+ unter Windows Ordner-Dialog) fuer einen noch nicht gefundenen Ordner.
bool DrawPortalFolderField(const char* hint, const char* id, std::string& value) {
    bool changed = false;
    ImGui::TextWrapped("%s", hint);
    std::vector<char> buf(value.begin(), value.end());
    buf.resize(std::max<std::size_t>(buf.size() + 1, 512));
    ImGui::SetNextItemWidth(-1.0f);
    if (UI::InputText((std::string("##") + id).c_str(), buf.data(), buf.size())) { value.assign(buf.data()); changed = true; }
#ifdef _WIN32
    if (UI::Button((std::string("Ordner wählen...##") + id).c_str())) {
        if (auto p = BrowseForFolderWindows(hint)) { value = *p; changed = true; }
    }
#endif
    return changed;
}
void DrawPortalsToolsPanel(EditorState& state) {
    EnsurePortalDataLoaded(state);

    if (state.npcDialogRessystemRoot.empty()) {
        if (DrawPortalFolderField(L("Client-ressystem-Ordner (enthält TownPortal.shn, MapInfo.shn) noch nicht gefunden.","Client ressystem folder (containing TownPortal.shn, MapInfo.shn) was not found."), "portal_ressystem", state.npcDialogRessystemRoot)) state.portalDataSig.clear();
    }
    if (PortalServerRoot(state).empty()) {
        if (DrawPortalFolderField(L("Server-Ordner Server/9Data/Shine (enthält World/RecallCoord.txt, World/NPC.txt) noch nicht gewählt.","Server folder Server/9Data/Shine (containing World/RecallCoord.txt, World/NPC.txt) has not been selected yet."), "portal_server", state.shineTextRoot)) state.portalDataSig.clear();
    }
    if (state.legacySaveStem[0] == '\0') { ImGui::TextDisabled("%s",L("Keine Karte offen.","No map open.")); return; }

    const auto markers = CollectPortalMarkers(state);
    // Auswahl bereinigen (z.B. nach Kartenwechsel oder Neuladen).
    bool selectionValid = false;
    for (const auto& m : markers) if (m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx) selectionValid = true;
    if (!selectionValid) { state.selectedPortalKind = kPortalKindNone; state.selectedPortalIdx = -1; }

    if (!state.townPortalLoaded) ImGui::TextWrapped("%s",L("TownPortal.shn konnte nicht geladen werden.","TownPortal.shn could not be loaded."));
    if (!state.recallCoordLoaded) ImGui::TextWrapped("%s",L("World/RecallCoord.txt konnte nicht geladen werden.","World/RecallCoord.txt could not be loaded."));

    ImGui::TextDisabled(L("%zu Teleport-Ziele auf '%s' · Auswahl links im Szene-Outliner","%zu teleport targets on '%s' · select in the Scene Outliner on the left"),
                        markers.size(), state.legacySaveStem);
    ImGui::TextDisabled("%s",L("Raute = TownPortal · Dreieck = Schriftrolle · Quadrat = Gate_Town · Ring = Wiederbelebung","Diamond = TownPortal · triangle = scroll · square = Gate_Town · ring = respawn"));

    ImGui::SeparatorText(L("Auswahl","Selection"));
    if (state.selectedPortalKind == kPortalKindNone)
        ImGui::TextDisabled("%s",L("Portal im Szene-Outliner oder in der 2D-Ansicht auswählen.","Select a portal in the Scene Outliner or the 2D view."));

    for (const auto& m : markers) {
        if (m.kind != state.selectedPortalKind || static_cast<int>(m.idx) != state.selectedPortalIdx) continue;
        const char* kindText = m.kind == kPortalKindTown ? L("TownPortal · auswählbares Ziel","TownPortal · selectable target")
                             : m.kind == kPortalKindRecall ? L("RecallCoord · festes Schriftrollen-Ziel","RecallCoord · fixed scroll target")
                             : L("World/NPC.txt · ausgehende Gate-Verknüpfung","World/NPC.txt · outbound gate link");
        ImGui::TextColored(UiTheme::AccentCyan, "%s", m.label.c_str());
        ImGui::TextDisabled("%s",kindText);
        ImGui::SeparatorText(m.kind == kPortalKindGateLink ? L("Ausgangspunkt","Origin") : "Position");
        int x = static_cast<int>(m.x), y = static_cast<int>(m.y);
        if (m.kind == kPortalKindGateLink) {
            ImGui::Text("X: %d   Y: %d",x,y);
        } else {
            bool changed = false;
            changed |= UI::InputInt("X", &x, 1, 50);
            changed |= UI::InputInt("Y", &y, 1, 50);
            if (changed) SetSelectedPortalPosition(state, x, y);
        }
        if (UI::Button(L("Kamera auf Portal","Camera to portal"), ImVec2(-1,0))) {
            const float wx=static_cast<float>(x), wz=static_cast<float>(y);
            state.camera.SetTarget(wx,state.heightmap.SampleWorld(wx,wz)+25.0f,wz);
            state.camera.Zoom(260.0f-state.camera.Distance());
        }

        if (m.kind == kPortalKindGateLink) {
            ImGui::SeparatorText(L("Ziel","Target"));
            ImGui::Text("Link: %s",m.linkKey.c_str());
            if (!m.sourceRole.empty()) ImGui::TextDisabled(L("Rolle: %s","Role: %s"),m.sourceRole.c_str());
            ImGui::Text(L("Client-Karte: %s","Client map: %s"),m.targetMapClient.empty() ? L("(leer)","(empty)") : m.targetMapClient.c_str());
            ImGui::Text(L("Server-Karte: %s","Server map: %s"),m.targetMapServer.empty() ? L("(leer)","(empty)") : m.targetMapServer.c_str());
            ImGui::Text(L("Ziel X/Y: %.0f / %.0f","Target X/Y: %.0f / %.0f"),m.targetX,m.targetY);
            ImGui::TextDisabled(L("Richtung: %d · Party: %s","Direction: %d · Party: %s"),m.targetDirect,m.targetParty ? L("ja","yes") : L("nein","no"));
            if (DrawIconButton("openGateTarget",L("Ziel öffnen","Open target"),DrawIconPortal,false,ImVec2(100,58)))
                NavigateToPortalTarget(state,m);
        }

        if (m.kind == kPortalKindTown) {
            ImGui::SeparatorText(L("Bedingungen","Conditions"));
            auto& f = ShnDocFile(state, state.townPortalDoc);
            const int cLvl = FindShnColumnByName(f, "MinLevel");
            const int cGrp = FindShnColumnByName(f, "TP_GroupNo");
            if (cLvl >= 0) {
                int v = static_cast<int>(ShnCellInt(f, m.idx, cLvl));
                if (UI::InputInt(L("Mindestlevel","Minimum level"), &v)) {
                    SetShnCellInt(f, m.idx, cLvl, std::clamp(v, 0, 255));
                    state.townPortalDirty=true;
                }
            }
            if (cGrp >= 0) {
                int v = static_cast<int>(ShnCellInt(f, m.idx, cGrp));
                if (UI::InputInt(L("Menü-Gruppe","Menu group"), &v)) {
                    SetShnCellInt(f, m.idx, cGrp, std::clamp(v, 0, 255));
                    state.townPortalDirty=true;
                }
            }
        }
        float gx = 0.0f, gy = 0.0f;
        if (FindGateTownNpc(state, gx, gy)) {
            ImGui::TextDisabled(L("Abstand zum Gate_Town-NPC: %.0f","Distance to Gate_Town NPC: %.0f"), std::sqrt((gx - m.x) * (gx - m.x) + (gy - m.y) * (gy - m.y)));
        }
        break;
    }
    ImGui::SeparatorText(L("Positionieren","Positioning"));
    const bool canRepositionPortal = state.selectedPortalKind == kPortalKindTown ||
                                     state.selectedPortalKind == kPortalKindRecall;
    ImGui::BeginDisabled(!canRepositionPortal);
    UI::Checkbox(L("Position per Klick im 2D-View setzen","Set position by clicking in the 2D view"), &state.portalPickMode);
    ImGui::EndDisabled();
    if (!canRepositionPortal) state.portalPickMode = false;
    if (state.portalPickMode && state.selectedPortalKind == kPortalKindNone)
        ImGui::TextDisabled("%s",L("Zuerst ein Ziel im Szene-Outliner oder in der 2D-Ansicht wählen.","Select a target in the Scene Outliner or 2D view first."));

    ImGui::SeparatorText(L("Aktionen","Actions"));
    if (state.townPortalLoaded) {
        bool hasTown = false;
        for (const auto& m : markers) if (m.kind == kPortalKindTown) hasTown = true;
        if (!hasTown) {
            ImGui::TextDisabled("%s",L("Diese Karte hat noch kein TownPortal-Ziel.","This map does not have a TownPortal target yet."));
        }
        if (UI::Button(L("TownPortal-Ziel hier hinzufügen","Add TownPortal target here"))) AddTownPortalForCurrentMap(state);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",L("Legt einen neuen Eintrag in TownPortal.shn an und erzeugt die Portalbilder (resmenu/TownPortal/<Karte>.dds und _disable.dds), falls sie fehlen.",
                                                             "Creates a new entry in TownPortal.shn and generates the portal images (resmenu/TownPortal/<map>.dds and _disable.dds) if they are missing."));

        // Portalbilder des (ausgewählten bzw. ersten) TownPortal-Ziels dieser Karte.
        const PortalMarker* town = nullptr;
        for (const auto& m : markers) {
            if (m.kind != kPortalKindTown) continue;
            if (!town || (state.selectedPortalKind == kPortalKindTown && state.selectedPortalIdx == static_cast<int>(m.idx))) town = &m;
        }
        if (town) {
            const std::string mapName = TownPortalMapName(state, town->idx);
            ImGui::SeparatorText(L("Portalbilder","Portal images"));
            for (const bool grey : {false, true}) {
                bool fromProject = false;
                const auto path = GameResmenuPath(state, core::legacy::TownPortalImageRelativePath(mapName, grey), &fromProject);
                const std::uint32_t tex = path.empty() ? 0 : GetOrLoadAssetThumbnail(state, path, false).tex;
                ImGui::BeginGroup();
                if (tex) {
                    // DDS-Vorschaubilder liegen in OpenGL-Zeilenfolge vor: V spiegeln.
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(tex)), ImVec2(96, 96), ImVec2(0, 1), ImVec2(1, 0));
                } else {
                    ImGui::Dummy(ImVec2(96, 96));
                    const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(r0, r1, IM_COL32(240, 82, 94, 200), 6.0f);
                }
                ImGui::TextDisabled("%s%s", grey ? "_disable" : L("aktiv","enabled"),
                                    tex ? (fromProject ? L(" · Projekt"," · project") : " · Client") : L(" · fehlt"," · missing"));
                ImGui::EndGroup();
                if (!grey) ImGui::SameLine();
            }
            ImGui::SetNextItemWidth(160.0f);
            UI::SliderFloat(L("Blickrichtung","View direction"), &state.townPortalImageYaw, -3.1416f, 3.1416f, "%.2f rad");
            if (UI::Button(L("Portalbilder erzeugen","Generate portal images"))) {
                std::vector<std::string> report;
                GenerateTownPortalImages(state, mapName, town->x, town->y, report);
                std::string summary;
                for (const auto& line : report) summary += (summary.empty() ? "" : "  ·  ") + line;
                state.statusMessage = summary;
            }
        }
    }

    ImGui::BeginDisabled(!state.townPortalDirty);
    if (state.townPortalLoaded && UI::Button(state.townPortalDirty
            ? L("TownPortal.shn speichern *","Save TownPortal.shn *")
            : L("TownPortal.shn speichern","Save TownPortal.shn"), ImVec2(-1,0)))
        SaveTownPortalFiles(state);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!state.recallCoordDirty);
    if (state.recallCoordLoaded && UI::Button(state.recallCoordDirty
            ? L("RecallCoord.txt speichern *","Save RecallCoord.txt *")
            : L("RecallCoord.txt speichern","Save RecallCoord.txt"), ImVec2(-1,0)))
        SaveRecallCoordFile(state);
    ImGui::EndDisabled();
    if (UI::Button(L("Verwerfen und neu laden","Discard and reload"), ImVec2(-1,0))) {
        state.townPortalDirty=false;
        state.recallCoordDirty=false;
        state.portalDataSig.clear();
    }
}
// Zeichnet die Portal-Marker in den 2D-View (Farben wie im restlichen Theme: Weiss = gewaehlt,
// Blau = uebrige, helles Grau = Referenz).
void DrawPortalMarkers2D(EditorState& state, const ImVec2& origin, const ImVec2& size, float spanX, float spanZ) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto toScreen = [&](float wx, float wy) {
        return ImVec2(origin.x + (spanX > 0.0f ? wx / spanX : 0.0f) * size.x,
                      origin.y + (1.0f - (spanZ > 0.0f ? wy / spanZ : 0.0f)) * size.y);
    };
    const ImU32 refCol = IM_COL32(170, 185, 205, 210);
    float gx = 0.0f, gy = 0.0f, rx = 0.0f, ry = 0.0f;
    const bool hasGate = FindGateTownNpc(state, gx, gy);
    const bool hasRegen = FindMapRegen(state, rx, ry);
    if (hasGate) {
        const ImVec2 c = toScreen(gx, gy);
        dl->AddRect(ImVec2(c.x - 4.0f, c.y - 4.0f), ImVec2(c.x + 4.0f, c.y + 4.0f), refCol, 0.0f, 0, 1.5f);
    }
    if (hasRegen) {
        const ImVec2 c = toScreen(rx, ry);
        dl->AddCircle(c, 6.0f, IM_COL32(230, 230, 235, 170), 16, 1.25f);
    }
    for (const auto& m : CollectPortalMarkers(state)) {
        const bool selected = m.kind == state.selectedPortalKind && static_cast<int>(m.idx) == state.selectedPortalIdx;
        const ImU32 col = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(70, 135, 210, 230);
        const ImVec2 c = toScreen(m.x, m.y);
        const float r = selected ? 7.0f : 5.5f;
        if (m.kind == kPortalKindTown) {
            if (hasGate) dl->AddLine(toScreen(gx, gy), c, IM_COL32(170, 185, 205, 90), 1.0f);
            const ImVec2 pts[4] = {ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y)};
            dl->AddConvexPolyFilled(pts, 4, col);
        } else if (m.kind == kPortalKindRecall) {
            dl->AddTriangleFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y + r), ImVec2(c.x - r, c.y + r), col);
        } else {
            dl->AddRectFilled(ImVec2(c.x-r,c.y-r),ImVec2(c.x+r,c.y+r),col,2.0f);
        }
        if (selected) dl->AddText(ImVec2(c.x + r + 4.0f, c.y - r), col, m.label.c_str());
    }
}

} // namespace nge
