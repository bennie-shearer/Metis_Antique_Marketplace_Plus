#include "app_sync.hpp"
#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "json.hpp"
#include "pson.hpp"
#include "smtp.hpp"   // for TCP send helper pattern

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSESOCK closesocket
typedef SOCKET sync_sock_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#define CLOSESOCK close
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
typedef int sync_sock_t;
#endif

#ifdef METIS_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <cstring>
#include <atomic>

namespace metis::antique {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// HTTPS GET/POST helper -- used by all channel adapters
// ---------------------------------------------------------------------------
struct HttpResult { int status; std::string body; };

static HttpResult https_request(const std::string& host,
                                 const std::string& path,
                                 const std::string& method,
                                 const std::string& body_data,
                                 const std::vector<std::string>& extra_headers) {
#ifndef METIS_TLS
    return {0, "TLS not compiled in"};
#else
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), "443", &hints, &res) != 0 || !res)
        return {0, "DNS failed: " + host};

    sync_sock_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) { freeaddrinfo(res); return {0, "socket() failed"}; }
    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        CLOSESOCK(fd); freeaddrinfo(res);
        return {0, "connect() failed to " + host};
    }
    freeaddrinfo(res);

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { CLOSESOCK(fd); return {0, "SSL_CTX_new failed"}; }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, (int)fd);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl); SSL_CTX_free(ctx); CLOSESOCK(fd);
        return {0, "SSL_connect failed"};
    }

    // Build HTTP/1.1 request
    std::string req = method + " " + path + " HTTP/1.1\r\n";
    req += "Host: " + host + "\r\n";
    req += "User-Agent: MetisAntiquePlus/1.2.35\r\n";
    req += "Accept: application/json\r\n";
    req += "Connection: close\r\n";
    for (const auto& h : extra_headers) req += h + "\r\n";
    if (!body_data.empty()) {
        req += "Content-Type: application/json\r\n";
        req += "Content-Length: " + std::to_string(body_data.size()) + "\r\n";
    }
    req += "\r\n";
    if (!body_data.empty()) req += body_data;

    SSL_write(ssl, req.c_str(), (int)req.size());

    // Read response
    std::string resp;
    char buf[4096];
    int n;
    while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0)
        resp.append(buf, n);

    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); CLOSESOCK(fd);

    // Parse status line
    int status = 0;
    if (resp.size() > 12) {
        try { status = std::stoi(resp.substr(9, 3)); } catch (...) {}
    }
    // Find body after \r\n\r\n
    auto pos = resp.find("\r\n\r\n");
    std::string body = (pos != std::string::npos) ? resp.substr(pos + 4) : resp;
    return {status, body};
#endif
}

// ---------------------------------------------------------------------------
// Channel adapters
// ---------------------------------------------------------------------------

// ── eBay ────────────────────────────────────────────────────────────────────
static void sync_ebay_listing(const std::string& listing_id,
                               const Pson& cfg) {
    // Fetch listing from DB
    Rows rows = Db::instance().query(
        "SELECT l.id,l.item_id,i.title,i.description,l.list_price,"
        "l.list_type,l.external_id,i.sku,i.category "
        "FROM listings l JOIN items i ON i.id=l.item_id "
        "WHERE l.id=? AND l.channel='eBay'", {listing_id});
    if (rows.empty()) return;

    bool sandbox     = cfg.get_bool("sync.ebay", "sandbox", true);
    std::string host = sandbox
        ? cfg.get("sync_hosts", "ebay_sandbox",    "api.sandbox.ebay.com")
        : cfg.get("sync_hosts", "ebay_production", "api.ebay.com");
    std::string tok  = cfg.get("sync.ebay", "refresh_token", "");
    if (tok.empty()) {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"eBay refresh_token not configured in config.pson", listing_id});
        return;
    }

    const auto& r = rows[0];
    std::string ext_id = r[6];
    bool is_new = ext_id.empty();

    // Build eBay Inventory Item payload (eBay REST API)
    std::string sku_key = r[7].empty() ? "AMP-" + r[0] : r[7];
    json payload = {
        {"availability", {{"shipToLocationAvailability",
            {{"quantity", 1}}}}},
        {"condition", "USED_EXCELLENT"},
        {"product", {
            {"title",       r[2]},
            {"description", r[3]},
            {"aspects",     {{"Category", {r[8]}}}},
        }}
    };

    std::string path = "/sell/inventory/v1/inventory_item/" + sku_key;
    std::vector<std::string> headers = {
        "Authorization: Bearer " + tok,
        "Content-Language: en-US",
        "Accept-Language: en-US"
    };

    auto res = https_request(host, path, "PUT",
                             payload.dump(), headers);

    if (res.status == 200 || res.status == 201 || res.status == 204) {
        Db::instance().exec(
            "UPDATE listings SET sync_status='synced',external_id=?,"
            "last_synced=datetime('now'),sync_error='' WHERE id=?",
            {sku_key, listing_id});
        LOG_INFO("eBay sync OK: listing=" + listing_id + " sku=" + sku_key);
    } else {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"eBay HTTP " + std::to_string(res.status) + ": " + res.body.substr(0,200),
             listing_id});
        LOG_WARN("eBay sync error: listing=" + listing_id + " status=" +
                 std::to_string(res.status));
    }
}

// ── Etsy ────────────────────────────────────────────────────────────────────
static void sync_etsy_listing(const std::string& listing_id,
                               const Pson& cfg) {
    Rows rows = Db::instance().query(
        "SELECT l.id,l.item_id,i.title,i.description,l.list_price,"
        "l.external_id,i.sku,i.category,i.condition "
        "FROM listings l JOIN items i ON i.id=l.item_id "
        "WHERE l.id=? AND l.channel='Etsy'", {listing_id});
    if (rows.empty()) return;

    std::string tok     = cfg.get("sync.etsy", "access_token", "");
    std::string shop_id = cfg.get("sync.etsy", "shop_id", "");
    if (tok.empty() || shop_id.empty()) {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"Etsy access_token or shop_id not configured", listing_id});
        return;
    }

    const auto& r = rows[0];
    std::string ext_id = r[5];
    bool is_new = ext_id.empty();
    char price_buf[32];
    double price = 0; try { price = std::stod(r[4]); } catch (...) {}
    // Etsy price is in cents (integer)
    long price_cents = (long)(price * 100.0);

    json payload = {
        {"title",       r[2]},
        {"description", r[3].empty() ? r[2] : r[3]},
        {"price",       price_cents},
        {"quantity",    1},
        {"who_made",    "someone_else"},
        {"when_made",   "before_1990"},
        {"taxonomy_id", 1}   // Antiques & Collectibles - simplified
    };

    std::string host = cfg.get("sync_hosts", "etsy", "openapi.etsy.com");
    std::string path = is_new
        ? "/v3/application/shops/" + shop_id + "/listings"
        : "/v3/application/shops/" + shop_id + "/listings/" + ext_id;
    std::string method = is_new ? "POST" : "PATCH";
    std::string api_key = cfg.get("sync.etsy", "api_key", "");

    std::vector<std::string> headers = {
        "Authorization: Bearer " + tok,
        "x-api-key: " + api_key
    };

    auto res = https_request(host, path, method, payload.dump(), headers);

    if (res.status == 200 || res.status == 201) {
        std::string new_ext_id = ext_id;
        try {
            json resp = json::parse(res.body);
            if (resp.contains("listing_id"))
                new_ext_id = std::to_string(resp["listing_id"].get<long>());
        } catch (...) {}
        Db::instance().exec(
            "UPDATE listings SET sync_status='synced',external_id=?,"
            "external_url=?,last_synced=datetime('now'),sync_error='' WHERE id=?",
            {new_ext_id,
             "https://www.etsy.com/listing/" + new_ext_id,
             listing_id});
        LOG_INFO("Etsy sync OK: listing=" + listing_id);
    } else {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"Etsy HTTP " + std::to_string(res.status) + ": " +
             res.body.substr(0, 200), listing_id});
        LOG_WARN("Etsy sync error: listing=" + listing_id);
    }
}

// ── 1stDibs ─────────────────────────────────────────────────────────────────
static void sync_onedibs_listing(const std::string& listing_id,
                                  const Pson& cfg) {
    Rows rows = Db::instance().query(
        "SELECT l.id,l.item_id,i.title,i.description,l.list_price,"
        "l.external_id,i.category,i.era,i.maker,i.condition "
        "FROM listings l JOIN items i ON i.id=l.item_id "
        "WHERE l.id=? AND l.channel='1stDibs'", {listing_id});
    if (rows.empty()) return;

    std::string api_key   = cfg.get("sync.onedibs", "api_key", "");
    std::string dealer_id = cfg.get("sync.onedibs", "dealer_id", "");
    if (api_key.empty()) {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"1stDibs api_key not configured", listing_id});
        return;
    }

    const auto& r = rows[0];
    double price = 0; try { price = std::stod(r[4]); } catch (...) {}

    json payload = {
        {"title",      r[2]},
        {"description",r[3].empty() ? r[2] : r[3]},
        {"price",      {{"amount", price}, {"currency", "USD"}}},
        {"category",   r[6]},
        {"period",     r[7]},
        {"creator",    r[8]},
        {"condition",  r[9]},
        {"quantity",   1}
    };

    std::string ext_id = r[5];
    bool is_new = ext_id.empty();
    std::string host = cfg.get("sync_hosts", "onedibs", "api.1stdibs.com");
    std::string path = is_new
        ? "/v1/dealers/" + dealer_id + "/listings"
        : "/v1/dealers/" + dealer_id + "/listings/" + ext_id;

    std::vector<std::string> headers = {
        "X-1stDibs-Api-Key: " + api_key
    };

    auto res = https_request(host, path,
                             is_new ? "POST" : "PUT",
                             payload.dump(), headers);

    if (res.status == 200 || res.status == 201) {
        std::string new_ext_id = ext_id;
        try {
            json resp = json::parse(res.body);
            if (resp.contains("id"))
                new_ext_id = resp["id"].get<std::string>();
        } catch (...) {}
        Db::instance().exec(
            "UPDATE listings SET sync_status='synced',external_id=?,"
            "last_synced=datetime('now'),sync_error='' WHERE id=?",
            {new_ext_id, listing_id});
        LOG_INFO("1stDibs sync OK: listing=" + listing_id);
    } else {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"1stDibs HTTP " + std::to_string(res.status) + ": " +
             res.body.substr(0, 200), listing_id});
        LOG_WARN("1stDibs sync error: listing=" + listing_id);
    }
}

// ── Chairish ────────────────────────────────────────────────────────────────
static void sync_chairish_listing(const std::string& listing_id,
                                   const Pson& cfg) {
    Rows rows = Db::instance().query(
        "SELECT l.id,l.item_id,i.title,i.description,l.list_price,"
        "l.external_id,i.category,i.era,i.maker,i.condition "
        "FROM listings l JOIN items i ON i.id=l.item_id "
        "WHERE l.id=? AND l.channel='Chairish'", {listing_id});
    if (rows.empty()) return;

    std::string api_key = cfg.get("sync.chairish", "api_key", "");
    if (api_key.empty()) {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"Chairish api_key not configured", listing_id});
        return;
    }

    const auto& r = rows[0];
    double price = 0; try { price = std::stod(r[4]); } catch (...) {}

    json payload = {
        {"title",       r[2]},
        {"description", r[3].empty() ? r[2] : r[3]},
        {"price",       price},
        {"quantity",    1},
        {"category",    r[6]},
        {"era",         r[7]},
        {"designer",    r[8]},
        {"condition",   r[9]}
    };

    std::string ext_id = r[5];
    bool is_new = ext_id.empty();
    std::string host = cfg.get("sync_hosts", "chairish", "api.chairish.com");
    std::string path = is_new ? "/v1/items" : "/v1/items/" + ext_id;

    std::vector<std::string> headers = {
        "Authorization: Token " + api_key
    };

    auto res = https_request(host, path,
                             is_new ? "POST" : "PUT",
                             payload.dump(), headers);

    if (res.status == 200 || res.status == 201) {
        std::string new_ext_id = ext_id;
        try {
            json resp = json::parse(res.body);
            if (resp.contains("id"))
                new_ext_id = resp["id"].get<std::string>();
        } catch (...) {}
        Db::instance().exec(
            "UPDATE listings SET sync_status='synced',external_id=?,"
            "last_synced=datetime('now'),sync_error='' WHERE id=?",
            {new_ext_id, listing_id});
        LOG_INFO("Chairish sync OK: listing=" + listing_id);
    } else {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"Chairish HTTP " + std::to_string(res.status) + ": " +
             res.body.substr(0, 200), listing_id});
        LOG_WARN("Chairish sync error: listing=" + listing_id);
    }
}

// ---------------------------------------------------------------------------
// Dispatch to correct channel adapter
// ---------------------------------------------------------------------------
static void sync_listing(const std::string& listing_id,
                          const std::string& channel,
                          const Pson& cfg) {
    Db::instance().exec(
        "UPDATE listings SET sync_status='syncing' WHERE id=?", {listing_id});
    if (channel == "eBay")     sync_ebay_listing(listing_id, cfg);
    else if (channel == "Etsy")     sync_etsy_listing(listing_id, cfg);
    else if (channel == "1stDibs")  sync_onedibs_listing(listing_id, cfg);
    else if (channel == "Chairish") sync_chairish_listing(listing_id, cfg);
    else {
        Db::instance().exec(
            "UPDATE listings SET sync_status='error',sync_error=? WHERE id=?",
            {"Channel not supported for sync: " + channel, listing_id});
    }
}

// ---------------------------------------------------------------------------
// Background polling thread -- pulls updates at configured interval
// ---------------------------------------------------------------------------
void start_sync_threads(const Pson& cfg) {
    // Check if any channel is enabled
    bool any = cfg.get_bool("sync.ebay",     "enabled", false) ||
               cfg.get_bool("sync.etsy",     "enabled", false) ||
               cfg.get_bool("sync.chairish", "enabled", false) ||
               cfg.get_bool("sync.onedibs",  "enabled", false);
    if (!any) return;

    static std::jthread sync_thread([&cfg](std::stop_token st) {
        using namespace std::chrono;
        using Rows = metis::antique::Rows;
        LOG_SYSTEM("Platform sync thread started");
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(minutes(1));
            if (st.stop_requested()) break;

            // Find listings pending sync or that haven't been synced in interval
            Rows pending = Db::instance().query(
                "SELECT id,channel FROM listings "
                "WHERE status='active' AND sync_status IN ('pending','synced') "
                "AND channel IN ('eBay','Etsy','1stDibs','Chairish') "
                "AND (last_synced IS NULL OR "
                "     (strftime('%s','now') - strftime('%s',last_synced)) > 1800)"
                " LIMIT 20");

            for (const auto& row : pending) {
                if (st.stop_requested()) break;
                std::string lid     = row[0];
                std::string channel = row[1];
                // Check channel enabled
                std::string key = (channel=="eBay")?"ebay":
                                  (channel=="Etsy")?"etsy":
                                  (channel=="1stDibs")?"onedibs":"chairish";
                if (!cfg.get_bool("sync." + key, "enabled", false)) continue;
                sync_listing(lid, channel, cfg);
                std::this_thread::sleep_for(seconds(2)); // rate limit
            }
        }
        LOG_SYSTEM("Platform sync thread stopped");
    });
}

// ---------------------------------------------------------------------------
// API routes
// ---------------------------------------------------------------------------
void register_sync_routes(Router& r, const Pson& cfg) {

    // GET /api/sync/status -- which channels are configured and enabled
    r.add("GET", "/api/sync/status", [&cfg](const HttpRequest&) {
        auto chan = [&cfg](const std::string& key, const std::string& label) {
            bool en   = cfg.get_bool("sync." + key, "enabled", false);
            bool cred = !cfg.get("sync." + key, "api_key", "").empty() ||
                        !cfg.get("sync." + key, "access_token", "").empty() ||
                        !cfg.get("sync." + key, "refresh_token", "").empty();
            return "{\"channel\":\"" + label + "\","
                   "\"enabled\":" + (en?"true":"false") + ","
                   "\"credentials\":" + (cred?"true":"false") + "}";
        };
        return HttpResponse{200,
            "{\"channels\":["
            + chan("ebay",     "eBay")     + ","
            + chan("etsy",     "Etsy")     + ","
            + chan("onedibs",  "1stDibs")  + ","
            + chan("chairish", "Chairish") + "]}"};;
    });

    // POST /api/sync/push  { "listing_id": N }  -- manual push to channel
    r.add("POST", "/api/sync/push", [&cfg](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string lid = j.value("listing_id", "");
        if (lid.empty())
            return HttpResponse{400, R"({"error":"listing_id required"})"};
        Rows rows = Db::instance().query(
            "SELECT channel FROM listings WHERE id=?", {lid});
        if (rows.empty())
            return HttpResponse{404, R"({"error":"listing not found"})"};
        std::string channel = rows[0][0];
        // Run sync in detached thread so we can return immediately
        std::thread([lid, channel, &cfg]() {
            sync_listing(lid, channel, cfg);
        }).detach();
        return HttpResponse{200,
            "{\"ok\":true,\"message\":\"Sync started for listing " + lid + "\"}"};
    });

    // POST /api/sync/push-all  -- push all active local listings for a channel
    r.add("POST", "/api/sync/push-all", [&cfg](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) { j = json::object(); }
        std::string channel = j.value("channel", "");
        std::string sql =
            "SELECT id FROM listings WHERE status='active' "
            "AND sync_status NOT IN ('syncing')";
        std::vector<std::string> params;
        if (!channel.empty()) { sql += " AND channel=?"; params.push_back(channel); }
        Rows rows = Db::instance().query(sql, params);
        int count = (int)rows.size();
        std::thread([rows, &cfg]() {
            for (const auto& r2 : rows) {
                Rows cr = Db::instance().query(
                    "SELECT channel FROM listings WHERE id=?", {r2[0]});
                if (!cr.empty()) sync_listing(r2[0], cr[0][0], cfg);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }).detach();
        return HttpResponse{200,
            "{\"ok\":true,\"queued\":" + std::to_string(count) + "}"};
    });

    // GET /api/sync/listings -- listings with sync status
    r.add("GET", "/api/sync/listings", [](const HttpRequest& req) {
        std::string channel = util::query_param(req.query, "channel");
        std::string sql =
            "SELECT l.id,l.item_id,i.title,l.channel,l.list_price,"
            "l.sync_status,COALESCE(l.external_id,''),COALESCE(l.external_url,''),"
            "COALESCE(l.last_synced,''),COALESCE(l.sync_error,''),l.status "
            "FROM listings l JOIN items i ON i.id=l.item_id WHERE 1=1";
        std::vector<std::string> params;
        if (!channel.empty()) { sql += " AND l.channel=?"; params.push_back(channel); }
        sql += " ORDER BY l.created_at DESC";
        Rows rows = Db::instance().query(sql, params);
        std::string out = "[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) out += ",";
            const auto& r = rows[i];
            out += "{\"id\":" + r[0] +
                   ",\"item_id\":" + r[1] +
                   ",\"title\":\"" + util::json_escape(r[2]) + "\"" +
                   ",\"channel\":\"" + util::json_escape(r[3]) + "\"" +
                   ",\"list_price\":" + r[4] +
                   ",\"sync_status\":\"" + r[5] + "\"" +
                   ",\"external_id\":\"" + util::json_escape(r[6]) + "\"" +
                   ",\"external_url\":\"" + util::json_escape(r[7]) + "\"" +
                   ",\"last_synced\":\"" + r[8] + "\"" +
                   ",\"sync_error\":\"" + util::json_escape(r[9]) + "\"" +
                   ",\"status\":\"" + r[10] + "\"}";
        }
        return HttpResponse{200, out + "]"};
    });

    // POST /api/sync/webhook/:channel -- receive sold/updated notifications
    // eBay, Etsy, and Chairish POST to this endpoint when an item sells
    r.add("POST", "/api/sync/webhook/:channel", [&cfg](const HttpRequest& req) {
        std::string channel = req.params.count("channel") ? req.params.at("channel") : "";
        LOG_INFO("Sync webhook received: channel=" + channel);
        json j; try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{200, R"({"ok":true})"}; } // always 200 to webhook callers

        // eBay: notification.data.order -> look up by externalId and mark sold
        std::string ext_id;
        try {
            if (channel == "ebay" && j.contains("notification"))
                ext_id = j["notification"]["data"]["order"]["lineItems"][0]["sku"].get<std::string>();
            else if (channel == "etsy" && j.contains("listing_id"))
                ext_id = std::to_string(j["listing_id"].get<long>());
            else if (channel == "chairish" && j.contains("item") && j["item"].contains("id"))
                ext_id = j["item"]["id"].get<std::string>();
        } catch (...) {}

        if (!ext_id.empty()) {
            Rows listing = Db::instance().query(
                "SELECT l.id,l.item_id,l.list_price FROM listings l "
                "WHERE l.external_id=? LIMIT 1", {ext_id});
            if (!listing.empty()) {
                std::string lid    = listing[0][0];
                std::string iid    = listing[0][1];
                std::string price  = listing[0][2];
                // Check auto_mark_sold config
                std::string ckey   = (channel=="ebay")?"ebay":
                                     (channel=="etsy")?"etsy":
                                     (channel=="onedibs")?"onedibs":"chairish";
                bool auto_sold = cfg.get_bool("sync." + ckey, "auto_mark_sold", true);
                if (auto_sold) {
                    // Record sale
                    double sp = 0; try { sp = std::stod(price); } catch (...) {}
                    Db::instance().exec(
                        "INSERT INTO sales(item_id,sale_price,net_proceeds,"
                        "channel,sale_date) VALUES(?,?,?,?,date('now'))",
                        {iid, std::to_string(sp), std::to_string(sp),
                         channel});
                    // Mark item sold
                    Db::instance().exec(
                        "UPDATE items SET status='sold',updated_at=datetime('now') "
                        "WHERE id=?", {iid});
                    // Mark listing removed
                    Db::instance().exec(
                        "UPDATE listings SET status='sold',"
                        "sync_status='sold' WHERE id=?", {lid});
                    LOG_SYSTEM("Auto-sold via webhook: channel=" + channel +
                               " item=" + iid + " price=" + price);
                }
            }
        }
        return HttpResponse{200, R"({"ok":true})"};
    });
}

} // namespace metis::antique
