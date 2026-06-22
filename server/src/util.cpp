#include "util.hpp"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace metis::antique::util {

std::string exe_dir() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path().string();
#else
    char buf[PATH_MAX] = {};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) return std::filesystem::path(buf).parent_path().string();
    return ".";
#endif
}

std::string resolve_exe_relative(const std::string& rel) {
    return (std::filesystem::path(exe_dir()) / rel).string();
}

std::string url_decode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') { out += ' '; }
        else if (s[i] == '%' && i + 2 < s.size()) {
            int val = 0;
            std::istringstream ss(s.substr(i + 1, 2));
            ss >> std::hex >> val;
            out += static_cast<char>(val);
            i += 2;
        } else { out += s[i]; }
    }
    return out;
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20)  out += "?";
        else                out += c;
    }
    return out;
}

std::string now_iso() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string today_iso() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

long file_size(const std::string& path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return ec ? -1 : static_cast<long>(sz);
}

std::string query_param(const std::string& query, const std::string& key) {
    // Parse ?foo=bar&baz=qux style query string
    std::string search = key + "=";
    size_t pos = query.find(search);
    if (pos == std::string::npos) return "";
    size_t start = pos + search.size();
    size_t end = query.find('&', start);
    std::string val = (end == std::string::npos)
        ? query.substr(start)
        : query.substr(start, end - start);
    return url_decode(val);
}

} // namespace metis::antique::util
