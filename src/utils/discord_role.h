#pragma once

#include "env.h"
#include <curl/curl.h>

namespace pt {

inline bool grantDiscordRole(const std::string &discordId) {
    const auto botToken = env("DISCORD_BOT_TOKEN");
    const auto guildId = env("DISCORD_GUILD_ID");
    const auto roleId = env("DISCORD_VERIFIED_ROLE_ID");
    if (botToken.empty() || guildId.empty() || roleId.empty() || discordId.empty()) return false;

    CURL *curl = curl_easy_init();
    if (!curl) return false;
    const auto url = "https://discord.com/api/v10/guilds/" + guildId + "/members/" + discordId + "/roles/" + roleId;
    struct curl_slist *headers = curl_slist_append(nullptr, ("Authorization: Bot " + botToken).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    const auto result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result == CURLE_OK && (status == 204 || status == 200);
}

} // namespace pt
