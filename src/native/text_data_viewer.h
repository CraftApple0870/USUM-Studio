#pragma once

#include "formats/archive.h"
#include "field/map_catalog.h"
#include "native/localization.h"
#include "native/project_binding.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace studio {
// Read-only browsing of USUM Game Text and Story Text archives.
// The ROM text language follows the interface language: EN slot 2 / FR slot 3.
class TextDataViewer {
public:
    void draw(const std::filesystem::path &dump, bool active) {
        if (!active) return;
        if (!ImGui::Begin("Text Data", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End(); return;
        }
        const auto root = project_store() ? project_store()->source : dump;
        const int wanted_language = localization::current == localization::Language::French ? 3 : 2;
        const auto wanted_path = archive_path(root, wanted_language);
        if (wanted_path != path_) load(root, wanted_language);
        if (ImGui::Button("Actualiser###text-data-reload")) load(root, wanted_language);
        ImGui::SameLine();
        ImGui::TextDisabled("%s  |  %s  |  lecture seule",
            kind_ == 0 ? "Game Text" : "Story Text",
            wanted_language == 3 ? "FR" : "EN");
        if (!error_.empty()) ImGui::TextWrapped("Archive : %s", error_.c_str());
        if (archive_) {
            ImGui::Text("Archive : %s", path_.string().c_str());
            ImGui::Text("Tables : %zu", archive_->size());
            ImGui::Separator();
            if (selected_ >= 0 && std::size_t(selected_) < archive_->size()) {
                ImGui::Text("Table #%d", selected_);
                if (loaded_member_ != selected_) load_member();
                if (!member_error_.empty()) ImGui::TextWrapped("Erreur de decodage : %s", member_error_.c_str());
                else {
                    ImGui::TextDisabled("%zu lignes", lines_.size());
                    ImGui::Separator();
                    ImGui::BeginChild("##text-lines", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    ImGuiListClipper clipper;
                    clipper.Begin(int(lines_.size()));
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                            ImGui::PushID(i);
                            ImGui::TextDisabled("%04d", i);
                            ImGui::SameLine();
                            ImGui::TextUnformatted(lines_[i].c_str());
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();
                }
            }
        }
        ImGui::End();
    }

    void draw_browser() {
        const char *kinds[] = {"Game Text", "Story Text"};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##text-kind", &kind_, kinds, 2)) {
            path_.clear(); archive_.reset(); loaded_member_ = -1;
            selected_ = 0;
        }
        ImGui::Separator();
        if (!archive_) {
            ImGui::TextDisabled("Aucune archive chargee.");
            return;
        }
        ImGui::TextDisabled("%zu tables", archive_->size());
        if (ImGui::InputInt("Table###text-member-id", &selected_)) {
            selected_ = std::clamp(selected_, 0, int(archive_->size()) - 1);
            loaded_member_ = -1;
        }
        ImGui::Separator();
        ImGui::BeginChild("##text-table-list", ImVec2(0, 0), false);
        ImGuiListClipper clipper;
        clipper.Begin(int(archive_->size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                std::string label = "Table " + std::to_string(i) + "###text-table-" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), selected_ == i)) {
                    selected_ = i; loaded_member_ = -1;
                }
            }
        }
        ImGui::EndChild();
    }

private:
    std::filesystem::path archive_path(const std::filesystem::path &root, int language) const {
        return root / "romfs/a/0" / (kind_ == 0 ? "3" : "4") / std::to_string(language);
    }
    void load(const std::filesystem::path &root, int language) {
        path_ = archive_path(root, language);
        archive_.reset(); lines_.clear(); error_.clear(); member_error_.clear(); loaded_member_ = -1;
        try {
            archive_ = std::make_unique<Archive>(path_);
            if (archive_->size()) selected_ = std::clamp(selected_, 0, int(archive_->size()) - 1);
            else selected_ = 0;
        } catch (const std::exception &e) { error_ = e.what(); }
    }
    void load_member() {
        lines_.clear(); member_error_.clear(); loaded_member_ = selected_;
        try { lines_ = decode_location_text(archive_->decoded(std::size_t(selected_))); }
        catch (const std::exception &e) { member_error_ = e.what(); }
    }
    int kind_ = 0;
    int selected_ = 0;
    int loaded_member_ = -1;
    std::filesystem::path path_;
    std::unique_ptr<Archive> archive_;
    std::vector<std::string> lines_;
    std::string error_, member_error_;
};
}
