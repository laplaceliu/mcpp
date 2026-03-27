/**
 * @file metrics.hpp
 * @brief Prometheus-style metrics collector
 */
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <chrono>
#include <functional>
#include <sstream>
#include <iomanip>

namespace mcpp {
namespace enterprise {

/**
 * @brief Metric types supported
 */
enum class MetricType {
    Counter,   ///< Monotonically increasing value
    Gauge,     ///< Can go up or down
    Histogram, ///< Distribution of values
    Summary    ///< Quantile statistics
};

/**
 * @brief Base class for all metrics
 */
class Metric {
public:
    virtual ~Metric() = default;

    /**
     * @brief Get metric name
     * @return Name
     */
    const std::string& name() const { return name_; }

    /**
     * @brief Get metric help text
     * @return Help text
     */
    const std::string& help() const { return help_; }

    /**
     * @brief Get metric type
     * @return MetricType
     */
    virtual MetricType type() const = 0;

    /**
     * @brief Format metric for Prometheus export
     * @return Formatted string
     */
    virtual std::string format() const = 0;

protected:
    Metric(const std::string& name, const std::string& help)
        : name_(name), help_(help) {}

    std::string name_;
    std::string help_;
};

/**
 * @brief Counter metric - monotonically increasing
 */
class Counter : public Metric {
public:
    Counter(const std::string& name, const std::string& help) : Metric(name, help) {}

    MetricType type() const override { return MetricType::Counter; }

    /**
     * @brief Increment counter by value
     * @param value Value to add
     */
    void increment(double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ += value;
    }

    /**
     * @brief Get current value
     * @return Current value
     */
    double value() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

    std::string format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "# TYPE " << name_ << " counter\n";
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << name_ << " " << std::fixed << std::setprecision(3) << value_ << "\n";
        return oss.str();
    }

private:
    mutable std::mutex mutex_;
    double value_ = 0.0;
};

/**
 * @brief Gauge metric - can increase or decrease
 */
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::string& help) : Metric(name, help) {}

    MetricType type() const override { return MetricType::Gauge; }

    /**
     * @brief Set gauge to value
     * @param value New value
     */
    void set(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = value;
    }

    /**
     * @brief Increment gauge
     * @param value Value to add
     */
    void increment(double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ += value;
    }

    /**
     * @brief Decrement gauge
     * @param value Value to subtract
     */
    void decrement(double value = 1.0) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ -= value;
    }

    /**
     * @brief Get current value
     * @return Current value
     */
    double value() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

    std::string format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "# TYPE " << name_ << " gauge\n";
        oss << "# HELP " << name_ << " " << help_ << "\n";
        oss << name_ << " " << std::fixed << std::setprecision(3) << value_ << "\n";
        return oss.str();
    }

private:
    mutable std::mutex mutex_;
    double value_ = 0.0;
};

/**
 * @brief Histogram metric - buckets for distribution
 */
class Histogram : public Metric {
public:
    /**
     * @brief Bucket configuration
     */
    struct Bucket {
        double bound;              ///< Upper bound
        uint64_t count = 0;      ///< Count in this bucket
    };

    Histogram(const std::string& name, const std::string& help,
              const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0})
        : Metric(name, help), buckets_() {
        for (double bound : buckets) {
            Bucket b;
            b.bound = bound;
            buckets_.push_back(b);
        }
    }

    MetricType type() const override { return MetricType::Histogram; }

    /**
     * @brief Observe a value
     * @param value Value to record
     */
    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ += value;
        count_++;

        for (auto& bucket : buckets_) {
            if (value <= bucket.bound) {
                bucket.count++;
            }
        }
    }

    /**
     * @brief Get sum of observed values
     * @return Sum
     */
    double sum() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sum_;
    }

    /**
     * @brief Get total count
     * @return Count
     */
    uint64_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    std::string format() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        oss << "# TYPE " << name_ << " histogram\n";
        oss << "# HELP " << name_ << " " << help_ << "\n";

        uint64_t cumulative = 0;
        for (const auto& bucket : buckets_) {
            cumulative += bucket.count;
            oss << name_ << "_bucket{le=\"" << bucket.bound << "\"} " << cumulative << "\n";
        }
        oss << name_ << "_bucket{le=\"+Inf\"} " << count_ << "\n";
        oss << name_ << "_sum " << std::fixed << std::setprecision(6) << sum_ << "\n";
        oss << name_ << "_count " << count_ << "\n";

        return oss.str();
    }

private:
    mutable std::mutex mutex_;
    std::vector<Bucket> buckets_;
    double sum_ = 0.0;
    uint64_t count_ = 0;
};

/**
 * @brief Metrics registry - singleton for all metrics
 */
class MetricsRegistry {
public:
    static MetricsRegistry& instance() {
        static MetricsRegistry instance;
        return instance;
    }

    /**
     * @brief Register a counter
     * @param name Metric name
     * @param help Help text
     * @return Counter instance
     */
    std::shared_ptr<Counter> counter(const std::string& name, const std::string& help) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto metric = std::make_shared<Counter>(name, help);
        metrics_[name] = metric;
        return metric;
    }

    /**
     * @brief Register a gauge
     * @param name Metric name
     * @param help Help text
     * @return Gauge instance
     */
    std::shared_ptr<Gauge> gauge(const std::string& name, const std::string& help) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto metric = std::make_shared<Gauge>(name, help);
        metrics_[name] = metric;
        return metric;
    }

    /**
     * @brief Register a histogram
     * @param name Metric name
     * @param help Help text
     * @param buckets Bucket boundaries
     * @return Histogram instance
     */
    std::shared_ptr<Histogram> histogram(const std::string& name, const std::string& help,
                                         const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0}) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto metric = std::make_shared<Histogram>(name, help, buckets);
        metrics_[name] = metric;
        return metric;
    }

    /**
     * @brief Export all metrics in Prometheus format
     * @return Formatted metrics string
     */
    std::string export_prometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream oss;
        for (const auto& pair : metrics_) {
            oss << pair.second->format();
        }
        return oss.str();
    }

    /**
     * @brief Get all metric names
     * @return Vector of metric names
     */
    std::vector<std::string> names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& pair : metrics_) {
            result.push_back(pair.first);
        }
        return result;
    }

private:
    MetricsRegistry() = default;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<Metric>> metrics_;
};

/**
 * @brief Timer for measuring duration
 */
class Timer {
public:
    using Clock = std::chrono::steady_clock;

    /**
     * @brief Start the timer
     */
    void start() { start_ = Clock::now(); }

    /**
     * @brief Stop the timer and return duration in seconds
     * @return Duration in seconds
     */
    double stop() {
        auto end = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        return duration.count() / 1000000.0;
    }

    /**
     * @brief Get elapsed time without stopping
     * @return Elapsed seconds
     */
    double elapsed() const {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
        return duration.count() / 1000000.0;
    }

private:
    Clock::time_point start_;
};

/**
 * @brief Scoped timer that records to histogram when destroyed
 */
class ScopedTimer {
public:
    ScopedTimer(std::shared_ptr<Histogram> histogram)
        : histogram_(histogram) {
        timer_.start();
    }

    ~ScopedTimer() {
        histogram_->observe(timer_.stop());
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::shared_ptr<Histogram> histogram_;
    Timer timer_;
};

} // namespace enterprise
} // namespace mcpp