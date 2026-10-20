#pragma once
#include "formats/archive.h"
#include "native/project_binding.h"
#include "native/localization.h"
#include "field/map_catalog.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace studio {
// Gen 7 evolution data format as documented in pk3DS EvolutionSet7:
// eight 8-byte slots: uint16 method, uint16 argument, uint16 target species,
// int8 target form (-1 indicates default), uint8 level.
class EvolutionDataViewer {
    struct Slot { int method=0, argument=0, species=0, form=-1, level=0; };
    using Row = std::array<Slot,8>;
    static unsigned rd16(const Bytes &b, std::size_t at) {
        return unsigned(b[at]) | (unsigned(b[at+1]) << 8);
    }
    static Row decode(const Bytes &b) {
        Row result{};
        for (int i=0; i<8; ++i) {
            const int at=i*8;
            result[i].method=int(rd16(b,at));
            result[i].argument=int(rd16(b,at+2));
            result[i].species=int(rd16(b,at+4));
            result[i].form=int(static_cast<std::int8_t>(b[at+6]));
            result[i].level=int(b[at+7]);
        }
        return result;
    }
    static Bytes encode(const Row &row) {
        Bytes b(64,0);
        for (int i=0;i<8;++i) {
            const auto &v=row[i]; const int at=i*8;
            b[at]=std::uint8_t(v.method); b[at+1]=std::uint8_t(v.method>>8);
            b[at+2]=std::uint8_t(v.argument); b[at+3]=std::uint8_t(v.argument>>8);
            b[at+4]=std::uint8_t(v.species); b[at+5]=std::uint8_t(v.species>>8);
            b[at+6]=std::uint8_t(v.form); b[at+7]=std::uint8_t(v.level);
        }
        return b;
    }
    static const char *method_name(int m) {
        static constexpr const char *names[] = {
            "Aucune", "Bonheur", "Bonheur (matin)", "Bonheur (nuit)", "Niveau",
            "Echange", "Echange avec objet", "Echange special", "Objet utilise",
            "Niveau Att > Def", "Niveau Att = Def", "Niveau Att < Def", "Niveau aleatoire A",
            "Niveau aleatoire B", "Niveau Ninjask", "Niveau Munja", "Beaute",
            "Objet (male)", "Objet (femelle)", "Objet tenu (jour)", "Objet tenu (nuit)",
            "Capacite connue", "Pokemon dans equipe", "Niveau (male)", "Niveau (femelle)",
            "Lieu electrique", "Lieu foret", "Lieu froid", "3DS retournee", "Affection et type",
            "Type dans equipe", "Pluie", "Niveau (matin)", "Niveau (nuit)",
            "Niveau femelle (forme 1)", "Inutilise", "Niveau version", "Niveau jour/version",
            "Niveau nuit/version", "Sommet", "Crepuscule (USUM)", "Ultra-Breche (USUM)",
            "Objet dans Ultra-Breche (USUM)"};
        return m >= 0 && m < int(sizeof(names)/sizeof(names[0])) ? names[m] : "Methode inconnue";
    }
public:
    void draw(const std::filesystem::path &dump,bool active) {
        if (!active) return;
        if (!ImGui::Begin("Evolution Data",nullptr,ImGuiWindowFlags_NoCollapse)) { ImGui::End();return; }
        const auto root=project_store()?project_store()->source:dump;
        const auto desired=root/"romfs/a/0/1/4";
        if (!archive_ || desired!=path_) load(desired);
        if (ImGui::Button("Charger / Actualiser###evo-reload")) {
            if (!edits_.empty()) ImGui::OpenPopup("Recharger les evolutions ?###evo-confirm-reload");
            else load(desired);
        }
        if (ImGui::BeginPopupModal("Recharger les evolutions ?###evo-confirm-reload",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Les modifications locales non integrees seront perdues.");
            if (ImGui::Button("Recharger")) {load(desired);ImGui::CloseCurrentPopup();}
            ImGui::SameLine(); if(ImGui::Button("Annuler")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::SameLine(); if(ImGui::Button("Integrer les modifications au projet###evo-save")) save();
        ImGui::SameLine();ImGui::TextDisabled("a/0/1/4 | 8 emplacements par entree");
        if (!status_.empty()) ImGui::TextWrapped("%s",status_.c_str());
        if (!archive_){ImGui::TextDisabled("Archive Evolution Data indisponible."); ImGui::End();return;}
        ImGui::Text("%zu entrees | %zu entree(s) modifiee(s)",archive_->size(),edits_.size());
        if (archive_->size() && selected_ >= 0 && std::size_t(selected_)<archive_->size()) {
            ImGui::Separator(); ImGui::Text("Evolution Data #%d",selected_);
            auto original=archive_->decoded(std::size_t(selected_));
            if(original.size()!=64) {ImGui::Text("Taille inattendue : %zu octets (64 attendus)",original.size());}
            else {
                auto it=edits_.find(selected_);
                Row row=it!=edits_.end()?decode(it->second):decode(original);
                bool changed=false;
                if(ImGui::BeginTable("##evo-slots",6,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,24);
                    ImGui::TableSetupColumn("Methode",ImGuiTableColumnFlags_WidthStretch,2.0f);
                    ImGui::TableSetupColumn("Argument",ImGuiTableColumnFlags_WidthStretch,1.0f);
                    ImGui::TableSetupColumn("Espece cible",ImGuiTableColumnFlags_WidthStretch,1.0f);
                    ImGui::TableSetupColumn("Forme",ImGuiTableColumnFlags_WidthStretch,0.7f);
                    ImGui::TableSetupColumn("Niveau",ImGuiTableColumnFlags_WidthStretch,0.7f);
                    ImGui::TableHeadersRow();
                    for(int i=0;i<8;++i) {
                        ImGui::PushID(i);auto &e=row[i];ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);ImGui::Text("%d",i+1);
                        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
                        if (ImGui::BeginCombo("##method", (std::to_string(e.method)+" - "+method_name(e.method)).c_str())) {
                            for(int m=0;m<=42;++m) if(ImGui::Selectable((std::to_string(m)+" - "+method_name(m)).c_str(),e.method==m)) {e.method=m;changed=true;}
                            ImGui::EndCombo();
                        }
                        ImGui::TableSetColumnIndex(2);ImGui::SetNextItemWidth(-1);changed|=ImGui::InputInt("##arg",&e.argument,0,0);
                        ImGui::TableSetColumnIndex(3);ImGui::SetNextItemWidth(-1);changed|=ImGui::InputInt("##target",&e.species,0,0);
                        ImGui::TableSetColumnIndex(4);ImGui::SetNextItemWidth(-1);changed|=ImGui::InputInt("##form",&e.form,0,0);
                        ImGui::TableSetColumnIndex(5);ImGui::SetNextItemWidth(-1);changed|=ImGui::InputInt("##level",&e.level,0,0);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                if (changed) {
                    bool valid=true;
                    for(const auto &e:row) valid &= e.method>=0&&e.method<=65535&&e.argument>=0&&e.argument<=65535&&e.species>=0&&e.species<=65535&&e.form>=-128&&e.form<=127&&e.level>=0&&e.level<=255;
                    if(!valid) status_="Valeur hors limites : methode/argument/espece 0..65535 ; forme -128..127 ; niveau 0..255.";
                    else {auto now=encode(row); if(now==original) edits_.erase(selected_); else edits_[selected_]=std::move(now);status_.clear();}
                }
                ImGui::Separator();
                if(ImGui::Button("Reinitialiser cette entree")) edits_.erase(selected_);
                ImGui::SameLine(); ImGui::TextDisabled("La signification de l'argument depend de la methode.");
            }
        }
        ImGui::End();
    }
    void draw_browser() {
        if(ImGui::InputInt("Index###evo-id",&selected_)) clamp();
        if(!archive_) {ImGui::TextDisabled("Archive non chargee.");return;}
        ImGui::TextDisabled("%zu entrees",archive_->size());ImGui::Separator();
        ImGui::BeginChild("##evo-index-list",ImVec2(0,0),false);
        ImGuiListClipper clipper;clipper.Begin(int(archive_->size()));
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            const std::string label=std::to_string(i)+(edits_.count(i)?" *":"")+"##evo-"+std::to_string(i);
            if(ImGui::Selectable(label.c_str(),selected_==i))selected_=i;
        }
        ImGui::EndChild();
    }
private:
    void clamp() {if(archive_&&archive_->size())selected_=std::clamp(selected_,0,int(archive_->size())-1);else selected_=0;}
    void load(const std::filesystem::path &path) {
        path_=path;archive_.reset();edits_.clear();status_.clear();
        try {archive_=std::make_unique<Archive>(path);clamp();}
        catch(const std::exception &e){status_=std::string("Erreur lecture : ")+e.what();}
    }
    void save() {
        if(!archive_||edits_.empty()){status_="Aucune modification a integrer.";return;}
        auto *proj=project_store();if(!proj){status_="Ouvre un projet USUM Studio pour exporter.";return;}
        try {
            std::map<std::size_t,Bytes> replacements;
            for(const auto &[id,bytes]:edits_) {
                if(bytes.size()!=64||id<0||std::size_t(id)>=archive_->size())throw std::runtime_error("Entree invalide.");
                if(archive_->raw(std::size_t(id))!=archive_->decoded(std::size_t(id)))throw std::runtime_error("Entree compressee : export refuse.");
                replacements[std::size_t(id)]=bytes;
            }
            auto output=proj->root/"scratch"/"evolution-data-editor.garc";
            std::filesystem::create_directories(output.parent_path());
            archive_->export_to(output,replacements,true);
            // Read back every replacement before importing into the project.
            Archive check(output);
            for(const auto &[id,bytes]:replacements)if(check.decoded(id)!=bytes)throw std::runtime_error("Verification de l'export echouee.");
            proj->import_file(std::filesystem::path("romfs/a/0/1/4"),output,false);
            load(proj->source/"romfs/a/0/1/4");
            status_="Evolutions integrees au projet. Utilise ensuite l'export classique USUM Studio.";
        }catch(const std::exception &e){status_=std::string("Echec export : ")+e.what();}
    }
    std::filesystem::path path_;
    std::unique_ptr<Archive> archive_;
    int selected_=1;
    std::map<int,Bytes> edits_;
    std::string status_;
};
}
