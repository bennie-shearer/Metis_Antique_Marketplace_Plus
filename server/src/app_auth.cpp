#include "app_auth.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <sstream>
#include <chrono>
#include <mutex>
#include <map>
#include <thread>

// bcrypt
extern "C" {
#include "bcrypt.h"
}

namespace metis::antique {

// -----------------------------------------------------------------------
// Rate limiting: max 5 attempts per IP per minute
// -----------------------------------------------------------------------
static std::mutex rate_mu_;
static std::map<std::string, std::vector<std::chrono::steady_clock::time_point>> rate_map_;

static int  s_rate_limit_max = 5;
static int  s_rate_limit_win = 60;
static int  s_bcrypt_cost    = 12;

static bool rate_limited(const std::string& ip) {
    std::lock_guard<std::mutex> lk(rate_mu_);
    auto now = std::chrono::steady_clock::now();
    auto& attempts = rate_map_[ip];
    // Prune older than window
    attempts.erase(std::remove_if(attempts.begin(), attempts.end(),
        [&](const auto& t){ return now - t > std::chrono::seconds(s_rate_limit_win); }),
        attempts.end());
    if (static_cast<int>(attempts.size()) >= s_rate_limit_max) return true;
    attempts.push_back(now);
    return false;
}

// -----------------------------------------------------------------------
// Password hashing via bcrypt (cost factor from config)
// -----------------------------------------------------------------------
static std::string hash_password(const std::string& plain) {
    char salt[BCRYPT_HASHSIZE];
    char hash[BCRYPT_HASHSIZE];
    bcrypt_gensalt(s_bcrypt_cost, salt);
    bcrypt_hashpw(plain.c_str(), salt, hash);
    return std::string(hash);
}

static bool verify_password(const std::string& plain, const std::string& stored) {
    int ret = bcrypt_checkpw(plain.c_str(), stored.c_str());
    return ret == 0;
}

// -----------------------------------------------------------------------
// Session token
// -----------------------------------------------------------------------
static std::string make_token() {
    // FNV-1a on timestamp -- token is a random-enough opaque string
    auto ns = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    size_t h = 14695981039346656037ULL ^ static_cast<size_t>(ns);
    h ^= std::hash<std::string>{}(util::now_iso());
    h *= 1099511628211ULL;
    std::ostringstream ss;
    ss << std::hex << h << std::hex << (ns & 0xFFFFFFFF);
    return ss.str();
}

// -----------------------------------------------------------------------
// Session validation
// -----------------------------------------------------------------------
bool session_valid(const std::string& token, std::string& user_id) {
    if (token.empty()) return false;
    Rows r = Db::instance().query(
        "SELECT user_id FROM sessions WHERE token=? AND expires_at > datetime('now')",
        {token});
    if (r.empty()) return false;
    user_id = r[0][0];
    // Sliding window: extend session by 24h on each use
    Db::instance().exec(
        "UPDATE sessions SET expires_at=datetime('now','+24 hours') WHERE token=?",
        {token});
    return true;
}

std::string user_role(const std::string& user_id) {
    if (user_id.empty()) return "";
    Rows r = Db::instance().query("SELECT role FROM users WHERE id=?", {user_id});
    return r.empty() ? "" : r[0][0];
}

// -----------------------------------------------------------------------
// Seed default users from config on first boot
// -----------------------------------------------------------------------
static void seed_users(const Pson& cfg) {
    struct { std::string u_key, p_key, role; } defs[] = {
        {"admin_user",  "admin_pass",  "admin"},
        {"dealer_user", "dealer_pass", "dealer"},
        {"viewer_user", "viewer_pass", "viewer"}
    };
    for (auto& d : defs) {
        std::string uname = cfg.get("auth", d.u_key, "");
        std::string pass  = cfg.get("auth", d.p_key, "");
        if (uname.empty() || pass.empty()) continue;
        Rows ex = Db::instance().query(
            "SELECT id,pass_hash FROM users WHERE username=?", {uname});
        if (ex.empty()) {
            Db::instance().exec(
                "INSERT INTO users(username,pass_hash,role,email,display_name) VALUES(?,?,?,?,?)",
                {uname, hash_password(pass), d.role, "", ""});
            LOG_SYSTEM("Seeded user: " + uname + " role=" + d.role);
        } else if (!verify_password(pass, ex[0][1])) {
            // Config password changed -- update stored hash to match
            Db::instance().exec(
                "UPDATE users SET pass_hash=? WHERE username=?",
                {hash_password(pass), uname});
            LOG_SYSTEM("Updated password for user: " + uname);
        }
    }
}

// -----------------------------------------------------------------------
// User JSON helper
// -----------------------------------------------------------------------
static std::string user_to_json(const std::vector<std::string>& r) {
    // cols: id, username, role, created_at, email, display_name
    return R"({"id":)" + r[0] +
           R"(,"username":")" + util::json_escape(r[1]) + "\"" +
           R"(,"role":")" + util::json_escape(r[2]) + "\"" +
           R"(,"created_at":")" + r[3] + "\"" +
           R"(,"email":")" + util::json_escape(r.size()>4 ? r[4] : "") + "\"" +
           R"(,"display_name":")" + util::json_escape(r.size()>5 ? r[5] : "") + "\"}";
}

// -----------------------------------------------------------------------
// Session cleanup jthread (expired sessions)
// -----------------------------------------------------------------------
static void start_session_cleanup() {
    static std::jthread cleaner([](std::stop_token st){
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::hours(1));
            if (st.stop_requested()) break;
            Db::instance().exec(
                "DELETE FROM sessions WHERE expires_at < datetime('now')", {});
            LOG_INFO("Expired sessions pruned");
        }
    });
}

// -----------------------------------------------------------------------
// Route registration
// -----------------------------------------------------------------------
void register_auth_routes(Router& rtr, const Pson& cfg) {
    int sess_hours     = cfg.get_int("auth", "session_hours", 8);
    s_rate_limit_max   = cfg.get_int("auth", "rate_limit_max_attempts", 5);
    s_rate_limit_win   = cfg.get_int("auth", "rate_limit_window_sec",   60);
    s_bcrypt_cost      = cfg.get_int("auth", "bcrypt_cost",             12);
    seed_users(cfg);
    start_session_cleanup();

    // POST /api/auth/login
    rtr.add("POST", "/api/auth/login", [sess_hours](const HttpRequest& req) {
        // Rate limit by remote IP (use X-Forwarded-For if behind proxy)
        std::string ip = req.remote_ip.empty() ? "unknown" : req.remote_ip;
        auto fwd = req.headers.find("x-forwarded-for");
        if (fwd != req.headers.end() && !fwd->second.empty()) ip = fwd->second;
        if (rate_limited(ip)) {
            LOG_WARN("Rate limit hit for IP: " + ip);
            return HttpResponse{429, R"({"error":"too many attempts, try again in a minute"})"};
        }

        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); }
        catch (...) { return HttpResponse{400, R"({"error":"invalid JSON"})"}; }

        std::string user = j.value("username", "");
        std::string pass = j.value("password", "");
        LOG_INFO("Login attempt: user='" + user + "' ip=" + ip);
        if (user.empty() || pass.empty())
            return HttpResponse{400, R"({"error":"username and password required"})"};

        Rows r = Db::instance().query(
            "SELECT id,username,role,pass_hash FROM users WHERE username=?", {user});
        if (r.empty() || !verify_password(pass, r[0][3])) {
            LOG_WARN("Failed login: " + user + " ip=" + ip);
            Db::instance().exec("INSERT INTO login_attempts(ip_addr) VALUES(?)", {ip});
            return HttpResponse{401, R"({"error":"invalid credentials"})"};
        }

        std::string uid   = r[0][0];
        std::string uname = r[0][1];
        std::string role  = r[0][2];
        std::string token = make_token();

        Db::instance().exec(
            "INSERT INTO sessions(token,user_id,expires_at) "
            "VALUES(?,?,datetime('now','+" + std::to_string(sess_hours) + " hours'))",
            {token, uid});

        LOG_INFO("Login: " + uname + " role=" + role);
        return HttpResponse{200,
            R"({"token":")" + token + "\"" +
            R"(,"user":")" + util::json_escape(uname) + "\"" +
            R"(,"role":")" + role + "\"}"};
    });

    // POST /api/auth/logout
    rtr.add("POST", "/api/auth/logout", [](const HttpRequest& req) {
        if (!req.session_token.empty())
            Db::instance().exec("DELETE FROM sessions WHERE token=?", {req.session_token});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // GET /api/auth/me
    rtr.add("GET", "/api/auth/me", [](const HttpRequest& req) {
        if (req.user_id.empty()) return HttpResponse{401, R"({"error":"unauthorized"})"};
        Rows r = Db::instance().query(
            "SELECT id,username,role,created_at,COALESCE(email,""),COALESCE(display_name,"") FROM users WHERE id=?", {req.user_id});
        if (r.empty()) return HttpResponse{401, R"({"error":"unauthorized"})"};
        return HttpResponse{200, user_to_json(r[0])};
    });

    // POST /api/auth/change-password
    rtr.add("POST", "/api/auth/change-password", [](const HttpRequest& req) {
        if (req.user_id.empty()) return HttpResponse{401, R"({"error":"unauthorized"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string old_p = j.value("old_password", "");
        std::string new_p = j.value("new_password", "");
        if (old_p.empty() || new_p.empty())
            return HttpResponse{400, R"({"error":"old_password and new_password required"})"};
        Rows r = Db::instance().query(
            "SELECT pass_hash FROM users WHERE id=?", {req.user_id});
        if (r.empty() || !verify_password(old_p, r[0][0]))
            return HttpResponse{401, R"({"error":"current password incorrect"})"};
        Db::instance().exec("UPDATE users SET pass_hash=? WHERE id=?",
                            {hash_password(new_p), req.user_id});
        LOG_INFO("Password changed for user_id=" + req.user_id);
        return HttpResponse{200, R"({"ok":true})"};
    });

    // GET /api/users  (admin)
    rtr.add("GET", "/api/users", [](const HttpRequest& req) {
        if (user_role(req.user_id) != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        Rows rows = Db::instance().query(
            "SELECT id,username,role,created_at,COALESCE(email,""),COALESCE(display_name,"") FROM users ORDER BY id");
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += user_to_json(rows[i]);
        }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/users  (admin)
    rtr.add("POST", "/api/users", [](const HttpRequest& req) {
        if (user_role(req.user_id) != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string uname = j.value("username", "");
        std::string pass  = j.value("password", "");
        std::string role  = j.value("role", "dealer");
        std::string email = j.value("email", "");
        std::string dname = j.value("display_name", "");
        if (uname.empty() || pass.empty())
            return HttpResponse{400, R"({"error":"username and password required"})"};
        if (role != "admin" && role != "dealer" && role != "viewer") role = "dealer";
        Rows ex = Db::instance().query("SELECT id FROM users WHERE username=?", {uname});
        if (!ex.empty()) return HttpResponse{409, R"({"error":"username already exists"})"};
        Db::instance().exec("INSERT INTO users(username,pass_hash,role,email,display_name) VALUES(?,?,?,?,?)",
                            {uname, hash_password(pass), role, email, dname});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("User created: " + uname + " role=" + role);
        return HttpResponse{201, R"({"id":)" + id + "}"};
    });

    // PUT /api/users/:id  (admin)
    rtr.add("PUT", "/api/users/:id", [](const HttpRequest& req) {
        if (user_role(req.user_id) != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string tid = it->second;
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string role = j.value("role", "");
        std::string pass = j.value("password", "");
        if (!role.empty()) {
            if (role != "admin" && role != "dealer" && role != "viewer")
                return HttpResponse{400, R"({"error":"invalid role"})"};
            if (role != "admin") {
                Rows admins = Db::instance().query(
                    "SELECT COUNT(*) FROM users WHERE role='admin'");
                Rows target = Db::instance().query(
                    "SELECT role FROM users WHERE id=?", {tid});
                if (!admins.empty() && std::stoi(admins[0][0]) <= 1
                    && !target.empty() && target[0][0] == "admin")
                    return HttpResponse{409, R"({"error":"cannot remove last admin"})"};
            }
            Db::instance().exec("UPDATE users SET role=? WHERE id=?", {role, tid});
        }
        if (!pass.empty())
            Db::instance().exec("UPDATE users SET pass_hash=? WHERE id=?",
                                {hash_password(pass), tid});
        if (j.contains("email"))
            Db::instance().exec("UPDATE users SET email=? WHERE id=?",
                                {j.value("email",""), tid});
        if (j.contains("display_name"))
            Db::instance().exec("UPDATE users SET display_name=? WHERE id=?",
                                {j.value("display_name",""), tid});
        LOG_INFO("User updated id=" + tid);
        return HttpResponse{200, R"({"ok":true})"};
    });

    // DELETE /api/users/:id  (admin)
    rtr.add("DELETE", "/api/users/:id", [](const HttpRequest& req) {
        if (user_role(req.user_id) != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string tid = it->second;
        if (tid == req.user_id)
            return HttpResponse{409, R"({"error":"cannot delete yourself"})"};
        Rows target = Db::instance().query("SELECT role FROM users WHERE id=?", {tid});
        if (!target.empty() && target[0][0] == "admin") {
            Rows admins = Db::instance().query(
                "SELECT COUNT(*) FROM users WHERE role='admin'");
            if (!admins.empty() && std::stoi(admins[0][0]) <= 1)
                return HttpResponse{409, R"({"error":"cannot delete last admin"})"};
        }
        Db::instance().exec("DELETE FROM sessions WHERE user_id=?", {tid});
        Db::instance().exec("DELETE FROM users WHERE id=?", {tid});
        LOG_INFO("User deleted id=" + tid);
        return HttpResponse{200, R"({"ok":true})"};
    });
}

} // namespace metis::antique
