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

    BearerAuth();
    BearerAuth(const Config& config);

    /**
     * @brief Generate a token for a user
     * @param user User to generate token for
     * @return Token string
     */
    std::string generate_token(const User& user);

    /**
     * @brief Validate a bearer token
     * @param token Token to validate
     * @return AuthResult with validation details
     */
    AuthResult validate_token(const std::string& token);

    /**
     * @brief Revoke a token
     * @param token Token to revoke
     */
    void revoke_token(const std::string& token);

    /**
     * @brief Register a user
     * @param user User to register
     */
    void register_user(const User& user);

    /**
     * @brief Get user by ID
     * @param user_id User ID
     * @return User or nullptr if not found
     */
    std::shared_ptr<User> get_user(const std::string& user_id);

    /**
     * @brief Authenticate user with username/password
     * @param username Username
     * @param password Password
     * @return AuthResult with authentication details
     */
    AuthResult authenticate(const std::string& username, const std::string& password);

    /**
     * @brief Extract bearer token from Authorization header
     * @param header Authorization header value
     * @return Token string or empty if not found
     */
    static std::string extract_token(const std::string& header);

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
MiddlewareHandler make_auth_middleware(
    std::shared_ptr<BearerAuth> auth,
    const std::vector<std::string>& required_roles = {});

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
                               const std::string& action);

    /**
     * @brief Add a role -> permission mapping
     * @param role Role name
     * @param permission Permission to allow
     */
    static void add_role_permission(const std::string& role, const Permission& permission);

private:
    static std::map<std::string, std::vector<Permission>>& role_permissions();
};

} // namespace enterprise
} // namespace mcpp