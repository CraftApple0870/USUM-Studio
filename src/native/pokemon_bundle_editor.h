#pragma once
#include "assets/model_document.h"
#include "native/folder_picker.h"
#include "project/project_store.h"
#include <optional>
#include <utility>
namespace studio {
class PokemonBundleEditor {
  public:
    void draw(const ModelDocument &donor, SDL_Window *window);
    bool take_undone() { return std::exchange(undone_, false); }
    std::unique_ptr<ModelDocument> take_created() {
        return std::move(created_);
    }

  private:
    std::shared_ptr<FolderSelection> dialog_ = std::make_shared<FolderSelection>();
    std::unique_ptr<ModelDocument> created_, prepared_;
    std::string message_;
    std::filesystem::path folder_, export_;
    int mode_ = 0, target_ = 0, action_ = 0;
    bool create_personal_data_ = true;
    bool pending_create_personal_data_ = true;
    std::string owner_;
    std::optional<ProjectStore> undo_before_;
    std::string undo_after_base_, undo_after_state_, undo_after_staged_state_, undo_after_overlay_;
    std::vector<ProjectRevision> undo_after_revisions_;
    std::filesystem::path undo_after_root_;
    unsigned undo_species_ = 0, undo_form_ = 0;
    bool undone_ = false;
};
}
