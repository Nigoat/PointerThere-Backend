/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <sstream>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <drogon/drogon.h>

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

    static std::string base64UrlEncode(const std::string &input) {
        BIO *bio, *b64;
        BUF_MEM *bufferPtr;
        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        BIO_write(bio, input.data(), static_cast<int>(input.size()));
        BIO_flush(bio);
        BIO_get_mem_ptr(bio, &bufferPtr);

        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);

        for (auto &c : result) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }
        return result;
    }

    static std::string base64UrlDecode(std::string input) {
        for (auto &c : input) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }
        while (input.size() % 4 != 0) {
            input.push_back('=');
        }

        BIO *bio, *b64;
        int decodeLen = static_cast<int>(input.size());
        std::string result(decodeLen, '\0');

        b64 = BIO_new(BIO_f_base64());
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        bio = BIO_new_mem_buf(input.data(), decodeLen);
        bio = BIO_push(b64, bio);

        int actualLen = BIO_read(bio, &result[0], decodeLen);
        BIO_free_all(bio);

        if (actualLen > 0) {
            result.resize(actualLen);
        } else {
            result.clear();
        }
        return result;
    }

    static std::string hmacSha256(const std::string &data, const std::string &key) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char*>(data.data()), data.size(),
             hash, &len);
        return std::string(reinterpret_cast<char*>(hash), len);
    }

    std::string generate(const std::string &user_id,
                         const std::string &email,
                         bool is_admin = false,
                         int expires_hours = 720) const {
        Json::Value header;
        header["alg"] = "HS256";
        header["typ"] = "JWT";

        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        Json::Value payload;
        payload["iss"]      = "pointerthere";
        payload["sub"]      = user_id;
        payload["email"]    = email;
        payload["is_admin"] = is_admin ? "1" : "0";
        payload["iat"]      = static_cast<Json::Int64>(now);
        payload["exp"]      = static_cast<Json::Int64>(now + (expires_hours * 3600));

        Json::FastWriter writer;
        std::string headerEncoded  = base64UrlEncode(writer.write(header));
        std::string payloadEncoded = base64UrlEncode(writer.write(payload));

        std::string unsignedToken = headerEncoded + "." + payloadEncoded;
        std::string signature = base64UrlEncode(hmacSha256(unsignedToken, m_secret));

        return unsignedToken + "." + signature;
    }

    std::optional<JwtPayload> verify(const std::string &token) const {
        auto firstDot  = token.find('.');
        auto secondDot = token.rfind('.');

        if (firstDot == std::string::npos || secondDot == std::string::npos || firstDot == secondDot) {
            return std::nullopt;
        }

        std::string unsignedToken = token.substr(0, secondDot);
        std::string signature     = token.substr(secondDot + 1);

        std::string expectedSignature = base64UrlEncode(hmacSha256(unsignedToken, m_secret));
        if (signature != expectedSignature) {
            return std::nullopt;
        }

        std::string payloadJsonStr = base64UrlDecode(token.substr(firstDot + 1, secondDot - firstDot - 1));
        
        Json::CharReaderBuilder builder;
        Json::Value payloadJson;
        std::istringstream ss(payloadJsonStr);
        std::string errs;
        if (!Json::parseFromStream(builder, ss, &payloadJson, &errs)) {
            return std::nullopt;
        }

        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (payloadJson.isMember("exp") && payloadJson["exp"].asInt64() < now) {
            return std::nullopt;
        }

        JwtPayload payload;
        payload.user_id  = payloadJson["sub"].asString();
        payload.email    = payloadJson["email"].asString();
        payload.is_admin = (payloadJson["is_admin"].asString() == "1");
        return payload;
    }

    static std::optional<std::string> extractBearer(const drogon::HttpRequestPtr &req) {
        auto auth = req->getHeader("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            return auth.substr(7);
        }
        return std::nullopt;
    }

private:
    std::string m_secret{"pointerthere_jwt_default_secret"};
};

} // namespace pt
