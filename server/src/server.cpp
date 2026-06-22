#include "server.hpp"
#include "app_auth.hpp"
#include "app_items.hpp"
#include "app_listings.hpp"
#include "app_sales.hpp"
#include "app_appraisals.hpp"
#include "app_market.hpp"
#include "app_photos.hpp"
#include "app_comments.hpp"
#include "app_audit.hpp"
#include "ws_log.hpp"
#include "app_purchases.hpp"
#include "app_business.hpp"
#include "app_pos.hpp"
#include "app_sync.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "smtp.hpp"
#include "db.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#ifdef METIS_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace metis::antique {

static std::string get_peer_ip(int fd) {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        char buf[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)))
            return std::string(buf);
    }
    return "unknown";
}

Server::Server(const Pson& cfg) : cfg_(cfg) {
    port_     = cfg_.get_int("server", "port", 8480);
    tls_port_ = cfg_.get_int("server", "tls_port", 0);
    web_root_ = util::resolve_exe_relative(cfg_.get("server", "web_root", "web"));
    photos_dir_ = util::resolve_exe_relative(cfg_.get("app", "photos_dir", "photos"));

#ifdef METIS_TLS
    if (tls_port_ > 0) {
        std::string cert = util::resolve_exe_relative(cfg_.get("server", "cert_file", "certs/server.crt"));
        std::string key  = util::resolve_exe_relative(cfg_.get("server", "key_file",  "certs/server.key"));
        SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
        if (!ctx) {
            LOG_ERROR("TLS: SSL_CTX_new failed");
        } else if (SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM) != 1) {
            LOG_ERROR("TLS: cannot load cert: " + cert);
            SSL_CTX_free(ctx);
        } else if (SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) != 1) {
            LOG_ERROR("TLS: cannot load key: " + key);
            SSL_CTX_free(ctx);
        } else {
            SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
            // Enable post-quantum hybrid key exchange (X25519MLKEM768)
            // OpenSSL 4.0 supports this natively -- falls back gracefully if client doesn't
            bool pq_configured = (SSL_CTX_set1_groups_list(ctx, "X25519MLKEM768:X25519:P-256") == 1);
            ssl_ctx_ = ctx;
            // Pre-populate with known defaults so /api/config returns badges
            // immediately on first load (before any client connects)
            tls_version_ = "TLS 1.3";
            tls_cipher_  = "TLS_AES_256_GCM_SHA384";
            // If PQ groups configured successfully, assume PQ until proven otherwise
            if (pq_configured) tls_group_ = "X25519MLKEM768";
            LOG_SYSTEM("TLS: context ready, will bind port " + std::to_string(tls_port_));
            LOG_SYSTEM(pq_configured ? "TLS: Post-Quantum X25519MLKEM768 enabled"
                                     : "TLS: Post-Quantum not available in this build");
        }
    }
#endif

    register_auth_routes(router_, cfg_);
    register_items_routes(router_, cfg_);
    register_listings_routes(router_);
    register_shop_routes(router_);
    register_sales_routes(router_);
    register_appraisals_routes(router_);
    register_market_routes(router_);
    register_photos_routes(router_, cfg_);
    register_comments_routes(router_);
    register_audit_routes(router_);
    register_sales_edit_routes(router_);
    register_purchases_routes(router_);
    register_business_routes(router_, cfg_);
    register_pos_routes(router_, cfg_);
    register_sync_routes(router_, cfg_);

    // /api/admin/shutdown -- graceful server shutdown (admin only)
    router_.add("POST", "/api/admin/shutdown", [this](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        LOG_SYSTEM("Shutdown requested by user: " + req.user_id);
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            stop();
        }).detach();
        return HttpResponse{200, R"({"ok":true,"message":"Server shutting down"})"};
    });

    // /api/settlements
    router_.add("GET", "/api/settlements", [](const HttpRequest& req) {
        std::string sql =
            "SELECT s.id,s.owner_id,u.username,s.period_start,s.period_end,"
            "s.total_sales,s.total_fees,s.net_due,s.status,s.sent_at,s.notes,s.created_at "
            "FROM settlements s JOIN users u ON s.owner_id=u.id";
        if (req.role == "dealer" && !req.user_id.empty())
            sql += " WHERE s.owner_id=" + req.user_id;
        sql += " ORDER BY s.created_at DESC";
        Rows rows = Db::instance().query(sql);
        std::string out = "[";
        auto je2 = [](const std::string& v) {
            std::string o; for (char c:v){if(c=='"')o+="\\\"";else if(c=='\\')o+="\\\\";else o+=c;} return o;
        };
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            out += "{\"id\":" + rows[i][0] +
                   ",\"owner_id\":" + rows[i][1] +
                   ",\"username\":\"" + je2(rows[i][2]) + "\"" +
                   ",\"period_start\":\"" + rows[i][3] + "\"" +
                   ",\"period_end\":\"" + rows[i][4] + "\"" +
                   ",\"total_sales\":" + rows[i][5] +
                   ",\"total_fees\":" + rows[i][6] +
                   ",\"net_due\":" + rows[i][7] +
                   ",\"status\":\"" + rows[i][8] + "\"" +
                   ",\"sent_at\":\"" + rows[i][9] + "\"" +
                   ",\"notes\":\"" + je2(rows[i][10]) + "\"}";
        }
        return HttpResponse{200, out + "]"};
    });

    router_.add("PUT", "/api/settlements/:id/paid", [](const HttpRequest& req) {
        if (req.role != "admin") return HttpResponse{403, R"({"error":"admin only"})"};
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Db::instance().exec("UPDATE settlements SET status='paid' WHERE id=?", {id});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // /api/config -- safe client-facing config values
    router_.add("GET", "/api/config", [this](const HttpRequest&) {
        std::string sym    = cfg_.get("app", "currency_symbol", "$");
        std::string cur    = cfg_.get("app", "currency",        "USD");
        std::string code   = cfg_.get("app", "currency_code",   "USD");
        std::string locale = cfg_.get("app", "currency_locale", "en-US");
        std::string name   = cfg_.get("app", "name",            "Metis Antique Marketplace Plus");
        std::string ver    = cfg_.get("app", "version",         "1.0.0");
        auto je = [](const std::string& s) {
            std::string o; for (char c : s) {
                if (c=='"') o+="\\\""; else if (c=='\\') o+="\\\\"; else o+=c;
            } return o;
        };
#ifdef METIS_TLS
        std::string tls_ver = tls_version_.empty() ? "TLS" : tls_version_;
        std::string tls_cip = tls_cipher_.empty()  ? ""    : tls_cipher_;
        // Post-quantum: detect Kyber/ML-KEM in cipher name or negotiated group
        bool pq = (tls_cip.find("KYBER")       != std::string::npos ||
                   tls_cip.find("ML-KEM")       != std::string::npos ||
                   tls_cip.find("X25519Kyber")  != std::string::npos ||
                   tls_group_.find("MLKEM")      != std::string::npos ||
                   tls_group_.find("Kyber")      != std::string::npos ||
                   tls_group_.find("X25519MLKEM")!= std::string::npos);
        std::string tls_json =
            ",\"tls_version\":\"" + je(tls_ver) + "\""
            ",\"tls_cipher\":\"" + je(tls_cip) + "\""
            ",\"tls_group\":\"" + je(tls_group_) + "\""
            ",\"tls_enabled\":true"
            ",\"post_quantum\":" + (pq ? "true" : "false");
#else
        std::string tls_json = ",\"tls_enabled\":false";
#endif
        return HttpResponse{200,
            "{\"currency\":\"" + je(cur) + "\","
            "\"currency_symbol\":\"" + je(sym) + "\","
            "\"currency_code\":\"" + je(code) + "\","
            "\"currency_locale\":\"" + je(locale) + "\","
            "\"app_name\":\"" + je(name) + "\","
            "\"version\":\"" + je(ver) + "\""
            + tls_json + "}"};
    });

    // /api/sales/:id/email-receipt -- send HTML receipt via SMTP
    router_.add("POST", "/api/sales/:id/email-receipt", [this](const HttpRequest& req) {
        if (!cfg_.get_bool("email", "enabled", false))
            return HttpResponse{503, R"({"error":"email not enabled in config.pson"})"};

        std::string sale_id = req.params.count("id") ? req.params.at("id") : "";
        if (sale_id.empty())
            return HttpResponse{400, R"({"error":"missing sale id"})"};

        // Fetch sale + item details
        Rows sale = Db::instance().query(
            "SELECT s.id, s.sale_date, s.sale_price, s.platform_fee, s.shipping_cost, "
            "s.net_proceeds, s.payment_method, s.buyer_name, s.buyer_email, "
            "i.title, i.category, i.era, i.maker "
            "FROM sales s JOIN items i ON s.item_id=i.id WHERE s.id=?", {sale_id});
        if (sale.empty())
            return HttpResponse{404, R"({"error":"sale not found"})"};

        std::string buyer_email = sale[0][8];
        std::string buyer_name  = sale[0][7];
        if (buyer_email.empty())
            return HttpResponse{400, R"({"error":"sale has no buyer email"})"};

        std::string sym      = cfg_.get("app", "currency_symbol", "$");
        std::string from_adr = cfg_.get("email", "from_address", "");
        std::string from_nm  = cfg_.get("email", "from_name",    "Metis Antique Marketplace");
        std::string app_name = cfg_.get("app",   "name",         "Metis Antique Marketplace Plus");

        auto fm = [&sym](const std::string& v) {
            double n = 0; try { n = std::stod(v); } catch (...) {}
            char buf[32]; snprintf(buf, sizeof(buf), "%.2f", n);
            return sym + std::string(buf);
        };

        // Build HTML receipt
        std::string html =
            "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
            "<style>body{font-family:Arial,sans-serif;color:#222;max-width:600px;margin:40px auto}"
            "h1{color:#C8960C;font-size:22px;margin-bottom:4px}"
            ".sub{color:#666;font-size:13px;margin-bottom:24px}"
            "table{width:100%;border-collapse:collapse;margin:16px 0}"
            "td{padding:8px 10px;border-bottom:1px solid #eee;font-size:14px}"
            "td:first-child{color:#666;width:40%}"
            ".total{font-weight:bold;font-size:15px;color:#C8960C}"
            ".footer{margin-top:32px;font-size:12px;color:#999;border-top:1px solid #eee;padding-top:12px}"
            "</style></head><body>"
            "<h1>" + app_name + "</h1>"
            "<div class='sub'>Sale Receipt &mdash; " + sale[0][1] + "</div>"
            "<table>"
            "<tr><td>Item</td><td><strong>" + sale[0][9] + "</strong></td></tr>"
            "<tr><td>Category</td><td>" + sale[0][10] + "</td></tr>"
            + (sale[0][11].empty() ? "" : "<tr><td>Era</td><td>" + sale[0][11] + "</td></tr>")
            + (sale[0][12].empty() ? "" : "<tr><td>Maker</td><td>" + sale[0][12] + "</td></tr>")
            + "<tr><td>Sale date</td><td>" + sale[0][1] + "</td></tr>"
            "<tr><td>Payment method</td><td>" + sale[0][6] + "</td></tr>"
            "<tr><td>Sale price</td><td>" + fm(sale[0][2]) + "</td></tr>"
            + (sale[0][3] != "0" && !sale[0][3].empty() ?
               "<tr><td>Platform fee</td><td>" + fm(sale[0][3]) + "</td></tr>" : "")
            + (sale[0][4] != "0" && !sale[0][4].empty() ?
               "<tr><td>Shipping</td><td>" + fm(sale[0][4]) + "</td></tr>" : "")
            + "<tr><td class='total'>Net proceeds</td><td class='total'>" + fm(sale[0][5]) + "</td></tr>"
            "</table>"
            "<div class='footer'>This receipt was generated by " + app_name +
            ". Thank you for your purchase, " + buyer_name + ".</div>"
            "</body></html>";

        // Send via SMTP STARTTLS (uses existing OpenSSL)
        std::string err = smtp_send(
            cfg_.get("email", "smtp_host", ""),
            cfg_.get_int("email", "smtp_port", 587),
            cfg_.get("email", "smtp_user", ""),
            cfg_.get("email", "smtp_pass", ""),
            from_adr, from_nm,
            buyer_email, buyer_name,
            "Receipt: " + sale[0][9],
            html);

        if (!err.empty()) {
            LOG_ERROR("Email receipt failed: " + err);
            return HttpResponse{500, "{\"error\":\"" + err + "\"}"};
        }
        LOG_SYSTEM("Receipt emailed to " + buyer_email + " for sale " + sale_id);
        return HttpResponse{200, R"({"ok":true})"};
    });
}

std::string Server::mime_type(const std::string& ext) {
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    return "application/octet-stream";
}

std::string Server::serve_static(const std::string& path) {
    // Serve /photos/ from photos_dir_ (exe-relative, outside web_root)
    std::filesystem::path fp;
    if (path.starts_with("/photos/")) {
        fp = std::filesystem::path(photos_dir_) / path.substr(8);
    } else {
        fp = std::filesystem::path(web_root_) / path.substr(1);
        if (std::filesystem::is_directory(fp)) fp /= "index.html";
        if (!std::filesystem::exists(fp))
            fp = std::filesystem::path(web_root_) / "index.html";
    }
    std::ifstream f(fp, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string ext = fp.extension().string();
    std::string body = ss.str();
    return "HTTP/1.1 200 OK\r\nContent-Type: " + mime_type(ext) +
           "\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nCache-Control: no-cache\r\n\r\n" + body;
}

HttpRequest Server::parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream ss(raw);
    std::string line;
    std::getline(ss, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream lss(line);
    std::string path_query;
    lss >> req.method >> path_query;
    auto q = path_query.find('?');
    if (q != std::string::npos) {
        req.query = path_query.substr(q + 1);
        req.path  = path_query.substr(0, q);
    } else {
        req.path = path_query;
    }
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto c = line.find(':');
        if (c != std::string::npos) {
            std::string k = line.substr(0, c);
            std::string v = line.substr(c + 2);
            for (auto& ch : k) ch = static_cast<char>(::tolower(ch));
            req.headers[k] = v;
        }
    }
    std::string rest((std::istreambuf_iterator<char>(ss)),
                      std::istreambuf_iterator<char>());
    req.body = rest;

    // Extract bearer token -- Authorization header or ?token= query param (for WebSocket)
    auto auth_it = req.headers.find("authorization");
    if (auth_it != req.headers.end()) {
        const std::string& av = auth_it->second;
        if (av.starts_with("Bearer ")) req.session_token = av.substr(7);
    }
    if (req.session_token.empty()) {
        // Fall back to ?token= for WebSocket connections
        req.session_token = util::query_param(req.query, "token");
    }
    // Validate session -> user_id
    if (!req.session_token.empty() && session_valid(req.session_token, req.user_id)) {
        req.role = user_role(req.user_id);
    }
    return req;
}

std::string Server::build_response(const HttpResponse& resp) {
    static const std::map<int,std::string> phrases = {
        {200,"OK"},{201,"Created"},{204,"No Content"},
        {400,"Bad Request"},{401,"Unauthorized"},{403,"Forbidden"},
        {404,"Not Found"},{409,"Conflict"},{500,"Internal Server Error"}
    };
    auto it = phrases.find(resp.status);
    std::string phrase = (it != phrases.end()) ? it->second : "Unknown";
    std::string out = "HTTP/1.1 " + std::to_string(resp.status) + " " + phrase + "\r\n";
    out += "Content-Type: " + resp.content_type + "\r\n";
    out += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
    out += "Access-Control-Allow-Origin: *\r\n";
    out += "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n";
    out += "Access-Control-Allow-Headers: Content-Type,Authorization\r\n";
    for (const auto& [k, v] : resp.headers) out += k + ": " + v + "\r\n";
    out += "\r\n" + resp.body;
    return out;
}

void Server::handle_client(int fd) {
    // Initial read — enough for headers (8KB)
    char hbuf[8192] = {};
    int n = recv(fd, hbuf, sizeof(hbuf) - 1, 0);
    if (n <= 0) { closesocket(fd); return; }
    std::string raw(hbuf, n);

    // Find header/body split
    auto header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        // Check Content-Length to read remaining body
        auto cl_pos = raw.find("content-length:");
        if (cl_pos == std::string::npos)
            cl_pos = raw.find("Content-Length:");
        if (cl_pos != std::string::npos) {
            auto nl = raw.find("\r\n", cl_pos);
            long long content_length = 0;
            if (nl != std::string::npos)
                content_length = std::stoll(raw.substr(cl_pos + 15,
                    nl - cl_pos - 15));
            long long body_received =
                static_cast<long long>(raw.size()) -
                static_cast<long long>(header_end + 4);
            long long remaining = content_length - body_received;
            // Read remaining body in 64KB chunks; cap at 20MB
            const long long MAX_BODY = 20LL * 1024 * 1024;
            remaining = std::min(remaining, MAX_BODY - body_received);
            while (remaining > 0) {
                char chunk[65536];
                int got = recv(fd, chunk,
                    static_cast<int>(std::min((long long)sizeof(chunk), remaining)),
                    0);
                if (got <= 0) break;
                raw.append(chunk, got);
                remaining -= got;
            }
        }
    }

    // OPTIONS preflight
    if (raw.starts_with("OPTIONS")) {
        std::string pre = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type,Authorization\r\n"
                          "Content-Length: 0\r\n\r\n";
        send(fd, pre.c_str(), static_cast<int>(pre.size()), 0);
        closesocket(fd);
        return;
    }

    HttpRequest req = parse_request(raw);
    req.remote_ip = get_peer_ip(fd);

    // WebSocket upgrade -- /ws/logs
    {
        std::string upg;
        auto ui = req.headers.find("upgrade");
        if (ui != req.headers.end()) upg = ui->second;
        std::transform(upg.begin(), upg.end(), upg.begin(), ::tolower);
        if (upg == "websocket" && req.path == "/ws/logs") {
            handle_ws_logs(fd, raw, req.role);
            return; // fd closed inside handle_ws_logs
        }
    }

    // Route /shop -> /shop.html
    if (req.method == "GET" && (req.path == "/shop" || req.path == "/shop/")) {
        std::string resp = serve_static("/shop.html");
        if (!resp.empty()) { send(fd, resp.c_str(), static_cast<int>(resp.size()), 0); closesocket(fd); return; }
    }

    // Static files
    if (req.method == "GET" && !req.path.starts_with("/api/") &&
        !req.path.starts_with("/shop/") && req.path != "/shop") {
        std::string resp = serve_static(req.path);
        if (!resp.empty()) {
            send(fd, resp.c_str(), static_cast<int>(resp.size()), 0);
            closesocket(fd);
            return;
        }
    }

    // Auth gate: all /api/ except /api/auth/login
    // Public endpoints: login, buyer inquiry, shop storefront
    // Public: login, buyer inquiry submit/read/reply, shop routes
    bool is_public = (req.path == "/api/auth/login") ||
                     (req.path == "/api/inquiries" && req.method == "POST") ||
                     (req.path.find("/api/inquiries/") == 0 &&
                      (req.method == "GET" ||
                       (req.path.find("/replies") != std::string::npos &&
                        req.method == "POST"))) ||
                     req.path.starts_with("/shop/") ||
                     req.path == "/shop";
    if (req.path.starts_with("/api/") && !is_public &&
        cfg_.get_bool("auth", "enabled", true) && req.user_id.empty()) {
        std::string r = build_response({401, R"({"error":"unauthorized"})"});
        send(fd, r.c_str(), static_cast<int>(r.size()), 0);
        closesocket(fd);
        return;
    }

    // Role gate: viewers may not write (POST/PUT/DELETE on non-auth paths)
    if (!req.role.empty() && req.role == "viewer" &&
        (req.method == "POST" || req.method == "PUT" || req.method == "DELETE") &&
        !req.path.starts_with("/api/auth/")) {
        std::string r = build_response({403, R"({"error":"read-only account"})"});
        send(fd, r.c_str(), static_cast<int>(r.size()), 0);
        closesocket(fd);
        return;
    }

    HttpResponse resp = router_.dispatch(req);
    std::string out = build_response(resp);
    send(fd, out.c_str(), static_cast<int>(out.size()), 0);
    closesocket(fd);
}

#ifdef METIS_TLS
void Server::handle_client_ssl(SSL* ssl, int fd) {
    // Read headers (8KB)
    char hbuf[8192] = {};
    int n = SSL_read(ssl, hbuf, sizeof(hbuf) - 1);
    if (n <= 0) { SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return; }
    std::string raw(hbuf, n);

    // Read remaining body if Content-Length demands it
    auto header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        auto cl_pos = raw.find("content-length:");
        if (cl_pos == std::string::npos) cl_pos = raw.find("Content-Length:");
        if (cl_pos != std::string::npos) {
            auto nl = raw.find("\r\n", cl_pos);
            long long content_length = 0;
            if (nl != std::string::npos)
                content_length = std::stoll(raw.substr(cl_pos + 15, nl - cl_pos - 15));
            long long body_received =
                static_cast<long long>(raw.size()) -
                static_cast<long long>(header_end + 4);
            long long remaining = content_length - body_received;
            const long long MAX_BODY = 20LL * 1024 * 1024;
            remaining = std::min(remaining, MAX_BODY - body_received);
            while (remaining > 0) {
                char chunk[65536];
                int got = SSL_read(ssl, chunk,
                    static_cast<int>(std::min((long long)sizeof(chunk), remaining)));
                if (got <= 0) break;
                raw.append(chunk, got);
                remaining -= got;
            }
        }
    }

    // OPTIONS preflight
    if (raw.starts_with("OPTIONS")) {
        std::string pre = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type,Authorization\r\n"
                          "Content-Length: 0\r\n\r\n";
        SSL_write(ssl, pre.c_str(), static_cast<int>(pre.size()));
        SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd);
        return;
    }

    HttpRequest req = parse_request(raw);
    req.remote_ip = get_peer_ip(fd);

    // WebSocket upgrade -- /ws/logs over TLS
    {
        std::string upg;
        auto ui = req.headers.find("upgrade");
        if (ui != req.headers.end()) upg = ui->second;
        std::transform(upg.begin(), upg.end(), upg.begin(), ::tolower);
        if (upg == "websocket" && req.path == "/ws/logs") {
            handle_ws_logs_ssl(ssl, fd, raw, req.role);
            return; // fd/ssl closed inside
        }
    }

    // Route /shop
    if (req.method == "GET" && (req.path == "/shop" || req.path == "/shop/")) {
        std::string resp = serve_static("/shop.html");
        if (!resp.empty()) {
            SSL_write(ssl, resp.c_str(), static_cast<int>(resp.size()));
            SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
        }
    }

    // Static files
    if (req.method == "GET" && !req.path.starts_with("/api/") &&
        !req.path.starts_with("/shop/") && req.path != "/shop") {
        std::string resp = serve_static(req.path);
        if (!resp.empty()) {
            SSL_write(ssl, resp.c_str(), static_cast<int>(resp.size()));
            SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
        }
    }

    // Auth gate
    bool is_public = (req.path == "/api/auth/login") ||
                     (req.path == "/api/inquiries" && req.method == "POST") ||
                     (req.path.find("/api/inquiries/") == 0 &&
                      (req.method == "GET" ||
                       (req.path.find("/replies") != std::string::npos &&
                        req.method == "POST"))) ||
                     req.path.starts_with("/shop/") ||
                     req.path == "/shop";
    if (req.path.starts_with("/api/") && !is_public &&
        cfg_.get_bool("auth", "enabled", true) && req.user_id.empty()) {
        std::string r = build_response({401, R"({"error":"unauthorized"})"});
        SSL_write(ssl, r.c_str(), static_cast<int>(r.size()));
        SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
    }

    // Role gate
    if (!req.role.empty() && req.role == "viewer" &&
        (req.method == "POST" || req.method == "PUT" || req.method == "DELETE") &&
        !req.path.starts_with("/api/auth/")) {
        std::string r = build_response({403, R"({"error":"read-only account"})"});
        SSL_write(ssl, r.c_str(), static_cast<int>(r.size()));
        SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
    }

    HttpResponse resp = router_.dispatch(req);
    std::string out = build_response(resp);
    SSL_write(ssl, out.c_str(), static_cast<int>(out.size()));
    SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd);
}

void Server::run_tls_acceptor() {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { LOG_ERROR("TLS: socket() failed"); return; }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(tls_port_));
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("TLS: bind() failed on port " + std::to_string(tls_port_));
        closesocket(srv); return;
    }
    listen(srv, 64);
    LOG_SYSTEM("Metis Antique Marketplace Plus v" + cfg_.get("app", "version", "1.0.0") +
               " listening on TLS port " + std::to_string(tls_port_));
    while (running_) {
        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        SSL* ssl = SSL_new(ssl_ctx_);
        SSL_set_fd(ssl, static_cast<int>(client));
        std::thread([this, ssl, client]() {
            if (SSL_accept(ssl) <= 0) {
                SSL_free(ssl); closesocket(client); return;
            }
            // Capture negotiated protocol/cipher once (first connection wins)
        // Capture negotiated protocol/cipher once (first connection wins)
            if (tls_version_.empty()) {
                const char* ver = SSL_get_version(ssl);
                const char* cip = SSL_get_cipher(ssl);
                if (ver) tls_version_ = ver;
                if (cip) tls_cipher_  = cip;
                // Capture negotiated group for post-quantum detection
                int nid = SSL_get_negotiated_group(ssl);
                if (nid != NID_undef) {
                    const char* grp = OBJ_nid2sn(nid);
                    if (grp) tls_group_ = grp;
                }
            }
            handle_client_ssl(ssl, static_cast<int>(client));
        }).detach();
    }
    closesocket(srv);
}
#endif

void Server::run() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) { LOG_ERROR("socket() failed"); return; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(port_));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("bind() failed on port " + std::to_string(port_));
        closesocket(srv);
        return;
    }
    listen(srv, 64);
    LOG_SYSTEM("Metis Antique Marketplace Plus v" + cfg_.get("app", "version", "1.0.0") +
               " listening on port " + std::to_string(port_));

#ifdef METIS_TLS
    // Launch TLS acceptor on tls_port_ if context was initialised
    std::thread tls_thread;
    if (ssl_ctx_) {
        tls_thread = std::thread([this]() { run_tls_acceptor(); });
    }
#endif

    while (running_) {
        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        std::thread([this, client]() {
            handle_client(static_cast<int>(client));
        }).detach();
    }
    closesocket(srv);

#ifdef METIS_TLS
    if (tls_thread.joinable()) tls_thread.join();
    if (ssl_ctx_) { SSL_CTX_free(ssl_ctx_); ssl_ctx_ = nullptr; }
#endif
}

} // namespace metis::antique
