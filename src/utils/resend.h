/* Minimal Resend email helper. */
#pragma once

#include "env.h"
#include <curl/curl.h>

namespace pt {

inline bool sendResendEmail(const std::string &recipient, const std::string &subject, const std::string &html) {
    const auto apiKey = env("RESEND_API_KEY");
    const auto from = env("FROM_EMAIL");
    if (apiKey.empty() || from.empty()) return false;

    Json::Value payload;
    payload["from"] = from;
    payload["to"] = Json::Value(Json::arrayValue);
    payload["to"].append(recipient);
    payload["subject"] = subject;
    payload["html"] = html;
    const auto body = Json::writeString(Json::StreamWriterBuilder(), payload);

    CURL *curl = curl_easy_init();
    if (!curl) return false;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + apiKey).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.resend.com/emails");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    const auto result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && status >= 200 && status < 300;
}

} // namespace pt
