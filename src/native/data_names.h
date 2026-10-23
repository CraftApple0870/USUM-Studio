#pragma once
#include "formats/archive.h"
#include "field/area.h"
#include "field/map_catalog.h"
#include "native/localization.h"
#include "native/project_binding.h"
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>
#include <imgui.h>
namespace studio {
// Game Text resolver shared by read-only Data viewers.
class DataNames {
public:
    void load(const std::filesystem::path& root) {
        const int slot = localization::current == localization::Language::French ? 3 : 2;
        const auto path = root / "romfs/a/0/3" / std::to_string(slot);
        if (path == path_ && !species_.empty()) return;
        path_ = path; species_.clear(); moves_.clear(); owners_.clear(); error_.clear();
        try {
            Archive text(path);
            if (text.size() <= TargetProfile::move_names_member) { error_ = "Missing text tables / Tables de texte absentes"; return; }
            species_ = decode_location_text(text.decoded(TargetProfile::pokemon_names_member));
            moves_ = decode_location_text(text.decoded(TargetProfile::move_names_member));
        } catch (const std::exception& e) { error_ = e.what(); }
        try {
            Archive personal(root / "romfs/a/0/1/7");
            owners_.resize(personal.size());
            const int base = int(std::min<std::size_t>(personal.size(), personal.size() >= 1026 ? 1026 : 808));
            for (int s = 1; s < base; ++s) {
                auto b = personal.decoded(std::size_t(s));
                if (b.size() <= 0x20) continue;
                int count = b[0x20];
                int start = int(unsigned(b[0x1C]) | (unsigned(b[0x1D]) << 8));
                if (base > 808 && s <= 807 && start >= 808 && start < 1026) start += base - 808;
                if (start <= 0 || count <= 1) continue;
                for (int form = 1; form < count; ++form) {
                    const int index = start + form - 1;
                    if (index >= 0 && std::size_t(index) < owners_.size() && owners_[index].empty())
                        owners_[index] = species_label(s) + " - " + localization::bilingual("Form ", "Forme ") + std::to_string(form);
                }
            }
        } catch (const std::exception&) { /* Name tables remain usable without personal data. */ }
    }
    std::string species_label(int id) const {
        if (id >= 0 && std::size_t(id) < species_.size() && !species_[id].empty()) return species_[id];
        return std::string(localization::bilingual("Species ", "Espèce ")) + std::to_string(id);
    }
    std::string personal_label(int id) const {
        if (id >= 0 && std::size_t(id) < owners_.size() && !owners_[id].empty()) return owners_[id];
        return species_label(id);
    }
    std::string move_label(int id) const {
        if (id >= 0 && std::size_t(id) < moves_.size() && !moves_[id].empty()) return moves_[id];
        return std::string(localization::bilingual("Move ", "Capacité ")) + std::to_string(id);
    }
    const std::string& error() const { return error_; }
private:
    std::filesystem::path path_;
    std::vector<std::string> species_, moves_, owners_;
    std::string error_;
};
}
