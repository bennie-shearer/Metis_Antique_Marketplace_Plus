#include "app_appraisals.hpp"
#include "db.hpp"
#include "util.hpp"

namespace metis::antique {

static std::string pf(const std::string& body, const std::string& key) {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos); if (pos == std::string::npos) return "";
    size_t vs = pos + 1;
    while (vs < body.size() && body[vs] == ' ') ++vs;
    if (vs >= body.size()) return "";
    if (body[vs] == '"') {
        auto end = body.find('"', vs + 1);
        return end == std::string::npos ? "" : body.substr(vs + 1, end - vs - 1);
    }
    size_t end = vs;
    while (end < body.size() && body[end] != ',' && body[end] != '}') ++end;
    std::string v = body.substr(vs, end - vs);
    v.erase(v.find_last_not_of(" \t\r\n") + 1);
    return v;
}

void register_appraisals_routes(Router& r) {

    r.add("GET", "/api/appraisals", [](const HttpRequest&) {
        Rows rows = Db::instance().query(R"(
SELECT a.id,a.item_id,i.title,a.appraiser,a.appraised_at,
       a.value_low,a.value_high,a.condition,a.notes,a.status
FROM appraisals a JOIN items i ON i.id=a.item_id
ORDER BY a.appraised_at DESC)");
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"id":)" + r[0] +
                   R"(,"item_id":)" + r[1] +
                   R"(,"title":")" + util::json_escape(r[2]) + "\"" +
                   R"(,"appraiser":")" + util::json_escape(r[3]) + "\"" +
                   R"(,"appraised_at":")" + r[4] + "\"" +
                   R"(,"value_low":)" + (r[5].empty() ? "null" : r[5]) +
                   R"(,"value_high":)" + (r[6].empty() ? "null" : r[6]) +
                   R"(,"condition":")" + util::json_escape(r[7]) + "\"" +
                   R"(,"notes":")" + util::json_escape(r[8]) + "\"" +
                   R"(,"status":")" + r[9] + "\"}";
        }
        return HttpResponse{200, out + "]"};
    });

    r.add("POST", "/api/appraisals", [](const HttpRequest& req) {
        std::string item_id = pf(req.body, "item_id");
        if (item_id.empty()) return HttpResponse{400, R"({"error":"item_id required"})"};
        std::string vl = pf(req.body, "value_low");
        std::string vh = pf(req.body, "value_high");
        bool ok = Db::instance().exec(R"(
INSERT INTO appraisals(item_id,appraiser,value_low,value_high,condition,notes,status)
VALUES(?,?,NULLIF(?,''),NULLIF(?,''),?,?,?))",
            {item_id, pf(req.body, "appraiser"), vl, vh,
             pf(req.body, "condition"), pf(req.body, "notes"),
             pf(req.body, "status").empty() ? "pending" : pf(req.body, "status")});
        std::string id = Db::instance().last_insert_id();
        return ok ? HttpResponse{201, R"({"id":)" + id + "}"} :
                    HttpResponse{500, R"({"error":"insert failed"})"};
    });

    // GET /api/appraisals/:id
    r.add("GET", "/api/appraisals/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Rows rows = Db::instance().query(R"(
SELECT a.id,a.item_id,i.title,a.appraiser,a.appraised_at,
       a.value_low,a.value_high,a.condition,a.notes,a.status
FROM appraisals a JOIN items i ON i.id=a.item_id WHERE a.id=?)", {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        const auto& r = rows[0];
        std::string o = "{\"id\":" + r[0] + ",\"item_id\":" + r[1] +
            ",\"title\":\"" + util::json_escape(r[2]) + "\"" +
            ",\"appraiser\":\"" + util::json_escape(r[3]) + "\"" +
            ",\"appraised_at\":\"" + r[4] + "\"" +
            ",\"value_low\":" + (r[5].empty() ? "null" : r[5]) +
            ",\"value_high\":" + (r[6].empty() ? "null" : r[6]) +
            ",\"condition\":\"" + util::json_escape(r[7]) + "\"" +
            ",\"notes\":\"" + util::json_escape(r[8]) + "\"" +
            ",\"status\":\"" + util::json_escape(r[9]) + "\"}"; 
        return HttpResponse{200, o};
    });
    r.add("PUT", "/api/appraisals/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        std::string vl = pf(req.body, "value_low");
        std::string vh = pf(req.body, "value_high");
        bool ok = Db::instance().exec(R"(
UPDATE appraisals SET appraiser=?,value_low=NULLIF(?,''),value_high=NULLIF(?,''),
       condition=?,notes=?,status=?,appraised_at=datetime('now') WHERE id=?)",
            {pf(req.body, "appraiser"), vl, vh, pf(req.body, "condition"),
             pf(req.body, "notes"), pf(req.body, "status"), it->second});
        return ok ? HttpResponse{200, R"({"ok":true})"} :
                    HttpResponse{500, R"({"error":"update failed"})"};
    });
}

} // namespace metis::antique
