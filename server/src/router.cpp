#include "router.hpp"
#include <sstream>

namespace metis::antique {

void Router::add(const std::string& method, const std::string& pattern, Handler h) {
    // Insert literal routes before parameterised routes so exact matches win
    bool has_param = pattern.find(':') != std::string::npos;
    if (!has_param) {
        // Find first parameterised route and insert before it
        for (auto it = routes_.begin(); it != routes_.end(); ++it) {
            if (it->pattern.find(':') != std::string::npos) {
                routes_.insert(it, {method, pattern, std::move(h)});
                return;
            }
        }
    }
    routes_.push_back({method, pattern, std::move(h)});
}

bool Router::match(const std::string& pattern, const std::string& path,
                   std::map<std::string, std::string>& params) const {
    std::vector<std::string> pp, rp;
    auto split = [](const std::string& s, char d) {
        std::vector<std::string> v;
        std::istringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, d)) if (!tok.empty()) v.push_back(tok);
        return v;
    };
    pp = split(pattern, '/');
    rp = split(path, '/');
    if (pp.size() != rp.size()) return false;
    for (size_t i = 0; i < pp.size(); ++i) {
        if (pp[i].starts_with(':')) params[pp[i].substr(1)] = rp[i];
        else if (pp[i] != rp[i]) return false;
    }
    return true;
}

HttpResponse Router::dispatch(HttpRequest& req) const {
    for (const auto& r : routes_) {
        if (r.method != req.method) continue;
        std::map<std::string, std::string> params;
        if (match(r.pattern, req.path, params)) {
            req.params = params;
            return r.handler(req);
        }
    }
    return {404, R"({"error":"not found"})"};
}

} // namespace metis::antique
