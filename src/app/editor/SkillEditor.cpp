// SkillEditor.cpp
// Skill-Editor.

#include "EditorShared.hpp"

namespace nge {

// Sprachumschaltung fuer Texte ohne T()-Schluessel: liefert je nach eingestellter Sprache Deutsch oder Englisch.
const char* L(const char* de, const char* en) { return app::CurrentLanguage() == app::Language::German ? de : en; }
SkillDocs ResolveSkillDocs(EditorState& state) {
    SkillDocs d;
    using S = EditorState::ShnSource;
    d.skillC = FindOrLoadShnDoc(state, "ActiveSkill.shn", S::Client);
    d.skillS = FindOrLoadShnDoc(state, "ActiveSkill.shn", S::Server);
    d.server = FindOrLoadShnDoc(state, "ActiveSkillInfoServer.shn", S::Server);
    d.viewC = FindOrLoadShnDoc(state, "ActiveSkillView.shn", S::Client);
    d.viewS = FindOrLoadShnDoc(state, "ActiveSkillView.shn", S::Server);
    return d;
}
std::vector<int> SkillDocsOf(const SkillDocs& d, skilled::Doc kind) {
    std::vector<int> out;
    auto add = [&](int i) { if (i >= 0) out.push_back(i); };
    if (kind == skilled::Doc::Skill) { add(d.skillC); add(d.skillS); }
    else if (kind == skilled::Doc::Server) add(d.server);
    else { add(d.viewC); add(d.viewS); }
    return out;
}
// Zeile eines Skills (per ID) in einer Tabelle - Suche mit Cache je Dokument.
// Neben der Zeilenanzahl muessen auch Datei und Edit-Revision Teil des Cache-Schluessels sein:
// eine im Rohgrid geaenderte ID darf nicht bis zum naechsten Zeilen-Insert auf eine alte Zeile zeigen.
long long SkillRowIn(EditorState& state, int docIdx, long long skillId) {
    struct SkillRowCache {
        std::size_t rowCount = 0;
        std::uint64_t revision = std::numeric_limits<std::uint64_t>::max();
        std::string path;
        std::unordered_map<long long,std::size_t> rows;
    };
    static std::unordered_map<int,SkillRowCache> cache;
    auto& f = state.shnFiles[static_cast<std::size_t>(docIdx)].file;
    auto& entry = cache[docIdx];
    const std::string path = f.path.string();
    if (entry.rowCount != f.rows.size() || entry.revision != state.shnEditCounter ||
        entry.path != path || entry.rows.empty()) {
        entry.rows.clear();
        const int cId = ShnColumnIndexByName(f, "ID");
        for (std::size_t r = 0; r < f.rows.size() && cId >= 0; ++r) {
            long long id = 0;
            if (ShnValueAsInt(f.rows[r].values[static_cast<std::size_t>(cId)], id)) entry.rows.emplace(id,r);
        }
        entry.rowCount = f.rows.size();
        entry.revision = state.shnEditCounter;
        entry.path = path;
    }
    const auto it = entry.rows.find(skillId);
    return it == entry.rows.end() ? -1 : static_cast<long long>(it->second);
}
std::string SkillCellRead(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId) {
    for (const int di : SkillDocsOf(d, kind)) {
        const long long row = SkillRowIn(state, di, skillId);
        if (row < 0) continue;
        return ShnCellText(state.shnFiles[static_cast<std::size_t>(di)].file, static_cast<std::size_t>(row), col);
    }
    return {};
}
bool SkillCellCopiesDiffer(EditorState& state,const SkillDocs& d,skilled::Doc kind,
                                  const char* col,long long skillId,std::string* details) {
    bool haveValue=false, differ=false;
    std::string first;
    if (details) details->clear();
    for (const int di:SkillDocsOf(d,kind)) {
        if (di < 0 || di >= static_cast<int>(state.shnFiles.size())) continue;
        const auto& doc=state.shnFiles[static_cast<std::size_t>(di)];
        const long long row=SkillRowIn(state,di,skillId);
        const int ci=ShnColumnIndexByName(doc.file,col);
        std::string value;
        if (row < 0 || ci < 0) {
            value=L("(Zeile/Feld fehlt)","(row/field missing)");
            differ=true;
        } else {
            value=ShnCellText(doc.file,static_cast<std::size_t>(row),col);
            if (!haveValue) { first=value; haveValue=true; }
            else if (value != first) differ=true;
        }
        if (details) {
            if (!details->empty()) *details += "\n";
            *details += std::string(ShnSourceName(doc.source))+" "+doc.file.FileName()+": "+value;
        }
    }
    return differ;
}
// Schreibt in ALLE Kopien (Client und Server) der Zeile.
bool SkillCellWrite(EditorState& state, const SkillDocs& d, skilled::Doc kind, const char* col, long long skillId, const std::string& text) {
    bool any = false;
    for (const int di : SkillDocsOf(d, kind)) {
        const long long row = SkillRowIn(state, di, skillId);
        if (row < 0) continue;
        auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
        const int column = ShnColumnIndexByName(doc.file,col);
        if (column < 0) continue;
        const std::size_t r = static_cast<std::size_t>(row);
        const std::size_t ci = static_cast<std::size_t>(column);
        if (r >= doc.file.rows.size() || ci >= doc.file.rows[r].values.size()) continue;
        if (core::legacy::ShnValueToString(doc.file.rows[r].values[ci]) == text) continue;
        if (SetShnCellText(doc.file,r,col,text)) {
            doc.dirty = true;
            EnsureCellStatusSize(doc);
            doc.cellDirty[r][ci] = 1;
            any = true;
        }
    }
    if (any) ++state.shnEditCounter;
    return any;
}
// Datenbelegte Auswahllisten: Animation/VFX/Sound/Icon werden über ALLE Felder derselben
// Art aggregiert. Dadurch kann z.B. eine SwingAction auch als Referenz für ein anderes
// Animationsfeld gefunden werden, ohne Asset-Namen zu erfinden. Skill/State-Referenzen bleiben
// an ihre verifizierten InxName-Tabellen gebunden.
void BuildSkillPickerOptions(EditorState& state, const SkillDocs& d) {
    auto& ed = state.skill;
    const std::string key = std::to_string(state.shnEditCounter) + "|" + std::to_string(d.viewC) + "|" +
                            std::to_string(d.skillC) + "|" + std::to_string(d.server);
    if (key == ed.pickerBuiltKey) return;
    ed.pickerBuiltKey = key;
    ed.pickerOptions.clear();
    ed.pickerLabels.clear();

    auto storeCounts = [&](const std::string& mapKey, const std::unordered_map<std::string,int>& counts) {
        std::vector<std::pair<int,std::string>> sorted;
        sorted.reserve(counts.size());
        for (const auto& [value,count] : counts) sorted.emplace_back(count,value);
        std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        auto& opts=ed.pickerOptions[mapKey];
        auto& labels=ed.pickerLabels[mapKey];
        for (const auto& [count,value] : sorted) {
            opts.push_back(value);
            labels.push_back(value+"   ("+std::to_string(count)+"x)");
        }
    };

    auto addColumn = [&](int docIdx, const char* col, const std::string& mapKey) {
        if (docIdx < 0) return;
        const auto& file=state.shnFiles[static_cast<std::size_t>(docIdx)].file;
        const int ci=ShnColumnIndexByName(file,col);
        if (ci < 0) return;
        std::unordered_map<std::string,int> counts;
        for (const auto& row:file.rows) {
            if (static_cast<std::size_t>(ci) >= row.values.size()) continue;
            counts[core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(ci)])]++;
        }
        storeCounts(mapKey,counts);
    };

    auto addKind = [&](skilled::Kind kind,const std::string& mapKey) {
        if (d.viewC < 0) return;
        const auto& file=state.shnFiles[static_cast<std::size_t>(d.viewC)].file;
        std::unordered_map<std::string,int> counts;
        for (const auto& sec:skilled::kSections) {
            for (std::size_t i=0;i<sec.count;++i) {
                const auto& fld=sec.fields[i];
                if (fld.kind != kind || fld.doc != skilled::Doc::View) continue;
                const int ci=ShnColumnIndexByName(file,fld.col);
                if (ci < 0) continue;
                for (const auto& row:file.rows) {
                    if (static_cast<std::size_t>(ci) >= row.values.size()) continue;
                    counts[core::legacy::ShnValueToString(row.values[static_cast<std::size_t>(ci)])]++;
                }
            }
        }
        storeCounts(mapKey,counts);
    };

    addKind(skilled::Kind::Anim,"Anim");
    addKind(skilled::Kind::Effect,"Effect");
    addKind(skilled::Kind::Sound,"Sound");
    addKind(skilled::Kind::Icon,"Icon");
    addColumn(d.skillC,"InxName","SkillRef");
    const int abState=FindOrLoadShnDoc(state,"AbState.shn",EditorState::ShnSource::Server);
    addColumn(abState,"InxName","StateRef");
}
const char* SkillPickerKey(skilled::Kind kind,const char* column) {
    using K=skilled::Kind;
    if (kind == K::Anim) return "Anim";
    if (kind == K::Effect) return "Effect";
    if (kind == K::Sound) return "Sound";
    if (kind == K::Icon) return "Icon";
    if (kind == K::SkillRef) return "SkillRef";
    if (kind == K::StateRef) return "StateRef";
    return column;
}
// Rich picker for Animation/VFX fields. "Preview" here is deliberately a DATA REFERENCE preview:
// it shows real existing skills/columns that use the value. Actual KF/NIF playback belongs to the
// separately verified KFM playback roadmap and is not faked here.
bool SkillPresentationPickerPopup(EditorState& state,const SkillDocs& d,
                                         const skilled::Field& field,const std::string& mapKey,
                                         const std::string& current,std::string& chosen) {
    auto& ed=state.skill;
    bool picked=false;
    if (!ImGui::BeginPopup("##pick")) return false;

    const std::string popupKey=mapKey+"|"+field.col;
    if (ImGui::IsWindowAppearing() || ed.pickerPreviewKey != popupKey) {
        ed.pickerPreviewKey=popupKey;
        ed.pickerPreviewValue=current;
    }

    ImGui::TextColored(UiTheme::AccentCyan,"%s",
                       field.kind == skilled::Kind::Anim ? L("ANIMATION PICKER","ANIMATION PICKER")
                                                        : L("VFX PICKER","VFX PICKER"));
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]",field.col);
    DrawSearchInput("skillPresentationFilter",
                    L("Wert oder Referenz-Skill suchen","Search value or reference skill"),
                    ed.pickFilter,sizeof(ed.pickFilter),695.0f);
    ImGui::Separator();

    const auto& options=ed.pickerOptions[mapKey];
    const auto& labels=ed.pickerLabels[mapKey];
    const std::string needle=LowerAscii(ed.pickFilter);

    ImGui::BeginChild("##skillPickerValues",ImVec2(390.0f,350.0f),true);
    if (UI::Selectable(L("(leer) -","(empty) -"),ed.pickerPreviewValue=="-"))
        ed.pickerPreviewValue="-";
    int shown=0;
    for (std::size_t i=0;i<options.size();++i) {
        const std::string& value=options[i];
        const std::string& label=i<labels.size()?labels[i]:value;
        if (!needle.empty() && LowerAscii(label).find(needle)==std::string::npos) continue;
        if (++shown > 400) {
            ImGui::TextDisabled("%s",L("... weitere Treffer - Suche eingrenzen","... more matches - narrow the search"));
            break;
        }
        if (UI::Selectable((label+"##presentation"+std::to_string(i)).c_str(),
                           ed.pickerPreviewValue==value,ImGuiSelectableFlags_AllowDoubleClick)) {
            ed.pickerPreviewValue=value;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                chosen=value; picked=true; ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##skillPickerReference",ImVec2(330.0f,350.0f),true);
    ImGui::TextColored(UiTheme::TextPrimary,"%s",L("Referenz-Vorschau","Reference preview"));
    ImGui::Separator();

    const std::string preview=ed.pickerPreviewValue;
    if (preview.empty()) {
        ImGui::TextDisabled("%s",L("Links einen Wert wählen.","Choose a value on the left."));
    } else {
        ImGui::TextWrapped("%s",preview.c_str());
        ImGui::TextDisabled("%s",field.kind == skilled::Kind::Anim
            ? L("Animation - aus vorhandenen ActiveSkillView-Werten","Animation - from existing ActiveSkillView values")
            : L("Effekt/VFX - aus vorhandenen ActiveSkillView-Werten","Effect/VFX - from existing ActiveSkillView values"));
        ImGui::SeparatorText(L("Verwendet von","Used by"));

        int totalUses=0,shownRefs=0;
        if (d.viewC >= 0) {
            const auto& vf=state.shnFiles[static_cast<std::size_t>(d.viewC)].file;
            for (std::size_t row=0;row<vf.rows.size();++row) {
                std::vector<std::string> matchedColumns;
                for (const auto& sec:skilled::kSections) {
                    for (std::size_t fi=0;fi<sec.count;++fi) {
                        const auto& candidate=sec.fields[fi];
                        if (candidate.kind != field.kind || candidate.doc != skilled::Doc::View) continue;
                        const int ci=ShnColumnIndexByName(vf,candidate.col);
                        if (ci < 0 || static_cast<std::size_t>(ci)>=vf.rows[row].values.size()) continue;
                        if (core::legacy::ShnValueToString(vf.rows[row].values[static_cast<std::size_t>(ci)])==preview)
                            matchedColumns.push_back(candidate.col);
                    }
                }
                if (matchedColumns.empty()) continue;
                totalUses+=static_cast<int>(matchedColumns.size());
                if (shownRefs >= 8) continue;
                const long long id=std::atoll(ShnCellText(vf,row,"ID").c_str());
                const long long skillRow=SkillRowIn(state,d.skillC,id);
                const std::string inx=skillRow>=0 ? ShnCellText(state.shnFiles[static_cast<std::size_t>(d.skillC)].file,
                                                               static_cast<std::size_t>(skillRow),"InxName")
                                                   : std::string("#")+std::to_string(id);
                const std::string name=skillRow>=0 ? ShnCellText(state.shnFiles[static_cast<std::size_t>(d.skillC)].file,
                                                                static_cast<std::size_t>(skillRow),"Name") : std::string();
                ImGui::PushID(static_cast<int>(row));
                if (UI::SmallButton((std::string("» ")+inx).c_str())) {
                    state.skill.selectedId=id;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s",name.c_str());
                ImGui::TextDisabled("   %s", [&] {
                    static std::string cols;
                    cols.clear();
                    for (std::size_t k=0;k<matchedColumns.size();++k) {
                        if (k) cols += ", ";
                        cols += matchedColumns[k];
                    }
                    return cols.c_str();
                }());
                ImGui::PopID();
                ++shownRefs;
            }
        }
        ImGui::TextDisabled(L("%d Verwendung(en) in geladenem ActiveSkillView",
                              "%d use(s) in loaded ActiveSkillView"),totalUses);
        if (totalUses==0 && preview!="-")
            ImGui::TextColored(UiTheme::Warning,"%s",L("Nicht in den geladenen Skill-Darstellungen referenziert.",
                                                       "Not referenced by the loaded skill presentation data."));
        ImGui::Separator();
        ImGui::TextWrapped("%s",L("Dies prüft Referenzen in echten Skilldaten; es behauptet noch nicht, dass eine KF-/NIF-/Effektdatei physisch existiert.",
                                   "This checks references in real skill data; it does not yet claim that a physical KF/NIF/effect asset exists."));
    }

    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),300.0f));
    ImGui::BeginDisabled(preview.empty());
    if (UI::Button(L("Wert übernehmen","Use value"),ImVec2(-1.0f,0.0f))) {
        chosen=preview; picked=true; ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::EndPopup();
    return picked;
}
// Eine Feldzeile im Formular. Aenderungen werden SOFORT in alle Kopien geschrieben.
void DrawSkillField(EditorState& state, const SkillDocs& d, const skilled::Field& fld, long long skillId, ImVec4 dim) {
    auto& ed = state.skill;
    const bool de = app::CurrentLanguage() == app::Language::German;
    std::string current = SkillCellRead(state, d, fld.doc, fld.col, skillId);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    std::string syncDetails;
    const bool copiesDiffer=SkillCellCopiesDiffer(state,d,fld.doc,fld.col,skillId,&syncDetails);
    if (copiesDiffer) ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0f,0.68f,0.25f,1.0f));
    ImGui::TextUnformatted(de ? fld.labelDe : fld.labelEn);
    if (copiesDiffer) ImGui::PopStyleColor();
    if (copiesDiffer) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f),"!");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        const std::string tip=std::string(de ? fld.tipDe : fld.tipEn)+"\n["+fld.col+"]"+
                              (copiesDiffer ? "\n\n"+std::string(L("Client/Server unterschiedlich:","Client/server differ:"))+"\n"+syncDetails : "");
        ImGui::SetTooltip("%s",tip.c_str());
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(fld.col);
    using K = skilled::Kind;
    if (fld.kind == K::Int || fld.kind == K::Bool) {
        long long v = std::atoll(current.c_str());
        if (fld.kind == K::Bool) {
            bool b = v != 0;
            if (UI::Checkbox("##v", &b)) SkillCellWrite(state, d, fld.doc, fld.col, skillId, b ? "1" : "0");
        } else {
            int iv = static_cast<int>(std::clamp<long long>(v, -2147483647LL, 2147483647LL));
            ImGui::SetNextItemWidth(130.0f);
            if (UI::InputInt("##v", &iv, 0, 0)) SkillCellWrite(state, d, fld.doc, fld.col, skillId, std::to_string(iv));
        }
    } else {
        std::vector<char> buf(current.begin(), current.end());
        buf.resize(std::max<std::size_t>(buf.size() + 1, 256), '\0');
        const bool picker = fld.kind != K::Text;
        ImGui::SetNextItemWidth(picker ? 300.0f : 520.0f);
        if (UI::InputText("##v", buf.data(), buf.size()))
            SkillCellWrite(state,d,fld.doc,fld.col,skillId,std::string(buf.data()));
        if (picker) {
            const std::string mapKey=SkillPickerKey(fld.kind,fld.col);
            const auto& opts=ed.pickerOptions[mapKey];
            const bool dataReferenced=current.empty() || current=="-" ||
                std::find(opts.begin(),opts.end(),current)!=opts.end();

            ImGui::SameLine();
            if (UI::Button(de ? "Auswahl..." : "Choose...")) {
                ed.pickFilter[0]='\0';
                ed.pickerPreviewKey.clear();
                ImGui::OpenPopup("##pick");
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s",de
                    ? "Aus real verwendeten Werten wählen. Animation/VFX zeigt zusätzlich Referenz-Skills und die Spalten, in denen der Wert vorkommt."
                    : "Choose from values used in real data. Animation/VFX also shows reference skills and the columns in which the value occurs.");

            if (!dataReferenced && (fld.kind==K::Anim || fld.kind==K::Effect ||
                                    fld.kind==K::Sound || fld.kind==K::Icon)) {
                ImGui::SameLine();
                ImGui::TextColored(UiTheme::Warning,"!");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s",L("Dieser Wert kommt in den geladenen Vergleichsdaten nicht vor. Das ist eine Warnung, kein Beweis für ein fehlendes Asset.",
                                             "This value does not occur in the loaded reference data. This is a warning, not proof of a missing asset."));
            }

            std::string chosen;
            const bool richPicker=fld.kind==K::Anim || fld.kind==K::Effect;
            const bool picked=richPicker
                ? SkillPresentationPickerPopup(state,d,fld,mapKey,current,chosen)
                : StringPickerPopup("##pick",opts,ed.pickFilter,sizeof(ed.pickFilter),chosen,&ed.pickerLabels[mapKey]);
            if (picked) SkillCellWrite(state,d,fld.doc,fld.col,skillId,chosen);
        }
    }
    ImGui::PopID();
    (void)dim;
}
// Klont die Zeile(n) der Vorlage-ID in allen betroffenen Tabellen (Skill + optional Skillbuch).
void CreateSkillFrom(EditorState& state, const SkillDocs& d, long long templateId, const std::string& newInx, const std::string& newName, bool makeBook) {
    auto& ed = state.skill;
    ed.report.clear();
    using S = EditorState::ShnSource;
    if (newInx.empty()) { ed.report.push_back(L("Bitte einen InxName angeben.", "Please enter an InxName.")); return; }
    if (d.skillC < 0 || d.viewC < 0 || d.server < 0) { ed.report.push_back(L("ActiveSkill/ActiveSkillView/ActiveSkillInfoServer nicht geladen (Projekt-Ordner prüfen).", "ActiveSkill/ActiveSkillView/ActiveSkillInfoServer not loaded (check project folders).")); return; }
    const long long tRow = SkillRowIn(state, d.skillC, templateId);
    if (tRow < 0) { ed.report.push_back(L("Vorlage nicht gefunden.", "Template not found.")); return; }
    const std::string templateInx = ShnCellText(state.shnFiles[static_cast<std::size_t>(d.skillC)].file, static_cast<std::size_t>(tRow), "InxName");
    {
        const auto& f = state.shnFiles[static_cast<std::size_t>(d.skillC)].file;
        for (std::size_t r = 0; r < f.rows.size(); ++r)
            if (ShnCellText(f, r, "InxName") == newInx) { ed.report.push_back(L("InxName existiert bereits: ", "InxName already exists: ") + newInx); return; }
    }
    std::set<long long> used;
    const core::legacy::ShnColumn* idCol = nullptr;
    for (const int di : {d.skillC, d.skillS, d.server, d.viewC, d.viewS}) {
        if (di < 0) continue;
        const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
        const int c = ShnColumnIndexByName(f, "ID");
        CollectShnInts(f, c, used);
        if (!idCol && di == d.skillC && c >= 0) idCol = &f.columns[static_cast<std::size_t>(c)];
    }
    if (!idCol) { ed.report.push_back("ID?"); return; }
    const long long newId = SuggestFreeShnId(*idCol, used);
    if (newId < 0) { ed.report.push_back(L("Keine freie ID.", "No free ID.")); return; }
    ed.report.push_back(std::string(L("Neue Skill-ID: ", "New skill ID: ")) + std::to_string(newId) + "  InxName: " + newInx + "  (" + L("Vorlage", "template") + ": " + templateInx + ")");
    struct Spec { const char* file; S src; };
    const Spec specs[] = {{"ActiveSkill.shn", S::Client}, {"ActiveSkill.shn", S::Server}, {"ActiveSkillInfoServer.shn", S::Server}, {"ActiveSkillView.shn", S::Client}, {"ActiveSkillView.shn", S::Server}};
    for (const auto& sp : specs) CloneRowsById(state, sp.file, sp.src, "InxName", templateId, templateInx, newId, newInx, ed.report);
    ed.selectedId = newId;
    // Name setzen (beide ActiveSkill-Kopien)
    if (!newName.empty()) SkillCellWrite(state, d, skilled::Doc::Skill, "Name", newId, newName);

    if (makeBook) {
        // Skillbuch: Item mit GLEICHEM InxName wie der Skill (ItemInfo.MarketIndex=Skill)
        const int itemC = FindOrLoadShnDoc(state, "ItemInfo.shn", S::Client);
        long long templateItemId = -1;
        if (itemC >= 0) {
            const auto& f = state.shnFiles[static_cast<std::size_t>(itemC)].file;
            for (std::size_t r = 0; r < f.rows.size(); ++r)
                if (ShnCellText(f, r, "InxName") == templateInx) { templateItemId = std::atoll(ShnCellText(f, r, "ID").c_str()); break; }
        }
        if (templateItemId < 0) {
            ed.report.push_back(L("- Kein Skillbuch-Item zur Vorlage gefunden (übersprungen).", "- No skill book item found for the template (skipped)."));
        } else {
            std::set<long long> usedItems;
            const core::legacy::ShnColumn* itemIdCol = nullptr;
            const Spec itemSpecs[] = {{"ItemInfo.shn", S::Client}, {"ItemInfo.shn", S::Server}, {"ItemInfoServer.shn", S::Server}, {"ItemViewInfo.shn", S::Client}, {"ItemViewInfo.shn", S::Server}};
            for (const auto& sp : itemSpecs) {
                const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
                if (di < 0) continue;
                const auto& f = state.shnFiles[static_cast<std::size_t>(di)].file;
                const int c = ShnColumnIndexByName(f, "ID");
                CollectShnInts(f, c, usedItems);
                if (!itemIdCol && std::string(sp.file) == "ItemInfo.shn" && c >= 0) itemIdCol = &f.columns[static_cast<std::size_t>(c)];
            }
            const long long newItemId = itemIdCol ? SuggestFreeShnId(*itemIdCol, usedItems) : -1;
            if (newItemId < 0) {
                ed.report.push_back(L("- Keine freie Item-ID (Skillbuch übersprungen).", "- No free item ID (book skipped)."));
            } else {
                ed.report.push_back(std::string(L("Neue Item-ID (Skillbuch): ", "New item ID (skill book): ")) + std::to_string(newItemId));
                for (const auto& sp : itemSpecs) {
                    auto created = CloneRowsById(state, sp.file, sp.src, "InxName", templateItemId, templateInx, newItemId, newInx, ed.report);
                    const int di = FindOrLoadShnDoc(state, sp.file, sp.src);
                    if (di >= 0 && !newName.empty() && std::string(sp.file) != "ItemViewInfo.shn")
                        for (const std::size_t nr : created) SetShnCellText(state.shnFiles[static_cast<std::size_t>(di)].file, nr, "Name", newName);
                }
            }
        }
    }
    ed.report.push_back(L("Fertig - noch NICHT gespeichert (oben 'Alle geänderten SHN speichern').", "Done - NOT saved yet (use 'Save all changed SHN' above)."));
}
// Skalierung einer ganzen Reihe (gleicher InxName ohne Endziffern) in ActiveSkill (Client + Server).
int ScaleSkillSeries(EditorState& state, const SkillDocs& d, const std::string& seriesKey, int percent, const std::vector<const char*>& columns) {
    int changed = 0;
    for (const int di : {d.skillC, d.skillS}) {
        if (di < 0) continue;
        auto& doc = state.shnFiles[static_cast<std::size_t>(di)];
        auto& f = doc.file;
        EnsureCellStatusSize(doc);
        for (std::size_t r = 0; r < f.rows.size(); ++r) {
            std::string inx = ShnCellText(f, r, "InxName");
            while (!inx.empty() && std::isdigit(static_cast<unsigned char>(inx.back()))) inx.pop_back();
            if (inx != seriesKey) continue;
            for (const char* col : columns) {
                const int ci = ShnColumnIndexByName(f, col);
                if (ci < 0 || static_cast<std::size_t>(ci) >= f.rows[r].values.size()) continue;
                const std::string cur = ShnCellText(f, r, col);
                if (cur.empty()) continue;
                const long long v = std::atoll(cur.c_str());
                const long long nv = static_cast<long long>(std::llround(static_cast<double>(v) * percent / 100.0));
                if (nv == v || !SetShnCellText(f, r, col, std::to_string(nv))) continue;
                doc.dirty = true;
                doc.cellDirty[r][static_cast<std::size_t>(ci)] = 1;
                ++changed;
            }
        }
    }
    if (changed) ++state.shnEditCounter;
    return changed;
}
void DrawSkillEditor(EditorState& state) {
    auto& ed = state.skill;
    const bool de = app::CurrentLanguage() == app::Language::German;
    const ImVec4 dim(0.60f, 0.66f, 0.74f, 1.0f);
    const SkillDocs d = ResolveSkillDocs(state);
    DrawPanelHeader("skillEditorHeader", "SKILL EDITOR", DrawIconBolt,
                    "module.skill", L("Client + Server synchron bearbeiten","Edit client + server in sync"));
    const std::size_t dirtySkillDocs = DirtyShnDocumentCount(state);
    if (dirtySkillDocs > 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f), L("* %zu SHN geändert","* %zu SHN modified"), dirtySkillDocs);
    }
    ImGui::TextWrapped("%s", L("Vorhandene Skills bearbeiten, neue Skills aus Vorlagen klonen und vorhandene Animationen/Effekte zuweisen. Eine Zeile entspricht einer Stufe einer Skillreihe.",
                               "Edit existing skills, clone new skills from templates and assign existing animations/effects. One row represents one step of a skill series."));
    ImGui::Separator();
    if (d.skillC < 0 || d.viewC < 0 || d.server < 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", L("ActiveSkill.shn / ActiveSkillView.shn / ActiveSkillInfoServer.shn nicht gefunden - Client-/Server-Ordner im Projekt prüfen.", "ActiveSkill.shn / ActiveSkillView.shn / ActiveSkillInfoServer.shn not found - check client/server folders in the project."));
        return;
    }
    BuildSkillPickerOptions(state, d);
    auto& asf = state.shnFiles[static_cast<std::size_t>(d.skillC)].file;
    if (d.skillS < 0 || d.viewS < 0) {
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f),"%s",L(
            "! Mindestens eine Server-Kopie (ActiveSkill / ActiveSkillView) fehlt. Der Sync-Filter zeigt deshalb alle betroffenen Skills.",
            "! At least one server copy (ActiveSkill / ActiveSkillView) is missing. The sync filter therefore shows all affected skills."));
    }

    // Toolbar: speichern
    if (UI::Button(L("Alle geänderten SHN speichern", "Save all changed SHN")) ||
        (ShortcutPressed(state.shortcutSave) && DirtyShnDocumentCount(state) > 0)) {
        const auto [saved,failed] = SaveAllDirtyShnDocuments(state);
        ed.report.push_back(std::string(L("Gespeichert: ", "Saved: ")) + std::to_string(saved) +
                            (failed ? std::string(", ") + L("Fehler: ", "errors: ") + std::to_string(failed) : std::string()));
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Schreibt alle geänderten SHN-Tabellen (Client und Server) auf die Platte.", "Writes all modified SHN tables (client and server) to disk."));
    ImGui::SameLine();
    DrawSearchInput("skillFilter",
                    L("Suche: Name oder InxName oder ID", "Search: name, InxName or ID"),
                    ed.filter, sizeof(ed.filter), 235.0f);
    ImGui::SameLine();
    if (SceneQuickFilterButton("skillAll",L("Alle","All"),ed.quickFilter==0)) ed.quickFilter=0;
    ImGui::SameLine();
    if (SceneQuickFilterButton("skillChanged",L("Geändert","Modified"),ed.quickFilter==1)) ed.quickFilter=1;
    ImGui::SameLine();
    if (SceneQuickFilterButton("skillSync",L("Sync-Probleme","Sync issues"),ed.quickFilter==2)) ed.quickFilter=2;

    // Liste (Clipper, gecacht)
    const std::string key = std::string(ed.filter) + "|" + std::to_string(asf.rows.size()) + "|" +
                            std::to_string(state.shnEditCounter) + "|qf=" + std::to_string(ed.quickFilter) +
                            "|docs=" + std::to_string(d.skillC) + "," + std::to_string(d.skillS) + "," +
                            std::to_string(d.server) + "," + std::to_string(d.viewC) + "," + std::to_string(d.viewS);
    if (key != ed.listKey) {
        ed.listKey = key;
        ed.visible.clear();
        const std::string needle = LowerAscii(ed.filter);

        std::unordered_set<long long> modifiedIds;
        auto collectIds = [&](int docIdx, std::unordered_set<long long>& ids, bool dirtyOnly) {
            if (docIdx < 0 || docIdx >= static_cast<int>(state.shnFiles.size())) return;
            const auto& doc = state.shnFiles[static_cast<std::size_t>(docIdx)];
            for (std::size_t row = 0; row < doc.file.rows.size(); ++row) {
                if (dirtyOnly) {
                    if (row >= doc.cellDirty.size()) continue;
                    const bool rowDirty = std::any_of(doc.cellDirty[row].begin(), doc.cellDirty[row].end(),
                                                      [](std::uint8_t v){ return v != 0; });
                    if (!rowDirty) continue;
                }
                const std::string idText = ShnCellText(doc.file,row,"ID");
                if (!idText.empty()) ids.insert(std::atoll(idText.c_str()));
            }
        };
        if (ed.quickFilter == 1) {
            collectIds(d.skillC,modifiedIds,true);
            collectIds(d.skillS,modifiedIds,true);
            collectIds(d.viewC,modifiedIds,true);
            collectIds(d.viewS,modifiedIds,true);
            collectIds(d.server,modifiedIds,true);
        }

        const auto rowsDifferBySharedColumns = [&](int leftDoc,int rightDoc,long long id) {
            if (leftDoc < 0 || rightDoc < 0) return true;
            const long long lr=SkillRowIn(state,leftDoc,id), rr=SkillRowIn(state,rightDoc,id);
            if (lr < 0 || rr < 0) return true;
            const auto& lf=state.shnFiles[static_cast<std::size_t>(leftDoc)].file;
            const auto& rf=state.shnFiles[static_cast<std::size_t>(rightDoc)].file;
            const auto matched=MatchShnColumnsByName(lf,rf);
            for (std::size_t ci=0;ci<matched.size();++ci) {
                if (matched[ci] < 0) continue;
                const auto rci=static_cast<std::size_t>(matched[ci]);
                const auto lri=static_cast<std::size_t>(lr), rri=static_cast<std::size_t>(rr);
                if (lri >= lf.rows.size() || rri >= rf.rows.size() ||
                    ci >= lf.rows[lri].values.size() || rci >= rf.rows[rri].values.size()) return true;
                if (core::legacy::ShnValueToString(lf.rows[lri].values[ci]) !=
                    core::legacy::ShnValueToString(rf.rows[rri].values[rci])) return true;
            }
            return false;
        };

        ed.syncIssues.clear();
        for (std::size_t r=0;r<asf.rows.size();++r) {
            const long long id=std::atoll(ShnCellText(asf,r,"ID").c_str());
            const bool missingServerInfo=d.server < 0 || SkillRowIn(state,d.server,id) < 0;
            if (missingServerInfo ||
                rowsDifferBySharedColumns(d.skillC,d.skillS,id) ||
                rowsDifferBySharedColumns(d.viewC,d.viewS,id))
                ed.syncIssues.insert(id);
        }

        for (std::size_t r = 0; r < asf.rows.size(); ++r) {
            const std::string idText = ShnCellText(asf,r,"ID");
            const long long id = std::atoll(idText.c_str());
            if (!needle.empty()) {
                const std::string hay = LowerAscii(ShnCellText(asf, r, "InxName") + " " + ShnCellText(asf, r, "Name") + " " + idText);
                if (hay.find(needle) == std::string::npos) continue;
            }
            if (ed.quickFilter == 1 && !modifiedIds.contains(id)) continue;
            if (ed.quickFilter == 2 && !ed.syncIssues.contains(id)) continue;
            ed.visible.push_back(r);
        }
    }
    const float skillListW = std::clamp(ImGui::GetContentRegionAvail().x * 0.29f, 320.0f, 430.0f);
    ImGui::BeginChild("##skilllist", ImVec2(skillListW, 0.0f), true);
    const std::string skillListMeta = std::to_string(ed.visible.size()) + " / " +
                                      std::to_string(asf.rows.size()) + " · " +
                                      L("Sync-Probleme: ","Sync issues: ") +
                                      std::to_string(ed.syncIssues.size());
    DrawPanelHeader("skillListHeader", "SKILLS", DrawIconBolt, "module.skill",
                    skillListMeta.c_str());
    {
        struct SeriesGroup {
            std::string key;
            std::string displayName;
            std::vector<std::size_t> rows;
        };
        std::vector<SeriesGroup> groups;
        std::unordered_map<std::string, std::size_t> groupIndex;
        groups.reserve(ed.visible.size());

        auto seriesKeyOf = [&](std::size_t row) {
            std::string key = ShnCellText(asf, row, "InxName");
            while (!key.empty() && std::isdigit(static_cast<unsigned char>(key.back()))) key.pop_back();
            return key.empty() ? ShnCellText(asf, row, "InxName") : key;
        };

        for (const std::size_t row : ed.visible) {
            const std::string key = seriesKeyOf(row);
            auto [it, inserted] = groupIndex.emplace(key, groups.size());
            if (inserted) {
                SeriesGroup group;
                group.key = key;
                group.displayName = ShnCellText(asf, row, "Name");
                groups.push_back(std::move(group));
            }
            groups[it->second].rows.push_back(row);
        }

        for (auto& group : groups) {
            std::stable_sort(group.rows.begin(), group.rows.end(), [&](std::size_t a, std::size_t b) {
                const int sa = std::atoi(ShnCellText(asf, a, "Step").c_str());
                const int sb = std::atoi(ShnCellText(asf, b, "Step").c_str());
                if (sa != sb) return sa < sb;
                return a < b;
            });

            bool containsSelected = false;
            for (const auto row : group.rows) {
                const long long id = std::atoll(ShnCellText(asf, row, "ID").c_str());
                if (id == ed.selectedId) { containsSelected = true; break; }
            }

            ImGui::PushID(group.key.c_str());
            const std::string title = group.key + "  ·  " + std::to_string(group.rows.size()) +
                                      (group.rows.size() == 1 ? " Stufe" : " Stufen");
            const ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_SpanAvailWidth |
                (containsSelected ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            if (ImGui::TreeNodeEx("##series", flags, "%s", title.c_str())) {
                if (!group.displayName.empty()) {
                    ImGui::TextDisabled("%s", group.displayName.c_str());
                }
                for (const std::size_t row : group.rows) {
                    const long long id = std::atoll(ShnCellText(asf, row, "ID").c_str());
                    const int step = std::atoi(ShnCellText(asf, row, "Step").c_str());
                    const std::string name = ShnCellText(asf, row, "Name");
                    std::string label =
                        std::string("Stufe ") + std::to_string(step) + "  ·  #" + std::to_string(id) +
                        (name.empty() ? std::string() : "  " + name);
                    if (ed.syncIssues.contains(id)) label += L("  ! Sync","  ! Sync");
                    if (UI::Selectable((label + "##skillStep" + std::to_string(row)).c_str(), ed.selectedId == id)) {
                        ed.selectedId = id;
                        ed.report.clear();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##skilldetail", ImVec2(0.0f, 0.0f), true);
    DrawPanelHeader("skillPropertiesHeader", L("EIGENSCHAFTEN","PROPERTIES"),
                    DrawIconGear, "panel.properties");
    const long long sel = ed.selectedId;
    const long long selRow = sel >= 0 ? SkillRowIn(state, d.skillC, sel) : -1;
    if (selRow < 0) {
        ImGui::TextDisabled("%s", L("Links einen Skill wählen.", "Choose a skill on the left."));
    } else {
        const std::string inx = ShnCellText(asf, static_cast<std::size_t>(selRow), "InxName");
        ImGui::Text("%s  #%lld  %s", inx.c_str(), sel, ShnCellText(asf, static_cast<std::size_t>(selRow), "Name").c_str());
        ImGui::TextDisabled("%s", L("Der InxName eines vorhandenen Skills ist fest (Verweise anderer Tabellen). Für einen neuen Namen: unten 'Neuen Skill anlegen'.",
                                    "The InxName of an existing skill is fixed (other tables refer to it). For a new name use 'Create new skill' below."));
        ImGui::Separator();

        // --- Neu anlegen / Stufe / Serie
        if (UI::CollapsingHeader(L("Neuen Skill aus diesem anlegen (Vorlage)", "Create new skill from this one (template)"))) {
            ImGui::TextWrapped("%s", L("Klont ALLE Zeilen dieses Skills (Werte, Server-Werte, Darstellung mit Animationen/Effekten) unter einer neuen ID. Danach Werte, Animationen und Effekte unten anpassen.",
                                       "Clones ALL rows of this skill (values, server values, presentation with animations/effects) under a new ID. Then adjust values, animations and effects below."));
            ImGui::SetNextItemWidth(220.0f); UI::InputText(L("Neuer InxName (eindeutig)", "New InxName (unique)"), ed.newInx, sizeof(ed.newInx));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Interner Schlüsselname des neuen Skills (z.B. MeinFeuerball01). Muss in ActiveSkill eindeutig sein.", "Internal key name of the new skill (e.g. MyFireball01). Must be unique in ActiveSkill."));
            ImGui::SetNextItemWidth(260.0f); UI::InputText(L("Neuer Anzeigename", "New display name"), ed.newName, sizeof(ed.newName));
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Name, der im Spiel angezeigt wird (leer = Name der Vorlage).", "Name shown in game (empty = name of the template)."));
            UI::Checkbox(L("Skillbuch-Item mit anlegen (lernbar/handelbar)", "Also create skill book item (learnable/tradable)"), &ed.makeBook);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Ein Skill wird im Spiel über ein Item mit gleichem InxName gelernt (ItemInfo, MarketIndex=Skill). Klont dieses Item mit neuer Item-ID.", "A skill is learned through an item with the same InxName (ItemInfo, MarketIndex=Skill). Clones that item with a new item ID."));
            if (UI::Button(L("Neuen Skill anlegen", "Create new skill"))) CreateSkillFrom(state, d, sel, ed.newInx, ed.newName, ed.makeBook);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Klont alle Zeilen des gewählten Skills in Client und Server unter neuer ID.", "Clones all rows of the selected skill in client and server under a new ID."));
            ImGui::SameLine();
            if (UI::Button(L("Nächste Stufe anlegen", "Create next step"))) {
                std::string base = inx;
                while (!base.empty() && std::isdigit(static_cast<unsigned char>(base.back()))) base.pop_back();
                const int step = std::atoi(ShnCellText(asf, static_cast<std::size_t>(selRow), "Step").c_str());
                const std::string nextInx = base + std::to_string(step + 1);
                std::string nm = ShnCellText(asf, static_cast<std::size_t>(selRow), "Name");
                const std::string oldTag = "[" + std::to_string(step) + "]";
                if (const auto p = nm.find(oldTag); p != std::string::npos) nm.replace(p, oldTag.size(), "[" + std::to_string(step + 1) + "]");
                CreateSkillFrom(state, d, sel, nextInx, nm, ed.makeBook);
                if (ed.selectedId != sel) {
                    SkillCellWrite(state, d, skilled::Doc::Skill, "Step", ed.selectedId, std::to_string(step + 1));
                    SkillCellWrite(state, d, skilled::Doc::Skill, "DemandSk", ed.selectedId, inx);
                    ed.report.push_back(L("Stufe gesetzt, Voraussetzung = Vorstufe.", "Step set, requirement = previous step."));
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Legt <Reihe><Stufe+1> an: Stufe +1, Voraussetzungs-Skill = dieser Skill.", "Creates <series><step+1>: step +1, required skill = this skill."));
        }
        if (UI::CollapsingHeader(L("Ganze Reihe skalieren (z.B. Schaden +20 %)", "Scale the whole series (e.g. damage +20 %)"))) {
            std::string series = inx;
            while (!series.empty() && std::isdigit(static_cast<unsigned char>(series.back()))) series.pop_back();
            ImGui::Text("%s: %s", L("Reihe", "Series"), series.c_str());
            ImGui::SetNextItemWidth(140.0f); UI::SliderInt("##scalepct", &ed.scalePercent, 10, 400, "%d %%");
            UI::Checkbox(L("Schaden (MinWC/MaxWC/MinMA/MaxMA)", "Damage (MinWC/MaxWC/MinMA/MaxMA)"), &ed.scaleDamage);
            UI::Checkbox(L("Kosten (SP/HP/LP)", "Costs (SP/HP/LP)"), &ed.scaleCost);
            UI::Checkbox(L("Abklingzeit (DlyTime/DlyTimeGroup)", "Cooldown (DlyTime/DlyTimeGroup)"), &ed.scaleCooldown);
            UI::Checkbox(L("Zauberzeit (CastTime)", "Cast time (CastTime)"), &ed.scaleCast);
            if (UI::Button(L("Auf alle Stufen der Reihe anwenden", "Apply to all steps of the series"))) {
                std::vector<const char*> cols;
                if (ed.scaleDamage) { cols.push_back("MinWC"); cols.push_back("MaxWC"); cols.push_back("MinMA"); cols.push_back("MaxMA"); }
                if (ed.scaleCost) { cols.push_back("SP"); cols.push_back("HP"); cols.push_back("LP"); }
                if (ed.scaleCooldown) { cols.push_back("DlyTime"); cols.push_back("DlyTimeGroup"); }
                if (ed.scaleCast) cols.push_back("CastTime");
                const int n = ScaleSkillSeries(state, d, series, ed.scalePercent, cols);
                ed.report.push_back(std::to_string(n) + L(" Werte geändert (Client+Server).", " values changed (client+server)."));
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", L("Multipliziert die gewählten Werte JEDER Stufe der Reihe mit dem Prozentsatz (Client und Server).", "Multiplies the chosen values of EVERY step of the series by the percentage (client and server)."));
            ImGui::TextDisabled("%s", L("Skaliert jeden Wert jeder Stufe der Reihe um den Prozentsatz (100 % = unverändert).", "Scales every value of every step of the series by the percentage (100 % = unchanged)."));
        }
        for (const auto& line : ed.report) ImGui::TextWrapped("%s", line.c_str());
        ImGui::Separator();

        // --- Formular
        for (const auto& sec : skilled::kSections) {
            if (!UI::CollapsingHeader(de ? sec.titleDe : sec.titleEn, sec.defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0)) continue;
            ImGui::PushID(sec.titleEn);
            if (ImGui::BeginTable("##form", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("##l", ImGuiTableColumnFlags_WidthFixed, 250.0f);
                ImGui::TableSetupColumn("##v", ImGuiTableColumnFlags_WidthStretch);
                for (std::size_t i = 0; i < sec.count; ++i) DrawSkillField(state, d, sec.fields[i], sel, dim);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }
        // Zustaende (Buffs/Debuffs): 4 Zeilen Name/Staerke/Chance
        if (UI::CollapsingHeader(L("Zustände (Buffs/Debuffs)", "States (buffs/debuffs)"))) {
            ImGui::TextDisabled("%s", L("Bis zu 4 Zustände (AbState-Namen) mit Stärke und Erfolgschance.", "Up to 4 states (AbState names) with strength and success chance."));
            if (ImGui::BeginTable("##states", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26.0f);
                ImGui::TableSetupColumn(L("Zustand", "State"), ImGuiTableColumnFlags_WidthFixed, 380.0f);
                ImGui::TableSetupColumn(L("Stärke", "Strength"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(L("Chance", "Chance"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableHeadersRow();
                static const char* const kSuffix[4] = {"A", "B", "C", "D"};
                for (int i = 0; i < 4; ++i) {
                    ImGui::PushID(i);
                    const std::string nameCol = std::string("StaName") + kSuffix[i], strCol = std::string("StaStrength") + kSuffix[i], rateCol = std::string("StaSucRate") + kSuffix[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", kSuffix[i]);
                    ImGui::TableSetColumnIndex(1);
                    {
                        std::string cur = SkillCellRead(state, d, skilled::Doc::Skill, nameCol.c_str(), sel);
                        std::vector<char> buf(cur.begin(), cur.end()); buf.resize(std::max<std::size_t>(buf.size() + 1, 128), '\0');
                        ImGui::SetNextItemWidth(280.0f);
                        if (UI::InputText("##n", buf.data(), buf.size())) SkillCellWrite(state, d, skilled::Doc::Skill, nameCol.c_str(), sel, std::string(buf.data()));
                        ImGui::SameLine();
                        if (UI::SmallButton("...")) { ed.pickFilter[0] = '\0'; ImGui::OpenPopup("##pickstate"); }
                        std::string chosen;
                        if (StringPickerPopup("##pickstate", ed.pickerOptions["StateRef"], ed.pickFilter, sizeof(ed.pickFilter), chosen, &ed.pickerLabels["StateRef"]))
                            SkillCellWrite(state, d, skilled::Doc::Skill, nameCol.c_str(), sel, chosen);
                    }
                    for (int c = 0; c < 2; ++c) {
                        ImGui::TableSetColumnIndex(2 + c);
                        const std::string& col = c == 0 ? strCol : rateCol;
                        int v = std::atoi(SkillCellRead(state, d, skilled::Doc::Skill, col.c_str(), sel).c_str());
                        ImGui::SetNextItemWidth(96.0f);
                        if (UI::InputInt(c == 0 ? "##s" : "##r", &v, 0, 0)) SkillCellWrite(state, d, skilled::Doc::Skill, col.c_str(), sel, std::to_string(v));
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        // Alle uebrigen Spalten roh
        if (UI::CollapsingHeader(L("Alle übrigen Spalten (roh)", "All remaining columns (raw)"))) {
            ImGui::TextDisabled("%s", L("Spalten ohne eigene Beschreibung (u.a. die Stufen-Reihen nT0..nT3 - je 5 Werte pro Skillstufe). Werte als Text.", "Columns without their own description (incl. the series nT0..nT3 - 5 values per skill step). Values as text."));
            std::set<std::string> curated;
            for (const auto& sec : skilled::kSections) for (std::size_t i = 0; i < sec.count; ++i) curated.insert(sec.fields[i].col);
            for (const char* extra : {"ID", "InxName", "StaNameA", "StaNameB", "StaNameC", "StaNameD", "StaStrengthA", "StaStrengthB", "StaStrengthC", "StaStrengthD", "StaSucRateA", "StaSucRateB", "StaSucRateC", "StaSucRateD"}) curated.insert(extra);
            if (ImGui::BeginTable("##raw", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn(L("Tabelle", "Table"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
                ImGui::TableSetupColumn(L("Spalte", "Column"), ImGuiTableColumnFlags_WidthFixed, 190.0f);
                ImGui::TableSetupColumn(L("Wert", "Value"), ImGuiTableColumnFlags_WidthFixed, 260.0f);
                ImGui::TableHeadersRow();
                struct Src { const char* label; int doc; skilled::Doc kind; };
                const Src srcs[] = {{"ActiveSkill", d.skillC, skilled::Doc::Skill}, {"ActiveSkillInfoServer", d.server, skilled::Doc::Server}, {"ActiveSkillView", d.viewC, skilled::Doc::View}};
                for (const auto& sr : srcs) {
                    if (sr.doc < 0) continue;
                    const auto& f = state.shnFiles[static_cast<std::size_t>(sr.doc)].file;
                    for (const auto& col : f.columns) {
                        if (curated.count(col.name)) continue;
                        ImGui::PushID(sr.label); ImGui::PushID(col.name.c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(sr.label);
                        ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(col.name.c_str());
                        ImGui::TableSetColumnIndex(2);
                        std::string cur = SkillCellRead(state, d, sr.kind, col.name.c_str(), sel);
                        std::vector<char> buf(cur.begin(), cur.end()); buf.resize(std::max<std::size_t>(buf.size() + 1, 256), '\0');
                        ImGui::SetNextItemWidth(240.0f);
                        if (UI::InputText("##v", buf.data(), buf.size())) SkillCellWrite(state, d, sr.kind, col.name.c_str(), sel, std::string(buf.data()));
                        ImGui::PopID(); ImGui::PopID();
                    }
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

} // namespace nge
