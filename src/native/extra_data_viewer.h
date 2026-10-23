#pragma once
// Experimental, strictly read-only additional Data inspectors for USUM.
// Unknown binary layouts are never misrepresented as decoded game data.
#include "formats/archive.h"
#include "native/data_names.h"
#include "native/localization.h"
#include "native/project_binding.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace studio {
class ExtraDataViewer {
public:
    enum class Kind { EggMoves, Mega, TrainerClasses, TMTutor, Shops };
    explicit ExtraDataViewer(Kind kind) : kind_(kind) {
        const char* path = kind == Kind::EggMoves ? "romfs/a/0/1/2" :
                           kind == Kind::Mega ? "romfs/a/0/1/5" :
                           kind == Kind::TrainerClasses ? "romfs/a/1/0/5" :
                           kind == Kind::TMTutor ? "romfs/a/0/1/7" :
                           "romfs/Shop.cro";
        std::snprintf(relative_, sizeof(relative_), "%s", path);
    }
    void draw(const std::filesystem::path& dump, bool active) {
        if (!active) return;
        const auto root = project_store() ? project_store()->source : dump;
        root_ = root;
        const auto next = resolved_path(root);
        if (next != path_ || (!archive_ && !file_mode_ && error_.empty())) load(next);
        auto window = std::string(title_en()) + "###extra-data-" + std::to_string(int(kind_));
        if (!ImGui::Begin(window.c_str())) { ImGui::End(); return; }
        ImGui::TextDisabled("%s", localization::bilingual("Read-only inspector", "Inspecteur en lecture seule"));
        ImGui::TextWrapped("%s", instruction());
        ImGui::TextDisabled("%s: %s", localization::bilingual("Resolved source", "Source trouvee"), path_.u8string().empty() ? "-" : path_.string().c_str());
        ImGui::SetNextItemWidth(-100.f);
        if (ImGui::InputText("##archive-path", relative_, sizeof(relative_))) {
            // Loading follows on the next frame; never writes to the project.
        }
        ImGui::SameLine();
        if (ImGui::Button(localization::bilingual("Reload", "Actualiser"))) load(resolved_path(root));
        if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        if (file_mode_) {
            if (ImGui::BeginTabBar("##shop-tabs")) {
                if (ImGui::BeginTabItem(localization::bilingual("Information", "Informations"))) {
                    ImGui::TextWrapped("%s", localization::bilingual("Raw CRO file. Shop inventory offsets are not verified; editing and fabricated item names are disabled.", "Fichier CRO brut. Les positions des boutiques ne sont pas validees ; aucune modification ni aucun faux nom d'objet."));
                    ImGui::Text("%zu bytes", file_bytes_.size()); ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(localization::bilingual("Hexadecimal", "Hexadecimal"))) {
                    draw_hex(file_bytes_); ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End(); return;
        }
        if (!archive_ || archive_->size() == 0) { ImGui::End(); return; }
        names_.load(root);
        selected_ = std::clamp(selected_, 0, int(archive_->size())-1);
        ImGui::Text("%zu %s", archive_->size(), localization::bilingual("records", "enregistrements"));
        try {
            const auto bytes = archive_->decoded(std::size_t(selected_));
            ImGui::Text("ID: %d | %zu %s", selected_, bytes.size(), localization::bilingual("bytes", "octets"));
            if (ImGui::BeginTabBar("##extra-data-tabs")) {
                if (ImGui::BeginTabItem(localization::bilingual("Decoded", "Decode"))) {
                    draw_decoded(bytes);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(localization::bilingual("Hexadecimal", "Hexadecimal"))) {
                    draw_hex(bytes);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        } catch (const std::exception& e) { ImGui::TextWrapped("%s",e.what()); }
        ImGui::End();
    }
    void draw_browser(const std::filesystem::path& dump) {
        const auto root = project_store() ? project_store()->source : dump;
        root_ = root;
        const auto p = resolved_path(root);
        if (p != path_ || (!archive_ && !file_mode_ && error_.empty())) load(p);
        if (file_mode_) { ImGui::TextWrapped("%s", localization::bilingual("CRO file loaded; inspect its bytes in the main Shop window.", "Fichier CRO charge ; consultez ses octets dans la fenetre Boutiques.")); return; }
        if (!archive_ || archive_->size()==0) {
            ImGui::TextWrapped("%s", localization::bilingual("Choose the source archive in the reader window. No data is modified.",
                "Choisissez l'archive source dans la fenetre du lecteur. Aucune donnee n'est modifiee."));
            return;
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("ID##extra-selected", &selected_)) selected_ = std::clamp(selected_,0,int(archive_->size())-1);
        ImGui::BeginChild("##extra-browser", ImVec2(0,0));
        ImGuiListClipper clipper;
        clipper.Begin(int(archive_->size()));
        while (clipper.Step()) for (int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            std::string label=std::to_string(i);
            if (kind_==Kind::EggMoves || kind_==Kind::TMTutor) label += " - " + names_.personal_label(i);
            label += "##extra"+std::to_string(int(kind_))+"-"+std::to_string(i);
            if (ImGui::Selectable(label.c_str(),selected_==i)) selected_=i;
        }
        ImGui::EndChild();
    }
private:
    const char* title_en() const {
        switch (kind_) {
            case Kind::EggMoves: return "Egg Move Data";
            case Kind::Mega: return "Mega Evolution Data";
            case Kind::TrainerClasses: return "Trainer Class Data";
            case Kind::TMTutor: return "TM / Tutor Data";
            case Kind::Shops: return "Shop / Mart Data";
        }
        return "Data";
    }
    const char* instruction() const {
        switch(kind_) {
            case Kind::EggMoves: return localization::bilingual("USUM egg moves: a/0/1/2. Read only.","Capacites oeuf USUM : a/0/1/2. Lecture seule.");
            case Kind::Mega: return localization::bilingual("USUM Mega evolution GARC: a/0/1/5. Unverified fields remain raw.","Archive Mega-Evolutions USUM : a/0/1/5. Les champs non verifies restent bruts.");
            case Kind::TrainerClasses: return localization::bilingual("USUM trainer classes: a/1/0/5. Raw records, class layout to validate.","Classes de dresseurs USUM : a/1/0/5. Enregistrements bruts, format a valider.");
            case Kind::TMTutor: return localization::bilingual("Personal Data GARC (a/0/1/7) holds TM/Tutor compatibility. Exact bit mappings require validation.","Personal Data (a/0/1/7) contient les compatibilites CT/Tutor. La correspondance des bits reste a valider.");
            case Kind::Shops: return localization::bilingual("USUM shops may use code/CRO resources rather than a GARC; displaying shop.cro as raw bytes if found.","Les boutiques USUM peuvent utiliser du code/CRO et non une GARC ; affichage brut de shop.cro si present.");
        }
        return "";
    }
    static unsigned u16(const std::vector<std::uint8_t>& b, std::size_t p) { return p+1<b.size()?unsigned(b[p])|(unsigned(b[p+1])<<8):0; }
    void draw_decoded(const std::vector<std::uint8_t>& bytes) {
        if (kind_==Kind::EggMoves) {
            if (bytes.empty()) { ImGui::TextDisabled("%s",localization::bilingual("No egg moves.","Aucune capacite oeuf.")); return; }
            if (bytes.size()<2 || bytes.size()%2) { ImGui::TextDisabled("%s",localization::bilingual("Unsupported record length; see Hexadecimal.","Taille non reconnue : voir Hexadecimal.")); return; }
            unsigned count=u16(bytes,0);
            if (count > (bytes.size()-2)/2 || count>512) { ImGui::TextDisabled("%s",localization::bilingual("Invalid egg move count; see Hexadecimal.","Nombre de capacites invalide : voir Hexadecimal.")); return; }
            ImGui::Text("%s",names_.personal_label(selected_).c_str());
            ImGui::Text("%s: %u",localization::bilingual("Egg moves","Capacites oeuf"),count);
            for (unsigned i=0;i<count;++i) {
                unsigned move=u16(bytes,2+2*i);
                ImGui::BulletText("#%u - %s",move,names_.move_label(int(move)).c_str());
            }
            return;
        }
        if (kind_==Kind::Mega) {
            if (bytes.size() < 8 || bytes.size() % 8 != 0) {
                ImGui::TextDisabled("%s",localization::bilingual("Unexpected Mega record size; see Hexadecimal.", "Taille inattendue : voir Hexadecimal."));
                return;
            }
            ImGui::Text("%s", names_.personal_label(selected_).c_str());
            if (ImGui::BeginTable("##mega-data", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Form"); ImGui::TableSetupColumn("Method");
                ImGui::TableSetupColumn("Argument"); ImGui::TableSetupColumn("Unknown");
                ImGui::TableHeadersRow();
                for (std::size_t i = 0; i < bytes.size()/8; ++i) {
                    const auto off = i * 8;
                    const auto form = u16(bytes,off), method=u16(bytes,off+2);
                    const auto argument=u16(bytes,off+4), unknown=u16(bytes,off+6);
                    if (form==0 && method==0 && argument==0 && unknown==0) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%u",form);
                    ImGui::TableNextColumn(); ImGui::Text("%u",method);
                    ImGui::TableNextColumn(); ImGui::Text("%u",argument);
                    ImGui::TableNextColumn(); ImGui::Text("0x%04X",unknown);
                }
                ImGui::EndTable();
            }
            ImGui::TextDisabled("%s",localization::bilingual("Method 1: evolution by held item (verify before editing). Argument is the item ID.","Methode 1 : evolution par objet tenu (verifier avant edition). Argument : ID de l'objet."));
            return;
        }
        if (kind_==Kind::TrainerClasses) {
            ImGui::Text("%s: %d",localization::bilingual("Class record","Entree de classe"), selected_);
            ImGui::Text("%zu %s",bytes.size(),localization::bilingual("bytes","octets"));
            // The GARC holds 20-byte trainer-class entries in this USUM dump.
            // The binary layout of these entries is not verified. Surface their
            // individual values so they are inspectable without inventing labels.
            if (bytes.size()==20 && ImGui::BeginTable("##trainer-class-words",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Offset"); ImGui::TableSetupColumn("U16 (LE)"); ImGui::TableSetupColumn("Hex"); ImGui::TableHeadersRow();
                for (std::size_t off=0;off<20;off+=2) {
                    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("0x%02zX",off);
                    ImGui::TableNextColumn(); ImGui::Text("%u",u16(bytes,off));
                    ImGui::TableNextColumn(); ImGui::Text("%02X %02X",unsigned(bytes[off]),unsigned(bytes[off+1]));
                }
                ImGui::EndTable();
            }
            ImGui::TextDisabled("%s",localization::bilingual("Raw fields only: class field meanings require validation.","Champs bruts : la signification des champs reste a valider."));
            return;
        }
        if (kind_==Kind::TMTutor) {
            ImGui::Text("%s",names_.personal_label(selected_).c_str());
            if (bytes.size()!=84) {
                ImGui::TextDisabled("%s",localization::bilingual("Unexpected personal record size: inspect Hexadecimal.","Taille de fiche inattendue : consulter Hexadecimal."));
                return;
            }
            // Gen7 personal records are 0x54 bytes. The last 0x18 bytes are
            // compatibility flags: 0x10 TM/HM followed by 0x08 tutor flags.
            auto flags = [&](std::size_t begin,std::size_t count,const char* heading) {
                ImGui::SeparatorText(heading);
                unsigned total=0;
                for (std::size_t i=0;i<count*8;++i) if ((bytes[begin+i/8]>>(i%8))&1) ++total;
                ImGui::Text("%u %s",total,localization::bilingual("enabled flags","bits actifs"));
                if (ImGui::BeginTable(heading,8,ImGuiTableFlags_SizingStretchSame|ImGuiTableFlags_BordersInnerV)) {
                    for (std::size_t i=0;i<count*8;++i) if ((bytes[begin+i/8]>>(i%8))&1) {
                        ImGui::TableNextColumn(); ImGui::Text("#%zu",i+1);
                    }
                    ImGui::EndTable();
                }
            };
            flags(0x3C,16,localization::bilingual("TM / HM compatibility (bit indices)","Compatibilite CT / CS (indices de bits)"));
            flags(0x4C,8,localization::bilingual("Tutor compatibility (bit indices)","Compatibilite Tuteurs (indices de bits)"));
            ImGui::TextDisabled("%s",localization::bilingual("Bit indices are not move IDs; names require the game's TM/Tutor move mapping.","Les indices de bits ne sont pas des ID de capacites : les noms exigent la table CT/Tuteurs."));
            return;
        }
        ImGui::TextDisabled("%s",localization::bilingual("Format not validated: use the full hexadecimal tab.","Format non valide : utilisez l'onglet hexadecimal complet."));
    }
    std::filesystem::path resolved_path(const std::filesystem::path& root) const {
        const auto specified = std::filesystem::u8path(relative_);
        if (specified.is_absolute()) return specified;
        std::vector<std::filesystem::path> bases;
        auto add_base = [&](const std::filesystem::path& b) {
            if (!b.empty() && std::find(bases.begin(),bases.end(),b)==bases.end()) bases.push_back(b);
        };
        add_base(root);
        if (project_store()) {
            add_base(project_store()->source);
            add_base(project_store()->original);
            add_base(project_store()->root);
        }
        add_base(std::filesystem::current_path());
        // Some extracted dumps have romfs/ as their root; others contain romfs/.
        std::filesystem::path inner=specified;
        const auto first=inner.begin();
        if (first!=inner.end()) {
            const auto part=first->string();
            if (part=="romfs" || part=="RomFS" || part=="romFS") {
                inner.clear(); auto it=specified.begin(); ++it;
                for (;it!=specified.end();++it) inner/=*it;
            }
        }
        for (const auto& base : bases) {
            // USUM's Shop.cro is commonly stored directly at the RomFS root,
            // not in the fictitious romfs/cro/shop.cro directory.
            if (kind_==Kind::Shops) for (const auto& candidate : {
                    base/"romfs"/"Shop.cro",base/"romfs"/"shop.cro",
                    base/"RomFS"/"Shop.cro",base/"Shop.cro",
                    base/"source"/"romfs"/"Shop.cro",base/"original"/"romfs"/"Shop.cro"}) {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate,ec)) return candidate;
            }
            for (const auto& candidate : {base/specified,base/inner,base/"romfs"/inner,base/"RomFS"/inner,
                                           base/"romFS"/inner,base/"source"/specified,base/"source"/inner,
                                           base/"original"/specified,base/"original"/inner}) {
                std::error_code ec;
                if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
            }
        }
        // A missing source is reported, never silently replaced with guessed data.
        return root / specified;
    }
    static void draw_hex(const std::vector<std::uint8_t>& bytes) {
        ImGui::BeginChild("##extra-hex", ImVec2(0,0), true);
        ImGuiListClipper clipper;
        const std::size_t rows = (bytes.size()+15)/16;
        clipper.Begin(int(std::min<std::size_t>(rows, 0x7FFFFFFF)));
        while (clipper.Step()) for (int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            const auto off=std::size_t(i)*16;
            char row[96]; int n=std::snprintf(row,sizeof(row),"%06zX : ",off);
            for (std::size_t j=off;j<std::min(off+16,bytes.size()) && n>=0 && std::size_t(n)+4<sizeof(row);++j)
                n+=std::snprintf(row+n,sizeof(row)-std::size_t(n),"%02X ",unsigned(bytes[j]));
            ImGui::TextUnformatted(row);
        }
        ImGui::EndChild();
    }
    void load(const std::filesystem::path& path) {
        path_=path; archive_.reset(); file_mode_=false; file_bytes_.clear(); error_.clear(); selected_=0;
        if (relative_[0]=='\0') { error_=localization::bilingual("Specify a source path.","Indiquez un chemin source."); return; }
        if (!std::filesystem::exists(path)) { error_=localization::bilingual("Source file not found (check that the RomFS was extracted).","Fichier introuvable (verifiez l'extraction RomFS)."); return; }
        try {
            if (kind_==Kind::Shops && path.extension()==".cro") {
                std::ifstream input(path,std::ios::binary);
                if (!input) throw std::runtime_error("Cannot open CRO / Ouverture CRO impossible");
                file_bytes_=std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());
                file_mode_=true;
            } else archive_=std::make_unique<Archive>(path);
        } catch (const std::exception& e) {error_=e.what();}
    }
    std::filesystem::path root_;
    bool file_mode_=false;
    std::vector<std::uint8_t> file_bytes_;
    Kind kind_;
    char relative_[256]{};
    std::filesystem::path path_;
    std::unique_ptr<Archive> archive_;
    DataNames names_;
    int selected_=0;
    std::string error_;
};
} // namespace studio
