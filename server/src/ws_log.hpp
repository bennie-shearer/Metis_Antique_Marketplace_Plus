#pragma once
#include <string>
#include <map>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>

#ifdef METIS_TLS
#include <openssl/ssl.h>
#endif

namespace metis::antique {

// Thread-safe broadcast hub for log lines -> WebSocket clients
class LogBroadcaster {
public:
    static LogBroadcaster& instance();
    void publish(const std::string& line);
    // Subscribe: returns unsubscribe handle (index)
    int subscribe(std::function<void(const std::string&)> cb);
    void unsubscribe(int id);
private:
    LogBroadcaster() = default;
    std::mutex mu_;
    int next_id_ = 0;
    std::map<int, std::function<void(const std::string&)>> subs_;
};

// Perform RFC 6455 WebSocket handshake on socket fd
// Returns true if successful
bool ws_handshake(int fd, const std::string& raw_request);

// Send a WebSocket text frame
bool ws_send(int fd, const std::string& msg);

// Handle /ws/logs connection -- streams log lines until client disconnects
void handle_ws_logs(int fd, const std::string& raw_request, const std::string& user_role);

#ifdef METIS_TLS
// TLS variants -- same logic, all I/O via SSL_read/SSL_write
bool ws_handshake_ssl(SSL* ssl, const std::string& raw_request);
bool ws_send_ssl(SSL* ssl, const std::string& msg);
void handle_ws_logs_ssl(SSL* ssl, int fd, const std::string& raw_request, const std::string& user_role);
#endif

} // namespace metis::antique
