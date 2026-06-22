#pragma once
#include "router.hpp"
#include "pson.hpp"
namespace metis::antique {
void register_photos_routes(Router& r, const Pson& cfg);
} // namespace metis::antique
