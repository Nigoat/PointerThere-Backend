/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once
#include <drogon/HttpController.h>

namespace pt::controllers {

class RecordsController : public drogon::HttpController<RecordsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RecordsController::submitRecord, "/api/records",        drogon::Post);
    ADD_METHOD_TO(RecordsController::getRecent,    "/api/records/recent", drogon::Get);
    METHOD_LIST_END

    void submitRecord(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void getRecent   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
};

class RankingsController : public drogon::HttpController<RankingsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(RankingsController::getRankings, "/api/rankings", drogon::Get);
    METHOD_LIST_END

    void getRankings(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
};

class ApiKeysController : public drogon::HttpController<ApiKeysController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ApiKeysController::getKeys,    "/api/keys",              drogon::Get);
    ADD_METHOD_TO(ApiKeysController::createKey,  "/api/keys",              drogon::Post);
    ADD_METHOD_TO(ApiKeysController::revokeKey,  "/api/keys/{id}",         drogon::Delete);
    ADD_METHOD_TO(ApiKeysController::publicApi,  "/api/public/{key}/demons", drogon::Get);
    METHOD_LIST_END

    void getKeys   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void createKey (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void revokeKey (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void publicApi (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, const std::string &key);
};

class UserController : public drogon::HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UserController::getMe,           "/api/user/me",                drogon::Get);
    ADD_METHOD_TO(UserController::updateMe,        "/api/user/me",                drogon::Patch);
    ADD_METHOD_TO(UserController::deleteMe,        "/api/user/me",                drogon::Delete);
    ADD_METHOD_TO(UserController::uploadAvatar,    "/api/user/avatar",           drogon::Post);
    ADD_METHOD_TO(UserController::changePassword,  "/api/user/change-password",   drogon::Post);
    ADD_METHOD_TO(UserController::setup2FA,        "/api/user/2fa/setup",         drogon::Post);
    ADD_METHOD_TO(UserController::verify2FA,       "/api/user/2fa/verify",        drogon::Post);
    ADD_METHOD_TO(UserController::disable2FA,      "/api/user/2fa",               drogon::Delete);
    METHOD_LIST_END

    void getMe          (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateMe       (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void deleteMe       (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void uploadAvatar   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void changePassword (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void setup2FA       (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void verify2FA      (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void disable2FA     (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
};

class SettingsController : public drogon::HttpController<SettingsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SettingsController::getSiteSettings, "/api/settings/site", drogon::Get);
    METHOD_LIST_END

    void getSiteSettings(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
};

}
