#include "KfmPanel.hpp"
#include "mapeditor/app/Localization.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace theseed::mapeditor::app {
namespace {
const char* L(const char* de,const char* en) { return CurrentLanguage()==Language::German?de:en; }
std::string utf8(const std::filesystem::path& path) { const auto s=path.u8string();return {s.begin(),s.end()}; }
std::string lower(std::string value) { for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value; }
}
bool KfmPanel::Open(const std::filesystem::path& path) {
    auto loaded=core::LoadKfmFile(path);
    if(!loaded) { message_=loaded.error();return false; }
    source_=path;file_=std::move(*loaded);references_.reset();selected_=0;transitionCount_=0;dirty_=false;
    previewKf_.reset(); previewKfPath_.clear(); previewAnimationIndex_=static_cast<std::size_t>(-1);
    previewNif_.reset(); previewNifPath_.clear(); previewNifMessage_.clear();
    previewTime_=0.0f; previewPlaying_=false; previewMessage_.clear();
    for(const auto& a:file_->animations)transitionCount_+=a.transitions.size();
    std::snprintf(path_,sizeof(path_),"%s",utf8(path).c_str());
    const auto copy=path.parent_path()/(path.stem().string()+"-copy.kfm");
    std::snprintf(exportPath_,sizeof(exportPath_),"%s",utf8(copy).c_str());
    filter_[0]=0;message_.clear();Filter();return true;
}
void KfmPanel::Filter() {
    visible_.clear();if(!file_)return;
    const auto needle=lower(filter_);
    for(std::size_t i=0;i<file_->animations.size();++i) {
        const auto& a=file_->animations[i];
        if(needle.empty() || lower(a.kfFileName+" "+a.name+" "+std::to_string(a.eventCode)).find(needle)!=std::string::npos)visible_.push_back(i);
    }
}
void KfmPanel::MarkEdited(bool referencesChanged) {
    dirty_ = true;
    if (referencesChanged) references_.reset();
    Filter();
}

void KfmPanel::LoadSelectedKfPreview() {
    previewPlaying_ = false;
    previewKf_.reset();
    previewKfPath_.clear();
    previewAnimationIndex_ = static_cast<std::size_t>(-1);
    previewMessage_.clear();
    previewNifMessage_.clear();

    if (!file_ || selected_ >= file_->animations.size()) {
        previewMessage_ = L("Keine Animation ausgewählt.", "No animation selected.");
        return;
    }

    if (!references_) references_ = core::InspectKfmReferences(*file_, source_);
    if (selected_ >= references_->animations.size() || !references_->animations[selected_]) {
        previewMessage_ = L("KF-Datei konnte nicht aufgelöst werden.", "KF file could not be resolved.");
        return;
    }

    const auto& path = *references_->animations[selected_];
    auto loaded = core::LoadKfAnimation(path);
    if (!loaded) {
        previewMessage_ = loaded.error();
        return;
    }

    previewKfPath_ = path;
    previewKf_ = std::move(*loaded);
    previewAnimationIndex_ = selected_;
    previewTime_ = previewKf_->sequence.startTime;
    previewMessage_ = L("KF geladen.", "KF loaded.");

    // KFM liefert die NIF-Referenz explizit. Für den Skeleton-Viewport wird exakt diese
    // aufgelöste Datei geladen; es gibt keine rekursive Basename-Suche und keinen geratenen
    // Charakter-/Skeleton-Pfad.
    if (references_->nif) {
        if (!previewNif_ || previewNifPath_ != *references_->nif) {
            auto nif = core::LoadNifMesh(*references_->nif);
            if (nif) {
                previewNif_ = std::move(*nif);
                previewNifPath_ = *references_->nif;
                previewNifMessage_ = L("KFM-NIF geladen.", "KFM NIF loaded.");
            } else {
                previewNif_.reset();
                previewNifPath_.clear();
                previewNifMessage_ = nif.error();
            }
        }
    } else {
        previewNif_.reset();
        previewNifPath_.clear();
        previewNifMessage_ = L("KFM-NIF konnte nicht aufgelöst werden.",
                               "KFM NIF could not be resolved.");
    }
}


void KfmPanel::DrawSkeletonPreview() {
    if (!previewKf_) return;

    ImGui::SeparatorText(L("Skeleton-Viewport", "Skeleton viewport"));
    if (!previewNif_ || previewNif_->nodes.empty()) {
        ImGui::TextColored(ImVec4(1.0f,0.62f,0.30f,1.0f), "%s",
                           previewNifMessage_.empty()
                               ? L("Keine NIF-Hierarchie für die Vorschau verfügbar.",
                                   "No NIF hierarchy is available for preview.")
                               : previewNifMessage_.c_str());
        return;
    }

    struct Transform {
        core::NifVec3 t{};
        std::array<float,9> r{1,0,0,0,1,0,0,0,1};
        float s = 1.0f;
    };

    const auto quatMatrix=[](core::KfQuat q) {
        const float len=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
        if (len > 1.0e-8f) { q.w/=len; q.x/=len; q.y/=len; q.z/=len; }
        const float xx=q.x*q.x, yy=q.y*q.y, zz=q.z*q.z;
        const float xy=q.x*q.y, xz=q.x*q.z, yz=q.y*q.z;
        const float wx=q.w*q.x, wy=q.w*q.y, wz=q.w*q.z;
        return std::array<float,9>{
            1.0f-2.0f*(yy+zz), 2.0f*(xy-wz),       2.0f*(xz+wy),
            2.0f*(xy+wz),       1.0f-2.0f*(xx+zz), 2.0f*(yz-wx),
            2.0f*(xz-wy),       2.0f*(yz+wx),       1.0f-2.0f*(xx+yy)
        };
    };
    const auto rotate=[](const std::array<float,9>& r,const core::NifVec3& v) {
        return core::NifVec3{
            r[0]*v.x+r[1]*v.y+r[2]*v.z,
            r[3]*v.x+r[4]*v.y+r[5]*v.z,
            r[6]*v.x+r[7]*v.y+r[8]*v.z
        };
    };
    const auto multiplyRotation=[](const std::array<float,9>& a,const std::array<float,9>& b) {
        std::array<float,9> out{};
        for (int row=0;row<3;++row)
            for (int col=0;col<3;++col)
                out[static_cast<std::size_t>(row*3+col)] =
                    a[static_cast<std::size_t>(row*3+0)]*b[static_cast<std::size_t>(0*3+col)] +
                    a[static_cast<std::size_t>(row*3+1)]*b[static_cast<std::size_t>(1*3+col)] +
                    a[static_cast<std::size_t>(row*3+2)]*b[static_cast<std::size_t>(2*3+col)];
        return out;
    };

    const auto& nodes=previewNif_->nodes;
    const auto& kf=*previewKf_;
    std::unordered_map<std::string,const core::KfControlledTrack*> trackByNode;
    std::unordered_set<std::string> duplicateTrackNames;
    for (const auto& track:kf.sequence.transformTracks) {
        if (track.nodeName.empty()) continue;
        const auto [it,inserted]=trackByNode.emplace(track.nodeName,&track);
        if (!inserted) duplicateTrackNames.insert(track.nodeName);
    }

    std::vector<Transform> local(nodes.size());
    std::vector<char> animated(nodes.size(),0);
    std::vector<char> unsupported(nodes.size(),0);
    std::size_t matched=0, unsupportedCount=0;
    for (std::size_t i=0;i<nodes.size();++i) {
        local[i].t=nodes[i].localTranslation;
        local[i].r=nodes[i].localRotation;
        local[i].s=nodes[i].localScale;
        if (nodes[i].name.empty()) continue;
        const auto it=trackByNode.find(nodes[i].name);
        if (it==trackByNode.end()) continue;
        auto sampled=core::SampleKfTransformTrack(kf,*it->second,previewTime_);
        if (!sampled) {
            unsupported[i]=1;
            ++unsupportedCount;
            continue;
        }
        local[i].t={sampled->translation.x,sampled->translation.y,sampled->translation.z};
        local[i].r=quatMatrix(sampled->rotation);
        local[i].s=sampled->scale;
        animated[i]=1;
        ++matched;
    }

    std::vector<Transform> world(nodes.size());
    std::vector<std::uint8_t> visit(nodes.size(),0);
    std::function<void(std::size_t)> buildWorld=[&](std::size_t index) {
        if (visit[index]==2) return;
        if (visit[index]==1) { // defensive cycle break: keep local transform.
            world[index]=local[index];
            visit[index]=2;
            return;
        }
        visit[index]=1;
        const int parent=nodes[index].parentIndex;
        if (parent>=0 && static_cast<std::size_t>(parent)<nodes.size()) {
            buildWorld(static_cast<std::size_t>(parent));
            const Transform& p=world[static_cast<std::size_t>(parent)];
            const core::NifVec3 scaledLocal{
                local[index].t.x*p.s,local[index].t.y*p.s,local[index].t.z*p.s};
            const core::NifVec3 rotatedLocal=rotate(p.r,scaledLocal);
            world[index].t={p.t.x+rotatedLocal.x,p.t.y+rotatedLocal.y,p.t.z+rotatedLocal.z};
            world[index].r=multiplyRotation(p.r,local[index].r);
            world[index].s=p.s*local[index].s;
        } else {
            world[index]=local[index];
        }
        visit[index]=2;
    };
    for (std::size_t i=0;i<nodes.size();++i) buildWorld(i);

    // Legacy/Gamebryo (x,y,z) -> Editor frame (x,z,y), matching NifModel::position.
    std::vector<ImVec2> projected(nodes.size());
    std::vector<core::NifVec3> editorPoints(nodes.size());
    for (std::size_t i=0;i<nodes.size();++i)
        editorPoints[i]={world[i].t.x,world[i].t.z,world[i].t.y};

    ImGui::TextDisabled(L("%zu NIF-Nodes · %zu Tracks gematcht · %zu aktuell nicht samplebar",
                          "%zu NIF nodes · %zu tracks matched · %zu currently not sampleable"),
                        nodes.size(),matched,unsupportedCount);
    if (!duplicateTrackNames.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f,0.68f,0.25f,1.0f),
                           L("· %zu doppelte Track-Namen","· %zu duplicate track names"),
                           duplicateTrackNames.size());
    }

    const ImVec2 avail=ImGui::GetContentRegionAvail();
    const ImVec2 canvasSize(std::max(260.0f,avail.x),
                            std::clamp(avail.y*0.48f,220.0f,360.0f));
    const ImVec2 canvasMin=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##kfmSkeletonViewport",canvasSize,
                           ImGuiButtonFlags_MouseButtonLeft);
    const ImVec2 canvasMax(canvasMin.x+canvasSize.x,canvasMin.y+canvasSize.y);
    ImDrawList* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled(canvasMin,canvasMax,IM_COL32(7,16,25,255),5.0f);
    dl->AddRect(canvasMin,canvasMax,IM_COL32(31,82,116,220),5.0f);

    const bool hovered=ImGui::IsItemHovered();
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left,0.0f)) {
        const ImVec2 delta=ImGui::GetIO().MouseDelta;
        previewSkeletonYaw_ += delta.x*0.008f;
        previewSkeletonPitch_=std::clamp(previewSkeletonPitch_+delta.y*0.008f,-1.45f,1.45f);
    }
    if (hovered && std::abs(ImGui::GetIO().MouseWheel)>0.0f)
        previewSkeletonZoom_=std::clamp(previewSkeletonZoom_*
            std::pow(1.12f,ImGui::GetIO().MouseWheel),0.25f,5.0f);
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        previewSkeletonYaw_=0.35f;
        previewSkeletonPitch_=-0.20f;
        previewSkeletonZoom_=1.0f;
    }

    const float cy=std::cos(previewSkeletonYaw_), sy=std::sin(previewSkeletonYaw_);
    const float cp=std::cos(previewSkeletonPitch_), sp=std::sin(previewSkeletonPitch_);
    std::vector<ImVec2> rotated2(nodes.size());
    float minX=std::numeric_limits<float>::max(),maxX=std::numeric_limits<float>::lowest();
    float minY=std::numeric_limits<float>::max(),maxY=std::numeric_limits<float>::lowest();
    for (std::size_t i=0;i<nodes.size();++i) {
        const auto& p=editorPoints[i];
        const float x1=cy*p.x-sy*p.z;
        const float z1=sy*p.x+cy*p.z;
        const float y2=cp*p.y-sp*z1;
        rotated2[i]={x1,y2};
        minX=std::min(minX,x1);maxX=std::max(maxX,x1);
        minY=std::min(minY,y2);maxY=std::max(maxY,y2);
    }
    const float spanX=std::max(maxX-minX,1.0f),spanY=std::max(maxY-minY,1.0f);
    const float fit=std::min((canvasSize.x-34.0f)/spanX,(canvasSize.y-34.0f)/spanY)*previewSkeletonZoom_;
    const float cx=(minX+maxX)*0.5f,cy2=(minY+maxY)*0.5f;
    const ImVec2 center(canvasMin.x+canvasSize.x*0.5f,canvasMin.y+canvasSize.y*0.5f);
    for (std::size_t i=0;i<nodes.size();++i)
        projected[i]={center.x+(rotated2[i].x-cx)*fit,
                      center.y-(rotated2[i].y-cy2)*fit};

    // Parent-child lines are the actual NIF hierarchy; cyan marks sampled KF nodes,
    // amber marks a matching track whose interpolation is intentionally unsupported.
    for (std::size_t i=0;i<nodes.size();++i) {
        const int parent=nodes[i].parentIndex;
        if (parent<0 || static_cast<std::size_t>(parent)>=nodes.size()) continue;
        const ImU32 line=unsupported[i] ? IM_COL32(240,166,68,235)
                          : animated[i] ? IM_COL32(32,221,242,235)
                                        : IM_COL32(105,132,151,150);
        dl->AddLine(projected[static_cast<std::size_t>(parent)],projected[i],
                    line,animated[i]||unsupported[i]?2.0f:1.0f);
    }
    for (std::size_t i=0;i<nodes.size();++i) {
        const ImU32 dot=unsupported[i] ? IM_COL32(255,180,75,255)
                       : animated[i] ? IM_COL32(128,238,255,255)
                                     : IM_COL32(135,157,173,205);
        dl->AddCircleFilled(projected[i],animated[i]||unsupported[i]?2.7f:1.7f,dot);
    }

    dl->AddText(ImVec2(canvasMin.x+10.0f,canvasMin.y+8.0f),IM_COL32(169,197,217,225),
                L("Drag: drehen · Wheel: Zoom · Doppelklick: Reset",
                  "Drag: rotate · Wheel: zoom · Double-click: reset"));
    if (!previewNifPath_.empty()) {
        const std::string label=previewNifPath_.filename().string();
        dl->AddText(ImVec2(canvasMin.x+10.0f,canvasMax.y-22.0f),
                    IM_COL32(108,139,161,220),label.c_str());
    }

    if (hovered) {
        // nearest node hover, useful for verifying KF ↔ NIF name matching without cluttering
        // the viewport with permanent labels.
        const ImVec2 mouse=ImGui::GetMousePos();
        float best=64.0f;
        std::size_t bestIndex=nodes.size();
        for (std::size_t i=0;i<nodes.size();++i) {
            const float dx=mouse.x-projected[i].x,dy=mouse.y-projected[i].y;
            const float d2=dx*dx+dy*dy;
            if (d2<best) { best=d2; bestIndex=i; }
        }
        if (bestIndex<nodes.size() && !nodes[bestIndex].name.empty()) {
            ImGui::SetTooltip("%s%s",nodes[bestIndex].name.c_str(),
                unsupported[bestIndex]
                    ? L("\nKF-Track vorhanden, aber Interpolation noch nicht verifiziert.",
                        "\nKF track exists, but interpolation is not yet verified.")
                    : animated[bestIndex]
                        ? L("\nKF-Track aktiv gesampelt.","\nKF track actively sampled.")
                        : "");
        }
    }

    ImGui::TextDisabled("%s",
        L("Viewport = echte NIF-Hierarchie + verifizierte KF-Local-Transforms. "
          "Komprimierte B-Splines/TBC bleiben bewusst in Bind-Pose statt geraten zu werden.",
          "Viewport = real NIF hierarchy + verified KF local transforms. "
          "Compressed B-splines/TBC deliberately remain in bind pose instead of being guessed."));
}

void KfmPanel::Draw(const std::function<std::optional<std::string>()>& browse) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));

    // Datei-/Command-Leiste
    ImGui::BeginChild("##kfmFileBar", ImVec2(0, 74.0f), true);
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "%s", L("KFM-DATEI", "KFM FILE"));
    ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 290.0f));
    const bool enter = ImGui::InputText("##kfm-path", path_, sizeof(path_), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button(L("Öffnen", "Open")) || enter) Open(std::filesystem::u8path(path_));
    if (browse) {
        ImGui::SameLine();
        if (ImGui::Button(L("Durchsuchen...", "Browse...")))
            if (auto p = browse()) Open(std::filesystem::u8path(*p));
    }
    ImGui::EndChild();

    if (!message_.empty()) {
        ImGui::TextWrapped("%s", message_.c_str());
    }
    if (!file_) {
        ImGui::TextDisabled("%s", L("Keine KFM-Datei geladen.", "No KFM file loaded."));
        ImGui::PopStyleVar();
        return;
    }

    auto& f = *file_;

    // Kompakte Status-/Metadatenzeile
    ImGui::BeginChild("##kfmStats", ImVec2(0, 76.0f), true);
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "KFM %s%s",
                       core::KfmVersionName(f.version), dirty_ ? " *" : "");
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu %s | %zu %s", f.animations.size(), L("Animationen", "animations"),
                        transitionCount_, L("Übergänge", "transitions"));
    ImGui::TextWrapped("NIF: %s", f.nifFileName.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| %s: %s", L("Wurzel", "Root"), f.master.c_str());
    ImGui::EndChild();

    // Action row
    if (ImGui::Button(L("Dateiverweise prüfen", "Check file references")))
        references_ = core::InspectKfmReferences(f, source_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - 470.0f));
    ImGui::InputText("##kfm-copy", exportPath_, sizeof(exportPath_));
    ImGui::SameLine();
    if (ImGui::Button(L("Kopie exportieren", "Export copy"))) {
        auto saved = core::SaveKfmFile(f, std::filesystem::u8path(exportPath_));
        message_ = saved
            ? L(dirty_ ? "Bearbeitete KFM-Kopie gespeichert." : "KFM-Kopie gespeichert.",
                dirty_ ? "Edited KFM copy saved." : "KFM copy saved.")
            : saved.error();
    }

    if (references_) {
        const auto& r = *references_;
        ImGui::TextDisabled(
            L("NIF: %s | KF: %zu/%zu | doppelte IDs: %zu | fehlende Ziele: %zu | fehlende Zwischenanimationen: %zu",
              "NIF: %s | KF: %zu/%zu | duplicate IDs: %zu | missing targets: %zu | missing intermediate: %zu"),
            r.nif ? L("gefunden", "found") : L("nicht aufgelöst", "unresolved"),
            r.animations.size() - r.missingKfFiles, r.animations.size(), r.duplicateEventCodes,
            r.missingTransitionTargets, r.missingIntermediateTargets);
    }

    ImGui::SetNextItemWidth(360.0f);
    if (ImGui::InputTextWithHint("##kfm-filter",
            L("KF-Datei, Name oder Event-ID filtern", "Filter KF file, name or event ID"),
            filter_, sizeof(filter_))) Filter();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", visible_.size(), f.animations.size());

    const float gap = 8.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float leftW = std::max(460.0f, availW * 0.58f);

    ImGui::BeginChild("##kfmAnimationList", ImVec2(leftW, 0), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", L("ANIMATIONEN", "ANIMATIONS"));
    ImGui::Separator();
    if (ImGui::BeginTable("kfm-animations", 5,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY, ImVec2(0,0))) {
        ImGui::TableSetupColumn("Event ID", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableSetupColumn(L("KF-Datei", "KF file"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(L("Übergänge", "Transitions"), ImGuiTableColumnFlags_WidthFixed, 92);
        ImGui::TableSetupColumn(L("Referenz", "Reference"), ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible_.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto index = visible_[static_cast<std::size_t>(row)];
                const auto& a = f.animations[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::Selectable(std::to_string(a.eventCode).c_str(), selected_ == index,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    if (selected_ != index) {
                        selected_ = index;
                        previewPlaying_ = false;
                        if (previewAnimationIndex_ != selected_) {
                            previewKf_.reset();
                            previewKfPath_.clear();
                            previewAnimationIndex_ = static_cast<std::size_t>(-1);
                            previewMessage_.clear();
                        }
                    }
                }
                ImGui::TableNextColumn(); ImGui::Text("%d", a.index);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(a.kfFileName.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%zu", a.transitions.size());
                ImGui::TableNextColumn();
                if (!references_) {
                    ImGui::TextDisabled("%s", L("ungeprüft", "unchecked"));
                } else if (references_->animations[index]) {
                    ImGui::TextColored(ImVec4(0.45f,0.85f,0.60f,1.0f), "%s", L("gefunden", "found"));
                } else {
                    ImGui::TextColored(ImVec4(1.0f,0.45f,0.40f,1.0f), "%s", L("fehlt", "missing"));
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginChild("##kfmDetails", ImVec2(0,0), true);
    ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", L("DETAILS", "DETAILS"));
    ImGui::Separator();

    if (selected_ < f.animations.size()) {
        auto& a = f.animations[selected_];

        ImGui::SeparatorText(L("Animation", "Animation"));
        int eventCode = a.eventCode;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("Event ID##anim", &eventCode)) {
            a.eventCode = eventCode;
            MarkEdited(true);
        }
        ImGui::SameLine();
        int animationIndex = a.index;
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::InputInt("Index##anim", &animationIndex)) {
            a.index = animationIndex;
            MarkEdited(false);
        }

        {
            std::array<char,1024> buf{};
            std::snprintf(buf.data(),buf.size(),"%s",a.kfFileName.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(L("KF-Datei##anim", "KF file##anim"),buf.data(),buf.size())) {
                a.kfFileName = buf.data();
                previewKf_.reset();
                previewKfPath_.clear();
                previewAnimationIndex_ = static_cast<std::size_t>(-1);
                MarkEdited(true);
            }
        }
        if (f.version == core::KfmVersion::V1_2_4b) {
            std::array<char,512> buf{};
            std::snprintf(buf.data(),buf.size(),"%s",a.name.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(L("Legacy-Name##anim", "Legacy name##anim"),buf.data(),buf.size())) {
                a.name = buf.data();
                MarkEdited(false);
            }
        }

        if (references_ && selected_ < references_->animations.size() && references_->animations[selected_]) {
            ImGui::TextDisabled("%s", L("Aufgelöste Datei", "Resolved file"));
            ImGui::TextWrapped("%s", utf8(*references_->animations[selected_]).c_str());
        }

        ImGui::SeparatorText(L("Übergänge", "Transitions"));
        ImGui::TextDisabled("%s",
            L("Typ ist bewusst read-only: Struktur ist bekannt, Laufzeitsemantik der Typwerte noch nicht vollständig.",
              "Type is intentionally read-only: structure is known, runtime semantics of type values are not fully established."));

        std::optional<std::size_t> transitionToDelete;
        if (ImGui::BeginTable("kfm-transitions", 5,
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY, ImVec2(0, std::clamp(ImGui::GetContentRegionAvail().y * 0.34f, 120.0f, 245.0f)))) {
            ImGui::TableSetupColumn(L("Ziel", "Target"), ImGuiTableColumnFlags_WidthFixed, 88);
            ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 52);
            ImGui::TableSetupColumn(L("Dauer", "Duration"), ImGuiTableColumnFlags_WidthFixed, 92);
            ImGui::TableSetupColumn(L("Payload", "Payload"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 72);
            ImGui::TableHeadersRow();

            for (std::size_t row = 0; row < a.transitions.size(); ++row) {
                auto& t = a.transitions[row];
                ImGui::PushID(static_cast<int>(row));
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0f);
                int target = t.eventCode;
                if (ImGui::InputInt("##target",&target,0,0)) {
                    t.eventCode = target;
                    MarkEdited(true);
                }

                ImGui::TableNextColumn();
                ImGui::Text("%d",t.type);

                ImGui::TableNextColumn();
                if (t.type == 5) {
                    ImGui::TextDisabled("-");
                } else {
                    float duration = t.duration;
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputFloat("##duration",&duration,0.0f,0.0f,"%.4g")) {
                        t.duration = duration;
                        MarkEdited(false);
                    }
                }

                ImGui::TableNextColumn();
                if (t.type == 5)
                    ImGui::TextDisabled("%s",L("kein Payload","no payload"));
                else
                    ImGui::Text("%zu Keys · %zu Intermediates",t.textKeys.size(),t.intermediateAnimations.size());

                ImGui::TableNextColumn();
                if (t.type != 5) {
                    if (ImGui::SmallButton(L("Details", "Details")))
                        ImGui::OpenPopup("##transitionPayload");
                } else {
                    ImGui::TextDisabled("Type 5");
                }

                if (ImGui::BeginPopup("##transitionPayload")) {
                    ImGui::Text("%s %zu · Type %d",L("Übergang","Transition"),row,t.type);
                    ImGui::SeparatorText(L("Text-Key-Paare","Text-key pairs"));
                    std::optional<std::size_t> keyToDelete;
                    for (std::size_t ki=0;ki<t.textKeys.size();++ki) {
                        auto& key=t.textKeys[ki];
                        ImGui::PushID(static_cast<int>(ki));
                        std::array<char,384> src{},dst{};
                        std::snprintf(src.data(),src.size(),"%s",key.source.c_str());
                        std::snprintf(dst.data(),dst.size(),"%s",key.destination.c_str());
                        ImGui::SetNextItemWidth(170.0f);
                        if (ImGui::InputText("##src",src.data(),src.size())) {
                            key.source=src.data(); MarkEdited(false);
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(170.0f);
                        if (ImGui::InputText("##dst",dst.data(),dst.size())) {
                            key.destination=dst.data(); MarkEdited(false);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) keyToDelete=ki;
                        ImGui::PopID();
                    }
                    if (keyToDelete) {
                        t.textKeys.erase(t.textKeys.begin()+static_cast<std::ptrdiff_t>(*keyToDelete));
                        MarkEdited(false);
                    }
                    if (ImGui::SmallButton(L("+ Text-Key","+ Text key"))) {
                        t.textKeys.push_back({});
                        MarkEdited(false);
                    }

                    ImGui::SeparatorText(L("Intermediate-Animationen","Intermediate animations"));
                    std::optional<std::size_t> intermediateToDelete;
                    for (std::size_t mi=0;mi<t.intermediateAnimations.size();++mi) {
                        auto& mid=t.intermediateAnimations[mi];
                        ImGui::PushID(10000+static_cast<int>(mi));
                        int midEvent=mid.eventCode;
                        float midValue=mid.value;
                        ImGui::SetNextItemWidth(120.0f);
                        if (ImGui::InputInt("##midEvent",&midEvent,0,0)) {
                            mid.eventCode=midEvent; MarkEdited(true);
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(130.0f);
                        if (ImGui::InputFloat("##midValue",&midValue,0.0f,0.0f,"%.6g")) {
                            mid.value=midValue; MarkEdited(false);
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) intermediateToDelete=mi;
                        ImGui::PopID();
                    }
                    if (intermediateToDelete) {
                        t.intermediateAnimations.erase(
                            t.intermediateAnimations.begin()+static_cast<std::ptrdiff_t>(*intermediateToDelete));
                        MarkEdited(true);
                    }
                    if (ImGui::SmallButton(L("+ Intermediate","+ Intermediate"))) {
                        t.intermediateAnimations.push_back({});
                        MarkEdited(true);
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopupContextItem("##transitionContext")) {
                    ImGui::TextDisabled("%s %zu · Type %d",L("Übergang","Transition"),row,t.type);
                    if (ImGui::MenuItem(L("Übergang löschen","Delete transition")))
                        transitionToDelete=row;
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (transitionToDelete && *transitionToDelete < a.transitions.size()) {
            a.transitions.erase(a.transitions.begin()+static_cast<std::ptrdiff_t>(*transitionToDelete));
            --transitionCount_;
            MarkEdited(true);
        }

        if (ImGui::Button(L("+ Type-5-Übergang","+ Type-5 transition"))) {
            core::KfmTransition t;
            t.type = 5;
            t.eventCode = a.eventCode;
            a.transitions.push_back(std::move(t));
            ++transitionCount_;
            MarkEdited(true);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s",
            L("Neue Übergänge werden konservativ als payload-loser Type 5 angelegt.",
              "New transitions are conservatively created as payload-free type 5."));
    } else {
        ImGui::TextDisabled("%s", L("Keine Animation ausgewählt.", "No animation selected."));
    }

    ImGui::SeparatorText(L("KF Timeline / Track Preview", "KF Timeline / Track Preview"));
    const bool previewMatches = previewKf_ && previewAnimationIndex_ == selected_;
    if (!previewMatches) {
        if (ImGui::Button(L("Ausgewählte KF laden", "Load selected KF"))) LoadSelectedKfPreview();
        ImGui::SameLine();
        ImGui::TextDisabled("%s",
            L("Echtes KF-Sampling; komprimierte B-Splines werden noch nicht geraten.",
              "Real KF sampling; compressed B-splines are not guessed yet."));
    } else {
        auto& kf = *previewKf_;
        const float start = kf.sequence.startTime;
        const float stop = kf.sequence.stopTime;
        const float duration = std::max(0.0f, stop - start);

        if (previewPlaying_ && duration > 0.0f) {
            previewTime_ += ImGui::GetIO().DeltaTime * previewSpeed_ *
                            std::max(0.0f, kf.sequence.frequency);
            if (previewTime_ > stop) {
                if (previewLoop_) {
                    const float span = std::max(duration, 1.0e-6f);
                    previewTime_ = start + std::fmod(previewTime_ - start, span);
                } else {
                    previewTime_ = stop;
                    previewPlaying_ = false;
                }
            }
        }

        if (ImGui::Button(previewPlaying_ ? L("Pause", "Pause") : L("Play", "Play")))
            previewPlaying_ = !previewPlaying_;
        ImGui::SameLine();
        if (ImGui::Button(L("Start", "Start"))) {
            previewTime_ = start;
            previewPlaying_ = false;
        }
        ImGui::SameLine();
        ImGui::Checkbox(L("Loop", "Loop"), &previewLoop_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        ImGui::SliderFloat("##kfSpeed", &previewSpeed_, 0.1f, 3.0f, "%.1fx");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##kfTime", &previewTime_, start, stop,
                           duration > 0.0f ? "%.3f s" : "%.3f");

        std::size_t supported = 0, unsupported = 0;
        for (const auto& track : kf.sequence.transformTracks) {
            auto sample = core::SampleKfTransformTrack(kf, track, previewTime_);
            if (sample) ++supported;
            else ++unsupported;
        }

        ImGui::TextColored(ImVec4(0.35f,0.75f,1.0f,1.0f), "%s", kf.sequence.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%.3f .. %.3f s · %zu Tracks · %zu samplebar · %zu offen",
                            start, stop, kf.sequence.transformTracks.size(), supported, unsupported);
        ImGui::TextDisabled("%s", utf8(previewKfPath_).c_str());

        const float trackTableH = std::clamp(ImGui::GetContentRegionAvail().y - 36.0f, 130.0f, 300.0f);
        if (ImGui::BeginTable("##kfTrackPreview", 6,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                ImVec2(0, trackTableH))) {
            ImGui::TableSetupScrollFreeze(0,1);
            ImGui::TableSetupColumn(L("Knoten", "Node"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("Y", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn("Z", ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn(L("Skala", "Scale"), ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(kf.sequence.transformTracks.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const auto& track = kf.sequence.transformTracks[static_cast<std::size_t>(row)];
                    const auto sample = core::SampleKfTransformTrack(kf, track, previewTime_);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(track.nodeName.empty() ? "(unnamed)" : track.nodeName.c_str());
                    ImGui::TableNextColumn();
                    if (sample) {
                        ImGui::TextColored(ImVec4(0.45f,0.85f,0.60f,1.0f), "%s",
                                           track.compressedSpline ? "Spline" : "Keys");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f,0.62f,0.30f,1.0f), "%s",
                                           track.compressedSpline ? "Spline*" : "offen");
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", sample.error().c_str());
                    }
                    ImGui::TableNextColumn();
                    if (sample) ImGui::Text("%.2f", sample->translation.x); else ImGui::TextDisabled("-");
                    ImGui::TableNextColumn();
                    if (sample) ImGui::Text("%.2f", sample->translation.y); else ImGui::TextDisabled("-");
                    ImGui::TableNextColumn();
                    if (sample) ImGui::Text("%.2f", sample->translation.z); else ImGui::TextDisabled("-");
                    ImGui::TableNextColumn();
                    if (sample) ImGui::Text("%.3f", sample->scale); else ImGui::TextDisabled("-");
                }
            }
            ImGui::EndTable();
        }

        DrawSkeletonPreview();

        ImGui::TextDisabled("%s",
            L("Timeline und Skeleton-Viewport sampeln echte KF-Transforms. Mesh-Deformation "
              "bleibt separat gesperrt, bis animierte Skin-Weights/Bone-Matrizen Ende-zu-Ende verifiziert sind.",
              "Timeline and skeleton viewport sample real KF transforms. Mesh deformation remains "
              "separately locked until animated skin weights/bone matrices are verified end-to-end."));
    }
    if (!previewMessage_.empty()) {
        ImGui::TextWrapped("%s", previewMessage_.c_str());
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}
}
