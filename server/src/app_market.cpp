#include "app_market.hpp"
#include "db.hpp"
#include "util.hpp"

namespace metis::antique {

void register_market_routes(Router& r) {

    r.add("GET", "/api/market/categories", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string filter = ""; std::vector<std::string> params;
        if (!from.empty()) { filter += " AND s.sale_date >= ?"; params.push_back(from); }
        if (!to.empty())   { filter += " AND s.sale_date <= ?"; params.push_back(to); }
        Rows rows = Db::instance().query(R"(
SELECT i.category,COUNT(s.id),COALESCE(SUM(s.sale_price),0),
       COALESCE(SUM(s.net_proceeds),COALESCE(SUM(s.sale_price),0)),
       COALESCE(AVG(s.sale_price - i.cost_price),0)
FROM items i LEFT JOIN sales s ON s.item_id=i.id AND 1=1)" + filter + R"(
GROUP BY i.category ORDER BY SUM(s.sale_price) DESC NULLS LAST)", params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"category":")" + util::json_escape(r[0]) + "\"" +
                   R"(,"sold_count":)" + r[1] +
                   R"(,"revenue":)" + r[2] +
                   R"(,"net_revenue":)" + r[3] +
                   R"(,"avg_profit":)" + r[4] + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    r.add("GET", "/api/market/trend", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string filter = ""; std::vector<std::string> params;
        if (!from.empty()) { filter += " AND sale_date >= ?"; params.push_back(from); }
        if (!to.empty())   { filter += " AND sale_date <= ?"; params.push_back(to); }
        params.push_back("12");
        Rows rows = Db::instance().query(R"(
SELECT strftime('%Y-%m',sale_date) AS month,COUNT(*),
       COALESCE(SUM(sale_price),0),
       COALESCE(SUM(net_proceeds),COALESCE(SUM(sale_price),0))
FROM sales WHERE 1=1)" + filter + R"(
GROUP BY month ORDER BY month DESC LIMIT ?)", params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"month":")" + r[0] + "\"" +
                   R"(,"count":)" + r[1] +
                   R"(,"revenue":)" + r[2] +
                   R"(,"net_revenue":)" + r[3] + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    r.add("GET", "/api/market/top", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string filter = ""; std::vector<std::string> params;
        if (!from.empty()) { filter += " AND s.sale_date >= ?"; params.push_back(from); }
        if (!to.empty())   { filter += " AND s.sale_date <= ?"; params.push_back(to); }
        Rows rows = Db::instance().query(R"(
SELECT i.title,i.category,s.sale_price,i.cost_price,
       (s.sale_price-i.cost_price) AS profit,
       COALESCE(s.net_proceeds,s.sale_price),s.sale_date
FROM sales s JOIN items i ON i.id=s.item_id WHERE 1=1)" + filter + R"(
ORDER BY profit DESC LIMIT 10)", params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"title":")" + util::json_escape(r[0]) + "\"" +
                   R"(,"category":")" + util::json_escape(r[1]) + "\"" +
                   R"(,"sale_price":)" + r[2] +
                   R"(,"cost_price":)" + r[3] +
                   R"(,"profit":)" + r[4] +
                   R"(,"net_proceeds":)" + r[5] +
                   R"(,"sale_date":")" + r[6] + "\"}";
        }
        return HttpResponse{200, out + "]"};
    });

    // Portfolio valuation report
    r.add("GET", "/api/reports/portfolio", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        // Portfolio is inventory value -- date filter applies to acquisition date
        std::string sql = R"(
SELECT category,COUNT(*),COALESCE(SUM(asking_price),0),
       COALESCE(SUM(cost_price),0),
       COALESCE(SUM(asking_price)-SUM(cost_price),0)
FROM items WHERE status='inventory')";
        std::vector<std::string> params;
        if (!from.empty()) { sql += " AND created_at >= ?"; params.push_back(from); }
        if (!to.empty())   { sql += " AND created_at <= ?"; params.push_back(to); }
        sql += " GROUP BY category ORDER BY SUM(asking_price) DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"category":")" + util::json_escape(r[0]) + "\"" +
                   R"(,"count":)" + r[1] +
                   R"(,"asking_value":)" + r[2] +
                   R"(,"cost_value":)" + r[3] +
                   R"(,"unrealized_gain":)" + r[4] + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    // ROI by acquisition source
    r.add("GET", "/api/reports/roi_by_source", [](const HttpRequest& req) {
        std::string from = util::query_param(req.query, "from");
        std::string to   = util::query_param(req.query, "to");
        std::string sql = R"(
SELECT COALESCE(i.source,'Unknown'),COUNT(s.id),
       COALESCE(AVG(CASE WHEN i.cost_price>0
           THEN (s.sale_price-i.cost_price)/i.cost_price*100 END),0),
       COALESCE(SUM(s.sale_price),0),COALESCE(SUM(i.cost_price),0)
FROM items i LEFT JOIN sales s ON s.item_id=i.id
WHERE s.id IS NOT NULL)";
        std::vector<std::string> params;
        if (!from.empty()) { sql += " AND s.sale_date >= ?"; params.push_back(from); }
        if (!to.empty())   { sql += " AND s.sale_date <= ?"; params.push_back(to); }
        sql += " GROUP BY COALESCE(i.source,'Unknown') ORDER BY SUM(s.sale_price) DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ',';
            const auto& r = rows[i];
            out += R"({"source":")" + util::json_escape(r[0]) + "\"" +
                   R"(,"sold_count":)" + r[1] +
                   R"(,"avg_roi_pct":)" + r[2] +
                   R"(,"total_revenue":)" + r[3] +
                   R"(,"total_cost":)" + r[4] + "}";
        }
        return HttpResponse{200, out + "]"};
    });

    // Health check
    r.add("GET", "/api/health", [](const HttpRequest&) {
        Rows items = Db::instance().query("SELECT COUNT(*) FROM items");
        Rows users = Db::instance().query("SELECT COUNT(*) FROM users");
        Rows sales = Db::instance().query("SELECT COUNT(*) FROM sales");
        return HttpResponse{200,
            R"({"status":"ok","items":)" + (items.empty()?"0":items[0][0]) +
            R"(,"users":)" + (users.empty()?"0":users[0][0]) +
            R"(,"sales":)" + (sales.empty()?"0":sales[0][0]) + "}"};
    });
}

} // namespace metis::antique
