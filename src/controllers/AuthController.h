/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 */

#pragma once

#include <drogon/HttpController.h>

namespace pt::controllers {

/**
 * Handles all authentication endpoints:
 *   POST /api/auth/register
 *   POST /api/auth/login
 *   POST /api/auth/oauth
 *   POST /api/auth/logout
 *   POST /api/auth/verify-email
 *   POST /api/auth/forgot-password
 *   POST /api/auth/reset-password
 */
class AuthController : public drogon::HttpController<AuthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::doRegister,      "/api/auth/register",        drogon::Post);
    ADD_METHOD_TO(AuthController::doLogin,         "/api/auth/login",           drogon::Post);
    ADD_METHOD_TO(AuthController::doOAuth,         "/api/auth/oauth",           drogon::Post);
    ADD_METHOD_TO(AuthController::doLogout,        "/api/auth/logout",          drogon::Post);
    ADD_METHOD_TO(AuthController::verifyEmail,     "/api/auth/verify-email",    drogon::Post);
    ADD_METHOD_TO(AuthController::forgotPassword,  "/api/auth/forgot-password", drogon::Post);
    ADD_METHOD_TO(AuthController::resetPassword,   "/api/auth/reset-password",  drogon::Post);
    METHOD_LIST_END

    void doRegister(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void doLogin   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void doOAuth   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void doLogout  (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void verifyEmail   (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void forgotPassword(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
    void resetPassword (const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace pt::controllers
