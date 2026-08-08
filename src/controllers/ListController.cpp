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

static std::string sqlLiteral(const std::string &value) {
    std::string escaped = "'";
    for (char c : value) {
        if (c == '\'') escaped += "''";
        else escaped += c;
    }
    return escaped + "'";
}

static std::string youtubeThumbnailUrl(const std::string &videoUrl) {
    std::string videoId;

    if (const auto shortPos = videoUrl.find("youtu.be/"); shortPos != std::string::npos) {
        videoId = videoUrl.substr(shortPos + 9);
    } else if (const auto watchPos = videoUrl.find("youtube.com/watch"); watchPos != std::string::npos) {
        const auto valuePos = videoUrl.find("v=", watchPos);
        if (valuePos != std::string::npos) videoId = videoUrl.substr(valuePos + 2);
    } else if (const auto shortsPos = videoUrl.find("youtube.com/shorts/"); shortsPos != std::string::npos) {
        videoId = videoUrl.substr(shortsPos + 19);
    } else if (const auto embedPos = videoUrl.find("youtube.com/embed/"); embedPos != std::string::npos) {
        videoId = videoUrl.substr(embedPos + 18);
    }

    const auto end = videoId.find_first_of("?&#/");
    if (end != std::string::npos) videoId.resize(end);
    return videoId.size() == 11 ? "https://i.ytimg.com/vi/" + videoId + "/hqdefault.jpg" : "";
}

static Json::Value levelToJson(const orm::Row &row) {
    Json::Value j;
    j["id"]             = static_cast<Json::Int64>(row["id"].as<long long>());
    j["rank"]           = row["rank"].as<int>();
    j["name"]           = row["name"].as<std::string>();
    j["points"]         = row["points"].as<double>();
    j["verified_by"]    = row["verified_by"].as<std::string>();
    const auto videoUrl = row["video_url"].as<std::string>();
    auto thumbnailUrl = row["thumbnail_url"].isNull() ? "" : row["thumbnail_url"].as<std::string>();
    if (thumbnailUrl.empty()) thumbnailUrl = youtubeThumbnailUrl(videoUrl);
    j["video_url"]      = videoUrl;
    j["thumbnail_url"]  = thumbnailUrl;
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
    int page = 1;
    int limit = 20;
    try {
        const auto pageParam = req->getParameter("page");
        const auto limitParam = req->getParameter("limit");
        if (!pageParam.empty()) page = std::max(1, std::stoi(pageParam));
        if (!limitParam.empty()) limit = std::min(100, std::max(1, std::stoi(limitParam)));
    } catch (...) {
        cb(pt::errorResponse(k400BadRequest, "Page and limit must be valid numbers."));
        return;
    }
    auto offset  = (page - 1) * limit;
    auto tier    = req->getParameter("tier");
    auto creator = req->getParameter("creator");
    auto q       = req->getParameter("q");
    auto minPts  = req->getParameter("min_points");

    std::string whereClauses;
    auto addWhere = [&](const std::string &clause) {
        whereClauses += whereClauses.empty() ? " WHERE " : " AND ";
        whereClauses += clause;
    };

    if (!tier.empty()) {
        addWhere("difficulty_tier = " + sqlLiteral(tier));
    }
    if (!creator.empty()) {
        addWhere(sqlLiteral(creator) + " = ANY(creators)");
    }
    if (!q.empty()) {
        addWhere("name ILIKE " + sqlLiteral("%" + q + "%"));
    }
    if (!minPts.empty()) {
        try {
            size_t consumed = 0;
            const auto points = std::stod(minPts, &consumed);
            if (consumed == minPts.size() && points >= 0) addWhere("points >= " + std::to_string(points));
        } catch (...) {}
    }

    auto dataSql  = "SELECT id, rank, name, points, verified_by, creators, video_url, thumbnail_url, difficulty_tier, created_at, "
                    "(SELECT COUNT(*) FROM records r WHERE r.level_id = demon_levels.id AND r.status = 'accepted') AS records_count, "
                    "COUNT(*) OVER() AS total_count "
                    "FROM demon_levels" + whereClauses +
                    " ORDER BY rank ASC LIMIT " + std::to_string(limit) +
                    " OFFSET " + std::to_string(offset);

    auto db = drogon::app().getDbClient();
    db->execSqlAsync(dataSql,
        [cb](const orm::Result &dataRes) mutable {
            Json::Value j;
            j["total"] = dataRes.empty() ? 0 : static_cast<Json::Int64>(dataRes[0]["total_count"].as<long long>());
            Json::Value levels(Json::arrayValue);
            for (const auto &row : dataRes) levels.append(levelToJson(row));
            j["levels"] = levels;
            cb(pt::okResponse(j));
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
