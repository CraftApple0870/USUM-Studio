#pragma once
#include "native/data_names.h"
#include <imgui.h>
namespace studio {
class LearnsetDataViewer {
public:
 void draw(const std::filesystem::path& dump,bool active){
  if(!active)return;
  if(!ImGui::Begin(localization::bilingual("Learnset Data###Learnset Data","Capacités apprises###Learnset Data"))){ImGui::End();return;}
  const auto root=project_store()?project_store()->source:dump;
  const auto path=root/"romfs/a/0/1/3";
  if(!archive_||path_!=path)load(root);
  names_.load(root);
  if(ImGui::Button(localization::bilingual("Reload","Actualiser")))load(root);
  ImGui::SameLine();ImGui::TextDisabled("a/0/1/3 | %s",localization::bilingual("Read only","Lecture seule"));
  if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
  if(archive_&&archive_->size()){
   selected_=std::clamp(selected_,0,int(archive_->size())-1);
   auto b=archive_->decoded(std::size_t(selected_));
   ImGui::SeparatorText(names_.personal_label(selected_).c_str());
   ImGui::Text("ID: %d | %zu %s",selected_,b.size(),localization::bilingual("bytes","octets"));
   if(b.size()%4){ImGui::TextDisabled("%s",localization::bilingual("Unexpected learnset record size.","Taille de learnset inattendue."));}
   else {
    ImGui::TextDisabled("%s",localization::bilingual("Level-up moves (USUM 16-bit pairs)","Capacités apprises par niveau (paires 16 bits USUM)"));
    if(ImGui::BeginTable("##learnset",2,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)){
     ImGui::TableSetupColumn(localization::bilingual("Level","Niveau"));ImGui::TableSetupColumn(localization::bilingual("Move","Capacité"));ImGui::TableHeadersRow();
     for(std::size_t i=0;i+3<b.size();i+=4){
      unsigned move=unsigned(b[i])|(unsigned(b[i+1])<<8);
      unsigned level=unsigned(b[i+2])|(unsigned(b[i+3])<<8);
      if(move==0xFFFFu||level==0xFFFFu)break;
      ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::Text("%u",level);
      ImGui::TableSetColumnIndex(1);ImGui::Text("%u - %s",move,names_.move_label(int(move)).c_str());
     }
     ImGui::EndTable();
    }
   }
  }
  ImGui::End();
 }
 void draw_browser(){
  if(!archive_){ImGui::TextDisabled("%s",localization::bilingual("No learnset archive loaded.","Archive des capacités apprises non chargée."));return;}
  ImGui::Text("%zu %s",archive_->size(),localization::bilingual("entries","entrées"));
  ImGui::BeginChild("##learnset-browser",ImVec2(0,0));ImGuiListClipper clipper;clipper.Begin(int(archive_->size()));
  while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
   auto label=std::to_string(i)+" - "+names_.personal_label(i)+"###learnset-"+std::to_string(i);
   if(ImGui::Selectable(label.c_str(),selected_==i))selected_=i;
  }
  ImGui::EndChild();
 }
private:
 void load(const std::filesystem::path& root){path_=root/"romfs/a/0/1/3";archive_.reset();error_.clear();try{archive_=std::make_unique<Archive>(path_);}catch(const std::exception&e){error_=e.what();}}
 std::filesystem::path path_;std::unique_ptr<Archive> archive_;DataNames names_;std::string error_;int selected_=1;
};
}
