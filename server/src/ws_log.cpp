#include "ws_log.hpp"
#include "logger.hpp"
#include <map>
#include <thread>
#include <chrono>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
#endif

namespace metis::antique {

// -----------------------------------------------------------------------
// SHA-1 (RFC 3174) -- needed for WebSocket handshake key
// Self-contained, no external dependencies
// -----------------------------------------------------------------------
static void sha1(const uint8_t* msg, size_t len, uint8_t out[20]) {
    uint32_t h0=0x67452301,h1=0xEFCDAB89,h2=0x98BADCFE,h3=0x10325476,h4=0xC3D2E1F0;
    // Padding
    std::vector<uint8_t> padded(msg, msg+len);
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) padded.push_back(0x00);
    uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i=7;i>=0;--i) padded.push_back(static_cast<uint8_t>(bits>>(i*8)));
    // Process blocks
    for (size_t i=0;i<padded.size();i+=64) {
        uint32_t w[80];
        for (int j=0;j<16;++j)
            w[j]=(padded[i+j*4]<<24)|(padded[i+j*4+1]<<16)|(padded[i+j*4+2]<<8)|padded[i+j*4+3];
        for (int j=16;j<80;++j) {
            uint32_t x=w[j-3]^w[j-8]^w[j-14]^w[j-16];
            w[j]=(x<<1)|(x>>31);
        }
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
        for (int j=0;j<80;++j) {
            uint32_t f,k;
            if      (j<20){f=(b&c)|(~b&d);k=0x5A827999;}
            else if (j<40){f=b^c^d;        k=0x6ED9EBA1;}
            else if (j<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else           {f=b^c^d;        k=0xCA62C1D6;}
            uint32_t tmp=((a<<5)|(a>>27))+f+e+k+w[j];
            e=d;d=c;c=(b<<30)|(b>>2);b=a;a=tmp;
        }
        h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;
    }
    uint32_t hh[]={h0,h1,h2,h3,h4};
    for (int i=0;i<5;++i) for (int j=3;j>=0;--j) out[i*4+(3-j)]=(hh[i]>>(j*8))&0xFF;
}

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64_encode(const uint8_t* in, size_t len) {
    std::string out;
    for (size_t i=0;i<len;i+=3) {
        uint32_t v=(in[i]<<16)|((i+1<len?in[i+1]:0)<<8)|(i+2<len?in[i+2]:0);
        out+=B64[(v>>18)&63]; out+=B64[(v>>12)&63];
        out+=(i+1<len?B64[(v>>6)&63]:'=');
        out+=(i+2<len?B64[v&63]:'=');
    }
    return out;
}

// -----------------------------------------------------------------------
// LogBroadcaster
// -----------------------------------------------------------------------
LogBroadcaster& LogBroadcaster::instance() {
    static LogBroadcaster inst;
    return inst;
}

void LogBroadcaster::publish(const std::string& line) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& [id, cb] : subs_) {
        try { cb(line); } catch (...) {}
    }
}

int LogBroadcaster::subscribe(std::function<void(const std::string&)> cb) {
    std::lock_guard<std::mutex> lk(mu_);
    int id = next_id_++;
    subs_[id] = std::move(cb);
    return id;
}

void LogBroadcaster::unsubscribe(int id) {
    std::lock_guard<std::mutex> lk(mu_);
    subs_.erase(id);
}

// -----------------------------------------------------------------------
// WebSocket handshake (RFC 6455)
// -----------------------------------------------------------------------
static std::string extract_header(const std::string& raw, const std::string& key) {
    std::string lkey = key;
    std::transform(lkey.begin(), lkey.end(), lkey.begin(), ::tolower);
    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string lline = line;
        std::transform(lline.begin(), lline.end(), lline.begin(), ::tolower);
        if (lline.starts_with(lkey + ":")) {
            std::string val = line.substr(lkey.size() + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t") + 1);
            return val;
        }
    }
    return "";
}

bool ws_handshake(int fd, const std::string& raw_request) {
    std::string ws_key = extract_header(raw_request, "sec-websocket-key");
    if (ws_key.empty()) return false;
    static const char* MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = ws_key + MAGIC;
    uint8_t digest[20];
    sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size(), digest);
    std::string accept = base64_encode(digest, 20);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return send(fd, response.c_str(), static_cast<int>(response.size()), 0) > 0;
}

// -----------------------------------------------------------------------
// Send a WebSocket text frame (RFC 6455 §5.2)
// -----------------------------------------------------------------------
bool ws_send(int fd, const std::string& msg) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + opcode text
    size_t len = msg.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>((len >> (i*8)) & 0xFF));
    }
    frame.insert(frame.end(), msg.begin(), msg.end());
    return send(fd, reinterpret_cast<const char*>(frame.data()),
                static_cast<int>(frame.size()), 0) > 0;
}

// Check if client sent a close frame (opcode 8) -- non-blocking peek
static bool ws_client_closed(int fd) {
    // Use select() with zero timeout for cross-platform non-blocking check
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(static_cast<unsigned>(fd), &fds);
    struct timeval tv = {0, 0};
    int ready = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (ready <= 0) return false;  // nothing to read = still connected
    char buf[4] = {};
    int n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) return true;  // connection closed or error
    if (n >= 2 && (buf[0] & 0x0F) == 8) return true;  // WebSocket close frame
    return false;
}

// -----------------------------------------------------------------------
// handle_ws_logs: streams log lines to WebSocket client
// -----------------------------------------------------------------------
void handle_ws_logs(int fd, const std::string& raw_request, const std::string& user_role) {
    // Admin-only
    if (user_role != "admin") {
        std::string deny =
            "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        send(fd, deny.c_str(), static_cast<int>(deny.size()), 0);
        closesocket(fd);
        return;
    }

    if (!ws_handshake(fd, raw_request)) { closesocket(fd); return; }

    // Send last 50 lines as history immediately
    auto history = Logger::instance().tail(50, "");
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        std::string json = "{\"ts\":\"" + it->timestamp + "\",\"level\":\"" +
                           it->level + "\",\"msg\":\"" +
                           [&]{ // json-escape message
                               std::string s;
                               for (char c : it->message) {
                                   if (c=='"') s+="\\\"";
                                   else if (c=='\\') s+="\\\\";
                                   else if (c=='\n') s+="\\n";
                                   else s+=c;
                               }
                               return s;
                           }() + "\"}";
        if (!ws_send(fd, json)) { closesocket(fd); return; }
    }
    ws_send(fd, "{\"type\":\"history_end\"}");

    // Subscribe to live log lines
    std::atomic<bool> alive{true};
    int sub_id = LogBroadcaster::instance().subscribe([&](const std::string& line) {
        if (!alive) return;
        // Parse line format: [timestamp] [level] message
        std::string ts, lv, msg;
        if (line.size() > 2 && line[0] == '[') {
            auto t1 = line.find(']');
            if (t1 != std::string::npos) {
                ts = line.substr(1, t1-1);
                auto l1 = line.find('[', t1+1);
                auto l2 = l1 != std::string::npos ? line.find(']', l1+1) : std::string::npos;
                if (l1 != std::string::npos && l2 != std::string::npos) {
                    lv  = line.substr(l1+1, l2-l1-1);
                    msg = l2+2 < line.size() ? line.substr(l2+2) : "";
                }
            }
        } else { msg = line; }
        // Remove trailing newline
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();
        auto je = [](const std::string& s) {
            std::string o;
            for (char c : s) {
                if (c=='"') o+="\\\""; else if (c=='\\') o+="\\\\";
                else if (c=='\n') o+="\\n"; else o+=c;
            }
            return o;
        };
        std::string json = "{\"ts\":\""+je(ts)+"\",\"level\":\""+je(lv)+"\",\"msg\":\""+je(msg)+"\"}";
        if (!ws_send(fd, json)) alive = false;
    });

    // Heartbeat loop -- detect client disconnect
    while (alive) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!alive) break;
        // Send ping frame (opcode 9)
        uint8_t ping[2] = {0x89, 0x00};
        if (send(fd, reinterpret_cast<char*>(ping), 2, 0) <= 0) break;
        if (ws_client_closed(fd)) break;
    }

    LogBroadcaster::instance().unsubscribe(sub_id);
    closesocket(fd);
}

// ---------------------------------------------------------------------------
// TLS variants of WebSocket helpers
// All socket I/O replaced with SSL_read / SSL_write
// ---------------------------------------------------------------------------
#ifdef METIS_TLS
#include <openssl/ssl.h>

bool ws_handshake_ssl(SSL* ssl, const std::string& raw_request) {
    std::string ws_key = extract_header(raw_request, "sec-websocket-key");
    if (ws_key.empty()) return false;
    static const char* MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = ws_key + MAGIC;
    uint8_t digest[20];
    sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size(), digest);
    std::string accept = base64_encode(digest, 20);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return SSL_write(ssl, response.c_str(), static_cast<int>(response.size())) > 0;
}

bool ws_send_ssl(SSL* ssl, const std::string& msg) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + opcode text
    size_t len = msg.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<uint8_t>((len >> (i*8)) & 0xFF));
    }
    frame.insert(frame.end(), msg.begin(), msg.end());
    return SSL_write(ssl, reinterpret_cast<const char*>(frame.data()),
                     static_cast<int>(frame.size())) > 0;
}

static bool ws_client_closed_ssl(SSL* ssl, int fd) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(static_cast<unsigned>(fd), &fds);
    struct timeval tv = {0, 0};
    int ready = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (ready <= 0) return false;
    char buf[4] = {};
    int n = SSL_read(ssl, buf, sizeof(buf));
    if (n <= 0) return true;
    if (n >= 2 && (buf[0] & 0x0F) == 8) return true;
    return false;
}

void handle_ws_logs_ssl(SSL* ssl, int fd, const std::string& raw_request,
                        const std::string& user_role) {
    if (user_role != "admin") {
        std::string deny = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        SSL_write(ssl, deny.c_str(), static_cast<int>(deny.size()));
        SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd);
        return;
    }

    if (!ws_handshake_ssl(ssl, raw_request)) {
        SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
    }

    // Send history
    auto history = Logger::instance().tail(50, "");
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        std::string json = "{\"ts\":\"" + it->timestamp + "\",\"level\":\"" +
                           it->level + "\",\"msg\":\"" +
                           [&]{
                               std::string s;
                               for (char c : it->message) {
                                   if (c=='"') s+="\\\"";
                                   else if (c=='\\') s+="\\\\";
                                   else if (c=='\n') s+="\\n";
                                   else s+=c;
                               }
                               return s;
                           }() + "\"}";
        if (!ws_send_ssl(ssl, json)) {
            SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd); return;
        }
    }
    ws_send_ssl(ssl, "{\"type\":\"history_end\"}");

    // Subscribe to live log lines
    std::atomic<bool> alive{true};
    int sub_id = LogBroadcaster::instance().subscribe([&](const std::string& line) {
        if (!alive) return;
        std::string ts, lv, msg;
        if (line.size() > 2 && line[0] == '[') {
            auto t1 = line.find(']');
            if (t1 != std::string::npos) {
                ts = line.substr(1, t1-1);
                auto l1 = line.find('[', t1+1);
                auto l2 = l1 != std::string::npos ? line.find(']', l1+1) : std::string::npos;
                if (l1 != std::string::npos && l2 != std::string::npos) {
                    lv  = line.substr(l1+1, l2-l1-1);
                    msg = l2+2 < line.size() ? line.substr(l2+2) : "";
                }
            }
        } else { msg = line; }
        if (!msg.empty() && msg.back() == '\n') msg.pop_back();
        auto je = [](const std::string& s) {
            std::string o;
            for (char c : s) {
                if (c=='"') o+="\\\""; else if (c=='\\') o+="\\\\";
                else if (c=='\n') o+="\\n"; else o+=c;
            }
            return o;
        };
        std::string json = "{\"ts\":\""+je(ts)+"\",\"level\":\""+je(lv)+"\",\"msg\":\""+je(msg)+"\"}";
        if (!ws_send_ssl(ssl, json)) alive = false;
    });

    // Heartbeat loop
    while (alive) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!alive) break;
        uint8_t ping[2] = {0x89, 0x00};
        if (SSL_write(ssl, reinterpret_cast<char*>(ping), 2) <= 0) break;
        if (ws_client_closed_ssl(ssl, fd)) break;
    }

    LogBroadcaster::instance().unsubscribe(sub_id);
    SSL_shutdown(ssl); SSL_free(ssl); closesocket(fd);
}
#endif // METIS_TLS

} // namespace metis::antique
