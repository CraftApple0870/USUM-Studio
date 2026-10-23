#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <cstdint>
#include <utility>

namespace studio {
// Local display aliases. No GARC, Pokemon model, or game text is touched.
class ModelAliases {
    std::filesystem::path root_, file_;
    std::string source_;
    std::map<std::string, std::string> values_;
    std::map<std::string, std::string> favorites_;
    std::filesystem::path favorites_file_;
    static std::string hex(std::string_view s) {
        constexpr char digits[] = "0123456789ABCDEF";
        std::string out; out.reserve(s.size() * 2);
        for (unsigned char c : s) { out.push_back(digits[c >> 4]); out.push_back(digits[c & 15]); }
        return out;
    }
    static std::string unhex(std::string_view s) {
        if (s.size() % 2) return {};
        auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        std::string out;
        for (size_t i = 0; i < s.size(); i += 2) {
            int a = digit(s[i]), b = digit(s[i + 1]);
            if (a < 0 || b < 0) return {};
            out.push_back(char(a * 16 + b));
        }
        return out;
    }
public:
    void configure(const std::filesystem::path &root) { root_ = root; }
    void bind(const std::filesystem::path &dump) {
        auto key = dump.lexically_normal().generic_string();
        if (key == source_) return;
        source_ = std::move(key);
        values_.clear(); favorites_.clear(); file_.clear(); favorites_file_.clear();
        if (source_.empty() || root_.empty()) return;
        std::uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : source_) { hash ^= c; hash *= 1099511628211ull; }
        file_ = root_ / "model_aliases" / (std::to_string(hash) + ".txt");
        favorites_file_ = root_ / "model_aliases" / (std::to_string(hash) + ".favorites.txt");
        {
            std::ifstream favorites_input(favorites_file_, std::ios::binary);
            std::string favorite_line;
            while (std::getline(favorites_input, favorite_line)) {
                auto sep = favorite_line.find('\t');
                if (sep == std::string::npos) continue;
                auto key = unhex(std::string_view(favorite_line).substr(0, sep));
                auto title = unhex(std::string_view(favorite_line).substr(sep + 1));
                if (!key.empty()) favorites_[std::move(key)] = std::move(title);
            }
        }
        std::ifstream in(file_, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            auto sep = line.find('\t');
            if (sep == std::string::npos) continue;
            auto k = unhex(std::string_view(line).substr(0, sep));
            auto v = unhex(std::string_view(line).substr(sep + 1));
            if (!k.empty() && !v.empty()) values_[std::move(k)] = std::move(v);
        }
    }
    static std::string pokemon_key(unsigned data_index) {
        return "pokemon:" + std::to_string(data_index);
    }
    static std::string library_key(int category, std::size_t member) {
        return "library:" + std::to_string(category) + ":" + std::to_string(member);
    }
    std::string get(const std::string &key) const {
        auto it = values_.find(key);
        return it == values_.end() ? std::string{} : it->second;
    }
    bool is_favorite(const std::string &key) const { return favorites_.contains(key); }
    const std::map<std::string, std::string> &favorites() const { return favorites_; }
    bool set_favorite(const std::string &key, bool enabled, const std::string &original) {
        if (favorites_file_.empty() || key.empty()) return false;
        auto previous = favorites_;
        if (enabled) favorites_[key] = original;
        else favorites_.erase(key);
        std::error_code ec;
        std::filesystem::create_directories(favorites_file_.parent_path(), ec);
        if (ec) { favorites_ = std::move(previous); return false; }
        auto temp = favorites_file_; temp += ".tmp";
        { std::ofstream out(temp, std::ios::binary | std::ios::trunc);
          if (!out) { favorites_ = std::move(previous); return false; }
          for (const auto &[k, title] : favorites_)
              out << hex(k) << '\t' << hex(title) << '\n';
          out.close();
          if (!out) { favorites_ = std::move(previous); return false; }
        }
        std::filesystem::rename(temp, favorites_file_, ec);
        if (ec) {
            // Windows does not replace an existing destination with rename().
            std::filesystem::remove(favorites_file_, ec);
            ec.clear(); std::filesystem::rename(temp, favorites_file_, ec);
        }
        if (ec) favorites_ = std::move(previous);
        return !ec;
    }
    bool set(const std::string &key, std::string value) {
        if (file_.empty() || key.empty()) return false;
        if (value.empty()) values_.erase(key);
        else values_[key] = std::move(value);
        std::error_code ec;
        std::filesystem::create_directories(file_.parent_path(), ec);
        if (ec) return false;
        auto temp = file_; temp += ".tmp";
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const auto &[k, v] : values_) out << hex(k) << '\t' << hex(v) << '\n';
        out.close();
        if (!out) return false;
        std::filesystem::rename(temp, file_, ec);
        if (ec) {
            std::filesystem::remove(file_, ec);
            ec.clear(); std::filesystem::rename(temp, file_, ec);
        }
        return !ec;
    }
};
inline ModelAliases &model_aliases() { static ModelAliases aliases; return aliases; }
}
