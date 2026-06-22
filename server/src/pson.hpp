#pragma once
#include <string>
#include <map>

namespace metis::antique {

class Pson {
public:
    bool load(const std::string& path);
    std::string get(const std::string& section, const std::string& key,
                    const std::string& def = "") const;
    int  get_int   (const std::string& section, const std::string& key, int    def = 0)     const;
    bool get_bool  (const std::string& section, const std::string& key, bool   def = false) const;
    double get_double(const std::string& section, const std::string& key, double def = 0.0) const;

private:
    std::map<std::string, std::map<std::string, std::string>> data_;
};

} // namespace metis::antique
