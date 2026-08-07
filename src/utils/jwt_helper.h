/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <stdexcept>
#include <jwt-cpp/jwt.h>

namespace pt {

struct JwtPayload {
    std::string user_id;
    std::string email;
    bool        is_admin{false};
};

class JwtHelper {
public:
    static JwtHelper &instance() {
        static JwtHelper inst;
        return inst;
    }

    void setSecret(const std::string &secret) { m_secret = secret; }

    std::string generate(const std::string &user_id,
                         const std::string &email,
                         bool is_admin = false,
                         int expires_hours = 720) const {
        auto now = std::chrono::system_clock::now();
        return jwt::create()
            .set_issuer("pointerthere")
            .set_subject(user_id)
            .set_payload_claim("email",    jwt::claim(email))
            .set_payload_claim("is_admin", jwt::claim(std::to_string(is_admin)))
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::hours(expires_hours))
            .sign(jwt::algorithm::hs256{m_secret});
    }

    std::optional<JwtPayload> verify(const std::string &token) const {
        try {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{m_secret})
                .with_issuer("pointerthere");
            verifier.verify(decoded);

            JwtPayload payload;
            payload.user_id  = decoded.get_subject();
            payload.email    = decoded.get_payload_claim("email").as_string();
            payload.is_admin = (decoded.get_payload_claim("is_admin").as_string() == "1");
            return payload;
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::optional<std::string> extractBearer(const drogon::HttpRequestPtr &req) {
        auto auth = req->getHeader("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            return auth.substr(7);
        }
        return std::nullopt;
    }

private:
    std::string m_secret;
};

}
