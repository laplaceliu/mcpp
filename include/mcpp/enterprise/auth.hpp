/**
 * @file auth.hpp
 * @brief Authentication and authorization implementation
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "mcpp/enterprise/middleware.hpp"

namespace mcpp {
namespace enterprise {

/**
 * @brief Authentication result
 */
struct AuthResult {
    bool success;                    ///< Whether authentication succeeded
    std::string user_id;            ///< Authenticated user ID
    std::vector<std::string> roles;  ///< User roles
    std::string error_message;       ///< Error message if failed
};

/**
 * @brief User information
 */
struct User {
    std::string id;                  ///< User ID
    std::string name;                ///< User name
    std::string password_hash;       ///< Hashed password
    std::vector<std::string> roles;  ///< User roles
    bool enabled = true;             ///< Whether user is enabled
};

/**
 * @brief Token information
 */
struct Token {
    std::string access_token;        ///< Bearer token
    std::string user_id;             ///< Associated user ID
    int64_t expires_at;              ///< Expiration timestamp (seconds since epoch)
    std::vector<std::string> scopes; ///< Token scopes
};

/**
 * @brief Bearer token authenticator
 */
class BearerAuth {
public:
    /**
     * @brief Configuration for bearer auth
     */
    struct Config {
        int token_expiry_seconds = 3600;     ///< Token lifetime (1 hour default)
        std::string issuer = "mcpp-server";  ///< Token issuer
        std::string audience = "mcpp-client"; ///< Expected audience
    };

    BearerAuth() : config_(Config()) {}
    BearerAuth(const Config& config) : config_(config) {}

    /**
     * @brief Generate a token for a user
     * @param user User to generate token for
     * @return Token string
     */
    std::string generate_token(const User& user) {
        std::lock_guard<std::mutex> lock(mutex_);

        Token token;
        // Generate a random token (in production, use a cryptographically secure random)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::ostringstream token_oss;
        for (int i = 0; i < 32; ++i) {
            token_oss << std::hex << dis(gen);
        }
        token.access_token = token_oss.str();
        token.user_id = user.id;
        token.expires_at = std::chrono::system_clock::now().time_since_epoch().count() / 1000 +
                            config_.token_expiry_seconds;
        token.scopes = user.roles;

        tokens_[token.access_token] = token;

        return token.access_token;
    }

    /**
     * @brief Validate a bearer token
     * @param token Token to validate
     * @return AuthResult with validation details
     */
    AuthResult validate_token(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return {false, "", {}, "Invalid or malformed token"};
        }

        const Token& parsed_token = it->second;

        // Check expiration
        auto now = std::chrono::system_clock::now().time_since_epoch().count() / 1000;
        if (parsed_token.expires_at < now) {
            return {false, "", {}, "Token has expired"};
        }

        auto user_it = users_.find(parsed_token.user_id);
        if (user_it == users_.end()) {
            return {false, "", {}, "User not found"};
        }

        const User& user = user_it->second;
        if (!user.enabled) {
            return {false, "", {}, "User account is disabled"};
        }

        return {true, user.id, user.roles, ""};
    }

    /**
     * @brief Revoke a token
     * @param token Token to revoke
     */
    void revoke_token(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.erase(token);
    }

    /**
     * @brief Register a user
     * @param user User to register
     */
    void register_user(const User& user) {
        std::lock_guard<std::mutex> lock(mutex_);
        users_[user.id] = user;
        users_by_name_[user.name] = user;
    }

    /**
     * @brief Get user by ID
     * @param user_id User ID
     * @return User or nullptr if not found
     */
    std::shared_ptr<User> get_user(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(user_id);
        if (it != users_.end()) {
            return std::make_shared<User>(it->second);
        }
        return nullptr;
    }

    /**
     * @brief Authenticate user with username/password
     * @param username Username
     * @param password Password
     * @return AuthResult with authentication details
     */
    AuthResult authenticate(const std::string& username, const std::string& password) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto user_it = users_by_name_.find(username);
        if (user_it == users_by_name_.end()) {
            return {false, "", {}, "Invalid username or password"};
        }

        const User& user = user_it->second;
        if (!user.enabled) {
            return {false, "", {}, "User account is disabled"};
        }

        // Simple password check (in production, use bcrypt or similar)
        if (user.password_hash != password) {
            return {false, "", {}, "Invalid username or password"};
        }

        std::string token = generate_token(user);
        return {true, user.id, user.roles, ""};
    }

    /**
     * @brief Extract bearer token from Authorization header
     * @param header Authorization header value
     * @return Token string or empty if not found
     */
    static std::string extract_token(const std::string& header) {
        if (header.substr(0, 7) == "Bearer ") {
            return header.substr(7);
        }
        return "";
    }

private:
    std::string sign_token(const Token& token);
    bool verify_token(const std::string& token, Token& out_token);

    Config config_;
    std::map<std::string, User> users_;
    std::map<std::string, User> users_by_name_;
    std::map<std::string, Token> tokens_;
    std::mutex mutex_;
};

/**
 * @brief Create bearer auth middleware
 * @param auth Authenticator instance
 * @param required_roles Roles required for access (empty = any authenticated)
 * @return Middleware handler
 */
inline MiddlewareHandler make_auth_middleware(
    std::shared_ptr<BearerAuth> auth,
    const std::vector<std::string>& required_roles = {}) {

    return [auth, required_roles](RequestContext& ctx) -> MiddlewareResult {
        auto auth_header_it = ctx.headers.find("Authorization");
        if (auth_header_it == ctx.headers.end()) {
            ctx.status_code = 401;
            ctx.response_body = "{\"error\": \"Missing Authorization header\"}";
            return MiddlewareResult::Reject;
        }

        std::string token = BearerAuth::extract_token(auth_header_it->second);
        if (token.empty()) {
            ctx.status_code = 401;
            ctx.response_body = "{\"error\": \"Invalid Authorization header format\"}";
            return MiddlewareResult::Reject;
        }

        AuthResult result = auth->validate_token(token);
        if (!result.success) {
            ctx.status_code = 401;
            ctx.response_body = "{\"error\": \"" + result.error_message + "\"}";
            return MiddlewareResult::Reject;
        }

        // Check role requirements
        if (!required_roles.empty()) {
            bool has_role = false;
            for (const auto& required : required_roles) {
                for (const auto& user_role : result.roles) {
                    if (user_role == required || user_role == "admin") {
                        has_role = true;
                        break;
                    }
                }
                if (has_role) break;
            }
            if (!has_role) {
                ctx.status_code = 403;
                ctx.response_body = "{\"error\": \"Insufficient permissions\"}";
                return MiddlewareResult::Reject;
            }
        }

        ctx.authenticated = true;
        ctx.user_id = result.user_id;
        ctx.roles = result.roles;

        return MiddlewareResult::Continue;
    };
}

/**
 * @brief Simple role-based access control
 */
class RBAC {
public:
    /**
     * @brief Permission definition
     */
    struct Permission {
        std::string resource;  ///< Resource name
        std::string action;     ///< Action (read, write, delete)
    };

    /**
     * @brief Check if a role has a permission
     * @param role Role to check
     * @param resource Resource name
     * @param action Action name
     * @return true if allowed
     */
    static bool has_permission(const std::vector<std::string>& roles,
                               const std::string& resource,
                               const std::string& action) {
        for (const auto& role : roles) {
            if (role == "admin") {
                return true; // Admin has all permissions
            }

            auto it = role_permissions().find(role);
            if (it != role_permissions().end()) {
                for (const auto& perm : it->second) {
                    if ((perm.resource == resource || perm.resource == "*") &&
                        (perm.action == action || perm.action == "*")) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    /**
     * @brief Add a role -> permission mapping
     * @param role Role name
     * @param permission Permission to allow
     */
    static void add_role_permission(const std::string& role, const Permission& permission) {
        role_permissions()[role].push_back(permission);
    }

private:
    static std::map<std::string, std::vector<Permission>>& role_permissions() {
        static std::map<std::string, std::vector<Permission>> instance;
        return instance;
    }
};

} // namespace enterprise
} // namespace mcpp