#pragma once
#include "formats/archive.h"
#include "native/project_binding.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace studio {
// USUM 1/5/9: members 0 (gifts, 0x14), 1 (statics, 0x38), 4 (trades, 0x34).
// Raw entries are retained; only explicitly edited bytes/bits are changed.
class EncounterDataViewer {
    struct Group {int member; int stride; const char* title;};
    static constexpr Group groups_[3]={{0,0x14,"Gifts"},{1,0x38,"Static Encounters"},{4,0x34,"Trades"}};
    static unsigned u16(const Bytes& b,int x){return unsigned(b[x])|(unsigned(b[x+1])<<8);}
    static int s16(const Bytes& b,int x){return int(static_cast<std::int16_t>(u16(b,x)));}
    static void w16(Bytes& b,int x,int v){b[x]=std::uint8_t(v);b[x+1]=std::uint8_t(unsigned(v)>>8);}
    static bool number(const char* name,Bytes& b,int off,int size,int low,int high,bool sign=false){
        int v=size==2?(sign?s16(b,off):int(u16(b,off))):(sign?int(static_cast<std::int8_t>(b[off])):int(b[off]));
        int entered=v; ImGui::SetNextItemWidth(150);bool edited=ImGui::InputInt(name,&entered,0,0);
        if(edited){entered=std::clamp(entered,low,high);if(size==2)w16(b,off,entered);else b[off]=std::uint8_t(entered);}
        return edited;
    }
    static bool bit(const char* name,Bytes& b,int off,int mask){bool v=(b[off]&mask)!=0;bool old=v;if(ImGui::Checkbox(name,&v)) {if(v)b[off]|=std::uint8_t(mask);else b[off]&=std::uint8_t(~mask);return v!=old;}return false;}
    static bool field(const char* label,Bytes& b,int off,int bytes,int lo,int hi,bool sign=false){ImGui::PushID(off);bool r=number(label,b,off,bytes,lo,hi,sign);ImGui::PopID();return r;}
    static const char* category(int kind,int id,int species,bool locked){
        if(kind==0){if(id<3)return studio::localization::bilingual("Starters", "Starters");if(id<=13)return studio::localization::bilingual("Fossils", "Fossiles");if(id>=22&&id<=32)return "Recompenses / Dominants";if(id>=14&&id<=21||id==33||id==34)return "Cadeaux / scenario";return "Autres Gifts";}
        if(kind==2)return studio::localization::bilingual("Trades", "Echanges");
        if(id>=100&&id<=128)return "Island Scan";
        if((id>=229&&id<=251))return "Quetes / evenements";
        if(id>=150&&id<=228){
            constexpr int red[]={384,641,642,645,488,717,144,145,146,888,889,250,890};
            constexpr int yellow[]={896,897,898,484,485,486,487,1014,1015,1016,383};
            constexpr int green[]={640,483,644,643,905,716,243,244,150,638,639};
            constexpr int blue[]={480,481,482,646,1001,1002,1003,1004,245,249,380,381,382};
            constexpr int beasts[]={800,805,806,793,794,795,796,797,798,799};
            auto contains=[species](auto& arr){for(int x:arr)if(x==species)return true;return false;};
            if(contains(red))return studio::localization::bilingual("Red Ultra Wormhole", "Ultra-Breche rouge");if(contains(yellow))return studio::localization::bilingual("Yellow Ultra Wormhole", "Ultra-Breche jaune");
            if(contains(green))return studio::localization::bilingual("Green Ultra Wormhole", "Ultra-Breche verte");if(contains(blue))return studio::localization::bilingual("Blue Ultra Wormhole", "Ultra-Breche bleue");
            if(contains(beasts))return studio::localization::bilingual("Ultra Beasts", "Ultra-Chimeres");
        }
        return locked?"Autres Static (Shiny Lock)":"Autres Static";
    }
    static const char* island_label(int id){
        const char* days[]={studio::localization::bilingual("Sunday", "Dimanche"),studio::localization::bilingual("Monday", "Lundi"),studio::localization::bilingual("Tuesday", "Mardi"),studio::localization::bilingual("Wednesday", "Mercredi"),studio::localization::bilingual("Thursday", "Jeudi"),studio::localization::bilingual("Friday", "Vendredi"),studio::localization::bilingual("Saturday", "Samedi")};
        if(id<100||id>128)return "";
        int x=id-100;int island=x<7?0:x<14?1:x<22?2:3;
        const char* place[]={"Melemele","Akala","Ula-Ula","Poni"};
        int weekday=x<7?x:x<14?x-7:x<22?(x==118?3:x<118?x-114:x-115):x-122;
        static std::string label;label=std::string(place[island])+" - "+days[std::clamp(weekday,0,6)];return label.c_str();
    }
    Bytes& data(int group){return buffers_[group];}
    const Bytes& data(int group)const{return buffers_[group];}
    bool valid(int g)const{return loaded_&&g>=0&&g<3&&data(g).size()%groups_[g].stride==0;}
    int count(int g)const{return valid(g)?int(data(g).size()/groups_[g].stride):0;}
    Bytes row(int g,int i)const{auto &b=data(g);auto st=std::size_t(i)*groups_[g].stride;return Bytes(b.begin()+st,b.begin()+st+groups_[g].stride);}
    void commit(int g,int i,const Bytes& b){auto &out=data(g);auto st=std::size_t(i)*groups_[g].stride;std::copy(b.begin(),b.end(),out.begin()+st);dirty_[g]=(out!=original_[g]);}
    void open(const std::filesystem::path& path){
        path_=path;loaded_=false;status_.clear();dirty_.fill(false);
        try{
            archive_=std::make_unique<Archive>(path);
            if(archive_->size()<=4)throw std::runtime_error("L'archive doit avoir au moins 5 membres.");
            for(int g=0;g<3;++g){buffers_[g]=archive_->decoded(groups_[g].member);original_[g]=buffers_[g];
                if(buffers_[g].size()%groups_[g].stride)throw std::runtime_error(std::string("Taille incompatible pour ")+groups_[g].title);
                if(archive_->raw(groups_[g].member)!=buffers_[g])throw std::runtime_error(studio::localization::bilingual("Writing compressed archive is not supported.", "Archive compressee non prise en charge en ecriture."));
            }
            loaded_=true;selected_=std::clamp(selected_,0,std::max(0,count(mode_)-1));
        }catch(const std::exception& e){status_=std::string("Erreur lecture: ")+e.what();archive_.reset();}
    }
    void save(){
        if(!loaded_||!archive_)return;
        auto *proj=project_store();if(!proj){status_=studio::localization::bilingual("Open a USUM Studio project.", "Ouvre un projet USUM Studio.");return;}
        try{
            std::map<std::size_t,Bytes> changed;
            for(int g=0;g<3;++g)if(dirty_[g])changed[std::size_t(groups_[g].member)]=buffers_[g];
            if(changed.empty()){status_=studio::localization::bilingual("No changes.", "Aucune modification.");return;}
            const auto out=proj->root/"scratch"/"encounter-data-editor.garc";
            std::filesystem::create_directories(out.parent_path());archive_->export_to(out,changed,true);
            Archive verify(out);for(auto &entry:changed)if(verify.decoded(entry.first)!=entry.second)throw std::runtime_error("La relecture de l'archive ne correspond pas.");
            proj->import_file(std::filesystem::path("romfs/a/1/5/9"),out,false);
            open(proj->source/"romfs/a/1/5/9");status_=studio::localization::bilingual("Encounter Data integrated into project. Use standard export.", "Encounter Data integrees au projet. Utilise l'export classique.");
        }catch(const std::exception& e){status_=std::string("Export refuse: ")+e.what();}
    }
    void preset(){
        static constexpr int vals[][4]={
            {100,151,0,12},{101,251,0,12},{102,385,0,14},{103,492,0,18},{104,494,0,9},{105,647,0,10},{106,648,0,9},
            {107,386,0,17},{108,491,0,19},{109,649,0,20},{110,808,0,24},{111,670,5,14},{112,893,0,16},{113,893,1,17},
            {114,721,0,32},{115,720,0,33},{116,493,0,35},{117,798,0,36},{118,798,0,36},{119,802,0,29},{120,807,0,29},{121,1025,0,30},
            {122,144,1,55},{123,145,1,57},{124,146,1,61},{125,1017,0,57},{126,1024,0,45},{127,891,0,44},{128,794,0,44}
        };
        if(count(1)<=128){status_=studio::localization::bilingual("Preset refused: fewer than 129 Static Encounters.", "Preset refuse : moins de 129 Static Encounters.");return;}
        for(auto &v:vals){const int at=v[0]*groups_[1].stride;w16(data(1),at,v[1]);data(1)[at+2]=std::uint8_t(v[2]);data(1)[at+3]=std::uint8_t(v[3]);}
        dirty_[1]=(data(1)!=original_[1]);status_="Preset Island Scan applique en memoire (100..128, y compris 117 et 118).";
    }
    void unlock(){int changed=0;for(int g=0;g<2;++g){int n=count(g);for(int i=0;i<n;++i){auto &b=data(g);int at=i*groups_[g].stride+(g==0?4:6);if(b[at]&2){b[at]&=std::uint8_t(~2);++changed;}}dirty_[g]=(data(g)!=original_[g]);}status_=std::to_string(changed)+" Shiny Locks retires en memoire.";}
public:
    void draw(const std::filesystem::path& dump,bool active){
        if(!active)return;
        if(!ImGui::Begin(studio::localization::tr("Encounter Data"),nullptr,ImGuiWindowFlags_NoCollapse)){ImGui::End();return;}
        auto root=project_store()?project_store()->source:dump;auto want=root/"romfs/a/1/5/9";
        if(!loaded_||want!=path_){if(!(dirty_[0]||dirty_[1]||dirty_[2]))open(want);}
        if(ImGui::Button(studio::localization::bilingual("Load / Refresh", "Charger / Actualiser"))){
            if(dirty_[0]||dirty_[1]||dirty_[2])ImGui::OpenPopup(studio::localization::bilingual("Discard changes?", "Abandonner les modifications ?"));else open(want);
        }
        if(ImGui::BeginPopupModal(studio::localization::bilingual("Discard changes?", "Abandonner les modifications ?"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped(studio::localization::bilingual("In-memory changes will be lost.", "Les modifications en memoire seront perdues."));
            if(ImGui::Button(studio::localization::bilingual("Yes, reload", "Oui, recharger"))){open(want);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(studio::localization::bilingual("Cancel", "Annuler")))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
        ImGui::SameLine();if(ImGui::Button(studio::localization::bilingual("Integrate into project", "Integrer au projet")))save();
        ImGui::SameLine();ImGui::TextDisabled("romfs/a/1/5/9 | conservation des octets inconnus");
        if(!status_.empty())ImGui::TextWrapped("%s",status_.c_str());
        if(!loaded_){ImGui::TextDisabled(studio::localization::bilingual("Archive unavailable.", "Archive indisponible."));ImGui::End();return;}
        ImGui::Text("%d Gifts | %d Static Encounters | %d Trades",count(0),count(1),count(2));
        if(ImGui::Button(studio::localization::bilingual("Apply MY Island Scan preset", "Appliquer MON preset Island Scan")))ImGui::OpenPopup(studio::localization::bilingual("Confirm Island Scan preset", "Confirmer preset Island Scan"));
        ImGui::SameLine();if(ImGui::Button(studio::localization::bilingual("Remove ALL Shiny Locks", "Retirer TOUS les Shiny Locks")))ImGui::OpenPopup(studio::localization::bilingual("Confirm Shiny Lock removal", "Confirmer retrait Shiny Locks"));
        if(ImGui::BeginPopupModal(studio::localization::bilingual("Confirm Island Scan preset", "Confirmer preset Island Scan"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped(studio::localization::bilingual("Replace species, form and level for entries 100 to 128 in memory?", "Remplacer en memoire espece, forme et niveau des entrees 100 a 128 ?"));
            if(ImGui::Button(studio::localization::bilingual("Apply", "Appliquer"))){preset();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(studio::localization::bilingual("Cancel", "Annuler")))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
        if(ImGui::BeginPopupModal(studio::localization::bilingual("Confirm Shiny Lock removal", "Confirmer retrait Shiny Locks"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextWrapped(studio::localization::bilingual("Remove all Shiny Locks from Gifts and Static Encounters?", "Retirer tous les Shiny Locks des Gifts et Static Encounters ?"));
            if(ImGui::Button(studio::localization::bilingual("Remove", "Retirer"))){unlock();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button(studio::localization::bilingual("Cancel", "Annuler")))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
        selected_=std::clamp(selected_,0,std::max(0,count(mode_)-1));
        if(count(mode_)>0){
            ImGui::Separator();ImGui::Text("%s #%d",groups_[mode_].title,selected_);
            Bytes b=row(mode_,selected_);bool edit=false;
            edit|=field(studio::localization::bilingual("Species", "Espece"),b,0,2,0,65535);
            edit|=field(studio::localization::bilingual("Form", "Forme"),b,mode_==2?4:2,1,0,255);
            edit|=field(studio::localization::bilingual("Level", "Niveau"),b,mode_==2?5:3,1,0,100);
            if(mode_==0){
                edit|=field("Talent (brut)",b,6,1,-128,127,true);
                edit|=field("Nature (brut)",b,7,1,-128,127,true);
                edit|=field(studio::localization::bilingual("Held item", "Objet tenu"),b,8,2,0,65535);
                edit|=bit("Shiny Lock",b,4,2);
                bool egg=b[10]==1;if(ImGui::Checkbox(studio::localization::bilingual("Egg", "Oeuf"),&egg)){b[10]=egg?1:0;edit=true;}
                edit|=field(studio::localization::bilingual("Special move", "Capacite speciale"),b,12,2,0,65535);
            }else if(mode_==1){
                int held=s16(b,4);if(held<0)held=0;
                if(ImGui::InputInt("Objet tenu (0 = aucun)",&held)){held=std::clamp(held,0,32767);w16(b,4,held==0?-1:held);edit=true;}
                edit|=bit("Shiny Lock",b,6,2);
                int gender=(b[6]>>2)&3;if(ImGui::InputInt("Sexe (brut 0..3)",&gender)){gender=std::clamp(gender,0,3);b[6]=std::uint8_t((b[6]&~0xC)|(gender<<2));edit=true;}
                int ability=(b[6]>>4)&7;if(ImGui::InputInt("Talent (brut 0..7)",&ability)){ability=std::clamp(ability,0,7);b[6]=std::uint8_t((b[6]&~0x70)|(ability<<4));edit=true;}
                int map=s16(b,8)-1;if(ImGui::InputInt("Map ID (-1 = aucun)",&map)){map=std::clamp(map,-1,32766);w16(b,8,map+1);edit=true;}
                for(int n=0;n<4;++n){ImGui::PushID(n+500);edit|=field("Capacite (ID)",b,12+n*2,2,0,65535);ImGui::PopID();}
                edit|=field("Nature (brut)",b,0x14,1,0,255);
                edit|=field("Aura",b,0x25,1,0,255);
                if(ImGui::CollapsingHeader("Parametres avances pk3DS (IV / EV / SOS)")){
                    for(int n=0;n<6;++n){ImGui::PushID(n+600);edit|=field("IV signe (-128..127)",b,0x15+n,1,-128,127,true);edit|=field("EV (0..255)",b,0x1B+n,1,0,255);ImGui::PopID();}
                    edit|=field("Allies (brut)",b,0x27,1,0,255);edit|=field("Allie 1 (brut)",b,0x28,1,0,255);edit|=field("Allie 2 (brut)",b,0x2C,1,0,255);
                }
            }else{
                edit|=field("Pokemon demande",b,0x2C,2,0,65535);
                edit|=field(studio::localization::bilingual("Ability", "Talent"),b,0xC,1,0,255);edit|=field(studio::localization::bilingual("Nature", "Nature"),b,0xD,1,0,255);
                edit|=field(studio::localization::bilingual("Gender", "Sexe"),b,0xE,1,0,255);edit|=field("TID",b,0x10,2,0,65535);
                edit|=field("SID",b,0x12,2,0,65535);
                int item=s16(b,0x14);if(item<0)item=0;
                if(ImGui::InputInt("Objet tenu (0 = aucun)",&item)){item=std::clamp(item,0,32767);w16(b,0x14,item?item:-1);edit=true;}
                if(ImGui::CollapsingHeader(studio::localization::bilingual("Advanced pk3DS fields", "Champs avances pk3DS"))){
                    edit|=field("String 1",b,2,2,0,65535);edit|=field("String 2",b,0x18,2,0,65535);
                    edit|=field("Genre DO",b,0x1A,1,0,255);
                    for(int n=0;n<6;++n){ImGui::PushID(n+700);edit|=field("IV signe",b,6+n,1,-128,127,true);ImGui::PopID();}
                    edit|=field("DO Intensite",b,0x1C,2,0,65535);edit|=field("DO Memoire",b,0x1E,2,0,65535);
                    edit|=field("DO Variable texte",b,0x20,2,0,65535);edit|=field("DO Ressenti",b,0x22,2,0,65535);
                }
            }
            if(edit){commit(mode_,selected_,b);if(mode_==1&&selected_==117&&count(1)>118){auto mirror=row(1,118);w16(mirror,0,u16(b,0));mirror[2]=b[2];mirror[3]=b[3];commit(1,118,mirror);}}
            if(ImGui::Button(studio::localization::bilingual("Discard changes for this entry", "Annuler modifications de cette entree"))){auto off=std::size_t(selected_)*groups_[mode_].stride;Bytes old(original_[mode_].begin()+off,original_[mode_].begin()+off+groups_[mode_].stride);commit(mode_,selected_,old);}
        }
        ImGui::End();
    }
    void draw_browser(){
        const char* modes[]={"Gifts (0.bin)","Static Encounters (1.bin)","Trades (4.bin)"};
        ImGui::SetNextItemWidth(-1);if(ImGui::Combo("##enc-type",&mode_,modes,3))selected_=0;
        if(!loaded_){ImGui::TextDisabled(studio::localization::bilingual("No archive loaded.", "Aucune archive chargee."));return;}
        ImGui::TextDisabled("%d entrees",count(mode_));
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##enc-search","Recherche ID / espece",search_,sizeof(search_));
        if(mode_==1){const char* cats[]={studio::localization::bilingual("All", "Tous"),"Island Scan","Quetes / evenements",studio::localization::bilingual("Red Ultra Wormhole", "Ultra-Breche rouge"),studio::localization::bilingual("Yellow Ultra Wormhole", "Ultra-Breche jaune"),studio::localization::bilingual("Green Ultra Wormhole", "Ultra-Breche verte"),studio::localization::bilingual("Blue Ultra Wormhole", "Ultra-Breche bleue"),studio::localization::bilingual("Ultra Beasts", "Ultra-Chimeres"),"Autres Static","Shiny Lock uniquement"};ImGui::SetNextItemWidth(-1);ImGui::Combo("##enc-cat",&filter_,cats,10);}
        else if(mode_==0){const char* cats[]={studio::localization::bilingual("All", "Tous"),studio::localization::bilingual("Starters", "Starters"),studio::localization::bilingual("Fossils", "Fossiles"),"Cadeaux / scenario","Recompenses / Dominants","Autres Gifts"};ImGui::SetNextItemWidth(-1);ImGui::Combo("##enc-cat",&filter_,cats,6);}
        ImGui::Separator();ImGui::BeginChild("##enc-items",ImVec2(0,0));
        for(int i=0;i<count(mode_);++i){auto b=row(mode_,i);int species=int(u16(b,0));bool lock=mode_!=2&&(b[mode_==0?4:6]&2);
            auto cat=category(mode_,i,species,lock);
            if(mode_==1&&filter_>0){const char* cats[]={studio::localization::bilingual("All", "Tous"),"Island Scan","Quetes / evenements",studio::localization::bilingual("Red Ultra Wormhole", "Ultra-Breche rouge"),studio::localization::bilingual("Yellow Ultra Wormhole", "Ultra-Breche jaune"),studio::localization::bilingual("Green Ultra Wormhole", "Ultra-Breche verte"),studio::localization::bilingual("Blue Ultra Wormhole", "Ultra-Breche bleue"),studio::localization::bilingual("Ultra Beasts", "Ultra-Chimeres"),"Autres Static"};if(filter_==9&&!lock)continue;if(filter_<9&&std::string(cat)!=cats[filter_])continue;}
            if(mode_==0&&filter_>0){const char* cats[]={studio::localization::bilingual("All", "Tous"),studio::localization::bilingual("Starters", "Starters"),studio::localization::bilingual("Fossils", "Fossiles"),"Cadeaux / scenario","Recompenses / Dominants","Autres Gifts"};if(std::string(cat)!=cats[filter_])continue;}
            std::string text=std::to_string(i)+" - Pokemon "+std::to_string(species);
            if(mode_==1&&i>=100&&i<=128)text+=" | "+std::string(island_label(i));
            if(dirty_[mode_]){auto off=std::size_t(i)*groups_[mode_].stride;if(!std::equal(b.begin(),b.end(),original_[mode_].begin()+off))text+=" *";}
            if(search_[0]&&text.find(search_)==std::string::npos)continue;
            if(ImGui::Selectable((text+"##enc"+std::to_string(i)).c_str(),selected_==i))selected_=i;
        }ImGui::EndChild();
    }
private:
    std::unique_ptr<Archive> archive_;
    std::filesystem::path path_;
    std::array<Bytes,3> buffers_{},original_{};
    std::array<bool,3> dirty_{};
    int mode_=1,selected_=0,filter_=0;char search_[128]{};bool loaded_=false;
    std::string status_;
};
}
