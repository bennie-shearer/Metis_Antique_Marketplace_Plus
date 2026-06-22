#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace metis::antique {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;
    std::string session_token;
    std::string user_id;
    std::string role;          // "admin" | "dealer" | "viewer"
    std::string remote_ip;     // populated from getpeername() by the server
};

struct HttpResponse {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";
    std::map<std::string, std::string> headers;
};

using Handler = std::function<HttpResponse(const HttpRequest&)>;

struct Route {
    std::string method;
    std::string pattern;
    Handler     handler;
};

using Rows = std::vector<std::vector<std::string>>;

// Role helpers
inline bool is_admin(const HttpRequest& req)  { return req.role == "admin"; }
inline bool is_writer(const HttpRequest& req) { return req.role == "admin" || req.role == "dealer"; }
inline bool is_reader(const HttpRequest& req) { return !req.role.empty(); }

} // namespace metis::antique
