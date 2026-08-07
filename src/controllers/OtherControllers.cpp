/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include "OtherControllers.h"
#include "../utils/env.h"
#include "../utils/jwt_helper.h"
#include "../utils/turnstile.h"
#include <drogon/drogon.h>
#include <bcrypt/BCrypt.hpp>
#include <random>
#include <openssl/sha.h>

using namespace pt::controllers;
using namespace drogon;

void RecordsController::submitRecord(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto levelName  = (*body)["level_name"].asString();
    auto playerName = (*body)["player_name"].asString();
    auto videoUrl   = (*body)["video_url"].asString();
    auto progress   = (*body)["progress"].asInt();
    auto notes      = (*body)["notes"].asString();
    auto cfToken    = (*body)["turnstile_token"].asString();

    if (levelName.empty() || playerName.empty() || videoUrl.empty()) {
        cb(pt::errorResponse(k400BadRequest, "Missing required fields.")); return;
    }
    if (progress < 1 || progress > 100) {
        cb(pt::errorResponse(k400BadRequest, "Progress must be 1-100.")); return;
    }

    const std::string cfSecret = pt::env("TURNSTILE_SECRET_KEY");
    pt::verifyTurnstile(cfToken, cfSecret, [=, cb = std::move(cb)](bool valid) mutable {
        if (!valid) { cb(pt::errorResponse(k400BadRequest, "CAPTCHA verification failed.")); return; }

        auto db = drogon::app().getDbClient();
        db->execSqlAsync(
            "SELECT id FROM demon_levels WHERE name ILIKE $1 LIMIT 1",
            [=, cb = std::move(cb)](const orm::Result &res) mutable {
                if (res.empty()) {
                    cb(pt::errorResponse(k404NotFound, "Level not found on the list.")); return;
                }
                long long levelId = res[0]["id"].as<Json::Int64>();

                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT timeout_until FROM users WHERE username ILIKE $1 AND timeout_until > NOW() LIMIT 1",
                    [=, cb = std::move(cb)](const orm::Result &tout) mutable {
                        if (!tout.empty()) {
                            cb(pt::errorResponse(k403Forbidden, "You are currently timed out from submitting records.")); return;
                        }
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync(
                            "SELECT id FROM records WHERE level_id = $1 AND player_name ILIKE $2 AND status = 'pending' LIMIT 1",
                            [=, cb = std::move(cb)](const orm::Result &dup) mutable {
                                if (!dup.empty()) {
                                    cb(pt::errorResponse(k409Conflict, "You already have a pending submission for this level.")); return;
                                }
                                auto db4 = drogon::app().getDbClient();
                                db4->execSqlAsync(
                                    "INSERT INTO records (level_id, player_name, progress, video_url, notes) "
                                    "VALUES ($1, $2, $3, $4, $5) RETURNING id",
                                    [cb](const orm::Result &r) mutable {
                                        Json::Value j;
                                        j["ok"] = true;
                                        j["record_id"] = r[0]["id"].as<Json::Int64>();
                                        cb(pt::okResponse(j));
                                    },
                                    [cb](const orm::DrogonDbException &e) mutable {
                                        cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                                    },
                                    levelId, playerName, progress, videoUrl, notes);
                            },
                            [cb](const orm::DrogonDbException &e) mutable {
                                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                            },
                            levelId, playerName);
                    },
                    [cb](const orm::DrogonDbException &e) mutable {
                        cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                    },
                    playerName);
            },
            [cb](const orm::DrogonDbException &e) mutable {
                cb(pt::errorResponse(k500InternalServerError, e.base().what()));
            },
            levelName);
    });
}

void RecordsController::getRecent(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb) {
    auto limit  = std::min(20, std::max(1, std::stoi(req->getParameter("limit").empty() ? "4" : req->getParameter("limit"))));
    auto page   = std::max(1, std::stoi(req->getParameter("page").empty() ? "1" : req->getParameter("page")));
    auto offset = (page - 1) * limit;

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT r.id, r.player_name, d.name AS level_name, r.level_id, r.progress, r.video_url, "
        "r.status, r.submitted_at::text AS submitted_at "
        "FROM records r JOIN demon_levels d ON d.id = r.level_id "
        "WHERE r.status = 'accepted' "
        "ORDER BY r.submitted_at DESC LIMIT $1 OFFSET $2",
        [cb](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            for (auto &row : res) {
                Json::Value r;
                r["id"]           = row["id"].as<Json::Int64>();
                r["player_name"]  = row["player_name"].as<std::string>();
                r["level_name"]   = row["level_name"].as<std::string>();
                r["level_id"]     = row["level_id"].as<Json::Int64>();
                r["progress"]     = row["progress"].as<int>();
                r["video_url"]    = row["video_url"].as<std::string>();
                r["status"]       = row["status"].as<std::string>();
                r["submitted_at"] = row["submitted_at"].as<std::string>();
                arr.append(r);
            }
            j["records"] = arr;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        limit, offset);
}

void RankingsController::getRankings(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&cb) {
    auto continent = req->getParameter("continent");
    auto country   = req->getParameter("country");
    auto q         = req->getParameter("q");

    std::string where;
    auto addWhere = [&](const std::string &clause) {
        where += where.empty() ? " WHERE is_public = TRUE AND NOT is_banned" : " AND ";
        where += clause;
    };

    if (where.empty()) where = " WHERE is_public = TRUE AND NOT is_banned";

    std::string sql =
        "SELECT id, username, country, continent, points, "
        "(SELECT name FROM demon_levels d JOIN records r ON r.level_id = d.id "
        " WHERE r.player_name = users.username AND r.status = 'accepted' "
        " ORDER BY d.rank ASC LIMIT 1) AS hardest_demon, "
        "avatar_url "
        "FROM users" + where;

    if (!continent.empty() && continent != "All") {
        sql += " AND continent = '" + continent + "'";
    }
    if (!country.empty() && country != "All") {
        sql += " AND country = '" + country + "'";
    }
    if (!q.empty()) {
        sql += " AND username ILIKE '%" + q + "%'";
    }
    sql += " ORDER BY points DESC LIMIT 1000";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(sql,
        [cb](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            int rank = 1;
            for (auto &row : res) {
                Json::Value p;
                p["id"]            = row["id"].as<Json::Int64>();
                p["username"]      = row["username"].as<std::string>();
                p["country"]       = row["country"].as<std::string>();
                p["continent"]     = row["continent"].as<std::string>();
                p["points"]        = row["points"].as<double>();
                p["rank"]          = rank++;
                p["hardest_demon"] = row["hardest_demon"].isNull() ? "" : row["hardest_demon"].as<std::string>();
                p["avatar_url"]    = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>();
                arr.append(p);
            }
            j["players"] = arr;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}

static std::string generateApiKey() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream ss;
    ss << "pt_" << std::hex << dis(gen) << dis(gen) << dis(gen) << dis(gen);
    return ss.str();
}

static std::string sha256Hex(const std::string &input) {
    unsigned char hash[32];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    std::ostringstream ss;
    for (int i = 0; i < 32; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

void ApiKeysController::getKeys(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, name, key_prefix, created_at::text, last_used_at::text, monthly_usage "
        "FROM api_keys WHERE user_id = $1 ORDER BY created_at DESC",
        [cb](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            for (auto &row : res) {
                Json::Value k;
                k["id"]           = row["id"].as<Json::Int64>();
                k["name"]         = row["name"].as<std::string>();
                k["key_prefix"]   = row["key_prefix"].as<std::string>();
                k["created_at"]   = row["created_at"].as<std::string>();
                k["last_used_at"] = row["last_used_at"].isNull() ? Json::nullValue : Json::Value(row["last_used_at"].as<std::string>());
                k["monthly_usage"]= row["monthly_usage"].as<int>();
                arr.append(k);
            }
            j["keys"] = arr;
            Json::Value usage;
            int total = 0;
            for (auto &row : res) total += row["monthly_usage"].as<int>();
            usage["requests"] = total;
            usage["limit"]    = 100000;
            j["usage"] = usage;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        std::stoll(payload->user_id));
}

void ApiKeysController::createKey(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto body = req->getJsonObject();
    auto name = (*body)["name"].asString();
    if (name.empty()) { cb(pt::errorResponse(k400BadRequest, "Key name is required.")); return; }

    auto userId = std::stoll(payload->user_id);
    auto db = drogon::app().getDbClient();

    db->execSqlAsync("SELECT COUNT(*) FROM api_keys WHERE user_id = $1",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res[0][0].as<int>() >= 5) {
                cb(pt::errorResponse(k429TooManyRequests, "Maximum 5 API keys reached.")); return;
            }
            auto rawKey = generateApiKey();
            auto hash   = sha256Hex(rawKey);
            auto prefix = rawKey.substr(0, 12);

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "INSERT INTO api_keys (user_id, name, key_hash, key_prefix) "
                "VALUES ($1, $2, $3, $4) RETURNING id, name, key_prefix, created_at::text",
                [=, cb = std::move(cb)](const orm::Result &r) mutable {
                    Json::Value j;
                    j["key"] = rawKey;
                    Json::Value apiKey;
                    apiKey["id"]         = r[0]["id"].as<Json::Int64>();
                    apiKey["name"]       = r[0]["name"].as<std::string>();
                    apiKey["key_prefix"] = r[0]["key_prefix"].as<std::string>();
                    apiKey["created_at"] = r[0]["created_at"].as<std::string>();
                    j["api_key"] = apiKey;
                    cb(pt::okResponse(j));
                },
                [cb](const orm::DrogonDbException &e) mutable {
                    cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                },
                userId, name, hash, prefix);
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        userId);
}

void ApiKeysController::revokeKey(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   long long id) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "DELETE FROM api_keys WHERE id = $1 AND user_id = $2 RETURNING id",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Key not found.")); return; }
            Json::Value j; j["ok"] = true; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        id, std::stoll(payload->user_id));
}

void ApiKeysController::publicApi(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   const std::string &key) {
    auto hash = sha256Hex(key);
    auto db   = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, monthly_usage FROM api_keys WHERE key_hash = $1 LIMIT 1",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k401Unauthorized, "Invalid API key.")); return; }
            auto keyId = res[0]["id"].as<Json::Int64>();
            auto usage = res[0]["monthly_usage"].as<int>();
            if (usage >= 100000) { cb(pt::errorResponse(k429TooManyRequests, "Monthly rate limit exceeded.")); return; }

            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync("UPDATE api_keys SET monthly_usage = monthly_usage + 1, last_used_at = NOW() WHERE id = $1",
                [](const orm::Result &) {}, [](const orm::DrogonDbException &) {}, keyId);

            auto db3 = drogon::app().getDbClient();
            db3->execSqlAsync(
                "SELECT id, rank, name, points, verified_by, creators, video_url, difficulty_tier "
                "FROM demon_levels ORDER BY rank ASC",
                [cb](const orm::Result &r) mutable {
                    Json::Value j;
                    Json::Value arr(Json::arrayValue);
                    for (auto &row : r) {
                        Json::Value l;
                        l["id"]             = row["id"].as<Json::Int64>();
                        l["rank"]           = row["rank"].as<int>();
                        l["name"]           = row["name"].as<std::string>();
                        l["points"]         = row["points"].as<double>();
                        l["verified_by"]    = row["verified_by"].as<std::string>();
                        l["video_url"]      = row["video_url"].as<std::string>();
                        l["difficulty_tier"]= row["difficulty_tier"].as<std::string>();
                        arr.append(l);
                    }
                    j["demons"] = arr;
                    j["count"]  = (long long)arr.size();
                    cb(pt::okResponse(j));
                },
                [cb](const orm::DrogonDbException &e) mutable {
                    cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                });
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        hash);
}

void UserController::getMe(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, username, email, bio, avatar_url, country, continent, is_public, points, "
        "two_factor_enabled, discord_id IS NOT NULL AS discord_connected, "
        "google_id IS NOT NULL AS google_connected "
        "FROM users WHERE id = $1 LIMIT 1",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "User not found.")); return; }
            auto row = res[0];
            Json::Value j; Json::Value u;
            u["id"]               = row["id"].as<Json::Int64>();
            u["username"]         = row["username"].as<std::string>();
            u["email"]            = row["email"].as<std::string>();
            u["bio"]              = row["bio"].as<std::string>();
            u["avatar_url"]       = row["avatar_url"].isNull() ? "" : row["avatar_url"].as<std::string>();
            u["country"]          = row["country"].as<std::string>();
            u["continent"]        = row["continent"].as<std::string>();
            u["is_public"]        = row["is_public"].as<bool>();
            u["points"]           = row["points"].as<double>();
            u["two_factor_enabled"]    = row["two_factor_enabled"].as<bool>();
            u["discord_connected"]     = row["discord_connected"].as<bool>();
            u["google_connected"]      = row["google_connected"].as<bool>();
            j["user"] = u; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        std::stoll(payload->user_id));
}

void UserController::updateMe(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto body      = req->getJsonObject();
    auto bio       = (*body)["bio"].asString();
    auto country   = (*body)["country"].asString();
    auto continent = (*body)["continent"].asString();
    auto isPublic  = (*body)["is_public"].asBool();

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET bio = $1, country = $2, continent = $3, is_public = $4 WHERE id = $5",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        bio, country, continent, isPublic, std::stoll(payload->user_id));
}

void UserController::deleteMe(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_banned = TRUE, ban_reason = 'Account deletion requested', "
        "ban_expires_at = NOW() + INTERVAL '30 days' WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        std::stoll(payload->user_id));
}

void UserController::changePassword(const HttpRequestPtr &req,
                                     std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto body        = req->getJsonObject();
    auto currentPass = (*body)["current_password"].asString();
    auto newPass     = (*body)["new_password"].asString();
    if (newPass.size() < 8) { cb(pt::errorResponse(k400BadRequest, "Password must be at least 8 characters.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync("SELECT password_hash FROM users WHERE id = $1",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res.empty() || !BCrypt::validatePassword(currentPass, res[0]["password_hash"].as<std::string>())) {
                cb(pt::errorResponse(k401Unauthorized, "Current password is incorrect.")); return;
            }
            auto newHash = BCrypt::generateHash(newPass);
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync("UPDATE users SET password_hash = $1 WHERE id = $2",
                [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
                [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
                newHash, std::stoll(payload->user_id));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        std::stoll(payload->user_id));
}

void UserController::setup2FA(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    static const char base32chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 31);
    std::string secret;
    for (int i = 0; i < 32; ++i) secret += base32chars[dis(gen)];

    auto db = drogon::app().getDbClient();
    db->execSqlAsync("UPDATE users SET two_factor_secret = $1 WHERE id = $2 RETURNING email, username",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "User not found.")); return; }
            auto username = res[0]["username"].as<std::string>();
            std::string uri = "otpauth://totp/PointerThere%3A" + username +
                              "?secret=" + secret + "&issuer=PointerThere&algorithm=SHA1&digits=6&period=30";
            Json::Value j; j["uri"] = uri; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        secret, std::stoll(payload->user_id));
}

void UserController::verify2FA(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync("UPDATE users SET two_factor_enabled = TRUE WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        std::stoll(payload->user_id));
}

void UserController::disable2FA(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&cb) {
    auto tokenOpt = pt::JwtHelper::extractBearer(req);
    if (!tokenOpt) { cb(pt::errorResponse(k401Unauthorized, "Authentication required.")); return; }
    auto payload = pt::JwtHelper::instance().verify(*tokenOpt);
    if (!payload) { cb(pt::errorResponse(k401Unauthorized, "Invalid token.")); return; }

    auto db = drogon::app().getDbClient();
    db->execSqlAsync("UPDATE users SET two_factor_enabled = FALSE, two_factor_secret = NULL WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        std::stoll(payload->user_id));
}

void SettingsController::getSiteSettings(const HttpRequestPtr &,
                                          std::function<void(const HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT discord_url, twitter_url, youtube_url, twitch_url, github_url, patreon_url, "
        "db_cost, deploy_cost, bot_cost FROM site_settings WHERE id = 1",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Settings not configured.")); return; }
            auto row = res[0];
            Json::Value j; Json::Value s;
            s["discord_url"]  = row["discord_url"].as<std::string>();
            s["twitter_url"]  = row["twitter_url"].as<std::string>();
            s["youtube_url"]  = row["youtube_url"].as<std::string>();
            s["twitch_url"]   = row["twitch_url"].as<std::string>();
            s["github_url"]   = row["github_url"].as<std::string>();
            s["patreon_url"]  = row["patreon_url"].as<std::string>();
            s["db_cost"]      = row["db_cost"].as<double>();
            s["deploy_cost"]  = row["deploy_cost"].as<double>();
            s["bot_cost"]     = row["bot_cost"].as<double>();
            j["settings"] = s; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}
