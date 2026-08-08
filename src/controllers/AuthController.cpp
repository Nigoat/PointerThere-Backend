/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include "AuthController.h"
#include "../utils/env.h"
#include "../utils/jwt_helper.h"
#include "../utils/turnstile.h"
#include <drogon/drogon.h>
#include <bcrypt/BCrypt.hpp>
#include <sstream>
#include <random>
#include <thread>

using namespace pt::controllers;
using namespace drogon;

static std::string generateToken(size_t length = 32) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    std::string token;
    token.reserve(length);
    for (size_t i = 0; i < length; ++i)
        token += chars[dis(gen)];
    return token;
}

static Json::Value buildUserResponse(const orm::Row &row, const std::string &jwtToken) {
    Json::Value user;
    user["id"]       = row["id"].as<Json::Int64>();
    user["username"] = row["username"].as<std::string>();
    user["email"]    = row["email"].as<std::string>();
    user["avatar_url"] = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>();
    user["token"]    = jwtToken;
    user["two_factor_required"] = row["two_factor_enabled"].as<bool>() ? true : false;
    return user;
}

static orm::DbClientPtr getDatabaseClient() {
    auto db = drogon::app().getDbClient("default");
    if (!db) db = drogon::app().getDbClient();
    if (!db) db = drogon::app().getFastDbClient("default");
    if (!db) db = drogon::app().getFastDbClient();
    return db;
}

static std::string generateNumericCode(size_t length = 6) {
    static const char digits[] = "0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9);
    std::string code;
    code.reserve(length);
    for (size_t i = 0; i < length; ++i)
        code += digits[dis(gen)];
    return code;
}

void AuthController::doRegister(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body     = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto email    = (*body)["email"].asString();
    auto password = (*body)["password"].asString();
    auto username = (*body)["username"].asString();
    auto cfToken  = (*body)["turnstile_token"].asString();

    if (email.empty() || password.size() < 8 || username.size() < 3) {
        cb(pt::errorResponse(k400BadRequest, "Invalid registration data."));
        return;
    }

    const std::string cfSecret = pt::env("TURNSTILE_SECRET_KEY");
    pt::verifyTurnstile(cfToken, cfSecret, [=, cb = std::move(cb)](bool valid) mutable {
        if (!valid) { cb(pt::errorResponse(k400BadRequest, "CAPTCHA verification failed.")); return; }

        auto db = getDatabaseClient();
        if (!db) {
            cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized."));
            return;
        }

        db->execSqlAsync(
            "SELECT id FROM users WHERE email = $1 OR username = $2 LIMIT 1",
            [=, cb = std::move(cb)](const orm::Result &res) mutable {
                if (!res.empty()) {
                    cb(pt::errorResponse(k409Conflict, "Email or username already exists."));
                    return;
                }

                std::thread([=, cb = std::move(cb)]() mutable {
                    std::string hash;
                    try {
                        hash = BCrypt::generateHash(password);
                    } catch (const std::exception &e) {
                        drogon::app().getLoop()->runInLoop([cb = std::move(cb), err = std::string(e.what())] {
                            cb(pt::errorResponse(k500InternalServerError, "Password hashing error: " + err));
                        });
                        return;
                    }

                    std::string verifyCode = generateNumericCode(6);
                    std::cout << "[PointerThere Auth] Generated verification code for " << email << ": " << verifyCode << "\n";

                    drogon::app().getLoop()->runInLoop([=, cb = std::move(cb)]() mutable {
                        auto db2 = getDatabaseClient();
                        if (!db2) {
                            cb(pt::errorResponse(k500InternalServerError, "Database client lost."));
                            return;
                        }

                        db2->execSqlAsync(
                            "INSERT INTO users (username, email, password_hash, email_verify_token) "
                            "VALUES ($1, $2, $3, $4) RETURNING id, username, email, avatar_url, two_factor_enabled",
                            [=, cb = std::move(cb)](const orm::Result &res2) mutable {
                                if (res2.empty()) {
                                    cb(pt::errorResponse(k500InternalServerError, "Failed to create account."));
                                    return;
                                }
                                auto row = res2[0];
                                auto token = pt::JwtHelper::instance().generate(
                                    std::to_string(row["id"].as<Json::Int64>()),
                                    row["email"].as<std::string>());
                                cb(pt::okResponse(buildUserResponse(row, token)));
                            },
                            [cb](const drogon::orm::DrogonDbException &e) mutable {
                                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                            },
                            username, email, hash, verifyCode);
                    });
                }).detach();
            },
            [cb](const drogon::orm::DrogonDbException &e) mutable {
                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
            },
            email, username);
    });
}

void AuthController::doLogin(const HttpRequestPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body    = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto email    = (*body)["email"].asString();
    auto password = (*body)["password"].asString();
    auto cfToken  = (*body)["turnstile_token"].asString();

    if (email.empty() || password.empty()) {
        cb(pt::errorResponse(k400BadRequest, "Email and password are required."));
        return;
    }

    const std::string cfSecret = pt::env("TURNSTILE_SECRET_KEY");
    pt::verifyTurnstile(cfToken, cfSecret, [=, cb = std::move(cb)](bool valid) mutable {
        if (!valid) { cb(pt::errorResponse(k400BadRequest, "CAPTCHA verification failed.")); return; }

        auto db = getDatabaseClient();
        if (!db) {
            cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized."));
            return;
        }

        db->execSqlAsync(
            "SELECT id, username, email, avatar_url, password_hash, two_factor_enabled, is_banned "
            "FROM users WHERE email = $1 LIMIT 1",
            [=, cb = std::move(cb)](const orm::Result &res) mutable {
                if (res.empty()) {
                    Json::Value j;
                    j["error"] = "NEW_ACCOUNT";
                    auto resp = HttpResponse::newHttpJsonResponse(j);
                    resp->setStatusCode(k404NotFound);
                    cb(resp);
                    return;
                }
                auto row = res[0];
                if (row["is_banned"].as<bool>()) {
                    cb(pt::errorResponse(k403Forbidden, "Account is banned."));
                    return;
                }
                auto storedHash = row["password_hash"].as<std::string>();

                std::thread([=, cb = std::move(cb), row]() mutable {
                    bool validPassword = false;
                    try {
                        validPassword = BCrypt::validatePassword(password, storedHash);
                    } catch (...) {}

                    drogon::app().getLoop()->runInLoop([=, cb = std::move(cb)]() mutable {
                        if (!validPassword) {
                            cb(pt::errorResponse(k401Unauthorized, "Invalid credentials."));
                            return;
                        }
                        auto token = pt::JwtHelper::instance().generate(
                            std::to_string(row["id"].as<Json::Int64>()),
                            row["email"].as<std::string>());
                        cb(pt::okResponse(buildUserResponse(row, token)));
                    });
                }).detach();
            },
            [cb](const drogon::orm::DrogonDbException &e) mutable {
                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
            },
            email);
    });
}

void AuthController::doOAuth(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body       = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto provider    = (*body)["provider"].asString();
    auto providerId  = (*body)["provider_id"].asString();
    auto email       = (*body)["email"].asString();
    auto name        = (*body)["name"].asString();
    auto avatarUrl   = (*body)["avatar_url"].asString();

    if (provider.empty() || providerId.empty() || email.empty()) {
        cb(pt::errorResponse(k400BadRequest, "Missing OAuth fields."));
        return;
    }

    std::string idColumn = (provider == "discord") ? "discord_id" : "google_id";
    auto db = getDatabaseClient();
    if (!db) {
        cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized."));
        return;
    }

    db->execSqlAsync(
        "SELECT id, username, email, avatar_url, two_factor_enabled FROM users WHERE " + idColumn + " = $1 LIMIT 1",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (!res.empty()) {
                auto row = res[0];
                auto token = pt::JwtHelper::instance().generate(
                    std::to_string(row["id"].as<Json::Int64>()),
                    row["email"].as<std::string>());
                cb(pt::okResponse(buildUserResponse(row, token)));
                return;
            }
            auto db2 = getDatabaseClient();
            if (!db2) {
                cb(pt::errorResponse(k500InternalServerError, "Database connection lost."));
                return;
            }
            std::string sql = (provider == "discord")
                ? "INSERT INTO users (username, email, avatar_url, discord_id, email_verified) "
                  "VALUES ($1, $2, $3, $4, TRUE) "
                  "ON CONFLICT (email) DO UPDATE SET discord_id = $4, avatar_url = COALESCE(users.avatar_url, $3) "
                  "RETURNING id, username, email, avatar_url, two_factor_enabled"
                : "INSERT INTO users (username, email, avatar_url, google_id, email_verified) "
                  "VALUES ($1, $2, $3, $4, TRUE) "
                  "ON CONFLICT (email) DO UPDATE SET google_id = $4, avatar_url = COALESCE(users.avatar_url, $3) "
                  "RETURNING id, username, email, avatar_url, two_factor_enabled";

            db2->execSqlAsync(
                sql,
                [=, cb = std::move(cb)](const orm::Result &res2) mutable {
                    if (res2.empty()) {
                        cb(pt::errorResponse(k500InternalServerError, "Failed to create OAuth user."));
                        return;
                    }
                    auto row = res2[0];
                    auto token = pt::JwtHelper::instance().generate(
                        std::to_string(row["id"].as<Json::Int64>()),
                        row["email"].as<std::string>());
                    cb(pt::okResponse(buildUserResponse(row, token)));
                },
                [cb](const drogon::orm::DrogonDbException &e) mutable {
                    cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                },
                name, email, avatarUrl, providerId);
        },
        [cb](const drogon::orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        providerId);
}

void AuthController::doLogout(const HttpRequestPtr &,
                              std::function<void(const HttpResponsePtr &)> &&cb) {
    Json::Value j;
    j["ok"] = true;
    cb(pt::okResponse(j));
}

void AuthController::verifyEmail(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body  = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto token = (*body)["token"].asString();
    auto email = (*body)["email"].asString();

    if (token.empty()) {
        cb(pt::errorResponse(k400BadRequest, "Verification token is required."));
        return;
    }

    auto db = getDatabaseClient();
    if (!db) { cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized.")); return; }

    if (email.empty()) {
        db->execSqlAsync(
            "UPDATE users SET email_verified = TRUE, email_verify_token = NULL "
            "WHERE email_verify_token = $1 RETURNING id",
            [cb](const orm::Result &res) mutable {
                if (res.empty()) { cb(pt::errorResponse(k400BadRequest, "Invalid or expired verification code.")); return; }
                Json::Value j; j["ok"] = true;
                cb(pt::okResponse(j));
            },
            [cb](const drogon::orm::DrogonDbException &e) mutable {
                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
            },
            token);
    } else {
        db->execSqlAsync(
            "UPDATE users SET email_verified = TRUE, email_verify_token = NULL "
            "WHERE (email_verify_token = $1 OR email = $2) AND (email_verify_token = $1 OR email_verify_token IS NULL) RETURNING id",
            [cb](const orm::Result &res) mutable {
                if (res.empty()) { cb(pt::errorResponse(k400BadRequest, "Invalid or expired verification code.")); return; }
                Json::Value j; j["ok"] = true;
                cb(pt::okResponse(j));
            },
            [cb](const drogon::orm::DrogonDbException &e) mutable {
                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
            },
            token, email);
    }
}

void AuthController::forgotPassword(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body  = req->getJsonObject();
    auto email = (*body)["email"].asString();
    auto resetToken = generateToken(48);
    auto db = getDatabaseClient();
    if (!db) { cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized.")); return; }
    db->execSqlAsync(
        "UPDATE users SET reset_token = $1, reset_token_expires = NOW() + INTERVAL '1 hour' WHERE email = $2",
        [](const orm::Result &) {},
        [](const drogon::orm::DrogonDbException &) {},
        resetToken, email);
    Json::Value j; j["ok"] = true;
    cb(pt::okResponse(j));
}

void AuthController::resetPassword(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body     = req->getJsonObject();
    auto token    = (*body)["token"].asString();
    auto password = (*body)["password"].asString();
    if (password.size() < 8) { cb(pt::errorResponse(k400BadRequest, "Password too short.")); return; }
    auto db = getDatabaseClient();
    if (!db) { cb(pt::errorResponse(k500InternalServerError, "Database connection not initialized.")); return; }
    
    std::thread([=, cb = std::move(cb)]() mutable {
        std::string hash;
        try {
            hash = BCrypt::generateHash(password);
        } catch (...) {
            drogon::app().getLoop()->runInLoop([cb = std::move(cb)] {
                cb(pt::errorResponse(k500InternalServerError, "Failed to hash password."));
            });
            return;
        }

        drogon::app().getLoop()->runInLoop([=, cb = std::move(cb)]() mutable {
            auto db2 = getDatabaseClient();
            if (!db2) { cb(pt::errorResponse(k500InternalServerError, "Database connection lost.")); return; }
            db2->execSqlAsync(
                "UPDATE users SET password_hash = $1, reset_token = NULL, reset_token_expires = NULL "
                "WHERE reset_token = $2 AND reset_token_expires > NOW() RETURNING id",
                [cb](const orm::Result &res) mutable {
                    if (res.empty()) { cb(pt::errorResponse(k400BadRequest, "Invalid or expired reset token.")); return; }
                    Json::Value j; j["ok"] = true;
                    cb(pt::okResponse(j));
                },
                [cb](const drogon::orm::DrogonDbException &e) mutable {
                    cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                },
                hash, token);
        });
    }).detach();
}
