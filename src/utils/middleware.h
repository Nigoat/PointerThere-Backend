/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include "env.h"

namespace pt {

inline void addCorsHeaders(const drogon::HttpResponsePtr &resp) {
    if (!resp) return;
    const auto origin = pt::env("ALLOWED_ORIGIN", "*");
    resp->addHeader("Access-Control-Allow-Origin", origin);
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
    resp->addHeader("Access-Control-Max-Age", "86400");
}

} // namespace pt
