#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <vector>
#include <algorithm>

namespace studio {
// Virtual project-local folders. Only identifiers are stored; game archives are untouched.
class ModelFolders {
public:
    struct Folder { int id = 0, parent = 0; std::string name; };
private:
    std::filesystem::path root_, file_;
    std::string source_;
    std::vector<Folder> folders_;
    std::map<std::string, std::set<int>> members_;
    int next_id_ = 1;
    static std::string hex(std::string_view s) {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string r;
        for (unsigned char c : s) { r += digits[c >> 4]; r += digits[c & 15]; }
        return r;
    }
    static std::string unhex(std::string_view s) {
        if (s.size() % 2) return {};
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        std::string r;
        for (std::size_t i = 0; i < s.size(); i += 2) {
            int a = digit(s[i]), b = digit(s[i + 1]);
            if (a < 0 || b < 0) return {};
            r += char(a * 16 + b);
        }
        return r;
    }
    bool save() const {
        if (file_.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(file_.parent_path(), ec);
        if (ec) return false;
        auto temp = file_;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            for (const auto &f : folders_)
                out << "F\t" << f.id << '\t' << f.parent << '\t' << hex(f.name) << '\n';
            for (const auto &[key, ids] : members_)
                for (int id : ids) out << "M\t" << hex(key) << '\t' << id << '\n';
            out.close();
            if (!out) return false;
        }
        std::filesystem::rename(temp, file_, ec);
        if (ec) {
            // Match the existing alias/favorites save behavior on Windows.
            std::filesystem::remove(file_, ec);
            ec.clear();
            std::filesystem::rename(temp, file_, ec);
        }
        return !ec;
    }
    template <class Change> bool update(Change &&change) {
        auto old_folders = folders_;
        auto old_members = members_;
        int old_next = next_id_;
        change();
        if (save()) return true;
        folders_ = std::move(old_folders);
        members_ = std::move(old_members);
        next_id_ = old_next;
        return false;
    }
public:
    void configure(std::filesystem::path root) { root_ = std::move(root); }
    void bind(const std::filesystem::path &dump) {
        auto key = dump.lexically_normal().generic_string();
        if (key == source_) return;
        source_ = std::move(key);
        folders_.clear(); members_.clear(); next_id_ = 1; file_.clear();
        if (source_.empty() || root_.empty()) return;
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : source_) { hash ^= c; hash *= 1099511628211ull; }
        file_ = root_ / "model_aliases" / (std::to_string(hash) + ".folders.txt");
        std::ifstream input(file_, std::ios::binary);
        std::string line;
        while (std::getline(input, line)) {
            if (line.size() < 3 || line[1] != '\t') continue;
            auto a = line.find('\t', 2);
            if (a == std::string::npos) continue;
            try {
                if (line[0] == 'F') {
                    auto b = line.find('\t', a + 1);
                    if (b == std::string::npos) continue;
                    int id = std::stoi(line.substr(2, a - 2));
                    int parent = std::stoi(line.substr(a + 1, b - a - 1));
                    auto name = unhex(std::string_view(line).substr(b + 1));
                    if (id < 1 || id > 1000000 || parent < 0 || parent == id || name.empty()) continue;
                    folders_.push_back({id, parent, std::move(name)});
                    next_id_ = std::max(next_id_, id + 1);
                } else if (line[0] == 'M') {
                    auto key_text = unhex(std::string_view(line).substr(2, a - 2));
                    int id = std::stoi(line.substr(a + 1));
                    if (!key_text.empty() && id > 0) members_[std::move(key_text)].insert(id);
                }
            } catch (...) { /* Ignore malformed metadata records. */ }
        }
    }
    const std::vector<Folder> &folders() const { return folders_; }
    const Folder *find(int id) const {
        auto it = std::find_if(folders_.begin(), folders_.end(), [id](const auto &f) { return f.id == id; });
        return it == folders_.end() ? nullptr : &*it;
    }
    bool descendant(int id, int parent) const {
        for (std::size_t hops = 0; id > 0 && hops <= folders_.size(); ++hops) {
            if (id == parent) return true;
            auto *f = find(id);
            if (!f || f->parent == id) break;
            id = f->parent;
        }
        return false;
    }
    bool contains(const std::string &key, int id, bool include_children) const {
        auto it = members_.find(key);
        if (it == members_.end()) return false;
        for (int current : it->second)
            if (current == id || (include_children && descendant(current, id))) return true;
        return false;
    }
    bool direct_member(const std::string &key, int id) const { return contains(key, id, false); }
    bool set_member(const std::string &key, int id, bool enabled) {
        if (key.empty() || !find(id)) return false;
        return update([&] {
            if (enabled) members_[key].insert(id);
            else {
                auto it = members_.find(key);
                if (it != members_.end()) { it->second.erase(id); if (it->second.empty()) members_.erase(it); }
            }
        });
    }
    int create(int parent, const std::string &name) {
        if (name.empty() || name.size() > 255 || (parent && !find(parent))) return -1;
        int id = next_id_;
        return update([&] { folders_.push_back({next_id_++, parent, name}); }) ? id : -1;
    }
    bool rename(int id, const std::string &name) {
        if (!find(id) || name.empty() || name.size() > 255) return false;
        return update([&] { for (auto &f : folders_) if (f.id == id) f.name = name; });
    }
    bool remove(int id) {
        if (!find(id)) return false;
        return update([&] {
            std::set<int> removed;
            for (const auto &f : folders_) if (descendant(f.id, id)) removed.insert(f.id);
            folders_.erase(std::remove_if(folders_.begin(), folders_.end(), [&](const auto &f) {
                return removed.contains(f.id);
            }), folders_.end());
            for (auto it = members_.begin(); it != members_.end();) {
                for (int target : removed) it->second.erase(target);
                if (it->second.empty()) it = members_.erase(it); else ++it;
            }
        });
    }
};
inline ModelFolders &model_folders() { static ModelFolders instance; return instance; }
}
