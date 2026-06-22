#include "app_pos.hpp"
#include "db.hpp"
#include "logger.hpp"
#include "util.hpp"
#include "json.hpp"
#include "pson.hpp"

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
typedef SOCKET pos_sock_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#define CLOSESOCK close
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
typedef int pos_sock_t;
#endif

#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <cstdio>

namespace metis::antique {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// ESC/POS constants
// ---------------------------------------------------------------------------
static const char ESC  = 0x1B;
static const char GS   = 0x1D;
static const char LF   = 0x0A;
static const char NUL  = 0x00;

static std::string esc_init()        { return std::string({ESC,'@'}); }
static std::string esc_align_center(){ return std::string({ESC,'a',1}); }
static std::string esc_align_left()  { return std::string({ESC,'a',0}); }
static std::string esc_bold_on()     { return std::string({ESC,'E',1}); }
static std::string esc_bold_off()    { return std::string({ESC,'E',0}); }
static std::string esc_double_height(){ return std::string({ESC,'!',0x10}); }
static std::string esc_normal_size() { return std::string({ESC,'!',0}); }
static std::string esc_feed(int n)   { return std::string({ESC,'d',(char)n}); }
static std::string esc_cut()         { return std::string({GS,'V',1}); }
// Cash drawer pulse on pin 2
static std::string esc_cash_drawer() { return std::string({ESC,'p',0,(char)0x19,(char)0x78}); }

// ---------------------------------------------------------------------------
// Build ESC/POS receipt bytes from a sale record
// ---------------------------------------------------------------------------
static std::string build_receipt(const Pson& cfg,
                                  const std::string& sale_id) {
    Rows sale = Db::instance().query(
        "SELECT s.id,s.sale_date,s.sale_price,s.platform_fee,s.shipping_cost,"
        "s.net_proceeds,s.payment_method,s.buyer_name,"
        "i.title,i.category,i.sku "
        "FROM sales s JOIN items i ON s.item_id=i.id WHERE s.id=?", {sale_id});
    if (sale.empty()) return "";

    std::string header  = cfg.get("pos", "receipt_header",
                                  "Metis Antique Marketplace");
    std::string footer  = cfg.get("pos", "receipt_footer",
                                  "Thank you for your purchase!");
    std::string sym     = cfg.get("app", "currency_symbol", "$");

    auto fm = [&sym](const std::string& v) -> std::string {
        double n = 0; try { n = std::stod(v); } catch (...) {}
        char buf[32]; snprintf(buf, sizeof(buf), "%.2f", n);
        return sym + std::string(buf);
    };
    auto padline = [](const std::string& l, const std::string& r,
                      int width=42) -> std::string {
        int space = width - (int)l.size() - (int)r.size();
        if (space < 1) space = 1;
        return l + std::string(space, ' ') + r + "\n";
    };
    auto divider = []() -> std::string {
        return std::string(42, '-') + "\n";
    };

    std::string out;
    out += esc_init();
    out += esc_align_center();
    out += esc_double_height();
    out += esc_bold_on();
    out += header + "\n";
    out += esc_normal_size();
    out += esc_bold_off();
    out += esc_align_left();
    out += "\n";

    // Date
    out += "Sale #" + sale[0][0] + "   " + sale[0][1].substr(0,10) + "\n";
    out += divider();

    // Item
    out += esc_bold_on();
    out += sale[0][8] + "\n";
    out += esc_bold_off();
    out += "Category: " + sale[0][9] + "\n";
    if (!sale[0][10].empty()) out += "SKU: " + sale[0][10] + "\n";
    out += "\n";

    // Amounts
    out += divider();
    out += padline("Sale price:",   fm(sale[0][2]));
    if (!sale[0][3].empty() && sale[0][3] != "0")
        out += padline("Platform fee:", fm(sale[0][3]));
    if (!sale[0][4].empty() && sale[0][4] != "0")
        out += padline("Shipping:",    fm(sale[0][4]));
    out += divider();
    out += esc_bold_on();
    out += padline("NET PROCEEDS:", fm(sale[0][5]));
    out += esc_bold_off();
    out += "\n";

    if (!sale[0][6].empty()) out += "Payment: " + sale[0][6] + "\n";
    if (!sale[0][7].empty()) out += "Buyer:   " + sale[0][7] + "\n";
    out += "\n";

    // Footer
    out += esc_align_center();
    out += footer + "\n";
    out += "\n";
    out += esc_feed(4);
    out += esc_cut();

    return out;
}

// ---------------------------------------------------------------------------
// Build price tag label (compact receipt for labeling)
// ---------------------------------------------------------------------------
static std::string build_price_tag(const Pson& cfg,
                                    const std::string& item_id) {
    Rows item = Db::instance().query(
        "SELECT title,category,era,maker,condition,"
        "asking_price,sku FROM items WHERE id=?", {item_id});
    if (item.empty()) return "";

    std::string sym = cfg.get("app", "currency_symbol", "$");
    std::string shop = cfg.get("pos", "receipt_header", "Metis Antique Marketplace");
    auto fm = [&sym](const std::string& v) {
        double n=0; try{n=std::stod(v);}catch(...){}
        char b[32]; snprintf(b,sizeof(b),"%.2f",n); return sym+std::string(b);
    };

    std::string out;
    out += esc_init();
    out += esc_align_center();
    out += esc_bold_on();
    out += esc_double_height();
    out += fm(item[0][5]) + "\n";
    out += esc_normal_size();
    out += esc_bold_off();
    out += item[0][0].substr(0, 40) + "\n";
    if (!item[0][2].empty()) out += item[0][2];
    if (!item[0][3].empty()) out += "  " + item[0][3];
    out += "\n";
    out += "Condition: " + item[0][4] + "\n";
    if (!item[0][6].empty()) out += "SKU: " + item[0][6] + "\n";
    out += "\n" + shop + "\n";
    out += esc_feed(3);
    out += esc_cut();
    return out;
}

// ---------------------------------------------------------------------------
// Send raw bytes to ESC/POS network printer
// ---------------------------------------------------------------------------
std::string pos_print_receipt(const Pson& cfg, const std::string& data) {
    if (!cfg.get_bool("pos", "enabled", false))
        return "POS not enabled in config.pson";

    std::string type = cfg.get("pos", "receipt_printer_type", "network");
    if (type == "none") return "receipt_printer_type=none";

    std::string host = cfg.get("pos", "receipt_printer_ip", "");
    int port = cfg.get_int("pos", "receipt_printer_port", 9100);
    if (host.empty()) return "receipt_printer_ip not set in config.pson";

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_s = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0 || !res)
        return "DNS resolution failed for " + host;

    pos_sock_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) { freeaddrinfo(res); return "socket() failed"; }

    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        CLOSESOCK(fd); freeaddrinfo(res);
        return "Cannot connect to printer at " + host + ":" + port_s;
    }
    freeaddrinfo(res);

    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(fd, data.c_str() + sent, (int)(data.size() - sent), 0);
        if (n <= 0) { CLOSESOCK(fd); return "send() failed after " + std::to_string(sent) + " bytes"; }
        sent += n;
    }

    // Trigger cash drawer if enabled
    if (cfg.get_bool("pos", "cash_drawer_enabled", false)) {
        std::string pulse = esc_cash_drawer();
        send(fd, pulse.c_str(), (int)pulse.size(), 0);
    }

    CLOSESOCK(fd);
    LOG_INFO("ESC/POS: printed " + std::to_string(sent) + " bytes to " + host);
    return "";
}

// ---------------------------------------------------------------------------
// API routes
// ---------------------------------------------------------------------------
void register_pos_routes(Router& r, const Pson& cfg) {

    // GET /api/pos/status
    r.add("GET", "/api/pos/status", [&cfg](const HttpRequest&) {
        bool enabled = cfg.get_bool("pos", "enabled", false);
        std::string ip   = cfg.get("pos", "receipt_printer_ip", "");
        int port         = cfg.get_int("pos", "receipt_printer_port", 9100);
        bool drawer      = cfg.get_bool("pos", "cash_drawer_enabled", false);
        std::string type = cfg.get("pos", "receipt_printer_type", "network");
        return HttpResponse{200,
            "{\"enabled\":" + std::string(enabled?"true":"false") +
            ",\"printer_ip\":\"" + ip + "\"" +
            ",\"printer_port\":" + std::to_string(port) +
            ",\"printer_type\":\"" + type + "\"" +
            ",\"cash_drawer\":" + std::string(drawer?"true":"false") + "}"};
    });

    // POST /api/pos/print-receipt  { "sale_id": N }
    r.add("POST", "/api/pos/print-receipt", [&cfg](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string sale_id = j.value("sale_id", "");
        if (sale_id.empty())
            return HttpResponse{400, R"({"error":"sale_id required"})"};
        std::string data = build_receipt(cfg, sale_id);
        if (data.empty())
            return HttpResponse{404, R"({"error":"sale not found"})"};
        std::string err = pos_print_receipt(cfg, data);
        if (!err.empty())
            return HttpResponse{502, "{\"error\":\"" + err + "\"}"};
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/pos/print-tag  { "item_id": N }
    r.add("POST", "/api/pos/print-tag", [&cfg](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string item_id = j.value("item_id", "");
        if (item_id.empty())
            return HttpResponse{400, R"({"error":"item_id required"})"};
        std::string data = build_price_tag(cfg, item_id);
        if (data.empty())
            return HttpResponse{404, R"({"error":"item not found"})"};
        std::string err = pos_print_receipt(cfg, data);
        if (!err.empty())
            return HttpResponse{502, "{\"error\":\"" + err + "\"}"};
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/pos/open-drawer
    r.add("POST", "/api/pos/open-drawer", [&cfg](const HttpRequest&) {
        if (!cfg.get_bool("pos", "cash_drawer_enabled", false))
            return HttpResponse{503, R"({"error":"cash drawer not enabled"})"};
        std::string pulse = esc_init() + esc_cash_drawer();
        std::string err = pos_print_receipt(cfg, pulse);
        if (!err.empty())
            return HttpResponse{502, "{\"error\":\"" + err + "\"}"};
        return HttpResponse{200, R"({"ok":true})"};
    });

    // POST /api/pos/scan  { "sku": "AMP-2026-00042" }
    // Looks up item by SKU or UPC and returns item details for quick sale
    r.add("POST", "/api/pos/scan", [](const HttpRequest& req) {
        json j; try { j = json::parse(req.body); } catch (...) {
            return HttpResponse{400, R"({"error":"invalid JSON"})"}; }
        std::string code = j.value("sku", j.value("upc", ""));
        if (code.empty())
            return HttpResponse{400, R"({"error":"sku or upc required"})"};
        Rows rows = Db::instance().query(
            "SELECT id,title,category,status,asking_price,sku,upc "
            "FROM items WHERE sku=? OR upc=? LIMIT 1", {code, code});
        if (rows.empty())
            return HttpResponse{404, R"({"error":"item not found"})"};
        const auto& r2 = rows[0];
        return HttpResponse{200,
            "{\"id\":" + r2[0] +
            ",\"title\":\"" + util::json_escape(r2[1]) + "\"" +
            ",\"category\":\"" + util::json_escape(r2[2]) + "\"" +
            ",\"status\":\"" + r2[3] + "\"" +
            ",\"asking_price\":" + r2[4] +
            ",\"sku\":\"" + util::json_escape(r2[5]) + "\"" +
            ",\"upc\":\"" + util::json_escape(r2[6]) + "\"}"};
    });
}

} // namespace metis::antique
