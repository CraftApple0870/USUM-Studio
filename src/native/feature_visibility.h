#pragma once
// User interface visibility only: these switches NEVER delete aliases, favorites,
// folders or model data. The persisted data remains intact while hidden.
namespace studio::feature_visibility {
inline bool folders = true;
inline bool universal_search = true;
inline bool favorites = true;
inline bool renaming = true;
inline bool model_ids = false;
}
