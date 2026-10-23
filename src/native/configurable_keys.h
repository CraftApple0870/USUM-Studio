#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>
namespace studio::configurable_keys {
inline std::unordered_map<std::string, ImGuiKey> keys;
inline void set(const char *id, ImGuiKey key) { keys[id] = key; }
inline bool pressed(const char *id, ImGuiKey fallback) {
    auto it = keys.find(id);
    return ImGui::IsKeyPressed(it == keys.end() ? fallback : it->second);
}
}
