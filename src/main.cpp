/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "utils/env.h"
#include "utils/jwt_helper.h"
#include "utils/middleware.h"

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
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t\"'"));
        val.erase(val.find_last_not_of(" \t\"'") + 1);
        setenv(key.c_str(), val.c_str(), 0);
    }
}

int main() {
    loadEnvFile(".env");

    pt::JwtHelper::instance().setSecret(pt::env("JWT_SECRET", "changeme"));

    auto host  = pt::env("HOST", "0.0.0.0");
    auto port  = std::stoi(pt::env("PORT", "8080"));
    auto dbUrl = pt::env("DATABASE_URL");

    if (dbUrl.empty()) {
        std::cerr << "[PointerThere] FATAL: DATABASE_URL environment variable is missing!\n";
        return 1;
    }

    std::cout << "[PointerThere] Starting backend service on " << host << ":" << port << "\n";

    auto &app = drogon::app();

    app.addListener(host, port);
    app.setThreadNum(std::thread::hardware_concurrency());
    app.setLogLevel(trantor::Logger::kInfo);

    try {
        drogon::orm::PostgresConfig pgConfig;
        pgConfig.connectInfo = dbUrl;
        pgConfig.connectionNumber = 10;
        pgConfig.name = "default";
        app.addDbClient(pgConfig);
        std::cout << "[PointerThere] PostgreSQL client initialized successfully.\n";
    } catch (const std::exception &e) {
        std::cerr << "[PointerThere] ERROR initializing PostgreSQL client: " << e.what() << "\n";
    }

    app.registerPostHandlingAdvice([](const drogon::HttpRequestPtr &, const drogon::HttpResponsePtr &resp) {
        pt::addCorsHeaders(resp);
    });

    app.setCustom404Page([] {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value j; j["error"] = "Not found."; return j;
        }());
        resp->setStatusCode(drogon::k404NotFound);
        pt::addCorsHeaders(resp);
        return resp;
    }());

    std::cout << "[PointerThere] Backend ready and listening on port " << port << "\n";
    app.run();
    return 0;
}
