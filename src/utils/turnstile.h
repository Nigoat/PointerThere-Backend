/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpClient.h>
#include <string>
#include <functional>

namespace pt {

inline void verifyTurnstile(const std::string &token,
                             const std::string &secret,
                             std::function<void(bool)> callback) {
    if (token.empty() || secret.empty()) {
        callback(false);
        return;
    }

    auto client = drogon::HttpClient::newHttpClient("https://challenges.cloudflare.com");
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/turnstile/v0/siteverify");
    req->setContentTypeCode(drogon::CT_APPLICATION_X_FORM);
    req->setBody("secret=" + secret + "&response=" + token);

    client->sendRequest(
        req,
        [cb = std::move(callback)](drogon::ReqResult result, const drogon::HttpResponsePtr &resp) {
            if (result != drogon::ReqResult::Ok || !resp) {
                cb(false);
                return;
            }

            auto json = resp->getJsonObject();
            if (json && (*json)["success"].isBool()) {
                cb((*json)["success"].asBool());
            } else {
                cb(false);
            }
        },
        5.0
    );
}

}
