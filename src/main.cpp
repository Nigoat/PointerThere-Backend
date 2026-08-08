/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#include <drogon/drogon.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
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

// Fail-safe PostgreSQL URL parser
static void parsePostgresUrl(const std::string &url, std::string &host, unsigned short &port,
                            std::string &user, std::string &password, std::string &database) {
    port = 5432;
    std::string str = url;
    const std::string p1 = "postgresql://";
    const std::string p2 = "postgres://";
    if (str.rfind(p1, 0) == 0) str = str.substr(p1.length());
    else if (str.rfind(p2, 0) == 0) str = str.substr(p2.length());

    auto atPos = str.find('@');
    if (atPos != std::string::npos) {
        std::string userPass = str.substr(0, atPos);
        std::string hostDb = str.substr(atPos + 1);

        auto colonPos = userPass.find(':');
        if (colonPos != std::string::npos) {
            user = userPass.substr(0, colonPos);
            password = userPass.substr(colonPos + 1);
        } else {
            user = userPass;
        }

        auto slashPos = hostDb.find('/');
        if (slashPos != std::string::npos) {
            std::string hostPort = hostDb.substr(0, slashPos);
            std::string dbParams = hostDb.substr(slashPos + 1);

            auto qPos = dbParams.find('?');
            database = (qPos != std::string::npos) ? dbParams.substr(0, qPos) : dbParams;

            auto hColon = hostPort.find(':');
            if (hColon != std::string::npos) {
                host = hostPort.substr(0, hColon);
                try { port = static_cast<unsigned short>(std::stoi(hostPort.substr(hColon + 1))); } catch (...) {}
            } else {
                host = hostPort;
            }
        } else {
            host = hostDb;
        }
    }
}

int main() {
    loadEnvFile(".env");

    pt::JwtHelper::instance().setSecret(pt::env("JWT_SECRET", "changeme"));

    auto port = std::stoi(pt::env("PORT", "8080"));
    auto dbUrl = pt::env("DATABASE_URL");

    std::cout << "[PointerThere] Starting backend service on 0.0.0.0:" << port << "\n";

    auto &app = drogon::app();
    app.addListener("0.0.0.0", port);
    app.setThreadNum(std::thread::hardware_concurrency());
    app.setLogLevel(trantor::Logger::kInfo);

    if (!dbUrl.empty()) {
        std::string dbHost, dbUser, dbPassword, dbName;
        unsigned short dbPort = 5432;

        try {
            parsePostgresUrl(dbUrl, dbHost, dbPort, dbUser, dbPassword, dbName);
            std::cout << "[PointerThere] DB Config -> Host: " << dbHost << ", Port: " << dbPort << ", User: " << dbUser << ", DB: " << dbName << "\n";
            
            drogon::orm::PostgresConfig pgConfig;
            pgConfig.host = dbHost;
            pgConfig.port = dbPort;
            pgConfig.username = dbUser;
            pgConfig.password = dbPassword;
            pgConfig.databaseName = dbName;
            pgConfig.connectionNumber = 10;
            pgConfig.name = "default";
            app.addDbClient(pgConfig);

            drogon::orm::PostgresConfig pgConfigFast = pgConfig;
            pgConfigFast.isFast = true;
            app.addDbClient(pgConfigFast);

            drogon::orm::PostgresConfig pgConfig2 = pgConfig;
            pgConfig2.name = "";
            app.addDbClient(pgConfig2);

            drogon::orm::PostgresConfig pgConfig2Fast = pgConfig2;
            pgConfig2Fast.isFast = true;
            app.addDbClient(pgConfig2Fast);

            std::cout << "[PointerThere] PostgreSQL database clients (standard & fast) added successfully.\n";
        } catch (const std::exception &e) {
            std::cerr << "[PointerThere] ERROR initializing DB client: " << e.what() << "\n";
        }
    } else {
        std::cerr << "[PointerThere] WARNING: DATABASE_URL environment variable is empty!\n";
    }

    app.registerHandler("/", [](const drogon::HttpRequestPtr &,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
        Json::Value j;
        j["status"] = "online";
        j["service"] = "PointerThere Backend";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
        pt::addCorsHeaders(resp);
        cb(resp);
    }, {drogon::Get});

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

    std::cout << "[PointerThere] Backend ready and listening on 0.0.0.0:" << port << "\n";
    app.run();
    return 0;
}
