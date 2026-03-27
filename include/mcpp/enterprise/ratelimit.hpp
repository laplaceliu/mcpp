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
    RateLimiter() : config_(Config()) {}

    /**
     * @brief Construct a rate limiter
     * @param config Rate limiter configuration
     */
    explicit RateLimiter(const Config& config) : config_(config) {}

    /**
     * @brief Check if a request is allowed
     * @param key Identifier for the rate limit bucket (e.g., user_id, IP)
     * @return Result with allowed status and metadata
     */
    Result check(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);

        Bucket& bucket = get_bucket(key);
        refill_bucket(bucket);

        Result result;
        result.remaining_tokens = bucket.tokens;

        if (bucket.tokens >= config_.tokens_per_request) {
            bucket.tokens -= config_.tokens_per_request;
            result.allowed = true;
            result.retry_after_ms = 0;
        } else {
            result.allowed = false;
            // Calculate time until enough tokens are available
            int tokens_needed = config_.tokens_per_request - bucket.tokens;
            result.retry_after_ms = (tokens_needed * 1000) / config_.tokens_per_second;
            if (result.retry_after_ms < 100) {
                result.retry_after_ms = 100;
            }
        }

        return result;
    }

    /**
     * @brief Reset rate limit for a key
     * @param key Identifier to reset
     */
    void reset(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        buckets_.erase(key);
    }

    /**
     * @brief Reset all rate limits
     */
    void reset_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        buckets_.clear();
    }

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

    Bucket& get_bucket(const std::string& key) {
        auto now = std::chrono::steady_clock::now();
        auto it = buckets_.find(key);
        if (it == buckets_.end()) {
            Bucket bucket;
            bucket.tokens = config_.max_tokens;
            bucket.last_update = now;
            buckets_[key] = bucket;
            return buckets_[key];
        }
        return it->second;
    }

    void refill_bucket(Bucket& bucket) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - bucket.last_update).count();

        // Calculate tokens to add based on elapsed time
        double tokens_to_add = (elapsed / 1000.0) * config_.tokens_per_second;
        bucket.tokens = std::min(config_.max_tokens,
                                bucket.tokens + static_cast<int>(tokens_to_add));
        bucket.last_update = now;
    }

    Config config_;
    std::map<std::string, Bucket> buckets_;
    std::mutex mutex_;
};

/**
 * @brief Global rate limiter instance
 */
class GlobalRateLimiter {
public:
    static GlobalRateLimiter& instance() {
        static GlobalRateLimiter instance;
        return instance;
    }

    /**
     * @brief Check rate limit for a key
     * @param key Rate limit key
     * @return Result
     */
    RateLimiter::Result check(const std::string& key) {
        if (!limiter_) {
            limiter_ = std::make_shared<RateLimiter>();
        }
        return limiter_->check(key);
    }

    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void configure(const RateLimiter::Config& config) {
        if (!limiter_) {
            limiter_ = std::make_shared<RateLimiter>(config);
        } else {
            limiter_.reset(new RateLimiter(config));
        }
    }

    /**
     * @brief Reset all limits
     */
    void reset() {
        if (limiter_) {
            limiter_->reset_all();
        }
    }

private:
    GlobalRateLimiter() = default;
    std::shared_ptr<RateLimiter> limiter_;
};

} // namespace enterprise
} // namespace mcpp