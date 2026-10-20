#pragma once
#include "formats/archive.h"
#include "assets/pokemon_catalog.h"
#include "core/game_profile.h"
#include "native/project_binding.h"
#include "native/localization.h"
#include "native/personal_data_events.h"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <cstdio>
#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>
#include <set>
#include <array>
#include <vector>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace studio {
// Explorateur Personal Data de generation VII, en lecture seule.
// Les entrees >= 808 ne sont PAS automatiquement des formes :
// leur attribution precise necessite le decodage de la table des formes.
class PersonalDataViewer {
public:
    void draw(const std::filesystem::path &dump, bool active) {
        if (!active) return;
        // A normal dockable ImGui window, controlled by the Data dockspace.
        if (!ImGui::Begin("Personal Data", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }
        auto path = dump / GameProfile::personal_archive;
        if (auto *project = project_store()) path = project->source / GameProfile::personal_archive;
        // Opening the workspace loads the live project source automatically.
        // Revision notifications come from the model-form importer. Never discard
        // unexported edits: ask the user to export or manually reload instead.
        const bool source_changed = archive_ && !external_source_ && loaded_path_ != path;
        const bool changed_by_models = observed_revision_ != personal_data_revision;
        if (!archive_ && !external_source_) {
            load(path);
            observed_revision_ = personal_data_revision;
        } else if ((source_changed || changed_by_models) && !external_source_) {
            if (edits_.empty()) {
                const int keep_species = species_pick_;
                const int keep_form = form_pick_;
                load(path);
                species_pick_ = std::clamp(keep_species, 1, std::max(1, base_species_count_ - 1));
                form_pick_ = std::clamp(keep_form, 0, std::max(0, form_count(species_pick_) - 1));
                const int wanted = personal_index(species_pick_, form_pick_);
                if (wanted >= 0 && archive_ && wanted < int(archive_->size()))
                    select(wanted);
                status_ = "Personal Data rechargees automatiquement depuis le projet.";
                observed_revision_ = personal_data_revision;
            } else {
                status_ = "Les modeles ont modifie les Personal Data. Exporte ou actualise tes modifications avant de recharger.";
            }
        }

        if (ImGui::Button("Charger / Actualiser###personal-load")) {
            load(path);
            external_source_ = false;
            observed_revision_ = personal_data_revision;
        }
        ImGui::SameLine();
        if (ImGui::Button("Ouvrir archive externe...###personal-external-toggle"))
            show_external_path_ = !show_external_path_;
        if (show_external_path_) {
            ImGui::SetNextItemWidth(-180.f);
            ImGui::InputTextWithHint("##personal-external-path", "Chemin complet de personal.garc", external_path_, sizeof(external_path_));
            ImGui::SameLine();
            if (ImGui::Button("Ouvrir fichier###personal-external-open")) {
                if (external_path_[0]) {
                    load(std::filesystem::u8path(external_path_));
                    external_source_ = true;
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Archive 0/1/7 | Export via le projet USUM Studio");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fichier : %s", path.string().c_str());
        if (!error_.empty()) ImGui::TextWrapped("Erreur : %s", error_.c_str());
        if (!archive_) {
            ImGui::Separator();
            ImGui::TextWrapped("Ouvre une ROM extraite ou un projet, puis clique sur Charger / Actualiser.");
            ImGui::End(); return;
        }

        const int max_id = static_cast<int>(std::min<std::size_t>(archive_->size(), 2147483647));
        ImGui::SameLine();
        if (ImGui::Button("Integrer les changements au projet###personal-export")) export_archive(path);
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
        ImGui::Separator();
        ImGui::Text("%d entrees chargees  |  %zu modification(s) non exportee(s)", max_id, edits_.size());
        if (max_id == 0) { ImGui::End(); return; }
        ImGui::BeginChild("##personal-details", ImVec2(0, 0), true);
        ImGui::Text("Personal Data #%d", selected_);
        if (edits_.contains(selected_)) { ImGui::SameLine(); ImGui::TextDisabled("[MODIFIE]"); }
        ImGui::SameLine(); ImGui::TextDisabled("%zu octets", bytes_.size());
        ImGui::Separator();
        if (ImGui::Button("< Precedent") && selected_ > 0) select(selected_ - 1);
        ImGui::SameLine();
        if (ImGui::Button("Suivant >") && selected_ + 1 < max_id) select(selected_ + 1);
        ImGui::SameLine();
        ImGui::TextDisabled("Index %d / %d", selected_, max_id - 1);
        if (bytes_.size() >= 8) {
            if (ImGui::BeginTabBar("##personal-tabs")) {
                if (ImGui::BeginTabItem("Statistiques")) {
                    static const char *labels[] = {"PV", "Attaque", "Defense", "Vitesse", "Attaque speciale", "Defense speciale"};
                    ImGui::Spacing();
                    for (int i = 0; i < 6; ++i) {
                        ImGui::PushID(i);
                        ImGui::Text("%-19s  %3u", labels[i], unsigned(bytes_[std::size_t(i)]));
                        ImGui::SameLine(260.f);
                        ImGui::ProgressBar(float(bytes_[std::size_t(i)]) / 255.f,
                                           ImVec2(-1, 13), "");
                        ImGui::PopID();
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText("Edition des statistiques");
                    ImGui::TextDisabled("Valeurs en memoire ; seules les copies exportees sont ecrites.");
                    for (int i = 0; i < 6; ++i) {
                        int value = int(bytes_[std::size_t(i)]);
                        ImGui::PushID(100 + i);
                        if (ImGui::InputInt(labels[i], &value)) {
                            bytes_[std::size_t(i)] = std::uint8_t(std::clamp(value, 0, 255));
                            remember_edit();
                        }
                        ImGui::PopID();
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText("Types");
                    static const char *types[] = {"Normal", "Combat", "Vol", "Poison", "Sol", "Roche", "Insecte", "Spectre", "Acier", "Feu", "Eau", "Plante", "Electrik", "Psy", "Glace", "Dragon", "Tenebres", "Fee"};
                    ImGui::Text("Type principal : %s", bytes_[6] < 18 ? types[bytes_[6]] : "Inconnu");
                    ImGui::Text("Type secondaire : %s", bytes_[7] < 18 ? types[bytes_[7]] : "Inconnu");
                    for (int i = 6; i <= 7; ++i) {
                        int type = bytes_[std::size_t(i)] < 18 ? int(bytes_[std::size_t(i)]) : 0;
                        ImGui::PushID(i);
                        if (ImGui::Combo(i == 6 ? "Type 1" : "Type 2", &type, types, 18)) {
                            bytes_[std::size_t(i)] = std::uint8_t(type);
                            remember_edit();
                        }
                        ImGui::PopID();
                    }
                    if (edits_.contains(selected_) && ImGui::Button("Annuler modifications de cette entree")) {
                        edits_.erase(selected_);
                        load_entry();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Diagnostic 0-9-4")) {
                    ImGui::TextWrapped("Controle en lecture seule : correspondance espece / forme entre Personal Data et ressources 3D.");
                    ImGui::Text("Espece : %d | Forme : %d", species_pick_, form_pick_);
                    if (ImGui::Button("Analyser 0/9/4###pd-diagnostic")) {
                        run_model_diagnostic(dump);
                    }
                    ImGui::Separator();
                    ImGui::TextWrapped("Audit global en lecture seule : les resultats sont enregistres dans un fichier texte, sans modifier les archives.");
                    if (ImGui::Button("Verifier toutes les formes###pd-audit-all"))
                        run_global_form_audit(dump);
                    if (!audit_status_.empty())
                        ImGui::TextWrapped("%s", audit_status_.c_str());
                    ImGui::Separator();
                    if (!model_diag_.empty())
                        ImGui::TextWrapped("%s", model_diag_.c_str());
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Formes et references")) {
                    ImGui::TextDisabled("Structure USUM (84 octets) - pointeurs en lecture seule");
                    if (bytes_.size() < 0x21) {
                        ImGui::TextDisabled("Entree insuffisante pour lire les pointeurs.");
                    } else {
                        const unsigned raw_pointer = word(bytes_, 0x1C);
                        const unsigned sprite = word(bytes_, 0x1E);
                        const unsigned count = bytes_[0x20];
                        ImGui::Text("Index Personal actuel : %d", selected_);
                        ImGui::Text("FormStatsIndex (0x1C) : %u", raw_pointer);
                        ImGui::Text("FormeSprite (0x1E) : %u", sprite);
                        ImGui::Text("FormeCount (0x20) : %u", count);
                        ImGui::SeparatorText("Correspondances de formes");
                        if (selected_ >= 0 && selected_ < int(owners_.size()) && !owners_[selected_].empty()) {
                            for (const auto &owner : owners_[selected_])
                                ImGui::BulletText("Espece %d - forme %d", owner.species, owner.form);
                        } else if (selected_ < base_species_count_) {
                            ImGui::Text("Espece de base : %d (forme 0)", selected_);
                        } else {
                            ImGui::TextDisabled("Aucune espece associee par les pointeurs lus.");
                        }
                        if (selected_ >= 0 && selected_ < base_species_count_ && count > 1) {
                            ImGui::SeparatorText("Formes declarees par cette espece");
                            if (raw_pointer == 0) {
                                ImGui::TextDisabled("ATTENTION : aucune entree de formes (pointeur nul).");
                            } else {
                                const int start = resolved_form_start(selected_, int(raw_pointer));
                                if (start != int(raw_pointer))
                                    ImGui::TextDisabled("Decalage Expansion Pack applique : +%d", start - int(raw_pointer));
                                for (int form = 1; form < int(count); ++form) {
                                    const int target = start + form - 1;
                                    if (target >= 0 && target < max_id) {
                                        ImGui::PushID(form);
                                        char button[96];
                                        std::snprintf(button, sizeof(button), "Forme %d : entree %d", form, target);
                                        if (ImGui::SmallButton(button)) select(target);
                                        ImGui::PopID();
                                    } else {
                                        ImGui::TextColored(ImVec4(1.f, .42f, .3f, 1.f),
                                            "Forme %d : index %d hors archive !", form, target);
                                    }
                                }
                            }
                        }
                        ImGui::Separator();
                        ImGui::TextWrapped("FormeSprite n'est pas un pointeur direct vers un modele 3D. "
                                           "L'association au GARC 0/9/4 reste a verifier separement.");
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Hexadecimal")) {
                    ImGui::BeginChild("##personal-hex", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                    for (std::size_t i = 0; i < bytes_.size(); i += 16) {
                        char line[160];
                        int n = std::snprintf(line, sizeof(line), "%06zX  ", i);
                        for (std::size_t j = 0; j < 16; ++j)
                            n += std::snprintf(line + n, sizeof(line) - std::size_t(n),
                                i + j < bytes_.size() ? "%02X " : "   ",
                                i + j < bytes_.size() ? unsigned(bytes_[i + j]) : 0u);
                        ImGui::TextUnformatted(line);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        } else {
            ImGui::TextDisabled("Cette entree est trop petite pour decoder les statistiques et les types.");
        }
        ImGui::EndChild();
        ImGui::End();
    }
    // Navigation shared by all Data-workspace editors. The parent Data browser
    // window supplies the docking, so no floating windows or nested left panel.
    void draw_browser() {
        if (!archive_) {
            ImGui::TextDisabled("Charge une archive Personal Data dans le panneau de droite.");
            return;
        }
        const int max_id = static_cast<int>(std::min<std::size_t>(archive_->size(), 2147483647));
        if (max_id <= 0) return;
        ImGui::SeparatorText("Navigateur Personal Data");
        ImGui::TextUnformatted("Selection par espece / forme");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("Espece###personal-species", &species_pick_))
            species_pick_ = std::clamp(species_pick_, 1, std::max(1, base_species_count_ - 1));
        if (species_pick_ > 0 && species_pick_ < base_species_count_) {
            const int count = form_count(species_pick_);
            form_pick_ = std::clamp(form_pick_, 0, std::max(0, count - 1));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("Forme###personal-form", form_pick_ == 0 ? "0 - Normale" : ("Forme " + std::to_string(form_pick_)).c_str())) {
                for (int form = 0; form < count; ++form) {
                    const std::string label = form == 0 ? "0 - Normale" : "Forme " + std::to_string(form);
                    if (ImGui::Selectable(label.c_str(), form_pick_ == form)) form_pick_ = form;
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Ouvrir espece / forme###personal-goto-form")) {
                const int index = personal_index(species_pick_, form_pick_);
                if (index >= 0 && index < max_id) select(index);
                else status_ = "Index de forme non valide dans cette archive.";
            }
        }
        ImGui::SeparatorText("Toutes les entrees");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##personal-search", "Filtrer par numero (ex. 172)", search_, sizeof(search_));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputInt("##personal-id", &requested_id_)) {
            requested_id_ = std::clamp(requested_id_, 0, max_id - 1);
            select(requested_id_);
        }
        ImGui::TextDisabled("Aller a l'index (0 a %d)", max_id - 1);
        ImGui::Separator();
        ImGui::BeginChild("##personal-entry-list", ImVec2(0, 0), false);
        // ImGuiListClipper conserve la fluidite meme avec plusieurs milliers d'entrees.
        // Un filtrage textuel sur l'index est volontairement utilise tant que la
        // correspondance espece/forme des entrees supplementaires n'est pas connue.
        if (!search_[0]) {
            ImGuiListClipper clipper;
            clipper.Begin(max_id);
            while (clipper.Step()) {
                for (int id = clipper.DisplayStart; id < clipper.DisplayEnd; ++id)
                    entry_row(id);
            }
        } else {
            for (int id = 0; id < max_id; ++id) {
                char number[24]; std::snprintf(number, sizeof(number), "%d", id);
                if (std::string(number).find(search_) != std::string::npos)
                    entry_row(id);
            }
        }
        ImGui::EndChild();

    }

private:
    void entry_row(int id) {
        char label[64];
        std::snprintf(label, sizeof(label), "%04d  -  Entree %d", id, id);
        if (ImGui::Selectable(label, selected_ == id)) select(id);
    }
    void select(int id) {
        if (selected_ == id) { requested_id_ = id; return; }
        selected_ = id; requested_id_ = id; load_entry();
    }
    void run_model_diagnostic(const std::filesystem::path &dump) {
        try {
            auto model_path = dump / TargetProfile::pokemon_archive;
            if (auto *project = project_store())
                model_path = project->source / TargetProfile::pokemon_archive;
            Archive pokemon(model_path);
            auto catalog = decode_pokemon_catalog(pokemon.decoded(0), pokemon.size(), {});
            const PokemonEntry *match = nullptr;
            unsigned graphical_count = 0;
            for (const auto &e : catalog) {
                if (int(e.species) != species_pick_ || e.female) continue;
                ++graphical_count;
                if (int(e.form) == form_pick_) match = &e;
            }
            const int pd_count = form_count(species_pick_);
            const int personal = personal_index(species_pick_, form_pick_);
            const bool valid_personal = personal >= 0 && archive_ &&
                                        personal < int(archive_->size());
            std::string report = "Archive modeles : " + model_path.string() + "\n";
            report += "Formes graphiques trouvees : " + std::to_string(graphical_count) +
                      " | FormCount Personal : " + std::to_string(pd_count) + "\n";
            report += "Personal Data : ";
            report += valid_personal ? "entree " + std::to_string(personal) : "ABSENTES / index invalide";
            report += "\n";
            if (!match) {
                report += "AUCUNE entree graphique pour cette espece et cette forme.\n";
            } else {
                const unsigned base = 1 + 9 * match->data_index;
                report += "Variante graphique : " + std::to_string(match->data_index) +
                          " | groupe de 9 membres : " + std::to_string(base) + " a " +
                          std::to_string(base + 8) + "\n";
                report += "Indices resolus (partage de ressources pris en compte) :\n";
                report += "Modele : " + std::to_string(match->model_member) +
                          " | Textures : " + std::to_string(match->texture_member) +
                          " | Animations : " + std::to_string(match->motion_member) + "\n";
                const bool valid_members = match->model_member < pokemon.size() &&
                                           match->texture_member < pokemon.size() &&
                                           match->motion_member < pokemon.size();
                report += valid_members ? "Indices graphiques dans les limites de l'archive.\n" :
                                          "ERREUR : un indice graphique depasse l'archive.\n";
                report += "Ce test ne valide pas encore le rendu en jeu ni chaque fichier interne.\n";
            }
            if (graphical_count != unsigned(pd_count))
                report += "ATTENTION : nombres de formes differents. Verifier aussi les variantes femelles et partagees.\n";
            if (match && valid_personal)
                report += "Correspondance espece / forme detectee dans les deux archives.\n";
            model_diag_ = std::move(report);
        } catch (const std::exception &e) {
            model_diag_ = std::string("Analyse impossible : ") + e.what();
        }
    }

    void run_global_form_audit(const std::filesystem::path &dump) {
        // Strictly read-only for game archives. Only a new .txt report is written.
        try {
            if (!archive_) throw std::runtime_error("Charge les Personal Data avant l'audit.");
            const auto *project = project_store();
            const auto model_path = project ? project->source / TargetProfile::pokemon_archive
                                            : dump / TargetProfile::pokemon_archive;
            Archive models(model_path);
            const auto catalog = decode_pokemon_catalog(models.decoded(0), models.size(), {});
            std::map<int, std::map<int, const PokemonEntry *>> graphics;
            std::map<int, int> female_variants;
            std::size_t invalid_graphic_members = 0;
            std::size_t graphical_duplicates = 0;
            for (const auto &entry : catalog) {
                const bool valid = entry.model_member < models.size() &&
                                   entry.texture_member < models.size() &&
                                   entry.motion_member < models.size() &&
                                   1ULL + 9ULL * entry.data_index + 8ULL < models.size();
                if (!valid) ++invalid_graphic_members;
                if (entry.female) { ++female_variants[int(entry.species)]; continue; }
                auto &forms = graphics[int(entry.species)];
                if (!forms.emplace(int(entry.form), &entry).second) ++graphical_duplicates;
            }
            std::ostringstream report;
            report << "USUM Studio - Audit global des formes (lecture seule)\n"
                   << "Source 0/1/7 : " << loaded_path_.string() << "\n"
                   << "Source 0/9/4 : " << model_path.string() << "\n"
                   << "Personal entries : " << archive_->size() << "\n"
                   << "Model archive members : " << models.size() << "\n"
                   << "Model catalog entries : " << catalog.size() << "\n\n"
                   << "IMPORTANT : des formes visuelles (sexe, partage de modeles) peuvent\n"
                   << "ne pas correspondre a une Personal Data distincte.\n"
                   << "Ce rapport ne valide pas le fonctionnement dans le jeu.\n\n";
            int serious = 0, warnings = 0, matching = 0;
            const int limit = std::min(base_species_count_, int(archive_->size()));
            for (int species = 1; species < limit; ++species) {
                const Bytes info = archive_->decoded(std::size_t(species));
                if (info.size() < 0x21) {
                    report << "ERREUR | Espece " << species << " : entree Personal trop courte ("
                           << info.size() << " octets)\n";
                    ++serious;
                    continue;
                }
                const int count = int(info[0x20]);
                const int pointer = int(word(info, 0x1C));
                const int first = resolved_form_start(species, pointer);
                const auto g = graphics.find(species);
                const int graphic_count = g == graphics.end() ? 0 : int(g->second.size());
                bool broken = count <= 0;
                if (count > 1 && pointer == 0) broken = true;
                if (count > 1 && (first < 0 ||
                    std::uint64_t(first) + std::uint64_t(count - 2) >= archive_->size())) broken = true;
                if (broken) {
                    report << "ERREUR | Espece " << species << " : pointeur " << pointer
                           << ", debut calcule " << first << ", FormCount " << count << "\n";
                    ++serious;
                    continue;
                }
                bool discrepancy = false;
                if (g == graphics.end() || !g->second.contains(0)) discrepancy = true;
                if (count != graphic_count) discrepancy = true;
                if (g != graphics.end()) {
                    for (int form = 0; form < count; ++form)
                        if (!g->second.contains(form)) discrepancy = true;
                    for (const auto &[form, entry] : g->second)
                        if (form >= count) discrepancy = true;
                }
                if (discrepancy) {
                    report << "A EXAMINER | Espece " << species << " : Personal " << count
                           << " forme(s), graphique " << graphic_count
                           << ", femelles supplementaires " << female_variants[species]
                           << ", pointeur " << pointer << "\n";
                    ++warnings;
                } else ++matching;
            }
            if (invalid_graphic_members) {
                report << "ERREUR | " << invalid_graphic_members
                       << " variantes graphiques ont au moins un index hors archive.\n";
                serious += int(invalid_graphic_members);
            }
            if (graphical_duplicates) {
                report << "A EXAMINER | " << graphical_duplicates
                       << " doublons graphiques espece/forme.\n";
                warnings += int(graphical_duplicates);
            }
            report << "\nBILAN : " << serious << " erreur(s) structurelle(s), " << warnings
                   << " ecart(s) a examiner, " << matching << " espece(s) correspondante(s).\n";
            const std::filesystem::path folder = project ? project->root / "scratch" :
                                                        dump / "usum-diagnostics";
            std::filesystem::create_directories(folder);
            const auto output = folder / "audit-formes-017-094.txt";
            std::ofstream out(output, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("Impossible de creer le rapport TXT.");
            const auto contents = report.str();
            out.write(contents.data(), std::streamsize(contents.size()));
            out.close();
            if (!out) throw std::runtime_error("Erreur pendant l'ecriture du rapport TXT.");
            audit_status_ = "Audit termine : " + std::to_string(serious) + " erreur(s), " +
                            std::to_string(warnings) + " ecart(s). Rapport : " + output.string();
        } catch (const std::exception &e) {
            audit_status_ = std::string("Audit impossible : ") + e.what();
        }
    }

    void load(const std::filesystem::path &path) {
        archive_.reset(); bytes_.clear(); owners_.clear(); edits_.clear(); error_.clear(); status_.clear(); selected_ = 0; requested_id_ = 0;
        try {
            archive_ = std::make_unique<Archive>(path);
            loaded_path_ = path;
            rebuild_form_links();
            if (archive_->size()) load_entry();
        } catch (const std::exception &e) { archive_.reset(); error_ = e.what(); }
    }
    void load_entry() {
        bytes_.clear(); error_.clear();
        try { if (archive_) {
            auto it = edits_.find(selected_);
            bytes_ = it != edits_.end() ? it->second : archive_->decoded(std::size_t(selected_));
        } }
        catch (const std::exception &e) { error_ = e.what(); }
    }
    void remember_edit() { if (bytes_.size() == 0x54) edits_[selected_] = bytes_; }
    int form_count(int species) const {
        try {
            if (species < 0 || species >= base_species_count_ || !archive_) return 1;
            const Bytes b = archive_->decoded(std::size_t(species));
            return b.size() > 0x20 ? std::clamp(int(b[0x20]), 1, 255) : 1;
        } catch (...) { return 1; }
    }
    int personal_index(int species, int form) const {
        if (!archive_ || species < 0 || species >= base_species_count_) return -1;
        if (form == 0) return species;
        try {
            const Bytes b = archive_->decoded(std::size_t(species));
            if (b.size() < 0x21 || form >= int(b[0x20])) return -1;
            const int pointer = int(word(b, 0x1C));
            return pointer == 0 ? -1 : resolved_form_start(species, pointer) + form - 1;
        } catch (...) { return -1; }
    }
    void export_archive(const std::filesystem::path &source) {
        if (!archive_) { status_ = "Charge d'abord une archive."; return; }
        if (edits_.empty()) { status_ = "Aucune modification a exporter."; return; }
        if (loaded_path_ != source) {
            status_ = "Archive externe ouverte : reviens aux donnees du projet avant d'integrer des modifications.";
            return;
        }
        try {
            std::map<std::size_t, Bytes> changes;
            for (const auto &[id, value] : edits_) {
                if (value.size() != 0x54) throw std::runtime_error("Personal Data invalide : taille differente de 84 octets.");
                const Bytes original_raw = archive_->raw(std::size_t(id));
                const Bytes original_decoded = archive_->decoded(std::size_t(id));
                if (original_raw != original_decoded)
                    throw std::runtime_error("Entree compressee : export direct refuse pour securite.");
                changes[std::size_t(id)] = value;
            }
            const auto *project = project_store();
            if (!project) {
                status_ = "Ouvre un projet USUM Studio pour exporter dans ses exports classiques.";
                return;
            }
            const auto output = project->root / "scratch" / "personal-data-editor.garc";
            std::filesystem::create_directories(output.parent_path());
            archive_->export_to(output, changes, true);
            // Imports the archive into the project's managed source. The regular
            // Stage / Export workflow then publishes it to the exports folder.
            project_store()->import_file(GameProfile::personal_archive, output, false);
            edits_.clear();
            archive_.reset();
            load(project_store()->source / GameProfile::personal_archive);
            external_source_ = false;
            observed_revision_ = personal_data_revision;
            notify_personal_data_changed();
            observed_revision_ = personal_data_revision;
            status_ = "Personal Data integrees au projet. Utilise la commande Exporter du projet pour generer exports/.";
        } catch (const std::exception &e) { status_ = std::string("Echec export : ") + e.what(); }
    }
    struct FormOwner { int species; int form; };
    static unsigned word(const Bytes &value, std::size_t offset) {
        return offset + 1 < value.size()
            ? unsigned(value[offset]) | (unsigned(value[offset + 1]) << 8) : 0u;
    }
    int resolved_form_start(int species, int pointer) const {
        // Regle observee dans pk3DS Expansion : les anciennes formes USUM
        // (808..1025) sont decalees apres les nouvelles especes.
        if (base_species_count_ > 808 && species <= 807 &&
            pointer >= 808 && pointer < 1026)
            return pointer + (base_species_count_ - 808);
        return pointer;
    }
    void rebuild_form_links() {
        if (!archive_) return;
        const std::size_t total = archive_->size();
        owners_.resize(total);
        // Le fork pk3DS utilise MaxSpecies=1025 (Expansion) ou 807 (USUM).
        // Ici ce seuil est une hypothese deduite de la taille, non une lecture
        // d'une configuration du patch du jeu.
        base_species_count_ = total >= 1026 ? 1026 : 808;
        base_species_count_ = std::min(base_species_count_, int(total));
        for (int species = 0; species < base_species_count_; ++species) {
            try {
                const Bytes info = archive_->decoded(std::size_t(species));
                if (info.size() < 0x21) continue;
                const int count = int(info[0x20]);
                const int pointer = int(word(info, 0x1C));
                if (count <= 1 || pointer <= 0) continue;
                const int start = resolved_form_start(species, pointer);
                for (int form = 1; form < count; ++form) {
                    const int index = start + form - 1;
                    if (index >= 0 && index < int(total))
                        owners_[std::size_t(index)].push_back({species, form});
                }
            } catch (const std::exception &) {
                // Une entree invalide n'empeche pas d'explorer les autres.
            }
        }
    }
    std::filesystem::path loaded_path_;
    std::uint64_t observed_revision_ = 0;
    bool external_source_ = false;
    bool show_external_path_ = false;
    char external_path_[2048]{};
    std::unique_ptr<Archive> archive_;
    std::vector<std::vector<FormOwner>> owners_;
    int base_species_count_ = 808;
    Bytes bytes_;
    std::map<int, Bytes> edits_;
    std::string status_;
    std::string model_diag_;
    std::string audit_status_;
    int species_pick_ = 3;
    int form_pick_ = 0;
    std::string error_;
    int selected_ = 0;
    int requested_id_ = 0;
    char search_[64] = {};
};
} // namespace studio
