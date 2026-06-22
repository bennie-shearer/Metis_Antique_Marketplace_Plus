#include "smtp.hpp"
#include "logger.hpp"

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
typedef SOCKET smtp_sock_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#define CLOSESOCK close
#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
typedef int smtp_sock_t;
#endif

#ifdef METIS_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <ctime>

namespace metis::antique {

// ---------------------------------------------------------------------------
// Base64 encode (for AUTH LOGIN and HTML body)
// ---------------------------------------------------------------------------
static std::string b64_encode(const unsigned char* data, size_t len) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int b = (data[i] << 16)
            | (i+1 < len ? data[i+1] << 8 : 0)
            | (i+2 < len ? data[i+2]      : 0);
        out += T[(b >> 18) & 63];
        out += T[(b >> 12) & 63];
        out += (i+1 < len) ? T[(b >>  6) & 63] : '=';
        out += (i+2 < len) ? T[ b        & 63] : '=';
    }
    return out;
}
static std::string b64(const std::string& s) {
    return b64_encode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

// ---------------------------------------------------------------------------
// Quoted-printable encode (for HTML body lines > 76 chars safety)
// ---------------------------------------------------------------------------
static std::string qp_encode(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 64);
    int col = 0;
    for (unsigned char c : s) {
        if ((c == '\r') || (c == '\n')) { out += c; col = 0; continue; }
        if (c >= 33 && c <= 126 && c != '=') {
            if (col >= 75) { out += "=\r\n"; col = 0; }
            out += (char)c; ++col;
        } else {
            if (col >= 73) { out += "=\r\n"; col = 0; }
            char hex[4]; snprintf(hex, sizeof(hex), "=%02X", c);
            out += hex; col += 3;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// RFC 2822 date string
// ---------------------------------------------------------------------------
static std::string rfc2822_date() {
    time_t t = time(nullptr);
    struct tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tm);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Low-level SMTP conversation over SSL*
// Returns empty string on success, error string on failure.
// ---------------------------------------------------------------------------
#ifdef METIS_TLS
static std::string ssl_line(SSL* ssl, std::string& buf) {
    while (true) {
        auto pos = buf.find('\n');
        if (pos != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf = buf.substr(pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
        char tmp[1024];
        int n = SSL_read(ssl, tmp, sizeof(tmp));
        if (n <= 0) return "";
        buf.append(tmp, n);
    }
}

static bool ssl_cmd(SSL* ssl, std::string& rbuf,
                    const std::string& cmd, const std::string& expected_code) {
    if (!cmd.empty()) {
        std::string c = cmd + "\r\n";
        if (SSL_write(ssl, c.c_str(), (int)c.size()) <= 0) return false;
    }
    std::string line = ssl_line(ssl, rbuf);
    // Multi-line responses: keep reading until line with space after code
    while (line.size() >= 4 && line[3] == '-') line = ssl_line(ssl, rbuf);
    return line.substr(0, expected_code.size()) == expected_code;
}

static std::string do_smtp(
    SSL* ssl,
    const std::string& smtp_user,
    const std::string& smtp_pass,
    const std::string& from_address,
    const std::string& from_name,
    const std::string& to_address,
    const std::string& to_name,
    const std::string& subject,
    const std::string& html_body)
{
    std::string rbuf;
    // Greeting
    if (!ssl_cmd(ssl, rbuf, "", "220")) return "no 220 greeting";
    // EHLO
    if (!ssl_cmd(ssl, rbuf, "EHLO localhost", "250")) return "EHLO failed";
    // AUTH LOGIN
    if (!ssl_cmd(ssl, rbuf, "AUTH LOGIN", "334")) return "AUTH LOGIN failed";
    if (!ssl_cmd(ssl, rbuf, b64(smtp_user), "334")) return "username rejected";
    if (!ssl_cmd(ssl, rbuf, b64(smtp_pass), "235")) return "password rejected";
    // MAIL FROM
    if (!ssl_cmd(ssl, rbuf, "MAIL FROM:<" + from_address + ">", "250"))
        return "MAIL FROM rejected";
    // RCPT TO
    if (!ssl_cmd(ssl, rbuf, "RCPT TO:<" + to_address + ">", "250"))
        return "RCPT TO rejected";
    // DATA
    if (!ssl_cmd(ssl, rbuf, "DATA", "354")) return "DATA rejected";

    // Build message headers + body
    std::string msg =
        "Date: " + rfc2822_date() + "\r\n"
        "From: " + from_name + " <" + from_address + ">\r\n"
        "To: " + to_name + " <" + to_address + ">\r\n"
        "Subject: " + subject + "\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n"
        "\r\n"
        + qp_encode(html_body)
        + "\r\n.\r\n";

    if (SSL_write(ssl, msg.c_str(), (int)msg.size()) <= 0)
        return "failed writing message body";

    std::string resp = ssl_line(ssl, rbuf);
    while (resp.size() >= 4 && resp[3] == '-') resp = ssl_line(ssl, rbuf);
    if (resp.substr(0, 3) != "250") return "message rejected: " + resp;

    ssl_cmd(ssl, rbuf, "QUIT", "221");
    return "";
}
#endif // METIS_TLS

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
std::string smtp_send(
    const std::string& host,
    int                port,
    const std::string& smtp_user,
    const std::string& smtp_pass,
    const std::string& from_address,
    const std::string& from_name,
    const std::string& to_address,
    const std::string& to_name,
    const std::string& subject,
    const std::string& html_body)
{
#ifndef METIS_TLS
    return "email requires TLS build (METIS_TLS)";
#else
    if (host.empty() || smtp_user.empty() || smtp_pass.empty() || from_address.empty())
        return "email config incomplete (smtp_host/smtp_user/smtp_pass/from_address)";

    // Resolve host
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_s = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res) != 0 || !res)
        return "DNS resolution failed for " + host;

    smtp_sock_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == INVALID_SOCKET) { freeaddrinfo(res); return "socket() failed"; }
    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        CLOSESOCK(fd); freeaddrinfo(res);
        return "connect() failed to " + host + ":" + port_s;
    }
    freeaddrinfo(res);

    // Plain TCP greeting + STARTTLS upgrade
    char tmp[4096];
    // Read greeting on plain socket
    auto plain_readline = [&](smtp_sock_t s) -> std::string {
        std::string line;
        char c;
        while (true) {
            int n = recv(s, &c, 1, 0);
            if (n <= 0) break;
            if (c == '\n') break;
            if (c != '\r') line += c;
        }
        return line;
    };
    auto plain_cmd = [&](smtp_sock_t s, const std::string& cmd,
                         const std::string& code) -> bool {
        if (!cmd.empty()) {
            std::string c = cmd + "\r\n";
            send(s, c.c_str(), (int)c.size(), 0);
        }
        std::string line = plain_readline(s);
        while (line.size() >= 4 && line[3] == '-') line = plain_readline(s);
        return line.substr(0, code.size()) == code;
    };

    // Greeting
    plain_readline(fd); // consume greeting
    // EHLO
    if (!plain_cmd(fd, "EHLO localhost", "250")) {
        CLOSESOCK(fd); return "EHLO failed (plain)";
    }
    // STARTTLS
    if (!plain_cmd(fd, "STARTTLS", "220")) {
        CLOSESOCK(fd); return "STARTTLS rejected";
    }

    // Upgrade to TLS
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { CLOSESOCK(fd); return "SSL_CTX_new failed"; }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, (int)fd);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl); SSL_CTX_free(ctx); CLOSESOCK(fd);
        return "SSL_connect failed";
    }

    // Re-EHLO after STARTTLS
    std::string rbuf;
    if (!ssl_cmd(ssl, rbuf, "EHLO localhost", "250")) {
        SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); CLOSESOCK(fd);
        return "EHLO failed (TLS)";
    }

    std::string err = do_smtp(ssl, smtp_user, smtp_pass,
                               from_address, from_name,
                               to_address, to_name,
                               subject, html_body);

    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx);
    CLOSESOCK(fd);
    return err;
#endif
}

} // namespace metis::antique
