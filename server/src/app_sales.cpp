#include "app_sales.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <sstream>
#include <iomanip>

namespace metis::antique {

static std::string sale_to_json(const std::vector<std::string>& r) {
    // cols: id,item_id,title,cost_price,sale_price,platform_fee,shipping_cost,
    //       net_proceeds,payment_method,buyer_name,buyer_email,sale_date,channel,notes,margin_pct
    double cost  = r[3].empty() ? 0 : std::stod(r[3]);
    double price = r[4].empty() ? 0 : std::stod(r[4]);
    int margin = (cost > 0) ? static_cast<int>((price - cost) / cost * 100.0) : 0;
    return R"({"id":)" + r[0] +
           R"(,"item_id":)" + r[1] +
           R"(,"title":")" + util::json_escape(r[2]) + "\"" +
           R"(,"cost_price":)" + (r[3].empty()?"0":r[3]) +
           R"(,"sale_price":)" + (r[4].empty()?"0":r[4]) +
           R"(,"platform_fee":)" + (r[5].empty()?"0":r[5]) +
           R"(,"shipping_cost":)" + (r[6].empty()?"0":r[6]) +
           R"(,"net_proceeds":)" + (r[7].empty()?"0":r[7]) +
           R"(,"payment_method":")" + util::json_escape(r[8]) + "\"" +
           R"(,"buyer_name":")" + util::json_escape(r[9]) + "\"" +
           R"(,"buyer_email":")" + util::json_escape(r[10]) + "\"" +
           R"(,"sale_date":")" + r[11] + "\"" +
           R"(,"channel":")" + util::json_escape(r[12]) + "\"" +
           R"(,"notes":")" + util::json_escape(r[13]) + "\"" +
           R"(,"margin_pct":)" + std::to_string(margin) + "}";
}

void register_sales_routes(Router& r) {

    // GET /api/sales
    r.add("GET", "/api/sales", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string sql = R"(
SELECT s.id,s.item_id,i.title,i.cost_price,s.sale_price,
       COALESCE(s.platform_fee,0),COALESCE(s.shipping_cost,0),
       COALESCE(s.net_proceeds,s.sale_price),
       COALESCE(s.payment_method,''),
       COALESCE(s.buyer_name,''),COALESCE(s.buyer_email,''),
       s.sale_date,COALESCE(s.channel,''),COALESCE(s.notes,'')
FROM sales s JOIN items i ON i.id=s.item_id WHERE 1=1)";
        std::vector<std::string> params;
        // Dealers only see their own sales
        if (req.role == "dealer" && !req.user_id.empty()) {
            sql += " AND i.owner_id=?";
            params.push_back(req.user_id);
        }
        if (!from.empty()) { sql += " AND s.sale_date >= ?"; params.push_back(from); }
        if (!to.empty())   { sql += " AND s.sale_date <= ?"; params.push_back(to); }
        sql += " ORDER BY s.sale_date DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            out += sale_to_json(rows[i]);
        }
        return HttpResponse{200, out + "]"};
    });

    // GET /api/sales/:id
    r.add("GET", "/api/sales/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Rows rows = Db::instance().query(R"(
SELECT s.id,s.item_id,i.title,i.cost_price,s.sale_price,
       COALESCE(s.platform_fee,0),COALESCE(s.shipping_cost,0),
       COALESCE(s.net_proceeds,s.sale_price),
       COALESCE(s.payment_method,''),
       COALESCE(s.buyer_name,''),COALESCE(s.buyer_email,''),
       s.sale_date,COALESCE(s.channel,''),COALESCE(s.notes,'')
FROM sales s JOIN items i ON i.id=s.item_id WHERE s.id=?)", {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        return HttpResponse{200, sale_to_json(rows[0])};
    });

    // POST /api/sales
    r.add("POST", "/api/sales", [](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string item_id = std::to_string(j.value("item_id", 0));
        double sale_price   = j.value("sale_price", 0.0);
        if (item_id == "0" || sale_price <= 0)
            return HttpResponse{400, R"({"error":"item_id and sale_price required"})"};
        double platform_fee  = j.value("platform_fee", 0.0);
        double shipping_cost = j.value("shipping_cost", 0.0);
        double net_proceeds  = sale_price - platform_fee - shipping_cost;
        bool ok = Db::instance().exec(R"(
INSERT INTO sales(item_id,listing_id,buyer_name,buyer_email,sale_price,
                 platform_fee,shipping_cost,net_proceeds,payment_method,channel,notes)
VALUES(?,NULLIF(?,'0'),?,?,?,?,?,?,?,?,?))",
            {item_id,
             std::to_string(j.value("listing_id", 0)),
             j.value("buyer_name",""), j.value("buyer_email",""),
             std::to_string(sale_price),
             std::to_string(platform_fee), std::to_string(shipping_cost),
             std::to_string(net_proceeds),
             j.value("payment_method",""),
             j.value("channel",""), j.value("notes","")});
        if (ok) Db::instance().exec(
            "UPDATE items SET status='sold',updated_at=datetime('now') WHERE id=?", {item_id});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Sale recorded id=" + id + " item=" + item_id + " price=" + std::to_string(sale_price));
        return ok ? HttpResponse{201, R"({"id":)" + id + "}"}
                  : HttpResponse{500, R"({"error":"insert failed"})"};
    });

    // GET /api/sales/summary
    r.add("GET", "/api/sales/summary", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::vector<std::string> fp, tp;
        std::string date_filter = "";
        if (!from.empty()) { date_filter += " AND sale_date >= ?"; fp.push_back(from); tp.push_back(from); }
        if (!to.empty())   { date_filter += " AND sale_date <= ?"; fp.push_back(to);   tp.push_back(to); }

        Rows r1 = Db::instance().query(
            "SELECT COALESCE(SUM(sale_price),0),COUNT(*),COALESCE(SUM(net_proceeds),SUM(sale_price)) "
            "FROM sales WHERE sale_date >= date('now','start of month')" + date_filter, fp);
        Rows r2 = Db::instance().query(
            "SELECT COALESCE(SUM(sale_price),0),COALESCE(SUM(net_proceeds),SUM(sale_price)) "
            "FROM sales WHERE sale_date >= date('now','-30 days')" + date_filter, fp);
        Rows r3 = Db::instance().query(
            "SELECT COALESCE(SUM(s.sale_price),0),COALESCE(SUM(i.cost_price),0) "
            "FROM sales s JOIN items i ON i.id=s.item_id WHERE 1=1" + date_filter, tp);
        double total_rev  = r3.empty() || r3[0][0].empty() ? 0 : std::stod(r3[0][0]);
        double total_cost = r3.empty() || r3[0][1].empty() ? 0 : std::stod(r3[0][1]);
        int margin = (total_cost > 0) ?
            static_cast<int>((total_rev - total_cost) / total_cost * 100) : 0;
        return HttpResponse{200,
            R"({"month_revenue":)" + (r1.empty()?"0":r1[0][0]) +
            R"(,"month_net":)" + (r1.empty()?"0":r1[0][2]) +
            R"(,"month_count":)" + (r1.empty()?"0":r1[0][1]) +
            R"(,"rolling30_revenue":)" + (r2.empty()?"0":r2[0][0]) +
            R"(,"rolling30_net":)" + (r2.empty()?"0":r2[0][1]) +
            R"(,"avg_margin_pct":)" + std::to_string(margin) + "}"};
    });

    // GET /api/sales/export  -- CSV download
    r.add("GET", "/api/sales/export", [](const HttpRequest& req) {
        std::string year = util::query_param(req.query, "year");
        std::vector<std::string> params;
        std::string date_filter = "";
        if (!year.empty()) {
            date_filter = " AND s.sale_date >= ? AND s.sale_date < ?";
            params.push_back(year + "-01-01");
            params.push_back(std::to_string(std::stoi(year)+1) + "-01-01");
        }
        Rows rows = Db::instance().query(R"(
SELECT s.id,i.title,i.category,s.sale_date,s.sale_price,
       COALESCE(s.platform_fee,0),COALESCE(s.shipping_cost,0),
       COALESCE(s.net_proceeds,s.sale_price),i.cost_price,
       COALESCE(s.payment_method,''),COALESCE(s.channel,''),
       COALESCE(s.buyer_name,''),COALESCE(s.notes,'')
FROM sales s JOIN items i ON i.id=s.item_id WHERE 1=1)" + date_filter +
" ORDER BY s.sale_date DESC", params);

        std::ostringstream csv;
        csv << "id,title,category,sale_date,sale_price,platform_fee,"
               "shipping_cost,net_proceeds,cost_price,margin_pct,"
               "payment_method,channel,buyer_name,notes\n";
        for (const auto& r : rows) {
            double sale = r[4].empty()?0:std::stod(r[4]);
            double cost = r[8].empty()?0:std::stod(r[8]);
            int mg = (cost>0)?static_cast<int>((sale-cost)/cost*100):0;
            for (int i = 0; i < 9; ++i)
                csv << "\"" << r[i] << "\",";
            csv << mg << ",";
            for (int i = 9; i < 13; ++i) {
                csv << "\"" << r[i] << "\"";
                if (i < 12) csv << ",";
            }
            csv << "\n";
        }
        HttpResponse resp;
        resp.status = 200;
        resp.body = csv.str();
        resp.content_type = "text/csv";
        resp.headers["Content-Disposition"] =
            "attachment; filename=\"sales" + (year.empty()?"":"-"+year) + ".csv\"";
        return resp;
    });

    // GET /api/sales/:id/invoice  -- printable HTML
    r.add("GET", "/api/sales/:id/invoice", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, R"({"error":"missing id"})"};
        Rows rows = Db::instance().query(R"(
SELECT s.id,i.title,i.category,i.era,i.maker,i.description,i.condition,
       s.sale_price,COALESCE(s.platform_fee,0),COALESCE(s.shipping_cost,0),
       COALESCE(s.net_proceeds,s.sale_price),
       COALESCE(s.buyer_name,''),COALESCE(s.buyer_email,''),
       s.sale_date,COALESCE(s.channel,''),COALESCE(s.notes,''),
       COALESCE(i.photo_ref,'')
FROM sales s JOIN items i ON i.id=s.item_id WHERE s.id=?)", {it->second});
        if (rows.empty()) return HttpResponse{404, R"({"error":"not found"})"};
        const auto& r = rows[0];
        std::string html = R"(<!DOCTYPE html><html><head><meta charset="UTF-8">
<title>Receipt #)" + r[0] + R"(</title>
<style>body{font-family:Georgia,serif;max-width:680px;margin:40px auto;color:#222}
h1{border-bottom:2px solid #888;padding-bottom:8px}
table{width:100%;border-collapse:collapse;margin:16px 0}
td{padding:6px 10px;border:1px solid #ddd}
.label{font-weight:bold;width:140px;background:#f5f5f5}
.total{font-size:1.2em;font-weight:bold}
@media print{body{margin:0}}
</style></head><body>
<h1>Sale Receipt</h1>
<p><strong>Receipt #:)</strong> )" + r[0] + R"(</p>
<p><strong>Date:</strong> )" + r[13] + R"(</p>
<p><strong>Channel:</strong> )" + util::json_escape(r[14]) + R"(</p>
<h2>Item</h2>
<table>
<tr><td class="label">Title</td><td>)" + util::json_escape(r[1]) + R"(</td></tr>
<tr><td class="label">Category</td><td>)" + util::json_escape(r[2]) + R"(</td></tr>
<tr><td class="label">Era</td><td>)" + util::json_escape(r[3]) + R"(</td></tr>
<tr><td class="label">Maker</td><td>)" + util::json_escape(r[4]) + R"(</td></tr>
<tr><td class="label">Condition</td><td>)" + util::json_escape(r[6]) + R"(</td></tr>
<tr><td class="label">Description</td><td>)" + util::json_escape(r[5]) + R"(</td></tr>
</table>
<h2>Financial</h2>
<table>
<tr><td class="label">Sale price</td><td>$)" + r[7] + R"(</td></tr>
<tr><td class="label">Platform fee</td><td>$)" + r[8] + R"(</td></tr>
<tr><td class="label">Shipping</td><td>$)" + r[9] + R"(</td></tr>
<tr><td class="label total">Net proceeds</td><td class="total">$)" + r[10] + R"(</td></tr>
</table>
<h2>Buyer</h2>
<table>
<tr><td class="label">Name</td><td>)" + util::json_escape(r[11]) + R"(</td></tr>
<tr><td class="label">Email</td><td>)" + util::json_escape(r[12]) + R"(</td></tr>
</table>
)" + (r[15].empty() ? "" : "<p><strong>Notes:</strong> " + util::json_escape(r[15]) + "</p>") + R"(
<p style="margin-top:40px;color:#888;font-size:0.85em">Metis Antique Marketplace Plus</p>
</body></html>)";
        HttpResponse resp;
        resp.status = 200;
        resp.body = html;
        resp.content_type = "text/html; charset=utf-8";
        return resp;
    });
}

void register_sales_edit_routes(Router& r) {
    r.add("PUT", "/api/sales/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, "{\"error\":\"invalid JSON\"}"}; }
        Rows cur = Db::instance().query(
            "SELECT item_id,sale_price,platform_fee,shipping_cost,net_proceeds,"
            "payment_method,channel,buyer_name,buyer_email,notes,sale_date "
            "FROM sales WHERE id=?", {it->second});
        if (cur.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        double sp  = j.value("sale_price",   std::stod(cur[0][1]));
        double pf  = j.value("platform_fee", std::stod(cur[0][2]));
        double sc  = j.value("shipping_cost",std::stod(cur[0][3]));
        double np  = sp - pf - sc;
        Db::instance().exec(
            "UPDATE sales SET sale_price=?,platform_fee=?,shipping_cost=?,net_proceeds=?,"
            "payment_method=?,channel=?,buyer_name=?,buyer_email=?,notes=?,sale_date=? WHERE id=?",
            {std::to_string(sp), std::to_string(pf), std::to_string(sc), std::to_string(np),
             j.value("payment_method", cur[0][5]),
             j.value("channel",        cur[0][6]),
             j.value("buyer_name",     cur[0][7]),
             j.value("buyer_email",    cur[0][8]),
             j.value("notes",          cur[0][9]),
             j.value("sale_date",      cur[0][10]),
             it->second});
        LOG_INFO("Sale updated id=" + it->second);
        return HttpResponse{200, "{\"ok\":true}"};
    });

    r.add("DELETE", "/api/sales/:id", [](const HttpRequest& req) {
        if (req.role != "admin") return HttpResponse{403, "{\"error\":\"admin only\"}"};
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        // Restore item status to 'inventory'
        Rows sr = Db::instance().query("SELECT item_id FROM sales WHERE id=?", {it->second});
        if (!sr.empty())
            Db::instance().exec(
                "UPDATE items SET status='inventory',updated_at=datetime('now') WHERE id=?",
                {sr[0][0]});
        Db::instance().exec("DELETE FROM sales WHERE id=?", {it->second});
        LOG_INFO("Sale deleted id=" + it->second);
        return HttpResponse{200, "{\"ok\":true}"};
    });

    r.add("DELETE", "/api/appraisals/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Db::instance().exec("DELETE FROM appraisals WHERE id=?", {it->second});
        return HttpResponse{200, "{\"ok\":true}"};
    });
}

} // namespace metis::antique
