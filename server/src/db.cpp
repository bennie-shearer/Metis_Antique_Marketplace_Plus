#include "db.hpp"
#include "logger.hpp"
#include "sqlite3.h"
#include <filesystem>
#include <sstream>

namespace metis::antique {

Db& Db::instance() { static Db inst; return inst; }

bool Db::open(const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        LOG_ERROR("DB open failed: " + path);
        return false;
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;",  nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;",   nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);
    LOG_SYSTEM("Database opened: " + path);
    return true;
}

void Db::create_schema() {
    const char* ddl = R"(
CREATE TABLE IF NOT EXISTS users (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    username   TEXT NOT NULL UNIQUE,
    pass_hash  TEXT NOT NULL,
    role       TEXT NOT NULL DEFAULT 'dealer',
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS items (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    title          TEXT NOT NULL,
    category       TEXT NOT NULL,
    era            TEXT,
    maker          TEXT,
    origin         TEXT,
    material       TEXT,
    source         TEXT,
    description    TEXT,
    condition      TEXT NOT NULL DEFAULT 'Good',
    cost_price     REAL NOT NULL DEFAULT 0,
    asking_price   REAL NOT NULL DEFAULT 0,
    status         TEXT NOT NULL DEFAULT 'inventory',
    provenance     TEXT,
    photo_ref      TEXT,
    consignor_name TEXT,
    consignor_pct  REAL,
    created_at     TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at     TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS item_photos (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id     INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,
    filename    TEXT NOT NULL,
    caption     TEXT NOT NULL DEFAULT '',
    is_primary  INTEGER NOT NULL DEFAULT 0,
    sort_order  INTEGER NOT NULL DEFAULT 0,
    uploaded_by TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS item_comments (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id     INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,
    author      TEXT NOT NULL,
    body        TEXT NOT NULL,
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS buyer_inquiries (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    listing_id   INTEGER NOT NULL REFERENCES listings(id) ON DELETE CASCADE,
    item_id      INTEGER NOT NULL REFERENCES items(id),
    buyer_name   TEXT NOT NULL,
    buyer_email  TEXT NOT NULL,
    buyer_phone  TEXT NOT NULL DEFAULT '',
    subject      TEXT NOT NULL DEFAULT '',
    body         TEXT NOT NULL,
    status       TEXT NOT NULL DEFAULT 'open',
    created_at   TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS inquiry_replies (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    inquiry_id   INTEGER NOT NULL REFERENCES buyer_inquiries(id) ON DELETE CASCADE,
    author       TEXT NOT NULL,
    author_role  TEXT NOT NULL DEFAULT 'dealer',
    body         TEXT NOT NULL,
    created_at   TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS purchases (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id          INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,
    vendor_name      TEXT NOT NULL DEFAULT '',
    vendor_contact   TEXT NOT NULL DEFAULT '',
    acquisition_date TEXT NOT NULL DEFAULT (date('now')),
    purchase_price   REAL NOT NULL DEFAULT 0,
    buyers_premium   REAL NOT NULL DEFAULT 0,
    transport_cost   REAL NOT NULL DEFAULT 0,
    restoration_budget REAL NOT NULL DEFAULT 0,
    total_cost       REAL NOT NULL DEFAULT 0,
    purchase_channel TEXT NOT NULL DEFAULT '',
    payment_method   TEXT NOT NULL DEFAULT '',
    receipt_ref      TEXT NOT NULL DEFAULT '',
    notes            TEXT NOT NULL DEFAULT '',
    created_at       TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS expenses (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    category        TEXT NOT NULL,
    description     TEXT NOT NULL,
    amount          REAL NOT NULL,
    expense_date    TEXT NOT NULL DEFAULT (date('now')),
    payment_method  TEXT NOT NULL DEFAULT '',
    vendor          TEXT NOT NULL DEFAULT '',
    receipt_ref     TEXT NOT NULL DEFAULT '',
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS item_history (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id     INTEGER NOT NULL REFERENCES items(id),
    field       TEXT NOT NULL,
    old_value   TEXT,
    new_value   TEXT,
    changed_by  TEXT,
    changed_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS listings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id     INTEGER NOT NULL REFERENCES items(id),
    channel     TEXT NOT NULL,
    list_price  REAL NOT NULL,
    list_type   TEXT NOT NULL DEFAULT 'fixed',
    auction_end TEXT,
    listing_url TEXT,
    views       INTEGER NOT NULL DEFAULT 0,
    watchers    INTEGER NOT NULL DEFAULT 0,
    status      TEXT NOT NULL DEFAULT 'active',
    created_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS sales (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id        INTEGER NOT NULL REFERENCES items(id),
    listing_id     INTEGER REFERENCES listings(id),
    buyer_name     TEXT,
    buyer_email    TEXT,
    sale_price     REAL NOT NULL,
    platform_fee   REAL NOT NULL DEFAULT 0,
    shipping_cost  REAL NOT NULL DEFAULT 0,
    net_proceeds   REAL NOT NULL DEFAULT 0,
    payment_method TEXT,
    sale_date      TEXT NOT NULL DEFAULT (datetime('now')),
    channel        TEXT,
    notes          TEXT
);
CREATE TABLE IF NOT EXISTS appraisals (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id      INTEGER NOT NULL REFERENCES items(id),
    appraiser    TEXT,
    appraised_at TEXT NOT NULL DEFAULT (datetime('now')),
    value_low    REAL,
    value_high   REAL,
    condition    TEXT,
    notes        TEXT,
    status       TEXT NOT NULL DEFAULT 'pending'
);
CREATE TABLE IF NOT EXISTS sessions (
    token      TEXT PRIMARY KEY,
    user_id    INTEGER NOT NULL REFERENCES users(id),
    expires_at TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS login_attempts (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    ip_addr      TEXT NOT NULL,
    attempted_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS consignments (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id         INTEGER REFERENCES items(id) ON DELETE SET NULL,
    consignor_name  TEXT NOT NULL,
    consignor_email TEXT NOT NULL DEFAULT '',
    consignor_phone TEXT NOT NULL DEFAULT '',
    description     TEXT NOT NULL DEFAULT '',
    agreed_pct      REAL NOT NULL DEFAULT 0,
    agreed_floor    REAL NOT NULL DEFAULT 0,
    received_date   TEXT NOT NULL DEFAULT (date('now')),
    sold_date       TEXT,
    sale_price      REAL,
    payout_amount   REAL,
    payout_date     TEXT,
    status          TEXT NOT NULL DEFAULT 'active',
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS rentals (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    rental_type       TEXT NOT NULL DEFAULT 'booth',
    location          TEXT NOT NULL DEFAULT '',
    vendor            TEXT NOT NULL DEFAULT '',
    vendor_user_id    INTEGER REFERENCES users(id) ON DELETE SET NULL,
    monthly_rate      REAL NOT NULL DEFAULT 0,
    discount_pct      REAL NOT NULL DEFAULT 0,
    discount_label    TEXT NOT NULL DEFAULT '',
    start_date        TEXT NOT NULL DEFAULT (date('now')),
    end_date          TEXT,
    payment_method    TEXT NOT NULL DEFAULT '',
    auto_renew        INTEGER NOT NULL DEFAULT 1,
    due_day           INTEGER NOT NULL DEFAULT 1,
    grace_days        INTEGER NOT NULL DEFAULT 5,
    late_fee_pct      REAL NOT NULL DEFAULT 0,
    notes             TEXT NOT NULL DEFAULT '',
    created_at        TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS rental_payments (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    rental_id       INTEGER NOT NULL REFERENCES rentals(id) ON DELETE CASCADE,
    period          TEXT NOT NULL,
    due_date        TEXT NOT NULL DEFAULT (date('now')),
    base_amount     REAL NOT NULL DEFAULT 0,
    discount_pct    REAL NOT NULL DEFAULT 0,
    discount_amount REAL NOT NULL DEFAULT 0,
    late_fee        REAL NOT NULL DEFAULT 0,
    amount          REAL NOT NULL,
    paid_date       TEXT,
    status          TEXT NOT NULL DEFAULT 'pending',
    receipt_ref     TEXT NOT NULL DEFAULT '',
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS booth_invoices (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    rental_id       INTEGER NOT NULL REFERENCES rentals(id) ON DELETE CASCADE,
    payment_id      INTEGER REFERENCES rental_payments(id) ON DELETE SET NULL,
    period          TEXT NOT NULL,
    amount_due      REAL NOT NULL DEFAULT 0,
    discount_amt    REAL NOT NULL DEFAULT 0,
    late_fee        REAL NOT NULL DEFAULT 0,
    total_due       REAL NOT NULL DEFAULT 0,
    status          TEXT NOT NULL DEFAULT 'sent',
    sent_at         TEXT NOT NULL DEFAULT (datetime('now')),
    paid_at         TEXT,
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS advertising (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    platform        TEXT NOT NULL,
    campaign        TEXT NOT NULL DEFAULT '',
    ad_type         TEXT NOT NULL DEFAULT 'listing',
    start_date      TEXT NOT NULL DEFAULT (date('now')),
    end_date        TEXT,
    budget          REAL NOT NULL DEFAULT 0,
    spent           REAL NOT NULL DEFAULT 0,
    impressions     INTEGER NOT NULL DEFAULT 0,
    clicks          INTEGER NOT NULL DEFAULT 0,
    conversions     INTEGER NOT NULL DEFAULT 0,
    status          TEXT NOT NULL DEFAULT 'active',
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS cc_fees (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    sale_id         INTEGER REFERENCES sales(id) ON DELETE SET NULL,
    processor       TEXT NOT NULL DEFAULT '',
    fee_pct         REAL NOT NULL DEFAULT 0,
    fee_flat        REAL NOT NULL DEFAULT 0,
    fee_amount      REAL NOT NULL DEFAULT 0,
    transaction_ref TEXT NOT NULL DEFAULT '',
    fee_date        TEXT NOT NULL DEFAULT (date('now')),
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE TABLE IF NOT EXISTS settlements (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_id        INTEGER NOT NULL REFERENCES users(id),
    period_start    TEXT NOT NULL,
    period_end      TEXT NOT NULL,
    total_sales     REAL NOT NULL DEFAULT 0,
    total_fees      REAL NOT NULL DEFAULT 0,
    net_due         REAL NOT NULL DEFAULT 0,
    status          TEXT NOT NULL DEFAULT 'pending',
    sent_at         TEXT,
    notes           TEXT NOT NULL DEFAULT '',
    created_at      TEXT NOT NULL DEFAULT (datetime('now'))
);
)";
    char* err = nullptr;
    if (sqlite3_exec(db_, ddl, nullptr, nullptr, &err) != SQLITE_OK) {
        LOG_ERROR(std::string("Schema error: ") + (err ? err : ""));
        sqlite3_free(err);
    } else {
        LOG_SYSTEM("Schema ready");
    }
    // Safe ALTER TABLE migrations for existing databases
    run_migration("ALTER TABLE items ADD COLUMN source TEXT");
    run_migration("ALTER TABLE items ADD COLUMN consignor_name TEXT");
    run_migration("ALTER TABLE items ADD COLUMN consignor_pct REAL");
    run_migration("ALTER TABLE listings ADD COLUMN listing_url TEXT");
    run_migration("ALTER TABLE sales ADD COLUMN platform_fee REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE sales ADD COLUMN shipping_cost REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE sales ADD COLUMN net_proceeds REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE sales ADD COLUMN payment_method TEXT");
    run_migration("ALTER TABLE buyer_inquiries ADD COLUMN buyer_phone TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE items ADD COLUMN acquisition_date TEXT");
    run_migration("ALTER TABLE items ADD COLUMN vendor_name TEXT");
    run_migration("ALTER TABLE items ADD COLUMN vendor_contact TEXT");
    run_migration("ALTER TABLE items ADD COLUMN purchase_channel TEXT");
    run_migration("ALTER TABLE purchases ADD COLUMN restoration_budget REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE buyer_inquiries ADD COLUMN subject TEXT NOT NULL DEFAULT ''");
    // v1.2.31 migrations
    run_migration("ALTER TABLE items ADD COLUMN owner_id INTEGER REFERENCES users(id)");
    run_migration("ALTER TABLE items ADD COLUMN sku TEXT");
    run_migration("ALTER TABLE sales ADD COLUMN owner_id INTEGER REFERENCES users(id)");
    // v1.2.32 rental expansions
    run_migration("ALTER TABLE rentals ADD COLUMN vendor_user_id INTEGER REFERENCES users(id)");
    run_migration("ALTER TABLE rentals ADD COLUMN discount_pct REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rentals ADD COLUMN discount_label TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE rentals ADD COLUMN due_day INTEGER NOT NULL DEFAULT 1");
    run_migration("ALTER TABLE rentals ADD COLUMN grace_days INTEGER NOT NULL DEFAULT 5");
    run_migration("ALTER TABLE rentals ADD COLUMN late_fee_pct REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rental_payments ADD COLUMN due_date TEXT");
    run_migration("ALTER TABLE rental_payments ADD COLUMN base_amount REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rental_payments ADD COLUMN discount_pct REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rental_payments ADD COLUMN discount_amount REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rental_payments ADD COLUMN late_fee REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE rental_payments ADD COLUMN status TEXT NOT NULL DEFAULT 'pending'");
    run_migration("ALTER TABLE rental_payments ADD COLUMN paid_date TEXT");
    // v1.2.34 user email
    run_migration("ALTER TABLE users ADD COLUMN email TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE users ADD COLUMN display_name TEXT NOT NULL DEFAULT ''");
    // v1.2.35 platform sync + POS dimensions
    run_migration("ALTER TABLE listings ADD COLUMN external_id TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE listings ADD COLUMN external_url TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE listings ADD COLUMN sync_status TEXT NOT NULL DEFAULT 'local'");
    run_migration("ALTER TABLE listings ADD COLUMN last_synced TEXT");
    run_migration("ALTER TABLE listings ADD COLUMN sync_error TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE items ADD COLUMN upc TEXT NOT NULL DEFAULT ''");
    run_migration("ALTER TABLE items ADD COLUMN weight_oz REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE items ADD COLUMN width_in REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE items ADD COLUMN height_in REAL NOT NULL DEFAULT 0");
    run_migration("ALTER TABLE items ADD COLUMN depth_in REAL NOT NULL DEFAULT 0");
}

void Db::run_migration(const std::string& sql) {
    char* err = nullptr;
    sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (err) sqlite3_free(err);
}

bool Db::exec(const std::string& sql, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lk(mu_);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    for (int i = 0; i < static_cast<int>(params.size()); ++i)
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

Rows Db::query(const std::string& sql, const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lk(mu_);
    sqlite3_stmt* stmt = nullptr;
    Rows rows;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return rows;
    for (int i = 0; i < static_cast<int>(params.size()); ++i)
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    int cols = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int c = 0; c < cols; ++c) {
            const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
            row.push_back(v ? v : "");
        }
        rows.push_back(row);
    }
    sqlite3_finalize(stmt);
    return rows;
}

std::string Db::last_insert_id() {
    return std::to_string(sqlite3_last_insert_rowid(db_));
}

bool Db::backup(const std::string& dest_path) {
    std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
    sqlite3* dest = nullptr;
    if (sqlite3_open(dest_path.c_str(), &dest) != SQLITE_OK) return false;
    sqlite3_backup* bk = sqlite3_backup_init(dest, "main", db_, "main");
    if (!bk) { sqlite3_close(dest); return false; }
    sqlite3_backup_step(bk, -1);
    sqlite3_backup_finish(bk);
    sqlite3_close(dest);
    return true;
}

} // namespace metis::antique
// This won't work - need to inject into the schema string
