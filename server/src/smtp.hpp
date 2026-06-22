#pragma once
#include <string>

namespace metis::antique {

// Send an HTML email via SMTP STARTTLS.
// Returns empty string on success, error message on failure.
// Uses the OpenSSL already linked into the binary (METIS_TLS).
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
    const std::string& html_body);

} // namespace metis::antique
