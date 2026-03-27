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
    CircuitBreaker(const std::string& name, const Config& config)
        : config_(config), state_(CircuitState::Closed),
          consecutive_failures_(0), consecutive_successes_(0), name_(name) {}

    /**
     * @brief Check if request is allowed
     * @return true if circuit allows request
     */
    bool allow_request() {
        std::lock_guard<std::mutex> lock(mutex_);

        switch (state_.load()) {
            case CircuitState::Closed:
                return true;

            case CircuitState::Open: {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_failure_time_).count();
                if (elapsed >= config_.timeout_seconds) {
                    transition_to(CircuitState::HalfOpen);
                    return true;
                }
                return false;
            }

            case CircuitState::HalfOpen:
                // Allow limited requests in half-open state
                return true;
        }
        return false;
    }

    /**
     * @brief Record a successful request
     */
    void record_success() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == CircuitState::HalfOpen) {
            consecutive_successes_++;
            if (consecutive_successes_ >= config_.success_threshold) {
                transition_to(CircuitState::Closed);
            }
        } else if (state_ == CircuitState::Closed) {
            // Reset failure count on success
            consecutive_failures_ = 0;
        }
    }

    /**
     * @brief Record a failed request
     */
    void record_failure() {
        std::lock_guard<std::mutex> lock(mutex_);

        consecutive_failures_++;
        last_failure_time_ = std::chrono::steady_clock::now();

        if (state_ == CircuitState::HalfOpen) {
            // Immediately open on failure in half-open
            transition_to(CircuitState::Open);
        } else if (state_ == CircuitState::Closed) {
            if (consecutive_failures_ >= config_.failure_threshold) {
                transition_to(CircuitState::Open);
            }
        }
    }

    /**
     * @brief Get current state
     * @return Current circuit state
     */
    State state() const {
        State s;
        s.state = state_.load();
        s.consecutive_failures = consecutive_failures_.load();
        s.consecutive_successes = consecutive_successes_.load();
        s.last_failure_time = last_failure_time_;
        s.name = name_;
        return s;
    }

    /**
     * @brief Get configuration
     * @return Configuration
     */
    const Config& config() const { return config_; }

    /**
     * @brief Reset circuit to closed state
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        transition_to(CircuitState::Closed);
    }

    /**
     * @brief Force state change (for testing/admin)
     * @param new_state State to force
     */
    void force_state(CircuitState new_state) {
        std::lock_guard<std::mutex> lock(mutex_);
        transition_to(new_state);
    }

private:
    void transition_to(CircuitState new_state) {
        if (state_ != new_state) {
            state_ = new_state;
            consecutive_failures_ = 0;
            consecutive_successes_ = 0;
        }
    }

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
    static CircuitBreakerRegistry& instance() {
        static CircuitBreakerRegistry instance;
        return instance;
    }

    /**
     * @brief Get or create a circuit breaker
     * @param name Circuit name
     * @param config Configuration
     * @return Circuit breaker instance
     */
    std::shared_ptr<CircuitBreaker> get(const std::string& name,
                                        const CircuitBreaker::Config& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = circuits_.find(name);
        if (it != circuits_.end()) {
            return it->second;
        }

        auto breaker = std::make_shared<CircuitBreaker>(name, config);
        circuits_[name] = breaker;
        return breaker;
    }

    /**
     * @brief Get all circuit breakers
     * @return Map of name to circuit breaker
     */
    std::map<std::string, std::shared_ptr<CircuitBreaker>> all() {
        std::lock_guard<std::mutex> lock(mutex_);
        return circuits_;
    }

    /**
     * @brief Reset all circuits
     */
    void reset_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& pair : circuits_) {
            pair.second->reset();
        }
    }

private:
    CircuitBreakerRegistry() = default;
    std::map<std::string, std::shared_ptr<CircuitBreaker>> circuits_;
    std::mutex mutex_;
};

} // namespace enterprise
} // namespace mcpp