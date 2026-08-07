/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <drogon/drogon.h>
#include <string>
#include <functional>
#include <curl/curl.h>

namespace pt {

inline void verifyTurnstile(const std::string &token,
                             const std::string &secret,
                             std::function<void(bool)> callback) {
    drogon::app().getLoop()->runInLoop([token, secret, cb = std::move(callback)] {
        CURL *curl = curl_easy_init();
        if (!curl) { cb(false); return; }

        std::string postData = "secret=" + secret + "&response=" + token;
        std::string responseBody;

        curl_easy_setopt(curl, CURLOPT_URL, "https://challenges.cloudflare.com/turnstile/v0/siteverify");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *userdata) -> size_t {
            static_cast<std::string *>(userdata)->append(ptr, nmemb);
            return nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) { cb(false); return; }

        Json::CharReaderBuilder builder;
        Json::Value root;
        std::istringstream ss(responseBody);
        std::string errs;
        if (Json::parseFromStream(builder, ss, &root, &errs)) {
            cb(root["success"].asBool());
        } else {
            cb(false);
        }
    });
}

}
