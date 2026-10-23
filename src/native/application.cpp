#include "native/model_folders.h"
#include <string_view>
#include "native/feature_visibility.h"
#include "native/application.h"
#include "native/activity_log.h"
#include "scene/resource_reuse.h"
#include "native/undo_shortcuts.h"
#include "native/field_system_inspector.h"
#include "native/map_workspace.h"
#include "native/tutorial_widgets.h"
#include "native/tutorial_controller.h"
#include "native/project_workspace.h"
#include "native/overworld_editor.h"
#include "native/related_data_inspector.h"
#include "native/encounter_editor.h"
#include "native/map_cursor.h"
#include "native/camera_editor.h"
#include "native/collision_editor.h"
#include "native/cry_editor.h"
#include "native/audio_editor.h"
#include "native/warp_ride_workspace.h"
#include "native/image_editor.h"
#include "native/map_music.h"
#include "native/theme.h"
#include "native/renderer.h"
#include "native/model_workspace.h"
#include "native/model_aliases.h"
#include "native/material_inspector.h"
#include "native/scene_browser.h"
#include "native/placement_editor.h"
#include "native/map_authoring_workspace.h"
#include "native/interaction_inspector.h"
#include "native/conversation_editor.h"
#include "native/warp_editor.h"
#include "native/weather_editor.h"
#include "native/zone_editor.h"
#include "native/pickup_editor.h"
#include "native/map_selector.h"
#include "native/archive_picker.h"
#include "native/imgui_renderer.h"
#include "native/preferences.h"
#include "native/localization.h"
#include "native/personal_data_viewer.h"
#include "native/item_data_viewer.h"
#include "native/move_data_viewer.h"
#include "native/learnset_data_viewer.h"
#include "native/text_data_viewer.h"
#include "native/evolution_data_viewer.h"
#include "native/encounter_data_viewer.h"
#include "native/trainer_data_viewer.h"
#include "native/extra_data_viewer.h"
#include "native/frame_pacing.h"
#include "native/camera.h"
#include "native/folder_picker.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_sdl3.h>
#include "native/configurable_keys.h"
#include <bx/math.h>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <set>
#include <sstream>
#include <string_view>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <functional>
#include <thread>
namespace {
// Beta 0.2: project-independent, persistent keyboard bindings.
struct KeyBinding {
    const char *id;
    const char *english;
    const char *french;
    ImGuiKey default_key;
    ImGuiKey key;
};
static KeyBinding bindings[] = {
    {"player.forward", "Forward", "Avancer", ImGuiKey_W, ImGuiKey_W},
    {"player.backward", "Backward", "Reculer", ImGuiKey_S, ImGuiKey_S},
    {"player.left", "Move left", "Aller à gauche", ImGuiKey_A, ImGuiKey_A},
    {"player.right", "Move right", "Aller à droite", ImGuiKey_D, ImGuiKey_D},
    {"player.restart", "Restart playtest", "Recommencer le test", ImGuiKey_R, ImGuiKey_R},
    {"player.exit", "Exit playtest", "Quitter le test", ImGuiKey_Escape, ImGuiKey_Escape},
    {"camera.forward", "Camera forward", "Caméra : avancer", ImGuiKey_W, ImGuiKey_W},
    {"camera.backward", "Camera backward", "Caméra : reculer", ImGuiKey_S, ImGuiKey_S},
    {"camera.left", "Camera left", "Caméra : gauche", ImGuiKey_A, ImGuiKey_A},
    {"camera.right", "Camera right", "Caméra : droite", ImGuiKey_D, ImGuiKey_D},
    {"camera.up", "Camera up", "Caméra : monter", ImGuiKey_E, ImGuiKey_E},
    {"camera.down", "Camera down", "Caméra : descendre", ImGuiKey_Q, ImGuiKey_Q},
    {"maps.place_move", "Move placed object", "Déplacer un objet", ImGuiKey_G, ImGuiKey_G},
    {"maps.place_rotate", "Rotate placed object", "Pivoter un objet", ImGuiKey_R, ImGuiKey_R},
    {"maps.warp_move", "Move warp", "Déplacer un téléporteur", ImGuiKey_G, ImGuiKey_G},
    {"maps.warp_rotate", "Rotate warp", "Pivoter un téléporteur", ImGuiKey_R, ImGuiKey_R},
    {"maps.warp_scale", "Scale warp", "Redimensionner un téléporteur", ImGuiKey_S, ImGuiKey_S},
    {"maps.encounter_move", "Move encounter", "Déplacer une rencontre", ImGuiKey_G, ImGuiKey_G},
    {"maps.encounter_rotate", "Rotate encounter", "Pivoter une rencontre", ImGuiKey_R, ImGuiKey_R},
    {"maps.encounter_scale", "Scale encounter", "Redimensionner une rencontre", ImGuiKey_S, ImGuiKey_S},
    {"maps.shape_move", "Move map shape", "Déplacer une zone", ImGuiKey_G, ImGuiKey_G},
    {"maps.shape_scale", "Scale map shape", "Redimensionner une zone", ImGuiKey_S, ImGuiKey_S},
    {"models.geometry_move", "Move geometry", "Déplacer la géométrie", ImGuiKey_G, ImGuiKey_G},
    {"models.geometry_rotate", "Rotate geometry", "Pivoter la géométrie", ImGuiKey_R, ImGuiKey_R},
    {"models.geometry_scale", "Scale geometry", "Redimensionner la géométrie", ImGuiKey_S, ImGuiKey_S},
    {"models.box_select", "Box selection", "Sélection rectangulaire", ImGuiKey_B, ImGuiKey_B},
    {"models.select_vertices", "Vertex selection", "Sélection des sommets", ImGuiKey_1, ImGuiKey_1},
    {"models.select_edges", "Edge selection", "Sélection des arêtes", ImGuiKey_2, ImGuiKey_2},
    {"models.select_faces", "Face selection", "Sélection des faces", ImGuiKey_3, ImGuiKey_3},
    {"models.frame", "Frame model", "Cadrer le modèle", ImGuiKey_F, ImGuiKey_F},
    {"uv.move", "Move UV", "Déplacer les UV", ImGuiKey_G, ImGuiKey_G},
    {"uv.rotate", "Rotate UV", "Pivoter les UV", ImGuiKey_R, ImGuiKey_R},
    {"uv.scale", "Scale UV", "Redimensionner les UV", ImGuiKey_S, ImGuiKey_S},
    {"uv.frame", "Frame UV", "Cadrer les UV", ImGuiKey_F, ImGuiKey_F},
    {"collision.select_1", "Collision selection mode 1", "Collision : sélection 1", ImGuiKey_1, ImGuiKey_1},
    {"collision.select_2", "Collision selection mode 2", "Collision : sélection 2", ImGuiKey_2, ImGuiKey_2},
    {"collision.move", "Move collision", "Déplacer la collision", ImGuiKey_G, ImGuiKey_G},
    {"collision.rotate", "Rotate collision", "Pivoter la collision", ImGuiKey_R, ImGuiKey_R},
    {"collision.box", "Box select collision", "Sélection rectangulaire collision", ImGuiKey_B, ImGuiKey_B},
    {"collision.frame", "Frame collision", "Cadrer la collision", ImGuiKey_F, ImGuiKey_F},

};
static bool binding_down(int i) { return ImGui::IsKeyDown(bindings[i].key); }
static bool binding_pressed(int i) { return ImGui::IsKeyPressed(bindings[i].key); }
struct Camera : studio::ViewportCamera {
    using studio::ViewportCamera::fit;
    bool looking = false;
    float look_x = 0, look_y = 0, cursor_x = 0, cursor_y = 0;
    void fit(const studio::Environment &s) {
        studio::ViewportCamera::fit(s.low, s.high);
    }
    void stop_look(SDL_Window *window) {
        if (looking) {
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
                SDL_WarpMouseInWindow(window, cursor_x, cursor_y);
            SDL_SetWindowRelativeMouseMode(window, false);
        }
        looking = false;
        look_x = look_y = 0;
    }
    void controls(SDL_Window *window, bool hovered) {
        auto &io = ImGui::GetIO();
        bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        if (!focused || !ImGui::IsMouseDown(ImGuiMouseButton_Right))
            stop_look(window);
        if (focused && hovered && !io.WantTextInput &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            SDL_GetMouseState(&cursor_x, &cursor_y);
            looking = SDL_SetWindowRelativeMouseMode(window, true);
            look_x = look_y = 0;
        }
        if (looking) {
            rotate(look_x, look_y, true);
            look_x = look_y = 0;
        }
        if (!focused || (!hovered && !looking) || io.WantTextInput)
            return;
        if (!looking) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                rotate(io.MouseDelta.x, io.MouseDelta.y, false);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                pan(io.MouseDelta.x, io.MouseDelta.y);
        }
        if (!(io.KeyCtrl && io.KeyShift) && (!ImGui::IsAnyItemActive() || looking)) {
            auto key = [](ImGuiKey k) {
                return ImGui::IsKeyDown(k) ? 1.f : 0.f;
            };
            fly(key(bindings[9].key) - key(bindings[8].key),
                key(bindings[10].key) - key(bindings[11].key),
                key(bindings[6].key) - key(bindings[7].key), io.DeltaTime,
                io.KeyShift  ? .25f
                : io.KeyCtrl ? fast_multiplier
                             : 1.f);
        }
        if (io.KeyCtrl)
            adjust_fast_speed(io.MouseWheel);
        else
            wheel(io.MouseWheel, looking);
    }
    void matrices(float *view, float *proj, float aspect) {
        auto p = eye();
        bx::mtxLookAt(view, {p[0], p[1], p[2]}, {target[0], target[1], target[2]}, {0, 1, 0},
                      bx::Handedness::Right);
        bx::mtxProj(proj, 45, aspect, near_clip(), far_clip(), bgfx::getCaps()->homogeneousDepth,
                    bx::Handedness::Right);
    }
};
void simplify_dock_tabs(ImGuiDockNode *node) {
    if (!node)
        return;
    node->SetLocalFlags(node->LocalFlags | ImGuiDockNodeFlags_NoWindowMenuButton);
    for (auto *child : node->ChildNodes)
        simplify_dock_tabs(child);
}
int workspace_selector(int selected, float reserved_width) {
    const char *names[] = {"Maps", "Models", "Studio", "Authoring", "Collision",
                           "Cameras", "Images", "Audio", "Warp Ride", "Data"};
    const char *translated[10];
    for (int i = 0; i < 10; ++i)
        translated[i] = studio::localization::tr(names[i]);
    const auto &style = ImGui::GetStyle();
    float available = ImGui::GetWindowWidth() - reserved_width - style.WindowPadding.x * 2;
    if (available < ImGui::GetFontSize() * 54 + style.ItemSpacing.x * 8) {
        ImGui::SetNextItemWidth(std::max(
            80.f, std::min(ImGui::GetFontSize() * 14, available - style.ItemSpacing.x * 2)));
        bool open = ImGui::BeginCombo("##workspace-navigation", translated[selected]);
        if (!open)
            studio::TutorialWidgets::item("main", "Workspace");
        if (open) {
            for (int i = 0; i < 10; ++i)
                if (ImGui::Selectable(translated[i], selected == i))
                    selected = i;
            ImGui::EndCombo();
        }
        return selected;
    }
    ImGui::PushID("workspace-navigation");
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetStyle().FrameRounding * 3);
    for (int i = 0; i < 10; ++i) {
        if (i)
            ImGui::SameLine();
        bool active = selected == i;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? ImVec4(.16f, .43f, .60f, 1) : ImVec4(.14f, .22f, .28f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.23f, .49f, .65f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.12f, .34f, .48f, 1));
        if (studio::TutorialWidgets::Button("main", translated[i], {ImGui::GetFontSize() * 6, 0}))
            selected = i;
        if (active) {
            auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
            float inset = ImGui::GetFontSize() * .6f;
            ImGui::GetWindowDrawList()->AddLine({low.x + inset, high.y - 3},
                                                {high.x - inset, high.y - 3},
                                                ImGui::GetColorU32(ImGuiCol_CheckMark), 2);
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return selected;
}

void layout(int workspace, bool reset) {
    auto *viewport = ImGui::GetMainViewport();
    const char *names[] = {"Map workspace layout 3",       "Model workspace layout 2",
                           "Studio workspace layout 2",    "Authoring workspace layout 3",
                           "Collision workspace layout 2", "Camera workspace layout 2",
                           "Image workspace layout 2",     "Audio workspace layout 2",
                           "Warp Ride workspace layout 2", "Data workspace layout 4"};
    ImGuiID ids[10];
    for (unsigned i = 0; i < 10; ++i)
        ids[i] = ImHashStr(names[i]);
    auto id = ids[workspace];
    bool create = reset || !ImGui::DockBuilderGetNode(id);
    for (unsigned i = 0; i < 10; ++i)
        if (i != unsigned(workspace) && ImGui::DockBuilderGetNode(ids[i]))
            ImGui::DockSpace(ids[i], {0, 0}, ImGuiDockNodeFlags_KeepAliveOnly);
    ImGui::DockSpaceOverViewport(id, viewport);
    simplify_dock_tabs(ImGui::DockBuilderGetNode(id));
    if (!create)
        return;
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, viewport->WorkSize);
    ImGuiID center = id;
    const float left_fraction =
        workspace != 2
            ? std::clamp((workspace == 0 ? 260.f : 280.f) / viewport->WorkSize.x, .13f, .24f)
            : .24f;
    if (workspace == 9) {
        // Same arrangement as Models: editor selector left, editor content right.
        auto browser = ImGui::DockBuilderSplitNode(
            id, ImGuiDir_Left,
            std::clamp(255.f / viewport->WorkSize.x, .13f, .25f),
            nullptr, &center);
        ImGui::DockBuilderDockWindow("Data browser", browser);
        ImGui::DockBuilderDockWindow("Personal Data", center);
        ImGui::DockBuilderDockWindow("Item Data", center);
        ImGui::DockBuilderDockWindow("Text Data", center);
        ImGui::DockBuilderDockWindow("Evolution Data", center);
        ImGui::DockBuilderDockWindow("Encounter Data", center);
        ImGui::DockBuilderDockWindow("Move Data", center);
        ImGui::DockBuilderDockWindow("Learnset Data", center);
        ImGui::DockBuilderFinish(id);
        return;
    }
    auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, left_fraction, nullptr, &center);
    const float right_fraction = workspace != 2
                                     ? std::clamp((workspace == 0 ? 310.f : 330.f) /
                                                      (viewport->WorkSize.x * (1 - left_fraction)),
                                                  .2f, .37f)
                                     : .29f;
    auto right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, right_fraction, nullptr, &center);
    if (workspace == 8) {
        ImGui::DockBuilderDockWindow("Warp Ride preview", center);
        ImGui::DockBuilderDockWindow("Warp Ride assets", left);
        ImGui::DockBuilderDockWindow("Warp Ride controls", right);
    } else if (workspace == 6) {
        ImGui::DockBuilderDockWindow("Image preview", center);
        ImGui::DockBuilderDockWindow("Image library", left);
        ImGui::DockBuilderDockWindow("Image properties", right);
    } else if (workspace == 7) {
        ImGui::DockBuilderDockWindow("Audio studio", center);
        ImGui::DockBuilderDockWindow("Audio library", left);
        ImGui::DockBuilderDockWindow("Audio inspector", right);
    } else if (workspace == 5) {
        ImGui::DockBuilderDockWindow("Camera viewport", center);
        ImGui::DockBuilderDockWindow("Camera regions", left);
        ImGui::DockBuilderDockWindow("Camera editing", right);
    } else if (workspace == 4) {
        ImGui::DockBuilderDockWindow("Collision viewport", center);
        ImGui::DockBuilderDockWindow("Collision layers", left);
        ImGui::DockBuilderDockWindow("Collision properties", right);
    } else if (workspace == 3) {
        ImGui::DockBuilderDockWindow("Authoring viewport", center);
        ImGui::DockBuilderDockWindow("Composition", left);
        ImGui::DockBuilderDockWindow("Asset Library", left);
        ImGui::DockBuilderDockWindow("Authoring tools", right);
    } else if (workspace == 2) {
        ImGui::DockBuilderDockWindow("Studio viewport", center);
        ImGui::DockBuilderDockWindow("Studio materials", left);
        ImGui::DockBuilderDockWindow("Studio inspector", right);
    } else if (workspace == 1) {
        ImGui::DockBuilderDockWindow("Model viewport", center);
        ImGui::DockBuilderDockWindow("Model browser", left);
        ImGui::DockBuilderDockWindow("Model inspector", right);
    } else {
        ImGui::DockBuilderDockWindow("Map viewport", center);
        ImGui::DockBuilderDockWindow("Map inspector", right);
        ImGui::DockBuilderDockWindow("Map browser", left);
    }
    ImGui::DockBuilderFinish(id);
    simplify_dock_tabs(ImGui::DockBuilderGetNode(id));
}

int run(SDL_Window *window, int argc, char **argv, studio::ApplicationObserver *observer) {
    struct PlacementReloadContext {
        std::filesystem::path project;
        int area = -1;
        studio::OverworldReference placement;
        studio::ViewportCamera camera;
    };
    static std::optional<PlacementReloadContext> placement_reload;
    auto integer = [](const std::string &value) {
        int result = 0;
        auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        studio::require(error == std::errc{} && end == value.data() + value.size() && result >= 0,
                        "Expected a nonnegative integer");
        return result;
    };
    auto *pref_path = SDL_GetPrefPath("USUMStudio", "USUMStudio");
    auto preference_root = pref_path ? std::filesystem::u8path(pref_path) : std::filesystem::path{};
    studio::Preferences preferences(preference_root.empty() ? std::filesystem::path{}
                                                            : preference_root / "preferences.cfg");
    SDL_free(pref_path);
    studio::model_aliases().configure(preference_root);
    studio::model_folders().configure(preference_root);
    studio::localization::set_language(preferences.get("language"));
    auto load_visibility = [&](const char *key) { return preferences.get(key) != "0"; };
    studio::feature_visibility::folders = load_visibility("features.model_folders");
    studio::feature_visibility::universal_search = load_visibility("features.universal_search");
    studio::feature_visibility::favorites = load_visibility("features.model_favorites");
    studio::feature_visibility::renaming = load_visibility("features.model_renaming");
    studio::feature_visibility::model_ids = preferences.get("interface.model_ids") == "1";
    bool show_console_notifications = preferences.get("interface.console_notifications") != "0";
    for (auto &binding : bindings) {
        auto saved = preferences.get(std::string("keybind.") + binding.id);
        if (!saved.empty()) {
            int number = 0;
            auto parsed = std::from_chars(saved.data(), saved.data() + saved.size(), number);
            if (parsed.ec == std::errc{} && parsed.ptr == saved.data() + saved.size() &&
                number >= int(ImGuiKey_NamedKey_BEGIN) && number < int(ImGuiKey_NamedKey_END))
                binding.key = ImGuiKey(number);
        }
    }
    for (const auto &binding : bindings)
        studio::configurable_keys::set(binding.id, binding.key);
    bool show_app_preferences = false;
    int key_settings_category = 0;
    int capturing_binding = -1;
    bool skip_capture_frame = false;
    studio::FramePacing pacing;
    auto saved_flag = [&](const char *key, bool fallback) {
        auto value = preferences.get(key);
        return value.empty() ? fallback : value == "1";
    };
    auto saved_rate = [&](const char *key, int fallback) {
        try {
            auto value = preferences.get(key);
            return value.empty() ? fallback : std::clamp(integer(value), 1, 1000);
        } catch (const std::exception &) {
            return fallback;
        }
    };
    studio::ImGuiRenderer::preview_3ds = saved_flag("preview_3ds", false);
    studio::ImGuiRenderer::preview_native_size = saved_flag("preview_native_size", false);
    pacing.limit_enabled = saved_flag("fps_limit_enabled", false);
    pacing.background_enabled = saved_flag("background_limit_enabled", true);
    pacing.fps = saved_rate("fps_limit", 60);
    pacing.background_fps = saved_rate("background_fps", 15);
    auto save_pacing = [&]() {
        preferences.set("fps_limit_enabled", pacing.limit_enabled ? "1" : "0");
        preferences.set("fps_limit", std::to_string(pacing.fps));
        preferences.set("background_limit_enabled", pacing.background_enabled ? "1" : "0");
        preferences.set("background_fps", std::to_string(pacing.background_fps));
        preferences.save();
    };
    bool requested_battle = false;
    std::string pokemon_settings;
    bool camera_workspace = false, audio_workspace = false, image_workspace = false;
    bool warp_ride_workspace = false, data_workspace = false;
    std::string camera_patch;
    std::string dump = preferences.get("dump"), font_path, shader_directory, frame_material,
                camera_pose, overlay_mode;
    float light_hour = 12;
    double animation_time = 0;
    bool animation_playing = true;
    float animation_speed = 1;
    studio::ArchiveSources source_archives;
    int area = 0, requested_weather = -1;
    bool cutaway = false;
    float cut_height = 0;
    bgfx::RendererType::Enum backend = bgfx::RendererType::OpenGL;
    if (!preferences.get("area").empty())
        try {
            area = integer(preferences.get("area"));
        } catch (const std::exception &) {
            preferences.error = "Saved area is invalid";
        }
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--project") {
            studio::require(i + 1 < argc, "Missing project file");
            auto file = std::filesystem::absolute(argv[++i]);
            std::ifstream input(file);
            studio::require(bool(input), "Cannot open project file");
            std::string line;
            std::set<std::string> keys;
            while (std::getline(input, line)) {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty() || line[0] == '#')
                    continue;
                auto equal = line.find('=');
                studio::require(equal != std::string::npos, "Project entries must use key=value");
                auto key = line.substr(0, equal), value = line.substr(equal + 1);
                studio::require(keys.insert(key).second, "Repeated project setting");
                if (key == "dump" || key == "font" || key == "field_archive" ||
                    key == "terrain_archive" || key == "pokemon_archive") {
                    auto path = std::filesystem::path(value);
                    auto resolved = value.empty()
                                        ? std::string{}
                                        : (path.is_absolute() ? path : file.parent_path() / path)
                                              .lexically_normal()
                                              .string();
                    if (key == "dump")
                        dump = resolved;
                    else if (key == "font")
                        font_path = resolved;
                    else if (key == "field_archive")
                        source_archives.field = resolved;
                    else if (key == "terrain_archive")
                        source_archives.terrain = resolved;
                    else
                        source_archives.pokemon = resolved;
                } else if (key == "area")
                    area = integer(value);
            }
        }
    std::string project_directory;
    bool explicit_source = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project-directory" && i + 1 < argc)
            project_directory = argv[++i];
        else if (arg == "--dump" || arg == "--project")
            explicit_source = true;
    }
    std::string placement_patch, composition_path, collision_patch;
    bool requested_collision = false;
    bool requested_authoring = false;
    int requested_player = -1, requested_pokemon = -1, requested_form = 0;
    bool model_shiny = false, model_female = false, requested_studio = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--warp-ride") {
            warp_ride_workspace = true;
            continue;
        }
        if (arg == "--images") {
            image_workspace = true;
            continue;
        }
        if (arg == "--cameras") {
            camera_workspace = true;
            continue;
        }
        if (arg == "--collision")
            requested_collision = true;
        else if (arg == "--authoring")
            requested_authoring = true;
        else if (arg == "--audio") {
            audio_workspace = true;
            continue;
        } else if (arg == "--vulkan")
            backend = bgfx::RendererType::Vulkan;
        else if (arg == "--studio")
            requested_studio = true;
        else if (arg == "--battle-preview")
            requested_battle = true;
        else if (arg == "--shiny")
            model_shiny = true;
        else if (arg == "--female")
            model_female = true;
        else {
            studio::require(i + 1 < argc, "Missing option value");
            auto value = argv[++i];
            if (arg == "--pokemon-settings")
                pokemon_settings = value;
            else if (arg == "--collision-patch") {
                collision_patch = value;
                requested_collision = true;
            } else if (arg == "--composition") {
                composition_path = value;
                requested_authoring = true;
            } else if (arg == "--project" || arg == "--project-directory") {
            } else if (arg == "--field-archive")
                source_archives.field = std::filesystem::absolute(std::filesystem::u8path(value));
            else if (arg == "--terrain-archive")
                source_archives.terrain = std::filesystem::absolute(std::filesystem::u8path(value));
            else if (arg == "--pokemon-archive")
                source_archives.pokemon = std::filesystem::absolute(std::filesystem::u8path(value));
            else if (arg == "--dump")
                dump = value;
            else if (arg == "--font")
                font_path = value;
            else if (arg == "--shaders")
                shader_directory = value;
            else if (arg == "--animation-time") {
                animation_time = std::stod(value);
                studio::require(std::isfinite(animation_time) && animation_time >= 0,
                                "Invalid animation time");
                animation_playing = false;
            } else if (arg == "--pokemon")
                requested_pokemon = integer(value);
            else if (arg == "--form")
                requested_form = integer(value);
            else if (arg == "--camera-patch") {
                camera_patch = value;
                camera_workspace = true;
            } else if (arg == "--patch")
                placement_patch = value;
            else if (arg == "--player") {
                requested_player = integer(value);
                studio::require(requested_player == 0 || requested_player == 1,
                                "Player appearance must be 0 or 1");
            } else if (arg == "--overlays")
                overlay_mode = value;
            else if (arg == "--camera")
                camera_pose = value;
            else if (arg == "--frame-material")
                frame_material = value;
            else if (arg == "--weather")
                requested_weather = integer(value);
            else if (arg == "--area")
                area = integer(value);
            else if (arg == "--light-hour") {
                light_hour = std::stof(value);
                studio::require(std::isfinite(light_hour) && light_hour >= 0 && light_hour <= 24,
                                "Light hour must be between 0 and 24");
            } else if (arg == "--cut-height") {
                cut_height = std::stof(value);
                studio::require(std::isfinite(cut_height), "Invalid cut height");
                cutaway = true;
            } else
                throw std::runtime_error("Unknown option: " + arg);
        }
    }
    studio::require(!requested_collision || (!requested_authoring && requested_pokemon < 0),
                    "Choose Collision, Authoring or Pokemon viewing at startup");
    studio::require(!requested_authoring || requested_pokemon < 0,
                    "Choose Authoring or Pokemon viewing at startup");
    studio::require((!requested_battle && pokemon_settings.empty()) || requested_pokemon >= 0,
                    "Pokemon settings preview requires --pokemon");
    studio::require(!requested_studio || requested_pokemon >= 0, "--studio requires --pokemon");
    if (project_directory.empty() && !explicit_source)
        project_directory = preferences.get("last_project");
    studio::ProjectWorkspace projects(window, preferences, project_directory, !explicit_source);
    if (studio::project_store()) {
        dump = projects.source();
        source_archives = {};
    } else if (projects.gate())
        dump.clear();
    auto properties = SDL_GetWindowProperties(window);
    studio::require(properties != 0, SDL_GetError());
    bgfx::Init init;
    init.type = backend;
#if defined(_WIN32)
    init.swapChain.nwh =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    studio::require(std::string(SDL_GetCurrentVideoDriver()) == "x11",
                    "The Linux viewport requires X11 or XWayland");
    init.swapChain.ndt =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    init.swapChain.nwh = reinterpret_cast<void *>(
        std::uintptr_t(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
#endif
    studio::require(init.swapChain.nwh != nullptr, "Missing native window handle");
    int initial_w, initial_h;
    studio::require(SDL_GetWindowSizeInPixels(window, &initial_w, &initial_h), SDL_GetError());
    init.swapChain.width = initial_w;
    init.swapChain.height = initial_h;
    init.reset = BGFX_RESET_VSYNC;
    init.profile = true;
    init.fallback = false;
    studio::require(bgfx::init(init), "bgfx could not initialize the requested backend");
    struct GraphicsCleanup {
        ~GraphicsCleanup() {
            bgfx::shutdown();
        }
    } graphics_cleanup;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    struct ContextCleanup {
        ~ContextCleanup() {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }
    } context_cleanup;
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    std::string layout_path =
        preference_root.empty() ? std::string{} : (preference_root / "layout.ini").string();
    io.IniFilename = layout_path.empty() ? nullptr : layout_path.c_str();
    struct IniLifetime {
        ~IniLifetime() {
            ImGui::GetIO().IniFilename = nullptr;
        }
    } ini_lifetime;
    io.FontGlobalScale = 1.f;
    studio::apply_editor_theme();
    auto &style = ImGui::GetStyle();
    studio::require(ImGui_ImplSDL3_InitForOther(window), "Cannot initialize SDL3 UI backend");
    auto *base = SDL_GetBasePath();
    studio::require(base != nullptr, SDL_GetError());
    auto icon_path = (std::filesystem::u8path(base) / "resources/icon.png").u8string();
    if (auto *icon = SDL_LoadPNG(reinterpret_cast<const char *>(icon_path.c_str()))) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
    if (font_path.empty())
        font_path = std::string(base) + "resources/fonts/DejaVuSans.ttf";
    const auto base_style = style;
    float font_scale = 0, pixel_density = 0;
    auto update_font = [&]() {
        float scale = std::max(SDL_GetWindowDisplayScale(window), 1.f),
              density = std::max(SDL_GetWindowPixelDensity(window), 1.f);
        if (std::abs(scale - font_scale) < .01f && std::abs(density - pixel_density) < .01f)
            return false;
        auto font_bytes = studio::read_file(std::filesystem::u8path(font_path));
        io.Fonts->Clear();
        io.FontDefault = nullptr;
        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        auto *data = IM_ALLOC(font_bytes.size());
        std::memcpy(data, font_bytes.data(), font_bytes.size());
        static const ImWchar glyph_ranges[] = {0x0020, 0x024f, 0x2000, 0x206f, 0x2640, 0x2642, 0};
        studio::require(io.Fonts->AddFontFromMemoryTTF(data, int(font_bytes.size()),
                                                       std::round(17.f * scale), &config,
                                                       glyph_ranges) != nullptr,
                        "Cannot load UI font");
        static const ImWchar dialogue_glyphs[] = {
            0x1100, 0x11ff, 0x3000, 0x30ff, 0x3130, 0x318f, 0x31f0, 0x31ff,
            0x3400, 0x9fff, 0xac00, 0xd7af, 0xf900, 0xfaff, 0xff00, 0xffef, 0};
        std::vector<std::filesystem::path> dialogue_fonts{
            std::filesystem::u8path(font_path),
            std::filesystem::u8path(base) / "resources/fonts/Dialogue.ttf"};
#ifdef _WIN32
        if (const auto *windows = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "WINDIR")) {
            auto fonts = std::filesystem::u8path(windows) / "Fonts";
            for (const auto *name : {"msyh.ttc", "malgun.ttf"})
                dialogue_fonts.push_back(fonts / name);
        }
#endif
        for (const auto &path : dialogue_fonts) {
            if (!std::filesystem::is_regular_file(path)) continue;
            auto bytes = studio::read_file(path);
            auto *glyph_data = IM_ALLOC(bytes.size());
            std::memcpy(glyph_data, bytes.data(), bytes.size());
            ImFontConfig fallback;
            fallback.MergeMode = true;
            fallback.OversampleH = fallback.OversampleV = 1;
            studio::require(io.Fonts->AddFontFromMemoryTTF(glyph_data, int(bytes.size()),
                                std::round(17.f * scale), &fallback, dialogue_glyphs) != nullptr,
                            "Cannot load dialogue font");
        }
        io.FontGlobalScale = 1.f / density;
        style = base_style;
        style.ScaleAllSizes(scale / density);
        font_scale = scale;
        pixel_density = density;
        return true;
    };
    update_font();
    studio::ImGuiRenderer ui;
    auto root = shader_directory.empty() ? std::filesystem::path(base) / "viewport-shaders"
                                         : std::filesystem::path(shader_directory);
    auto shader_path = root / (backend == bgfx::RendererType::Vulkan ? "spirv" : "glsl");
    studio::EnvironmentRenderer renderer(shader_path);
    renderer.lighting.enabled = preferences.get("lighting_source") != "neutral";
    renderer.lighting.game = preferences.get("lighting_source") != "preview";
    renderer.lighting.hour = light_hour;
    renderer.fog_enabled = saved_flag("fog_enabled", true);
    renderer.bloom_enabled = saved_flag("bloom_enabled", false);
    renderer.playback.skeletal = saved_flag("skeletal_enabled", true);
    int weather_profile = -1;
    renderer.weather_effect = 0;
    renderer.sky_type = 0;
    renderer.sky_enabled = saved_flag("sky_enabled", true);
    renderer.characters_enabled = saved_flag("characters_enabled", true);
    renderer.conditional_characters = saved_flag("conditional_characters", true);
    renderer.particles_enabled = saved_flag("particles_enabled", true);
    renderer.visibility_enabled = saved_flag("visibility_enabled", true);
    bool start_at_spawn = saved_flag("start_at_spawn", true);
    std::string camera_notice;
    const char *overlay_keys[] = {"overlay_ground",
                                  "overlay_walls",
                                  "overlay_water",
                                  "overlay_ride",
                                  "overlay_mudsdale",
                                  "overlay_placements",
                                  "overlay_cameras",
                                  "overlay_scroll_stops",
                                  "overlay_zones",
                                  "overlay_entrances",
                                  "overlay_story",
                                  "overlay_interactions",
                                  "overlay_actors",
                                  "overlay_pickups",
                                  "overlay_encounters",
                                  "overlay_field_actions",
                                  "overlay_special_encounters",
                                  "overlay_pedestrians",
                                  "overlay_berries",
                                  "overlay_fishing",
                                  "overlay_photo_spots",
                                  "overlay_contact",
                                  "overlay_rock_puzzles",
                                  "overlay_ambient_sounds"};
    static_assert(std::size(overlay_keys) == unsigned(studio::SpatialKind::Count));
    for (unsigned i = 0; i < renderer.spatial.enabled.size(); ++i) {
        renderer.spatial.enabled[i] = saved_flag(overlay_keys[i], false);
        renderer.spatial.locked[i] =
            saved_flag((std::string(overlay_keys[i]) + "_locked").c_str(), false);
    }
    renderer.spatial.xray = saved_flag("overlay_xray", false);
    renderer.spatial.filled = saved_flag("overlay_filled", true);
    renderer.spatial.pick_overlays = saved_flag("overlay_picking", false);
    if (!overlay_mode.empty()) {
        studio::require(overlay_mode == "all" || overlay_mode == "cameras" ||
                            overlay_mode == "collision" || overlay_mode == "interactions",
                        "Overlays must be all, cameras, collision or interactions");
        for (unsigned i = 0; i < renderer.spatial.enabled.size(); ++i)
            renderer.spatial.enabled[i] =
                overlay_mode == "all" ||
                (overlay_mode == "interactions" ? i >= unsigned(studio::SpatialKind::Entrance)
                 : overlay_mode == "cameras"    ? (i >= unsigned(studio::SpatialKind::Camera) &&
                                                i <= unsigned(studio::SpatialKind::Zone))
                                                : i < unsigned(studio::SpatialKind::Camera));
        renderer.spatial.focus = true;
        if (overlay_mode == "interactions")
            renderer.spatial.pick_overlays = true;
    }
    studio::CryEditor cry_editor(window);
    studio::AudioEditor audio_editor(window, cry_editor);
    studio::ImageEditor image_editor(window);
    studio::WarpRideWorkspace warp_ride(shader_path, window);
    studio::ModelWorkspace models(shader_path, window), asset_studio(shader_path, window, true);
    bool show_map_diagnostics = false;
    bool show_activity_console = false;
    bool show_navigation_history = false;
    std::vector<int> workspace_history;
    int previous_breadcrumb_workspace = -1;
    int previous_breadcrumb_area = -1;
    int previous_breadcrumb_zone = -1;
    std::string last_toast_message;
    std::size_t last_toast_count = 0;
    double last_toast_time = -100.;
    bool console_info = true, console_warnings = true, console_errors = true, console_auto_scroll = true;
    char console_search[160]{};
    int logged_workspace = -1;
    bool studio_workspace = false;
    std::string studio_notice;
    bool model_workspace = requested_pokemon >= 0;
    models.set_cry_editor(&cry_editor);
    asset_studio.set_cry_editor(&cry_editor);
    studio::MapMusic map_music;
    studio::MapAuthoringWorkspace authoring(shader_path, window);
    bool authoring_workspace = requested_authoring;
    bool collision_workspace = requested_collision;
    bool authored_collision = false;
    studio::PlacementEditor editor(window, renderer);
    studio::CollisionEditor collision(window, renderer);
    studio::CameraEditor cameras(window, renderer);
    std::filesystem::path loading_dump, loaded_dump, loaded_project;
    studio::SceneBrowser browser;
    studio::MapInspectorPage map_page = studio::MapInspectorPage::Selection;
    bool map_layers = false, map_preview = false, map_materials = false, map_region_details = false;
    bool map_focus = false;
    ImGuiID map_view_dock = 0;
    studio::MaterialSelection selection;
    std::uint32_t graphics_frame = 0;
    auto animation_tick = std::chrono::steady_clock::now();
    std::shared_ptr<studio::Environment> scene;
    std::future<studio::Environment> job;
    std::atomic_bool cancel = false;
    std::string status = "Ready", failure;
    char source[4096];
    std::snprintf(source, sizeof(source), "%s", dump.c_str());
    Camera camera;
    bool player_hovered = false;
    studio::InteractionInspector interaction_inspector;
    studio::RelatedDataInspector related_data;
    std::optional<studio::RelatedMapTarget> pending_related;
    studio::ConversationEditor conversation_editor;
    projects.conversations(conversation_editor);
    studio::WarpEditor warps(renderer);
    studio::PickupEditor pickups(renderer);
    studio::WeatherEditor weather_editor;
    studio::ZoneEditor zone_editor;
    studio::TutorialController tutorials(preferences);
    if (requested_weather >= 0)
        weather_editor.manual_preview();
    studio::OverworldEditor overworld(renderer);
    overworld.prepare_save = [&] {
        editor.finish_working_edit();
    };
    studio::FieldSystemInspector field_systems(renderer);
    studio::EncounterEditor encounters(renderer);
    studio::MapCursor map_cursor;
    bool running = true, colors = true, raw_materials = false;
    int frame = 0, last_w = initial_w, last_h = initial_h, loading_area = area, loaded_area = -1;
    double cpu_ms = 0;
    struct JobCleanup {
        std::atomic_bool &cancel;
        std::future<studio::Environment> &job;
        ~JobCleanup() {
            cancel = true;
            if (job.valid())
                job.wait();
        }
    } job_cleanup{cancel, job};
    auto folder = std::make_shared<studio::FolderSelection>();
    std::string folder_error;
    bool settings_dirty = false;
    studio::ArchivePicker field_picker, terrain_picker, pokemon_picker;
    studio::MapSelector maps;
    maps.refresh(source, source_archives);
    std::string map_source = source;
    int selected_zone = -1, loading_zone = -1, loaded_zone = -1;
    if (!preferences.get("map_zone").empty())
        try {
            selected_zone = integer(preferences.get("map_zone"));
        } catch (const std::exception &) {
        }
    auto start_player = [&]() {
        if (!scene || !scene->player_available[renderer.player.appearance])
            return;
        auto spawn = scene->start_position(loaded_zone);
        if (!spawn) {
            renderer.player.notice = "This map has no default start. Use Start at view target.";
            return;
        }
        camera.stop_look(window);
        renderer.player.start(scene->spatial, *spawn, loaded_zone);
    };
    auto focus_spawn = [&]() {
        camera_notice.clear();
        if (scene) {
            if (auto position = scene->start_position(loaded_zone))
                camera.focus_start(*position);
            else
                camera_notice = "No default start for this map; keeping the current view.";
        }
    };
    auto remember = [&]() {
        if (map_source != source) {
            map_source = source;
            selected_zone = -1;
            maps.refresh(source, source_archives);
        }
        preferences.set("map_zone", selected_zone < 0 ? "" : std::to_string(selected_zone));
        if (area >= 0) {
            if (!studio::project_store())
                preferences.set("dump", source);
            preferences.set("area", std::to_string(area));
            settings_dirty = !preferences.save();
        }
    };
    auto load = [&](bool persist = true, std::function<void()> prepare = {}) {
        cameras.request_leave([&, persist, prepare] {
            collision.request_leave([&, persist, prepare]() {
                editor.request_leave([&, persist, prepare]() {
                    if (prepare)
                        prepare();
                    studio::require(area >= 0, "Area must be nonnegative");
                    if (persist)
                        remember();
                    renderer.player.active = false;
                    cancel = false;
                    failure.clear();
                    status = "Reading and decoding area...";
                    auto path = std::string(source);
                    auto archives = source_archives;
                    auto selected = area;
                    loading_area = area;
                    loading_zone = selected_zone;
                    loading_dump = std::filesystem::u8path(path);
                    job = std::async(std::launch::async, [&, path, selected, archives]() {
                        return studio::load_environment(std::filesystem::u8path(path),
                                                        std::size_t(selected), &cancel, archives);
                    });
                    if (observer)
                        observer->scene_changed();
                });
            });
        });
    };
    if (requested_authoring) {
        studio::require(!dump.empty(), "Map authoring requires a dump folder");
        authoring.open_template(std::filesystem::u8path(dump), unsigned(area),
                                std::filesystem::u8path(composition_path));
    } else if (model_workspace) {
        studio::require(!dump.empty(), "Pokemon viewing requires a dump folder");
        if (requested_battle)
            models.battle_preview();
        if (!pokemon_settings.empty())
            models.open_settings(std::filesystem::u8path(pokemon_settings));
        models.open_species(std::filesystem::u8path(dump), unsigned(requested_pokemon),
                            unsigned(requested_form), model_female, model_shiny, source_archives);
    } else if (audio_workspace || image_workspace || warp_ride_workspace) {
    } else if (!dump.empty())
        load(false);

    int pending_entrance_zone = -1;
    unsigned pending_entrance_event = 0;
    auto handle_event = [&](const SDL_Event &event) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_MOUSE_MOTION && camera.looking) {
            camera.look_x += event.motion.xrel;
            camera.look_y += event.motion.yrel;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
            (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT))
            camera.stop_look(window);
        if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                             event.window.windowID == SDL_GetWindowID(window))) {
            if (studio::project_store() || projects.gate()) {
                if (projects.close())
                    running = false;
                return;
            }
            data_workspace = false;
            warp_ride_workspace = false;
            image_workspace = true;
            image_editor.request_leave([&] {
                image_workspace = false;
                audio_workspace = true;
                audio_editor.request_leave([&] {
                    audio_workspace = false;
                    cry_editor.request_leave([&] {
                        camera_workspace = false;
                        collision_workspace = false;
                        authoring_workspace = false;
                        studio_workspace = false;
                        model_workspace = true;
                        models.request_leave([&] {
                            camera_workspace = false;
                            authoring_workspace = false;
                            studio_workspace = false;
                            model_workspace = false;
                            collision.request_leave([&] {
                                collision_workspace = false;
                                authoring_workspace = true;
                                authoring.request_leave([&] {
                                    authoring_workspace = false;
                                    studio_workspace = true;
                                    asset_studio.request_leave([&] {
                                        studio_workspace = false;
                                        model_workspace = false;
                                        editor.request_leave([&] {
                                            cameras.request_leave([&] {
                                                running = false;
                                            });
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        }
    };
    while (running) {
        auto begin = std::chrono::steady_clock::now();
        SDL_Event event;
        while (SDL_PollEvent(&event))
            handle_event(event);
        if (!running)
            break;
        if (auto working = overworld.poll_preview(job.valid(), [&] {
                editor.refresh_working_transforms();
            })) {
            field_systems.pedestrians.clear_scene_preview();
            conversation_editor.clear_scene_preview();
            const auto lighting = renderer.lighting;
            std::optional<studio::OverworldReference> selected_reference;
            if (scene && renderer.spatial.selected >= 0 &&
                std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()) {
                const auto &r = scene->spatial.regions[renderer.spatial.selected];
                if (r.overworld)
                    selected_reference = *r.overworld;
            }
            if (auto focus = overworld.take_preview_focus())
                for (const auto &region : working->spatial.regions)
                    if (region.overworld && region.overworld->editor_id == focus) {
                        selected_reference = *region.overworld;
                        break;
                    }
            auto placement_region = [](const studio::SpatialRegion &r) {
                if (!r.overworld)
                    return false;
                auto category = r.overworld->category;
                return category == studio::TargetProfile::position_event_pack ||
                       category == studio::TargetProfile::character_placement_pack ||
                       category == studio::TargetProfile::warp_placement_pack ||
                       category == studio::TargetProfile::interaction_placement_pack ||
                       category == studio::TargetProfile::static_pack ||
                       category == studio::TargetProfile::trainer_placement_pack ||
                       category == studio::TargetProfile::pickup_placement_pack;
            };
            for (auto &region : working->spatial.regions) {
                if (placement_region(region))
                    continue;
                auto existing = std::find_if(scene->spatial.regions.begin(),
                                             scene->spatial.regions.end(), [&](const auto &r) {
                                                 return !placement_region(r) &&
                                                        r.kind == region.kind &&
                                                        r.name == region.name;
                                             });
                if (existing != scene->spatial.regions.end())
                    region = *existing;
            }
            auto regions = std::move(working->spatial.regions);
            working->spatial = scene->spatial;
            working->spatial.regions = std::move(regions);
            const auto reused = studio::reusable_scene_draws(*scene, *working);
            std::vector<bool> visible;
            for (auto old : reused)
                visible.push_back(old >= scene->draws.size() || renderer.draw_visible(old));
            renderer.select_draw(-1);
            renderer.spatial.selected = -1;
            *scene = std::move(*working);
            overworld.adopt_preview(scene);
            renderer.reload_scene_buffers();
            for (unsigned i = 0; i < visible.size(); ++i)
                renderer.set_draw_visible(i, visible[i]);
            renderer.lighting = lighting;
            selection = {};
            browser.rebuild(*scene);
            editor.set_working_scene(scene, overworld.working_document());
            if (selected_reference)
                for (unsigned i = 0; i < scene->spatial.regions.size(); ++i) {
                    const auto &r = scene->spatial.regions[i];
                    if (r.overworld && r.overworld->category == selected_reference->category &&
                        r.overworld->local_zone == selected_reference->local_zone &&
                        (selected_reference->editor_id
                             ? r.overworld->editor_id == selected_reference->editor_id
                             : r.overworld->event == selected_reference->event)) {
                        renderer.spatial.selected = int(i);
                        map_page = studio::MapInspectorPage::Selection;
                        if (r.placement >= 0)
                            for (unsigned d = 0; d < scene->draws.size(); ++d)
                                if (scene->draws[d].placement == r.placement) {
                                    selection.select_mesh(int(d), int(scene->draws[d].material),
                                                          false);
                                    renderer.select_draw(int(d));
                                    break;
                                }
                        break;
                    }
                }
        }
        if (job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                std::optional<studio::OverworldReference> restore_placement;
                if (scene && loading_area == loaded_area && renderer.spatial.selected >= 0 &&
                    std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()) {
                    const auto &region = scene->spatial.regions[renderer.spatial.selected];
                    if (region.overworld)
                        restore_placement = *region.overworld;
                }
                std::optional<studio::ViewportCamera> restore_view;
                if (placement_reload) {
                    if (studio::project_store() &&
                        placement_reload->project == studio::project_store()->root &&
                        placement_reload->area == loading_area) {
                        restore_placement = placement_reload->placement;
                        restore_view = placement_reload->camera;
                    }
                    placement_reload.reset();
                }
                authored_collision = false;
                scene = std::make_shared<studio::Environment>(job.get());
                renderer.set_scene(scene);
                editor.set_scene(scene, unsigned(loading_area), loading_dump);
                collision.set_scene(scene, unsigned(loading_area), loading_dump);
                cameras.set_scene(scene, unsigned(loading_area), loading_dump);
                warps.set_scene(scene, unsigned(loading_area), loading_dump);
                pickups.set_scene(scene, unsigned(loading_area), loading_dump);
                weather_editor.load(loading_dump);
                zone_editor.load(loading_dump);
                overworld.set_scene(scene, unsigned(loading_area), loading_dump);
                if (studio::project_store() && !studio::project_store()->edits.contains(
                                                   "placements/" + std::to_string(loading_area)))
                    editor.set_working_scene(scene, overworld.working_document());
                field_systems.set_scene(scene, loading_dump, unsigned(loading_area));
                encounters.set_scene(scene, unsigned(loading_area), loading_dump);
                if (!camera_patch.empty()) {
                    cameras.open_patch(std::filesystem::u8path(camera_patch));
                    camera_patch.clear();
                }
                if (!collision_patch.empty()) {
                    collision.open_patch(std::filesystem::u8path(collision_patch));
                    collision_patch.clear();
                }
                if (!placement_patch.empty()) {
                    editor.open_patch(std::filesystem::u8path(placement_patch));
                    placement_patch.clear();
                }
                browser.rebuild(*scene);
                renderer.lighting.context = 0;
                renderer.lighting.motion = 0;
                weather_profile = -1;
                renderer.weather_effect = 0;
                renderer.sky_type = 0;
                for (unsigned i = 0; i < scene->lighting_contexts.size(); ++i)
                    if (int(scene->lighting_contexts[i].zone) == loading_zone) {
                        renderer.lighting.context = i;
                        renderer.lighting.motion = 0;
                    }
                if (requested_weather >= 0 &&
                    renderer.lighting.context < scene->lighting_contexts.size()) {
                    auto &profiles = scene->lighting_contexts[renderer.lighting.context].weather;
                    auto w = std::find_if(profiles.begin(), profiles.end(), [&](auto &p) {
                        return int(p.kind) == requested_weather;
                    });
                    studio::require(w != profiles.end(),
                                    "Weather profile unavailable for this map");
                    weather_profile = int(w->kind);
                    renderer.weather_effect = w->effect;
                    renderer.sky_type = w->sky;
                    renderer.lighting.motion = w->motion;
                }
                selection = {};
                if (restore_placement)
                    for (unsigned i = 0; i < scene->spatial.regions.size(); ++i) {
                        const auto &r = scene->spatial.regions[i];
                        if (!r.overworld || r.overworld->category != restore_placement->category ||
                            r.overworld->local_zone != restore_placement->local_zone ||
                            (r.overworld->category == 0
                                 ? r.overworld->row != restore_placement->row
                                 : r.overworld->event != restore_placement->event))
                            continue;
                        renderer.spatial.selected = int(i);
                        if (r.placement >= 0)
                            for (unsigned d = 0; d < scene->draws.size(); ++d)
                                if (scene->draws[d].placement == r.placement) {
                                    selection.select_mesh(int(d), int(scene->draws[d].material),
                                                          false);
                                    renderer.select_draw(int(d));
                                    break;
                                }
                        break;
                    }

                renderer.playback.seconds = animation_time;
                loaded_area = loading_area;
                loaded_dump = loading_dump;
                loaded_project = studio::project_store() ? studio::project_store()->root
                                                         : std::filesystem::path{};
                loaded_zone = loading_zone;
                if (loaded_zone >= 0 && std::none_of(scene->locations.begin(),
                                                     scene->locations.end(), [&](auto &location) {
                                                         return location.zone == loaded_zone;
                                                     }))
                    loaded_zone = -1;
                camera_notice.clear();
                camera.fit(*scene);
                if (start_at_spawn)
                    focus_spawn();
                if (restore_view) {
                    camera.target = restore_view->target;
                    camera.yaw = restore_view->yaw;
                    camera.pitch = restore_view->pitch;
                    camera.distance = restore_view->distance;
                    map_page = studio::MapInspectorPage::Selection;
                }
                if (!frame_material.empty()) {
                    auto low = scene->high, high = scene->low;
                    bool found = false;
                    for (auto &draw : scene->draws)
                        if (scene->materials[draw.material].name == frame_material)
                            for (auto &v : draw.vertices) {
                                found = true;
                                std::array<float, 3> p{v.x, v.y, v.z};
                                for (unsigned k = 0; k < 3; ++k) {
                                    low[k] = std::min(low[k], p[k]);
                                    high[k] = std::max(high[k], p[k]);
                                }
                            }
                    studio::require(found, "Frame material was not found");
                    camera.fit(low, high);
                    camera.bounds_low = scene->low;
                    camera.bounds_high = scene->high;
                }
                if (!camera_pose.empty()) {
                    std::istringstream pose(camera_pose);
                    studio::require(bool(pose >> camera.target[0] >> camera.target[1] >>
                                         camera.target[2] >> camera.yaw >> camera.pitch >>
                                         camera.distance),
                                    "Camera needs target X Y Z, yaw, pitch and distance");
                    pose >> std::ws;
                    studio::require(pose.eof() && std::isfinite(camera.target[0]) &&
                                        std::isfinite(camera.target[1]) &&
                                        std::isfinite(camera.target[2]) &&
                                        std::isfinite(camera.yaw) && std::isfinite(camera.pitch) &&
                                        std::isfinite(camera.distance) && camera.distance > 0 &&
                                        std::abs(camera.pitch) <= 1.5f,
                                    "Invalid camera pose");
                }
                if (requested_player >= 0) {
                    renderer.player.appearance = unsigned(requested_player);
                    start_player();
                    studio::require(renderer.player.active,
                                    "Player mode unavailable: " + renderer.player.notice);
                }
                studio::activity_log::add("Maps", "Loaded area " + std::to_string(loading_area) + " / zone " + std::to_string(loading_zone));
                status = "Uploading scene...";
                std::cout << scene->report() << std::flush;
            } catch (const std::exception &e) {
                failure = e.what();
                studio::activity_log::add("Maps", failure, studio::activity_log::Level::Error);
                status = "Load failed";
            }
        }
        if (scene && !job.valid() && pending_entrance_zone >= 0) {
            bool found = false;
            for (unsigned i = 0; i < scene->spatial.regions.size(); ++i) {
                auto &r = scene->spatial.regions[i];
                if (r.kind == studio::SpatialKind::Entrance && r.zone == pending_entrance_zone &&
                    r.overworld && r.overworld->event == pending_entrance_event) {
                    renderer.spatial.selected = int(i);
                    renderer.spatial.enabled[unsigned(r.kind)] = true;
                    renderer.spatial.focus = true;
                    camera.target = r.overworld->position;
                    camera.distance = 450;
                    found = true;
                    break;
                }
            }
            if (!found)
                camera_notice = "Destination entrance was not found in the loaded map.";
            pending_entrance_zone = -1;
        }
        if (!job.valid()) {
            if (auto request = related_data.take_request()) {
                if (int(request->area) == loaded_area) pending_related = request;
                else {
                    auto target_dump = loaded_dump;
                    auto target_archives = scene->archive_sources;
                    load(false, [&, request, target_dump, target_archives] {
                        auto path = target_dump.u8string();
                        studio::require(path.size() < sizeof(source), "Dump path is too long");
                        std::memcpy(source, path.c_str(), path.size() + 1);
                        source_archives = target_archives;
                        area = int(request->area); selected_zone = request->zone; pending_related = request;
                    });
                }
            }
            if (pending_related && !job.valid() && scene && int(pending_related->area) == loaded_area) {
                bool found = false;
                for (std::size_t i = 0; i < scene->spatial.regions.size(); ++i) {
                    const auto &region = scene->spatial.regions[i];
                    if (!region.overworld) continue;
                    const auto &ref = *region.overworld;
                    if (ref.category != pending_related->category || ref.local_zone != pending_related->local_zone ||
                        ref.row != pending_related->row || ref.event != pending_related->event) continue;
                    selection = {}; renderer.spatial.selected = int(i);
                    renderer.spatial.enabled[unsigned(region.kind)] = true;
                    browser.frame_selection(*scene, renderer, selection, camera);
                    map_page = studio::MapInspectorPage::Selection; map_materials = false;
                    found = true; break;
                }
                if (!found) camera_notice = "The referenced placement is not present in the loaded area. Refresh its dependency scan.";
                pending_related.reset();
            }
        }
        if (renderer.spatial.destination_zone >= 0 && !job.valid()) {
            auto target = renderer.spatial.destination_zone;
            auto event = renderer.spatial.destination_event;
            renderer.spatial.destination_zone = -1;
            try {
                auto catalog = studio::load_map_catalog(loaded_dump, scene->archive_sources);
                auto it = std::find_if(catalog.locations.begin(), catalog.locations.end(),
                                       [&](auto &location) {
                                           return location.zone == target;
                                       });
                studio::require(it != catalog.locations.end(),
                                "Destination zone is not in the map catalog");
                if (it->area == loaded_area) {
                    pending_entrance_zone = target;
                    pending_entrance_event = event;
                } else {
                    auto target_area = it->area;
                    auto target_dump = loaded_dump;
                    auto target_archives = scene->archive_sources;
                    load(false, [&, target, event, target_area, target_dump, target_archives] {
                        auto path = target_dump.u8string();
                        studio::require(path.size() < sizeof(source), "Dump path is too long");
                        std::memcpy(source, path.c_str(), path.size() + 1);
                        source_archives = target_archives;
                        area = target_area;
                        selected_zone = target;
                        pending_entrance_zone = target;
                        pending_entrance_event = event;
                    });
                }
            } catch (const std::exception &e) {
                camera_notice = e.what();
                pending_entrance_zone = -1;
            }
        }
        auto tick = std::chrono::steady_clock::now();
        if (renderer.ready() && animation_playing && renderer.playback.enabled)
            renderer.playback.seconds +=
                std::min(std::chrono::duration<double>(tick - animation_tick).count(), .25) *
                animation_speed;
        animation_tick = tick;
        if (auto picked = renderer.poll_pick(graphics_frame)) {
            if (*picked >= int(scene->draws.size())) {
                selection = {};
                renderer.spatial.selected = *picked - int(scene->draws.size());
                renderer.spatial.focus = true;
            } else {
                renderer.spatial.selected = studio::SceneBrowser::character_region(*scene, *picked);
                selection.draw = *picked;
                selection.material = *picked >= 0 ? int(scene->draws[*picked].material) : -1;
                selection.focus = *picked >= 0;
                selection.reveal_scene = *picked >= 0;
                if (*picked >= 0)
                    selection.filter[0] = 0;
            }
        }
        if (selection.focus || renderer.spatial.focus) {
            map_page = studio::MapInspectorPage::Selection;
            map_materials = false;
        }
        if (scene)
            weather_editor.update(*scene, renderer, weather_profile);
        renderer.upload_step();
        if (renderer.ready() && !job.valid() && failure.empty())
            status = "Area loaded - approximate material preview";
        int width, height;
        studio::require(SDL_GetWindowSizeInPixels(window, &width, &height), SDL_GetError());
        if (width <= 0 || height <= 0 || SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            map_music.update(false);
            if (SDL_WaitEventTimeout(&event, 100))
                handle_event(event);
            continue;
        }
        if (width != last_w || height != last_h) {
            bgfx::SwapChain resized;
            resized.width = std::uint32_t(width);
            resized.height = std::uint32_t(height);
            bgfx::reset(BGFX_RESET_VSYNC, &resized);
            last_w = width;
            last_h = height;
        }
        {
            std::lock_guard lock(folder->mutex);
            if (folder->ready) {
                folder->ready = false;
                folder_error = folder->error;
                if (!folder->path.empty()) {
                    if (folder->path.size() < sizeof(source)) {
                        std::snprintf(source, sizeof(source), "%s", folder->path.c_str());
                        remember();
                    } else
                        folder_error = "Selected path is too long";
                }
            }
        }
        bool map_archives_changed =
            field_picker.poll(source_archives.field, studio::TargetProfile::field_archive);
        map_archives_changed |=
            terrain_picker.poll(source_archives.terrain, studio::TargetProfile::terrain_archive);
        if (pokemon_picker.poll(source_archives.pokemon, studio::TargetProfile::pokemon_archive))
            models.request_catalog_refresh();
        if (map_archives_changed)
            maps.refresh(source, source_archives);
        auto archive_controls = [&](bool map, bool pokemon) {
            if (studio::project_store())
                return;
            bool changed = false;
            auto dump_path = std::filesystem::u8path(source);
            if (map) {
                changed |=
                    field_picker.draw(window, "Field archive", source_archives.field,
                                      dump_path / studio::GameProfile::field_archive(dump_path));
                changed |= terrain_picker.draw(window, "Terrain resources archive",
                                               source_archives.terrain,
                                               dump_path / studio::TargetProfile::terrain_archive);
            }
            if (pokemon && pokemon_picker.draw(window, "Pokemon archive", source_archives.pokemon,
                                               dump_path / studio::TargetProfile::pokemon_archive))
                models.request_catalog_refresh();
            if (changed)
                maps.refresh(source, source_archives);
            ImGui::TextWrapped(
                "%s",
                map ? (pokemon ? "Other resources come from the dump. Load a map after changing "
                                 "its sources; the Pokemon list refreshes automatically."
                               : "Other resources come from the dump. Load the map after changing "
                                 "sources.")
                    : "The Pokemon list refreshes automatically. Names come from the dump; search "
                      "by number for added species.");
        };
        // USUM_MENU_V2: native title follows the active project's real game target.
        {
            static std::string last_title;
            std::string title = "USUM Studio";
            const auto game = projects.active_game_name();
            if (!game.empty()) title += " - Pokemon " + game;
            if (title != last_title) {
                SDL_SetWindowTitle(window, title.c_str());
                last_title = std::move(title);
            }
        }
        ImGui_ImplSDL3_NewFrame();
        if (update_font())
            ui.upload_font();
        ImGui::NewFrame();
        studio::UndoShortcuts::begin_frame();
        tutorials.begin_frame(projects.gate()       ? -1
                              : warp_ride_workspace ? 8
                              : image_workspace     ? 6
                              : audio_workspace     ? 7
                              : camera_workspace    ? 5
                              : collision_workspace ? 4
                              : authoring_workspace ? 3
                              : studio_workspace    ? 2
                                                    : int(model_workspace),
                              camera.looking || job.valid() || projects.tutorials_suspended() ||
                                  !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS));
        image_editor.update();
        collision.update();
        cameras.update();
        if (cameras.take_focus_request()) {
            data_workspace = false;
            warp_ride_workspace = false;
            audio_workspace = false;
            image_workspace = false;
            camera_workspace = true;
            collision_workspace = false;
            authoring_workspace = false;
            studio_workspace = false;
            model_workspace = false;
            camera.stop_look(window);
        }
        if (collision.take_focus_request()) {
            data_workspace = false;
            warp_ride_workspace = false;
            authored_collision = false;
            audio_workspace = false;
            image_workspace = false;
            camera_workspace = false;
            collision_workspace = true;
            authoring_workspace = false;
            studio_workspace = false;
            model_workspace = false;
            camera.stop_look(window);
        }
        static studio::PersonalDataViewer personal_data_viewer;
        static studio::ItemDataViewer item_data_viewer;
        static studio::MoveDataViewer move_data_viewer;
        static studio::LearnsetDataViewer learnset_data_viewer;
        static studio::TextDataViewer text_data_viewer;
        static studio::EvolutionDataViewer evolution_data_viewer;
        static studio::EncounterDataViewer encounter_data_viewer;
        static studio::TrainerDataViewer trainer_data_viewer(false);
        static studio::TrainerDataViewer trainer_pokemon_viewer(true);
        static studio::ExtraDataViewer egg_move_viewer(studio::ExtraDataViewer::Kind::EggMoves);
        static studio::ExtraDataViewer mega_viewer(studio::ExtraDataViewer::Kind::Mega);
        static studio::ExtraDataViewer trainer_class_viewer(studio::ExtraDataViewer::Kind::TrainerClasses);
        static studio::ExtraDataViewer tm_tutor_viewer(studio::ExtraDataViewer::Kind::TMTutor);
        static studio::ExtraDataViewer shop_viewer(studio::ExtraDataViewer::Kind::Shops);
        // Universal command search.  Search labels are bilingual, and actions are
        // resolved to workspaces/readers instead of duplicating their logic.
        static bool focus_global_search = false;
        static bool request_global_search = false;
        static char global_search_text[192] = {};
        static int selected_data_editor = 0;
        static int preference_page = 0;
        static int queued_workspace = -1;
        // Only show genuine Pokemon models found in the dump's model catalog.
        static std::future<std::vector<studio::PokemonEntry>> model_search_job;
        static std::vector<studio::PokemonEntry> model_search_entries;
        static std::string model_search_source;
        static std::string model_search_error;
        static int queued_library_category = -1;
        static std::size_t queued_library_member = 0;
        static int queued_model_species = -1;
        static unsigned queued_model_form = 0;
        static bool queued_model_female = false;
        // Map catalog is loaded only on demand. Never invent map destinations.
        struct LibrarySearchResult { int category; studio::LibraryModel entry; };
        static std::array<std::future<std::vector<studio::LibraryModel>>, 6> library_search_jobs;
        static std::array<std::vector<studio::LibraryModel>, 6> library_search_entries;
        static std::string library_search_source;
        static std::future<studio::MapCatalog> map_search_job;
        static std::vector<studio::MapLocation> map_search_entries;
        static std::string map_search_source;
        static std::string map_search_error;
        static int queued_map_area = -1;
        static int queued_map_zone = -1;
        static int queued_map_workspace = -1;

        const bool global_search_fr = studio::localization::current ==
                                      studio::localization::Language::French;
        if (studio::feature_visibility::universal_search && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false) &&
            !ImGui::GetIO().WantTextInput) request_global_search = true;
        if (!studio::feature_visibility::universal_search) request_global_search = false;
        if (request_global_search) {
            studio::activity_log::add("Search", "Open universal search");
            request_global_search = false;
            global_search_text[0] = '\0';
            focus_global_search = true;
            if (model_search_source != source && !model_search_job.valid()) {
                model_search_entries.clear();
                model_search_error.clear();
                model_search_source = source;
                if (source[0] != '\0') {
                    auto dump = std::filesystem::u8path(source);
                    auto archives = source_archives;
                    model_search_job = std::async(std::launch::async, [dump, archives] {
                        return studio::load_pokemon_catalog(dump, archives);
                    });
                }
            }
            if (map_search_source != source && !map_search_job.valid()) {
                map_search_entries.clear();
                map_search_error.clear();
                map_search_source = source;
                if (source[0] != '\0') {
                    auto dump_path = std::filesystem::u8path(source);
                    auto archives = source_archives;
                    map_search_job = std::async(std::launch::async, [dump_path, archives] {
                        return studio::load_map_catalog(dump_path, archives);
                    });
                }
            }
            if (library_search_source != source) {
                bool active = false;
                for (auto &job : library_search_jobs) if (job.valid()) active = true;
                if (!active) {
                    library_search_source = source;
                    for (auto &entries : library_search_entries) entries.clear();
                    if (source[0]) for (int i = 0; i < 6; ++i) {
                        auto path = std::filesystem::u8path(source) /
                                    studio::model_category_archive(studio::ModelCategory(i));
                        library_search_jobs[i] = std::async(std::launch::async, [path, i] {
                            return studio::load_model_library(path, studio::ModelCategory(i));
                        });
                    }
                }
            }
            studio::model_aliases().bind(std::filesystem::u8path(source));
            ImGui::OpenPopup((global_search_fr ? "Recherche universelle###universal_search" : "Universal search###universal_search"));
        }
        ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);
        if (studio::feature_visibility::universal_search && ImGui::BeginPopupModal((global_search_fr ? "Recherche universelle###universal_search" : "Universal search###universal_search"), nullptr,
                                  ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(global_search_fr ? "Recherche universelle (Ctrl+K)" :
                                                     "Universal search (Ctrl+K)");
            ImGui::SameLine();
            if (ImGui::Button(global_search_fr ? "Fermer##search_close" : "Close##search_close"))
                ImGui::CloseCurrentPopup();
            if (model_search_job.valid() &&
                model_search_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try { model_search_entries = model_search_job.get(); }
                catch (const std::exception &error) {
                    model_search_error = error.what();
                    model_search_entries.clear();
                }
            }
            if (map_search_job.valid() &&
                map_search_job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try { map_search_entries = map_search_job.get().locations; }
                catch (const std::exception &error) {
                    map_search_error = error.what();
                    map_search_entries.clear();
                }
            }
            for (int i = 0; i < 6; ++i)
                if (library_search_jobs[i].valid() &&
                    library_search_jobs[i].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    try { library_search_entries[i] = library_search_jobs[i].get(); }
                    catch (const std::exception &) { library_search_entries[i].clear(); }
                }
            ImGui::SetNextItemWidth(-1);
            if (focus_global_search) {
                ImGui::SetKeyboardFocusHere();
                focus_global_search = false;
            }
            ImGui::InputTextWithHint("##global_search_input",
                                     global_search_fr ? "Rechercher un Pokemon, une carte, un editeur..." :
                                                        "Search Pokemon, maps, editors...",
                                     global_search_text, sizeof(global_search_text));
            struct SearchTarget {
                const char *en;
                const char *fr;
                const char *keywords;
                int workspace;
                int data_editor;
            };
            static const SearchTarget targets[] = {
                {"Maps", "Cartes", "map terrain field zone", 0, -1},
                {"Models", "Modeles", "model pokemon 3d", 1, -1},
                {"Studio", "Studio", "texture uv material", 2, -1},
                {"Authoring", "Composition de cartes", "authoring map object npc", 3, -1},
                {"Collision", "Collisions", "collider mesh", 4, -1},
                {"Cameras", "Cameras", "camera", 5, -1},
                {"Images", "Images", "png image resize", 6, -1},
                {"Audio", "Audio", "music cry son musique cri", 7, -1},
                {"Warp Ride", "Ultra-Exploration", "warp ride minigame", 8, -1},
                {"Data", "Donnees", "data database", 9, 0},
                {"Personal Data", "Donnees personnelles", "pokemon stat form", 9, 0},
                {"Item Data", "Donnees des objets", "item bag objet", 9, 1},
                {"Text Data", "Donnees des textes", "text localization traduction", 9, 2},
                {"Evolution Data", "Donnees des evolutions", "evolution pokemon", 9, 3},
                {"Encounter Data", "Donnees des rencontres", "encounter wild pokemon", 9, 4},
                {"Move Data", "Donnees des capacites", "move attack power pp", 9, 5},
                {"Learnset Data", "Capacites apprises", "learnset level attack", 9, 6},
                {"Egg Move Data", "Capacites oeuf", "egg moves reproduction", 9, 9},
                {"Mega Evolution Data", "Mega-Evolutions", "mega stones", 9, 10},
                {"Trainer Class Data", "Classes de dresseurs", "class trainer", 9, 11},
                {"TM / Tutor Data", "CT Maitres capacites", "tm tutor", 9, 12},
                {"Shop / Mart Data", "Boutiques", "shop mart items", 9, 13},
                {"Keyboard controls", "Configuration des touches", "settings preferences keys keyboard raccourcis", -2, -1},
            };
            auto fold_ascii = [](std::string_view text) {
                std::string out;
                out.reserve(text.size());
                for (unsigned char c : text) {
                    if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
                    out.push_back(static_cast<char>(c));
                }
                return out;
            };
            std::string query = fold_ascii(global_search_text);
            // Match every word independently; this works with mixed FR/EN keywords.
            auto matches = [&](const SearchTarget &entry) {
                std::string searchable = fold_ascii(entry.en) + " " +
                                         fold_ascii(entry.fr) + " " + entry.keywords;
                size_t at = 0;
                while (at < query.size()) {
                    while (at < query.size() && query[at] == ' ') ++at;
                    size_t end = query.find(' ', at);
                    if (end == std::string::npos) end = query.size();
                    if (end > at && searchable.find(query.substr(at, end - at)) ==
                                    std::string::npos) return false;
                    at = end;
                }
                return true;
            };
            ImGui::Separator();
            int results = 0;
            const bool activate_first = ImGui::IsKeyPressed(ImGuiKey_Enter, false);
            for (const auto &entry : targets) {
                if (!matches(entry)) continue;
                ++results;
                ImGui::PushID(&entry);
                const char *title = global_search_fr ? entry.fr : entry.en;
                if (ImGui::Selectable(title) || (activate_first && results == 1)) {
                    if (entry.workspace == -2) { show_app_preferences = true; preference_page = 2; }
                    else {
                        queued_workspace = entry.workspace;
                        if (entry.data_editor >= 0) selected_data_editor = entry.data_editor;
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            // Direct model results come from the actual USUM model management table.
            // Never invent a form/model when no catalog entry is available.
            const bool search_favorites = studio::feature_visibility::favorites &&
                (query == "favoris" || query == "favorites" || query == "favorite" || query == "fav");
            if (search_favorites && studio::feature_visibility::favorites) {
                ImGui::SeparatorText(global_search_fr ? "Favoris - tous les modeles" :
                                                        "Favorites - all models");
            }
            if (!query.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("%s", global_search_fr ? "Modeles Pokemon" : "Pokemon models");
                int model_results = 0;
                for (const auto &pokemon : model_search_entries) {
                    const std::string label = fold_ascii(pokemon.label);
                    const std::string name = fold_ascii(pokemon.name);
                    const std::string number = std::to_string(pokemon.species);
                    const std::string alias = studio::feature_visibility::renaming ? studio::model_aliases().get(
                        studio::ModelAliases::pokemon_key(pokemon.data_index)) : std::string{};
                    if (search_favorites ?
                        !studio::model_aliases().is_favorite(studio::ModelAliases::pokemon_key(pokemon.data_index)) :
                        (name.find(query) == std::string::npos &&
                         label.find(query) == std::string::npos &&
                         number.find(query) == std::string::npos &&
                         fold_ascii(alias).find(query) == std::string::npos)) continue;
                    ++model_results;
                    std::string display = "[" + std::to_string(pokemon.data_index) + "] " +
                        (alias.empty() ? pokemon.label : alias + " (" + pokemon.label + ")") +
                        (global_search_fr ? "  -> Ouvrir le modele" : "  -> Open model");
                    ImGui::PushID(static_cast<int>(pokemon.data_index));
                    if (ImGui::Selectable(display.c_str())) {
                        queued_model_species = static_cast<int>(pokemon.species);
                        queued_model_form = pokemon.form;
                        queued_model_female = pokemon.female;
                        queued_workspace = 1;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                    if (model_results >= 80) break;
                }
                results += model_results;
                if (model_search_job.valid())
                    ImGui::TextDisabled("%s", global_search_fr ?
                        "Chargement des modeles Pokemon..." : "Loading Pokemon models...");
                else if (!model_search_error.empty())
                    ImGui::TextDisabled("%s: %s", global_search_fr ?
                        "Catalogue de modeles indisponible" : "Model catalog unavailable",
                        model_search_error.c_str());
                else if (model_search_source.empty())
                    ImGui::TextDisabled("%s", global_search_fr ?
                        "Ouvrez d'abord un dump USUM." : "Open a USUM dump first.");
            }
            if (!query.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("%s", global_search_fr ? "Autres modeles" : "Other models");
                int shown = 0;
                for (int cat = 0; cat < 6 && shown < 80; ++cat)
                    for (const auto &model : library_search_entries[cat]) {
                        auto alias = studio::feature_visibility::renaming ? studio::model_aliases().get(
                            studio::ModelAliases::library_key(cat + 1, model.member)) : std::string{};
                        auto number = std::to_string(model.member);
                        if (search_favorites ?
                            !studio::model_aliases().is_favorite(studio::ModelAliases::library_key(cat + 1, model.member)) :
                            (fold_ascii(alias).find(query) == std::string::npos &&
                             fold_ascii(model.name).find(query) == std::string::npos &&
                             number.find(query) == std::string::npos)) continue;
                        std::string label = "[" + number + "] " +
                            (alias.empty() ? model.name : alias + " (" + model.name + ")") +
                            (global_search_fr ? "  -> Modele" : "  -> Model");
                        ImGui::PushID(cat); ImGui::PushID(int(model.member));
                        if (ImGui::Selectable(label.c_str())) {
                            queued_library_category = cat + 1;
                            queued_library_member = model.member;
                            queued_workspace = 1;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopID(); ImGui::PopID();
                        ++shown; ++results;
                        if (shown >= 80) break;
                    }
            }
            if (!query.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("%s", global_search_fr ? "Cartes disponibles" : "Available maps");
                int found_maps = 0;
                for (const auto &location : map_search_entries) {
                    const std::string location_name = fold_ascii(location.name);
                    const std::string area_number = std::to_string(location.area);
                    if (location_name.find(query) == std::string::npos &&
                        area_number.find(query) == std::string::npos) continue;
                    for (int destination : {0, 3}) {
                        std::string display = location.name +
                            (destination == 0 ?
                                (global_search_fr ? "  -> Cartes" : "  -> Maps") :
                                (global_search_fr ? "  -> Creation de cartes" : "  -> Authoring"));
                        ImGui::PushID(location.area);
                        ImGui::PushID(location.zone);
                        ImGui::PushID(destination);
                        if (ImGui::Selectable(display.c_str())) {
                            queued_map_area = location.area;
                            queued_map_zone = location.zone;
                            queued_map_workspace = destination;
                            queued_workspace = destination;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopID();
                        ImGui::PopID();
                        ImGui::PopID();
                        ++results;
                    }
                    if (++found_maps >= 30) break;
                }
                if (map_search_job.valid())
                    ImGui::TextDisabled("%s", global_search_fr ?
                        "Chargement des cartes..." : "Loading maps...");
                else if (!map_search_error.empty())
                    ImGui::TextDisabled("%s: %s", global_search_fr ?
                        "Catalogue des cartes indisponible" : "Map catalog unavailable",
                        map_search_error.c_str());
            }
            if (!results) ImGui::TextDisabled("%s", global_search_fr ?
                          "Aucun resultat" : "No results");
            ImGui::EndPopup();
        }
        bool reset_layout = false;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(style.FramePadding.x, ImGui::GetFontSize() * .5f));
        conversation_editor.global_controls(preferences);
        if (ImGui::BeginMainMenuBar()) {
            // Native-sized system menu: commands, no bulky workspace tabs.
            if (ImGui::BeginMenu(global_search_fr ? "Fichier" : "File")) {
                // Direct access to existing project commands; no reimplementation.
                projects.file_menu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(studio::localization::tr("View"))) {
                if (studio::feature_visibility::universal_search &&
                    ImGui::MenuItem(global_search_fr ? "Recherche universelle..." : "Universal search...", "Ctrl+K"))
                    request_global_search = true;
                ImGui::Separator();
                ImGui::TextDisabled("%s", global_search_fr ? "ESPACE DE TRAVAIL" : "WORKSPACE");
                ImGui::Separator();
                if (studio::TutorialWidgets::MenuItem("main", global_search_fr ?
                        "Restaurer la disposition" : "Reset workspace layout"))
                    reset_layout = true;
                ImGui::Spacing();
                ImGui::TextDisabled("%s", global_search_fr ? "INSPECTION" : "INSPECTION");
                ImGui::Separator();
                studio::TutorialWidgets::MenuItem("main", global_search_fr ?
                    "Diagnostics des cartes" : "Map diagnostics", nullptr, &show_map_diagnostics);
                ImGui::Separator();
                ImGui::MenuItem(global_search_fr ? "Console des activités..." : "Activity console...", nullptr, &show_activity_console);
                ImGui::EndMenu();
            }
            // Settings is a direct button: Preferences is the single settings window.
            if (ImGui::MenuItem(global_search_fr ? "Paramètres" : "Settings"))
                show_app_preferences = true;
            if (ImGui::BeginMenu(studio::localization::tr("Project"))) {
                projects.menu(area);
                conversation_editor.menu(preferences);
                ImGui::EndMenu();
            }
            tutorials.menu();
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();
        // Row 2: substantial workspace tabs, with quick controls anchored to the right.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 5.f));
        const ImGuiWindowFlags navigation_flags = ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::BeginViewportSideBar("##usum_workspace_bar", ImGui::GetMainViewport(),
                                       ImGuiDir_Up, ImGui::GetFrameHeight() + 13.f,
                                       navigation_flags)) {
            int workspace = data_workspace        ? 9
                            : warp_ride_workspace   ? 8
                            : image_workspace     ? 6
                            : audio_workspace     ? 7
                            : camera_workspace    ? 5
                            : collision_workspace ? 4
                            : authoring_workspace ? 3
                            : studio_workspace    ? 2
                                                  : int(model_workspace);

            // Reserve space so quick actions never overlap the workspace navigation.
            const float reserve_quick = 4.f * 43.f + 22.f;
            int chosen_workspace = workspace_selector(workspace, reserve_quick);
            if (queued_workspace >= 0) {
                chosen_workspace = queued_workspace;
                queued_workspace = -1;
            }
            if (queued_library_category > 0) {
                studio::activity_log::add("Models", "Open library category " + std::to_string(queued_library_category) + " member " + std::to_string(queued_library_member));
                if (source[0]) models.open_library_member(
                    std::filesystem::u8path(source), queued_library_category,
                    queued_library_member, source_archives);
                queued_library_category = -1;
            }
            if (queued_model_species >= 0) {
                // ModelWorkspace resolves the requested species/form asynchronously.
                // It also reports missing variants rather than opening the wrong model.
                if (source[0] != '\0') {
                    studio::activity_log::add("Models", "Open Pokemon #" + std::to_string(queued_model_species) + " form " + std::to_string(queued_model_form));
                    models.open_species(std::filesystem::u8path(source),
                        static_cast<unsigned>(queued_model_species), queued_model_form,
                        queued_model_female, false, source_archives);
                }
                queued_model_species = -1;
            }
            if (queued_map_workspace >= 0) {
                const int target_area = queued_map_area;
                const int target_zone = queued_map_zone;
                const int destination = queued_map_workspace;
                queued_map_workspace = -1;
                queued_map_area = -1;
                queued_map_zone = -1;
                studio::activity_log::add("Maps", "Open area " + std::to_string(target_area) + " zone " + std::to_string(target_zone) + (destination == 0 ? " in Maps" : " in Authoring"));
                if (target_area >= 0 && !source[0]) {
                    camera_notice = global_search_fr ?
                        "Selectionnez d'abord un dump USUM." : "Select a USUM dump first.";
                } else if (target_area >= 0) {
                    if (destination == 0) {
                        // Go through the existing map loader: it handles pending edits.
                        load(false, [&, target_area, target_zone] {
                            area = target_area;
                            selected_zone = target_zone;
                        });
                    } else {
                        // Use the map-authoring API rather than only changing tabs.
                        authoring.open_template(std::filesystem::u8path(source),
                                                static_cast<unsigned>(target_area));
                        area = target_area;
                        selected_zone = target_zone;
                    }
                }
            }
            if (chosen_workspace != workspace) {
                const char *names[] = {"Maps", "Models", "Studio", "Authoring", "Collision", "Cameras", "Images", "Audio", "Warp Ride", "Data"};
                studio::activity_log::add("Navigation", std::string("Open workspace: ") + names[std::clamp(chosen_workspace, 0, 9)]);
                if (chosen_workspace == 0)
                    authored_collision = false;
                data_workspace = chosen_workspace == 9;
                warp_ride_workspace = chosen_workspace == 8;
                image_workspace = chosen_workspace == 6;
                audio_workspace = chosen_workspace == 7;
                camera_workspace = chosen_workspace == 5;
                collision_workspace = chosen_workspace == 4;
                authoring_workspace = chosen_workspace == 3;
                studio_workspace = chosen_workspace == 2;
                model_workspace = chosen_workspace == 1;
                camera.stop_look(window);
            }

            // Buttons use the existing project logic, never duplicate save/stage code.
            constexpr float quick_width = 4.f * 43.f + 3.f * 8.f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                          ImGui::GetWindowWidth() - quick_width - 5.f));
            projects.quick_actions(true);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        // Row 3: low-profile contextual strip. Keep workspaces' own tools unchanged.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 2.f));
        if (ImGui::BeginViewportSideBar("##usum_context_bar", ImGui::GetMainViewport(),
                                       ImGuiDir_Up, ImGui::GetFrameHeight() + 3.f,
                                       navigation_flags)) {
            const char *spaces[] = {"Maps", "Models", "Studio", "Authoring", "Collision",
                "Cameras", "Images", "Audio", "Warp Ride", "Data"};
            const int active_space = std::clamp(data_workspace ? 9 : warp_ride_workspace ? 8 : image_workspace ? 6 : audio_workspace ? 7 : camera_workspace ? 5 : collision_workspace ? 4 : authoring_workspace ? 3 : studio_workspace ? 2 : int(model_workspace), 0, 9);
            if (previous_breadcrumb_workspace != active_space) {
                if (previous_breadcrumb_workspace >= 0 &&
                    (workspace_history.empty() || workspace_history.back() != previous_breadcrumb_workspace))
                    workspace_history.push_back(previous_breadcrumb_workspace);
                if (workspace_history.size() > 32) workspace_history.erase(workspace_history.begin());
                previous_breadcrumb_workspace = active_space;
            }
            if (ImGui::SmallButton("<##breadcrumb_back")) {
                if (!workspace_history.empty()) {
                    queued_workspace = workspace_history.back();
                    workspace_history.pop_back();
                    previous_breadcrumb_workspace = -1;
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", global_search_fr ? "Espace precedent" : "Previous workspace");
            ImGui::SameLine();
            if (ImGui::SmallButton(spaces[active_space])) ImGui::OpenPopup("##breadcrumb_spaces");
            if (ImGui::BeginPopup("##breadcrumb_spaces")) {
                for (int i = 0; i < 10; ++i)
                    if (ImGui::MenuItem(spaces[i], nullptr, active_space == i)) queued_workspace = i;
                ImGui::EndPopup();
            }
            if (active_space == 0 || active_space == 3) {
                ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                const std::string area_name = (global_search_fr ? "Zone " : "Area ") + std::to_string(area);
                if (ImGui::SmallButton(area_name.c_str())) queued_workspace = 0;
                if (selected_zone >= 0) {
                    ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                    ImGui::TextUnformatted((std::string(global_search_fr ? "Secteur " : "Zone ") + std::to_string(selected_zone)).c_str());
                }
            }
            ImGui::SameLine();
            if (studio::feature_visibility::universal_search)
                ImGui::TextDisabled("  |  %s", global_search_fr ?
                    "Ctrl+K : rechercher une ressource" : "Ctrl+K: find a resource");
        }
        ImGui::End();
        ImGui::PopStyleVar();
        // Single categorized preferences window, reusing existing persisted settings.
        if (show_app_preferences) {
            const bool fr = studio::localization::current == studio::localization::Language::French;
            ImGui::SetNextWindowSize(ImVec2(780, 520), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(fr ? "Préférences###studio_preferences" :
                                  "Preferences###studio_preferences", &show_app_preferences)) {
                ImGui::BeginChild("##preferences_categories", ImVec2(205, -1), true);
                ImGui::TextDisabled("%s", fr ? "CATEGORIES" : "CATEGORIES");
                ImGui::Separator();
                const char *fr_pages[] = {"Général", "Langue", "Raccourcis clavier", "Prévisualisation 3DS", "Fonctionnalités"};
                const char *en_pages[] = {"General", "Language", "Keyboard controls", "3DS preview", "Features"};
                for (int i = 0; i < 5; ++i)
                    if (ImGui::Selectable(fr ? fr_pages[i] : en_pages[i], preference_page == i))
                        preference_page = i;
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("##preferences_content", ImVec2(0, -1), false);
                ImGui::SeparatorText(fr ? fr_pages[preference_page] : en_pages[preference_page]);
                if (preference_page == 0) {
                    if (ImGui::Checkbox(fr ? "Afficher les identifiants des modeles ([1960])" :
                                           "Show model identifiers ([1960])",
                                        &studio::feature_visibility::model_ids)) {
                        preferences.set("interface.model_ids", studio::feature_visibility::model_ids ? "1" : "0");
                        preferences.save();
                    }
                    ImGui::TextDisabled("%s", fr ? "Change uniquement l’affichage dans Models ; conserve les noms et alias." :
                                                "Only changes labels in Models; names and aliases are preserved.");
                    ImGui::Spacing();
                    if (ImGui::Checkbox(fr ? "Notifications de la console" : "Console notifications",
                                        &show_console_notifications)) {
                        preferences.set("interface.console_notifications", show_console_notifications ? "1" : "0");
                        preferences.save();
                    }
                    ImGui::TextDisabled("%s", fr ?
                        "Masque les notifications temporaires sans arrêter le journal ni les erreurs." :
                        "Hides pop-up notifications; activity history and errors remain available.");
                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", fr ?
                        "Les préférences sont sauvegardées automatiquement. Choisissez une catégorie à gauche." :
                        "Preferences save automatically. Choose a category on the left.");
                } else if (preference_page == 1) {
                    if (ImGui::RadioButton("Français", fr)) {
                        studio::localization::set_language("fr");
                        preferences.set("language", "fr"); preferences.save();
                    }
                    if (ImGui::RadioButton("English", !fr)) {
                        studio::localization::set_language("en");
                        preferences.set("language", "en"); preferences.save();
                    }
                } else if (preference_page == 2) {
                    const bool fr = studio::localization::current == studio::localization::Language::French;
                    ImGui::PushID("embedded_keyboard");
                const char *categories_fr[] = {"Personnage", "Caméra 3D", "Maps et placements", "Modèles 3D", "UV / Textures", "Collisions"};
                const char *categories_en[] = {"Player", "3D Camera", "Maps and placements", "3D Models", "UV / Textures", "Collisions"};
                const int category_starts[] = {0, 6, 12, 22, 30, 34, int(sizeof(bindings) / sizeof(bindings[0]))};
                // Sidebar with explicit categories; stable internal identifiers survive language switching.
                ImGui::BeginChild("##keysettings_navigation", ImVec2(150, -1), true);
                ImGui::TextUnformatted(fr ? "COMMANDES" : "CONTROLS");
                ImGui::Separator();
                for (int category = 0; category < 6; ++category) {
                    const char *title = fr ? categories_fr[category] : categories_en[category];
                    if (ImGui::Selectable(title, key_settings_category == category)) {
                        key_settings_category = category;
                        capturing_binding = -1;
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild("##keysettings_content", ImVec2(0, -1), false);
                ImGui::SeparatorText(fr ? categories_fr[key_settings_category] : categories_en[key_settings_category]);
                ImGui::TextWrapped("%s", fr ? "Cliquez sur Modifier, puis appuyez sur la touche souhaitée. Enregistrement automatique."
                                             : "Click Change, then press the desired key. Changes are saved automatically.");
                ImGui::Spacing();
                for (int i = category_starts[key_settings_category]; i < category_starts[key_settings_category + 1]; ++i) {
                    ImGui::PushID(i);
                    const auto &b = bindings[i];
                    ImGui::TextUnformatted(fr ? b.french : b.english);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(ImGui::GetKeyName(b.key));
                    ImGui::SameLine();
                    if (ImGui::Button(capturing_binding == i ? (fr ? "Touche..." : "Press key...")
                                                             : (fr ? "Modifier" : "Change"))) {
                        capturing_binding = i;
                        skip_capture_frame = true;
                    }
                    ImGui::PopID();
                }
                if (capturing_binding >= 0) {
                    ImGui::Separator();
                    ImGui::TextUnformatted(fr ? "En attente d'une touche..." : "Waiting for a key...");
                    if (skip_capture_frame) skip_capture_frame = false;
                    else for (int k = int(ImGuiKey_NamedKey_BEGIN); k < int(ImGuiKey_NamedKey_END); ++k) {
                        if (!ImGui::IsKeyPressed(ImGuiKey(k))) continue;
                        bindings[capturing_binding].key = ImGuiKey(k);
                        studio::configurable_keys::set(bindings[capturing_binding].id, ImGuiKey(k));
                        preferences.set(std::string("keybind.") + bindings[capturing_binding].id, std::to_string(k));
                        preferences.save();
                        capturing_binding = -1;
                        break;
                    }
                }
                ImGui::Separator();
                if (ImGui::Button(fr ? "Rétablir toutes les touches par défaut" : "Restore all default bindings")) {
                    for (auto &b : bindings) {
                        b.key = b.default_key;
                        studio::configurable_keys::set(b.id, b.key);
                        preferences.set(std::string("keybind.") + b.id, std::to_string(int(b.key)));
                    }
                    preferences.save();
                    capturing_binding = -1;
                }
                ImGui::EndChild();
                    ImGui::PopID();
                } else if (preference_page == 3) {
                    if (ImGui::Checkbox(fr ? "Aperçu 3DS (400 x 240)" :
                                             "3DS preview (400 x 240)",
                                         &studio::ImGuiRenderer::preview_3ds)) {
                        preferences.set("preview_3ds", studio::ImGuiRenderer::preview_3ds ? "1" : "0");
                        preferences.save();
                    }
                    ImGui::BeginDisabled(!studio::ImGuiRenderer::preview_3ds);
                    if (ImGui::Checkbox(fr ? "Taille native" : "Native size",
                                         &studio::ImGuiRenderer::preview_native_size)) {
                        preferences.set("preview_native_size",
                            studio::ImGuiRenderer::preview_native_size ? "1" : "0");
                        preferences.save();
                    }
                    ImGui::EndDisabled();
                } else if (preference_page == 4) {
                    ImGui::TextWrapped("%s", fr ?
                        "Masquer une fonctionnalite ne supprime aucune donnee. Vos noms, favoris et dossiers seront retrouves lors de sa reactivation." :
                        "Hiding a feature never deletes its data. Names, favorites and folders are preserved until you enable it again.");
                    ImGui::Spacing();
                    auto option = [&](const char *label, const char *description,
                                      const char *key, bool &enabled) {
                        if (ImGui::Checkbox(label, &enabled)) {
                            preferences.set(key, enabled ? "1" : "0");
                            preferences.save();
                        }
                        ImGui::TextDisabled("%s", description);
                        ImGui::Spacing();
                    };
                    option(fr ? "Dossiers et sous-dossiers" : "Folders and subfolders",
                           fr ? "Masque les filtres et commandes de classement." :
                                "Hide folder filters and organizing controls.",
                           "features.model_folders", studio::feature_visibility::folders);
                    option(fr ? "Recherche universelle (Ctrl+K)" : "Universal search (Ctrl+K)",
                           fr ? "Masque l'acces et desactive le raccourci." :
                                "Hide the command and disable the shortcut.",
                           "features.universal_search", studio::feature_visibility::universal_search);
                    option(fr ? "Favoris des modeles" : "Model favorites",
                           fr ? "Masque les etoiles et le filtre des favoris." :
                                "Hide favorite buttons and filtering.",
                           "features.model_favorites", studio::feature_visibility::favorites);
                    option(fr ? "Renommage des modeles" : "Model renaming",
                           fr ? "Affiche les noms originaux sans effacer les alias." :
                                "Show original names without deleting aliases.",
                           "features.model_renaming", studio::feature_visibility::renaming);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        if (show_activity_console) {
            const bool fr = studio::localization::current == studio::localization::Language::French;
            ImGui::SetNextWindowSize(ImVec2(820, 440), ImGuiCond_FirstUseEver);
            if (ImGui::Begin(fr ? "Console des activités###activity_console" : "Activity console###activity_console", &show_activity_console)) {
                ImGui::Checkbox(fr ? "Infos" : "Info", &console_info); ImGui::SameLine();
                ImGui::Checkbox(fr ? "Avertissements" : "Warnings", &console_warnings); ImGui::SameLine();
                ImGui::Checkbox(fr ? "Erreurs" : "Errors", &console_errors);
                ImGui::SetNextItemWidth(235);
                ImGui::InputTextWithHint("##activity_filter", fr ? "Rechercher dans le journal..." : "Search log...", console_search, sizeof(console_search));
                ImGui::SameLine();
                if (ImGui::Button(fr ? "Copier" : "Copy")) {
                    std::string report;
                    for (const auto &line : studio::activity_log::snapshot())
                        report += "[" + line.time + "] [" + line.category + "] " + line.message + "\n";
                    ImGui::SetClipboardText(report.c_str());
                }
                ImGui::SameLine();
                if (ImGui::Button(fr ? "Effacer" : "Clear")) studio::activity_log::clear();
                ImGui::SameLine();
                ImGui::Checkbox(fr ? "Défilement auto" : "Auto-scroll", &console_auto_scroll);
                ImGui::Separator();
                if (ImGui::BeginChild("##activity_rows", ImVec2(0, 0), true)) {
                    const std::string query = console_search;
                    for (const auto &line : studio::activity_log::snapshot()) {
                        if ((line.level == studio::activity_log::Level::Info && !console_info) ||
                            (line.level == studio::activity_log::Level::Warning && !console_warnings) ||
                            (line.level == studio::activity_log::Level::Error && !console_errors)) continue;
                        const std::string row = "[" + line.time + "] [" + line.category + "] " + line.message;
                        if (!query.empty() && row.find(query) == std::string::npos) continue;
                        if (line.level == studio::activity_log::Level::Error)
                            ImGui::TextColored(ImVec4(1.f,.44f,.44f,1.f), "%s", row.c_str());
                        else if (line.level == studio::activity_log::Level::Warning)
                            ImGui::TextColored(ImVec4(1.f,.77f,.33f,1.f), "%s", row.c_str());
                        else ImGui::TextUnformatted(row.c_str());
                    }
                    if (console_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 24.f)
                        ImGui::SetScrollHereY(1.f);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        // Global status and compact notifications, sourced from the real activity log.
        {
            const auto entries = studio::activity_log::snapshot();
            if (entries.size() != last_toast_count) {
                last_toast_count = entries.size();
                if (!entries.empty()) {
                    const auto &event = entries.back();
                    last_toast_message = "[" + event.category + "] " + event.message;
                    last_toast_time = ImGui::GetTime();
                }
            }
            const auto *vp = ImGui::GetMainViewport();
            // The former status window overlaid the editor viewport. Keep the
            // activity log and notifications, but do not create that overlay.
            if (show_console_notifications && !last_toast_message.empty() && ImGui::GetTime() - last_toast_time < 4.0 && !show_activity_console) {
                ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 405,
                    vp->WorkPos.y + vp->WorkSize.y - 85), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(395, 45), ImGuiCond_Always);
                if (ImGui::Begin("##studio_activity_toast", nullptr, ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoDocking)) {
                    ImGui::TextWrapped("%s", last_toast_message.c_str());
                    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) show_activity_console = true;
                }
                ImGui::End();
            }
        }
        if (show_map_diagnostics) {
            ImGui::SetNextWindowSize(ImVec2(680, 360), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Map diagnostics", &show_map_diagnostics)) {
                if (!scene)
                    ImGui::TextUnformatted(studio::localization::tr("Load a map to view diagnostics."));
                else if (scene->diagnostics.empty())
                    ImGui::TextUnformatted(studio::localization::tr("No loading messages."));
                else {
                    if (studio::TutorialWidgets::Button("main", studio::localization::tr("Copy details"))) {
                        std::string details;
                        for (const auto &note : scene->diagnostics)
                            details += note + '\n';
                        SDL_SetClipboardText(details.c_str());
                    }
                    ImGui::Separator();
                    for (const auto &note : scene->diagnostics) {
                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", note.c_str());
                    }
                }
            }
            ImGui::End();
        }
        layout(data_workspace        ? 9
               : warp_ride_workspace   ? 8
               : image_workspace     ? 6
               : audio_workspace     ? 7
               : camera_workspace    ? 5
               : collision_workspace ? 4
               : authoring_workspace ? 3
               : studio_workspace    ? 2
                                     : int(model_workspace),
               reset_layout);
        field_systems.update(!warp_ride_workspace && !image_workspace && !audio_workspace &&
                             !authoring_workspace && !studio_workspace && !model_workspace &&
                             !camera_workspace && !collision_workspace);
        conversation_editor.update(!warp_ride_workspace && !image_workspace && !audio_workspace &&
                                   !authoring_workspace && !studio_workspace && !model_workspace &&
                                   !camera_workspace && !collision_workspace);
        audio_editor.update(audio_workspace);
        cry_editor.update(!image_workspace &&
                          (audio_workspace ? audio_editor.cries_active()
                                           : !camera_workspace && !collision_workspace &&
                                                 !authoring_workspace &&
                                                 (studio_workspace || model_workspace)));
        map_music.update(!warp_ride_workspace && !image_workspace && !audio_workspace &&
                         !authoring_workspace && !studio_workspace && !model_workspace);
        if (authoring.take_collision_request()) {
            authored_collision = true;
            collision_workspace = true;
            authoring_workspace = false;
        }
        if (!authoring.has_collision())
            authored_collision = false;
        authoring.collision_tick(collision_workspace && authored_collision);
        collision.activate(collision_workspace && !authored_collision);
        auto stats = bgfx::getStats();
        warp_ride.activate(warp_ride_workspace && !projects.gate());
        if (projects.gate()) {
        } else if (data_workspace) {
            // Populate the editor data before drawing the associated browser.
            if (selected_data_editor == 0)
                personal_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 1)
                item_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 2)
                text_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 3)
                evolution_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 4)
                encounter_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 5)
                move_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 6)
                learnset_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 7)
                trainer_data_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 8)
                trainer_pokemon_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 9) egg_move_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 10) mega_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 11) trainer_class_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 12) tm_tutor_viewer.draw(std::filesystem::u8path(source), true);
            else if (selected_data_editor == 13) shop_viewer.draw(std::filesystem::u8path(source), true);
            if (ImGui::Begin(studio::localization::bilingual("Data browser###Data browser", "Navigateur de données###Data browser"))) {
                // Selector is shared with future editors (evolutions, moves, etc.).
                const char* data_editors[] = {"Personal Data", "Item Data", "Text Data", "Evolution Data", "Encounter Data", "Move Data", "Learnset Data", "Trainer Data", "Trainer Pokemon", "Egg Move Data", "Mega Evolution Data", "Trainer Class Data", "TM / Tutor Data", "Shop / Mart Data"};
                ImGui::SetNextItemWidth(-1.f);
                const char* data_editor_labels[] = {studio::localization::tr("Personal Data"), studio::localization::tr("Item Data"), studio::localization::tr("Text Data"), studio::localization::tr("Evolution Data"), studio::localization::tr("Encounter Data"), studio::localization::tr("Move Data"), studio::localization::tr("Learnset Data"), studio::localization::bilingual("Trainer Data", "Donnees des dresseurs"), studio::localization::bilingual("Trainer Pokemon", "Pokemon des dresseurs"), studio::localization::bilingual("Egg Move Data", "Capacites oeuf"), studio::localization::bilingual("Mega Evolution Data", "Mega-Evolutions"), studio::localization::bilingual("Trainer Class Data", "Classes des dresseurs"), studio::localization::bilingual("TM / Tutor Data", "CT / Maitres des capacites"), studio::localization::bilingual("Shop / Mart Data", "Boutiques")};
                if (ImGui::Combo("##data-editor", &selected_data_editor, [](void* data, int idx, const char** out) { *out = static_cast<const char**>(data)[idx]; return true; }, data_editor_labels, 14))
                    studio::activity_log::add("Data", std::string("Open editor: ") + data_editors[selected_data_editor]);
                ImGui::Separator();
                if (selected_data_editor == 0)
                    personal_data_viewer.draw_browser();
                else if (selected_data_editor == 1)
                    item_data_viewer.draw_browser();
                else if (selected_data_editor == 2)
                    text_data_viewer.draw_browser();
                else if (selected_data_editor == 3)
                    evolution_data_viewer.draw_browser();
                else if (selected_data_editor == 4)
                    encounter_data_viewer.draw_browser();
                else if (selected_data_editor == 5)
                    move_data_viewer.draw_browser();
                else if (selected_data_editor == 6)
                    learnset_data_viewer.draw_browser();
                else if (selected_data_editor == 7)
                    trainer_data_viewer.draw_browser();
                else if (selected_data_editor == 8)
                    trainer_pokemon_viewer.draw_browser();
                else if (selected_data_editor == 9) egg_move_viewer.draw_browser(std::filesystem::u8path(source));
                else if (selected_data_editor == 10) mega_viewer.draw_browser(std::filesystem::u8path(source));
                else if (selected_data_editor == 11) trainer_class_viewer.draw_browser(std::filesystem::u8path(source));
                else if (selected_data_editor == 12) tm_tutor_viewer.draw_browser(std::filesystem::u8path(source));
                else if (selected_data_editor == 13) shop_viewer.draw_browser(std::filesystem::u8path(source));
            }
            ImGui::End();
        } else if (warp_ride_workspace) {
            warp_ride.draw(std::filesystem::u8path(source));
        } else if (image_workspace) {
            image_editor.draw(std::filesystem::u8path(source));
        } else if (audio_workspace) {
            audio_editor.draw(std::filesystem::u8path(source));
        } else if (camera_workspace) {
            if (cameras.draw_workspace(job.valid()))
                camera_workspace = false;
        } else if (collision_workspace) {
            if (authored_collision) {
                if (authoring.draw_collision()) {
                    collision_workspace = false;
                    authoring_workspace = true;
                }
            } else if (collision.draw_workspace(job.valid()))
                collision_workspace = false;
        } else if (authoring_workspace) {
            if (!source_archives.field.empty() || !source_archives.terrain.empty()) {
                ImGui::Begin("Composition");
                ImGui::TextWrapped("Authoring uses archives from the dump folder. External field "
                                   "and terrain selections currently apply to Maps only.");
                ImGui::End();
            }
            authoring.draw(graphics_frame, std::filesystem::u8path(source),
                           unsigned(std::max(area, 0)));
            if (!studio_notice.empty()) {
                ImGui::Begin("Authoring tools");
                ImGui::TextWrapped("%s", studio_notice.c_str());
                ImGui::End();
            }
        } else if (studio_workspace) {
            asset_studio.draw(graphics_frame, source);
            if (asset_studio.take_character_stage_request())
                projects.stage_and_reload();
            if (!studio_notice.empty()) {
                ImGui::Begin("Studio inspector");
                ImGui::TextWrapped("%s", studio_notice.c_str());
                ImGui::End();
            }
        } else if (model_workspace) {
            models.draw(graphics_frame, source, source_archives);
            if (models.take_character_stage_request())
                projects.stage_and_reload();
            if (!studio::project_store() && models.browsing_pokemon() &&
                models.source_details_visible()) {
                ImGui::Begin("Model inspector");
                if (studio::TutorialWidgets::CollapsingHeader("main", "Pokemon source archive"))
                    archive_controls(false, true);
                ImGui::End();
            }
        } else {

            auto map_settings = [&] {
                if (auto *project = studio::project_store()) {
                    ImGui::TextWrapped("Project: %s", project->root.filename().string().c_str());
                } else {
                    ImGui::TextUnformatted(studio::localization::tr("Extracted dump folder"));
                    ImGui::BeginDisabled(false);
                    ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() - style.ItemSpacing.x);
                    if (ImGui::InputText("##dump", source, sizeof(source)))
                        settings_dirty = true;
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        remember();
                    ImGui::SameLine();
                    bool picking;
                    {
                        std::lock_guard lock(folder->mutex);
                        picking = folder->pending;
                    }
                    ImGui::BeginDisabled(picking);
                    auto icon = ImGui::GetCursorScreenPos();
                    auto button_size = ImGui::GetFrameHeight();
                    bool browse = studio::TutorialWidgets::Button("main", "##choose-folder",
                                                                  ImVec2(button_size, button_size));
                    auto *draw = ImGui::GetWindowDrawList();
                    auto tint = ImGui::GetColorU32(ImGuiCol_Text);
                    draw->AddRect(ImVec2(icon.x + button_size * .2f, icon.y + button_size * .36f),
                                  ImVec2(icon.x + button_size * .8f, icon.y + button_size * .75f),
                                  tint, 2);
                    draw->AddLine(ImVec2(icon.x + button_size * .2f, icon.y + button_size * .36f),
                                  ImVec2(icon.x + button_size * .2f, icon.y + button_size * .24f),
                                  tint);
                    draw->AddLine(ImVec2(icon.x + button_size * .2f, icon.y + button_size * .24f),
                                  ImVec2(icon.x + button_size * .46f, icon.y + button_size * .24f),
                                  tint);
                    draw->AddLine(ImVec2(icon.x + button_size * .46f, icon.y + button_size * .24f),
                                  ImVec2(icon.x + button_size * .55f, icon.y + button_size * .36f),
                                  tint);
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("Choose game dump folder");
                    if (browse) {
                        folder_error.clear();
                        studio::choose_folder(window, folder, source);
                    }
                    ImGui::EndDisabled();
                }
                if (!folder_error.empty())
                    ImGui::TextWrapped("%s", folder_error.c_str());
                if (!preferences.error.empty())
                    ImGui::TextWrapped("%s", preferences.error.c_str());
                if (studio::TutorialWidgets::TreeNode("main", "Advanced selection")) {
                    if (ImGui::InputInt("Area index", &area)) {
                        selected_zone = -1;
                        settings_dirty = true;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                        remember();
                    ImGui::TreePop();
                }
                if (studio::TutorialWidgets::Checkbox("main", "Start at map spawn",
                                                      &start_at_spawn)) {
                    preferences.set("start_at_spawn", start_at_spawn ? "1" : "0");
                    preferences.save();
                    if (start_at_spawn)
                        focus_spawn();
                    else
                        camera_notice.clear();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "Start near the selected zone's default position when loading. "
                        "Enabling also moves the current view.");
                ImGui::BeginDisabled(job.valid() || area < 0 || source[0] == 0);
                if (studio::primary_button("Load map"))
                    load();
                ImGui::EndDisabled();
                if (job.valid()) {
                    ImGui::SameLine();
                    if (studio::TutorialWidgets::Button("main", studio::localization::tr("Cancel")))
                        cancel = true;
                }
                if (job.valid() || !scene)
                    ImGui::TextWrapped("%s", status.c_str());
                if (scene) {
                    auto loaded = std::find_if(scene->locations.begin(), scene->locations.end(),
                                               [&](auto &m) {
                                                   return m.zone == loaded_zone;
                                               });
                    if (loaded != scene->locations.end())
                        ImGui::TextWrapped("In viewport: %s", loaded->name.c_str());
                    else
                        ImGui::Text("In viewport: area %d", loaded_area);
                    if (studio::TutorialWidgets::TreeNode("main", "Loaded archive sources")) {
                        ImGui::TextWrapped(
                            "Loaded field: %s",
                            scene->archive_sources
                                .resolve(loaded_dump, studio::TargetProfile::field_archive)
                                .string()
                                .c_str());
                        ImGui::TextWrapped(
                            "Loaded terrain: %s",
                            scene->archive_sources
                                .resolve(loaded_dump, studio::TargetProfile::terrain_archive)
                                .string()
                                .c_str());
                        ImGui::TreePop();
                    }
                    if (area != loaded_area || selected_zone != loaded_zone ||
                        loaded_dump != std::filesystem::u8path(source) ||
                        scene->archive_sources.field != source_archives.field ||
                        scene->archive_sources.terrain != source_archives.terrain)
                        ImGui::TextWrapped(
                            "Selection changed. Choose Load map to update the viewport.");
                }
                if (!failure.empty())
                    ImGui::TextWrapped("%s", failure.c_str());
                if (!camera_notice.empty())
                    ImGui::TextWrapped("%s", camera_notice.c_str());
                if (!studio::project_store()) {
                    ImGui::Separator();
                    if (studio::TutorialWidgets::CollapsingHeader(
                            "main", "Source archives",
                            (!source_archives.field.empty() || !source_archives.terrain.empty())
                                ? ImGuiTreeNodeFlags_DefaultOpen
                                : 0))
                        archive_controls(true, false);
                }
            };
            auto environment_controls = [&] {
                {
                    int mode = renderer.lighting.enabled ? (renderer.lighting.game ? 0 : 1) : 2;
                    if (ImGui::Combo("Source", &mode, "Game lights\0Preview light\0Neutral\0")) {
                        renderer.lighting.enabled = mode != 2;
                        renderer.lighting.game = mode == 0;
                        preferences.set("lighting_source", mode == 0   ? "game"
                                                           : mode == 1 ? "preview"
                                                                       : "neutral");
                        preferences.save();
                    }
                    if (mode == 0) {
                        if (scene && !scene->lighting_contexts.empty()) {
                            int context = int(renderer.lighting.context);
                            std::string selected =
                                "Zone " +
                                std::to_string(scene
                                                   ->lighting_contexts[std::min(
                                                       std::size_t(context),
                                                       scene->lighting_contexts.size() - 1)]
                                                   .zone);
                            if (ImGui::BeginCombo("Lighting zone", selected.c_str())) {
                                for (unsigned i = 0; i < scene->lighting_contexts.size(); ++i) {
                                    auto label =
                                        "Zone " + std::to_string(scene->lighting_contexts[i].zone);
                                    if (ImGui::Selectable(label.c_str(),
                                                          i == renderer.lighting.context)) {
                                        renderer.lighting.context = i;
                                        renderer.lighting.motion = 0;
                                        weather_profile = -1;
                                        renderer.weather_effect = 0;
                                        renderer.sky_type = 0;
                                    }
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SliderFloat("Time of day", &renderer.lighting.hour, 0.f, 24.f,
                                               "%.2f h");
                            auto &environment = scene->lighting_contexts[renderer.lighting.context];
                            weather_editor.draw(environment, renderer, weather_profile);
                        } else
                            ImGui::TextWrapped("Map lighting unavailable; using preview lighting.");
                    } else if (mode == 1) {
                        ImGui::SliderFloat("Ambient fill", &renderer.lighting.ambient, 0.f, 1.f);
                        ImGui::SliderFloat("Light strength", &renderer.lighting.strength, 0.f, 2.f);
                        ImGui::SliderFloat("Highlights", &renderer.lighting.highlights, 0.f, 1.f);
                    }
                    bool effects_changed =
                        studio::TutorialWidgets::Checkbox("main", studio::localization::tr("Fog"), &renderer.fog_enabled);
                    effects_changed |=
                        studio::TutorialWidgets::Checkbox("main", studio::localization::tr("Bloom"), &renderer.bloom_enabled);
                    if (effects_changed) {
                        preferences.set("fog_enabled", renderer.fog_enabled ? "1" : "0");
                        preferences.set("bloom_enabled", renderer.bloom_enabled ? "1" : "0");
                        preferences.save();
                    }

                    ImGui::Checkbox("PICA float24 UV precision", &renderer.pica_texture_precision);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Round texture coordinates to 16 fraction bits before sampling. "
                                          "Experimental approximation; preview only.");
                    ImGui::BeginDisabled(mode == 2);
                    studio::TutorialWidgets::Checkbox("main", "Normal maps",
                                                      &renderer.lighting.normal_maps);
                    ImGui::EndDisabled();
                }
                map_music.draw(scene.get(), loaded_dump, loaded_zone);
            };
            auto player_controls = [&] {
                {
                    int appearance = int(renderer.player.appearance);
                    ImGui::BeginDisabled(renderer.player.active);
                    if (ImGui::Combo("Appearance", &appearance, "Boy\0Girl\0"))
                        renderer.player.appearance = unsigned(appearance);
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(!scene || !renderer.ready() ||
                                         !scene->player_available[renderer.player.appearance]);
                    if (studio::TutorialWidgets::Button("main", renderer.player.active
                                                                    ? "Return to free camera"
                                                                    : "Play from map start")) {
                        if (renderer.player.active)
                            renderer.player.active = false;
                        else
                            start_player();
                    }
                    if (studio::TutorialWidgets::Button("main", "Start at view target") && scene) {
                        camera.stop_look(window);
                        renderer.player.start(scene->spatial, camera.target, loaded_zone);
                    }
                    ImGui::EndDisabled();
                    if (scene && !scene->player_available[renderer.player.appearance])
                        ImGui::TextWrapped("Outfit unavailable. See View > Map diagnostics.");
                    if (renderer.player.active) {
                        ImGui::TextWrapped("WASD: walk | Ctrl: run | Shift: through "
                                           "walls\nCtrl + wheel: running "
                                           "speed\nEsc: free camera | R: reset to map start");
                        ImGui::Text("Running: %.2fx (%.0f units/s)", renderer.player.run_multiplier,
                                    360 * renderer.player.run_multiplier);
                        ImGui::Text("Zone %d | Camera region %d", renderer.player.zone,
                                    renderer.player.camera_region);
                        ImGui::Text("Position %.1f, %.1f, %.1f", renderer.player.position[0],
                                    renderer.player.position[1], renderer.player.position[2]);
                    }
                    if (!renderer.player.notice.empty())
                        ImGui::TextWrapped("%s", renderer.player.notice.c_str());
                    ImGui::TextWrapped("Preview movement with ground, walls and map cameras.");
                }
            };
            auto display_controls = [&] {
                if (studio::TutorialWidgets::Checkbox("main", studio::localization::tr("3DS preview"),
                                                      &studio::ImGuiRenderer::preview_3ds)) {
                    preferences.set("preview_3ds", studio::ImGuiRenderer::preview_3ds ? "1" : "0");
                    preferences.save();
                }
                if (studio::ImGuiRenderer::preview_3ds)
                    if (studio::TutorialWidgets::Checkbox(
                            "main", "Keep 400 x 240 size",
                            &studio::ImGuiRenderer::preview_native_size)) {
                        preferences.set("preview_native_size",
                                        studio::ImGuiRenderer::preview_native_size ? "1" : "0");
                        preferences.save();
                    }
                if (studio::TutorialWidgets::Button("main", studio::localization::tr("Fit environment")) && scene)
                    camera.fit(*scene);
                if (studio::TutorialWidgets::CollapsingHeader("main", "Scene contents")) {
                    bool changed =
                        studio::TutorialWidgets::Checkbox("main", "Skybox", &renderer.sky_enabled);
                    if (scene && renderer.lighting.context < scene->lighting_contexts.size() &&
                        !scene->lighting_contexts[renderer.lighting.context].sky_enabled)
                        ImGui::TextDisabled("This zone has no skybox.");
                    changed |= studio::TutorialWidgets::Checkbox("main", "Weather effects",
                                                                 &renderer.particles_enabled);
                    changed |= studio::TutorialWidgets::Checkbox("main", studio::localization::tr("Characters"),
                                                                 &renderer.characters_enabled);
                    changed |= studio::TutorialWidgets::Checkbox(
                        "main", "Include story-dependent characters",
                        &renderer.conditional_characters);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(
                            "Show placements whose story or version conditions cannot be "
                            "resolved without game state.");
                    changed |= studio::TutorialWidgets::Checkbox(
                        "main", "Mesh visibility animation", &renderer.visibility_enabled);
                    if (changed) {
                        preferences.set("sky_enabled", renderer.sky_enabled ? "1" : "0");
                        preferences.set("characters_enabled",
                                        renderer.characters_enabled ? "1" : "0");
                        preferences.set("conditional_characters",
                                        renderer.conditional_characters ? "1" : "0");
                        preferences.set("particles_enabled",
                                        renderer.particles_enabled ? "1" : "0");
                        preferences.set("visibility_enabled",
                                        renderer.visibility_enabled ? "1" : "0");
                        preferences.save();
                    }
                    if (scene)
                        ImGui::TextDisabled("%zu character placements",
                                            scene->character_placements);
                }
                if (studio::TutorialWidgets::Checkbox("main", "Skeletal animation",
                                                      &renderer.playback.skeletal)) {
                    preferences.set("skeletal_enabled", renderer.playback.skeletal ? "1" : "0");
                    preferences.save();
                }
                if (scene)
                    ImGui::TextDisabled("Animated scene objects: %zu",
                                        std::count_if(scene->skeletons.begin(),
                                                      scene->skeletons.end(), [](const auto &rig) {
                                                          return rig.player < 0;
                                                      }));
                if (studio::TutorialWidgets::CollapsingHeader("main", "Viewport display")) {
                    studio::TutorialWidgets::Checkbox("main", "Vertex colors", &colors);
                    studio::TutorialWidgets::Checkbox("main", "Wireframe", &renderer.wireframe);
                    studio::TutorialWidgets::Checkbox("main", "Preview unsupported inputs",
                                                      &raw_materials);
                }
                if (!renderer.player.active &&
                    studio::TutorialWidgets::CollapsingHeader("main", "Navigation help")) {
                    ImGui::TextWrapped("RMB: look from position\nWASD: fly | Q / E: local down / "
                                       "up\nShift: slow | Ctrl: fast\nCtrl + wheel: fast-flight "
                                       "multiplier\nRMB + wheel: base flight speed\nCtrl + click: "
                                       "select | Left drag: orbit\nMiddle drag: pan\nWheel: zoom");
                    if (!renderer.player.active)
                        ImGui::TextDisabled("Flight: %.0f units/s | Ctrl: %.1fx", camera.speed,
                                            camera.fast_multiplier);
                }
                if (studio::TutorialWidgets::CollapsingHeader("main", studio::localization::tr("Frame pacing"))) {
                    bool changed = studio::TutorialWidgets::Checkbox("main", "Limit FPS",
                                                                     &pacing.limit_enabled);
                    ImGui::BeginDisabled(!pacing.limit_enabled);
                    if (ImGui::InputInt("FPS limit", &pacing.fps)) {
                        pacing.fps = std::clamp(pacing.fps, 1, 1000);
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    changed |= studio::TutorialWidgets::Checkbox(
                        "main", "Reduce background refresh", &pacing.background_enabled);
                    ImGui::BeginDisabled(!pacing.background_enabled);
                    if (ImGui::InputInt("Background FPS", &pacing.background_fps)) {
                        pacing.background_fps = std::clamp(pacing.background_fps, 1, 1000);
                        changed = true;
                    }
                    ImGui::EndDisabled();
                    if (changed)
                        save_pacing();
                    ImGui::Text("Current rate: %.1f FPS", io.Framerate);
                    ImGui::TextWrapped("VSync may cap FPS. Rendering pauses while minimized.");
                }
                if (studio::TutorialWidgets::CollapsingHeader("main", "Performance details")) {
                    ImGui::Text("Backend: %s", bgfx::getRendererName(bgfx::getRendererType()));
                    ImGui::Text("App CPU: %.2f ms", cpu_ms);
                    if (stats->gpuTimerFreq > 0 && stats->gpuTimeEnd >= stats->gpuTimeBegin)
                        ImGui::Text("GPU: %.2f ms",
                                    1000.0 * double(stats->gpuTimeEnd - stats->gpuTimeBegin) /
                                        double(stats->gpuTimerFreq));
                    else
                        ImGui::TextUnformatted("GPU timing: unavailable");
                    ImGui::Text("Submitted draws: %u", stats->numDraw);
                    ImGui::Text("Textures: %.2f MiB (payload)",
                                double(renderer.texture_bytes) / 1048576);
                    ImGui::Text("Geometry: %.2f MiB (payload)",
                                double(renderer.geometry_bytes) / 1048576);
                    if (scene) {
                        ImGui::Text("Showing area: %d", loaded_area);
                        ImGui::Text("Decode: %.1f ms", scene->load_ms);
                        ImGui::Text("Scene draws: %zu / %zu", renderer.uploaded_draws(),
                                    scene->draws.size());
                        ImGui::Text("Texture resources: %zu", scene->textures.size());
                    }
                    ImGui::TextWrapped("Memory totals exclude driver overhead.");
                }
            };
            bool layers_changed = false;
            ImGui::Begin("Map browser");
            ImGui::BeginDisabled(job.valid());
            if (auto chosen = maps.draw(area, selected_zone)) {
                area = chosen->area;
                selected_zone = chosen->zone;
                remember();
                if (!job.valid())
                    load();
            }

            ImGui::EndDisabled();
            if (job.valid())
                ImGui::TextDisabled("Loading map...");
            if (!failure.empty())
                ImGui::TextWrapped("%s", failure.c_str());
            if (ImGui::Button("Map settings", {-1, 0}))
                map_page = studio::MapInspectorPage::Settings;
            ImGui::Separator();
            if (ImGui::Selectable("Scene", !map_layers, 0, {75, 0}))
                map_layers = false;
            ImGui::SameLine();
            if (ImGui::Selectable(studio::localization::tr("Layers"), map_layers, 0, {75, 0}))
                map_layers = true;
            ImGui::Separator();
            const auto before_draw = selection.draw;
            const auto before_region = renderer.spatial.selected;
            if (!map_layers)
                browser.draw(scene.get(), renderer, selection, camera, true);
            ImGui::End();
            map_cursor.draw(scene.get(), camera, map_layers);
            if (map_layers)
                layers_changed |= renderer.spatial.controls(camera, loaded_zone);
            if (selection.draw != before_draw || renderer.spatial.selected != before_region) {
                map_page = studio::MapInspectorPage::Selection;
                map_materials = false;
            }
            const auto *region =
                scene && renderer.spatial.selected >= 0 &&
                        std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()
                    ? &scene->spatial.regions[renderer.spatial.selected]
                    : nullptr;
            const auto *draw =
                scene && selection.draw >= 0 && std::size_t(selection.draw) < scene->draws.size()
                    ? &scene->draws[selection.draw]
                    : nullptr;
            bool properties =
                map_page == studio::MapInspectorPage::Selection && !map_preview && !map_materials;
            bool map_actions = !map_preview && (map_page == studio::MapInspectorPage::Settings ||
                                                (properties && !draw && !region));
            ImGui::Begin("Map inspector");
            if (map_page != studio::MapInspectorPage::Selection || map_preview) {
                if (ImGui::SmallButton("Back to selection")) {
                    map_page = studio::MapInspectorPage::Selection;
                    map_preview = false;
                    renderer.player.active = false;
                }
                ImGui::Separator();
            }
            if (map_page == studio::MapInspectorPage::Environment)
                environment_controls();
            else if (map_page == studio::MapInspectorPage::Display)
                display_controls();
            else if (map_page == studio::MapInspectorPage::Settings) {
                ImGui::SeparatorText("Map settings");
                if (ImGui::Button(studio::localization::tr("Environment & music"), {-1, 0}))
                    map_page = studio::MapInspectorPage::Environment;
                if (ImGui::TreeNode("Loading & source details")) {
                    map_settings();
                    ImGui::TreePop();
                }
            } else if (map_preview) {
                ImGui::SeparatorText("Player preview");
                player_controls();
                if (ImGui::Button(studio::localization::tr("Environment & music"), {-1, 0}))
                    map_page = studio::MapInspectorPage::Environment;
            } else if (draw || region) {
                auto name = draw ? draw->name.substr(0, draw->name.find(" / ")) : region->name;
                ImGui::TextWrapped("%s", name.c_str());
                ImGui::TextDisabled("%s", draw ? (draw->character        ? "Character"
                                                  : draw->placement >= 0 ? "Static model"
                                                                         : "Terrain / scenery")
                                               : studio::spatial_kind_name(region->kind));
                if (ImGui::SmallButton(studio::localization::tr("Clear selection"))) {
                    selection = {};
                    renderer.spatial.selected = -1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Frame (F)"))
                    browser.frame_selection(*scene, renderer, selection, camera);
                ImGui::Separator();
                if (draw) {
                    if (ImGui::Selectable("Properties", !map_materials, 0, {95, 0}))
                        map_materials = false;
                    ImGui::SameLine();
                    if (ImGui::Selectable("Materials", map_materials, 0, {95, 0}))
                        map_materials = true;
                    ImGui::Separator();
                    if (map_materials)
                        studio::material_inspector(scene.get(), renderer, selection,
                                                   "Map inspector", true);
                }
                if (region && region->overworld && region->overworld->destination_zone >= 0)
                    ImGui::Text("Destination: zone %d / entrance %u",
                                region->overworld->destination_zone,
                                region->overworld->destination_event);
            } else {
                ImGui::SeparatorText("Map settings");
                if (scene) {
                    const auto location = std::find_if(scene->locations.begin(),
                                                       scene->locations.end(), [&](const auto &m) {
                                                           return m.zone == loaded_zone;
                                                       });
                    if (location != scene->locations.end())
                        ImGui::TextWrapped("%s", location->name.c_str());
                    if (loaded_zone >= 0)
                        ImGui::TextDisabled("Zone %d", loaded_zone);
                } else
                    ImGui::TextWrapped("Choose a map to get started.");
                if (ImGui::Button(studio::localization::tr("Environment & music"), {-1, 0}))
                    map_page = studio::MapInspectorPage::Environment;
            }
            if (properties && region && region->overworld && region->overworld->editor_id) {
                ImGui::BeginDisabled(job.valid() || overworld.preview_busy());
                if (ImGui::Button(studio::localization::tr("Edit placement..."), {-1, 0}))
                    overworld.inspect(*region->overworld);
                ImGui::EndDisabled();
            }
            ImGui::End();
            ImGui::BeginDisabled((overworld.pending() && !editor.uses_working_scene()) ||
                                 encounters.pending() || overworld.preview_busy() ||
                                 overworld.draft_active());
            editor.draw(selection, map_cursor.position(),
                        (properties && draw && draw->placement >= 0) ||
                            map_page == studio::MapInspectorPage::Settings);
            ImGui::EndDisabled();
            ImGui::Begin("Map inspector");
            if (properties && draw && draw->source) {
                ImGui::SeparatorText("Model");
                ImGui::BeginDisabled(overworld.pending());
                if (studio::primary_button("Edit model in Studio")) {
                    try {
                        auto document = studio::isolate_map_model(*scene, selection.draw,
                                                                  loaded_dump, loaded_area);
                        studio_workspace = true;
                        asset_studio.request_leave([&, document = std::move(document)]() mutable {
                            asset_studio.open_document(std::move(document));
                            studio_notice.clear();
                        });
                    } catch (const std::exception &e) {
                        studio_notice = e.what();
                    }
                }
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip(
                        overworld.pending()
                            ? "Stage placement changes before editing their model resources."
                            : "Edit the shared model, materials and textures used by "
                              "matching placements.");
            }
            if (!studio_notice.empty())
                ImGui::TextWrapped("%s", studio_notice.c_str());
            if (map_actions) {
                ImGui::SeparatorText("Gameplay");
                if (ImGui::Button("Map cameras", {-1, 0}))
                    camera_workspace = true;
                if (ImGui::Button(studio::localization::tr("Collision"), {-1, 0})) {
                    collision_workspace = true;
                    camera.stop_look(window);
                }
            }
            ImGui::End();
            if (zone_editor.draw(scene.get(), loaded_zone, map_actions, job.valid()))
                map_page = studio::MapInspectorPage::Environment;
            const auto field_selection_before = renderer.spatial.selected;
            field_systems.draw(camera, map_actions || (properties && region && region->overworld));
            if (renderer.spatial.selected != field_selection_before) {
                selection = {};
                map_page = studio::MapInspectorPage::Selection;
            }
            region = scene && renderer.spatial.selected >= 0 &&
                             std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()
                         ? &scene->spatial.regions[renderer.spatial.selected]
                         : nullptr;
            const bool interaction_needs_stage =
                region && region->overworld &&
                !overworld.interaction_target_available(*region->overworld);
            ImGui::BeginDisabled(job.valid() || overworld.preview_busy() ||
                                 interaction_needs_stage);
            conversation_editor.controls(scene, loaded_area, renderer.spatial.selected,
                                         properties && region, preferences, renderer, map_cursor);
            interaction_inspector.controls(scene.get(), loaded_dump, loaded_area,
                                           renderer.spatial.selected, properties && region);
            ImGui::EndDisabled();
            if (properties && interaction_needs_stage) {
                ImGui::Begin("Map inspector");
                ImGui::TextWrapped(
                    "Stage and reload this placement's changes to access its interaction.");
                ImGui::End();
            }
            warps.draw(job.valid() || overworld.pending() || encounters.pending(), camera,
                       map_cursor.position(),
                       map_actions ||
                           (properties && region && region->kind == studio::SpatialKind::Entrance));
            region = scene && renderer.spatial.selected >= 0 &&
                             std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()
                         ? &scene->spatial.regions[renderer.spatial.selected]
                         : nullptr;
            pickups.draw(job.valid() || overworld.pending() || encounters.pending(),
                         properties && region && region->kind == studio::SpatialKind::Pickup);
            overworld.draw(
                camera, job.valid(),
                [&] {
                    projects.stage_and_reload();
                },
                map_cursor.position(), map_actions || (properties && region && region->overworld));
            encounters.draw(
                job.valid() || overworld.pending(), camera,
                [&] {
                    projects.stage_and_reload();
                },
                map_cursor.position(),
                map_actions ||
                    (properties && region && region->kind == studio::SpatialKind::Encounter));
            if (properties && region) {
                ImGui::Begin("Map inspector");
                map_region_details =
                    ImGui::CollapsingHeader("Region details", ImGuiTreeNodeFlags_None);
                ImGui::End();
                if (map_region_details)
                    layers_changed |= renderer.spatial.controls(camera, loaded_zone, true);
            }
            if (properties && scene && !job.valid() && !overworld.preview_busy()) {
                ImGui::Begin("Map inspector");
                related_data.draw(*scene, loaded_dump, unsigned(loaded_area), renderer);
                if (studio::related_data_inspector(*scene, selection, renderer))
                    browser.frame_selection(*scene, renderer, selection, camera);
                ImGui::End();
            }
            if (layers_changed) {
                for (unsigned i = 0; i < renderer.spatial.enabled.size(); ++i) {
                    preferences.set(overlay_keys[i], renderer.spatial.enabled[i] ? "1" : "0");
                    preferences.set(std::string(overlay_keys[i]) + "_locked",
                                    renderer.spatial.locked[i] ? "1" : "0");
                }
                preferences.set("overlay_xray", renderer.spatial.xray ? "1" : "0");
                preferences.set("overlay_filled", renderer.spatial.filled ? "1" : "0");
                preferences.set("overlay_picking", renderer.spatial.pick_overlays ? "1" : "0");
                preferences.save();
            }
            selection.focus = false;
            if (!map_layers)
                renderer.spatial.focus = false;
            if (map_focus) {
                auto *vp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(vp->WorkPos);
                ImGui::SetNextWindowSize(vp->WorkSize);
                ImGui::SetNextWindowDockID(0);
            } else if (map_view_dock)
                ImGui::SetNextWindowDockID(map_view_dock, ImGuiCond_Always);
            ImGui::Begin("Map viewport", nullptr,
                         map_focus ? ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                   : 0);
            if (!map_focus)
                map_view_dock = ImGui::GetWindowDockID();
            using studio::MapToolIcon;
            if (studio::map_tool_button("Edit", MapToolIcon::Select, !map_preview)) {
                map_preview = false;
                renderer.player.active = false;
                map_page = studio::MapInspectorPage::Selection;
            }
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            if (studio::map_tool_button("Preview", MapToolIcon::Play, map_preview)) {
                map_preview = true;
                map_page = studio::MapInspectorPage::Selection;
            }
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            ImGui::BeginDisabled(!scene || renderer.player.active);
            if (studio::map_tool_button("Frame", MapToolIcon::Frame))
                browser.frame_selection(*scene, renderer, selection, camera);
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            if (studio::map_tool_button("Map start", MapToolIcon::Start))
                focus_spawn();
            ImGui::EndDisabled();
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            char hour_label[32];
            std::snprintf(hour_label, sizeof(hour_label), "%02d:%02d",
                          int(renderer.lighting.hour) % 24, int(renderer.lighting.hour * 60) % 60);
            if (studio::map_tool_button(hour_label, MapToolIcon::Sun,
                                        map_page == studio::MapInspectorPage::Environment))
                map_page = map_page == studio::MapInspectorPage::Environment
                               ? studio::MapInspectorPage::Selection
                               : studio::MapInspectorPage::Environment;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Environment, weather and map music");
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            if (studio::map_tool_button("Display", MapToolIcon::Display,
                                        map_page == studio::MapInspectorPage::Display))
                map_page = map_page == studio::MapInspectorPage::Display
                               ? studio::MapInspectorPage::Selection
                               : studio::MapInspectorPage::Display;
            if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x -
                    ImGui::GetItemRectMax().x >
                115.f)
                ImGui::SameLine();
            if (studio::map_tool_button(map_focus ? "Restore" : "Focus", MapToolIcon::Focus))
                map_focus = !map_focus;
            ImGui::Separator();
            auto available = ImGui::GetContentRegionAvail();
            available.y = std::max(1.f, available.y -
                                            ImGui::GetFrameHeightWithSpacing() *
                                                (available.x < 650.f ? 2.f : 1.f) -
                                            style.ItemSpacing.y);
            auto resolution = studio::ImGuiRenderer::viewport_resolution(available.x, available.y);
            available = {resolution.display_width, resolution.display_height};
            auto vw = resolution.width, vh = resolution.height;
            float view[16], projection[16];
            if (renderer.player.active && scene) {
                bool input = player_hovered && !(io.KeyCtrl && io.KeyShift) && !io.WantTextInput &&
                             (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
                if (input && binding_pressed(5))
                    renderer.player.active = false;
                if (input && binding_pressed(4))
                    start_player();
                if (input && io.KeyCtrl && io.MouseWheel != 0)
                    renderer.player.adjust_running_speed(io.MouseWheel);
                float side = input ? float(binding_down(3)) -
                                         float(binding_down(2))
                                   : 0,
                      ahead = input ? float(binding_down(0)) -
                                          float(binding_down(1))
                                    : 0;
                renderer.player.step(scene->spatial, side, ahead, io.KeyCtrl, io.KeyShift,
                                     io.DeltaTime);
                auto &pc = renderer.player.camera;
                camera.target = pc.target;
                auto delta = studio::SpatialPoint{
                    pc.eye[0] - pc.target[0], pc.eye[1] - pc.target[1], pc.eye[2] - pc.target[2]};
                camera.distance =
                    std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
                camera.yaw = std::atan2(delta[0], delta[2]);
                camera.pitch =
                    std::asin(std::clamp(delta[1] / std::max(camera.distance, .001f), -1.f, 1.f));
                bx::mtxLookAt(view, {pc.eye[0], pc.eye[1], pc.eye[2]},
                              {pc.target[0], pc.target[1], pc.target[2]},
                              {pc.up[0], pc.up[1], pc.up[2]}, bx::Handedness::Right);
                bx::mtxProj(projection, pc.fov, float(vw) / float(vh), 1.f, camera.far_clip(),
                            bgfx::getCaps()->homogeneousDepth, bx::Handedness::Right);
                for (unsigned i = 0; i < scene->lighting_contexts.size(); ++i)
                    if (int(scene->lighting_contexts[i].zone) == renderer.player.zone)
                        renderer.lighting.context = i;
            } else
                camera.matrices(view, projection, float(vw) / float(vh));
            renderer.particle_origin = camera.target;
            renderer.select_draw(selection.draw);
            auto texture = renderer.render(vw, vh, view, projection, colors, cutaway, cut_height,
                                           raw_materials);
            bool flip = bgfx::getCaps()->originBottomLeft;
            ImGui::Image(ImTextureID(studio::ImGuiRenderer::image_id(
                             texture, false, studio::ImGuiRenderer::preview_3ds)),
                         ImVec2(std::max(available.x, 1.f), std::max(available.y, 1.f)),
                         ImVec2(0, flip ? 1.f : 0.f), ImVec2(1, flip ? 0.f : 1.f));
            bool hovered = ImGui::IsItemHovered();
            auto image_origin = ImGui::GetItemRectMin();
            bool editing_shape = overworld.shape_active() || overworld.patrol_active();
            bool gizmo_captured =
                overworld.gizmo(view, projection, image_origin, available, hovered);
            if (!editing_shape && !gizmo_captured)
                gizmo_captured =
                    scene && map_cursor.viewport(*scene, renderer, view, projection, image_origin,
                                                 available, hovered, cutaway);
            if (!editing_shape && !map_cursor.position() && !gizmo_captured)
                gizmo_captured = field_systems.pedestrians.viewport(view, projection, image_origin,
                                                                    available, hovered);
            if (!editing_shape && !map_cursor.position() && !gizmo_captured)
                gizmo_captured = warps.gizmo(view, projection, image_origin, available, hovered);
            if (!editing_shape && !map_cursor.position() && !gizmo_captured)
                gizmo_captured =
                    encounters.gizmo(view, projection, image_origin, available, hovered);
            if (!editing_shape && !map_cursor.position() && !gizmo_captured &&
                !encounters.active() && (!overworld.pending() || editor.uses_working_scene()) &&
                !overworld.preview_busy() && !overworld.draft_active() && !encounters.pending())
                gizmo_captured = editor.gizmo(view, projection, image_origin, available, hovered);
            bool click = !editing_shape && !renderer.player.active && !gizmo_captured && hovered &&
                         !io.KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                         io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <
                             io.MouseDragThreshold * io.MouseDragThreshold;
            unsigned px = resolution.pixel_x(io.MousePos.x - image_origin.x),
                     py = resolution.pixel_y(io.MousePos.y - image_origin.y);
            bool requested_pick = false;
            if (observer && renderer.ready())
                if (auto pixel = observer->pick_pixel(vw, vh)) {
                    px = (*pixel)[0];
                    py = (*pixel)[1];
                    requested_pick = true;
                }
            if (click || requested_pick) {
                const bool submitted = renderer.request_pick(
                    px, py, vw, vh, view, projection, colors, cutaway, cut_height, raw_materials);
                if (requested_pick && submitted)
                    observer->pick_submitted();
            }
            player_hovered = hovered;
            if (!renderer.player.active)
                camera.controls(window, hovered && !gizmo_captured && !(io.KeyCtrl && io.KeyShift));

            if (hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Tab))
                map_focus = !map_focus;
            if (hovered && !renderer.player.active && !io.WantTextInput &&
                ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                selection = {};
                renderer.spatial.selected = -1;
            }
            if (hovered && region && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F))
                browser.frame_selection(*scene, renderer, selection, camera);
            studio::TutorialWidgets::Checkbox("main", studio::localization::tr("Animate"), &renderer.playback.enabled);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable scene animation; daily tracks follow the time of day");
            ImGui::SameLine();
            ImGui::BeginDisabled(!renderer.playback.enabled);
            if (studio::TutorialWidgets::Button("main", animation_playing ? "Pause" : "Play"))
                animation_playing = !animation_playing;
            ImGui::SameLine();
            if (studio::TutorialWidgets::Button("main", studio::localization::tr("Restart")))
                renderer.playback.seconds = 0;
            if (ImGui::GetWindowWidth() >= 666.f)
                ImGui::SameLine();
            float duration = 1;
            if (scene) {
                for (auto &a : scene->material_animations)
                    if (!a.daily && !a.bindings.empty())
                        duration = std::max(duration, a.motion.frames / 30);
                for (auto &rig : scene->skeletons)
                    if (!rig.daily)
                        duration = std::max(duration, rig.motion.frames / 30);
                for (auto &a : scene->visibility_animations)
                    if (!a.daily)
                        duration = std::max(duration, a.motion.clock.frames / 30);
                if (renderer.weather_effect)
                    duration = std::max(duration, 10.f);
            }
            float time = float(std::fmod(renderer.playback.seconds, double(duration)));
            ImGui::SetNextItemWidth(
                std::clamp(ImGui::GetContentRegionAvail().x - 170.f, 80.f, 260.f));
            if (ImGui::SliderFloat("Time", &time, 0, duration, "%.2f s")) {
                renderer.playback.seconds = time;
                animation_playing = false;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(75);
            ImGui::DragFloat("Speed", &animation_speed, .05f, .25f, 4.f, "%.2fx",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::EndDisabled();
            ImGui::End();
        }
        projects.update();
        projects.draw();
        tutorials.draw();
        if (projects.restarting())
            running = false;
        studio::UndoShortcuts::end_frame();
        ImGui::Render();
        ui.render(ImGui::GetDrawData());
        cpu_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin)
                     .count();
        graphics_frame = bgfx::frame();
        ++frame;
        if (requested_studio && models.ready()) {
            if (requested_battle)
                asset_studio.battle_preview();
            if (!pokemon_settings.empty())
                asset_studio.open_settings(std::filesystem::u8path(pokemon_settings));
            requested_studio = false;
            models.send_to_studio();
            studio::activity_log::add("Studio", "Sent current model from Models to Studio");
            if (observer)
                observer->scene_changed();
        }
        if (auto donor = models.take_shader_donor()) {
            asset_studio.editor().set_shader_donor(std::move(*donor));
            studio_workspace = true;
        }
        if (auto resource = authoring.take_studio_request()) {
            asset_studio.request_leave([&, resource = *resource] {
                try {
                    auto model = authoring.open_asset_studio(resource);
                    asset_studio.open_document(std::move(model));
                    authoring.attach_asset_studio(asset_studio.editor().shared_document(), [&] {
                        return asset_studio.editor().editing_available();
                    });
                    authoring_workspace = false;
                    studio_workspace = true;
                    studio_notice.clear();
                } catch (const std::exception &e) {
                    studio_notice = e.what();
                }
            });
        }
        if (auto request = models.take_studio_request()) {
            studio_workspace = true;
            studio::activity_log::add("Studio", "Opening model from Models in Studio");
            asset_studio.request_leave([&, document = std::move(*request)]() mutable {
                asset_studio.open_document(std::move(document));
                studio_notice.clear();
            });
        }
        if (asset_studio.editor().take_apply()) {
            auto &document = *asset_studio.editor().document();
            if (document.model.project_asset) {
                try {
                    authoring.save_asset_studio();
                    studio_workspace = false;
                    authoring_workspace = true;
                    studio_notice.clear();
                } catch (const std::exception &e) {
                    studio_notice = e.what();
                }
            } else if (document.model.originating_map >= 0 ||
                       (document.model.area >= 0 && studio::project_store())) {
                try {
                    projects.stage_and_reload();
                    studio_workspace = false;
                    studio_notice.clear();
                } catch (const std::exception &e) {
                    studio_notice = e.what();
                }
            } else if (document.model.area < 0) {
                studio_notice =
                    models.apply(document)
                        ? "Applied to Models. Use Write game files to update an archive."
                        : "Open the originating model in Models before applying.";
            } else if (scene && loaded_area == document.model.area &&
                       loaded_dump == document.model.dump &&
                       scene->archive_sources.field == document.model.archive_sources.field &&
                       scene->archive_sources.terrain == document.model.archive_sources.terrain) {
                document.apply_to(*scene);
                renderer.refresh_materials();
                renderer.refresh_textures();
                studio_notice = "Applied to the loaded map's shared resource. Use Write game files "
                                "to update an archive.";
            } else
                studio_notice = "Load the originating map before applying.";
        }
        if (observer) {
            studio::ApplicationFrame state;
            state.has_source = !dump.empty();
            state.ready = data_workspace ? true
                          : warp_ride_workspace ? warp_ride.ready()
                          : (authoring_workspace || (collision_workspace && authored_collision))
                              ? authoring.ready()
                          : studio_workspace ? asset_studio.ready()
                          : model_workspace  ? models.ready()
                                             : renderer.ready() && !job.valid();
            state.map = !data_workspace && !warp_ride_workspace && !collision_workspace && !authoring_workspace &&
                        !studio_workspace && !model_workspace;
            state.error = warp_ride_workspace   ? warp_ride.error()
                          : authoring_workspace ? authoring.error()
                          : model_workspace     ? models.error()
                                                : failure;
            state.renderer = &renderer;
            state.selection = &selection;
            state.width = width;
            state.height = height;
            state.frame_limit =
                pacing.limit((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0);
            state.fps = io.Framerate;
            state.cpu_ms = cpu_ms;
            state.describe = [&]() {
                if (collision_workspace && authored_collision)
                    return authoring.report();
                if (collision_workspace && scene)
                    return collision.report() + scene->report();
                if (authoring_workspace)
                    return authoring.report();
                if (studio_workspace)
                    return asset_studio.report();
                if (model_workspace)
                    return models.report();
                return scene ? scene->report() : std::string{};
            };
            if (!observer->frame(state))
                running = false;
        }
        while (running) {
            bool focused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
            auto remaining = pacing.remaining(begin, std::chrono::steady_clock::now(), focused);
            if (remaining.count() <= 0)
                break;
            if (remaining < std::chrono::milliseconds(2)) {
                SDL_DelayPrecise(static_cast<Uint64>(remaining.count()));
                break;
            }
            auto timeout = int(std::min<std::int64_t>(
                50, std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count() - 1));
            if (SDL_WaitEventTimeout(&event, timeout))
                handle_event(event);
        }
    }
    if (settings_dirty)
        remember();
    if (io.IniFilename)
        ImGui::SaveIniSettingsToDisk(io.IniFilename);
    if (projects.restarting() && scene && renderer.spatial.selected >= 0 &&
        std::size_t(renderer.spatial.selected) < scene->spatial.regions.size()) {
        const auto &region = scene->spatial.regions[renderer.spatial.selected];
        if (region.overworld)
            placement_reload =
                PlacementReloadContext{loaded_project, loaded_area, *region.overworld, camera};
    }
    return projects.restarting() ? 2 : 0;
}
}
int studio::run_application(int argc, char **argv, ApplicationObserver *observer) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout
            << "USUMStudio viewport\n  --project FILE  --dump FOLDER  --area INDEX\n  "
               "--field-archive FILE  --terrain-archive FILE  --pokemon-archive FILE\n  --font "
               "FILE  --shaders FOLDER  --vulkan\n  --pokemon SPECIES --form INDEX --female "
               "--shiny --studio\n  --battle-preview --pokemon-settings FILE\n  --authoring  "
               "--composition FILE\n  --collision  --collision-patch FILE\n  --cameras  "
               "--camera-patch FILE\n  --images  --audio\n  --patch FILE  --player 0|1\n  "
               "--overlays all|cameras|collision|interactions\n  --animation-time SECONDS  "
               "--light-hour HOUR  --weather PROFILE  --frame-material "
               "NAME\n  --camera \"X Y Z YAW PITCH DISTANCE\"\n  --cut-height HEIGHT\n  "
               "--warp-ride\n";
        return 0;
    }

#if !defined(_WIN32)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    auto *window = SDL_CreateWindow("USUMStudio", 1440, 900,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    int result = 1;
    try {
        studio::require(window != nullptr, SDL_GetError());
        do {
            result = run(window, argc, argv, observer);
            argc = 1;
        } while (result == 2);
    } catch (const std::exception &e) {
        result = 1;
        std::cerr << e.what() << '\n';
    }
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}

void studio::capture_window_image(const std::string &path) {
    bgfx::requestScreenShot(BGFX_INVALID_HANDLE, path.c_str());
}
