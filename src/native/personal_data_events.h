#pragma once
#include <cstdint>

namespace studio {
// UI-thread notification; fired only after the project Personal GARC is imported.
// No background threads or filesystem watcher required.
inline std::uint64_t personal_data_revision = 0;
inline void notify_personal_data_changed() noexcept { ++personal_data_revision; }
}
