#include "pson.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace metis::antique {

bool Pson::load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line, section;
    while (std::getline(f, line)) {
        // Strip comments: only treat # as comment when outside quoted strings
        {
            bool in_quote = false;
            std::string stripped;
            for (size_t i = 0; i < line.size(); ++i) {
                char c = line[i];
                if (c == '"') { in_quote = !in_quote; stripped += c; }
                else if (c == '#' && !in_quote) { break; }
                else { stripped += c; }
            }
            line = stripped;
        }
        // trim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        if (line.back() == '{') {
            section = line.substr(0, line.find_first_of(" \t{"));
        } else if (line == "}") {
            section.clear();
        } else {
            auto eq = line.find('=');
            if (eq == std::string::npos || section.empty()) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t\""));
            v.erase(v.find_last_not_of(" \t\"") + 1);
            data_[section][k] = v;
        }
    }
    return true;
}

std::string Pson::get(const std::string& sec, const std::string& key,
                      const std::string& def) const {
    auto si = data_.find(sec);
    if (si == data_.end()) return def;
    auto ki = si->second.find(key);
    return ki == si->second.end() ? def : ki->second;
}

int Pson::get_int(const std::string& sec, const std::string& key, int def) const {
    auto v = get(sec, key, "");
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

bool Pson::get_bool(const std::string& sec, const std::string& key, bool def) const {
    auto v = get(sec, key, "");
    if (v == "true" || v == "1" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "no") return false;
    return def;
}

double Pson::get_double(const std::string& sec, const std::string& key, double def) const {
    auto v = get(sec, key, "");
    if (v.empty()) return def;
    try { return std::stod(v); } catch (...) { return def; }
}

} // namespace metis::antique
