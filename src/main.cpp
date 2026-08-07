/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2024 PointerThere — GPLv3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "utils/env.h"
#include "utils/jwt_helper.h"
#include "utils/middleware.h"

/**
 * Load a .env file into the environment.
 * Each line should be KEY=VALUE (comments with # are ignored).
 */
static void loadEnvFile(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t\"'"));
        val.erase(val.find_last_not_of(" \t\"'") + 1);
        setenv(key.c_str(), val.c_str(), 0);  // 0 = don't override existing env vars
    }
}

int main() {
    // Load .env file (environment variables set before startup take precedence)
    loadEnvFile(".env");

    // ── Configure JWT ────────────────────────────────────────────────────────
    pt::JwtHelper::instance().setSecret(pt::env("JWT_SECRET", "changeme"));

    // ── Configure Drogon ─────────────────────────────────────────────────────
    auto host = pt::env("HOST", "0.0.0.0");
    auto port = std::stoi(pt::env("PORT", "8080"));
    auto dbUrl = pt::env("DATABASE_URL");

    if (dbUrl.empty()) {
        std::cerr << "[PointerThere] ERROR: DATABASE_URL is not set!\n";
        return 1;
    }

    std::cout << "[PointerThere] Backend starting on " << host << ":" << port << "\n";

    drogon::app()
        // Server
        .setListeners({{host, port}})
        .setThreadNum(std::thread::hardware_concurrency())
        .setLogPath("./logs")
        .setLogLevel(trantor::Logger::kInfo)
        .setDocumentRoot("./static")

        // Database
        .addDbClient(drogon::orm::DbConfig{
            .dbType     = drogon::orm::ClientType::PostgreSQL,
            .host       = "",
            .port       = 5432,
            .databaseName = "",
            .userName   = "",
            .password   = "",
            .connectionString = dbUrl,
            .connectionNumber = 10,
            .name       = "default"
        })

        // CORS (applied globally via filter)
        .registerFilter<pt::CorsFilter>()

        // Global before-handler: add CORS to all responses
        .registerHandlerViaRegex(".*",
            [](const drogon::HttpRequestPtr &,
               std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                pt::CorsFilter::addCorsHeaders(resp);
                cb(resp);
            },
            {drogon::Options})

        // Static 404 handler
        .setCustom404Page([] {
            auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
                Json::Value j; j["error"] = "Not found."; return j;
            }());
            resp->setStatusCode(drogon::k404NotFound);
            return resp;
        }())

        .run();

    return 0;
}
