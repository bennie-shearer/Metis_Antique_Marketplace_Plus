#pragma once
#include "router.hpp"
#include "pson.hpp"
#include <string>

namespace metis::antique {

void register_auth_routes(Router& r, const Pson& cfg);
bool session_valid(const std::string& token, std::string& user_id);
std::string user_role(const std::string& user_id);

} // namespace metis::antique
