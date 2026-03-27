/**
 * @file ratelimit.cpp
 * @brief Rate limiter implementation
 */

#include "mcpp/enterprise/ratelimit.hpp"
#include <algorithm>

namespace mcpp {
namespace enterprise {

RateLimiter::RateLimiter() : config_(Config()) {}

RateLimiter::RateLimiter(const Config& config) : config_(config) {}

RateLimiter::Bucket& RateLimiter::get_bucket(const std::string& key) {
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

void RateLimiter::refill_bucket(Bucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - bucket.last_update).count();

    // Calculate tokens to add based on elapsed time
    double tokens_to_add = (elapsed / 1000.0) * config_.tokens_per_second;
    bucket.tokens = std::min(config_.max_tokens,
                            bucket.tokens + static_cast<int>(tokens_to_add));
    bucket.last_update = now;
}

RateLimiter::Result RateLimiter::check(const std::string& key) {
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

void RateLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.erase(key);
}

void RateLimiter::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.clear();
}

// ============ Global Rate Limiter ============

GlobalRateLimiter& GlobalRateLimiter::instance() {
    static GlobalRateLimiter instance;
    return instance;
}

RateLimiter::Result GlobalRateLimiter::check(const std::string& key) {
    if (!limiter_) {
        limiter_ = std::make_shared<RateLimiter>();
    }
    return limiter_->check(key);
}

void GlobalRateLimiter::configure(const RateLimiter::Config& config) {
    if (!limiter_) {
        limiter_ = std::make_shared<RateLimiter>(config);
    } else {
        limiter_.reset(new RateLimiter(config));
    }
}

void GlobalRateLimiter::reset() {
    if (limiter_) {
        limiter_->reset_all();
    }
}

} // namespace enterprise
} // namespace mcpp