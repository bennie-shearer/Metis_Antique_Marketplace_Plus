#pragma once
#include "router.hpp"
#include "pson.hpp"
#include <string>
#include <atomic>

#ifdef METIS_TLS
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

namespace metis::antique {

class Server {
public:
    explicit Server(const Pson& cfg);
    void run();
    void stop() { running_ = false; }

private:
    void handle_client(int fd);
    HttpRequest  parse_request(const std::string& raw);
    std::string  build_response(const HttpResponse& resp);
    std::string  serve_static(const std::string& path);
    std::string  mime_type(const std::string& ext);

#ifdef METIS_TLS
    void handle_client_ssl(SSL* ssl, int fd);
    void run_tls_acceptor();
#endif

    Pson              cfg_;
    Router            router_;
    std::string       web_root_;
    std::string       photos_dir_;
    std::atomic<bool> running_{true};
    int               port_;
    int               tls_port_{0};

#ifdef METIS_TLS
    SSL_CTX*          ssl_ctx_{nullptr};
    std::string       tls_version_;
    std::string       tls_cipher_;
    std::string       tls_group_;
#endif
};

} // namespace metis::antique
