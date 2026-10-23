#pragma once
#include "formats/archive.h"
#include "native/project_binding.h"
#include "native/localization.h"
#include "field/area.h"
#include "field/map_catalog.h"
#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace studio {
// Read-only, bounds-checked inspection of the two USUM trainer GARCs.
// a/1/0/6: trainer metadata; a/1/0/7: parties/Pokemon.
// No undocumented offsets are interpreted as gameplay fields.
class TrainerDataViewer {
public:
    explicit TrainerDataViewer(bool teams) : teams_(teams) {}

    void draw(const std::filesystem::path& dump, bool active) {
        if (!active) return;
        const char* title = teams_ ?
            localization::bilingual("Trainer Pokemon###trainer-pokemon-view", "Pokemon des dresseurs###trainer-pokemon-view") :
            localization::bilingual("Trainer Data###trainer-data-view", "Donnees des dresseurs###trainer-data-view");
        if (!ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
        const auto root = project_store() ? project_store()->source : dump;
        const auto candidate = root / (teams_ ? "romfs/a/1/0/7" : "romfs/a/1/0/6");
        if (!archive_ || path_ != candidate) load(candidate);
        const int desired_language = localization::current == localization::Language::French ? 3 : 2;
        if (name_root_ != root || name_language_ != desired_language) load_names(root, desired_language);
        if (ImGui::Button(localization::bilingual("Reload", "Actualiser"))) { load(candidate); load_names(root, desired_language); }
        ImGui::SameLine();
        ImGui::TextDisabled("%s | %s", teams_ ? "a/1/0/7" : "a/1/0/6",
            localization::bilingual("read-only", "lecture seule"));
        if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        if (!name_error_.empty()) ImGui::TextDisabled("%s: %s", localization::bilingual("Game Text", "Textes du jeu"), name_error_.c_str());
        if (archive_ && archive_->size()) {
            selected_ = std::clamp(selected_, 0, int(archive_->size()) - 1);
            ImGui::Text(localization::bilingual("Entry %d / %zu", "Entree %d / %zu"), selected_, archive_->size() - 1);
            try {
                const auto bytes = archive_->decoded(std::size_t(selected_));
                ImGui::Text("%zu %s", bytes.size(), localization::bilingual("bytes", "octets"));
                if (ImGui::BeginTabBar("##trainer-detail-tabs")) {
                    if (ImGui::BeginTabItem(localization::bilingual("Decoded", "Decode"))) {
                        draw_decoded(bytes);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem(localization::bilingual("Hexadecimal", "Hexadecimal"))) {
                        ImGui::BeginChild("##trainer-bytes", ImVec2(0, 0), true);
                        for (std::size_t off = 0; off < bytes.size(); off += 16) {
                            char row[96];
                            int written = std::snprintf(row, sizeof(row), "%04zX : ", off);
                            for (std::size_t j = off; j < std::min(off + 16, bytes.size()) && written >= 0 && std::size_t(written) + 4 < sizeof(row); ++j)
                                written += std::snprintf(row + written, sizeof(row) - std::size_t(written), "%02X ", unsigned(bytes[j]));
                            ImGui::TextUnformatted(row);
                        }
                        ImGui::EndChild();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            } catch (const std::exception& e) {
                ImGui::TextWrapped("%s: %s", localization::bilingual("Decode error", "Erreur de decodage"), e.what());
            }
        }
        ImGui::End();
    }

    void draw_browser() {
        if (!archive_ || !archive_->size()) {
            ImGui::TextDisabled("%s", localization::bilingual("No trainer archive loaded.", "Archive des dresseurs non chargee."));
            return;
        }
        ImGui::Text("%zu %s", archive_->size(), localization::bilingual("records", "enregistrements"));
        if (ImGui::InputInt("ID##trainer-record-id", &selected_))
            selected_ = std::clamp(selected_, 0, int(archive_->size()) - 1);
        ImGui::Separator();
        ImGui::BeginChild("##trainer-records", ImVec2(0,0));
        ImGuiListClipper clipper;
        clipper.Begin(int(archive_->size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto label = std::string(localization::bilingual("Trainer #", "Dresseur n°")) + std::to_string(i) + "##trainer-" + (teams_ ? "team-" : "data-") + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), selected_ == i)) selected_ = i;
            }
        }
        ImGui::EndChild();
    }
private:
    // Gen 7 layouts: pk3DS TrainerData7 (0x14) and TrainerPoke7 (0x20).
    // Read-only; all offsets are checked before access.
    static unsigned u16(const std::vector<std::uint8_t>& b, std::size_t off) {
        return off + 1 < b.size() ? unsigned(b[off]) | (unsigned(b[off + 1]) << 8) : 0;
    }
    void draw_decoded(const std::vector<std::uint8_t>& bytes) {
        if (!teams_) {
            if (bytes.size() < 0x14) {
                ImGui::TextDisabled("%s", localization::bilingual("Incomplete trainer record (expected 20 bytes)",
                    "Donnees dresseur incompletes (20 octets attendus)"));
                return;
            }
            ImGui::Text("%s: %u", localization::bilingual("Trainer class", "Classe"), u16(bytes, 0));
            ImGui::Text("%s: %u", localization::bilingual("Battle mode", "Mode combat"), unsigned(bytes[2]));
            ImGui::Text("%s: %u", localization::bilingual("Team size", "Taille equipe"), unsigned(bytes[3]));
            ImGui::Text("AI: %u", unsigned(bytes[0x0C]));
            ImGui::Text("%s: %u", localization::bilingual("Money", "Argent"), unsigned(bytes[0x11]));
            for (int i=0; i<4; ++i) {
                const unsigned id=u16(bytes, 4 + i*2);
                if (id) ImGui::Text("%s %d: %s", localization::bilingual("Bag item", "Objet sac"), i+1, name_for(items_, id).c_str());
            }
            ImGui::TextDisabled("%s", localization::bilingual("Open Trainer Pokemon with the same ID to inspect the team.",
                "Ouvrez Pokemon des dresseurs avec le meme ID pour voir son equipe."));
            return;
        }
        constexpr std::size_t record_size = 0x20;
        if (bytes.empty()) {
            ImGui::TextDisabled("%s", localization::bilingual("Empty team", "Equipe vide"));
            return;
        }
        if (bytes.size() % record_size != 0) {
            ImGui::TextDisabled("%s", localization::bilingual("Nonstandard team record length: inspect hexadecimal below.",
                "Taille d'equipe inhabituelle : consultez l'hexadecimal."));
            return;
        }
        const auto count = bytes.size()/record_size;
        for (std::size_t i=0; i<count; ++i) {
            const std::size_t base=i*record_size;
            ImGui::PushID(int(i));
            const unsigned species=u16(bytes,base+0x10);
            const unsigned form=bytes[base+0x12];
            const unsigned level=bytes[base+0x0E];
            char title[128];
            const auto species_name = name_for(species_, species);
            std::snprintf(title,sizeof(title),"%s %zu - %s (%s %u) - Lv.%u",
                localization::bilingual("Pokemon", "Pokemon"),i+1,species_name.c_str(),
                localization::bilingual("form", "forme"),form,level);
            if (ImGui::TreeNodeEx("##pokemon", ImGuiTreeNodeFlags_DefaultOpen, "%s",title)) {
                const unsigned item=u16(bytes,base+0x14);
                ImGui::Text("%s: %s",localization::bilingual("Held item", "Objet tenu"), name_for(items_, item).c_str());
                for (int move=0;move<4;++move) {
                    const unsigned id=u16(bytes,base+0x18+move*2);
                    ImGui::Text("%s %d: %s",localization::bilingual("Move", "Attaque"),move+1,name_for(moves_, id).c_str());
                }
                ImGui::Text("%s: %u",localization::bilingual("Nature ID", "ID nature"),unsigned(bytes[base+1]));
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::Separator();
        ImGui::TextDisabled("%s", localization::bilingual(
            "Names are read from Game Text (EN/FR). Missing names display IDs. Hex remains available.",
            "Noms issus des textes du jeu (FR/EN). Un nom absent affiche son ID. Hexa conserve."));
    }
    static std::string name_for(const std::vector<std::string>& names, unsigned id) {
        // Preserve ID for debugging and never invent a text for missing records.
        if (id < names.size() && !names[id].empty())
            return names[id] + " (#" + std::to_string(id) + ")";
        return "#" + std::to_string(id);
    }
    void load_names(const std::filesystem::path& root, int language) {
        name_root_ = root;
        name_language_ = language;
        species_.clear(); moves_.clear(); items_.clear(); name_error_.clear();
        try {
            // Same text archive and member IDs used by Personal/Move/Item Data viewers.
            Archive text(root / "romfs/a/0/3" / std::to_string(language));
            const auto required = std::max({std::size_t(TargetProfile::pokemon_names_member),
                                            std::size_t(TargetProfile::move_names_member),
                                            std::size_t(TargetProfile::item_names_member)});
            if (text.size() <= required) {
                name_error_ = localization::bilingual("Text archive lacks required tables", "Tables de noms absentes de l'archive");
                return;
            }
            species_ = decode_location_text(text.decoded(TargetProfile::pokemon_names_member));
            moves_   = decode_location_text(text.decoded(TargetProfile::move_names_member));
            items_   = decode_location_text(text.decoded(TargetProfile::item_names_member));
        } catch (const std::exception& e) {
            name_error_ = e.what();
        }
    }
    void load(const std::filesystem::path& path) {
        path_ = path;
        archive_.reset();
        error_.clear();
        selected_ = 0;
        try {
            archive_ = std::make_unique<Archive>(path);
            if (!archive_->size()) error_ = localization::bilingual("Empty trainer archive.", "Archive des dresseurs vide.");
        } catch (const std::exception& e) { error_ = e.what(); }
    }
    std::filesystem::path name_root_;
    int name_language_ = -1;
    std::vector<std::string> species_, moves_, items_;
    std::string name_error_;
    bool teams_;
    int selected_ = 0;
    std::filesystem::path path_;
    std::unique_ptr<Archive> archive_;
    std::string error_;
};
} // namespace studio
