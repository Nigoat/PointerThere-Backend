/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <curl/curl.h>

namespace pt {

inline void verifyTurnstile(const std::string &token,
                             const std::string &secret,
                             std::function<void(bool)> callback) {
    if (token.empty() || secret.empty()) {
        callback(false);
        return;
    }

    auto called = std::make_shared<std::atomic<bool>>(false);
    auto safeCallback = [cb = std::move(callback), called](bool result) mutable {
        if (!called->exchange(true)) {
            drogon::app().getLoop()->runInLoop([cb = std::move(cb), result] {
                cb(result);
            });
        }
    };

    // Hard 3-second timeout guard to prevent Railway 30s proxy timeout
    drogon::app().getLoop()->runAfter(3.0, [safeCallback]() mutable {
        safeCallback(false);
    });

    // Run libcurl on an isolated background worker thread
    std::thread([token, secret, safeCallback]() mutable {
        CURL *curl = curl_easy_init();
        if (!curl) {
            safeCallback(false);
            return;
        }

        std::string postData = "secret=" + secret + "&response=" + token;
        std::string responseBody;

        curl_easy_setopt(curl, CURLOPT_URL, "https://challenges.cloudflare.com/turnstile/v0/siteverify");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *userdata) -> size_t {
            static_cast<std::string *>(userdata)->append(ptr, nmemb);
            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            safeCallback(false);
            return;
        }

        Json::CharReaderBuilder builder;
        Json::Value root;
        std::istringstream ss(responseBody);
        std::string errs;
        if (Json::parseFromStream(builder, ss, &root, &errs) && root.isMember("success")) {
            safeCallback(root["success"].asBool());
        } else {
            safeCallback(false);
        }
    }).detach();
}

}
