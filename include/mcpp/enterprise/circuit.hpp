/**
 * @file circuit.hpp
 * @brief Circuit breaker pattern implementation
 */
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <functional>
#include <chrono>

namespace mcpp {
namespace enterprise {

/**
 * @brief Circuit breaker states
 */
enum class CircuitState {
    Closed,   ///< Normal operation, requests pass through
    Open,     ///< Circuit is open, requests are rejected
    HalfOpen  ///< Testing if service has recovered
};

/**
 * @brief Circuit breaker for protecting against cascading failures
 */
class CircuitBreaker {
public:
    /**
     * @brief Configuration for circuit breaker
     */
    struct Config {
        int failure_threshold = 5;         ///< Failures before opening circuit
        int success_threshold = 3;        ///< Successes in half-open to close
        int timeout_seconds = 30;         ///< Seconds before trying half-open
        int max_consecutive_failures = 10; ///< Max failures before fast reject
    };

    /**
     * @brief Circuit state with metadata
     */
    struct State {
        CircuitState state;
        int consecutive_failures;
        int consecutive_successes;
        std::chrono::steady_clock::time_point last_failure_time;
        std::string name;
    };

    /**
     * @brief Construct a circuit breaker
     * @param name Circuit name (e.g., service identifier)
     * @param config Configuration
     */
    CircuitBreaker(const std::string& name, const Config& config);

    /**
     * @brief Check if request is allowed
     * @return true if circuit allows request
     */
    bool allow_request();

    /**
     * @brief Record a successful request
     */
    void record_success();

    /**
     * @brief Record a failed request
     */
    void record_failure();

    /**
     * @brief Get current state
     * @return Current circuit state
     */
    State state() const;

    /**
     * @brief Get configuration
     * @return Configuration
     */
    const Config& config() const { return config_; }

    /**
     * @brief Reset circuit to closed state
     */
    void reset();

    /**
     * @brief Force state change (for testing/admin)
     * @param new_state State to force
     */
    void force_state(CircuitState new_state);

private:
    void transition_to(CircuitState new_state);

    Config config_;
    std::atomic<CircuitState> state_;
    std::atomic<int> consecutive_failures_;
    std::atomic<int> consecutive_successes_;
    std::chrono::steady_clock::time_point last_failure_time_;
    std::string name_;
    std::mutex mutex_;
};

/**
 * @brief Execute a function with circuit breaker protection
 * @param breaker Circuit breaker to use
 * @param func Function to execute
 * @param fallback Fallback function if circuit is open
 * @return Result of func or fallback
 */
template<typename T>
T execute_with_circuit(
    CircuitBreaker& breaker,
    std::function<T()> func,
    std::function<T()> fallback) {

    if (!breaker.allow_request()) {
        return fallback();
    }

    try {
        T result = func();
        breaker.record_success();
        return result;
    } catch (const std::exception& e) {
        breaker.record_failure();
        return fallback();
    }
}

/**
 * @brief Global circuit breaker registry
 */
class CircuitBreakerRegistry {
public:
    static CircuitBreakerRegistry& instance();

    /**
     * @brief Get or create a circuit breaker
     * @param name Circuit name
     * @param config Configuration
     * @return Circuit breaker instance
     */
    std::shared_ptr<CircuitBreaker> get(const std::string& name,
                                        const CircuitBreaker::Config& config);

    /**
     * @brief Get all circuit breakers
     * @return Map of name to circuit breaker
     */
    std::map<std::string, std::shared_ptr<CircuitBreaker>> all();

    /**
     * @brief Reset all circuits
     */
    void reset_all();

private:
    CircuitBreakerRegistry() = default;
    std::map<std::string, std::shared_ptr<CircuitBreaker>> circuits_;
    std::mutex mutex_;
};

} // namespace enterprise
} // namespace mcpp