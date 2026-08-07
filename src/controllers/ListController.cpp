/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include "ListController.h"
#include "../utils/env.h"
#include <drogon/drogon.h>
#include <sstream>

using namespace pt::controllers;
using namespace drogon;

static Json::Value levelToJson(const orm::Row &row) {
    Json::Value j;
    j["id"]             = static_cast<Json::Int64>(row["id"].as<long long>());
    j["rank"]           = row["rank"].as<int>();
    j["name"]           = row["name"].as<std::string>();
    j["points"]         = row["points"].as<double>();
    j["verified_by"]    = row["verified_by"].as<std::string>();
    j["video_url"]      = row["video_url"].as<std::string>();
    j["thumbnail_url"]  = row["thumbnail_url"].isNull() ? "" : row["thumbnail_url"].as<std::string>();
    j["difficulty_tier"] = row["difficulty_tier"].as<std::string>();
    j["created_at"]     = row["created_at"].as<std::string>();

    auto creatorsRaw = row["creators"].as<std::string>();
    Json::Value creators(Json::arrayValue);
    if (creatorsRaw.size() > 2) {
        auto inner = creatorsRaw.substr(1, creatorsRaw.size() - 2);
        std::stringstream ss(inner);
        std::string item;
        while (std::getline(ss, item, ',')) creators.append(item);
    }
    j["creators"] = creators;

    if (!row["records_count"].isNull()) {
        j["records_count"] = row["records_count"].as<int>();
    }
    return j;
}

void ListController::getList(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&cb, int) {
    auto page    = std::max(1, std::stoi(req->getParameter("page").empty() ? "1" : req->getParameter("page")));
    auto limit   = std::min(100, std::max(1, std::stoi(req->getParameter("limit").empty() ? "20" : req->getParameter("limit"))));
    auto offset  = (page - 1) * limit;
    auto tier    = req->getParameter("tier");
    auto creator = req->getParameter("creator");
    auto q       = req->getParameter("q");
    auto minPts  = req->getParameter("min_points");

    std::string whereClauses;
    std::vector<std::string> params;
    int paramIdx = 1;

    auto addWhere = [&](const std::string &clause) {
        whereClauses += whereClauses.empty() ? " WHERE " : " AND ";
        whereClauses += clause;
    };

    if (!tier.empty()) {
        addWhere("difficulty_tier = $" + std::to_string(paramIdx++));
        params.push_back(tier);
    }
    if (!creator.empty()) {
        addWhere("$" + std::to_string(paramIdx++) + " = ANY(creators)");
        params.push_back(creator);
    }
    if (!q.empty()) {
        addWhere("name ILIKE $" + std::to_string(paramIdx++));
        params.push_back("%" + q + "%");
    }
    if (!minPts.empty()) {
        addWhere("points >= $" + std::to_string(paramIdx++));
        params.push_back(minPts);
    }

    auto countSql = "SELECT COUNT(*) FROM demon_levels" + whereClauses;
    auto dataSql  = "SELECT id, rank, name, points, verified_by, creators, video_url, thumbnail_url, difficulty_tier, created_at, "
                    "(SELECT COUNT(*) FROM records r WHERE r.level_id = demon_levels.id AND r.status = 'accepted') AS records_count "
                    "FROM demon_levels" + whereClauses +
                    " ORDER BY rank ASC LIMIT $" + std::to_string(paramIdx) +
                    " OFFSET $" + std::to_string(paramIdx + 1);

    auto db = drogon::app().getDbClient();
    auto limitStr = std::to_string(limit);
    auto offsetStr = std::to_string(offset);
    params.push_back(limitStr);
    params.push_back(offsetStr);

    db->execSqlAsync(countSql + ";",
        [=, cb = std::move(cb), dataSql = dataSql](const orm::Result &countRes) mutable {
            long long total = countRes[0][0].as<long long>();
            auto db2 = drogon::app().getDbClient();
            db2->execSqlAsync(dataSql + ";",
                [=, cb = std::move(cb), total = total](const orm::Result &dataRes) mutable {
                    Json::Value j;
                    j["total"]  = static_cast<Json::Int64>(total);
                    Json::Value levels(Json::arrayValue);
                    for (const auto &row : dataRes) levels.append(levelToJson(row));
                    j["levels"] = levels;
                    cb(pt::okResponse(j));
                },
                [cb](const orm::DrogonDbException &e) mutable {
                    cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                });
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}

void ListController::getFeatured(const HttpRequestPtr &,
                                  std::function<void(const HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT d.id, d.rank, d.name, d.points, d.verified_by, d.creators, d.video_url, "
        "d.thumbnail_url, d.difficulty_tier, d.created_at, "
        "(SELECT COUNT(*) FROM records r WHERE r.level_id = d.id AND r.status = 'accepted') AS records_count "
        "FROM demon_levels d "
        "JOIN site_settings s ON s.featured_level_id = d.id "
        "WHERE s.id = 1 LIMIT 1",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) {
                auto db2 = drogon::app().getDbClient();
                db2->execSqlAsync(
                    "SELECT id, rank, name, points, verified_by, creators, video_url, "
                    "thumbnail_url, difficulty_tier, created_at, "
                    "(SELECT COUNT(*) FROM records r WHERE r.level_id = demon_levels.id AND r.status = 'accepted') AS records_count "
                    "FROM demon_levels ORDER BY rank ASC LIMIT 1",
                    [cb](const orm::Result &r2) mutable {
                        Json::Value j;
                        j["level"] = r2.empty() ? Json::nullValue : levelToJson(r2[0]);
                        cb(pt::okResponse(j));
                    },
                    [cb](const orm::DrogonDbException &e) mutable {
                        cb(pt::errorResponse(k500InternalServerError, e.base().what()));
                    });
                return;
            }
            Json::Value j;
            j["level"] = levelToJson(res[0]);
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}

void ListController::getMovements(const HttpRequestPtr &,
                                   std::function<void(const HttpResponsePtr &)> &&cb) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT lm.id, lm.level_id, d.name AS level_name, lm.old_rank, lm.new_rank, lm.created_at::text AS date "
        "FROM list_movements lm JOIN demon_levels d ON d.id = lm.level_id "
        "ORDER BY lm.created_at DESC LIMIT 10",
        [cb](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            for (const auto &row : res) {
                Json::Value m;
                m["id"]         = static_cast<Json::Int64>(row["id"].as<long long>());
                m["level_id"]   = static_cast<Json::Int64>(row["level_id"].as<long long>());
                m["level_name"] = row["level_name"].as<std::string>();
                m["old_rank"]   = row["old_rank"].isNull() ? Json::nullValue : Json::Value(row["old_rank"].as<int>());
                m["new_rank"]   = row["new_rank"].as<int>();
                m["date"]       = row["date"].as<std::string>();
                arr.append(m);
            }
            j["movements"] = arr;
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        });
}

void ListController::getLevel(const HttpRequestPtr &,
                               std::function<void(const HttpResponsePtr &)> &&cb,
                               long long id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, rank, name, points, verified_by, creators, video_url, thumbnail_url, difficulty_tier, created_at, "
        "(SELECT COUNT(*) FROM records r WHERE r.level_id = demon_levels.id AND r.status = 'accepted') AS records_count "
        "FROM demon_levels WHERE id = $1 LIMIT 1",
        [cb](const orm::Result &res) mutable {
            if (res.empty()) { cb(pt::errorResponse(k404NotFound, "Level not found.")); return; }
            Json::Value j;
            j["level"] = levelToJson(res[0]);
            cb(pt::okResponse(j));
        },
        [cb](const orm::DrogonDbException &e) mutable {
            cb(pt::errorResponse(k500InternalServerError, e.base().what()));
        },
        id);
}

void ListController::getLevelRecs(const HttpRequestPtr &,
                                   std::function<void(const HttpResponsePtr &)> &&cb,
                                   long long id) {
    auto db = drogon::app().getDbClient();
    db->execSqlAsync(
        "SELECT id, player_name, progress, video_url, status, submitted_at::text AS submitted_at "
        "FROM records WHERE level_id = $1 AND status = 'accepted' ORDER BY progress DESC, submitted_at ASC",
        [cb](const orm::Result &res) mutable {
            Json::Value j;
            Json::Value arr(Json::arrayValue);
            for (const auto &row : res) {
                Json::Value r;
                r["id"]           = static_cast<Json::Int64>(row["id"].as<long long>());
                r["player_name"]  = row["player_name"].as<std::string>();
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
        id);
}
