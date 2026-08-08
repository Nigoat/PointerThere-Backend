#pragma once

#include "env.h"
#include <curl/curl.h>
#include <iostream>

namespace pt {

inline bool grantDiscordRole(const std::string &discordId, long &statusCode) {
    const auto botToken = env("DISCORD_BOT_TOKEN");
    const auto guildId = env("DISCORD_GUILD_ID");
    const auto roleId = env("DISCORD_VERIFIED_ROLE_ID");
    statusCode = 0;
    if (botToken.empty() || guildId.empty() || roleId.empty() || discordId.empty()) return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;
    const auto url = "https://discord.com/api/v10/guilds/" + guildId + "/members/" + discordId + "/roles/" + roleId;
    struct curl_slist *headers = curl_slist_append(nullptr, ("Authorization: Bot " + botToken).c_str());
    headers = curl_slist_append(headers, "Content-Length: 0");
    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char *ptr, size_t, size_t nmemb, void *userdata) -> size_t {
        static_cast<std::string *>(userdata)->append(ptr, nmemb);
        return nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    const auto result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || (statusCode != 204 && statusCode != 200)) {
        std::cerr << "[Discord Verification] Role request failed (curl=" << curl_easy_strerror(result)
                  << ", HTTP=" << statusCode << "): " << responseBody << "\n";
        return false;
    }
    return true;
}

} // namespace pt
