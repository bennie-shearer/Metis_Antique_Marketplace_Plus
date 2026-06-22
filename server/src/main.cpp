#include "server.hpp"
#include "db.hpp"
#include "logger.hpp"
#include "pson.hpp"
#include "util.hpp"
#include "app_sync.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <filesystem>
#include <chrono>

static metis::antique::Server* g_server = nullptr;

static void handle_signal(int) {
    if (g_server) g_server->stop();
}

// Priority 7: Validate required config fields
static bool validate_config(const metis::antique::Pson& cfg) {
    bool ok = true;
    auto require = [&](const std::string& sec, const std::string& key) {
        if (cfg.get(sec, key, "__MISSING__") == "__MISSING__") {
            std::cerr << "[ERROR] config.pson missing: " << sec << "." << key << "\n";
            ok = false;
        }
    };
    require("server", "port");
    require("server", "web_root");
    require("server", "log_dir");
    require("auth",   "admin_user");
    require("auth",   "admin_pass");
    require("database", "path");
    return ok;
}

// Priority 7: Nightly backup jthread
static void start_backup_jthread(const metis::antique::Pson& cfg) {
    std::string backup_dir = cfg.get("app", "backup_dir", "");
    if (backup_dir.empty()) return;
    backup_dir = metis::antique::util::resolve_exe_relative(backup_dir);
    int keep        = cfg.get_int("app", "backup_keep", 7);
    int backup_hour = cfg.get_int("app", "backup_hour", 2);

    static std::jthread backer([backup_dir, keep, backup_hour](std::stop_token st){
        using namespace std::chrono;
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(hours(1));
            if (st.stop_requested()) break;
            auto now = system_clock::now();
            std::time_t t = system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            if (tm.tm_hour != backup_hour) continue;

            std::filesystem::create_directories(backup_dir);
            char datebuf[16];
            strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", &tm);
            std::string dest = backup_dir + "/antique-" + datebuf + ".db";
            if (metis::antique::Db::instance().backup(dest)) {
                LOG_SYSTEM("Backup written: " + dest);
            } else {
                LOG_ERROR("Backup failed: " + dest);
            }
            // Prune old backups
            std::vector<std::filesystem::path> backups;
            for (auto& e : std::filesystem::directory_iterator(backup_dir)) {
                if (e.path().extension() == ".db") backups.push_back(e.path());
            }
            std::sort(backups.begin(), backups.end());
            while (static_cast<int>(backups.size()) > keep) {
                std::error_code ec;
                std::filesystem::remove(backups.front(), ec);
                backups.erase(backups.begin());
            }
        }
    });
}

int main() {
    std::string cfg_path = metis::antique::util::resolve_exe_relative("config.pson");
    metis::antique::Pson cfg;
    if (!cfg.load(cfg_path)) {
        std::cerr << "[ERROR] Cannot load config.pson from: " << cfg_path << "\n";
        return 1;
    }

    if (!validate_config(cfg)) return 1;

    std::string log_dir  = metis::antique::util::resolve_exe_relative(
        cfg.get("server", "log_dir", "logs"));
    metis::antique::Logger::instance().init(log_dir,
        cfg.get_int("server", "log_max_mb", 10));

    LOG_SYSTEM("=== Metis Antique Marketplace Plus v" + cfg.get("app", "version", "1.0.0") + " ===");

    std::string db_path = metis::antique::util::resolve_exe_relative(
        cfg.get("database", "path", "data/antique.db"));
    if (!metis::antique::Db::instance().open(db_path)) return 1;
    metis::antique::Db::instance().create_schema();

    start_backup_jthread(cfg);
    metis::antique::start_sync_threads(cfg);

    // Booth rent collection thread
    if (cfg.get_bool("booth_rent", "collection_enabled", false)) {
        int invoice_day  = cfg.get_int("booth_rent", "invoice_day",    1);
        int due_day      = cfg.get_int("booth_rent", "due_day",        5);
        int grace_days   = cfg.get_int("booth_rent", "grace_days",     5);
        int check_hour   = cfg.get_int("booth_rent", "collection_hour",8);
        double lf_pct    = cfg.get_double("booth_rent","late_fee_pct", 5.0);

        static std::jthread booth_collector([invoice_day, due_day, grace_days,
                                             check_hour, lf_pct](std::stop_token st) {
            using namespace std::chrono;
            using Rows = metis::antique::Rows;
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(hours(1));
                if (st.stop_requested()) break;
                std::time_t t = system_clock::to_time_t(system_clock::now());
                std::tm tm{};
#ifdef _WIN32
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif
                if (tm.tm_hour != check_hour) continue;
                int dom = tm.tm_mday;

                if (dom == invoice_day) {
                    char period[8];
                    strftime(period, sizeof(period), "%Y-%m", &tm);
                    Rows active = metis::antique::Db::instance().query(
                        "SELECT id,monthly_rate,vendor FROM rentals "
                        "WHERE end_date IS NULL OR end_date=''");
                    for (const auto& r : active) {
                        std::string rid = r[0];
                        Rows ex = metis::antique::Db::instance().query(
                            "SELECT id FROM booth_invoices WHERE rental_id=? AND period=?",
                            std::vector<std::string>{rid, std::string(period)});
                        if (!ex.empty()) continue;
                        double rate = std::stod(r[1]);
                        metis::antique::Db::instance().exec(
                            "INSERT INTO rental_payments(rental_id,period,base_amount,amount,status) "
                            "VALUES(?,?,?,?,'pending')",
                            std::vector<std::string>{rid, std::string(period), std::to_string(rate), std::to_string(rate)});
                        metis::antique::Db::instance().exec(
                            "INSERT INTO booth_invoices(rental_id,period,amount_due,discount_amt,"
                            "late_fee,total_due,status,sent_at) "
                            "VALUES(?,?,?,0,0,?,'sent',datetime('now'))",
                            std::vector<std::string>{rid, std::string(period), std::to_string(rate), std::to_string(rate)});
                        LOG_SYSTEM("Booth invoice generated: rental=" + rid +
                                   " vendor=" + r[2] + " period=" + std::string(period));
                    }
                }

                if (dom > due_day + grace_days) {
                    char period[8];
                    strftime(period, sizeof(period), "%Y-%m", &tm);
                    Rows overdue = metis::antique::Db::instance().query(
                        "SELECT rp.id,r.monthly_rate FROM rental_payments rp "
                        "JOIN rentals r ON r.id=rp.rental_id "
                        "WHERE rp.period=? AND rp.status='pending'",
                        std::vector<std::string>{std::string(period)});
                    for (const auto& op : overdue) {
                        double late_fee = std::stod(op[1]) * (lf_pct / 100.0);
                        double new_total = std::stod(op[1]) + late_fee;
                        metis::antique::Db::instance().exec(
                            "UPDATE rental_payments SET status='overdue',"
                            "late_fee=?,amount=? WHERE id=?",
                            std::vector<std::string>{std::to_string(late_fee), std::to_string(new_total), op[0]});
                        metis::antique::Db::instance().exec(
                            "UPDATE booth_invoices SET status='overdue',"
                            "late_fee=?,total_due=total_due+? "
                            "WHERE payment_id=?",
                            std::vector<std::string>{std::to_string(late_fee), std::to_string(late_fee), op[0]});
                        LOG_WARN("Booth rent overdue: payment_id=" + op[0]);
                    }
                }
            }
        });
    }
    if (cfg.get_bool("settlements", "enabled", false)) {
        int grace_days    = cfg.get_int("settlements", "grace_days", 7);
        int check_hour    = cfg.get_int("settlements", "check_hour", 3);
        std::string from_addr = cfg.get("email", "from_address", "");
        bool email_ok = cfg.get_bool("email", "enabled", false);

        static std::jthread settler([grace_days, check_hour, email_ok, from_addr,
                                     &cfg](std::stop_token st) {
            using namespace std::chrono;
            using Rows = metis::antique::Rows;
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(hours(1));
                if (st.stop_requested()) break;
                auto now = system_clock::now();
                std::time_t t = system_clock::to_time_t(now);
                std::tm tm{};
#ifdef _WIN32
                localtime_s(&tm, &t);
#else
                localtime_r(&t, &tm);
#endif
                if (tm.tm_hour != check_hour) continue;

                // Find dealers with unsettled sales older than grace_days
                char cutoff[16];
                // cutoff = today - grace_days
                std::time_t cutoff_t = t - static_cast<std::time_t>(grace_days) * 86400;
                std::tm ctm{};
#ifdef _WIN32
                localtime_s(&ctm, &cutoff_t);
#else
                localtime_r(&cutoff_t, &ctm);
#endif
                strftime(cutoff, sizeof(cutoff), "%Y-%m-%d", &ctm);

                // Find dealers with unsettled sales
                Rows dealers = metis::antique::Db::instance().query(R"(
SELECT DISTINCT i.owner_id, u.username
FROM sales s
JOIN items i ON s.item_id = i.id
JOIN users u ON i.owner_id = u.id
WHERE i.owner_id IS NOT NULL
  AND s.sale_date <= ?
  AND NOT EXISTS (
      SELECT 1 FROM settlements st
      WHERE st.owner_id = i.owner_id
        AND st.period_end >= s.sale_date
        AND st.status != 'cancelled'
  ))", std::vector<std::string>{std::string(cutoff)});

                for (const auto& d : dealers) {
                    if (d.size() < 2) continue;
                    std::string owner_id = d[0];
                    std::string username = d[1];

                    // Calculate settlement totals
                    Rows totals = metis::antique::Db::instance().query(R"(
SELECT COALESCE(SUM(s.net_proceeds),0),
       COALESCE(SUM(s.platform_fee+s.shipping_cost),0),
       MIN(s.sale_date), MAX(s.sale_date)
FROM sales s
JOIN items i ON s.item_id = i.id
WHERE i.owner_id = ?
  AND s.sale_date <= ?
  AND NOT EXISTS (
      SELECT 1 FROM settlements st
      WHERE st.owner_id = i.owner_id
        AND st.period_end >= s.sale_date
        AND st.status != 'cancelled'
  ))", std::vector<std::string>{owner_id, std::string(cutoff)});

                    if (totals.empty() || totals[0][0] == "0") continue;

                    double net  = std::stod(totals[0][0]);
                    double fees = std::stod(totals[0][1]);
                    std::string p_start = totals[0][2];
                    std::string p_end   = totals[0][3];

                    // Create settlement record
                    metis::antique::Db::instance().exec(R"(
INSERT INTO settlements(owner_id,period_start,period_end,total_sales,total_fees,net_due,status)
VALUES(?,?,?,?,?,?,'pending'))",
                        std::vector<std::string>{owner_id, p_start, p_end,
                         totals[0][0], std::to_string(fees),
                         std::to_string(net - fees)});

                    LOG_SYSTEM("Settlement created for dealer: " + username +
                               " net=" + std::to_string(net - fees));

                    // Email settlement statement if configured
                    if (email_ok && !from_addr.empty()) {
                        Rows email_r = metis::antique::Db::instance().query(
                            "SELECT id FROM users WHERE username=? LIMIT 1", std::vector<std::string>{username});
                        // Get dealer email from users table if extended later
                        // For now log only
                        LOG_SYSTEM("Settlement email queued for: " + username);
                    }
                }
            }
        });
    }

    metis::antique::Server server(cfg);
    g_server = &server;

    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);

    server.run();
    LOG_SYSTEM("Server stopped");
    return 0;
}
