#include "app_listings.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"

namespace metis::antique {

void register_listings_routes(Router& r) {

    r.add("GET", "/api/listings", [](const HttpRequest& req) {
        std::string stat = util::query_param(req.query, "status");
        // expiring_soon computed in SQL -- no N+1 query per row
        std::string sql = R"(
SELECT l.id,l.item_id,i.title,l.channel,l.list_price,l.list_type,
       COALESCE(l.auction_end,''),l.views,l.watchers,l.status,
       COALESCE(l.listing_url,''),l.created_at,
       CASE WHEN l.auction_end IS NOT NULL AND l.auction_end != ''
            AND (julianday(l.auction_end)-julianday('now')) < 2
            AND julianday(l.auction_end) > julianday('now')
            THEN 1 ELSE 0 END AS expiring_soon
FROM listings l JOIN items i ON i.id=l.item_id WHERE 1=1)";
        std::vector<std::string> params;
        if (!stat.empty()) { sql += " AND l.status=?"; params.push_back(stat); }
        sql += " ORDER BY l.created_at DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& l = rows[i];
            bool expiring = l.size() > 12 && l[12] == "1";
            out += R"({"id":)" + l[0] +
                   R"(,"item_id":)" + l[1] +
                   R"(,"title":")" + util::json_escape(l[2]) + "\"" +
                   R"(,"channel":")" + util::json_escape(l[3]) + "\"" +
                   R"(,"list_price":)" + l[4] +
                   R"(,"list_type":")" + l[5] + "\"" +
                   R"(,"auction_end":")" + l[6] + "\"" +
                   R"(,"views":)" + l[7] +
                   R"(,"watchers":)" + l[8] +
                   R"(,"status":")" + l[9] + "\"" +
                   R"(,"listing_url":")" + util::json_escape(l[10]) + "\"" +
                   R"(,"created_at":")" + l[11] + "\"" +
                   R"(,"expiring_soon":)" + (expiring?"true":"false") + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    r.add("POST", "/api/listings", [](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string item_id = std::to_string(j.value("item_id", 0));
        std::string channel = j.value("channel", "");
        double price        = j.value("list_price", 0.0);
        if (item_id == "0" || channel.empty() || price <= 0)
            return HttpResponse{400, R"({"error":"item_id, channel, list_price required"})"};
        bool ok = Db::instance().exec(R"(
INSERT INTO listings(item_id,channel,list_price,list_type,auction_end,listing_url)
VALUES(?,?,?,?,NULLIF(?,''),NULLIF(?,'')))",
            {item_id, channel, std::to_string(price),
             j.value("list_type","fixed"),
             j.value("auction_end",""),
             j.value("listing_url","")});
        if (ok) Db::instance().exec(
            "UPDATE items SET status='listed',updated_at=datetime('now') WHERE id=?", {item_id});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Listing created id=" + id);
        return ok ? HttpResponse{201, R"({"id":)" + id + "}"}
                  : HttpResponse{500, R"({"error":"insert failed"})"};
    });

    // GET /api/listings/:id
    r.add("GET", "/api/listings/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"}; 
        Rows rows = Db::instance().query(R"(
SELECT l.id,l.item_id,i.title,l.channel,l.list_price,l.list_type,
       COALESCE(l.auction_end,''),l.views,l.watchers,l.status,
       COALESCE(l.listing_url,''),l.created_at,0
FROM listings l JOIN items i ON i.id=l.item_id WHERE l.id=?)", {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        const auto& l = rows[0];
        std::string o = "{\"id\":" + l[0] + ",\"item_id\":" + l[1] +
            ",\"title\":\"" + util::json_escape(l[2]) + "\"" +
            ",\"channel\":\"" + util::json_escape(l[3]) + "\"" +
            ",\"list_price\":" + l[4] +
            ",\"list_type\":\"" + l[5] + "\"" +
            ",\"auction_end\":\"" + l[6] + "\"" +
            ",\"views\":" + l[7] +
            ",\"watchers\":" + l[8] +
            ",\"status\":\"" + l[9] + "\"" +
            ",\"listing_url\":\"" + util::json_escape(l[10]) + "\"" +
            ",\"created_at\":\"" + l[11] + "\"" +
            ",\"expiring_soon\":false}";
        return HttpResponse{200, o};
    });

    r.add("PUT", "/api/listings/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        // Update individual fields
        if (j.contains("watchers"))
            Db::instance().exec("UPDATE listings SET watchers=? WHERE id=?",
                {std::to_string(j["watchers"].get<int>()), it->second});
        if (j.contains("listing_url"))
            Db::instance().exec("UPDATE listings SET listing_url=? WHERE id=?",
                {j["listing_url"].get<std::string>(), it->second});
        if (j.contains("list_price"))
            Db::instance().exec("UPDATE listings SET list_price=? WHERE id=?",
                {std::to_string(j["list_price"].get<double>()), it->second});
        if (j.contains("status"))
            Db::instance().exec("UPDATE listings SET status=? WHERE id=?",
                {j["status"].get<std::string>(), it->second});
        if (j.contains("auction_end"))
            Db::instance().exec("UPDATE listings SET auction_end=? WHERE id=?",
                {j["auction_end"].get<std::string>(), it->second});
        if (j.contains("channel"))
            Db::instance().exec("UPDATE listings SET channel=? WHERE id=?",
                {j["channel"].get<std::string>(), it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    r.add("DELETE", "/api/listings/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Db::instance().exec("UPDATE listings SET status='removed' WHERE id=?", {it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });

    r.add("POST", "/api/listings/:id/view", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Db::instance().exec("UPDATE listings SET views=views+1 WHERE id=?", {it->second});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

void register_shop_routes(Router& r) {

    // GET /shop/listings  -- all active listings with item details and primary photo
    r.add("GET", "/shop/listings", [](const HttpRequest& req) {
        std::string cat  = util::query_param(req.query, "category");
        std::string q    = util::query_param(req.query, "q");
        std::string page_s = util::query_param(req.query, "page");
        int page  = page_s.empty() ? 1 : std::max(1, std::stoi(page_s));
        int limit = 12;
        int offset = (page - 1) * limit;

        std::string sql = R"(
SELECT l.id, l.item_id, i.title, i.category, i.era, i.maker, i.origin,
       i.material, i.condition, i.description, l.list_price, l.list_type,
       l.auction_end, l.views, l.channel,
       COALESCE((SELECT filename FROM item_photos
                 WHERE item_id=i.id AND is_primary=1 LIMIT 1),
                NULLIF(i.photo_ref,''), '') AS primary_photo
FROM listings l
JOIN items i ON i.id = l.item_id
WHERE l.status = 'active' AND i.status IN ('listed','inventory'))";
        std::vector<std::string> params;
        if (!cat.empty()) { sql += " AND i.category=?"; params.push_back(cat); }
        if (!q.empty()) {
            sql += " AND (i.title LIKE ? OR i.maker LIKE ? OR i.description LIKE ?)";
            std::string like = "%" + q + "%";
            params.insert(params.end(), {like, like, like});
        }

        // Count
        std::string count_sql = "SELECT COUNT(*) FROM listings l JOIN items i ON i.id=l.item_id WHERE l.status='active' AND i.status IN ('listed','inventory')";
        if (!cat.empty()) count_sql += " AND i.category=?";
        if (!q.empty())   count_sql += " AND (i.title LIKE ? OR i.maker LIKE ? OR i.description LIKE ?)";
        Rows cnt = Db::instance().query(count_sql, params);
        int total = cnt.empty() ? 0 : std::stoi(cnt[0][0]);

        sql += " ORDER BY l.created_at DESC LIMIT ? OFFSET ?";
        params.push_back(std::to_string(limit));
        params.push_back(std::to_string(offset));

        Rows rows = Db::instance().query(sql, params);
        std::string out = R"({"total":)" + std::to_string(total) +
                          R"(,"page":)" + std::to_string(page) +
                          R"(,"limit":)" + std::to_string(limit) +
                          R"(,"listings":[)";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"listing_id":)" + r[0] +
                   R"(,"item_id":)" + r[1] +
                   R"(,"title":")" + util::json_escape(r[2]) + "\"" +
                   R"(,"category":")" + util::json_escape(r[3]) + "\"" +
                   R"(,"era":")" + util::json_escape(r[4]) + "\"" +
                   R"(,"maker":")" + util::json_escape(r[5]) + "\"" +
                   R"(,"origin":")" + util::json_escape(r[6]) + "\"" +
                   R"(,"material":")" + util::json_escape(r[7]) + "\"" +
                   R"(,"condition":")" + util::json_escape(r[8]) + "\"" +
                   R"(,"description":")" + util::json_escape(r[9]) + "\"" +
                   R"(,"price":)" + r[10] +
                   R"(,"list_type":")" + r[11] + "\"" +
                   R"(,"auction_end":")" + r[12] + "\"" +
                   R"(,"views":)" + r[13] +
                   R"(,"channel":")" + util::json_escape(r[14]) + "\"" +
                   R"(,"primary_photo":")" + util::json_escape(r[15]) + "\"" +
                   R"(,"photo_url":)" +
                   (r[15].empty() ? "null" :
                    "\"" + util::json_escape("/photos/" + r[15]) + "\"") +
                   "}";
        }
        // Increment views for all returned (not per-item to avoid spam)
        return HttpResponse{200, out + "]}"};
    });

    // GET /shop/listings/:id  -- full listing detail with all photos and description
    r.add("GET", "/shop/listings/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(R"(
SELECT l.id, l.item_id, i.title, i.category, i.era, i.maker, i.origin,
       i.material, i.condition, i.description, i.provenance,
       l.list_price, l.list_type, l.auction_end, l.views, l.channel,
       l.listing_url
FROM listings l JOIN items i ON i.id=l.item_id
WHERE l.id=? AND l.status='active')", {it->second});
        if (rows.empty()) return HttpResponse{404, R"({"error":"not found"})"};
        const auto& row = rows[0];

        // All photos
        Rows photos = Db::instance().query(
            "SELECT id,filename,caption,is_primary,sort_order FROM item_photos "
            "WHERE item_id=? ORDER BY is_primary DESC, sort_order, id",
            {row[1]});

        std::string photos_json = "[";
        for (size_t i = 0; i < photos.size(); ++i) {
            if (i) photos_json += ',';
            const auto& p = photos[i];
            photos_json += R"({"id":)" + p[0] +
                           R"(,"filename":")" + util::json_escape(p[1]) + "\"" +
                           R"(,"url":"/photos/)" + util::json_escape(p[1]) + "\"" +
                           R"(,"caption":")" + util::json_escape(p[2]) + "\"" +
                           R"(,"is_primary":)" + (p[3]=="1"?"true":"false") + "}";
        }
        photos_json += "]";

        // Fallback to legacy photo_ref if no item_photos rows
        if (photos.empty()) {
            Rows pr = Db::instance().query(
                "SELECT NULLIF(photo_ref,'') FROM items WHERE id=?", {row[1]});
            if (!pr.empty() && !pr[0][0].empty()) {
                photos_json = R"([{"id":0,"filename":")" + util::json_escape(pr[0][0]) +
                              R"(","url":"/photos/)" + util::json_escape(pr[0][0]) +
                              R"(","caption":"","is_primary":true}])";
            }
        }

        // Increment view counter
        Db::instance().exec("UPDATE listings SET views=views+1 WHERE id=?", {it->second});

        return HttpResponse{200,
            R"({"listing_id":)" + row[0] +
            R"(,"item_id":)" + row[1] +
            R"(,"title":")" + util::json_escape(row[2]) + "\"" +
            R"(,"category":")" + util::json_escape(row[3]) + "\"" +
            R"(,"era":")" + util::json_escape(row[4]) + "\"" +
            R"(,"maker":")" + util::json_escape(row[5]) + "\"" +
            R"(,"origin":")" + util::json_escape(row[6]) + "\"" +
            R"(,"material":")" + util::json_escape(row[7]) + "\"" +
            R"(,"condition":")" + util::json_escape(row[8]) + "\"" +
            R"(,"description":")" + util::json_escape(row[9]) + "\"" +
            R"(,"provenance":")" + util::json_escape(row[10]) + "\"" +
            R"(,"price":)" + row[11] +
            R"(,"list_type":")" + row[12] + "\"" +
            R"(,"auction_end":")" + row[13] + "\"" +
            R"(,"views":)" + row[14] +
            R"(,"channel":")" + util::json_escape(row[15]) + "\"" +
            R"(,"listing_url":")" + util::json_escape(row[16]) + "\"" +
            R"(,"photos":)" + photos_json + "}"};
    });

    // GET /shop/categories  -- distinct active categories for filter nav
    r.add("GET", "/shop/categories", [](const HttpRequest&) {
        Rows rows = Db::instance().query(R"(
SELECT DISTINCT i.category, COUNT(l.id) as cnt
FROM listings l JOIN items i ON i.id=l.item_id
WHERE l.status='active'
GROUP BY i.category ORDER BY cnt DESC)");
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += R"({"category":")" + util::json_escape(rows[i][0]) +
                   R"(","count":)" + rows[i][1] + "}";
        }
        return HttpResponse{200, out + "]"};
    });
}

} // namespace metis::antique
