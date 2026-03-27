/**
 * @file test_ratelimit.cpp
 * @brief Unit tests for RateLimiter
 */
#include "gtest/gtest.h"
#include "mcpp/enterprise/ratelimit.hpp"

using namespace mcpp::enterprise;

TEST(RateLimiterTest, DefaultConfig) {
    RateLimiter limiter;
    EXPECT_EQ(limiter.config().tokens_per_second, 100);
    EXPECT_EQ(limiter.config().max_tokens, 100);
    EXPECT_EQ(limiter.config().tokens_per_request, 1);
}

TEST(RateLimiterTest, CustomConfig) {
    RateLimiter::Config config;
    config.tokens_per_second = 10;
    config.max_tokens = 50;
    config.tokens_per_request = 5;

    RateLimiter limiter(config);
    EXPECT_EQ(limiter.config().tokens_per_second, 10);
    EXPECT_EQ(limiter.config().max_tokens, 50);
    EXPECT_EQ(limiter.config().tokens_per_request, 5);
}

TEST(RateLimiterTest, CheckAllowed) {
    RateLimiter limiter;
    auto result = limiter.check("user1");

    EXPECT_TRUE(result.allowed);
    EXPECT_GE(result.remaining_tokens, 0);
}

TEST(RateLimiterTest, MultipleRequests) {
    RateLimiter limiter;
    int allowed_count = 0;

    for (int i = 0; i < 10; i++) {
        auto result = limiter.check("user1");
        if (result.allowed) {
            allowed_count++;
        }
    }

    EXPECT_EQ(allowed_count, 10);
}

TEST(RateLimiterTest, Reset) {
    RateLimiter limiter;
    limiter.check("user1");
    limiter.reset("user1");

    auto result = limiter.check("user1");
    EXPECT_TRUE(result.allowed);
}

TEST(RateLimiterTest, ResetAll) {
    RateLimiter limiter;
    limiter.check("user1");
    limiter.check("user2");
    limiter.reset_all();

    auto result1 = limiter.check("user1");
    auto result2 = limiter.check("user2");
    EXPECT_TRUE(result1.allowed);
    EXPECT_TRUE(result2.allowed);
}

TEST(RateLimiterTest, DifferentKeys) {
    RateLimiter limiter;

    auto result1 = limiter.check("user1");
    auto result2 = limiter.check("user2");

    EXPECT_TRUE(result1.allowed);
    EXPECT_TRUE(result2.allowed);
}

TEST(RateLimiterTest, LowTokenLimit) {
    RateLimiter::Config config;
    config.tokens_per_second = 1;
    config.max_tokens = 1;
    config.tokens_per_request = 1;

    RateLimiter limiter(config);

    auto result1 = limiter.check("user1");
    EXPECT_TRUE(result1.allowed);

    auto result2 = limiter.check("user1");
    EXPECT_FALSE(result2.allowed);
}

TEST(RateLimiterTest, GlobalRateLimiter) {
    auto& global = GlobalRateLimiter::instance();
    RateLimiter::Config config;
    config.tokens_per_second = 100;
    config.max_tokens = 100;
    global.configure(config);

    auto result = global.check("global_user");
    EXPECT_TRUE(result.allowed);

    global.reset();
}
