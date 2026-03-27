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
    Counter(const std::string& name, const std::string& help);

    MetricType type() const override { return MetricType::Counter; }

    /**
     * @brief Increment counter by value
     * @param value Value to add
     */
    void increment(double value = 1.0);

    /**
     * @brief Get current value
     * @return Current value
     */
    double value() const;

    std::string format() const override;

private:
    mutable std::mutex mutex_;
    double value_ = 0.0;
};

/**
 * @brief Gauge metric - can increase or decrease
 */
class Gauge : public Metric {
public:
    Gauge(const std::string& name, const std::string& help);

    MetricType type() const override { return MetricType::Gauge; }

    /**
     * @brief Set gauge to value
     * @param value New value
     */
    void set(double value);

    /**
     * @brief Increment gauge
     * @param value Value to add
     */
    void increment(double value = 1.0);

    /**
     * @brief Decrement gauge
     * @param value Value to subtract
     */
    void decrement(double value = 1.0);

    /**
     * @brief Get current value
     * @return Current value
     */
    double value() const;

    std::string format() const override;

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
              const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0});

    MetricType type() const override { return MetricType::Histogram; }

    /**
     * @brief Observe a value
     * @param value Value to record
     */
    void observe(double value);

    /**
     * @brief Get sum of observed values
     * @return Sum
     */
    double sum() const;

    /**
     * @brief Get total count
     * @return Count
     */
    uint64_t count() const;

    std::string format() const override;

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
    static MetricsRegistry& instance();

    /**
     * @brief Register a counter
     * @param name Metric name
     * @param help Help text
     * @return Counter instance
     */
    std::shared_ptr<Counter> counter(const std::string& name, const std::string& help);

    /**
     * @brief Register a gauge
     * @param name Metric name
     * @param help Help text
     * @return Gauge instance
     */
    std::shared_ptr<Gauge> gauge(const std::string& name, const std::string& help);

    /**
     * @brief Register a histogram
     * @param name Metric name
     * @param help Help text
     * @param buckets Bucket boundaries
     * @return Histogram instance
     */
    std::shared_ptr<Histogram> histogram(const std::string& name, const std::string& help,
                                         const std::vector<double>& buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0});

    /**
     * @brief Export all metrics in Prometheus format
     * @return Formatted metrics string
     */
    std::string export_prometheus() const;

    /**
     * @brief Get all metric names
     * @return Vector of metric names
     */
    std::vector<std::string> names() const;

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
    double stop();

    /**
     * @brief Get elapsed time without stopping
     * @return Elapsed seconds
     */
    double elapsed() const;

private:
    Clock::time_point start_;
};

/**
 * @brief Scoped timer that records to histogram when destroyed
 */
class ScopedTimer {
public:
    ScopedTimer(std::shared_ptr<Histogram> histogram);
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::shared_ptr<Histogram> histogram_;
    Timer timer_;
};

} // namespace enterprise
} // namespace mcpp