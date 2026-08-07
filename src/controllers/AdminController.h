/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once
#include <drogon/HttpController.h>

namespace pt::controllers {

class AdminController : public drogon::HttpController<AdminController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AdminController::login,           "/api/admin/login",               drogon::Post);
    ADD_METHOD_TO(AdminController::getStats,        "/api/admin/stats",               drogon::Get);
    ADD_METHOD_TO(AdminController::getPending,      "/api/admin/records/pending",     drogon::Get);
    ADD_METHOD_TO(AdminController::updateRecord,    "/api/admin/records/{id}",        drogon::Patch);
    ADD_METHOD_TO(AdminController::getUsers,        "/api/admin/users",               drogon::Get);
    ADD_METHOD_TO(AdminController::updateUser,      "/api/admin/users/{id}",          drogon::Patch);
    ADD_METHOD_TO(AdminController::banUser,         "/api/admin/users/{id}/ban",      drogon::Post);
    ADD_METHOD_TO(AdminController::unbanUser,       "/api/admin/users/{id}/ban",      drogon::Delete);
    ADD_METHOD_TO(AdminController::timeoutUser,     "/api/admin/users/{id}/timeout",  drogon::Post);
    ADD_METHOD_TO(AdminController::getLevels,       "/api/admin/levels",              drogon::Get);
    ADD_METHOD_TO(AdminController::addLevel,        "/api/admin/levels",              drogon::Post);
    ADD_METHOD_TO(AdminController::updateLevel,     "/api/admin/levels/{id}",         drogon::Patch);
    ADD_METHOD_TO(AdminController::deleteLevel,     "/api/admin/levels/{id}",         drogon::Delete);
    ADD_METHOD_TO(AdminController::getAppeals,      "/api/admin/appeals",             drogon::Get);
    ADD_METHOD_TO(AdminController::updateAppeal,    "/api/admin/appeals/{id}",        drogon::Patch);
    ADD_METHOD_TO(AdminController::getSettings,     "/api/admin/settings",            drogon::Get);
    ADD_METHOD_TO(AdminController::updateSettings,  "/api/admin/settings",            drogon::Patch);
    METHOD_LIST_END

    void login         (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void getStats      (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void getPending    (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateRecord  (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void getUsers      (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateUser    (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void banUser       (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void unbanUser     (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void timeoutUser   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void getLevels     (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void addLevel      (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateLevel   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void deleteLevel   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void getAppeals    (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateAppeal  (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&, long long id);
    void getSettings   (const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
    void updateSettings(const drogon::HttpRequestPtr &, std::function<void(const drogon::HttpResponsePtr &)> &&);
};

}
