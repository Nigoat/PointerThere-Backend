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
#include <iostream>
#include <curl/curl.h>

namespace pt {

inline void verifyTurnstile(const std::string &token,
                             const std::string &secret,
                             std::function<void(bool)> callback) {
    if (token.empty() || secret.empty()) {
        std::cerr << "[Turnstile] Token or secret is empty!\n";
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

    // 5-second safety guard timeout
    drogon::app().getLoop()->runAfter(5.0, [safeCallback]() mutable {
        std::cerr << "[Turnstile] Verification timed out after 5s.\n";
        safeCallback(false);
    });

    std::thread([token, secret, safeCallback]() mutable {
        CURL *curl = curl_easy_init();
        if (!curl) {
            std::cerr << "[Turnstile] Failed to init curl!\n";
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
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); // Force IPv4 (fast DNS)
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "PointerThere-Backend/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "[Turnstile] libcurl error: " << curl_easy_strerror(res) << "\n";
            safeCallback(false);
            return;
        }

        std::cout << "[Turnstile] Cloudflare response: " << responseBody << "\n";

        Json::CharReaderBuilder builder;
        Json::Value root;
        std::istringstream ss(responseBody);
        std::string errs;
        if (Json::parseFromStream(builder, ss, &root, &errs) && root.isMember("success")) {
            bool isSuccess = root["success"].asBool();
            if (!isSuccess && root.isMember("error-codes")) {
                std::cerr << "[Turnstile] Cloudflare returned error codes: " << root["error-codes"].toStyledString() << "\n";
            }
            safeCallback(isSuccess);
        } else {
            std::cerr << "[Turnstile] Failed to parse JSON response from Cloudflare.\n";
            safeCallback(false);
        }
    }).detach();
}

}
