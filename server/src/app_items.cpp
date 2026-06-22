#include "app_items.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace metis::antique {

// -----------------------------------------------------------------------
// Item JSON serializer
// -----------------------------------------------------------------------
static std::string item_to_json(const std::vector<std::string>& r) {
    // cols: id,title,category,era,maker,origin,material,source,description,
    //       condition,cost_price,asking_price,status,provenance,photo_ref,
    //       consignor_name,consignor_pct,created_at,updated_at,sku,owner_id
    auto n = [&](int i) -> std::string {
        return r[i].empty() ? "null" : r[i];
    };
    return R"({"id":)" + r[0] +
           R"(,"title":")" + util::json_escape(r[1]) + "\"" +
           R"(,"category":")" + util::json_escape(r[2]) + "\"" +
           R"(,"era":")" + util::json_escape(r[3]) + "\"" +
           R"(,"maker":")" + util::json_escape(r[4]) + "\"" +
           R"(,"origin":")" + util::json_escape(r[5]) + "\"" +
           R"(,"material":")" + util::json_escape(r[6]) + "\"" +
           R"(,"source":")" + util::json_escape(r[7]) + "\"" +
           R"(,"description":")" + util::json_escape(r[8]) + "\"" +
           R"(,"condition":")" + util::json_escape(r[9]) + "\"" +
           R"(,"cost_price":)" + (r[10].empty() ? "0" : r[10]) +
           R"(,"asking_price":)" + (r[11].empty() ? "0" : r[11]) +
           R"(,"status":")" + r[12] + "\"" +
           R"(,"provenance":")" + util::json_escape(r[13]) + "\"" +
           R"(,"photo_ref":")" + util::json_escape(r[14]) + "\"" +
           R"(,"consignor_name":")" + util::json_escape(r[15]) + "\"" +
           R"(,"consignor_pct":)" + (r[16].empty() ? "null" : r[16]) +
           R"(,"created_at":")" + r[17] + "\"" +
           R"(,"updated_at":")" + r[18] + "\"" +
           R"(,"sku":")" + util::json_escape(r.size() > 19 ? r[19] : "") + "\"" +
           R"(,"owner_id":)" + (r.size() > 20 && !r[20].empty() ? r[20] : "null") +
           "}";
}

static const char* ITEM_COLS = R"(
    id,title,category,era,maker,origin,material,
    COALESCE(source,''),description,condition,cost_price,asking_price,
    status,COALESCE(provenance,''),COALESCE(photo_ref,''),
    COALESCE(consignor_name,''),COALESCE(consignor_pct,''),
    created_at,updated_at,
    COALESCE(sku,''),COALESCE(owner_id,''))";

// -----------------------------------------------------------------------
// Audit log helper
// -----------------------------------------------------------------------
static void audit_change(const std::string& item_id, const std::string& field,
                          const std::string& old_v, const std::string& new_v,
                          const std::string& user_id) {
    if (old_v == new_v) return;
    Db::instance().exec(
        "INSERT INTO item_history(item_id,field,old_value,new_value,changed_by) "
        "VALUES(?,?,?,?,?)",
        {item_id, field, old_v, new_v, user_id});
}

void register_items_routes(Router& r, const Pson& cfg) {
    std::string photos_dir = util::resolve_exe_relative(
        cfg.get("app", "photos_dir", "photos"));
    std::filesystem::create_directories(photos_dir);

    // GET /api/items
    r.add("GET", "/api/items", [&cfg](const HttpRequest& req) {
        std::string q    = util::query_param(req.query, "q");
        std::string cat  = util::query_param(req.query, "category");
        std::string stat = util::query_param(req.query, "status");
        std::string page_s = util::query_param(req.query, "page");
        std::string limit_s = util::query_param(req.query, "limit");
        int page  = page_s.empty()  ? 1  : std::max(1, std::stoi(page_s));
        int default_limit = cfg.get_int("app", "items_per_page", 20);
        int limit = limit_s.empty() ? default_limit : std::min(100, std::max(1, std::stoi(limit_s)));
        int offset = (page - 1) * limit;

        std::string sql = std::string("SELECT") + ITEM_COLS +
            " FROM items WHERE 1=1";
        std::vector<std::string> params;
        // Dealers only see their own items
        if (req.role == "dealer" && !req.user_id.empty()) {
            sql += " AND owner_id=?";
            params.push_back(req.user_id);
        }
        if (!q.empty()) {
            sql += " AND (title LIKE ? OR maker LIKE ? OR era LIKE ? OR description LIKE ?)";
            std::string like = "%" + q + "%";
            params.insert(params.end(), {like, like, like, like});
        }
        if (!cat.empty())  { sql += " AND category=?"; params.push_back(cat); }
        if (!stat.empty()) { sql += " AND status=?";   params.push_back(stat); }

        // Count for pagination
        std::string count_sql = "SELECT COUNT(*) FROM items WHERE 1=1";
        std::vector<std::string> count_params;
        if (req.role == "dealer" && !req.user_id.empty()) {
            count_sql += " AND owner_id=?";
            count_params.push_back(req.user_id);
        }
        if (!q.empty()) {
            count_sql += " AND (title LIKE ? OR maker LIKE ? OR era LIKE ? OR description LIKE ?)";
            std::string like = "%" + q + "%";
            count_params.insert(count_params.end(), {like, like, like, like});
        }
        if (!cat.empty())  { count_sql += " AND category=?"; count_params.push_back(cat); }
        if (!stat.empty()) { count_sql += " AND status=?";   count_params.push_back(stat); }
        Rows cnt = Db::instance().query(count_sql, count_params);
        int total = cnt.empty() ? 0 : std::stoi(cnt[0][0]);

        sql += " ORDER BY updated_at DESC LIMIT ? OFFSET ?";
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        Rows rows = Db::instance().query(sql, params);
        std::string out = R"({"total":)" + std::to_string(total) +
                          R"(,"page":)" + std::to_string(page) +
                          R"(,"limit":)" + std::to_string(limit) +
                          R"(,"items":[)";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += item_to_json(rows[i]);
        }
        return HttpResponse{200, out + "]}"};
    });

    // GET /api/items/stats
    r.add("GET", "/api/items/stats", [](const HttpRequest&) {
        Rows r1 = Db::instance().query("SELECT COUNT(*) FROM items WHERE status='inventory'");
        Rows r2 = Db::instance().query("SELECT COUNT(*) FROM items WHERE status='listed'");
        Rows r3 = Db::instance().query("SELECT COUNT(*) FROM items WHERE status='sold'");
        Rows r4 = Db::instance().query("SELECT COALESCE(SUM(asking_price),0) FROM items WHERE status='inventory'");
        Rows r5 = Db::instance().query("SELECT COALESCE(SUM(cost_price),0) FROM items WHERE status='inventory'");
        return HttpResponse{200,
            R"({"inventory":)" + (r1.empty() ? "0" : r1[0][0]) +
            R"(,"listed":)" + (r2.empty() ? "0" : r2[0][0]) +
            R"(,"sold":)" + (r3.empty() ? "0" : r3[0][0]) +
            R"(,"inventory_value":)" + (r4.empty() ? "0" : r4[0][0]) +
            R"(,"inventory_cost":)" + (r5.empty() ? "0" : r5[0][0]) + "}"};
    });

    // GET /api/items/:id
    r.add("GET", "/api/items/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(
            std::string("SELECT") + ITEM_COLS + " FROM items WHERE id=?", {it->second});
        if (rows.empty()) return HttpResponse{404, R"({"error":"not found"})"};
        return HttpResponse{200, item_to_json(rows[0])};
    });

    // GET /api/items/:id/history
    r.add("GET", "/api/items/:id/history", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(
            "SELECT field,old_value,new_value,changed_by,changed_at "
            "FROM item_history WHERE item_id=? ORDER BY changed_at DESC", {it->second});
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"field":")" + util::json_escape(r[0]) + "\"" +
                   R"(,"old_value":")" + util::json_escape(r[1]) + "\"" +
                   R"(,"new_value":")" + util::json_escape(r[2]) + "\"" +
                   R"(,"changed_by":")" + util::json_escape(r[3]) + "\"" +
                   R"(,"changed_at":")" + r[4] + "\"}";
        }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/items
    r.add("POST", "/api/items", [&cfg](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string title    = j.value("title", "");
        std::string category = j.value("category", "");
        if (title.empty() || category.empty())
            return HttpResponse{400, R"({"error":"title and category required"})"};
        Db::instance().exec(R"(
INSERT INTO items(title,category,era,maker,origin,material,source,description,
                 condition,cost_price,asking_price,status,provenance,photo_ref,
                 consignor_name,consignor_pct,owner_id)
VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,NULLIF(?,''),NULLIF(?,'')))",
            {title, category,
             j.value("era",""), j.value("maker",""), j.value("origin",""),
             j.value("material",""), j.value("source",""), j.value("description",""),
             j.value("condition","Good"),
             std::to_string(j.value("cost_price",0.0)),
             std::to_string(j.value("asking_price",0.0)),
             j.value("status","inventory"),
             j.value("provenance",""), j.value("photo_ref",""),
             j.value("consignor_name",""),
             j.contains("consignor_pct") ? std::to_string(j["consignor_pct"].get<double>()) : "",
             req.user_id});
        std::string id = Db::instance().last_insert_id();
        // Auto-generate SKU: PREFIX-YYYY-NNNNN (prefix from config.pson sku.prefix)
        {
            std::string prefix = cfg.get("sku", "prefix", "AMP");
            char skubuf[64];
            std::time_t t = std::time(nullptr);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            snprintf(skubuf, sizeof(skubuf), "%s-%04d-%05d",
                     prefix.c_str(), tm.tm_year + 1900, std::stoi(id));
            Db::instance().exec("UPDATE items SET sku=? WHERE id=? AND sku IS NULL",
                                {std::string(skubuf), id});
        }
        LOG_INFO("Item created id=" + id + " title=" + title);
        return HttpResponse{201, R"({"id":)" + id + "}"};
    });

    // PUT /api/items/:id
    r.add("PUT", "/api/items/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string tid = it->second;
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }

        // Fetch current values for audit log
        Rows cur = Db::instance().query(
            std::string("SELECT") + ITEM_COLS + " FROM items WHERE id=?", {tid});
        if (cur.empty()) return HttpResponse{404, R"({"error":"not found"})"};

        // Fields to audit: title, asking_price, status, condition
        std::string new_title = j.value("title", cur[0][1]);
        std::string new_ask   = j.contains("asking_price") ?
            std::to_string(j["asking_price"].get<double>()) : cur[0][11];
        std::string new_stat  = j.value("status", cur[0][12]);
        std::string new_cond  = j.value("condition", cur[0][9]);

        audit_change(tid, "title",        cur[0][1], new_title, req.user_id);
        audit_change(tid, "asking_price", cur[0][11], new_ask,  req.user_id);
        audit_change(tid, "status",       cur[0][12], new_stat,  req.user_id);
        audit_change(tid, "condition",    cur[0][9],  new_cond,  req.user_id);

        bool ok = Db::instance().exec(R"(
UPDATE items SET title=?,category=?,era=?,maker=?,origin=?,material=?,source=?,
                description=?,condition=?,cost_price=?,asking_price=?,status=?,
                provenance=?,photo_ref=?,consignor_name=?,consignor_pct=NULLIF(?,''),
                updated_at=datetime('now') WHERE id=?)",
            {new_title,
             j.value("category", cur[0][2]),
             j.value("era", cur[0][3]), j.value("maker", cur[0][4]),
             j.value("origin", cur[0][5]), j.value("material", cur[0][6]),
             j.value("source", cur[0][7]), j.value("description", cur[0][8]),
             new_cond,
             j.contains("cost_price") ? std::to_string(j["cost_price"].get<double>()) : cur[0][10],
             new_ask, new_stat,
             j.value("provenance", cur[0][13]), j.value("photo_ref", cur[0][14]),
             j.value("consignor_name", cur[0][15]),
             j.contains("consignor_pct") ? std::to_string(j["consignor_pct"].get<double>()) : cur[0][16],
             tid});
        return ok ? HttpResponse{200, R"({"ok":true})"}
                  : HttpResponse{500, R"({"error":"update failed"})"};
    });

    // POST /api/items/:id/duplicate
    r.add("POST", "/api/items/:id/duplicate", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows cur = Db::instance().query(
            std::string("SELECT") + ITEM_COLS + " FROM items WHERE id=?", {it->second});
        if (cur.empty()) return HttpResponse{404, R"({"error":"not found"})"};
        const auto& c = cur[0];
        Db::instance().exec(R"(
INSERT INTO items(title,category,era,maker,origin,material,source,description,
                 condition,cost_price,asking_price,status,provenance,
                 consignor_name,consignor_pct)
VALUES(?,?,?,?,?,?,?,?,?,?,?,'inventory',?,?,NULLIF(?,'')))",
            {c[1]+" (copy)",c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],c[11],c[13],c[15],c[16]});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Item duplicated from " + it->second + " -> " + id);
        return HttpResponse{201, R"({"id":)" + id + "}"};
    });

    // DELETE /api/items/:id
    r.add("DELETE", "/api/items/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        // Cascade: remove all records that reference this item
        Db::instance().exec("DELETE FROM item_history     WHERE item_id=?", {it->second});
        Db::instance().exec("DELETE FROM appraisals       WHERE item_id=?", {it->second});
        Db::instance().exec("DELETE FROM buyer_inquiries  WHERE item_id=?", {it->second});
        // Note: sales are kept (historical record); item_id FK becomes dangling
        // but sales rows are preserved for P&L accuracy
        Db::instance().exec("DELETE FROM items WHERE id=?", {it->second});
        LOG_INFO("Item deleted id=" + it->second);
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/items/:id/photo  -- multipart form-data upload
    r.add("POST", "/api/items/:id/photo", [photos_dir](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string tid = it->second;
        // Extract binary content from multipart body
        // Look for boundary in Content-Type header
        std::string ct;
        auto cth = req.headers.find("content-type");
        if (cth != req.headers.end()) ct = cth->second;
        std::string boundary;
        auto bp = ct.find("boundary=");
        if (bp != std::string::npos) boundary = "--" + ct.substr(bp + 9);
        if (boundary.empty())
            return HttpResponse{400, R"({"error":"missing multipart boundary"})"};

        // Find file data after double CRLF
        auto body_start = req.body.find("\r\n\r\n");
        if (body_start == std::string::npos)
            return HttpResponse{400, R"({"error":"malformed multipart"})"};
        body_start += 4;
        std::string boundary_end = boundary + "--";
        auto body_end = req.body.find("\r\n" + boundary_end, body_start);
        if (body_end == std::string::npos) body_end = req.body.size();

        // Extract filename from Content-Disposition
        std::string fn = "photo_" + tid + ".jpg";
        auto cdn = req.body.find("filename=\"");
        if (cdn != std::string::npos && cdn < body_start) {
            auto fns = cdn + 10;
            auto fne = req.body.find('"', fns);
            if (fne != std::string::npos) fn = "item_" + tid + "_" + req.body.substr(fns, fne-fns);
        }

        std::string dest = photos_dir + "/" + fn;
        std::ofstream out(dest, std::ios::binary);
        if (!out.is_open()) return HttpResponse{500, R"({"error":"cannot write photo"})"};
        out.write(req.body.data() + body_start, body_end - body_start);
        out.close();

        Db::instance().exec("UPDATE items SET photo_ref=?,updated_at=datetime('now') WHERE id=?",
                            {fn, tid});
        LOG_INFO("Photo uploaded for item " + tid + ": " + fn);
        return HttpResponse{200, R"({"ok":true,"photo_ref":")" + util::json_escape(fn) + "\"}"};
    });

    // DELETE /api/items/:id/photo
    r.add("DELETE", "/api/items/:id/photo", [photos_dir](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows cur = Db::instance().query("SELECT photo_ref FROM items WHERE id=?", {it->second});
        if (!cur.empty() && !cur[0][0].empty()) {
            std::error_code ec;
            std::filesystem::remove(photos_dir + "/" + cur[0][0], ec);
        }
        Db::instance().exec("UPDATE items SET photo_ref='',updated_at=datetime('now') WHERE id=?",
                            {it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/items/import  -- CSV bulk import
    r.add("POST", "/api/items/import", [](const HttpRequest& req) {
        // Expect CSV with header: title,category,era,maker,origin,material,condition,cost_price,asking_price,provenance
        std::istringstream ss(req.body);
        std::string line;
        std::getline(ss, line); // skip header
        int imported = 0;
        std::vector<std::string> errors;

        auto split_csv = [](const std::string& s) {
            std::vector<std::string> fields;
            bool in_q = false;
            std::string cur;
            for (char c : s) {
                if (c == '"') { in_q = !in_q; }
                else if (c == ',' && !in_q) { fields.push_back(cur); cur.clear(); }
                else cur += c;
            }
            fields.push_back(cur);
            return fields;
        };

        int row = 1;
        while (std::getline(ss, line)) {
            ++row;
            if (line.empty() || line == "\r") continue;
            if (line.back() == '\r') line.pop_back();
            auto f = split_csv(line);
            while (f.size() < 10) f.push_back("");
            if (f[0].empty()) { errors.push_back("Row " + std::to_string(row) + ": missing title"); continue; }
            if (f[1].empty()) { errors.push_back("Row " + std::to_string(row) + ": missing category"); continue; }
            Db::instance().exec(R"(
INSERT INTO items(title,category,era,maker,origin,material,condition,
                 cost_price,asking_price,provenance,status)
VALUES(?,?,?,?,?,?,?,?,?,?,'inventory'))",
                {f[0],f[1],f[2],f[3],f[4],f[5],
                 f[6].empty()?"Good":f[6],
                 f[7].empty()?"0":f[7],
                 f[8].empty()?"0":f[8],
                 f[9]});
            ++imported;
        }
        std::string err_json = "[";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i) err_json += ',';
            err_json += "\"" + util::json_escape(errors[i]) + "\"";
        }
        LOG_INFO("CSV import: " + std::to_string(imported) + " rows");
        return HttpResponse{200,
            R"({"imported":)" + std::to_string(imported) +
            R"(,"errors":)" + err_json + "]}"};
    });
}

} // namespace metis::antique
