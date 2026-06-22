#include "app_comments.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"

namespace metis::antique {

// -----------------------------------------------------------------------
// Item comments (internal dealer notes -- all staff can see)
// -----------------------------------------------------------------------
static std::string comment_to_json(const std::vector<std::string>& r) {
    // cols: id, item_id, author, body, created_at
    return R"({"id":)" + r[0] +
           R"(,"item_id":)" + r[1] +
           R"(,"author":")" + util::json_escape(r[2]) + "\"" +
           R"(,"body":")" + util::json_escape(r[3]) + "\"" +
           R"(,"created_at":")" + r[4] + "\"}";
}

// -----------------------------------------------------------------------
// Buyer inquiry JSON
// -----------------------------------------------------------------------
static std::string inquiry_to_json(const std::vector<std::string>& r,
                                    const std::string& replies_json = "") {
    // cols: id, listing_id, item_id, item_title, buyer_name, buyer_email,
    //       buyer_phone, subject, body, status, created_at
    return R"({"id":)" + r[0] +
           R"(,"listing_id":)" + r[1] +
           R"(,"item_id":)" + r[2] +
           R"(,"item_title":")" + util::json_escape(r[3]) + "\"" +
           R"(,"buyer_name":")" + util::json_escape(r[4]) + "\"" +
           R"(,"buyer_email":")" + util::json_escape(r[5]) + "\"" +
           R"(,"buyer_phone":")" + util::json_escape(r[6]) + "\"" +
           R"(,"subject":")" + util::json_escape(r[7]) + "\"" +
           R"(,"body":")" + util::json_escape(r[8]) + "\"" +
           R"(,"status":")" + r[9] + "\"" +
           R"(,"created_at":")" + r[10] + "\"" +
           (replies_json.empty() ? "" : R"(,"replies":)" + replies_json) +
           "}";
}

static std::string reply_to_json(const std::vector<std::string>& r) {
    // cols: id, inquiry_id, author, author_role, body, created_at
    return R"({"id":)" + r[0] +
           R"(,"inquiry_id":)" + r[1] +
           R"(,"author":")" + util::json_escape(r[2]) + "\"" +
           R"(,"author_role":")" + util::json_escape(r[3]) + "\"" +
           R"(,"body":")" + util::json_escape(r[4]) + "\"" +
           R"(,"created_at":")" + r[5] + "\"}";
}

void register_comments_routes(Router& r) {

    // ================================================================
    // ITEM COMMENTS (internal dealer thread)
    // ================================================================

    // GET /api/items/:id/comments
    r.add("GET", "/api/items/:id/comments", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(
            "SELECT id,item_id,author,body,created_at FROM item_comments "
            "WHERE item_id=? ORDER BY created_at ASC", {it->second});
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += comment_to_json(rows[i]);
        }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/items/:id/comments
    r.add("POST", "/api/items/:id/comments", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        if (req.user_id.empty()) return HttpResponse{401, R"({"error":"unauthorized"})"};

        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string body = j.value("body", "");
        if (body.empty()) return HttpResponse{400, R"({"error":"body required"})"};

        // Get author name from users table
        Rows ur = Db::instance().query("SELECT username FROM users WHERE id=?", {req.user_id});
        std::string author = ur.empty() ? "unknown" : ur[0][0];

        Db::instance().exec(
            "INSERT INTO item_comments(item_id,author,body) VALUES(?,?,?)",
            {it->second, author, body});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Comment added to item " + it->second + " by " + author);
        return HttpResponse{201,
            R"({"id":)" + id +
            R"(,"author":")" + util::json_escape(author) + "\"" +
            R"(,"body":")" + util::json_escape(body) + "\"" +
            R"(,"created_at":")" + util::now_iso() + "\"}",
        };
    });

    // DELETE /api/items/:id/comments/:comment_id  (admin only)
    r.add("DELETE", "/api/items/:id/comments/:comment_id", [](const HttpRequest& req) {
        if (req.role != "admin") return HttpResponse{403, R"({"error":"admin only"})"};
        auto cid = req.params.find("comment_id");
        if (cid == req.params.end()) return HttpResponse{400, R"({"error":"missing comment_id"})"};
        Db::instance().exec("DELETE FROM item_comments WHERE id=?", {cid->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // ================================================================
    // BUYER INQUIRIES (buyer contacts dealer about a listing)
    // ================================================================

    // GET /api/inquiries  -- all inquiries (dealer view, with optional filters)
    r.add("GET", "/api/inquiries", [](const HttpRequest& req) {
        std::string stat    = util::query_param(req.query, "status");
        std::string item_id = util::query_param(req.query, "item_id");
        std::string sql = R"(
SELECT bi.id,bi.listing_id,bi.item_id,i.title,
       bi.buyer_name,bi.buyer_email,bi.buyer_phone,
       bi.subject,bi.body,bi.status,bi.created_at
FROM buyer_inquiries bi JOIN items i ON i.id=bi.item_id WHERE 1=1)";
        std::vector<std::string> params;
        if (!stat.empty())    { sql += " AND bi.status=?";  params.push_back(stat); }
        if (!item_id.empty()) { sql += " AND bi.item_id=?"; params.push_back(item_id); }
        sql += " ORDER BY bi.created_at DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            // Count replies
            Rows rc = Db::instance().query(
                "SELECT COUNT(*) FROM inquiry_replies WHERE inquiry_id=?", {rows[i][0]});
            std::string reply_count = rc.empty() ? "0" : rc[0][0];
            out += inquiry_to_json(rows[i]);
            // Inject reply count
            out.pop_back(); // remove closing }
            out += R"(,"reply_count":)" + reply_count + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    // GET /api/inquiries/:id  -- single inquiry with full reply thread
    r.add("GET", "/api/inquiries/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(R"(
SELECT bi.id,bi.listing_id,bi.item_id,i.title,
       bi.buyer_name,bi.buyer_email,bi.buyer_phone,
       bi.subject,bi.body,bi.status,bi.created_at
FROM buyer_inquiries bi JOIN items i ON i.id=bi.item_id WHERE bi.id=?)", {it->second});
        if (rows.empty()) return HttpResponse{404, R"({"error":"not found"})"};

        Rows replies = Db::instance().query(
            "SELECT id,inquiry_id,author,author_role,body,created_at "
            "FROM inquiry_replies WHERE inquiry_id=? ORDER BY created_at ASC",
            {it->second});
        std::string replies_json = "[";
        for (size_t i = 0; i < replies.size(); ++i) {
            if (i) replies_json += ',';
            replies_json += reply_to_json(replies[i]);
        }
        replies_json += "]";
        return HttpResponse{200, inquiry_to_json(rows[0], replies_json)};
    });

    // POST /api/inquiries  -- buyer submits inquiry (NO auth required)
    r.add("POST", "/api/inquiries", [](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string listing_id  = std::to_string(j.value("listing_id", 0));
        std::string buyer_name  = j.value("buyer_name", "");
        std::string buyer_email = j.value("buyer_email", "");
        std::string body_text   = j.value("body", "");
        if (listing_id == "0" || buyer_name.empty() || buyer_email.empty() || body_text.empty())
            return HttpResponse{400, R"({"error":"listing_id, buyer_name, buyer_email, body required"})"};

        // Get item_id from listing
        Rows lr = Db::instance().query("SELECT item_id FROM listings WHERE id=?", {listing_id});
        if (lr.empty()) return HttpResponse{404, R"({"error":"listing not found"})"};
        std::string item_id = lr[0][0];

        Db::instance().exec(R"(
INSERT INTO buyer_inquiries(listing_id,item_id,buyer_name,buyer_email,buyer_phone,subject,body)
VALUES(?,?,?,?,?,?,?))",
            {listing_id, item_id,
             buyer_name, buyer_email,
             j.value("buyer_phone", ""),
             j.value("subject", ""),
             body_text});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Buyer inquiry received from " + buyer_name + " for listing " + listing_id);
        return HttpResponse{201, R"({"id":)" + id + R"(,"ok":true})"};
    });

    // PUT /api/inquiries/:id/status  -- update status (open/replied/closed)
    r.add("PUT", "/api/inquiries/:id/status", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string status = j.value("status", "open");
        if (status != "open" && status != "replied" && status != "closed")
            status = "open";
        Db::instance().exec("UPDATE buyer_inquiries SET status=? WHERE id=?",
                            {status, it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/inquiries/:id/replies  -- dealer or buyer posts a reply
    // Dealer replies require auth; buyer replies are open (matched by email token in prod)
    r.add("POST", "/api/inquiries/:id/replies", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string body_text = j.value("body", "");
        if (body_text.empty()) return HttpResponse{400, R"({"error":"body required"})"};

        std::string author, author_role;
        if (!req.user_id.empty()) {
            // Authenticated dealer/admin reply
            Rows ur = Db::instance().query("SELECT username FROM users WHERE id=?", {req.user_id});
            author      = ur.empty() ? "dealer" : ur[0][0];
            author_role = req.role.empty() ? "dealer" : req.role;
        } else {
            // Unauthenticated buyer reply — name comes from inquiry record
            Rows ir = Db::instance().query(
                "SELECT buyer_name FROM buyer_inquiries WHERE id=?", {it->second});
            author      = j.value("buyer_name", ir.empty() ? "Buyer" : ir[0][0]);
            author_role = "buyer";
        }

        Db::instance().exec(
            "INSERT INTO inquiry_replies(inquiry_id,author,author_role,body) VALUES(?,?,?,?)",
            {it->second, author, author_role, body_text});
        std::string id = Db::instance().last_insert_id();

        // Auto-update inquiry status when dealer replies
        if (author_role != "buyer")
            Db::instance().exec(
                "UPDATE buyer_inquiries SET status='replied' WHERE id=?", {it->second});

        LOG_INFO("Reply added to inquiry " + it->second + " by " + author);
        return HttpResponse{201,
            R"({"id":)" + id +
            R"(,"author":")" + util::json_escape(author) + "\"" +
            R"(,"author_role":")" + author_role + "\"" +
            R"(,"body":")" + util::json_escape(body_text) + "\"" +
            R"(,"created_at":")" + util::now_iso() + "\"}"};
    });

    // DELETE /api/inquiries/:id  (admin only)
    r.add("DELETE", "/api/inquiries/:id", [](const HttpRequest& req) {
        if (req.role != "admin") return HttpResponse{403, R"({"error":"admin only"})"};
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Db::instance().exec("DELETE FROM inquiry_replies WHERE inquiry_id=?", {it->second});
        Db::instance().exec("DELETE FROM buyer_inquiries WHERE id=?", {it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // GET /api/inquiries/summary  -- counts for badge display
    r.add("GET", "/api/inquiries/summary", [](const HttpRequest&) {
        Rows open   = Db::instance().query("SELECT COUNT(*) FROM buyer_inquiries WHERE status='open'");
        Rows all    = Db::instance().query("SELECT COUNT(*) FROM buyer_inquiries");
        Rows closed = Db::instance().query("SELECT COUNT(*) FROM buyer_inquiries WHERE status='closed'");
        return HttpResponse{200,
            R"({"open":)" + (open.empty()?"0":open[0][0]) +
            R"(,"total":)" + (all.empty()?"0":all[0][0]) +
            R"(,"closed":)" + (closed.empty()?"0":closed[0][0]) + "}"};
    });
}

} // namespace metis::antique
