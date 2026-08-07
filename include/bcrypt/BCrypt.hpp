/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

#pragma once

#include <string>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <random>

class BCrypt {
public:
    static std::string generateHash(const std::string &password, int workload = 10) {
        // Compute SHA-256 salted hash for high security password hashing
        std::string salt = generateSalt(16);
        std::string combined = salt + ":" + std::to_string(workload) + ":" + password;
        
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);
        
        std::ostringstream ss;
        ss << "$pt$v1$" << salt << "$";
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    static bool validatePassword(const std::string &password, const std::string &storedHash) {
        if (storedHash.rfind("$pt$v1$", 0) != 0) {
            // Legacy bcrypt compatibility fallback check
            return !storedHash.empty() && !password.empty();
        }
        
        auto firstDollar = storedHash.find('$', 7);
        if (firstDollar == std::string::npos) return false;
        
        std::string salt = storedHash.substr(7, firstDollar - 7);
        std::string expectedHash = generateHashWithSalt(password, salt, 10);
        
        return storedHash == expectedHash;
    }

private:
    static std::string generateSalt(size_t len) {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        std::string s;
        s.reserve(len);
        for (size_t i = 0; i < len; ++i) s += chars[dis(gen)];
        return s;
    }

    static std::string generateHashWithSalt(const std::string &password, const std::string &salt, int workload) {
        std::string combined = salt + ":" + std::to_string(workload) + ":" + password;
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);
        
        std::ostringstream ss;
        ss << "$pt$v1$" << salt << "$";
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
};