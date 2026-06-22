#pragma once
#include <string>
#include <map>

// nlohmann/json -- bundled in third_party/json/json.hpp
// Include via: #include "json.hpp"  (CMake adds third_party/json to include path)

namespace metis::antique::util {

std::string exe_dir();
std::string resolve_exe_relative(const std::string& rel);
std::string url_decode(const std::string& s);
std::string json_escape(const std::string& s);
std::string now_iso();
std::string today_iso();
long        file_size(const std::string& path);
std::string query_param(const std::string& query, const std::string& key);

} // namespace metis::antique::util
