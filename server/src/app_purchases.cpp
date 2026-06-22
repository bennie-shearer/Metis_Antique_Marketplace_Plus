#include "app_purchases.hpp"
#include "db.hpp"
#include "util.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <sstream>
#include <iomanip>

namespace metis::antique {

static std::string je(const std::string& s) { return util::json_escape(s); }

static std::string pjson(const std::vector<std::string>& r) {
    auto n = [&](int i) -> const std::string& {
        static const std::string zero = "0";
        return r[i].empty() ? zero : r[i];
    };
    std::string o = "{";
    o += "\"id\":"                  + r[0];
    o += ",\"item_id\":"            + r[1];
    o += ",\"item_title\":\""       + je(r[2])  + "\"";
    o += ",\"vendor_name\":\""      + je(r[3])  + "\"";
    o += ",\"vendor_contact\":\""   + je(r[4])  + "\"";
    o += ",\"acquisition_date\":\"" + je(r[5])  + "\"";
    o += ",\"purchase_price\":"     + n(6);
    o += ",\"buyers_premium\":"     + n(7);
    o += ",\"transport_cost\":"     + n(8);
    o += ",\"restoration_budget\":" + n(9);
    o += ",\"total_cost\":"         + n(10);
    o += ",\"purchase_channel\":\"" + je(r[11]) + "\"";
    o += ",\"payment_method\":\""   + je(r[12]) + "\"";
    o += ",\"receipt_ref\":\""      + je(r[13]) + "\"";
    o += ",\"notes\":\""            + je(r[14]) + "\"";
    o += ",\"created_at\":\""       + je(r[15]) + "\"";
    o += "}";
    return o;
}

static std::string ejson(const std::vector<std::string>& r) {
    std::string o = "{";
    o += "\"id\":"                + r[0];
    o += ",\"category\":\""       + je(r[1]) + "\"";
    o += ",\"description\":\""    + je(r[2]) + "\"";
    o += ",\"amount\":"           + (r[3].empty() ? "0" : r[3]);
    o += ",\"expense_date\":\""   + je(r[4]) + "\"";
    o += ",\"payment_method\":\"" + je(r[5]) + "\"";
    o += ",\"vendor\":\""         + je(r[6]) + "\"";
    o += ",\"receipt_ref\":\""    + je(r[7]) + "\"";
    o += ",\"notes\":\""          + je(r[8]) + "\"";
    o += ",\"created_at\":\""     + je(r[9]) + "\"";
    o += "}";
    return o;
}

static const char* PCOLS = R"(
    p.id, p.item_id, i.title,
    p.vendor_name, p.vendor_contact, p.acquisition_date,
    p.purchase_price, p.buyers_premium, p.transport_cost,
    COALESCE(p.restoration_budget,0),
    p.total_cost, p.purchase_channel, p.payment_method,
    p.receipt_ref, p.notes, p.created_at
FROM purchases p JOIN items i ON i.id=p.item_id)";

void register_purchases_routes(Router& r) {

    // ============================================================
    // PURCHASES
    // ============================================================

    // GET /api/purchases
    r.add("GET", "/api/purchases", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string ch   = util::query_param(req.query, "channel");
        std::string sql  = std::string("SELECT") + PCOLS + " WHERE 1=1";
        std::vector<std::string> params;
        if (!from.empty()) { sql += " AND p.acquisition_date >= ?"; params.push_back(from); }
        if (!to.empty())   { sql += " AND p.acquisition_date <= ?"; params.push_back(to);   }
        if (!ch.empty())   { sql += " AND p.purchase_channel = ?";  params.push_back(ch);   }
        sql += " ORDER BY p.acquisition_date DESC, p.id DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) { if (i) out += ','; out += pjson(rows[i]); }
        return HttpResponse{200, out + "]"};
    });

    // GET /api/purchases/:id
    r.add("GET", "/api/purchases/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Rows rows = Db::instance().query(std::string("SELECT") + PCOLS + " WHERE p.id=?",
                                         {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        return HttpResponse{200, pjson(rows[0])};
    });

    // GET /api/items/:id/purchase
    r.add("GET", "/api/items/:id/purchase", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Rows rows = Db::instance().query(
            std::string("SELECT") + PCOLS + " WHERE p.item_id=? LIMIT 1", {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"no purchase record\"}"};
        return HttpResponse{200, pjson(rows[0])};
    });

    // POST /api/purchases
    r.add("POST", "/api/purchases", [](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, "{\"error\":\"invalid JSON\"}"}; }
        std::string item_id = std::to_string(j.value("item_id", 0));
        if (item_id == "0") return HttpResponse{400, "{\"error\":\"item_id required\"}"};
        double pp  = j.value("purchase_price",    0.0);
        double bp  = j.value("buyers_premium",    0.0);
        double tc  = j.value("transport_cost",    0.0);
        double rb  = j.value("restoration_budget",0.0);
        double tot = pp + bp + tc + rb;
        if (j.contains("total_cost") && j["total_cost"].get<double>() > 0)
            tot = j["total_cost"].get<double>();
        std::string acq = j.value("acquisition_date", util::today_iso());
        bool ok = Db::instance().exec(
            "INSERT INTO purchases(item_id,vendor_name,vendor_contact,acquisition_date,"
            "purchase_price,buyers_premium,transport_cost,restoration_budget,total_cost,"
            "purchase_channel,payment_method,receipt_ref,notes) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {item_id, j.value("vendor_name",""), j.value("vendor_contact",""), acq,
             std::to_string(pp), std::to_string(bp), std::to_string(tc),
             std::to_string(rb), std::to_string(tot),
             j.value("purchase_channel",""), j.value("payment_method",""),
             j.value("receipt_ref",""), j.value("notes","")});
        if (!ok) return HttpResponse{500, "{\"error\":\"insert failed\"}"};
        Db::instance().exec(
            "UPDATE items SET cost_price=?,updated_at=datetime('now') WHERE id=?",
            {std::to_string(tot), item_id});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Purchase id=" + id + " item=" + item_id + " total=" + std::to_string(tot));
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        out << "{\"id\":" << id << ",\"total_cost\":" << tot << "}";
        return HttpResponse{201, out.str()};
    });

    // PUT /api/purchases/:id
    r.add("PUT", "/api/purchases/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, "{\"error\":\"invalid JSON\"}"}; }
        Rows cur = Db::instance().query(
            "SELECT item_id,purchase_price,buyers_premium,transport_cost,"
            "COALESCE(restoration_budget,0),vendor_name,vendor_contact,"
            "acquisition_date,purchase_channel,payment_method,receipt_ref,notes "
            "FROM purchases WHERE id=?", {it->second});
        if (cur.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        double pp  = j.value("purchase_price",    std::stod(cur[0][1]));
        double bp  = j.value("buyers_premium",    std::stod(cur[0][2]));
        double tc  = j.value("transport_cost",    std::stod(cur[0][3]));
        double rb  = j.value("restoration_budget",std::stod(cur[0][4]));
        double tot = pp + bp + tc + rb;
        if (j.contains("total_cost") && j["total_cost"].get<double>() > 0)
            tot = j["total_cost"].get<double>();
        Db::instance().exec(
            "UPDATE purchases SET vendor_name=?,vendor_contact=?,acquisition_date=?,"
            "purchase_price=?,buyers_premium=?,transport_cost=?,restoration_budget=?,"
            "total_cost=?,purchase_channel=?,payment_method=?,receipt_ref=?,notes=? WHERE id=?",
            {j.value("vendor_name",    cur[0][5]),
             j.value("vendor_contact", cur[0][6]),
             j.value("acquisition_date", cur[0][7]),
             std::to_string(pp), std::to_string(bp),
             std::to_string(tc), std::to_string(rb), std::to_string(tot),
             j.value("purchase_channel", cur[0][8]),
             j.value("payment_method",   cur[0][9]),
             j.value("receipt_ref",      cur[0][10]),
             j.value("notes",            cur[0][11]),
             it->second});
        Db::instance().exec(
            "UPDATE items SET cost_price=?,updated_at=datetime('now') WHERE id=?",
            {std::to_string(tot), cur[0][0]});
        LOG_INFO("Purchase updated id=" + it->second);
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        out << "{\"ok\":true,\"total_cost\":" << tot << "}";
        return HttpResponse{200, out.str()};
    });

    // DELETE /api/purchases/:id  (admin only)
    r.add("DELETE", "/api/purchases/:id", [](const HttpRequest& req) {
        if (req.role != "admin") return HttpResponse{403, "{\"error\":\"admin only\"}"};
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Db::instance().exec("DELETE FROM purchases WHERE id=?", {it->second});
        LOG_INFO("Purchase deleted id=" + it->second);
        return HttpResponse{200, "{\"ok\":true}"};
    });

    // GET /api/purchases/summary
    r.add("GET", "/api/purchases/summary", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string f = ""; std::vector<std::string> params;
        if (!from.empty()) { f += " AND acquisition_date >= ?"; params.push_back(from); }
        if (!to.empty())   { f += " AND acquisition_date <= ?"; params.push_back(to);   }
        Rows r1 = Db::instance().query(
            "SELECT COUNT(*),COALESCE(SUM(total_cost),0),COALESCE(SUM(purchase_price),0),"
            "COALESCE(SUM(buyers_premium),0),COALESCE(SUM(transport_cost),0),"
            "COALESCE(SUM(COALESCE(restoration_budget,0)),0) FROM purchases WHERE 1=1" + f, params);
        Rows r2 = Db::instance().query(
            "SELECT COUNT(*),COALESCE(SUM(total_cost),0) FROM purchases "
            "WHERE acquisition_date >= date('now','start of month')", {});
        Rows r3 = Db::instance().query(
            "SELECT purchase_channel,COUNT(*),COALESCE(SUM(total_cost),0) FROM purchases "
            "WHERE 1=1" + f + " GROUP BY purchase_channel ORDER BY SUM(total_cost) DESC", params);
        std::string channels = "[";
        for (size_t i = 0; i < r3.size(); ++i) {
            if (i) channels += ',';
            channels += "{\"channel\":\"" + je(r3[i][0]) + "\",\"count\":" + r3[i][1] +
                        ",\"total\":" + r3[i][2] + "}";
        }
        std::string o = "{";
        o += "\"total_purchases\":"      + (r1.empty()?"0":r1[0][0]);
        o += ",\"total_cost\":"          + (r1.empty()?"0":r1[0][1]);
        o += ",\"total_purchase_price\":"+ (r1.empty()?"0":r1[0][2]);
        o += ",\"total_buyers_premium\":"+ (r1.empty()?"0":r1[0][3]);
        o += ",\"total_transport\":"     + (r1.empty()?"0":r1[0][4]);
        o += ",\"total_restoration\":"   + (r1.empty()?"0":r1[0][5]);
        o += ",\"month_purchases\":"     + (r2.empty()?"0":r2[0][0]);
        o += ",\"month_cost\":"          + (r2.empty()?"0":r2[0][1]);
        o += ",\"by_channel\":"          + channels + "}";
        return HttpResponse{200, o};
    });

    // ============================================================
    // EXPENSES
    // ============================================================

    // GET /api/expenses
    r.add("GET", "/api/expenses", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string cat  = util::query_param(req.query, "category");
        std::string sql  = "SELECT id,category,description,amount,expense_date,"
                           "payment_method,vendor,receipt_ref,notes,created_at "
                           "FROM expenses WHERE 1=1";
        std::vector<std::string> params;
        if (!from.empty()) { sql += " AND expense_date >= ?"; params.push_back(from); }
        if (!to.empty())   { sql += " AND expense_date <= ?"; params.push_back(to);   }
        if (!cat.empty())  { sql += " AND category = ?";      params.push_back(cat);  }
        sql += " ORDER BY expense_date DESC, id DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) { if (i) out += ','; out += ejson(rows[i]); }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/expenses
    r.add("POST", "/api/expenses", [](const HttpRequest& req) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, "{\"error\":\"invalid JSON\"}"}; }
        std::string cat  = j.value("category","");
        std::string desc = j.value("description","");
        double amt = j.value("amount", 0.0);
        if (cat.empty() || desc.empty() || amt <= 0)
            return HttpResponse{400, "{\"error\":\"category, description, amount required\"}"};
        Db::instance().exec(
            "INSERT INTO expenses(category,description,amount,expense_date,"
            "payment_method,vendor,receipt_ref,notes) VALUES(?,?,?,?,?,?,?,?)",
            {cat, desc, std::to_string(amt),
             j.value("expense_date", util::today_iso()),
             j.value("payment_method",""), j.value("vendor",""),
             j.value("receipt_ref",""),    j.value("notes","")});
        std::string id = Db::instance().last_insert_id();
        LOG_INFO("Expense id=" + id + " cat=" + cat + " amt=" + std::to_string(amt));
        return HttpResponse{201, "{\"id\":" + id + "}"};
    });

    // GET /api/expenses/:id
    r.add("GET", "/api/expenses/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Rows rows = Db::instance().query(
            "SELECT id,category,description,amount,expense_date,"
            "payment_method,vendor,receipt_ref,notes,created_at "
            "FROM expenses WHERE id=?", {it->second});
        if (rows.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        return HttpResponse{200, ejson(rows[0])};
    });

    // PUT /api/expenses/:id
    r.add("PUT", "/api/expenses/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        nlohmann::json j;
        try { j = nlohmann::json::parse(req.body); } catch (...) {
            return HttpResponse{400, "{\"error\":\"invalid JSON\"}"}; }
        Rows cur = Db::instance().query(
            "SELECT category,description,amount,expense_date,"
            "payment_method,vendor,receipt_ref,notes FROM expenses WHERE id=?", {it->second});
        if (cur.empty()) return HttpResponse{404, "{\"error\":\"not found\"}"};
        Db::instance().exec(
            "UPDATE expenses SET category=?,description=?,amount=?,expense_date=?,"
            "payment_method=?,vendor=?,receipt_ref=?,notes=? WHERE id=?",
            {j.value("category",    cur[0][0]),
             j.value("description", cur[0][1]),
             std::to_string(j.value("amount", std::stod(cur[0][2]))),
             j.value("expense_date",  cur[0][3]),
             j.value("payment_method",cur[0][4]),
             j.value("vendor",        cur[0][5]),
             j.value("receipt_ref",   cur[0][6]),
             j.value("notes",         cur[0][7]),
             it->second});
        return HttpResponse{200, "{\"ok\":true}"};
    });

    // DELETE /api/expenses/:id
    r.add("DELETE", "/api/expenses/:id", [](const HttpRequest& req) {
        auto it = req.params.find("id");
        if (it == req.params.end()) return HttpResponse{400, "{\"error\":\"missing id\"}"};
        Db::instance().exec("DELETE FROM expenses WHERE id=?", {it->second});
        return HttpResponse{200, "{\"ok\":true}"};
    });

    // GET /api/expenses/summary
    r.add("GET", "/api/expenses/summary", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string f = ""; std::vector<std::string> params;
        if (!from.empty()) { f += " AND expense_date >= ?"; params.push_back(from); }
        if (!to.empty())   { f += " AND expense_date <= ?"; params.push_back(to);   }
        Rows total = Db::instance().query(
            "SELECT COALESCE(SUM(amount),0),COUNT(*) FROM expenses WHERE 1=1" + f, params);
        Rows month = Db::instance().query(
            "SELECT COALESCE(SUM(amount),0) FROM expenses "
            "WHERE expense_date >= date('now','start of month')", {});
        Rows cats  = Db::instance().query(
            "SELECT category,COALESCE(SUM(amount),0),COUNT(*) FROM expenses "
            "WHERE 1=1" + f + " GROUP BY category ORDER BY SUM(amount) DESC", params);
        std::string cj = "[";
        for (size_t i = 0; i < cats.size(); ++i) {
            if (i) cj += ',';
            cj += "{\"category\":\"" + je(cats[i][0]) + "\",\"total\":" +
                  cats[i][1] + ",\"count\":" + cats[i][2] + "}";
        }
        std::string o = "{";
        o += "\"total_expenses\":"  + (total.empty()?"0":total[0][0]);
        o += ",\"expense_count\":"  + (total.empty()?"0":total[0][1]);
        o += ",\"month_expenses\":" + (month.empty()?"0":month[0][0]);
        o += ",\"by_category\":"    + cj + "}";
        return HttpResponse{200, o};
    });

    // ============================================================
    // P&L REPORT
    // ============================================================
    r.add("GET", "/api/reports/pnl", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::vector<std::string> sp, pp, ep;
        std::string sf="", pf="", ef="";
        if (!from.empty()) {
            sf += " AND s.sale_date >= ?";       sp.push_back(from);
            pf += " AND p.acquisition_date >= ?"; pp.push_back(from);
            ef += " AND expense_date >= ?";       ep.push_back(from);
        }
        if (!to.empty()) {
            sf += " AND s.sale_date <= ?";       sp.push_back(to);
            pf += " AND p.acquisition_date <= ?"; pp.push_back(to);
            ef += " AND expense_date <= ?";       ep.push_back(to);
        }

        Rows rev = Db::instance().query(
            "SELECT COALESCE(SUM(sale_price),0),COUNT(*),"
            "COALESCE(SUM(net_proceeds),COALESCE(SUM(sale_price),0)),"
            "COALESCE(SUM(platform_fee),0),COALESCE(SUM(shipping_cost),0) "
            "FROM sales s WHERE 1=1" + sf, sp);

        Rows cogs = Db::instance().query(
            "SELECT COALESCE(SUM(COALESCE(p.total_cost,i.cost_price)),0),COUNT(s.id) "
            "FROM sales s JOIN items i ON i.id=s.item_id "
            "LEFT JOIN purchases p ON p.item_id=s.item_id WHERE 1=1" + sf, sp);

        Rows exp = Db::instance().query(
            "SELECT COALESCE(SUM(amount),0),COUNT(*) FROM expenses WHERE 1=1" + ef, ep);

        Rows purch = Db::instance().query(
            "SELECT COALESCE(SUM(total_cost),0),COUNT(*) FROM purchases p WHERE 1=1" + pf, pp);

        Rows by_cat = Db::instance().query(
            "SELECT i.category,COUNT(s.id),"
            "COALESCE(SUM(s.net_proceeds),COALESCE(SUM(s.sale_price),0)),"
            "COALESCE(SUM(COALESCE(p.total_cost,i.cost_price)),0),"
            "COALESCE(SUM(s.net_proceeds),COALESCE(SUM(s.sale_price),0))-"
            "COALESCE(SUM(COALESCE(p.total_cost,i.cost_price)),0) "
            "FROM sales s JOIN items i ON i.id=s.item_id "
            "LEFT JOIN purchases p ON p.item_id=s.item_id WHERE 1=1" + sf +
            " GROUP BY i.category ORDER BY 5 DESC", sp);

        Rows ecats = Db::instance().query(
            "SELECT category,COALESCE(SUM(amount),0) FROM expenses "
            "WHERE 1=1" + ef + " GROUP BY category ORDER BY SUM(amount) DESC", ep);

        double gross_rev = rev.empty() ? 0 : std::stod(rev[0][0]);
        double net_rev   = rev.empty() ? 0 : std::stod(rev[0][2]);
        double fees      = rev.empty() ? 0 : std::stod(rev[0][3]);
        double shipping  = rev.empty() ? 0 : std::stod(rev[0][4]);
        int    sold      = rev.empty() ? 0 : std::stoi(rev[0][1]);
        double cogs_v    = cogs.empty()  ? 0 : std::stod(cogs[0][0]);
        double exp_v     = exp.empty()   ? 0 : std::stod(exp[0][0]);
        double purch_v   = purch.empty() ? 0 : std::stod(purch[0][0]);
        int    purch_n   = purch.empty() ? 0 : std::stoi(purch[0][1]);
        double gp        = net_rev - cogs_v;
        double np        = gp - exp_v;
        double margin    = net_rev > 0 ? gp / net_rev * 100.0 : 0.0;

        std::string ej = "[";
        for (size_t i = 0; i < ecats.size(); ++i) {
            if (i) ej += ',';
            ej += "{\"category\":\"" + je(ecats[i][0]) + "\",\"amount\":" + ecats[i][1] + "}";
        }
        ej += "]";

        std::string cj = "[";
        for (size_t i = 0; i < by_cat.size(); ++i) {
            if (i) cj += ',';
            double rv2 = by_cat[i][2].empty() ? 0 : std::stod(by_cat[i][2]);
            double gp2 = by_cat[i][4].empty() ? 0 : std::stod(by_cat[i][4]);
            int    mp  = rv2 > 0 ? static_cast<int>(gp2 / rv2 * 100.0) : 0;
            cj += "{\"category\":\"" + je(by_cat[i][0]) + "\",\"units\":"  + by_cat[i][1] +
                  ",\"revenue\":"    + by_cat[i][2] + ",\"cogs\":"          + by_cat[i][3] +
                  ",\"gross_profit\":"+ by_cat[i][4] + ",\"margin_pct\":"   + std::to_string(mp) + "}";
        }
        cj += "]";

        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        out << "{\"gross_revenue\":"        << gross_rev
            << ",\"net_revenue\":"          << net_rev
            << ",\"platform_fees\":"        << fees
            << ",\"shipping_out\":"         << shipping
            << ",\"items_sold\":"           << sold
            << ",\"cogs\":"                 << cogs_v
            << ",\"gross_profit\":"         << gp
            << ",\"overhead_expenses\":"    << exp_v
            << ",\"net_profit\":"           << np
            << ",\"margin_pct\":"           << margin
            << ",\"purchases_spent\":"      << purch_v
            << ",\"items_purchased\":"      << purch_n
            << ",\"expenses_by_category\":" << ej
            << ",\"by_category\":"          << cj
            << "}";
        return HttpResponse{200, out.str()};
    });
}

} // namespace metis::antique
