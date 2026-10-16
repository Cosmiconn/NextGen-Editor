// CreatureWizard.cpp
// Custom NPC/Mob: Anlegen, Avatar-Vorschau, Editor.

#include "EditorShared.hpp"

namespace nge {

void RunCreateCreature(EditorState& state) {
    auto& w = state.wiz;
    w.report.clear();
    const std::string newInx = w.newInx;
    if (newInx.empty() || w.templateId < 0) { w.report.push_back("Bitte Vorlage und InxName angeben."); return; }
    // Eindeutigkeit des Namens
    {
        const int mi = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client);
        if (mi < 0) { w.report.push_back("MobInfo.shn (Client) nicht geladen - Projekt-Ordner prüfen."); return; }
        const auto& f = state.shnFiles[static_cast<std::size_t>(mi)].file;
        for (std::size_t r = 0; r < f.rows.size(); ++r) {
            if (ShnCellText(f, r, "InxName") == newInx) { w.report.push_back("InxName '" + newInx + "' existiert bereits in MobInfo.shn."); return; }
        }
    }
    // Freie ID nur ueber die verifizierte Pflichtfamilie bestimmen. Unverifizierte/bedingte
    // Tabellen duerfen weder die ID-Vergabe noch den Clone-Vorgang still beeinflussen.
    struct Spec { const char* file; EditorState::ShnSource src; const char* nameCol; };
    const Spec specs[] = {
        {"MobInfo.shn", EditorState::ShnSource::Client, "InxName"},
        {"MobInfo.shn", EditorState::ShnSource::Server, "InxName"},
        {"MobInfoServer.shn", EditorState::ShnSource::Server, "InxName"},
        {"MobViewInfo.shn", EditorState::ShnSource::Client, "InxName"},
        {"MobViewInfo.shn", EditorState::ShnSource::Server, "InxName"},
    };
    std::set<long long> used;
    const core::legacy::ShnColumn* idColumn = nullptr;
    for (const auto& sp : specs) {
        const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
        if (di < 0) continue;
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int c = ShnColumnIndexByName(f, "ID");
        CollectShnInts(f, c, used);
        if (!idColumn && std::string(sp.file) == "MobInfo.shn" && c >= 0) idColumn = &f.columns[static_cast<std::size_t>(c)];
    }
    long long newId = w.autoId ? -1 : w.manualId;
    if (w.autoId) {
        if (!idColumn) { w.report.push_back("Keine ID-Spalte gefunden."); return; }
        newId = SuggestFreeShnId(*idColumn, used);
    } else if (used.count(newId)) {
        w.report.push_back("ID " + std::to_string(newId) + " ist bereits vergeben.");
        return;
    }
    if (newId < 0) { w.report.push_back("Keine freie ID gefunden."); return; }
    w.report.push_back("Neue ID: " + std::to_string(newId) + "   InxName: " + newInx + "   Vorlage: " + w.templateInx + " (#" + std::to_string(w.templateId) + ")");

    // 1) Klonen
    std::vector<std::size_t> viewRowsClient, viewRowsServer;
    for (const auto& sp : specs) {
        auto created = CloneRowsById(state, sp.file, sp.src, sp.nameCol, w.templateId, w.templateInx, newId, newInx, w.report);
        const std::string fname = sp.file;
        const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
        if (di < 0 || created.empty()) continue;
        auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        for (const std::size_t nr : created) {
            if (fname == "MobInfo.shn") {
                SetShnCellText(f, nr, "Name", w.displayName);
                SetShnCellText(f, nr, "IsNPC", w.isNpc ? "1" : "0");
                if (!w.isNpc) {
                    SetShnCellText(f, nr, "Level", std::to_string(w.level));
                    SetShnCellText(f, nr, "MaxHP", std::to_string(w.maxHp));
                    SetShnCellText(f, nr, "WalkSpeed", std::to_string(w.walkSpeed));
                    SetShnCellText(f, nr, "RunSpeed", std::to_string(w.runSpeed));
                    SetShnCellText(f, nr, "Size", std::to_string(w.size));
                }
            } else if (fname == "MobViewInfo.shn") {
                (sp.src == EditorState::ShnSource::Client ? viewRowsClient : viewRowsServer).push_back(nr);
                if (w.lookMode == 1 && w.modelFile[0]) SetShnCellText(f, nr, "FileName", w.modelFile);
            }
        }
    }

    // 2) Spieler-Avatar (NPCViewInfo) - nur NPC
    if (w.isNpc && w.lookMode == 2) {
        const int vi = FindOrLoadShnDoc(state, "NPCViewInfo.shn", EditorState::ShnSource::Client);
        if (vi < 0) {
            w.report.push_back("- Client NPCViewInfo.shn: nicht geladen - Avatar-Look nicht angelegt.");
        } else {
            auto& doc = state.shnFiles[static_cast<std::size_t>(vi)];
            auto& f = doc.file;
            std::set<long long> usedTypes;
            CollectShnInts(f, ShnColumnIndexByName(f, "TypeIndex"), usedTypes);
            const int ct = ShnColumnIndexByName(f, "TypeIndex");
            const long long typeIdx = ct >= 0 ? SuggestFreeShnId(f.columns[static_cast<std::size_t>(ct)], usedTypes) : -1;
            // Strukturvorlage: die Zeile, auf die die Vorlage-Kopie zeigt (falls Avatar), sonst die erste.
            std::size_t srcRow = 0;
            if (!viewRowsClient.empty()) {
                long long tplView = 0;
                {
                    const int mvc = FindOrLoadShnDoc(state, "MobViewInfo.shn", EditorState::ShnSource::Client);
                    if (mvc >= 0) {
                        const std::string txt = ShnCellText(state.shnFiles[static_cast<std::size_t>(mvc)].file, viewRowsClient[0], "NpcViewIndex");
                        tplView = std::atoll(txt.c_str());
                    }
                }
                for (std::size_t r = 0; r < f.rows.size(); ++r) {
                    long long t = -1;
                    if (ct >= 0 && ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(ct)], t) && t == tplView && tplView != 0) { srcRow = r; break; }
                }
            }
            if (typeIdx < 0 || f.rows.empty()) {
                w.report.push_back("- Client NPCViewInfo.shn: keine freie TypeIndex-Nummer - Avatar-Look nicht angelegt.");
            } else {
                core::legacy::ShnRow row = f.rows[srcRow];
                f.rows.push_back(std::move(row));
                const std::size_t nr = f.rows.size() - 1;
                SetShnCellText(f, nr, "TypeIndex", std::to_string(typeIdx));
                SetShnCellText(f, nr, "Class", std::to_string(w.avClass));
                SetShnCellText(f, nr, "Gender", std::to_string(w.avGender));
                SetShnCellText(f, nr, "FaceShape", std::to_string(w.avFace));
                SetShnCellText(f, nr, "HairType", std::to_string(w.avHairType));
                SetShnCellText(f, nr, "HairColor", std::to_string(w.avHairColor));
                for (int i = 0; i < 19; ++i) SetShnCellText(f, nr, kAvatarSlotColumns[i], w.equ[static_cast<std::size_t>(i)].empty() ? "-" : w.equ[static_cast<std::size_t>(i)]);
                doc.dirty = true;
                EnsureCellStatusSize(doc);
                if (nr < doc.cellDirty.size())
                    std::fill(doc.cellDirty[nr].begin(), doc.cellDirty[nr].end(), 1);
                ++state.shnEditCounter;
                w.report.push_back("+ Client NPCViewInfo.shn: Avatar-Zeile TypeIndex " + std::to_string(typeIdx) + " angelegt");
                for (const auto src : {EditorState::ShnSource::Client, EditorState::ShnSource::Server}) {
                    const int mvi = FindOrLoadShnDoc(state, "MobViewInfo.shn", src);
                    if (mvi < 0) continue;
                    auto& mv = state.shnFiles[static_cast<std::size_t>(mvi)];
                    for (const std::size_t vr : (src == EditorState::ShnSource::Client ? viewRowsClient : viewRowsServer)) {
                        SetShnCellText(mv.file, vr, "NpcViewIndex", std::to_string(typeIdx));
                    }
                }
            }
        }
    }

    // 3) Dialog (NPC)
    if (w.isNpc && w.copyDialog) {
        const int di = FindOrLoadShnDoc(state, "NpcDialogData.shn", EditorState::ShnSource::Client);
        if (di >= 0) {
            auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
            for (std::size_t r = 0; r < doc.file.rows.size(); ++r) {
                if (ShnCellText(doc.file, r, "MobIDX") != w.templateInx) continue;
                core::legacy::ShnRow row = doc.file.rows[r];
                doc.file.rows.push_back(std::move(row));
                const std::size_t nr = doc.file.rows.size() - 1;
                SetShnCellText(doc.file, nr, "MobIDX", newInx);
                doc.dirty = true;
                EnsureCellStatusSize(doc);
                if (nr < doc.cellDirty.size())
                    std::fill(doc.cellDirty[nr].begin(), doc.cellDirty[nr].end(), 1);
                ++state.shnEditCounter;
                w.report.push_back("+ Client NpcDialogData.shn: Dialog der Vorlage kopiert");
                break;
            }
        }
    }

    // 4) Platzierung in NPC.txt (nur NPC)
    if (w.isNpc && w.placeOnMap && state.legacySaveStem[0] != '\0') {
        EnsureNpcTextLoaded(state);
        if (auto* table = state.npcTextFile.FindTable("ShineNPC")) {
            core::legacy::ShineRecord rec;
            rec.sourceLine = 0;
            const std::string role = kNpcRoles[std::clamp(w.roleIdx, 0, 5)];
            rec.values = {newInx, CurrentMapServerName(state), std::to_string(w.placeX), std::to_string(w.placeY), std::to_string(w.placeDir), "1", role, w.roleArg};
            table->records.push_back(std::move(rec));
            w.report.push_back(std::string("+ World/NPC.txt: NPC auf '") + state.legacySaveStem + "' bei (" + std::to_string(w.placeX) + ", " + std::to_string(w.placeY) + ") vorgemerkt (Speichern im NPC-Tab oder unten).");
        } else {
            w.report.push_back("- World/NPC.txt nicht geladen - nicht platziert.");
        }
    }
    w.report.push_back(
        "BEDINGT und deshalb nicht automatisch geändert: MobSpecies.shn, QuestSpecies.shn, "
        "MobWeapon.shn, MobResist.shn, MobLifeTime.shn und AI/Roam-Dateien. "
        "Nur bei tatsächlich genutzter Funktion im passenden Editor ergänzen.");
    w.report.push_back("Fertig. Die geänderten Tabellen sind noch NICHT gespeichert - unten 'Alle geänderten SHN speichern'.");
}
// Baut (bei geaenderten Parametern) das Avatar-Vorschaumodell und rendert es in die GL-Textur.
bool g_disableGlUploads = false;
 // Tests ohne GL
// Baut die Avatar-Anfrage (Klasse, Geschlecht, Gesicht, Haare, Ausruestung) inkl. ItemViewInfo-/HairInfo-Daten
// - gemeinsam genutzt von der Assistenten-Vorschau und vom Rendern von Avatar-NPCs im 3D-Editor.
core::AvatarRequest MakeAvatarRequest(EditorState& state, int cls, bool male, int face, int hairType, const std::array<std::string, 19>& equ) {
    // Item-Daten (ItemViewInfo: Textur, Set-Nummer, Link-Datei) einmal aufbauen
    if (!state.itemViewBuilt) {
        state.itemViewBuilt = true;
        state.itemViewByInx.clear();
        const int vi = FindOrLoadShnDoc(state, "ItemViewInfo.shn", EditorState::ShnSource::Client);
        if (vi >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(vi)].file;
            const int cInx = ShnColumnIndexByName(f, "InxName"), cTex = ShnColumnIndexByName(f, "TextureFile"), cLink = ShnColumnIndexByName(f, "LinkFile");
            const int cM = ShnColumnIndexByName(f, "MSetNo"), cF = ShnColumnIndexByName(f, "FSetNo");
            for (std::size_t r = 0; r < f.rows.size() && cInx >= 0; ++r) {
                EditorState::ItemViewData d;
                d.textureFile = cTex >= 0 ? ShnCellText(f, r, "TextureFile") : std::string();
                d.linkFile = cLink >= 0 ? ShnCellText(f, r, "LinkFile") : std::string();
                d.mSet = cM >= 0 ? std::atoi(ShnCellText(f, r, "MSetNo").c_str()) : 0;
                d.fSet = cF >= 0 ? std::atoi(ShnCellText(f, r, "FSetNo").c_str()) : 0;
                state.itemViewByInx.emplace(ShnCellText(f, r, "InxName"), d);
            }
        }
    }
    core::AvatarRequest req;
    req.charRoot = state.charRoot;
    req.itemRoot = state.itemRoot;
    req.classIdx = cls;
    req.male = male;
    req.faceShape = std::max(1, face);
    // Haare aus HairInfo (ID = Frisur)
    {
        const int hi = FindOrLoadShnDoc(state, "HairInfo.shn", EditorState::ShnSource::Client);
        if (hi >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(hi)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r) {
                if (std::atoi(ShnCellText(f, r, "ID").c_str()) != hairType) continue;
                req.hairFront = ShnCellText(f, r, "acModelName_Front");
                req.hairBottom = ShnCellText(f, r, "acModelName_Bottom");
                req.hairTop = ShnCellText(f, r, "acModelName_Top");
                req.hairTexture = ShnCellText(f, r, "FrontTex");
                break;
            }
        }
    }
    static const char* const kSlotOf[19] = {"RightHand", "LeftHand", "Body", "Leg", "Shoes"};
    for (int i = 0; i < 5; ++i) {
        const std::string& inx = equ[static_cast<std::size_t>(i)];
        if (inx.empty() || inx == "-") continue;
        const auto it = state.itemViewByInx.find(inx);
        core::AvatarItem item;
        item.slot = kSlotOf[i];
        if (it != state.itemViewByInx.end()) {
            item.textureFile = it->second.textureFile;
            item.linkFile = it->second.linkFile;
            item.setNo = req.male ? it->second.mSet : it->second.fSet;
        }
        req.items.push_back(std::move(item));
    }
    return req;
}
void UpdateAvatarPreview(EditorState& state) {
    auto& w = state.wiz;
    if (state.charRoot.empty()) { state.avatarError = "Ordner 'reschar' nicht gefunden (Client-Ordner im Projekt prüfen)."; state.avatarModel.reset(); return; }
    core::AvatarRequest req = MakeAvatarRequest(state, w.avClass, w.avGender != 0, w.avFace, w.avHairType, w.equ);
    std::string key = std::to_string(req.classIdx) + (req.male ? "m" : "f") + std::to_string(req.faceShape) + "|" + req.hairFront + "|";
    for (const auto& item : req.items) key += item.slot + ":" + item.textureFile + ":" + std::to_string(item.setNo) + ":" + item.linkFile + ";";
    if (key != state.avatarKey) {
        state.avatarKey = key;
        auto built = core::BuildAvatarModel(req);
        if (built) { state.avatarModel = std::make_shared<const core::AvatarModel>(std::move(*built)); state.avatarError.clear(); }
        else { state.avatarModel.reset(); state.avatarError = built.error(); }
        state.avatarRenderedYaw = 1e9f; // neu rendern
    }
    if (!state.avatarModel || g_disableGlUploads) return;
    if (state.avatarTex != 0 && state.avatarYaw == state.avatarRenderedYaw) return; // nichts geaendert
    state.avatarRenderedYaw = state.avatarYaw;
    const int kW = 300, kH = 460;
    const auto pixels = core::RenderAvatarModel(*state.avatarModel, state.avatarYaw, kW, kH);
    if (state.avatarTex == 0) glGenTextures(1, &state.avatarTex);
    glBindTexture(GL_TEXTURE_2D, state.avatarTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kW, kH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
void DrawCreatureWizardPreview(EditorState& state) {
    auto& w = state.wiz;
    ImGui::BeginChild("##creatureWizardPreview", ImVec2(0,0), true);
    DrawPanelHeader("creatureWizardPreviewHeader", L("VORSCHAU","PREVIEW"),
                    DrawIconCube, w.isNpc ? "module.custom_npc" : "module.custom_mob",
                    w.isNpc ? "NPC" : L("Monster","Monster"));

    if (w.templateId < 0) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = std::max(100.0f, ImGui::GetContentRegionAvail().x);
        DrawIconCube(ImGui::GetWindowDrawList(),
                     ImVec2(p.x + width * 0.5f, p.y + 70.0f),
                     34.0f, IM_COL32(90,120,145,210));
        ImGui::Dummy(ImVec2(0,140.0f));
        ImGui::TextDisabled("%s", L("Zuerst links eine Vorlage wählen.", "Choose a template on the left first."));
    } else if (w.isNpc && w.lookMode == 2) {
        UpdateAvatarPreview(state);
        const float width = std::min(300.0f, std::max(160.0f, ImGui::GetContentRegionAvail().x));
        const float height = width * (460.0f / 300.0f);
        if (state.avatarModel && state.avatarTex != 0) {
            ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(state.avatarTex)),
                         ImVec2(width,height));
            ImGui::SetNextItemWidth(width);
            UI::SliderFloat("##wizardPreviewYaw",&state.avatarYaw,-3.1416f,3.1416f,L("Drehung %.2f", "Rotation %.2f"));
            for (const auto& note : state.avatarModel->notes)
                ImGui::TextColored(ImVec4(1.0f,0.75f,0.35f,1.0f),"%s",note.c_str());
        } else if (!state.avatarError.empty()) {
            ImGui::TextColored(ImVec4(1.0f,0.5f,0.4f,1.0f),"%s",state.avatarError.c_str());
        } else {
            ImGui::TextDisabled("%s", L("Avatar wird vorbereitet...", "Preparing avatar..."));
        }
    } else {
        EnsureNpcDialogRoot(state);
        EnsureMobViewInfoLoaded(state);
        EnsureRescharRoot(state);

        std::string fileName;
        if (w.lookMode == 1 && w.modelFile[0] != '\0')
            fileName = w.modelFile;
        else
            fileName = ResolveMobViewFileName(state,w.templateInx);

        bool renderedThumbnail = false;
        if (!fileName.empty() && state.project.clientFolder[0] != '\0') {
            const std::string rel = FindNpcModelPath(state,fileName);
            if (!rel.empty()) {
                const auto resolved = std::filesystem::path(state.project.clientFolder) / rel;
                std::filesystem::path resmapRoot;
                if (const auto root = FindResmapRootForAssets(state.project.clientFolder))
                    resmapRoot = *root;
                const auto thumb = GetOrLoadAssetThumbnail(state,resolved,true,resmapRoot);
                if (thumb.tex) {
                    const float width = std::min(280.0f,std::max(140.0f,ImGui::GetContentRegionAvail().x));
                    const float height = width / std::max(0.25f,thumb.aspect);
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(thumb.tex)),
                                 ImVec2(width,std::min(height,320.0f)));
                    renderedThumbnail = true;
                }
            }
        }
        if (!renderedThumbnail) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float width = std::max(100.0f, ImGui::GetContentRegionAvail().x);
            DrawIconCube(ImGui::GetWindowDrawList(),
                         ImVec2(p.x + width * 0.5f, p.y + 65.0f),
                         32.0f, IM_COL32(100,185,235,230));
            ImGui::Dummy(ImVec2(0,130.0f));
        }
        ImGui::TextDisabled("%s", L("Modell", "Model"));
        ImGui::TextWrapped("%s",fileName.empty() ? L("(Vorlagenmodell nicht aufgelöst)", "(template model unresolved)") : fileName.c_str());
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s", L("Vorlage", "Template"));
    ImGui::TextWrapped("%s%s%lld",
                       w.templateInx.empty() ? L("(keine)", "(none)") : w.templateInx.c_str(),
                       w.templateId >= 0 ? "  ·  #" : "",
                       w.templateId >= 0 ? w.templateId : 0);
    ImGui::TextDisabled("%s", L("Neue Identität", "New identity"));
    ImGui::TextWrapped("%s",w.newInx[0] ? w.newInx : L("(noch offen)", "(not set yet)"));
    if (w.displayName[0]) ImGui::TextWrapped("%s",w.displayName);
    if (!w.isNpc) {
        ImGui::SeparatorText(L("Werte", "Values"));
        ImGui::Text("Level %d",w.level);
        ImGui::Text("HP %d",w.maxHp);
        ImGui::Text(L("Größe %d", "Size %d"),w.size);
    } else if (w.placeOnMap) {
        ImGui::SeparatorText(L("Platzierung", "Placement"));
        ImGui::Text("%s",state.legacySaveStem[0] ? state.legacySaveStem : L("(keine Karte)", "(no map)"));
        ImGui::Text("X %d · Y %d",w.placeX,w.placeY);
        ImGui::TextDisabled("%s",kNpcRoles[std::clamp(w.roleIdx,0,5)]);
    }
    ImGui::EndChild();
}
void DrawCustomCreatureEditor(EditorState& state) {
    auto& w = state.wiz;
    EnsureItemLookup(state);
    DrawPanelHeader("customCreatureHeader", "CUSTOM NPC / MOB",
                    w.isNpc ? DrawIconPerson : DrawIconSpawn,
                    w.isNpc ? "module.custom_npc" : "module.custom_mob",
                    L("Vorlage klonen · Werte anpassen · Aussehen wählen · optional platzieren",
                      "Clone template · adjust values · choose appearance · optionally place"));
    if (DrawIconButton("customNpc", "NPC", DrawIconPerson, w.isNpc, ImVec2(94,58), true, "module.custom_npc"))
        w.isNpc = true;
    ImGui::SameLine();
    if (DrawIconButton("customMob", L("Monster", "Monster"), DrawIconSpawn, !w.isNpc, ImVec2(94,58), true, "module.custom_mob")) {
        w.isNpc = false;
        if (w.lookMode == 2) w.lookMode = 0;
    }
    ImGui::SameLine();
    ImGui::TextWrapped("%s", L("Klont die Vorlage konsistent in die zugehörigen Client-/Server-Tabellen. Die neue ID wird auf Wunsch automatisch über alle beteiligten Tabellen hinweg gewählt.",
                                  "Clones the template consistently into the related client/server tables. The new ID can be chosen automatically so it is free across all involved tables."));

    w.step = std::clamp(w.step, 0, 4);
    ImGui::Separator();
    struct WizardStep { const char* label; IconDrawFn icon; };
    const WizardStep wizardSteps[] = {
        {L("1 Vorlage", "1 Template"), DrawIconLayers},
        {L("2 Werte", "2 Values"), DrawIconTable},
        {L("3 Aussehen", "3 Appearance"), DrawIconCube},
        {L("4 Rolle / Ort", "4 Role / Place"), DrawIconPortal},
        {L("5 Anlegen", "5 Create"), DrawIconSave},
    };
    for (int i = 0; i < 5; ++i) {
        if (DrawIconButton((std::string("wizStep") + std::to_string(i)).c_str(),
                           wizardSteps[i].label, wizardSteps[i].icon, w.step == i, ImVec2(112,58)))
            w.step = i;
        if (i < 4) ImGui::SameLine();
    }
    ImGui::TextDisabled(L("Schritt %d / 5", "Step %d / 5"), w.step + 1);

    const float previewWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.28f, 280.0f, 360.0f);
    if (!ImGui::BeginTable("##creatureWizardLayout",2,ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn("##wizardMain",ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##wizardPreview",ImGuiTableColumnFlags_WidthFixed,previewWidth);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    // --- 1. Vorlage
    if (w.step == 0) {
    ImGui::SeparatorText(L("Vorlage", "Template"));
    const int mi = FindOrLoadShnDoc(state, "MobInfo.shn", EditorState::ShnSource::Client);
    if (mi < 0) {
        ImGui::TextWrapped("%s", L("MobInfo.shn (Client) nicht geladen - Client-Ordner im Projekt prüfen.", "MobInfo.shn (client) is not loaded - check the client folder in the project."));
    } else {
    auto& mobInfo = state.shnFiles[static_cast<std::size_t>(mi)].file;
    DrawSearchInput("creatureTemplateFilter",
                    L("Vorlage suchen (Name/InxName)", "Search template (name/InxName)"),
                    w.templateFilter, sizeof(w.templateFilter), 235.0f);
    ImGui::SameLine();
    ImGui::TextDisabled(w.templateId >= 0 ? L("gewählt: %s (#%lld)", "selected: %s (#%lld)") : L("noch keine Vorlage", "no template selected"), w.templateInx.c_str(), w.templateId);
    ImGui::BeginChild("##tpllist", ImVec2(0.0f, 130.0f), true);
    {
        const std::string needle = LowerAscii(w.templateFilter);
        const int cId = ShnColumnIndexByName(mobInfo, "ID");
        int shown = 0;
        for (std::size_t r = 0; r < mobInfo.rows.size() && shown < 150; ++r) {
            const bool npc = ShnCellText(mobInfo, r, "IsNPC") == "1";
            if (npc != w.isNpc) continue;
            const std::string inx = ShnCellText(mobInfo, r, "InxName");
            const std::string name = ShnCellText(mobInfo, r, "Name");
            if (!needle.empty() && LowerAscii(inx).find(needle) == std::string::npos && LowerAscii(name).find(needle) == std::string::npos) continue;
            ++shown;
            long long id = 0;
            if (cId >= 0) ShnValueAsInt(mobInfo.rows[r].values[static_cast<std::size_t>(cId)], id);
            const bool sel = w.templateId == id && w.templateInx == inx;
            if (UI::Selectable((inx + "   " + name + "   (#" + std::to_string(id) + ")##t" + std::to_string(r)).c_str(), sel)) {
                w.templateId = id;
                w.templateInx = inx;
                std::snprintf(w.displayName, sizeof(w.displayName), "%s", (name + " 2").c_str());
                std::snprintf(w.newInx, sizeof(w.newInx), "Custom%s", inx.c_str());
                w.level = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "Level").c_str()));
                w.maxHp = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "MaxHP").c_str()));
                w.walkSpeed = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "WalkSpeed").c_str()));
                w.runSpeed = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "RunSpeed").c_str()));
                w.size = static_cast<int>(std::atoi(ShnCellText(mobInfo, r, "Size").c_str()));
            }
        }
    }
    ImGui::EndChild();
    }
    }

    // --- 2. Werte
    if (w.step == 1) {
    ImGui::SeparatorText(L("Identität & Werte", "Identity & values"));
    ImGui::SetNextItemWidth(220.0f); UI::InputText(L("InxName (eindeutig)", "InxName (unique)"), w.newInx, sizeof(w.newInx));
    ImGui::SetNextItemWidth(220.0f); UI::InputText(L("Anzeigename", "Display name"), w.displayName, sizeof(w.displayName));
    UI::Checkbox(L("ID automatisch (erste freie im größten freien Block, in allen Tabellen frei)", "Automatic ID (first free in the largest free block, available in all tables)"), &w.autoId);
    if (!w.autoId) { ImGui::SetNextItemWidth(160.0f); UI::InputInt("ID", &w.manualId); }
    if (!w.isNpc) {
        ImGui::SetNextItemWidth(160.0f); UI::InputInt("Level", &w.level);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt(L("Max. HP", "Max HP"), &w.maxHp);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt(L("Gehtempo", "Walk speed"), &w.walkSpeed);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt(L("Lauftempo", "Run speed"), &w.runSpeed);
        ImGui::SetNextItemWidth(160.0f); UI::InputInt(L("Größe", "Size"), &w.size);
        ImGui::TextDisabled("%s", L("Alle weiteren Werte (EXP, Widerstände, Waffen ...) stammen von der Vorlage und lassen sich danach im Single SHN Editor ändern.", "All other values (EXP, resistances, weapons ...) are inherited from the template and can be changed later in the Single SHN Editor."));
    }
    }

    // --- 3. Aussehen
    if (w.step == 2) {
    ImGui::SeparatorText(L("Aussehen", "Appearance"));
    UI::RadioButton(L("Wie Vorlage", "Like template"), &w.lookMode, 0);
    ImGui::SameLine(); UI::RadioButton(L("Anderes Modell", "Different model"), &w.lookMode, 1);
    if (w.isNpc) { ImGui::SameLine(); UI::RadioButton(L("Spieler-Avatar mit Rüstung", "Player avatar with equipment"), &w.lookMode, 2); }
    if (w.lookMode == 1) {
        static std::vector<std::string> models;
        static bool modelsBuilt = false;
        if (!modelsBuilt) {
            const int vi = FindOrLoadShnDoc(state, "MobViewInfo.shn", EditorState::ShnSource::Client);
            if (vi >= 0) {
                std::set<std::string> names;
                const auto& vf = state.shnFiles[static_cast<std::size_t>(vi)].file;
                for (std::size_t r = 0; r < vf.rows.size(); ++r) { const std::string n = ShnCellText(vf, r, "FileName"); if (!n.empty() && n != "-") names.insert(n); }
                models.assign(names.begin(), names.end());
                modelsBuilt = true;
            }
        }
        ImGui::Text(L("Modell: %s", "Model: %s"), w.modelFile[0] ? w.modelFile : L("(wie Vorlage)", "(like template)"));
        ImGui::SameLine();
        if (UI::Button(L("Modell wählen...", "Choose model..."))) { w.modelFilter[0] = '\0'; ImGui::OpenPopup("Modell wählen##wiz"); }
        std::string chosen;
        if (StringPickerPopup("Modell wählen##wiz", models, w.modelFilter, sizeof(w.modelFilter), chosen)) std::snprintf(w.modelFile, sizeof(w.modelFile), "%s", chosen == "-" ? "" : chosen.c_str());
    }
    if (w.isNpc && w.lookMode == 2) {
        ImGui::BeginGroup();
        ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Klasse (0-5)", "Class (0-5)"), &w.avClass);
        w.avClass = std::clamp(w.avClass, 0, 5);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Geschlecht (1=männl., 0=weibl.)", "Gender (1=male, 0=female)"), &w.avGender);
        w.avGender = std::clamp(w.avGender, 0, 1);
        ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Gesicht", "Face"), &w.avFace);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Frisur", "Hair style"), &w.avHairType);
        ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Haarfarbe", "Hair color"), &w.avHairColor);
        w.avFace = std::max(0, w.avFace); w.avHairType = std::max(0, w.avHairType); w.avHairColor = std::max(0, w.avHairColor);
        ImGui::TextDisabled("%s", L("Ausrüstung (Slot klicken = Item wählen aus ItemInfo, Rechtsklick = leeren):", "Equipment (click slot = choose item from ItemInfo, right-click = clear):"));
        if (ImGui::BeginTable("##equ", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(620.0f, 0.0f))) { // feste Breite: rechts daneben die Vorschau
            for (int i = 0; i < 19; ++i) {
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImGui::Text("%s", app::CurrentLanguage() == app::Language::German ? kAvatarSlotNames[i] : kAvatarSlotNamesEn[i]);
                ImGui::SameLine(140.0f);
                std::string& v = w.equ[static_cast<std::size_t>(i)];
                const bool empty = v.empty() || v == "-";
                const bool known = empty || state.itemByInx.count(v) || state.itemEntries.empty();
                if (!known) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 40, 40, 255));
                if (UI::Button((empty ? std::string(L("(leer)", "(empty)")) : v).c_str(), ImVec2(150.0f, 0.0f))) { w.pickSlot = i; w.itemFilter[0] = '\0'; w.pickRequested = true; }
                if (!known) ImGui::PopStyleColor();
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) v = "-";
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        // OpenPopup MUSS auf derselben ID-Ebene wie BeginPopup passieren - der Button steckt in
        // PushID(i)/Tabelle, daher nur ein Merker dort und das Oeffnen hier (sonst oeffnete sich
        // das Popup nie: "Custom NPC laesst sich nicht ausruesten", CHANGELOG [0.44.29]).
        if (w.pickRequested) { ImGui::OpenPopup("Item wählen##wiz"); w.pickRequested = false; }
        static std::vector<std::string> itemNames, itemLabels;
        if (itemNames.size() != state.itemEntries.size()) {
            itemNames.clear(); itemLabels.clear();
            for (const auto& e : state.itemEntries) { itemNames.push_back(e.inx); itemLabels.push_back(e.inx + "   " + e.name + "   (#" + std::to_string(e.id) + ")"); }
        }
        std::string chosen;
        if (StringPickerPopup("Item wählen##wiz", itemNames, w.itemFilter, sizeof(w.itemFilter), chosen, &itemLabels) && w.pickSlot >= 0 && w.pickSlot < 19) {
            w.equ[static_cast<std::size_t>(w.pickSlot)] = chosen;
        }
        ImGui::EndGroup();
    }
    }

    // --- 4. NPC-Extras
    if (w.step == 3) {
        ImGui::SeparatorText(L("Rolle & Platzierung", "Role & placement"));
        if (w.isNpc) {
        UI::Checkbox(L("Dialog der Vorlage kopieren (NpcDialogData)", "Copy dialog from template (NpcDialogData)"), &w.copyDialog);
        UI::Checkbox(L("Auf der offenen Karte platzieren (World/NPC.txt)", "Place on the open map (World/NPC.txt)"), &w.placeOnMap);
        if (w.placeOnMap) {
            if (state.legacySaveStem[0] == '\0') {
                ImGui::TextDisabled("%s", L("Keine Karte offen - im Map-Editor eine Karte öffnen.", "No map is open - open a map in the map editor."));
            } else {
                ImGui::Text(L("Karte: %s", "Map: %s"), state.legacySaveStem);
                ImGui::SetNextItemWidth(120.0f); UI::InputInt("X", &w.placeX);
                ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt("Y", &w.placeY);
                ImGui::SameLine(); ImGui::SetNextItemWidth(120.0f); UI::InputInt(L("Richtung", "Direction"), &w.placeDir);
                if (UI::SmallButton(L("Kartenmitte", "Map center"))) {
                    w.placeX = static_cast<int>((state.heightmap.Width() - 1) * state.heightmap.BlockWidth() * 0.5f);
                    w.placeY = static_cast<int>((state.heightmap.Height() - 1) * state.heightmap.BlockHeight() * 0.5f);
                }
                ImGui::SetNextItemWidth(180.0f);
                UI::Combo(L("Rolle", "Role"), &w.roleIdx, kNpcRoles, 6);
                ImGui::SetNextItemWidth(180.0f);
                UI::InputText(L("Rollenargument (z.B. Quest, Item, Weapon, Skill)", "Role argument (e.g. Quest, Item, Weapon, Skill)"), w.roleArg, sizeof(w.roleArg));
                if (std::string(kNpcRoles[std::clamp(w.roleIdx, 0, 5)]) == "Merchant")
                    ImGui::TextDisabled("%s", L("Händler: danach im NPC-Tab 'Händler-Inventar bearbeiten' (legt NPCItemList/<InxName>.txt an).", "Merchant: afterwards use 'Edit merchant inventory' in the NPC tab (creates NPCItemList/<InxName>.txt)."));
            }
        }
        } else {
            ImGui::TextWrapped("%s", L("Monster werden über die SHN-Tabellen angelegt. Die eigentliche Spawn-Zone wird anschließend im Karteneditor unter 'Mobs' erstellt und positioniert.",
                                          "Monsters are created through the SHN tables. The actual spawn zone is then created and positioned in the map editor under 'Mobs'."));
        }
    }

    // --- Anlegen / Speichern
    if (w.step == 4) {
    ImGui::SeparatorText(L("Zusammenfassung & Anlegen", "Summary & create"));
    ImGui::TextDisabled("%s", L("Typ", "Type"));
    ImGui::Text("%s", w.isNpc ? "NPC" : L("Monster", "Monster"));
    ImGui::TextDisabled("%s", L("Vorlage", "Template"));
    ImGui::Text("%s%s%lld", w.templateInx.empty() ? L("(keine)", "(none)") : w.templateInx.c_str(),
                w.templateId >= 0 ? "  ·  #" : "", w.templateId >= 0 ? w.templateId : 0);
    ImGui::TextDisabled("%s", L("Neue Identität", "New identity"));
    ImGui::Text("%s  ·  %s", w.newInx[0] ? w.newInx : L("(InxName fehlt)", "(InxName missing)"),
                w.displayName[0] ? w.displayName : L("(Name fehlt)", "(name missing)"));
    ImGui::TextDisabled("ID");
    ImGui::Text("%s", w.autoId ? L("automatisch - erste konsistent freie ID", "automatic - first consistently free ID") : std::to_string(w.manualId).c_str());
    ImGui::TextDisabled("%s", L("Aussehen", "Appearance"));
    ImGui::Text("%s", w.lookMode == 0 ? L("wie Vorlage", "like template") : w.lookMode == 1 ? L("anderes Modell", "different model") : L("Spieler-Avatar", "player avatar"));
    if (w.isNpc) {
        ImGui::TextDisabled("%s", L("Kartenplatzierung", "Map placement"));
        ImGui::Text("%s", w.placeOnMap ? L("wird angelegt", "will be created") : L("keine", "none"));
        if (w.placeOnMap) ImGui::Text(L("X %d · Y %d · Richtung %d", "X %d · Y %d · direction %d"), w.placeX, w.placeY, w.placeDir);
    }
    ImGui::Separator();
    if (UI::Button(L("Anlegen", "Create"), ImVec2(160.0f, 0.0f))) RunCreateCreature(state);
    ImGui::SameLine();
    if (UI::Button(L("Alle geänderten SHN speichern", "Save all changed SHN"))) {
        const auto [shnSaved, shnFailed] = SaveAllDirtyShnDocuments(state);
        int saved = static_cast<int>(shnSaved), failed = static_cast<int>(shnFailed);
        // NPC.txt (falls ein NPC platziert wurde)
        if (state.npcTextLoaded && !state.shineTextRoot.empty()) {
            auto r = SaveProjectShineOverride(
                state.npcTextFile,
                ProjectServerShinePath(state.project, std::filesystem::path("World") / "NPC.txt"));
            if (r) ++saved; else { ++failed; w.report.push_back("Fehler beim Projekt-Speichern von NPC.txt: " + r.error()); }
        }
        w.report.push_back("Gespeichert: " + std::to_string(saved) + " Datei(en)" + (failed ? ", Fehler: " + std::to_string(failed) : std::string()));
    }
    for (const auto& line : w.report) ImGui::TextWrapped("%s", line.c_str());
    }

    ImGui::Separator();
    const bool canAdvance =
        !(w.step == 0 && w.templateId < 0) &&
        !(w.step == 1 && (w.newInx[0] == '\0' || w.displayName[0] == '\0'));
    if (w.step > 0 && UI::Button(L("< Zurück", "< Back"), ImVec2(120,0))) --w.step;
    if (w.step > 0 && w.step < 4) ImGui::SameLine();
    if (w.step < 4) {
        ImGui::BeginDisabled(!canAdvance);
        if (UI::Button(L("Weiter >", "Next >"), ImVec2(120,0))) ++w.step;
        ImGui::EndDisabled();
        if (!canAdvance) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", w.step == 0 ? L("Zuerst eine Vorlage wählen.", "Choose a template first.") : L("InxName und Anzeigename angeben.", "Enter InxName and display name."));
        }
    }

    ImGui::TableSetColumnIndex(1);
    DrawCreatureWizardPreview(state);
    ImGui::EndTable();
}

} // namespace nge
