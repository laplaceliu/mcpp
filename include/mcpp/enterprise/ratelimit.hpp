/**
 * @file ratelimit.hpp
 * @brief Token bucket rate limiter implementation
 */
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include "mcpp/enterprise/middleware.hpp"

namespace mcpp {
namespace enterprise {

/**
 * @brief Token bucket rate limiter
 * @details Implements the token bucket algorithm for request rate limiting
 */
class RateLimiter {
public:
    /**
     * @brief Configuration for rate limiter
     */
    struct Config {
        int tokens_per_second = 100;    ///< Tokens added per second
        int max_tokens = 100;          ///< Maximum bucket capacity
        int tokens_per_request = 1;    ///< Tokens consumed per request
    };

    /**
     * @brief Result of rate limit check
     */
    struct Result {
        bool allowed;                  ///< Whether request is allowed
        int remaining_tokens;          ///< Tokens remaining after check
        int retry_after_ms;            ///< Milliseconds until more tokens available
    };

    /**
     * @brief Construct a rate limiter with default config
     */
    RateLimiter();

    /**
     * @brief Construct a rate limiter
     * @param config Rate limiter configuration
     */
    explicit RateLimiter(const Config& config);

    /**
     * @brief Check if a request is allowed
     * @param key Identifier for the rate limit bucket (e.g., user_id, IP)
     * @return Result with allowed status and metadata
     */
    Result check(const std::string& key);

    /**
     * @brief Reset rate limit for a key
     * @param key Identifier to reset
     */
    void reset(const std::string& key);

    /**
     * @brief Reset all rate limits
     */
    void reset_all();

    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const Config& config() const { return config_; }

private:
    struct Bucket {
        int tokens;
        std::chrono::steady_clock::time_point last_update;
    };

    Bucket& get_bucket(const std::string& key);
    void refill_bucket(Bucket& bucket);

    Config config_;
    std::map<std::string, Bucket> buckets_;
    std::mutex mutex_;
};

/**
 * @brief Global rate limiter instance
 */
class GlobalRateLimiter {
public:
    static GlobalRateLimiter& instance();

    /**
     * @brief Check rate limit for a key
     * @param key Rate limit key
     * @return Result
     */
    RateLimiter::Result check(const std::string& key);

    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void configure(const RateLimiter::Config& config);

    /**
     * @brief Reset all limits
     */
    void reset();

private:
    GlobalRateLimiter() = default;
    std::shared_ptr<RateLimiter> limiter_;
};

} // namespace enterprise
} // namespace mcpp