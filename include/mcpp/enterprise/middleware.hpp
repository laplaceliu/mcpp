/**
 * @file middleware.hpp
 * @brief Middleware/Filter chain framework for enterprise features
 */
#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <chrono>
#include "mcpp/core/types.hpp"
#include "mcpp/core/json.hpp"

namespace mcpp {
namespace enterprise {

/**
 * @brief HTTP request context passed through middleware chain
 */
struct RequestContext {
    std::string method;                 ///< HTTP method
    std::string path;                    ///< Request path
    std::string query;                   ///< Query string
    std::map<std::string, std::string> headers; ///< Request headers
    std::string body;                    ///< Request body
    JsonValue params;                    ///< Parsed parameters

    // Response (filled by middleware)
    int status_code = 200;               ///< HTTP status code
    std::string response_body;           ///< Response body
    std::map<std::string, std::string> response_headers; ///< Response headers

    // Auth context
    std::string user_id;                 ///< Authenticated user ID
    std::vector<std::string> roles;      ///< User roles
    bool authenticated = false;         ///< Whether request is authenticated
};

/**
 * @brief Result of middleware processing
 */
enum class MiddlewareResult {
    Continue,  ///< Continue to next middleware
    Stop,      ///< Stop processing, response already sent
    Reject    ///< Reject request (401/403)
};

/**
 * @brief Middleware handler function type
 */
using MiddlewareHandler = std::function<MiddlewareResult(RequestContext& ctx)>;

/**
 * @brief Base class for middleware
 */
class Middleware {
public:
    virtual ~Middleware() = default;

    /**
     * @brief Get middleware name
     * @return Middleware name
     */
    virtual std::string name() const = 0;

    /**
     * @brief Process the middleware
     * @param ctx Request context
     * @return MiddlewareResult
     */
    virtual MiddlewareResult process(RequestContext& ctx) = 0;

    /**
     * @brief Set the next middleware in chain
     * @param next Next middleware
     */
    void set_next(std::shared_ptr<Middleware> next) { next_ = next; }

    /**
     * @brief Get the next middleware
     * @return Next middleware or nullptr
     */
    std::shared_ptr<Middleware> next() const { return next_; }

protected:
    std::shared_ptr<Middleware> next_;
};

/**
 * @brief Filter chain that executes middlewares in order
 */
class FilterChain {
public:
    /**
     * @brief Add a middleware to the chain
     * @param middleware Middleware to add
     */
    void add(std::shared_ptr<Middleware> middleware);

    /**
     * @brief Execute the filter chain
     * @param ctx Request context
     * @return true if all middleware passed, false if rejected
     */
    bool execute(RequestContext& ctx);

    /**
     * @brief Get middleware count
     * @return Number of middlewares in chain
     */
    size_t size() const { return middlewares_.size(); }

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};

/**
 * @brief Utility to create middleware from lambda
 */
class LambdaMiddleware : public Middleware {
public:
    LambdaMiddleware(const std::string& name, MiddlewareHandler handler)
        : name_(name), handler_(handler) {}

    std::string name() const override { return name_; }
    MiddlewareResult process(RequestContext& ctx) override { return handler_(ctx); }

private:
    std::string name_;
    MiddlewareHandler handler_;
};

} // namespace enterprise
} // namespace mcpp