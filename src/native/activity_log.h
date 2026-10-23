#pragma once
// UI-only activity journal. Does not alter game files or project resources.
#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
namespace studio::activity_log {
enum class Level { Info, Warning, Error };
struct Entry { std::string time, category, message; Level level; };
inline std::mutex mutex;
inline std::deque<Entry> entries;
inline void add(std::string category, std::string message, Level level = Level::Info) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[16];
    std::strftime(stamp, sizeof(stamp), "%H:%M:%S", &local);
    const char *tag = level == Level::Error ? "ERROR" : level == Level::Warning ? "WARN" : "INFO";
    {
        std::lock_guard<std::mutex> lock(mutex);
        entries.push_back({stamp, std::move(category), std::move(message), level});
        if (entries.size() > 5000) entries.pop_front();
        const auto &last = entries.back();
        std::fprintf(stdout, "[%s] [%s] [%s] %s\n", stamp, tag, last.category.c_str(), last.message.c_str());
        std::fflush(stdout);
    }
}
inline std::vector<Entry> snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return {entries.begin(), entries.end()};
}
inline void clear() { std::lock_guard<std::mutex> lock(mutex); entries.clear(); }
}
