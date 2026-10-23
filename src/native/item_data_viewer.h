#pragma once
#include "formats/archive.h"
#include "field/area.h"
#include "field/map_catalog.h"
#include "native/project_binding.h"
#include "native/localization.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace studio {
// USUM item table: GARC a/0/1/9, one binary entry per item.
// Read-only Item Data with selectable Game Text archives.
// pk3DS USUM TextReference: item descriptions = member 39, names = member 40.
class ItemDataViewer {
public:
    void draw(const std::filesystem::path &dump, bool active) {
        if (!active) return;
        if (!ImGui::Begin(studio::localization::tr("Item Data"), nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End(); return;
        }
        const auto root = project_store() ? project_store()->source : dump;
        const auto path = root / "romfs/a/0/1/9";
        if (!archive_ || path != path_) load(root);
        if (ImGui::Button("Charger / Actualiser###item-reload")) load(root);
        ImGui::SameLine();
        const int wanted_slot = studio::localization::current == studio::localization::Language::French ? 3 : 2;
        if (wanted_slot != text_slot_) {
            text_slot_ = wanted_slot;
            load_texts(root);
        }
        ImGui::TextDisabled("Game Text : %s (0/3/%d)",
            text_slot_ == 3 ? studio::localization::bilingual("French", "Francais") : "English", text_slot_);
        ImGui::SameLine();
        if (!text_error_.empty()) ImGui::TextDisabled("Game Text : %s", text_error_.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("a/0/1/9 | lecture seule");
        if (!error_.empty()) ImGui::TextWrapped(studio::localization::bilingual("Error: %s", "Erreur : %s"), error_.c_str());
        if (archive_ && archive_->size()) {
            const auto count = int(archive_->size());
            selected_ = std::clamp(selected_, 0, count - 1);
            ImGui::Text(studio::localization::bilingual("Item #%d / %d", "Objet #%d / %d"), selected_, count - 1);
            ImGui::SeparatorText(studio::localization::bilingual("Item text", "Textes de l'objet"));
            if (selected_ < int(names_.size()) && !names_[selected_].empty())
                ImGui::TextWrapped(studio::localization::bilingual("Name (table 40): %s", "Nom (table 40) : %s"), names_[selected_].c_str());
            else ImGui::TextDisabled(studio::localization::bilingual("No name for this ID in table 40.", "Nom absent pour cet ID dans la table 40."));
            if (selected_ < int(descriptions_.size()) && !descriptions_[selected_].empty())
                ImGui::TextWrapped(studio::localization::bilingual("Description (table 39): %s", "Description (table 39) : %s"), descriptions_[selected_].c_str());
            else ImGui::TextDisabled(studio::localization::bilingual("No description for this ID in table 39.", "Description absente pour cet ID dans la table 39."));
            auto bytes = archive_->decoded(std::size_t(selected_));
            ImGui::Separator();
            if (bytes.size() >= 0x24) {
                const auto u16 = [&](std::size_t offset) { return unsigned(bytes[offset]) | (unsigned(bytes[offset+1]) << 8); };
                ImGui::Text(studio::localization::bilingual("Buy price: %u", "Prix d'achat : %u"), u16(0) * 10);
                ImGui::Text(studio::localization::bilingual("Sell price: %u", "Prix de vente : %u"), u16(0) * 5);
                ImGui::Text(studio::localization::bilingual("Held effect: %u | Argument: %u", "Effet tenu : %u | Argument : %u"), unsigned(bytes[2]), unsigned(bytes[3]));
                ImGui::Text(studio::localization::bilingual("Fling effect: %u | power: %u", "Degommage - effet : %u | puissance : %u"), unsigned(bytes[5]), unsigned(bytes[6]));
                const unsigned packed = u16(8);
                ImGui::Text(studio::localization::bilingual("Pocket (field): %u | Pocket (battle): %u", "Poche (terrain) : %u | Poche (combat) : %u"), (packed >> 7) & 15, packed >> 11);
                ImGui::Text(studio::localization::bilingual("Field routine: %u | battle routine: %u", "Routine terrain : %u | routine combat : %u"), unsigned(bytes[10]), unsigned(bytes[11]));
                ImGui::Text(studio::localization::bilingual("Sort index: %u", "Index de tri : %u"), unsigned(bytes[15]));
                ImGui::SeparatorText(studio::localization::bilingual("Binary fields", "Champs binaires"));
                ImGui::TextDisabled(studio::localization::bilingual("%zu bytes | hexadecimal view", "%zu octets | affichage hexadecimale"), bytes.size());
            } else {
                ImGui::TextDisabled(studio::localization::bilingual("%zu-byte entry: unexpected format (expected 36).", "Entree de %zu octets : format inattendu (36 attendus)."), bytes.size());
            }
            if (ImGui::CollapsingHeader("Hexadecimal###item-hex")) {
                for (std::size_t off = 0; off < bytes.size(); off += 16) {
                    std::string row;
                    char tmp[16];
                    std::snprintf(tmp, sizeof(tmp), "%04zX : ", off);
                    row += tmp;
                    for (std::size_t i=off; i<std::min(off+16, bytes.size()); ++i) {
                        std::snprintf(tmp, sizeof(tmp), "%02X ", unsigned(bytes[i]));
                        row += tmp;
                    }
                    ImGui::TextUnformatted(row.c_str());
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled(studio::localization::bilingual("Icons: separate graphics archive, not loaded in this version.", "Icones : archive graphique distincte, non chargee dans cette version."));
        }
        ImGui::End();
    }

    void draw_browser() {
        if (ImGui::InputInt("ID###item-id", &selected_)) {
            if (archive_ && archive_->size()) selected_ = std::clamp(selected_, 0, int(archive_->size())-1);
            else selected_ = std::max(0, selected_);
        }
        if (!archive_) { ImGui::TextDisabled(studio::localization::bilingual("No item archive loaded.", "Aucune archive d'objets chargee.")); return; }
        ImGui::Separator();
        ImGui::TextDisabled(studio::localization::bilingual("%zu items", "%zu objets"), archive_->size());
        ImGui::BeginChild("##items-list", ImVec2(0,0), false);
        for (std::size_t i=0; i<archive_->size(); ++i) {
            std::string label = std::to_string(i) + " - ";
            label += i < names_.size() && !names_[i].empty() ? names_[i] : studio::localization::bilingual("Item ", "Objet ") + std::to_string(i);
            if (ImGui::Selectable((label+"###item-"+std::to_string(i)).c_str(), selected_ == int(i))) selected_ = int(i);
        }
        ImGui::EndChild();
    }
private:
    void load(const std::filesystem::path &root) {
        path_ = root / "romfs/a/0/1/9";
        archive_.reset(); names_.clear(); descriptions_.clear(); error_.clear();
        try { archive_ = std::make_unique<Archive>(path_); }
        catch (const std::exception &e) { error_ = e.what(); return; }
        text_slot_ = studio::localization::current == studio::localization::Language::French ? 3 : 2;
        load_texts(root);

    }
    void load_texts(const std::filesystem::path &root) {
        names_.clear();
        descriptions_.clear();
        text_error_.clear();
        try {
            const auto text_path = root / "romfs/a/0/3" / std::to_string(text_slot_);
            Archive text(text_path);
            if (text.size() <= 40) {
                text_error_ = "Tables 39/40 absentes dans cette archive.";
                return;
            }
            descriptions_ = decode_location_text(text.decoded(39));
            names_ = decode_location_text(text.decoded(40));
        } catch (const std::exception &e) {
            text_error_ = e.what();
        }
    }
    std::filesystem::path path_;
    std::unique_ptr<Archive> archive_;
    std::vector<std::string> names_;
    std::vector<std::string> descriptions_;
    std::string text_error_;
    std::string error_;
    int text_slot_ = -1; // Auto-selected from Affichage > Langue.
    int selected_ = 1;
};
}
