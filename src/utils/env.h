/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <optional>

namespace pt {

/**
 * Read an environment variable.
 * @param key     Environment variable name
 * @param default_val  Returned if the variable is not set (empty optional = no default)
 */
inline std::string env(const std::string &key, const std::string &default_val = "") {
    const char *val = std::getenv(key.c_str());
    return val ? std::string(val) : default_val;
}

/**
 * Convenience: build a standard JSON error response.
 */
inline drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode code,
                                              const std::string &message) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        Json::Value([&] {
            Json::Value j;
            j["error"] = message;
            return j;
        }())
    );
    resp->setStatusCode(code);
    return resp;
}

/**
 * Convenience: build a standard JSON success response.
 */
inline drogon::HttpResponsePtr okResponse(const Json::Value &body) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

} // namespace pt
