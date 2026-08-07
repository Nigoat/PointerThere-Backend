/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <regex>
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

static void parseDbUrl(const std::string &url, std::string &host, unsigned short &port, 
                       std::string &user, std::string &password, std::string &database) {
    std::regex pattern(R"(postgresql://([^:]+):([^@]+)@([^:/]+):(\d+)/(.+))");
    std::smatch match;
    
    if (std::regex_match(url, match, pattern)) {
        user = match[1];
        password = match[2];
        host = match[3];
        port = static_cast<unsigned short>(std::stoi(match[4]));
        database = match[5];
    } else {
        throw std::runtime_error("Invalid DATABASE_URL format. Expected: postgresql://user:password@host:port/database");
    }
}

int main() {
    loadEnvFile(".env");

    pt::JwtHelper::instance().setSecret(pt::env("JWT_SECRET", "changeme"));

    auto host  = pt::env("HOST", "0.0.0.0");
    auto port  = std::stoi(pt::env("PORT", "8080"));
    auto dbUrl = pt::env("DATABASE_URL");

    if (dbUrl.empty()) {
        std::cerr << "[PointerThere] ERROR: DATABASE_URL is not set!\n";
        return 1;
    }

    std::cout << "[PointerThere] Backend starting on " << host << ":" << port << "\n";

    auto &app = drogon::app();

    app.addListener(host, port);
    app.setThreadNum(std::thread::hardware_concurrency());
    app.setLogLevel(trantor::Logger::kInfo);

    // Parse DATABASE_URL and configure PostgreSQL connection
    std::string dbHost, dbUser, dbPassword, dbName;
    unsigned short dbPort;
    try {
        parseDbUrl(dbUrl, dbHost, dbPort, dbUser, dbPassword, dbName);
    } catch (const std::exception &e) {
        std::cerr << "[PointerThere] ERROR: " << e.what() << "\n";
        return 1;
    }

    drogon::orm::PostgresConfig pgConfig;
    pgConfig.host = dbHost;
    pgConfig.port = dbPort;
    pgConfig.username = dbUser;
    pgConfig.password = dbPassword;
    pgConfig.databaseName = dbName;
    pgConfig.connectionNumber = 10;
    pgConfig.name = "default";
    app.addDbClient(pgConfig);

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

    app.run();
    return 0;
}
