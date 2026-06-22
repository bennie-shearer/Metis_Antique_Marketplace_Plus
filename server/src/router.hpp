#pragma once
#include "types.hpp"
#include <vector>

namespace metis::antique {

class Router {
public:
    void add(const std::string& method, const std::string& pattern, Handler h);
    HttpResponse dispatch(HttpRequest& req) const;
private:
    bool match(const std::string& pattern, const std::string& path,
               std::map<std::string, std::string>& params) const;
    std::vector<Route> routes_;
};

} // namespace metis::antique
