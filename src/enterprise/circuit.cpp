/**
 * @file circuit.cpp
 * @brief Circuit breaker implementation
 */

#include "mcpp/enterprise/circuit.hpp"

namespace mcpp {
namespace enterprise {

CircuitBreaker::CircuitBreaker(const std::string& name, const Config& config)
    : config_(config), state_(CircuitState::Closed),
      consecutive_failures_(0), consecutive_successes_(0), name_(name) {
}

bool CircuitBreaker::allow_request() {
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

void CircuitBreaker::record_success() {
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

void CircuitBreaker::record_failure() {
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

CircuitBreaker::State CircuitBreaker::state() const {
    State s;
    s.state = state_.load();
    s.consecutive_failures = consecutive_failures_.load();
    s.consecutive_successes = consecutive_successes_.load();
    s.last_failure_time = last_failure_time_;
    s.name = name_;
    return s;
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    transition_to(CircuitState::Closed);
}

void CircuitBreaker::force_state(CircuitState new_state) {
    std::lock_guard<std::mutex> lock(mutex_);
    transition_to(new_state);
}

void CircuitBreaker::transition_to(CircuitState new_state) {
    if (state_ != new_state) {
        state_ = new_state;
        consecutive_failures_ = 0;
        consecutive_successes_ = 0;
    }
}

// ============ Circuit Breaker Registry ============

CircuitBreakerRegistry& CircuitBreakerRegistry::instance() {
    static CircuitBreakerRegistry instance;
    return instance;
}

std::shared_ptr<CircuitBreaker> CircuitBreakerRegistry::get(
    const std::string& name, const CircuitBreaker::Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = circuits_.find(name);
    if (it != circuits_.end()) {
        return it->second;
    }

    auto breaker = std::make_shared<CircuitBreaker>(name, config);
    circuits_[name] = breaker;
    return breaker;
}

std::map<std::string, std::shared_ptr<CircuitBreaker>> CircuitBreakerRegistry::all() {
    std::lock_guard<std::mutex> lock(mutex_);
    return circuits_;
}

void CircuitBreakerRegistry::reset_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : circuits_) {
        pair.second->reset();
    }
}

} // namespace enterprise
} // namespace mcpp