#pragma once
#include "native/data_names.h"
#include <imgui.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
namespace studio {
class MoveDataViewer {
public:
 void draw(const std::filesystem::path& dump, bool active) {
  if(!active) return;
  if(!ImGui::Begin(localization::bilingual("Move Data###Move Data", "Données des capacités###Move Data"))) {ImGui::End();return;}
  const auto root = project_store() ? project_store()->source : dump;
  const auto path = root / "romfs/a/0/1/1";
  if(!archive_ || path_ != path) load(root);
  names_.load(root);
  if(ImGui::Button(localization::bilingual("Reload", "Actualiser"))) load(root);
  ImGui::SameLine(); ImGui::TextDisabled("a/0/1/1 | %s", localization::bilingual("Read only", "Lecture seule"));
  if(!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());
  if(archive_ && records_.empty()) ImGui::TextDisabled("%s", localization::bilingual("No validated move records. The source archive is not a standard move table.","Aucune capacité validée. L’archive source ne correspond pas à une table standard de capacités."));
  if(archive_ && !records_.empty()) {
   selected_=std::clamp(selected_,0,int(records_.size())-1);
   const auto& bytes=records_[std::size_t(selected_)];
   ImGui::SeparatorText(names_.move_label(selected_).c_str());
   ImGui::Text("ID: %d | %zu %s", selected_,bytes.size(),localization::bilingual("bytes","octets"));
   if (bytes.size() >= 0x28) {
    ImGui::SeparatorText(localization::bilingual("Move properties", "Propriétés de la capacité"));
    const auto u8 = [&](std::size_t o) -> unsigned { return unsigned(bytes[o]); };
    const auto s8 = [&](std::size_t o) -> int { return int(static_cast<std::int8_t>(bytes[o])); };
    const auto u16 = [&](std::size_t o) -> unsigned { return u8(o) | (u8(o+1)<<8); };
    const auto u32 = [&](std::size_t o) -> std::uint32_t { return std::uint32_t(u16(o)) | (std::uint32_t(u16(o+2))<<16); };
    static constexpr const char* types_en[] = {"Normal","Fighting","Flying","Poison","Ground","Rock","Bug","Ghost","Steel","Fire","Water","Grass","Electric","Psychic","Ice","Dragon","Dark","Fairy"};
    static constexpr const char* types_fr[] = {"Normal","Combat","Vol","Poison","Sol","Roche","Insecte","Spectre","Acier","Feu","Eau","Plante","Électrik","Psy","Glace","Dragon","Ténèbres","Fée"};
    static constexpr const char* category_en[] = {"Status","Physical","Special"};
    static constexpr const char* category_fr[] = {"Statut","Physique","Spéciale"};
    const bool fr = localization::current == localization::Language::French;
    const auto lbl = [&](const char* en,const char* french)->const char* {return localization::bilingual(en,french);};
    auto type_label = [&](unsigned id)->std::string { if(id<18) return fr?types_fr[id]:types_en[id]; return std::string("#")+std::to_string(id); };
    auto cat_label = [&](unsigned id)->std::string { if(id<3) return fr?category_fr[id]:category_en[id]; return std::string("#")+std::to_string(id); };
    if(ImGui::BeginTable("##move-properties",2,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)) {
     ImGui::TableSetupColumn(lbl("Property","Propriété")); ImGui::TableSetupColumn(lbl("Value","Valeur")); ImGui::TableHeadersRow();
     auto row = [&](const char* name, const std::string& value) { ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name); ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(value.c_str()); };
     auto number = [&](const char* en,const char* french,int value){row(lbl(en,french),std::to_string(value));};
     row(lbl("Type","Type"),type_label(u8(0x00)));
     row(lbl("Damage category","Catégorie"),cat_label(u8(0x02)));
     number("Move quality","Nature de l'effet",int(u8(0x01)));
     number("Power","Puissance",int(u8(0x03)));
     number("Accuracy","Précision",int(u8(0x04)));
     number("PP","PP",int(u8(0x05)));
     number("Priority","Priorité",s8(0x06));
     number("Minimum hits","Coups minimum",int(u8(0x07)&15));
     number("Maximum hits","Coups maximum",int(u8(0x07)>>4));
     number("Status infliction ID","ID de statut infligé",int(u16(0x08)));
     number("Status chance (%)","Chance de statut (%)",int(u8(0x0A)));
     number("Infliction duration","Durée d'altération",int(u8(0x0B)));
     number("Minimum turns","Tours minimum",int(u8(0x0C)));
     number("Maximum turns","Tours maximum",int(u8(0x0D)));
     number("Critical stage","Niveau critique",int(u8(0x0E)));
     number("Flinch chance","Chance d'apeurement",int(u8(0x0F)));
     number("Effect sequence ID","ID séquence d'effet",int(u16(0x10)));
     number("Recoil","Contrecoup",s8(0x12));
     number("Healing","Soin",int(u8(0x13)));
     number("Target ID","ID de cible",int(u8(0x14)));
     for(int i=0;i<3;++i){const auto j=std::size_t(i);char label[80];std::snprintf(label,sizeof(label),"%s %d",lbl("Stat modifier","Modificateur de stat"),i+1);row(label,std::to_string(u8(0x15+j))+" / "+std::to_string(s8(0x18+j))+" / "+std::to_string(u8(0x1B+j))+"%");}
     number("Z-Move ID","ID de capacité Z",int(u16(0x1E)));
     number("Z-Power","Puissance Z",int(u8(0x20)));
     number("Z-Effect ID","ID d'effet Z",int(u8(0x21)));
     number("Refresh affliction type","Type d'affliction Poké-Détente",int(u8(0x22)));
     number("Refresh affliction chance","Chance d'affliction Poké-Détente",int(u8(0x23)));
     char flags[32];std::snprintf(flags,sizeof(flags),"0x%08X",unsigned(u32(0x24))); row(lbl("Move flags (bitmask)","Drapeaux (masque binaire)"),flags);
     ImGui::EndTable();
    }
    ImGui::TextDisabled("%s",lbl("Numeric IDs are kept where effect/target names are not yet mapped.","Les ID numériques restent visibles si leur nom n'est pas encore associé."));
   } else {
    ImGui::TextDisabled("%s",localization::bilingual("Invalid Gen 7 move record: expected at least 40 bytes.","Enregistrement de capacité Gen 7 invalide : au moins 40 octets attendus."));
   }
   ImGui::SeparatorText(localization::bilingual("Hexadecimal record", "Enregistrement hexadécimal"));
   for(std::size_t off=0;off<bytes.size();off+=16){
    char line[120];int n=std::snprintf(line,sizeof(line),"%04zX: ",off);
    for(std::size_t j=off;j<std::min(bytes.size(),off+16);++j) n+=std::snprintf(line+n,sizeof(line)-std::size_t(n),"%02X ",unsigned(bytes[j]));
    ImGui::TextUnformatted(line);
   }
  }
  ImGui::End();
 }
 void draw_browser(){
  if(!archive_ || records_.empty()){ImGui::TextDisabled("%s",localization::bilingual("No move archive loaded.","Archive des capacités non chargée."));return;}
  ImGui::Text("%zu %s",records_.size(),localization::bilingual("moves","capacités"));
  ImGui::BeginChild("##move-browser",ImVec2(0,0));
  ImGuiListClipper clipper;clipper.Begin(int(records_.size()));
  while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
   const std::string label=std::to_string(i)+" - "+names_.move_label(i)+"###move-"+std::to_string(i);
   if(ImGui::Selectable(label.c_str(),selected_==i))selected_=i;
  }
  ImGui::EndChild();
 }
private:
 // Gen 7 Move Data is a WD mini-container inside the first GARC member.
 // The WD header stores a 16-bit count followed by count+1 LE32 offsets.
 static std::uint32_t read_le32(const std::vector<std::uint8_t>& b, std::size_t p) {
  return std::uint32_t(b[p]) | (std::uint32_t(b[p+1])<<8) |
         (std::uint32_t(b[p+2])<<16) | (std::uint32_t(b[p+3])<<24);
 }
 static bool unpack_wd(const std::vector<std::uint8_t>& b,
                       std::vector<std::vector<std::uint8_t>>& output,
                       std::string& error) {
  if (b.size()<8 || b[0]!='W' || b[1]!='D') {
   error=localization::bilingual("Move GARC member is not a WD container.","Le membre GARC des capacités n'est pas un conteneur WD.");return false;
  }
  const std::size_t count=std::size_t(b[2]) | (std::size_t(b[3])<<8);
  if (count==0 || count>4096 || 4+(count+1)*4>b.size()) {
   error=localization::bilingual("Invalid WD move count or offset table.","Nombre de capacités ou table d'offsets WD invalide.");return false;
  }
  std::vector<std::vector<std::uint8_t>> parsed;
  parsed.reserve(count);
  for (std::size_t i=0;i<count;++i) {
   const std::size_t start=read_le32(b,4+i*4),end=read_le32(b,8+i*4);
   if (start<4+(count+1)*4 || end<start || end>b.size() || end-start<0x28 || end-start>0x2B) {
    error=localization::bilingual("Invalid WD record offsets or Gen 7 move size.","Offsets WD ou taille d'une capacité Gen 7 invalides.");return false;
   }
   // Each logical record is 0x28 bytes; WD can pad to a 4-byte boundary.
   parsed.emplace_back(b.begin()+std::ptrdiff_t(start),b.begin()+std::ptrdiff_t(start+0x28));
  }
  output=std::move(parsed);
  return true;
 }
 void load(const std::filesystem::path& root){
  path_=root/"romfs/a/0/1/1";archive_.reset();records_.clear();error_.clear();selected_=1;
  try {
   archive_=std::make_unique<Archive>(path_);
   if (archive_->size()==0) {
    error_=localization::bilingual("Empty Move Data GARC.","Archive GARC des capacités vide.");return;
   }
   auto b=archive_->decoded(0);
   if (!unpack_wd(b,records_,error_)) return;
   selected_=records_.size()>1?1:0;

  }catch(const std::exception&e){error_=e.what();}
 }
 std::filesystem::path path_;std::unique_ptr<Archive> archive_;DataNames names_;std::string error_;int selected_=1;
 std::vector<std::vector<std::uint8_t>> records_;
};
}
