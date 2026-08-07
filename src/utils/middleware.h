/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <functional>
#include <optional>
#include "jwt_helper.h"
#include "env.h"

namespace pt {

inline drogon::AdviceCallback requireAuth() {
    return [](const drogon::HttpRequestPtr &req,
              drogon::AdviceNextCallback &&next) {
        auto tokenOpt = JwtHelper::extractBearer(req);
        if (!tokenOpt) {
            auto resp = errorResponse(drogon::k401Unauthorized, "Authentication required.");
            return next(req, [resp](const drogon::HttpResponsePtr &) { return resp; });
        }
        auto payload = JwtHelper::instance().verify(*tokenOpt);
        if (!payload) {
            auto resp = errorResponse(drogon::k401Unauthorized, "Invalid or expired token.");
            return next(req, [resp](const drogon::HttpResponsePtr &) { return resp; });
        }
        req->addParameter("user_id", payload->user_id);
        req->addParameter("email",   payload->email);
        next(req);
    };
}

inline drogon::AdviceCallback requireApiKey() {
    return [](const drogon::HttpRequestPtr &req,
              drogon::AdviceNextCallback &&next) {
        auto key = req->getParameter("api_key");
        if (key.empty()) {
            auto resp = errorResponse(drogon::k401Unauthorized, "API key required.");
            return next(req, [resp](const drogon::HttpResponsePtr &) { return resp; });
        }
        next(req);
    };
}

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
