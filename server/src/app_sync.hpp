#pragma once
#include "router.hpp"
#include "pson.hpp"

namespace metis::antique {

void register_sync_routes(Router& r, const Pson& cfg);
void start_sync_threads(const Pson& cfg);

} // namespace metis::antique
