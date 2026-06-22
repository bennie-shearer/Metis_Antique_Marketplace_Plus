#include "app_audit.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <sstream>
#include <algorithm>

namespace metis::antique {

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
static std::string je(const std::string& s) { return util::json_escape(s); }

static std::string logline_to_json(const LogLine& ll) {
    return R"({"timestamp":")" + je(ll.timestamp) + "\"" +
           R"(,"level":")" + je(ll.level) + "\"" +
           R"(,"message":")" + je(ll.message) + "\"}";
}

// -----------------------------------------------------------------------
void register_audit_routes(Router& r) {

    // ------------------------------------------------------------------
    // GET /api/audit/log  (admin only)
    // Returns last N lines from antique_marketplace_plus.log, optionally filtered
    // Query params: limit (default 200), level (INFO|WARN|ERROR|system)
    // ------------------------------------------------------------------
    r.add("GET", "/api/audit/log", [](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};

        std::string limit_s = util::query_param(req.query, "limit");
        std::string level   = util::query_param(req.query, "level");
        int limit = limit_s.empty() ? 200 : std::min(2000, std::max(1, std::stoi(limit_s)));

        auto lines = Logger::instance().tail(limit, level);
        std::string out = R"({"count":)" + std::to_string(lines.size()) +
                          R"(,"log_path":")" + je(Logger::instance().log_path()) + "\"" +
                          R"(,"lines":[)";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i) out += ',';
            out += logline_to_json(lines[i]);
        }
        return HttpResponse{200, out + "]}"};
    });

    // ------------------------------------------------------------------
    // GET /api/audit/trail  (admin only)
    // Unified chronological audit trail from multiple tables
    // Covers: logins, failed logins, item changes, sales, listings,
    //         user management, inquiries, photo uploads, comments
    // Query params: limit (default 100), type (login|item|sale|listing|user|inquiry)
    // ------------------------------------------------------------------
    r.add("GET", "/api/audit/trail", [](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};

        std::string limit_s = util::query_param(req.query, "limit");
        std::string type_f  = util::query_param(req.query, "type");
        int limit = limit_s.empty() ? 100 : std::min(500, std::max(1, std::stoi(limit_s)));

        // Each source produces rows: (timestamp, type, actor, description, detail)
        // We UNION them all and take the latest N

        std::string sql = R"(
SELECT * FROM (

-- Item field changes (from item_history)
SELECT ih.changed_at AS ts,
       'item_change'  AS type,
       COALESCE(u.username, ih.changed_by, 'unknown') AS actor,
       'Changed ' || ih.field || ' on "' || i.title || '"' AS description,
       'From: ' || COALESCE(ih.old_value,'(empty)') ||
       ' → To: ' || COALESCE(ih.new_value,'(empty)') AS detail
FROM item_history ih
JOIN items i ON i.id = ih.item_id
LEFT JOIN users u ON u.id = CAST(ih.changed_by AS INTEGER)

UNION ALL

-- Sales recorded
SELECT s.sale_date AS ts,
       'sale'       AS type,
       COALESCE(u.username,'dealer') AS actor,
       'Sold "' || i.title || '" for $' || CAST(ROUND(s.sale_price,2) AS TEXT) AS description,
       'Net proceeds: $' || CAST(ROUND(COALESCE(s.net_proceeds,s.sale_price),2) AS TEXT) ||
       CASE WHEN s.buyer_name != '' THEN ' to ' || s.buyer_name ELSE '' END AS detail
FROM sales s
JOIN items i ON i.id = s.item_id
LEFT JOIN users u ON u.username = 'admin'

UNION ALL

-- Listings created
SELECT l.created_at AS ts,
       'listing'    AS type,
       'dealer'     AS actor,
       'Listed "' || i.title || '" on ' || l.channel AS description,
       l.list_type || ' at $' || CAST(ROUND(l.list_price,2) AS TEXT) AS detail
FROM listings l
JOIN items i ON i.id = l.item_id

UNION ALL

-- Items created
SELECT i.created_at AS ts,
       'item_create' AS type,
       'dealer'      AS actor,
       'Added "' || i.title || '" to inventory' AS description,
       i.category || CASE WHEN i.era != '' THEN ', ' || i.era ELSE '' END AS detail
FROM items i

UNION ALL

-- Photos uploaded (via item_photos)
SELECT ip.created_at AS ts,
       'photo'       AS type,
       COALESCE(ip.uploaded_by,'dealer') AS actor,
       'Photo uploaded for "' || i.title || '"' AS description,
       ip.filename AS detail
FROM item_photos ip
JOIN items i ON i.id = ip.item_id

UNION ALL

-- Internal comments posted
SELECT ic.created_at AS ts,
       'comment'     AS type,
       ic.author     AS actor,
       'Note on "' || i.title || '"' AS description,
       SUBSTR(ic.body, 1, 80) || CASE WHEN LENGTH(ic.body) > 80 THEN '...' ELSE '' END AS detail
FROM item_comments ic
JOIN items i ON i.id = ic.item_id

UNION ALL

-- Buyer inquiries received
SELECT bi.created_at AS ts,
       'inquiry'     AS type,
       bi.buyer_name AS actor,
       'Inquiry on "' || i.title || '"' AS description,
       COALESCE(NULLIF(bi.subject,''), SUBSTR(bi.body,1,60)) AS detail
FROM buyer_inquiries bi
JOIN items i ON i.id = bi.item_id

UNION ALL

-- User management actions (sessions table = proxy for logins)
SELECT s.created_at AS ts,
       'login'       AS type,
       u.username    AS actor,
       'User logged in' AS description,
       u.role AS detail
FROM sessions s
JOIN users u ON u.id = s.user_id

UNION ALL

-- Failed login attempts
SELECT attempted_at AS ts,
       'failed_login' AS type,
       ip_addr        AS actor,
       'Failed login attempt' AS description,
       '' AS detail
FROM login_attempts

) ORDER BY ts DESC LIMIT )";
        sql += std::to_string(limit);

        Rows rows = Db::instance().query(sql);
        std::string out = R"({"count":)" + std::to_string(rows.size()) +
                          R"(,"trail":[)";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            // Filter by type if requested
            if (!type_f.empty() && r[1] != type_f) {
                out.pop_back(); // remove trailing comma
                if (i) {} // leave as is -- will add comma on next valid row
                continue;
            }
            out += R"({"ts":")" + je(r[0]) + "\"" +
                   R"(,"type":")" + je(r[1]) + "\"" +
                   R"(,"actor":")" + je(r[2]) + "\"" +
                   R"(,"description":")" + je(r[3]) + "\"" +
                   R"(,"detail":")" + je(r[4]) + "\"}";
        }
        return HttpResponse{200, out + "]}"};
    });

    // ------------------------------------------------------------------
    // GET /api/audit/sessions  (admin only)
    // All currently active sessions
    // ------------------------------------------------------------------
    r.add("GET", "/api/audit/sessions", [](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        Rows rows = Db::instance().query(R"(
SELECT s.token, u.username, u.role, s.created_at, s.expires_at,
       CASE WHEN s.expires_at > datetime('now') THEN 1 ELSE 0 END AS active
FROM sessions s JOIN users u ON u.id = s.user_id
ORDER BY s.created_at DESC LIMIT 50)");
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            // Redact token -- show only first 8 chars
            std::string tok = r[0].size() > 8 ? r[0].substr(0,8) + "..." : r[0];
            out += R"({"token":")" + je(tok) + "\"" +
                   R"(,"username":")" + je(r[1]) + "\"" +
                   R"(,"role":")" + je(r[2]) + "\"" +
                   R"(,"created_at":")" + je(r[3]) + "\"" +
                   R"(,"expires_at":")" + je(r[4]) + "\"" +
                   R"(,"active":)" + r[5] + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    // ------------------------------------------------------------------
    // DELETE /api/audit/sessions/:token  (admin only)
    // Force-expire a specific session (kick a user)
    // ------------------------------------------------------------------
    r.add("DELETE", "/api/audit/sessions/:token", [](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        auto it = req.params.find("token");
        if (it == req.params.end())
            return HttpResponse{400, R"({"error":"missing token"})"};
        Db::instance().exec("DELETE FROM sessions WHERE token=?", {it->second});
        LOG_SYSTEM("Session force-expired by admin: " + req.user_id);
        return HttpResponse{200, R"({"ok":true})"};
    });

    // ------------------------------------------------------------------
    // GET /api/audit/stats  (admin only)
    // Summary counts for the audit dashboard header
    // ------------------------------------------------------------------
    r.add("GET", "/api/audit/stats", [](const HttpRequest& req) {
        if (req.role != "admin")
            return HttpResponse{403, R"({"error":"admin only"})"};
        Rows logins  = Db::instance().query("SELECT COUNT(*) FROM sessions");
        Rows active  = Db::instance().query(
            "SELECT COUNT(*) FROM sessions WHERE expires_at > datetime('now')");
        Rows changes = Db::instance().query("SELECT COUNT(*) FROM item_history");
        Rows fails   = Db::instance().query("SELECT COUNT(*) FROM login_attempts");
        Rows sales   = Db::instance().query("SELECT COUNT(*) FROM sales");
        Rows inqs    = Db::instance().query("SELECT COUNT(*) FROM buyer_inquiries");
        return HttpResponse{200,
            R"({"total_sessions":)" + (logins.empty()?"0":logins[0][0]) +
            R"(,"active_sessions":)" + (active.empty()?"0":active[0][0]) +
            R"(,"item_changes":)" + (changes.empty()?"0":changes[0][0]) +
            R"(,"failed_logins":)" + (fails.empty()?"0":fails[0][0]) +
            R"(,"total_sales":)" + (sales.empty()?"0":sales[0][0]) +
            R"(,"buyer_inquiries":)" + (inqs.empty()?"0":inqs[0][0]) + "}"};
    });
}

} // namespace metis::antique
