#include "app_photos.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace metis::antique {

static std::string photo_to_json(const std::vector<std::string>& r) {
    // cols: id, item_id, filename, caption, is_primary, sort_order, uploaded_by, created_at
    return R"({"id":)" + r[0] +
           R"(,"item_id":)" + r[1] +
           R"(,"filename":")" + util::json_escape(r[2]) + "\"" +
           R"(,"url":"/photos/)" + util::json_escape(r[2]) + "\"" +
           R"(,"caption":")" + util::json_escape(r[3]) + "\"" +
           R"(,"is_primary":)" + (r[4]=="1"?"true":"false") +
           R"(,"sort_order":)" + (r[5].empty()?"0":r[5]) +
           R"(,"uploaded_by":")" + util::json_escape(r[6]) + "\"" +
           R"(,"created_at":")" + r[7] + "\"}";
}

void register_photos_routes(Router& r, const Pson& cfg) {
    std::string photos_dir = util::resolve_exe_relative(
        cfg.get("app", "photos_dir", "photos"));
    std::filesystem::create_directories(photos_dir);

    // GET /api/items/:id/photos  -- all photos for an item
    r.add("GET", "/api/items/:id/photos", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(
            "SELECT id,item_id,filename,caption,is_primary,sort_order,uploaded_by,created_at "
            "FROM item_photos WHERE item_id=? ORDER BY sort_order,id",
            {it->second});
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += photo_to_json(rows[i]);
        }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/items/:id/photos  -- upload one or more photos (multipart)
    r.add("POST", "/api/items/:id/photos", [photos_dir](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string tid = it->second;

        // Get author from session
        Rows ur = Db::instance().query("SELECT username FROM users WHERE id=?", {req.user_id});
        std::string author = ur.empty() ? "unknown" : ur[0][0];

        // Check whether this item already has a primary
        Rows has_primary = Db::instance().query(
            "SELECT COUNT(*) FROM item_photos WHERE item_id=? AND is_primary=1", {tid});
        bool need_primary = has_primary.empty() || has_primary[0][0] == "0";

        // Get caption from JSON body field (if present alongside multipart)
        std::string caption;
        auto cap_pos = req.body.find("\"caption\"");
        if (cap_pos != std::string::npos) {
            auto colon = req.body.find(':', cap_pos);
            if (colon != std::string::npos) {
                auto qs = req.body.find('"', colon+1);
                auto qe = qs != std::string::npos ? req.body.find('"', qs+1) : std::string::npos;
                if (qs != std::string::npos && qe != std::string::npos)
                    caption = req.body.substr(qs+1, qe-qs-1);
            }
        }

        // Extract content-type boundary
        std::string ct;
        auto cth = req.headers.find("content-type");
        if (cth != req.headers.end()) ct = cth->second;
        std::string boundary;
        auto bp = ct.find("boundary=");
        if (bp != std::string::npos) boundary = "--" + ct.substr(bp + 9);

        std::vector<std::string> saved_files;

        if (!boundary.empty()) {
            // Parse multipart -- extract all file parts
            size_t search_pos = 0;
            int part_num = 0;
            while (true) {
                size_t part_start = req.body.find(boundary, search_pos);
                if (part_start == std::string::npos) break;
                part_start += boundary.size();
                if (part_start + 2 > req.body.size()) break;
                if (req.body[part_start] == '-' && req.body[part_start+1] == '-') break;
                if (req.body[part_start] == '\r') part_start += 2;

                size_t header_end = req.body.find("\r\n\r\n", part_start);
                if (header_end == std::string::npos) break;
                std::string part_headers = req.body.substr(part_start, header_end - part_start);

                size_t body_start = header_end + 4;
                size_t body_end   = req.body.find("\r\n" + boundary, body_start);
                if (body_end == std::string::npos) body_end = req.body.size();

                // Extract filename from Content-Disposition
                std::string fn_base = "photo_" + tid + "_" + std::to_string(++part_num);
                std::string ext = ".jpg";
                auto fn_pos = part_headers.find("filename=\"");
                if (fn_pos != std::string::npos) {
                    auto fn_start = fn_pos + 10;
                    auto fn_end = part_headers.find('"', fn_start);
                    if (fn_end != std::string::npos) {
                        std::string orig = part_headers.substr(fn_start, fn_end - fn_start);
                        auto dot = orig.rfind('.');
                        if (dot != std::string::npos) ext = orig.substr(dot);
                        fn_base = "item" + tid + "_p" + std::to_string(part_num);
                    }
                }

                // Skip non-file parts (text fields)
                if (part_headers.find("filename=") == std::string::npos) {
                    // Could be caption text field
                    if (part_headers.find("name=\"caption\"") != std::string::npos) {
                        caption = req.body.substr(body_start, body_end - body_start);
                    }
                    search_pos = body_end;
                    continue;
                }

                std::string filename = fn_base + ext;
                std::string dest = photos_dir + "/" + filename;
                std::ofstream out(dest, std::ios::binary);
                if (out.is_open()) {
                    out.write(req.body.data() + body_start, body_end - body_start);
                    out.close();
                    saved_files.push_back(filename);
                }
                search_pos = body_end;
            }
        } else {
            // Raw binary upload (single file, filename from query param or header)
            std::string fn = util::query_param(req.query, "filename");
            if (fn.empty()) fn = "item" + tid + "_p1.jpg";
            std::string dest = photos_dir + "/" + fn;
            std::ofstream out(dest, std::ios::binary);
            if (out.is_open()) {
                out.write(req.body.data(), req.body.size());
                out.close();
                saved_files.push_back(fn);
            }
        }

        if (saved_files.empty())
            return HttpResponse{400, R"({"error":"no files received"})"};

        // Get current max sort_order
        Rows mx = Db::instance().query(
            "SELECT COALESCE(MAX(sort_order),0) FROM item_photos WHERE item_id=?", {tid});
        int sort = mx.empty() ? 0 : std::stoi(mx[0][0]);

        std::string ids_json = "[";
        for (size_t i = 0; i < saved_files.size(); ++i) {
            bool is_primary = need_primary && i == 0;
            Db::instance().exec(
                "INSERT INTO item_photos(item_id,filename,caption,is_primary,sort_order,uploaded_by) "
                "VALUES(?,?,?,?,?,?)",
                {tid, saved_files[i], caption,
                 is_primary ? "1" : "0",
                 std::to_string(++sort), author});
            if (is_primary) {
                // Also update legacy photo_ref for backward compat
                Db::instance().exec(
                    "UPDATE items SET photo_ref=?,updated_at=datetime('now') WHERE id=?",
                    {saved_files[i], tid});
                need_primary = false;
            }
            if (i) ids_json += ',';
            ids_json += "\"" + util::json_escape(saved_files[i]) + "\"";
        }
        LOG_INFO("Photos uploaded for item " + tid + ": " + std::to_string(saved_files.size()) + " file(s)");
        return HttpResponse{201, R"({"ok":true,"count":)" +
            std::to_string(saved_files.size()) + R"(,"files":)" + ids_json + "]}"};
    });

    // PATCH /api/items/:id/photos/:photo_id  -- update caption or make primary
    r.add("PUT", "/api/items/:id/photos/:photo_id", [](const HttpRequest& req) {
        auto pid = req.params.find("photo_id");
        auto iid = req.params.find("id");
        if (pid == req.params.end()) return HttpResponse{400, R"({"error":"missing photo_id"})"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }

        if (j.contains("caption"))
            Db::instance().exec("UPDATE item_photos SET caption=? WHERE id=?",
                {j["caption"].get<std::string>(), pid->second});

        if (j.value("is_primary", false)) {
            // Clear existing primary for this item
            Db::instance().exec(
                "UPDATE item_photos SET is_primary=0 WHERE item_id=?", {iid->second});
            Db::instance().exec(
                "UPDATE item_photos SET is_primary=1 WHERE id=?", {pid->second});
            // Update legacy photo_ref
            Rows fn = Db::instance().query("SELECT filename FROM item_photos WHERE id=?", {pid->second});
            if (!fn.empty())
                Db::instance().exec(
                    "UPDATE items SET photo_ref=?,updated_at=datetime('now') WHERE id=?",
                    {fn[0][0], iid->second});
        }

        if (j.contains("sort_order"))
            Db::instance().exec("UPDATE item_photos SET sort_order=? WHERE id=?",
                {std::to_string(j["sort_order"].get<int>()), pid->second});

        return HttpResponse{200, R"({"ok":true})"};
    });

    // DELETE /api/items/:id/photos/:photo_id
    r.add("DELETE", "/api/items/:id/photos/:photo_id", [photos_dir](const HttpRequest& req) {
        auto pid = req.params.find("photo_id");
        auto iid = req.params.find("id");
        if (pid == req.params.end()) return HttpResponse{400, R"({"error":"missing photo_id"})"};
        Rows fn = Db::instance().query(
            "SELECT filename,is_primary FROM item_photos WHERE id=?", {pid->second});
        if (!fn.empty()) {
            std::error_code ec;
            std::filesystem::remove(photos_dir + "/" + fn[0][0], ec);
            if (fn[0][1] == "1") {
                // Deleted the primary — promote next photo if any
                Db::instance().exec("DELETE FROM item_photos WHERE id=?", {pid->second});
                Rows next = Db::instance().query(
                    "SELECT id,filename FROM item_photos WHERE item_id=? ORDER BY sort_order,id LIMIT 1",
                    {iid->second});
                if (!next.empty()) {
                    Db::instance().exec("UPDATE item_photos SET is_primary=1 WHERE id=?", {next[0][0]});
                    Db::instance().exec(
                        "UPDATE items SET photo_ref=?,updated_at=datetime('now') WHERE id=?",
                        {next[0][1], iid->second});
                } else {
                    Db::instance().exec(
                        "UPDATE items SET photo_ref='',updated_at=datetime('now') WHERE id=?",
                        {iid->second});
                }
                return HttpResponse{200, R"({"ok":true})"};
            }
        }
        Db::instance().exec("DELETE FROM item_photos WHERE id=?", {pid->second});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

} // namespace metis::antique
