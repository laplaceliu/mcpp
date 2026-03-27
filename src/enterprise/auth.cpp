/**
 * @file auth.cpp
 * @brief Authentication implementation
 */

#include "mcpp/enterprise/auth.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace mcpp {
namespace enterprise {

// ============ BearerAuth ============

BearerAuth::BearerAuth() : config_(Config()) {}

BearerAuth::BearerAuth(const Config& config) : config_(config) {}

std::string BearerAuth::generate_token(const User& user) {
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

AuthResult BearerAuth::validate_token(const std::string& token) {
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

void BearerAuth::revoke_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    tokens_.erase(token);
}

void BearerAuth::register_user(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    users_[user.id] = user;
    users_by_name_[user.name] = user;
}

std::shared_ptr<User> BearerAuth::get_user(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(user_id);
    if (it != users_.end()) {
        return std::make_shared<User>(it->second);
    }
    return nullptr;
}

AuthResult BearerAuth::authenticate(const std::string& username, const std::string& password) {
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

std::string BearerAuth::extract_token(const std::string& header) {
    if (header.substr(0, 7) == "Bearer ") {
        return header.substr(7);
    }
    return "";
}

// ============ Auth Middleware ============

MiddlewareHandler make_auth_middleware(
    std::shared_ptr<BearerAuth> auth,
    const std::vector<std::string>& required_roles) {

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

// ============ RBAC ============

static std::map<std::string, std::vector<RBAC::Permission>> g_role_permissions;

std::map<std::string, std::vector<RBAC::Permission>>& RBAC::role_permissions() {
    return g_role_permissions;
}

bool RBAC::has_permission(const std::vector<std::string>& roles,
                          const std::string& resource,
                          const std::string& action) {
    for (const auto& role : roles) {
        if (role == "admin") {
            return true; // Admin has all permissions
        }

        auto it = g_role_permissions.find(role);
        if (it != g_role_permissions.end()) {
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

void RBAC::add_role_permission(const std::string& role, const Permission& permission) {
    g_role_permissions[role].push_back(permission);
}

} // namespace enterprise
} // namespace mcpp