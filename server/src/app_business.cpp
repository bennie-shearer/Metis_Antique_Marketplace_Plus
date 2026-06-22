#include "app_business.hpp"
#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "json.hpp"
#include <string>
#include <sstream>

namespace metis::antique {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string je(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else o += c;
    }
    return o;
}

static std::string rows_to_json(const Rows& rows, const std::vector<std::string>& cols) {
    std::string out = "[";
    bool first = true;
    for (const auto& row : rows) {
        if (!first) out += ",";
        first = false;
        out += "{";
        for (size_t i = 0; i < cols.size() && i < row.size(); ++i) {
            if (i > 0) out += ",";
            out += "\"" + cols[i] + "\":\"" + je(row[i]) + "\"";
        }
        out += "}";
    }
    out += "]";
    return out;
}

// ---------------------------------------------------------------------------
// Consignments
// ---------------------------------------------------------------------------
static void register_consignment_routes(Router& r) {
    const std::vector<std::string> COLS = {
        "id","item_id","consignor_name","consignor_email","consignor_phone",
        "description","agreed_pct","agreed_floor","received_date","sold_date",
        "sale_price","payout_amount","payout_date","status","notes","created_at"
    };

    // List
    r.add("GET", "/api/consignments", [&COLS](const HttpRequest& req) {
        std::string status = util::query_param(req.query, "status");
        std::string sql = "SELECT c.id,c.item_id,c.consignor_name,c.consignor_email,"
            "c.consignor_phone,c.description,c.agreed_pct,c.agreed_floor,"
            "c.received_date,c.sold_date,c.sale_price,c.payout_amount,"
            "c.payout_date,c.status,c.notes,c.created_at "
            "FROM consignments c";
        if (!status.empty()) sql += " WHERE c.status='" + je(status) + "'";
        sql += " ORDER BY c.created_at DESC";
        Rows rows = Db::instance().query(sql);
        return HttpResponse{200, rows_to_json(rows, COLS)};
    });

    // Summary
    r.add("GET", "/api/consignments/summary", [](const HttpRequest&) {
        Rows active  = Db::instance().query("SELECT COUNT(*) FROM consignments WHERE status='active'");
        Rows sold    = Db::instance().query("SELECT COUNT(*) FROM consignments WHERE status='sold'");
        Rows unpaid  = Db::instance().query("SELECT COUNT(*) FROM consignments WHERE status='sold' AND payout_date IS NULL");
        Rows owing   = Db::instance().query("SELECT COALESCE(SUM(payout_amount),0) FROM consignments WHERE status='sold' AND payout_date IS NULL");
        return HttpResponse{200,
            "{\"active\":" + (active.empty() ? "0" : active[0][0]) +
            ",\"sold\":"   + (sold.empty()   ? "0" : sold[0][0]) +
            ",\"unpaid\":" + (unpaid.empty() ? "0" : unpaid[0][0]) +
            ",\"owing\":\""  + (owing.empty()  ? "0" : owing[0][0]) + "\"}"};
    });

    // Get one
    r.add("GET", "/api/consignments/:id", [&COLS](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Rows rows = Db::instance().query(
            "SELECT id,item_id,consignor_name,consignor_email,consignor_phone,"
            "description,agreed_pct,agreed_floor,received_date,sold_date,"
            "sale_price,payout_amount,payout_date,status,notes,created_at "
            "FROM consignments WHERE id=?", {id});
        if (rows.empty()) return HttpResponse{404, R"({"error":"not found"})"};
        return HttpResponse{200, rows_to_json(rows, COLS)};
    });

    // Create
    r.add("POST", "/api/consignments", [](const HttpRequest& req) {
        json j;
        try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"};
        }
        std::string name = j.value("consignor_name", "");
        if (name.empty()) return HttpResponse{400, R"({"error":"consignor_name required"})"};
        Rows r2 = Db::instance().query(
            "INSERT INTO consignments(item_id,consignor_name,consignor_email,"
            "consignor_phone,description,agreed_pct,agreed_floor,received_date,status,notes) "
            "VALUES(?,?,?,?,?,?,?,?,?,?) RETURNING id",
            {j.value("item_id",""),
             name,
             j.value("consignor_email",""),
             j.value("consignor_phone",""),
             j.value("description",""),
             j.value("agreed_pct","0"),
             j.value("agreed_floor","0"),
             j.value("received_date",""),
             j.value("status","active"),
             j.value("notes","")});
        std::string new_id = (r2.empty() ? "" : r2[0][0]);
        return HttpResponse{201, "{\"id\":" + new_id + "}"};
    });

    // Update
    r.add("PUT", "/api/consignments/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        json j;
        try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"};
        }
        // Auto-calculate payout if sold and pct set
        if (j.contains("sale_price") && j.contains("agreed_pct")) {
            double sp  = std::stod(j.value("sale_price","0"));
            double pct = std::stod(j.value("agreed_pct","0"));
            double fl  = std::stod(j.value("agreed_floor","0"));
            double pay = sp * (pct / 100.0);
            if (pay < fl) pay = fl;
            j["payout_amount"] = std::to_string(pay);
        }
        Db::instance().exec(
            "UPDATE consignments SET consignor_name=?,consignor_email=?,"
            "consignor_phone=?,description=?,agreed_pct=?,agreed_floor=?,"
            "received_date=?,sold_date=?,sale_price=?,payout_amount=?,"
            "payout_date=?,status=?,notes=? WHERE id=?",
            {j.value("consignor_name",""),
             j.value("consignor_email",""),
             j.value("consignor_phone",""),
             j.value("description",""),
             j.value("agreed_pct","0"),
             j.value("agreed_floor","0"),
             j.value("received_date",""),
             j.value("sold_date",""),
             j.value("sale_price",""),
             j.value("payout_amount",""),
             j.value("payout_date",""),
             j.value("status","active"),
             j.value("notes",""),
             id});
        return HttpResponse{200, R"({"ok":true})"};
    });

    // Delete
    r.add("DELETE", "/api/consignments/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Db::instance().exec("DELETE FROM consignments WHERE id=?", {id});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

// ---------------------------------------------------------------------------
// Rentals
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Rentals -- with automated collection, discount rules, and invoicing
// ---------------------------------------------------------------------------
static void register_rental_routes(Router& r, const Pson& cfg) {
    const std::vector<std::string> RCOLS = {
        "id","rental_type","location","vendor","vendor_user_id","monthly_rate",
        "discount_pct","discount_label","start_date","end_date",
        "payment_method","auto_renew","due_day","grace_days","late_fee_pct",
        "notes","created_at"
    };
    const std::vector<std::string> PCOLS = {
        "id","rental_id","period","due_date","base_amount","discount_pct",
        "discount_amount","late_fee","amount","paid_date","status",
        "receipt_ref","notes","created_at"
    };

    // ── Discount calculation ────────────────────────────────────────────
    // Returns discount_pct, label, final_amount
    auto calc_discount = [&cfg](double rate, double per_rental_pct,
                                const std::string& rental_id,
                                const std::string& vendor_uid,
                                int paid_day) -> std::tuple<double,std::string,double> {
        double dp = per_rental_pct;
        std::string lbl = per_rental_pct > 0 ? "Custom" : "";
        if (dp == 0) {
            int early_day = cfg.get_int("booth_rent", "early_pay_before_day", 3);
            double early_pct = cfg.get_double("booth_rent", "early_pay_discount_pct", 5.0);
            if (paid_day > 0 && paid_day < early_day && early_pct > 0)
                { dp = early_pct; lbl = "Early payment"; }
        }
        if (dp == 0 && !rental_id.empty()) {
            int lm = cfg.get_int("booth_rent","loyalty_months",12);
            double lp = cfg.get_double("booth_rent","loyalty_discount_pct",10.0);
            if (lp > 0) {
                Rows pr = Db::instance().query(
                    "SELECT COUNT(*) FROM rental_payments WHERE rental_id=? AND status='paid'",{rental_id});
                if (!pr.empty() && std::stoi(pr[0][0]) >= lm)
                    { dp = lp; lbl = "Loyalty"; }
            }
        }
        if (dp == 0 && !vendor_uid.empty()) {
            int mc = cfg.get_int("booth_rent","multi_booth_count",3);
            double mp = cfg.get_double("booth_rent","multi_booth_discount_pct",7.5);
            if (mp > 0) {
                Rows bk = Db::instance().query(
                    "SELECT COUNT(*) FROM rentals WHERE vendor_user_id=? AND (end_date IS NULL OR end_date='')",
                    {vendor_uid});
                if (!bk.empty() && std::stoi(bk[0][0]) >= mc)
                    { dp = mp; lbl = "Multi-booth"; }
            }
        }
        double da = rate * (dp / 100.0);
        return {dp, lbl, rate - da};
    };

    r.add("GET", "/api/rentals", [&RCOLS](const HttpRequest&) {
        Rows rows = Db::instance().query(
            "SELECT id,rental_type,location,vendor,"
            "COALESCE(vendor_user_id,''),monthly_rate,"
            "COALESCE(discount_pct,0),COALESCE(discount_label,''),"
            "start_date,COALESCE(end_date,''),payment_method,auto_renew,"
            "COALESCE(due_day,1),COALESCE(grace_days,5),"
            "COALESCE(late_fee_pct,0),notes,created_at "
            "FROM rentals ORDER BY start_date DESC");
        return HttpResponse{200, rows_to_json(rows, RCOLS)};
    });

    r.add("GET", "/api/rentals/summary", [](const HttpRequest&) {
        Rows a = Db::instance().query("SELECT COUNT(*),COALESCE(SUM(monthly_rate),0) FROM rentals WHERE end_date IS NULL OR end_date=''");
        Rows y = Db::instance().query("SELECT COALESCE(SUM(amount),0) FROM rental_payments WHERE strftime('%Y',COALESCE(paid_date,created_at))=strftime('%Y','now') AND status='paid'");
        Rows p = Db::instance().query("SELECT COUNT(*),COALESCE(SUM(amount),0) FROM rental_payments WHERE status='pending'");
        Rows ov= Db::instance().query("SELECT COUNT(*),COALESCE(SUM(amount),0) FROM rental_payments WHERE status='overdue'");
        return HttpResponse{200,
            "{\"active_count\":" + (a.empty()?"0":a[0][0]) +
            ",\"monthly_total\":\"" + (a.empty()?"0":a[0][1]) + "\"" +
            ",\"ytd_paid\":\"" + (y.empty()?"0":y[0][0]) + "\"" +
            ",\"pending_count\":" + (p.empty()?"0":p[0][0]) +
            ",\"pending_total\":\"" + (p.empty()?"0":p[0][1]) + "\"" +
            ",\"overdue_count\":" + (ov.empty()?"0":ov[0][0]) +
            ",\"overdue_total\":\"" + (ov.empty()?"0":ov[0][1]) + "\"}"};
    });

    r.add("POST", "/api/rentals", [&cfg](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) { return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        Rows r2 = Db::instance().query(
            "INSERT INTO rentals(rental_type,location,vendor,vendor_user_id,"
            "monthly_rate,discount_pct,discount_label,"
            "start_date,end_date,payment_method,auto_renew,due_day,grace_days,late_fee_pct,notes) "
            "VALUES(?,?,?,NULLIF(?,''),?,?,?,?,?,?,?,?,?,?,?) RETURNING id",
            {j.value("rental_type","booth"),j.value("location",""),j.value("vendor",""),
             j.value("vendor_user_id",""),j.value("monthly_rate","0"),
             j.value("discount_pct","0"),j.value("discount_label",""),
             j.value("start_date",""),j.value("end_date",""),j.value("payment_method",""),
             j.value("auto_renew","1"),
             j.value("due_day",std::to_string(cfg.get_int("booth_rent","due_day",1))),
             j.value("grace_days",std::to_string(cfg.get_int("booth_rent","grace_days",5))),
             j.value("late_fee_pct",std::to_string(cfg.get_double("booth_rent","late_fee_pct",0.0))),
             j.value("notes","")});
        return HttpResponse{201, "{\"id\":" + (r2.empty()?"0":r2[0][0]) + "}"};
    });

    r.add("PUT", "/api/rentals/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        json j; try { j = json::parse(req.body); } catch (...) { return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        Db::instance().exec(
            "UPDATE rentals SET rental_type=?,location=?,vendor=?,"
            "vendor_user_id=NULLIF(?,''),monthly_rate=?,"
            "discount_pct=?,discount_label=?,start_date=?,end_date=?,"
            "payment_method=?,auto_renew=?,due_day=?,grace_days=?,late_fee_pct=?,notes=? WHERE id=?",
            {j.value("rental_type","booth"),j.value("location",""),j.value("vendor",""),
             j.value("vendor_user_id",""),j.value("monthly_rate","0"),
             j.value("discount_pct","0"),j.value("discount_label",""),
             j.value("start_date",""),j.value("end_date",""),j.value("payment_method",""),
             j.value("auto_renew","1"),j.value("due_day","1"),
             j.value("grace_days","5"),j.value("late_fee_pct","0"),j.value("notes",""),id});
        return HttpResponse{200, R"({"ok":true})"};
    });

    r.add("DELETE", "/api/rentals/:id", [](const HttpRequest& req) {
        Db::instance().exec("DELETE FROM rentals WHERE id=?",
            {req.params.count("id") ? req.params.at("id") : ""});
        return HttpResponse{200, R"({"ok":true})"};
    });

    r.add("GET", "/api/rentals/:id/payments", [&PCOLS](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Rows rows = Db::instance().query(
            "SELECT id,rental_id,period,COALESCE(due_date,''),"
            "COALESCE(base_amount,amount),COALESCE(discount_pct,0),"
            "COALESCE(discount_amount,0),COALESCE(late_fee,0),"
            "amount,COALESCE(paid_date,''),COALESCE(status,'pending'),"
            "receipt_ref,notes,created_at "
            "FROM rental_payments WHERE rental_id=? ORDER BY period DESC", {id});
        return HttpResponse{200, rows_to_json(rows, PCOLS)};
    });

    r.add("POST", "/api/rentals/:id/payments", [&cfg, calc_discount](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        json j; try { j = json::parse(req.body); } catch (...) { return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        Rows rental = Db::instance().query(
            "SELECT monthly_rate,COALESCE(discount_pct,0),COALESCE(vendor_user_id,''),COALESCE(late_fee_pct,0) FROM rentals WHERE id=?",{id});
        if (rental.empty()) return HttpResponse{404, R"({"error":"rental not found"})"};
        double rate = std::stod(rental[0][0]);
        double per_pct = std::stod(rental[0][1]);
        std::string vuid = rental[0][2];
        double lf_pct = std::stod(rental[0][3]);
        int paid_day = j.value("paid_day", 0);
        auto [dp, dlbl, net] = calc_discount(rate, per_pct, id, vuid, paid_day);
        double da = rate * (dp / 100.0);
        double lf = (j.value("apply_late_fee", false)) ? rate * (lf_pct / 100.0) : 0.0;
        double total = j.contains("amount") ? std::stod(j.value("amount","0")) : (net + lf);
        Rows r2 = Db::instance().query(
            "INSERT INTO rental_payments(rental_id,period,due_date,base_amount,discount_pct,"
            "discount_amount,late_fee,amount,paid_date,status,receipt_ref,notes) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?) RETURNING id",
            {id,j.value("period",""),j.value("due_date",""),std::to_string(rate),
             std::to_string(dp),std::to_string(da),std::to_string(lf),
             std::to_string(total),j.value("paid_date",""),j.value("status","paid"),
             j.value("receipt_ref",""),j.value("notes","")});
        return HttpResponse{201,
            "{\"id\":" + (r2.empty()?"0":r2[0][0]) +
            ",\"base\":" + std::to_string(rate) +
            ",\"discount_pct\":" + std::to_string(dp) +
            ",\"discount_label\":\"" + je(dlbl) + "\"" +
            ",\"discount_amount\":" + std::to_string(da) +
            ",\"late_fee\":" + std::to_string(lf) +
            ",\"total_due\":" + std::to_string(total) + "}"};
    });

    r.add("POST", "/api/rentals/:id/invoice", [&cfg, calc_discount](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        json j; try { j = json::parse(req.body); } catch (...) { j = json::object(); }
        Rows rental = Db::instance().query(
            "SELECT monthly_rate,COALESCE(discount_pct,0),COALESCE(vendor_user_id,''),"
            "COALESCE(late_fee_pct,0),vendor,location FROM rentals WHERE id=?",{id});
        if (rental.empty()) return HttpResponse{404, R"({"error":"rental not found"})"};
        double rate = std::stod(rental[0][0]);
        auto [dp, dlbl, net] = calc_discount(rate, std::stod(rental[0][1]), id, rental[0][2], 0);
        double da = rate * (dp / 100.0);
        double lf = j.value("apply_late_fee",false) ? rate * (std::stod(rental[0][3]) / 100.0) : 0.0;
        double total = net + lf;
        std::string period = j.value("period","");
        Rows inv = Db::instance().query(
            "INSERT INTO booth_invoices(rental_id,period,amount_due,discount_amt,late_fee,total_due,status,sent_at) "
            "VALUES(?,?,?,?,?,?,'sent',datetime('now')) RETURNING id",
            {id,period,std::to_string(rate),std::to_string(da),std::to_string(lf),std::to_string(total)});
        return HttpResponse{200,
            "{\"invoice_id\":" + (inv.empty()?"0":inv[0][0]) +
            ",\"vendor\":\"" + je(rental[0][4]) + "\"" +
            ",\"period\":\"" + je(period) + "\"" +
            ",\"base\":" + std::to_string(rate) +
            ",\"discount_pct\":" + std::to_string(dp) +
            ",\"discount_label\":\"" + je(dlbl) + "\"" +
            ",\"discount_amount\":" + std::to_string(da) +
            ",\"late_fee\":" + std::to_string(lf) +
            ",\"total_due\":" + std::to_string(total) + "}"};
    });

    r.add("GET", "/api/booth-invoices", [](const HttpRequest&) {
        const std::vector<std::string> IC = {"id","rental_id","vendor","location","period",
            "amount_due","discount_amt","late_fee","total_due","status","sent_at","paid_at"};
        Rows rows = Db::instance().query(
            "SELECT i.id,i.rental_id,r.vendor,r.location,i.period,"
            "i.amount_due,i.discount_amt,i.late_fee,i.total_due,"
            "i.status,i.sent_at,COALESCE(i.paid_at,'') "
            "FROM booth_invoices i JOIN rentals r ON r.id=i.rental_id ORDER BY i.created_at DESC");
        return HttpResponse{200, rows_to_json(rows, IC)};
    });

    r.add("PUT", "/api/booth-invoices/:id/paid", [](const HttpRequest& req) {
        Db::instance().exec("UPDATE booth_invoices SET status='paid',paid_at=datetime('now') WHERE id=?",
            {req.params.count("id") ? req.params.at("id") : ""});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

static void register_advertising_routes(Router& r) {
    const std::vector<std::string> COLS = {
        "id","platform","campaign","ad_type","start_date","end_date",
        "budget","spent","impressions","clicks","conversions","status","notes","created_at"
    };

    r.add("GET", "/api/advertising", [&COLS](const HttpRequest& req) {
        std::string status = util::query_param(req.query, "status");
        std::string sql = "SELECT id,platform,campaign,ad_type,start_date,end_date,"
            "budget,spent,impressions,clicks,conversions,status,notes,created_at "
            "FROM advertising";
        if (!status.empty()) sql += " WHERE status='" + je(status) + "'";
        sql += " ORDER BY start_date DESC";
        Rows rows = Db::instance().query(sql);
        return HttpResponse{200, rows_to_json(rows, COLS)};
    });

    r.add("GET", "/api/advertising/summary", [](const HttpRequest&) {
        Rows active = Db::instance().query("SELECT COUNT(*),COALESCE(SUM(budget),0),COALESCE(SUM(spent),0) FROM advertising WHERE status='active'");
        Rows ytd    = Db::instance().query("SELECT COALESCE(SUM(spent),0),COALESCE(SUM(impressions),0),COALESCE(SUM(clicks),0),COALESCE(SUM(conversions),0) FROM advertising WHERE strftime('%Y',start_date)=strftime('%Y','now')");
        return HttpResponse{200,
            "{\"active_campaigns\":" + (active.empty() ? "0" : active[0][0]) +
            ",\"active_budget\":\"" + (active.empty() ? "0" : active[0][1]) + "\""
            ",\"active_spent\":\"" + (active.empty() ? "0" : active[0][2]) + "\""
            ",\"ytd_spent\":\"" + (ytd.empty() ? "0" : ytd[0][0]) + "\""
            ",\"ytd_impressions\":" + (ytd.empty() ? "0" : ytd[0][1]) +
            ",\"ytd_clicks\":" + (ytd.empty() ? "0" : ytd[0][2]) +
            ",\"ytd_conversions\":" + (ytd.empty() ? "0" : ytd[0][3]) + "}"};
    });

    r.add("POST", "/api/advertising", [](const HttpRequest& req) {
        json j;
        try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"};
        }
        if (j.value("platform","").empty())
            return HttpResponse{400, R"({"error":"platform required"})"};
        Rows r2 = Db::instance().query(
            "INSERT INTO advertising(platform,campaign,ad_type,start_date,end_date,"
            "budget,spent,impressions,clicks,conversions,status,notes) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?) RETURNING id",
            {j.value("platform",""),
             j.value("campaign",""),
             j.value("ad_type","listing"),
             j.value("start_date",""),
             j.value("end_date",""),
             j.value("budget","0"),
             j.value("spent","0"),
             j.value("impressions","0"),
             j.value("clicks","0"),
             j.value("conversions","0"),
             j.value("status","active"),
             j.value("notes","")});
        std::string new_id = (r2.empty() ? "" : r2[0][0]);
        return HttpResponse{201, "{\"id\":" + new_id + "}"};
    });

    r.add("PUT", "/api/advertising/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        json j;
        try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"};
        }
        Db::instance().exec(
            "UPDATE advertising SET platform=?,campaign=?,ad_type=?,start_date=?,"
            "end_date=?,budget=?,spent=?,impressions=?,clicks=?,conversions=?,status=?,notes=? "
            "WHERE id=?",
            {j.value("platform",""),
             j.value("campaign",""),
             j.value("ad_type","listing"),
             j.value("start_date",""),
             j.value("end_date",""),
             j.value("budget","0"),
             j.value("spent","0"),
             j.value("impressions","0"),
             j.value("clicks","0"),
             j.value("conversions","0"),
             j.value("status","active"),
             j.value("notes",""),
             id});
        return HttpResponse{200, R"({"ok":true})"};
    });

    r.add("DELETE", "/api/advertising/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Db::instance().exec("DELETE FROM advertising WHERE id=?", {id});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

// ---------------------------------------------------------------------------
// Credit card fees
// ---------------------------------------------------------------------------
static void register_cc_fee_routes(Router& r, const Pson& cfg) {
    const std::vector<std::string> COLS = {
        "id","sale_id","processor","fee_pct","fee_flat","fee_amount",
        "transaction_ref","fee_date","notes","created_at"
    };

    r.add("GET", "/api/cc-fees", [&COLS](const HttpRequest& req) {
        std::string sale_id = util::query_param(req.query, "sale_id");
        std::string sql = "SELECT id,sale_id,processor,fee_pct,fee_flat,fee_amount,"
            "transaction_ref,fee_date,notes,created_at FROM cc_fees";
        if (!sale_id.empty()) sql += " WHERE sale_id=" + sale_id;
        sql += " ORDER BY fee_date DESC";
        Rows rows = Db::instance().query(sql);
        return HttpResponse{200, rows_to_json(rows, COLS)};
    });

    r.add("GET", "/api/cc-fees/summary", [](const HttpRequest&) {
        Rows month = Db::instance().query(
            "SELECT COALESCE(SUM(fee_amount),0) FROM cc_fees "
            "WHERE strftime('%Y-%m',fee_date)=strftime('%Y-%m','now')");
        Rows ytd = Db::instance().query(
            "SELECT COALESCE(SUM(fee_amount),0) FROM cc_fees "
            "WHERE strftime('%Y',fee_date)=strftime('%Y','now')");
        Rows total = Db::instance().query("SELECT COALESCE(SUM(fee_amount),0) FROM cc_fees");
        return HttpResponse{200,
            "{\"month_fees\":\"" + (month.empty() ? "0" : month[0][0]) + "\""
            ",\"ytd_fees\":\"" + (ytd.empty() ? "0" : ytd[0][0]) + "\""
            ",\"total_fees\":\"" + (total.empty() ? "0" : total[0][0]) + "\"}"};
    });

    // Calculate fee for a sale amount
    r.add("GET", "/api/cc-fees/calculate", [&cfg](const HttpRequest& req) {
        std::string amt_s  = util::query_param(req.query, "amount");
        std::string proc_s = util::query_param(req.query, "processor");
        double amount = amt_s.empty() ? 0.0 : std::stod(amt_s);
        // Default rates from config.pson, fallback to common rates
        double pct  = cfg.get_double("cc_fees", proc_s + "_pct",  2.9);
        double flat = cfg.get_double("cc_fees", proc_s + "_flat", 0.30);
        double fee  = amount * (pct / 100.0) + flat;
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2f", fee);
        snprintf(buf + 32, 32, "%.2f", pct);
        return HttpResponse{200,
            "{\"amount\":" + amt_s +
            ",\"processor\":\"" + je(proc_s) + "\""
            ",\"fee_pct\":" + std::string(buf + 32) +
            ",\"fee_flat\":" + std::to_string(flat) +
            ",\"fee_amount\":\"" + std::string(buf) + "\"}"};
    });

    r.add("POST", "/api/cc-fees", [](const HttpRequest& req) {
        json j;
        try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"};
        }
        Rows r2 = Db::instance().query(
            "INSERT INTO cc_fees(sale_id,processor,fee_pct,fee_flat,fee_amount,"
            "transaction_ref,fee_date,notes) VALUES(?,?,?,?,?,?,?,?) RETURNING id",
            {j.value("sale_id",""),
             j.value("processor",""),
             j.value("fee_pct","0"),
             j.value("fee_flat","0"),
             j.value("fee_amount","0"),
             j.value("transaction_ref",""),
             j.value("fee_date",""),
             j.value("notes","")});
        std::string new_id = (r2.empty() ? "" : r2[0][0]);
        return HttpResponse{201, "{\"id\":" + new_id + "}"};
    });

    r.add("DELETE", "/api/cc-fees/:id", [](const HttpRequest& req) {
        std::string id = req.params.count("id") ? req.params.at("id") : "";
        Db::instance().exec("DELETE FROM cc_fees WHERE id=?", {id});
        return HttpResponse{200, R"({"ok":true})"};
    });
}

// ---------------------------------------------------------------------------
// Register all
// ---------------------------------------------------------------------------
void register_business_routes(Router& r, const Pson& cfg) {
    register_consignment_routes(r);
    register_rental_routes(r, cfg);
    register_advertising_routes(r);
    register_cc_fee_routes(r, cfg);
}

} // namespace metis::antique
