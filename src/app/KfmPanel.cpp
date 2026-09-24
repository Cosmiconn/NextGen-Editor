#include "KfmPanel.hpp"
#include "mapeditor/app/Localization.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

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
void KfmPanel::Draw(const std::function<std::optional<std::string>()>& browse) {
    ImGui::TextUnformatted(L("KFM-Animationskatalog","KFM animation catalog"));
    ImGui::SetNextItemWidth(std::max(160.0f,ImGui::GetContentRegionAvail().x-230.0f));
    const bool enter=ImGui::InputText("##kfm-path",path_,sizeof(path_),ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();if(ImGui::Button(L("Öffnen","Open")) || enter)Open(std::filesystem::u8path(path_));
    if(browse) { ImGui::SameLine();if(ImGui::Button(L("Durchsuchen...","Browse...")))if(auto p=browse())Open(std::filesystem::u8path(*p)); }
    if(!message_.empty())ImGui::TextWrapped("%s",message_.c_str());
    if(!file_)return;
    const auto& f=*file_;
    ImGui::Separator();
    ImGui::TextWrapped("%s: %s",L("Geladen","Loaded"),utf8(source_).c_str());
    ImGui::Text("KFM %s | %zu %s | %zu %s",core::KfmVersionName(f.version),f.animations.size(),L("Animationen","animations"),transitionCount_,L("Übergänge","transitions"));
    ImGui::TextWrapped("NIF: %s | %s: %s",f.nifFileName.c_str(),L("Wurzel","Root"),f.master.c_str());
    if(ImGui::Button(L("Dateiverweise prüfen","Check file references")))references_=core::InspectKfmReferences(f,source_);
    if(references_) {
        const auto& r=*references_;
        ImGui::TextWrapped(L("NIF: %s | KF gefunden: %zu/%zu | doppelte IDs: %zu | fehlende Übergangsziele: %zu | fehlende Zwischenanimationen: %zu",
                            "NIF: %s | KF found: %zu/%zu | duplicate IDs: %zu | missing transition targets: %zu | missing intermediate animations: %zu"),
            r.nif?L("gefunden","found"):L("nicht aufgelöst","unresolved"),r.animations.size()-r.missingKfFiles,r.animations.size(),r.duplicateEventCodes,r.missingTransitionTargets,r.missingIntermediateTargets);
    }
    ImGui::SetNextItemWidth(std::max(160.0f,ImGui::GetContentRegionAvail().x-230.0f));
    ImGui::InputText("##kfm-copy",exportPath_,sizeof(exportPath_));ImGui::SameLine();
    if(ImGui::Button(L("Kopie exportieren","Export copy"))) {
        auto saved=core::SaveKfmFile(f,std::filesystem::u8path(exportPath_));
        message_=saved?L("KFM-Kopie gespeichert.","KFM copy saved."):saved.error();
    }
    ImGui::SetNextItemWidth(360);
    if(ImGui::InputTextWithHint("##kfm-filter",L("KF-Datei, Name oder Event-ID filtern","Filter KF file, name or event ID"),filter_,sizeof(filter_)))Filter();
    const float listHeight=std::max(120.0f,ImGui::GetContentRegionAvail().y*0.48f);
    if(ImGui::BeginTable("kfm-animations",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,ImVec2(0,listHeight))) {
        ImGui::TableSetupColumn("Event ID",ImGuiTableColumnFlags_WidthFixed,90);ImGui::TableSetupColumn("Index",ImGuiTableColumnFlags_WidthFixed,55);ImGui::TableSetupColumn(L("KF-Datei","KF file"),ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(L("Übergänge","Transitions"),ImGuiTableColumnFlags_WidthFixed,100);ImGui::TableSetupColumn(L("Dateiverweis","File reference"),ImGuiTableColumnFlags_WidthFixed,130);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(visible_.size()));
        while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row) {
            const auto index=visible_[row];const auto& a=f.animations[index];ImGui::PushID(static_cast<int>(index));
            ImGui::TableNextRow();ImGui::TableNextColumn();
            if(ImGui::Selectable(std::to_string(a.eventCode).c_str(),selected_==index,ImGuiSelectableFlags_SpanAllColumns))selected_=index;
            ImGui::TableNextColumn();ImGui::Text("%d",a.index);ImGui::TableNextColumn();ImGui::TextUnformatted(a.kfFileName.c_str());
            ImGui::TableNextColumn();ImGui::Text("%zu",a.transitions.size());ImGui::TableNextColumn();
            ImGui::TextUnformatted(!references_?L("ungeprüft","unchecked"):references_->animations[index]?L("gefunden","found"):L("nicht aufgelöst","unresolved"));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if(selected_<f.animations.size()) {
        const auto& a=f.animations[selected_];
        ImGui::TextWrapped("%d | %s%s%s",a.eventCode,a.kfFileName.c_str(),a.name.empty()?"":" | ",a.name.c_str());
        if(references_ && references_->animations[selected_])ImGui::TextWrapped("%s",utf8(*references_->animations[selected_]).c_str());
        if(ImGui::BeginTable("kfm-transitions",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,ImVec2(0,std::max(70.0f,ImGui::GetContentRegionAvail().y-30.0f)))) {
            ImGui::TableSetupColumn(L("Ziel-ID","Target ID"),ImGuiTableColumnFlags_WidthFixed,90);ImGui::TableSetupColumn(L("Typ","Type"),ImGuiTableColumnFlags_WidthFixed,55);ImGui::TableSetupColumn(L("Dauer","Duration"),ImGuiTableColumnFlags_WidthFixed,100);
            ImGui::TableSetupColumn(L("Textschlüssel (Details bei Hover)","Text keys (hover for details)"));ImGui::TableSetupColumn(L("Zwischenanimationen (Details bei Hover)","Intermediate animations (hover for details)"));
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            ImGuiListClipper transitions;transitions.Begin(static_cast<int>(a.transitions.size()));
            while(transitions.Step())for(int row=transitions.DisplayStart;row<transitions.DisplayEnd;++row) {
                const auto& t=a.transitions[row];
                ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%d",t.eventCode);
                ImGui::TableNextColumn();ImGui::Text("%d",t.type);ImGui::TableNextColumn();
                if(t.type==5)ImGui::TextUnformatted("-");else ImGui::Text("%.6g",t.duration);
                ImGui::TableNextColumn();ImGui::Text("%zu",t.textKeys.size());
                if(!t.textKeys.empty() && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();for(const auto& k:t.textKeys)ImGui::Text("%s -> %s",k.source.c_str(),k.destination.c_str());ImGui::EndTooltip();
                }
                ImGui::TableNextColumn();ImGui::Text("%zu",t.intermediateAnimations.size());
                if(!t.intermediateAnimations.empty() && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();for(const auto& m:t.intermediateAnimations)ImGui::Text("%d / %.6g",m.eventCode,m.value);ImGui::EndTooltip();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::TextDisabled("%s",L("Skelettanimationen werden noch nicht abgespielt.","Skeletal animation playback is not implemented yet."));
}
}
