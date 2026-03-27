/**
 * @file test_circuit.cpp
 * @brief Unit tests for CircuitBreaker
 */
#include "gtest/gtest.h"
#include "mcpp/enterprise/circuit.hpp"
#include <thread>
#include <chrono>

using namespace mcpp::enterprise;

TEST(CircuitBreakerTest, DefaultConfig) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());

    EXPECT_EQ(breaker.config().failure_threshold, 5);
    EXPECT_EQ(breaker.config().success_threshold, 3);
    EXPECT_EQ(breaker.config().timeout_seconds, 30);
}

TEST(CircuitBreakerTest, InitialState) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());
    auto state = breaker.state();

    EXPECT_EQ(state.state, CircuitState::Closed);
}

TEST(CircuitBreakerTest, AllowRequestInClosedState) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());
    EXPECT_TRUE(breaker.allow_request());
}

TEST(CircuitBreakerTest, OpensAfterThreshold) {
    CircuitBreaker::Config config;
    config.failure_threshold = 3;
    CircuitBreaker breaker("test", config);

    breaker.record_failure();
    breaker.record_failure();

    EXPECT_TRUE(breaker.allow_request());

    breaker.record_failure();

    EXPECT_FALSE(breaker.allow_request());
}

TEST(CircuitBreakerTest, ResetClosesOpenCircuit) {
    CircuitBreaker::Config config;
    config.failure_threshold = 2;
    CircuitBreaker breaker("test", config);

    // Trip the circuit
    breaker.record_failure();
    breaker.record_failure();
    EXPECT_FALSE(breaker.allow_request());

    // Reset should close it
    breaker.reset();
    EXPECT_TRUE(breaker.allow_request());
}

TEST(CircuitBreakerTest, ForceStateChangesState) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());

    EXPECT_EQ(breaker.state().state, CircuitState::Closed);

    breaker.force_state(CircuitState::HalfOpen);
    EXPECT_EQ(breaker.state().state, CircuitState::HalfOpen);

    breaker.force_state(CircuitState::Closed);
    EXPECT_EQ(breaker.state().state, CircuitState::Closed);
}

TEST(CircuitBreakerTest, StateName) {
    CircuitBreaker breaker("my-circuit", CircuitBreaker::Config());
    EXPECT_EQ(breaker.state().name, "my-circuit");
}

TEST(CircuitBreakerTest, Registry) {
    auto& registry = CircuitBreakerRegistry::instance();

    CircuitBreaker::Config config;
    config.failure_threshold = 5;

    auto breaker1 = registry.get("service1", config);
    auto breaker2 = registry.get("service1", config);

    EXPECT_EQ(breaker1, breaker2);

    registry.reset_all();
}

TEST(CircuitBreakerTest, RegistryMultiple) {
    auto& registry = CircuitBreakerRegistry::instance();

    CircuitBreaker::Config config;

    auto breaker1 = registry.get("service1", config);
    auto breaker2 = registry.get("service2", config);

    EXPECT_NE(breaker1, breaker2);

    auto all = registry.all();
    EXPECT_GE(all.size(), 2);

    registry.reset_all();
}

TEST(CircuitBreakerTest, ExecuteWithCircuitSuccess) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());

    auto result = execute_with_circuit<int>(
        breaker,
        []() { return 42; },
        []() { return -1; }
    );

    EXPECT_EQ(result, 42);
}

TEST(CircuitBreakerTest, ExecuteWithCircuitFallback) {
    CircuitBreaker::Config config;
    config.failure_threshold = 1;
    CircuitBreaker breaker("test", config);

    breaker.record_failure();

    auto result = execute_with_circuit<int>(
        breaker,
        []() { return 42; },
        []() { return -1; }
    );

    EXPECT_EQ(result, -1);
}

TEST(CircuitBreakerTest, HalfOpenAllowsRequest) {
    CircuitBreaker breaker("test", CircuitBreaker::Config());

    breaker.force_state(CircuitState::HalfOpen);
    EXPECT_TRUE(breaker.allow_request());
}

TEST(CircuitBreakerTest, OpenRejectsRequest) {
    CircuitBreaker::Config config;
    config.failure_threshold = 1;
    CircuitBreaker breaker("test", config);

    breaker.record_failure();
    EXPECT_FALSE(breaker.allow_request());
}
