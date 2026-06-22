#pragma once
#include "router.hpp"
#include "pson.hpp"

namespace metis::antique {

void register_pos_routes(Router& r, const Pson& cfg);

// Send ESC/POS receipt to network printer
// Returns empty string on success, error message on failure
std::string pos_print_receipt(const Pson& cfg, const std::string& receipt_text);

} // namespace metis::antique
