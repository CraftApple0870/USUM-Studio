#include "native/tutorial_widgets.h"
#include "native/pokemon_bundle_editor.h"
#include "assets/pokemon_bundle.h"
#include "native/personal_form_creation.h"
#include "native/personal_data_events.h"
#include "core/game_profile.h"
#include "native/project_binding.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace studio {
namespace {
struct CreatedPokemonRecord {
    unsigned species = 0, form = 0;
    std::string before_base, after_base, after_state, after_staged_state, after_overlay;
};
std::filesystem::path creation_history(const ProjectStore &project) {
    return project.root / "pokemon-creations.usum";
}
std::vector<CreatedPokemonRecord> read_creations(const ProjectStore &project) {
    std::vector<CreatedPokemonRecord> result;
    std::ifstream stream(creation_history(project), std::ios::binary);
    if (!stream) return result;
    std::string version;
    std::getline(stream, version);
    require(version == "USUMSTUDIO_POKEMON_CREATIONS 1", "Invalid Pokemon creation history version");
    CreatedPokemonRecord r;
    while (stream >> r.species >> r.form >> std::quoted(r.before_base) >>
           std::quoted(r.after_base) >> std::quoted(r.after_state) >>
           std::quoted(r.after_staged_state) >> std::quoted(r.after_overlay)) {
        result.push_back(r);
    }
    require(stream.eof(), "Invalid Pokemon creation history");
    return result;
}
void write_creations(const ProjectStore &project, const std::vector<CreatedPokemonRecord> &records) {
    std::ostringstream out;
    out << "USUMSTUDIO_POKEMON_CREATIONS 1\n";
    for (const auto &r : records)
        out << r.species << ' ' << r.form << ' ' << std::quoted(r.before_base) << ' '
            << std::quoted(r.after_base) << ' ' << std::quoted(r.after_state) << ' '
            << std::quoted(r.after_staged_state) << ' ' << std::quoted(r.after_overlay) << '\n';
    const auto data = out.str();
    write_file_atomic(creation_history(project),
                      View(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
}
bool safe_to_remove(const ProjectStore &project, const CreatedPokemonRecord &r) {
    if (project.base != r.after_base || project.current_state != r.after_state ||
        project.staged_state != r.after_staged_state || project.current_overlay != r.after_overlay ||
        project.unstaged()) return false;
    for (const auto *binding : ProjectBinding::all())
        if (binding->pending()) return false;
    return true;
}
}
void PokemonBundleEditor::draw(const ModelDocument &donor, SDL_Window *window) {
    if (!donor.is_pokemon())
        return;
    auto source_path = [&]() {
        if (auto *project = project_store())
            return project->source / TargetProfile::pokemon_archive;
        return donor.archive_sources.resolve(donor.dump, TargetProfile::pokemon_archive);
    };
    std::string path, error;
    bool ready = false;
    {
        std::lock_guard lock(dialog_->mutex);
        ready = dialog_->ready;
        if (ready) {
            path = dialog_->path;
            error = dialog_->error;
            dialog_->ready = false;
        }
    }
    if (ready)
        try {
            auto action = action_;
            action_ = 0;
            require(error.empty(), error);
            if (!path.empty()) {
                if (action == 1) {
                    if (auto *project = project_store())
                        require(!project->unstaged() &&
                                    std::all_of(project->edits.begin(), project->edits.end(),
                                                [](const auto &entry) {
                                                    return entry.second.kind == "composition" ||
                                                           entry.second.kind == "field-map-created" ||
                                                           entry.second.kind == "character-registration-created" ||
                                                           entry.second.kind == "battle-model-added" ||
                                                           (entry.second.kind == "asset-library" || entry.second.kind == "studio-asset");
                                                }),
                                "Stage and reload project edits before adding a bundle");
                    require(owner_ == donor.name,
                            "The donor changed; choose the bundle folder again");
                    Archive source(source_path());
                    auto bundle =
                        clone_pokemon_bundle(source, donor.pokemon, unsigned(target_), mode_ == 1);
                    folder_ = std::filesystem::u8path(path) /
                              ("pokemon-" + std::to_string(bundle.species) + "-form-" +
                               std::to_string(bundle.form));
                    save_pokemon_bundle(source, bundle, folder_);
                    // Additional form: create the matching Personal Data row as
                    // part of the same generated bundle folder.
                    if (mode_ == 1 && pending_create_personal_data_) {
                        auto personal_path = donor.dump / GameProfile::personal_archive;
                        if (auto *project = project_store())
                            personal_path = project->source / GameProfile::personal_archive;
                        create_pokemon_personal_form(
                            personal_path, folder_ / "personal.garc", bundle.species, bundle.form);
                    }
                    if (mode_ == 0)
                        target_ = int(bundle.species + 1);
                    std::optional<ProjectStore> before_creation;
                    if (auto *project = project_store())
                        before_creation = *project;
                    auto dump = donor.dump;
                    auto archives = donor.archive_sources;
                    archives.pokemon = folder_ / "pokemon.garc";
                    if (auto *project = project_store()) {
                        // Validate/generate both bundles before importing either.
                        if (mode_ == 1 && pending_create_personal_data_)
                            project->import_file(GameProfile::personal_archive,
                                                 folder_ / "personal.garc", true);
                        project->import_file(TargetProfile::pokemon_archive, archives.pokemon,
                                             true);
                        // Notify the Personal Data browser only after both imports succeeded.
                        // A failed graphics import must not signal a successful bundle.
                        if (mode_ == 1 && pending_create_personal_data_)
                            notify_personal_data_changed();
                        dump = project->source;
                        archives.pokemon.clear();
                    }
                    auto catalog = load_pokemon_catalog(dump, archives);
                    auto found = std::find_if(catalog.begin(), catalog.end(), [&](auto &e) {
                        return e.species == bundle.species && e.form == bundle.form && !e.female;
                    });
                    require(found != catalog.end(), "Created Pokemon is missing");
                    prepared_ = std::make_unique<ModelDocument>(
                        load_pokemon(dump, *found, false, nullptr, archives));
                    if (before_creation) {
                        auto *project = project_store();
                        undo_before_ = std::move(before_creation);
                        undo_after_base_ = project->base;
                        undo_after_state_ = project->current_state;
                        undo_after_staged_state_ = project->staged_state;
                        undo_after_overlay_ = project->current_overlay;
                        undo_after_revisions_ = project->revisions;
                        undo_after_root_ = project->root;
                        undo_species_ = bundle.species;
                        undo_form_ = bundle.form;
                    }
                    if (before_creation) {
                        auto *project = project_store();
                        auto creations = read_creations(*project);
                        creations.push_back({bundle.species, bundle.form, undo_before_->base,
                                             project->base, project->current_state,
                                             project->staged_state, project->current_overlay});
                        write_creations(*project, creations);
                    }
                    message_ = mode_ == 1
                        ? (pending_create_personal_data_
                            ? "Model form and Personal Data created. Use normal Stage / Export."
                            : "Model form created without changing Personal Data (0/1/7). Use normal Stage / Export.")
                        : "Created independent donor assets. Send the new entry to Studio to edit it, or export its GARC.";
                } else {
                    export_ = std::filesystem::u8path(path);
                    Archive(folder_ / "pokemon.garc").export_to(export_, {});
                    message_ = "Exported and verified the expanded Pokemon GARC.";
                }
            }
        } catch (const std::exception &e) {
            message_ = e.what();
        }
    if (!studio::TutorialWidgets::CollapsingHeader("pokemon_bundle_editor", "Add Pokemon assets"))
        return;
    ImGui::BeginDisabled(action_ != 0);
    ImGui::TextWrapped("Clone this archive donor into a new species row or an additional form. "
                       "Other game registration may still be required.");
    if (studio::TutorialWidgets::RadioButton("pokemon_bundle_editor", "New species row", &mode_, 0))
        target_ = 0;
    ImGui::SameLine();
    if (studio::TutorialWidgets::RadioButton("pokemon_bundle_editor", "Additional form", &mode_, 1))
        target_ = int(donor.pokemon.species);
    if (target_ == 0)
        try {
            target_ = int(pokemon_species_rows(Archive(source_path())) + 1);
        } catch (const std::exception &e) {
            message_ = e.what();
        }
    if (mode_ == 1) {
        ImGui::Checkbox("Creer aussi les Personal Data (0/1/7)###bundle-personal-option",
                        &create_personal_data_);
        if (!create_personal_data_)
            ImGui::TextDisabled("Mode modele seul : ne modifie pas les Personal Data.");
    }
    if (mode_ == 0) {
        ImGui::Text("New species: %d", target_);
    } else {
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("Target species", &target_);
    }
    auto choose = [&](int action) {
        owner_ = donor.name;
        action_ = action;
        if (action == 1)
            pending_create_personal_data_ = (mode_ == 1 && create_personal_data_);
        if (action == 1 && mode_ == 0)
            target_ = int(pokemon_species_rows(Archive(source_path())) + 1);
        {
            std::lock_guard lock(dialog_->mutex);
            dialog_->path.clear();
            dialog_->error.clear();
            dialog_->ready = false;
        }
        auto *owner = new std::shared_ptr<FolderSelection>(dialog_);
        auto callback = [](void *data, const char *const *files, int) {
            std::unique_ptr<std::shared_ptr<FolderSelection>> holder(
                static_cast<std::shared_ptr<FolderSelection> *>(data));
            auto &state = **holder;
            std::lock_guard lock(state.mutex);
            if (!files)
                state.error = SDL_GetError();
            else if (files[0])
                state.path = files[0];
            state.ready = true;
        };
        if (action == 1)
            SDL_ShowOpenFolderDialog(callback, owner, window, nullptr, false);
        else {
            static const SDL_DialogFileFilter filters[] = {{"Pokemon archive", "garc"}};
            SDL_ShowSaveFileDialog(callback, owner, window, filters, 1, "pokemon.garc");
        }
    };
    if (studio::TutorialWidgets::Button("pokemon_bundle_editor", "Create donor bundle..."))
        try {
            if (auto *project = project_store())
                require(!project->unstaged() &&
                            std::all_of(project->edits.begin(), project->edits.end(),
                                        [](const auto &entry) {
                                            return entry.second.kind == "composition" ||
                                                   entry.second.kind == "field-map-created" ||
                                                           entry.second.kind == "character-registration-created" ||
                                                           entry.second.kind == "battle-model-added" ||
                                                           (entry.second.kind == "asset-library" || entry.second.kind == "studio-asset");
                                        }),
                        "Stage and reload project edits before adding a bundle");
            choose(1);
        } catch (const std::exception &e) {
            message_ = e.what();
        }
    // Persistent, safe deletion for the newest local creation. Older creations are
    // intentionally locked until their successors are removed: their member indices
    // depend on the appended archive layout.
    if (auto *project = project_store()) {
        try {
            auto creations = read_creations(*project);
            if (!creations.empty()) {
                const auto last = creations.back();
                ImGui::Separator();
                ImGui::TextWrapped("Created Pokemon %u / form %u (local project)",
                                   last.species, last.form);
                const bool removable = safe_to_remove(*project, last) && action_ == 0;
                ImGui::BeginDisabled(!removable);
                if (ImGui::Button("Remove saved creation... / Supprimer la creation..."))
                    ImGui::OpenPopup("Delete saved Pokemon creation");
                ImGui::EndDisabled();
                if (!removable)
                    ImGui::TextDisabled("Unavailable: this creation has dependent project changes. "
                                        "Finish/revert them first.");
                if (creations.size() > 1)
                    ImGui::TextDisabled("Older creations: remove later creations first to "
                                        "preserve archive indexes.");
                if (ImGui::BeginPopupModal("Delete saved Pokemon creation", nullptr,
                                          ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextWrapped("WARNING / ATTENTION: Permanently remove local Pokemon %u "
                                       "/ form %u from the project and future exports? "
                                       "Its index becomes reusable. Original ROM/dump data is "
                                       "untouched. Generated bundle folders are not deleted. "
                                       "This cannot be undone by this command.",
                                       last.species, last.form);
                    if (ImGui::Button("Confirm removal / Confirmer la suppression")) {
                        try {
                            require(safe_to_remove(*project, last),
                                    "Project changed; deletion cancelled");
                            auto old = *project;
                            try {
                                project->base = last.before_base;
                                project->source = project->source_for(project->base);
                                project->save();
                                creations.pop_back();
                                write_creations(*project, creations);
                            } catch (...) {
                                *project = std::move(old);
                                project->save();
                                throw;
                            }
                            prepared_.reset();
                            created_.reset();
                            undo_before_.reset();
                            undone_ = true;
                            notify_personal_data_changed();
                            message_ = "Local creation removed; its slot is now free. "
                                       "Reload Models to update the catalogue.";
                        } catch (const std::exception &e) { message_ = e.what(); }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel / Annuler")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
        } catch (const std::exception &e) {
            ImGui::TextWrapped("Creation history: %s", e.what());
        }
    }
    if (undo_before_) {
        ImGui::Separator();
        ImGui::TextWrapped("Last created entry: Pokemon %u / form %u. Undoing restores the "
                           "project state from immediately before creation (not the original dump).",
                           undo_species_, undo_form_);
        auto *project = project_store();
        const bool unchanged = project && project->root == undo_after_root_ &&
            project->base == undo_after_base_ && project->current_state == undo_after_state_ &&
            project->staged_state == undo_after_staged_state_ &&
            project->current_overlay == undo_after_overlay_ &&
            project->revisions.size() == undo_after_revisions_.size() &&
            std::equal(project->revisions.begin(), project->revisions.end(),
                       undo_after_revisions_.begin(), [](const auto &a, const auto &b) {
                           return a.state == b.state && a.overlay == b.overlay &&
                                  a.label == b.label;
                       }) &&
            std::none_of(ProjectBinding::all().begin(), ProjectBinding::all().end(),
                         [](const auto *binding) { return binding->pending(); });
        ImGui::BeginDisabled(!unchanged || action_ != 0);
        if (ImGui::Button("Undo last Pokemon creation / Annuler la derniere creation"))
            ImGui::OpenPopup("Undo Pokemon creation confirmation");
        ImGui::EndDisabled();
        if (!unchanged)
            ImGui::TextDisabled("Undo unavailable: project has changed since creation, "
                                "or unsaved edits exist.");
        if (ImGui::BeginPopupModal("Undo Pokemon creation confirmation", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Remove created Pokemon %u / form %u from the project? "
                               "Its slot will become reusable. No original dump files are changed. "
                               "The previously generated bundle folder remains on disk.",
                               undo_species_, undo_form_);
            if (ImGui::Button("Confirm undo / Confirmer")) {
                try {
                    require(unchanged, "Project changed since creation; undo was cancelled");
                    auto before = *project;
                    try {
                        *project = *undo_before_;
                        project->save();
                    } catch (...) {
                        *project = std::move(before);
                        throw;
                    }
                    auto creations = read_creations(*project);
                    if (!creations.empty() && creations.back().species == undo_species_ &&
                        creations.back().form == undo_form_) {
                        creations.pop_back();
                        write_creations(*project, creations);
                    }
                    prepared_.reset();
                    created_.reset();
                    undo_before_.reset();
                    undone_ = true;
                    notify_personal_data_changed();
                    message_ = "Creation removed from project and next export; slot released. "
                               "Reload the model browser to refresh the list.";
                } catch (const std::exception &e) {
                    message_ = e.what();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel / Annuler"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    if (prepared_) {
        if (studio::TutorialWidgets::Button("pokemon_bundle_editor", "Send new entry to Studio"))
            created_ = std::make_unique<ModelDocument>(*prepared_);
        if (studio::TutorialWidgets::Button("pokemon_bundle_editor",
                                            "Export initial bundle GARC..."))
            choose(2);
    }
    ImGui::EndDisabled();
    if (!message_.empty())
        ImGui::TextWrapped("%s", message_.c_str());
}
}
