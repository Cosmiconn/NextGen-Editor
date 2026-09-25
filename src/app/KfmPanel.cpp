#include "KfmPanel.hpp"
#include "mapeditor/app/Localization.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>

namespace theseed::mapeditor::app {
namespace {
const char* L(const char* de,const char* en) { return CurrentLanguage()==Language::German?de:en; }
std::string utf8(const std::filesystem::path& path) { const auto s=path.u8string();return {s.begin(),s.end()}; }
std::string lower(std::string value) { for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value; }
}
bool KfmPanel::Open(const std::filesystem::path& path) {
    auto loaded=core::LoadKfmFile(path);
    if(!loaded) { message_=loaded.error();return false; }
    source_=path;file_=std::move(*loaded);references_.reset();selected_=0;transitionCount_=0;
    previewKf_.reset(); previewKfPath_.clear(); previewAnimationIndex_=static_cast<std::size_t>(-1);
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
void KfmPanel::LoadSelectedKfPreview() {
    previewPlaying_ = false;
    previewKf_.reset();
    previewKfPath_.clear();
    previewAnimationIndex_ = static_cast<std::size_t>(-1);
    previewMessage_.clear();

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

    const auto& f = *file_;

    // Kompakte Status-/Metadatenzeile
    ImGui::BeginChild("##kfmStats", ImVec2(0, 76.0f), true);
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "KFM %s", core::KfmVersionName(f.version));
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
        message_ = saved ? L("KFM-Kopie gespeichert.", "KFM copy saved.") : saved.error();
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
        const auto& a = f.animations[selected_];
        ImGui::Text("Event ID %d", a.eventCode);
        ImGui::TextDisabled("Index %d", a.index);
        ImGui::TextWrapped("%s", a.kfFileName.c_str());
        if (!a.name.empty()) ImGui::TextDisabled("%s", a.name.c_str());
        if (references_ && references_->animations[selected_]) {
            ImGui::Separator();
            ImGui::TextDisabled("%s", L("Aufgelöste Datei", "Resolved file"));
            ImGui::TextWrapped("%s", utf8(*references_->animations[selected_]).c_str());
        }

        ImGui::Separator();
        ImGui::Text("%s (%zu)", L("Übergänge", "Transitions"), a.transitions.size());
        if (ImGui::BeginTable("kfm-transitions", 4,
                ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY, ImVec2(0, std::max(120.0f, ImGui::GetContentRegionAvail().y - 44.0f)))) {
            ImGui::TableSetupColumn(L("Ziel", "Target"), ImGuiTableColumnFlags_WidthFixed, 72);
            ImGui::TableSetupColumn(L("Typ", "Type"), ImGuiTableColumnFlags_WidthFixed, 48);
            ImGui::TableSetupColumn(L("Dauer", "Duration"), ImGuiTableColumnFlags_WidthFixed, 78);
            ImGui::TableSetupColumn(L("Details", "Details"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (std::size_t row = 0; row < a.transitions.size(); ++row) {
                const auto& t = a.transitions[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d", t.eventCode);
                ImGui::TableNextColumn(); ImGui::Text("%d", t.type);
                ImGui::TableNextColumn();
                if (t.type == 5) ImGui::TextUnformatted("-");
                else ImGui::Text("%.4g", t.duration);
                ImGui::TableNextColumn();
                ImGui::Text("%zu Keys · %zu Intermediates", t.textKeys.size(), t.intermediateAnimations.size());
                if ((!t.textKeys.empty() || !t.intermediateAnimations.empty()) && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    for (const auto& k : t.textKeys) ImGui::Text("%s -> %s", k.source.c_str(), k.destination.c_str());
                    for (const auto& m : t.intermediateAnimations) ImGui::Text("%d / %.6g", m.eventCode, m.value);
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndTable();
        }
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
            previewTime_ += ImGui::GetIO().DeltaTime * previewSpeed_;
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

        ImGui::TextDisabled("%s",
            L("Diese Timeline sampelt echte KF-Transforms. Skelett-/Mesh-Playback folgt erst, "
              "wenn komprimierte Fiesta-B-Splines und Bone-Hierarchie verifiziert sind.",
              "This timeline samples real KF transforms. Skeleton/mesh playback follows only "
              "after compressed Fiesta B-splines and bone hierarchy are verified."));
    }
    if (!previewMessage_.empty()) {
        ImGui::TextWrapped("%s", previewMessage_.c_str());
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();
}
}
