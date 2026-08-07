/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <optional>

namespace pt {

inline std::string env(const std::string &key, const std::string &default_val = "") {
    const char *val = std::getenv(key.c_str());
    return val ? std::string(val) : default_val;
}

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

inline drogon::HttpResponsePtr okResponse(const Json::Value &body) {
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(drogon::k200OK);
    return resp;
}

}
