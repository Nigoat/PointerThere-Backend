/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include "AdminController.h"
#include "../utils/env.h"
#include <drogon/drogon.h>

using namespace pt::controllers;
using namespace drogon;

static bool isAdmin(const HttpRequestPtr &req) {
    auto cookie = req->getCookie("admin_session");
    return !cookie.empty() && cookie == pt::env("ADMIN_SESSION_SECRET");
}

#define REQUIRE_ADMIN(req, cb) \
    if (!isAdmin(req)) { cb(pt::errorResponse(k401Unauthorized, "Admin access required.")); return; }

void AdminController::login(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&cb) {
    auto body = req->getJsonObject();
    if (!body) { cb(pt::errorResponse(k400BadRequest, "Invalid JSON.")); return; }

    auto username = (*body)["username"].asString();
    auto password = (*body)["password"].asString();

    auto expectedUser = pt::env("ADMIN_USERNAME", "Nigoattt");
    auto expectedPass = pt::env("ADMIN_PASSWORD", "XYZ123LMBLABLA1983");
    auto secret       = pt::env("ADMIN_SESSION_SECRET", "pointerthere_admin_session_secret_2026_secure_key");

    if (username != expectedUser || password != expectedPass) {
        cb(pt::errorResponse(k401Unauthorized, "Invalid credentials."));
        return;
    }

    Json::Value j;
    j["ok"] = true;
    auto resp = pt::okResponse(j);
    resp->addCookie("admin_session", secret, 60 * 60 * 8, "/", "", false, true);
    cb(resp);
}

void AdminController::getStats(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT "
        "(SELECT COUNT(*) FROM records WHERE status = 'pending') AS pending_count, "
        "(SELECT COUNT(*) FROM users WHERE NOT is_banned) AS users_count, "
        "(SELECT COUNT(*) FROM appeals WHERE status = 'pending') AS appeals_count, "
        "(SELECT COUNT(*) FROM records WHERE status = 'pending' AND submitted_at > NOW() - INTERVAL '24 hours') AS new_since_yesterday",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k500InternalServerError, "Stats unavailable.")); return; }
            Json::Value j;
            auto &row = res[0];
            Json::Value s;
            s["pending_count"]       = row["pending_count"].as<long long>();
            s["users_count"]         = row["users_count"].as<long long>();
            s["appeals_count"]       = row["appeals_count"].as<long long>();
            s["new_since_yesterday"] = row["new_since_yesterday"].as<long long>();
            j["stats"] = s;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}

void AdminController::getPending(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto status = req->getParameter("status");
    auto page   = std::max(1, std::stoi(req->getParameter("page").empty() ? "1" : req->getParameter("page")));
    auto limit  = std::min(100, std::max(1, std::stoi(req->getParameter("limit").empty() ? "20" : req->getParameter("limit"))));
    auto offset = (page - 1) * limit;

    std::string statusClause = "status = 'pending'";
    if (status == "flagged") statusClause = "status = 'flagged'";
    else if (status == "all") statusClause = "status IN ('pending', 'flagged')";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT r.id, d.name AS level_name, r.level_id, r.player_name, r.progress, r.video_url, "
        "r.discord_tag, r.status, r.submitted_at::text AS submitted_at, r.notes "
        "FROM records r JOIN demon_levels d ON d.id = r.level_id "
        "WHERE r." + statusClause +
        " ORDER BY r.submitted_at ASC LIMIT $1 OFFSET $2",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            for (auto &row : res) {
                Json::Value r;
                r["id"]           = row["id"].as<long long>();
                r["level_name"]   = row["level_name"].as<std::string>();
                r["level_id"]     = row["level_id"].as<long long>();
                r["player_name"]  = row["player_name"].as<std::string>();
                r["progress"]     = row["progress"].as<int>();
                r["video_url"]    = row["video_url"].as<std::string>();
                r["discord_tag"]  = row["discord_tag"].isNull() ? "" : row["discord_tag"].as<std::string>();
                r["status"]       = row["status"].as<std::string>();
                r["submitted_at"] = row["submitted_at"].as<std::string>();
                arr.append(r);
            }
            j["records"] = arr;
            j["total"]   = (long long)res.size();
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        limit, offset);
}

void AdminController::updateRecord(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&cb,
                                    long long id) {
    REQUIRE_ADMIN(req, cb);
    auto body   = req->getJsonObject();
    auto status = (*body)["status"].asString();
    if (status != "accepted" && status != "rejected" && status != "flagged") {
        cb(pt::errorResponse(k400BadRequest, "Invalid status."));
        return;
    }
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE records SET status = $1, reviewed_at = NOW() WHERE id = $2 RETURNING id, level_id, player_name, progress",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Record not found.")); return; }
            if (status == "accepted") {
                auto db2 = drogon::app().getDbClient();
                auto levelId = res[0]["level_id"].as<long long>();
                db2->execSqlAsync(
                    "UPDATE users SET points = points + "
                    "(SELECT points FROM demon_levels WHERE id = $1) "
                    "WHERE username = (SELECT player_name FROM records WHERE id = $2)",
                    [](const orm::Result &) {}, [](const orm::DrogonDbException &) {},
                    levelId, id);
            }
            Json::Value j; j["ok"] = true;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        status, id);
}

void AdminController::getUsers(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto q  = req->getParameter("q");
    auto db = drogon::app().getDbClient();

    std::string sql =
        "SELECT u.id, u.username, u.email, u.country, u.continent, u.points, "
        "u.is_banned, u.ban_expires_at::text, u.timeout_until::text, "
        "u.two_factor_enabled, u.discord_id, u.created_at::text, "
        "(SELECT COUNT(*) FROM records r WHERE r.player_name = u.username AND r.status = 'accepted') AS records_count "
        "FROM users u ";

    if (!q.empty()) {
        sql += "WHERE u.username ILIKE $1 OR u.email ILIKE $1 ORDER BY u.id DESC LIMIT 100";
        db->execSqlAsync(sql,
            [cb](const orm::Result &res) mutable {
                Json::Value j; Json::Value arr(Json::arrayValue);
                for (auto &row : res) {
                    Json::Value u;
                    u["id"]                  = row["id"].as<long long>();
                    u["username"]            = row["username"].as<std::string>();
                    u["email"]               = row["email"].as<std::string>();
                    u["country"]             = row["country"].as<std::string>();
                    u["continent"]           = row["continent"].as<std::string>();
                    u["points"]              = row["points"].as<double>();
                    u["is_banned"]           = row["is_banned"].as<bool>();
                    u["ban_expires_at"]      = row["ban_expires_at"].isNull() ? Json::nullValue : Json::Value(row["ban_expires_at"].as<std::string>());
                    u["timeout_until"]       = row["timeout_until"].isNull() ? Json::nullValue : Json::Value(row["timeout_until"].as<std::string>());
                    u["two_factor_enabled"]  = row["two_factor_enabled"].as<bool>();
                    u["discord_id"]          = row["discord_id"].isNull() ? Json::nullValue : Json::Value(row["discord_id"].as<std::string>());
                    u["created_at"]          = row["created_at"].as<std::string>();
                    u["records_count"]       = row["records_count"].as<long long>();
                    arr.append(u);
                }
                j["users"] = arr; cb(pt::okResponse(j));
            },
            [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
            "%" + q + "%");
    } else {
        sql += "ORDER BY u.points DESC LIMIT 500";
        db->execSqlAsync(sql,
            [cb](const orm::Result &res) mutable {
                Json::Value j; Json::Value arr(Json::arrayValue);
                for (auto &row : res) {
                    Json::Value u;
                    u["id"]             = row["id"].as<long long>();
                    u["username"]       = row["username"].as<std::string>();
                    u["email"]          = row["email"].as<std::string>();
                    u["country"]        = row["country"].as<std::string>();
                    u["continent"]      = row["continent"].as<std::string>();
                    u["points"]         = row["points"].as<double>();
                    u["is_banned"]      = row["is_banned"].as<bool>();
                    u["ban_expires_at"] = row["ban_expires_at"].isNull() ? Json::nullValue : Json::Value(row["ban_expires_at"].as<std::string>());
                    u["timeout_until"]  = row["timeout_until"].isNull() ? Json::nullValue : Json::Value(row["timeout_until"].as<std::string>());
                    u["two_factor_enabled"] = row["two_factor_enabled"].as<bool>();
                    u["discord_id"]     = row["discord_id"].isNull() ? Json::nullValue : Json::Value(row["discord_id"].as<std::string>());
                    u["created_at"]     = row["created_at"].as<std::string>();
                    u["records_count"]  = row["records_count"].as<long long>();
                    arr.append(u);
                }
                j["users"] = arr; cb(pt::okResponse(j));
            },
            [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); });
    }
}

void AdminController::updateUser(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&cb,
                                  long long id) {
    REQUIRE_ADMIN(req, cb);
    auto body = req->getJsonObject();
    std::string sets;
    std::vector<std::string> params;
    int idx = 1;
    auto tryAdd = [&](const std::string &col, const std::string &jsonKey) {
        if (body->isMember(jsonKey) && !(*body)[jsonKey].isNull()) {
            if (!sets.empty()) sets += ", ";
            sets += col + " = $" + std::to_string(idx++);
            params.push_back((*body)[jsonKey].asString());
        }
    };
    tryAdd("points",  "points");
    tryAdd("email",   "email");
    tryAdd("country", "country");
    tryAdd("continent", "continent");
    if (sets.empty()) { cb(pt::errorResponse(k400BadRequest, "No fields to update.")); return; }

    auto db = drogon::app().getDbClient();
    std::string sql = "UPDATE users SET " + sets + " WHERE id = $" + std::to_string(idx) + " RETURNING id";
    if (params.size() == 1) {
        db->execSqlAsync(sql, [cb](const orm::Result &r) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
            [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
            params[0], id);
    } else if (params.size() == 2) {
        db->execSqlAsync(sql, [cb](const orm::Result &r) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
            [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
            params[0], params[1], id);
    } else {
        db->execSqlAsync(sql, [cb](const orm::Result &r) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
            [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
            params[0], params[1], params[2], id);
    }
}

void AdminController::banUser(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&cb,
                               long long id) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_banned = TRUE, ban_expires_at = NOW() + INTERVAL '30 days' WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        id);
}

void AdminController::unbanUser(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&cb,
                                 long long id) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET is_banned = FALSE, ban_expires_at = NULL, ban_reason = NULL WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        id);
}

void AdminController::timeoutUser(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   long long id) {
    REQUIRE_ADMIN(req, cb);
    auto body = req->getJsonObject();
    auto days = (*body)["days"].asInt();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE users SET timeout_until = NOW() + ($1 || ' days')::INTERVAL WHERE id = $2",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        std::to_string(days), id);
}

void AdminController::getLevels(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, rank, name, points, verified_by, creators, video_url, thumbnail_url, difficulty_tier, created_at "
        "FROM demon_levels ORDER BY rank ASC",
        [cb](const orm::Result &res) mutable {
            Json::Value j; Json::Value arr(Json::arrayValue);
            for (auto &row : res) {
                Json::Value l;
                l["id"]             = row["id"].as<long long>();
                l["rank"]           = row["rank"].as<int>();
                l["name"]           = row["name"].as<std::string>();
                l["points"]         = row["points"].as<double>();
                l["verified_by"]    = row["verified_by"].as<std::string>();
                l["video_url"]      = row["video_url"].as<std::string>();
                l["difficulty_tier"] = row["difficulty_tier"].as<std::string>();
                arr.append(l);
            }
            j["levels"] = arr; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); });
}

void AdminController::addLevel(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto body = req->getJsonObject();
    auto name  = (*body)["name"].asString();
    auto rank  = (*body)["rank"].asInt();
    auto ver   = (*body)["verified_by"].asString();
    auto vid   = (*body)["video_url"].asString();
    auto tier  = (*body)["difficulty_tier"].asString();
    auto pts   = (*body)["points"].asDouble();
    std::string creatorsArr = "{";
    for (auto &c : (*body)["creators"]) { if (creatorsArr.size() > 1) creatorsArr += ","; creatorsArr += c.asString(); }
    creatorsArr += "}";

    auto db = drogon::app().getDbClient();
    db->execSqlAsync("UPDATE demon_levels SET rank = rank + 1 WHERE rank >= $1",
        [=, cb = std::move(cb)](const orm::Result &) mutable {
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "INSERT INTO demon_levels (rank, name, verified_by, video_url, difficulty_tier, points, creators) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id, rank, name, points, verified_by, difficulty_tier",
                [=, cb = std::move(cb)](const orm::Result &res) mutable {
                    if (res.empty()) { cb(pt::errorResponse(k500InternalServerError, "Insert failed.")); return; }
                    auto levelId = res[0]["id"].as<long long>();
                    auto db3 = drogon::app().getDbClient();
                    db3->execSqlAsync("INSERT INTO list_movements (level_id, old_rank, new_rank) VALUES ($1, NULL, $2)",
                        [](const orm::Result &) {}, [](const orm::DrogonDbException &) {}, levelId, rank);
                    Json::Value j; Json::Value lvl;
                    lvl["id"] = levelId; lvl["rank"] = rank; lvl["name"] = name;
                    j["level"] = lvl; cb(pt::okResponse(j));
                },
                [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
                rank, name, ver, vid, tier, pts, creatorsArr);
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        rank);
}

void AdminController::updateLevel(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   long long id) {
    REQUIRE_ADMIN(req, cb);
    auto body = req->getJsonObject();
    auto db = drogon::app().getDbClient();
    auto name  = (*body)["name"].asString();
    auto rank  = (*body)["rank"].asInt();
    auto ver   = (*body)["verified_by"].asString();
    auto vid   = (*body)["video_url"].asString();
    auto tier  = (*body)["difficulty_tier"].asString();
    auto pts   = (*body)["points"].asDouble();
    std::string creatorsArr = "{";
    for (auto &c : (*body)["creators"]) { if (creatorsArr.size() > 1) creatorsArr += ","; creatorsArr += c.asString(); }
    creatorsArr += "}";

    db->execSqlAsync("SELECT rank FROM demon_levels WHERE id = $1",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            int oldRank = res.empty() ? rank : res[0]["rank"].as<int>();
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(
                "UPDATE demon_levels SET name=$1, rank=$2, verified_by=$3, video_url=$4, difficulty_tier=$5, points=$6, creators=$7 WHERE id=$8",
                [=, cb = std::move(cb)](const orm::Result &) mutable {
                    if (oldRank != rank) {
                        auto db3 = drogon::app().getDbClient();
                        db3->execSqlAsync("INSERT INTO list_movements (level_id, old_rank, new_rank) VALUES ($1, $2, $3)",
                            [](const orm::Result &) {}, [](const orm::DrogonDbException &) {}, id, oldRank, rank);
                    }
                    Json::Value j; j["ok"] = true; cb(pt::okResponse(j));
                },
                [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
                name, rank, ver, vid, tier, pts, creatorsArr, id);
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        id);
}

void AdminController::deleteLevel(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   long long id) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync("DELETE FROM demon_levels WHERE id = $1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        id);
}

void AdminController::getAppeals(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto status = req->getParameter("status");
    std::string where = status == "all" || status.empty() ? "" : " WHERE a.status = '" + status + "'";
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT a.id, a.user_id, u.username, u.email, a.reason, a.status, a.created_at::text "
        "FROM appeals a JOIN users u ON u.id = a.user_id" + where +
        " ORDER BY a.created_at ASC",
        [cb](const orm::Result &res) mutable {
            Json::Value j; Json::Value arr(Json::arrayValue);
            for (auto &row : res) {
                Json::Value a;
                a["id"]         = row["id"].as<long long>();
                a["user_id"]    = row["user_id"].as<long long>();
                a["username"]   = row["username"].as<std::string>();
                a["email"]      = row["email"].as<std::string>();
                a["reason"]     = row["reason"].as<std::string>();
                a["status"]     = row["status"].as<std::string>();
                a["created_at"] = row["created_at"].as<std::string>();
                arr.append(a);
            }
            j["appeals"] = arr; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); });
}

void AdminController::updateAppeal(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&cb,
                                    long long id) {
    REQUIRE_ADMIN(req, cb);
    auto body   = req->getJsonObject();
    auto status = (*body)["status"].asString();
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE appeals SET status = $1, reviewed_at = NOW() WHERE id = $2 RETURNING user_id",
        [=, cb = std::move(cb)](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Appeal not found.")); return; }
            if (status == "resolved_lifted") {
                auto userId = res[0]["user_id"].as<long long>();
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync("UPDATE users SET is_banned = FALSE, ban_expires_at = NULL WHERE id = $1",
                    [](const orm::Result &) {}, [](const orm::DrogonDbException &) {}, userId);
            }
            Json::Value j; j["ok"] = true; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        status, id);
}

void AdminController::getSettings(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT discord_url, twitter_url, youtube_url, twitch_url, github_url, patreon_url, "
        "db_cost, deploy_cost, bot_cost, featured_level_id FROM site_settings WHERE id = 1",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Settings not found.")); return; }
            auto &row = res[0];
            Json::Value j; Json::Value s;
            s["discord_url"]       = row["discord_url"].as<std::string>();
            s["twitter_url"]       = row["twitter_url"].as<std::string>();
            s["youtube_url"]       = row["youtube_url"].as<std::string>();
            s["twitch_url"]        = row["twitch_url"].as<std::string>();
            s["github_url"]        = row["github_url"].as<std::string>();
            s["patreon_url"]       = row["patreon_url"].as<std::string>();
            s["db_cost"]           = row["db_cost"].as<double>();
            s["deploy_cost"]       = row["deploy_cost"].as<double>();
            s["bot_cost"]          = row["bot_cost"].as<double>();
            s["featured_level_id"] = row["featured_level_id"].isNull() ? Json::nullValue : Json::Value(row["featured_level_id"].as<long long>());
            j["settings"] = s; cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); });
}

void AdminController::updateSettings(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&cb) {
    REQUIRE_ADMIN(req, cb);
    auto body = req->getJsonObject();
    auto db   = drogon::app().getDbClient();
    db->execSqlAsync(
        "UPDATE site_settings SET "
        "discord_url = $1, twitter_url = $2, youtube_url = $3, twitch_url = $4, "
        "github_url = $5, patreon_url = $6, db_cost = $7, deploy_cost = $8, bot_cost = $9 "
        "WHERE id = 1",
        [cb](const orm::Result &) mutable { Json::Value j; j["ok"] = true; cb(pt::okResponse(j)); },
        [cb](const orm::DrogonDbException &e) mutable { cb(pt::errorResponse(k500InternalServerError, e.base().what())); },
        (*body)["discord_url"].asString(),
        (*body)["twitter_url"].asString(),
        (*body)["youtube_url"].asString(),
        (*body)["twitch_url"].asString(),
        (*body)["github_url"].asString(),
        (*body)["patreon_url"].asString(),
        (*body)["db_cost"].asDouble(),
        (*body)["deploy_cost"].asDouble(),
        (*body)["bot_cost"].asDouble());
}
