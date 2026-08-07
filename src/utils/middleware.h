/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <functional>
#include <optional>
#include "jwt_helper.h"
#include "env.h"

namespace pt {

class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&fcb,
                  drogon::FilterChainCallback &&fccb) override {
        if (req->method() == drogon::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            addCorsHeaders(resp);
            fcb(resp);
            return;
        }
        fccb();
    }

    static void addCorsHeaders(drogon::HttpResponsePtr &resp) {
        const auto origin = pt::env("ALLOWED_ORIGIN", "*");
        resp->addHeader("Access-Control-Allow-Origin", origin);
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
        resp->addHeader("Access-Control-Max-Age", "86400");
    }
};

}
