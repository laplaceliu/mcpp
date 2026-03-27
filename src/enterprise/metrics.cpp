/**
 * @file metrics.cpp
 * @brief Metrics implementation
 */

#include "mcpp/enterprise/metrics.hpp"
#include <sstream>
#include <iomanip>

namespace mcpp {
namespace enterprise {

// ============ Counter ============

Counter::Counter(const std::string& name, const std::string& help)
    : Metric(name, help) {}

void Counter::increment(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

double Counter::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

std::string Counter::format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "# TYPE " << name_ << " counter\n";
    oss << "# HELP " << name_ << " " << help_ << "\n";
    oss << name_ << " " << std::fixed << std::setprecision(3) << value_ << "\n";
    return oss.str();
}

// ============ Gauge ============

Gauge::Gauge(const std::string& name, const std::string& help)
    : Metric(name, help) {}

void Gauge::set(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
}

void Gauge::increment(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ += value;
}

void Gauge::decrement(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ -= value;
}

double Gauge::value() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

std::string Gauge::format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "# TYPE " << name_ << " gauge\n";
    oss << "# HELP " << name_ << " " << help_ << "\n";
    oss << name_ << " " << std::fixed << std::setprecision(3) << value_ << "\n";
    return oss.str();
}

// ============ Histogram ============

Histogram::Histogram(const std::string& name, const std::string& help,
                    const std::vector<double>& buckets)
    : Metric(name, help), buckets_() {
    for (double bound : buckets) {
        Bucket b;
        b.bound = bound;
        buckets_.push_back(b);
    }
}

void Histogram::observe(double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    sum_ += value;
    count_++;

    for (auto& bucket : buckets_) {
        if (value <= bucket.bound) {
            bucket.count++;
        }
    }
}

double Histogram::sum() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sum_;
}

uint64_t Histogram::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

std::string Histogram::format() const {
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

// ============ Metrics Registry ============

MetricsRegistry& MetricsRegistry::instance() {
    static MetricsRegistry instance;
    return instance;
}

std::shared_ptr<Counter> MetricsRegistry::counter(const std::string& name,
                                                   const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto metric = std::make_shared<Counter>(name, help);
    metrics_[name] = metric;
    return metric;
}

std::shared_ptr<Gauge> MetricsRegistry::gauge(const std::string& name,
                                               const std::string& help) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto metric = std::make_shared<Gauge>(name, help);
    metrics_[name] = metric;
    return metric;
}

std::shared_ptr<Histogram> MetricsRegistry::histogram(const std::string& name,
                                                      const std::string& help,
                                                      const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto metric = std::make_shared<Histogram>(name, help, buckets);
    metrics_[name] = metric;
    return metric;
}

std::string MetricsRegistry::export_prometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    for (const auto& pair : metrics_) {
        oss << pair.second->format();
    }
    return oss.str();
}

std::vector<std::string> MetricsRegistry::names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& pair : metrics_) {
        result.push_back(pair.first);
    }
    return result;
}

// ============ Timer ============

double Timer::stop() {
    auto end = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    return duration.count() / 1000000.0;
}

double Timer::elapsed() const {
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - start_);
    return duration.count() / 1000000.0;
}

// ============ ScopedTimer ============

ScopedTimer::ScopedTimer(std::shared_ptr<Histogram> histogram)
    : histogram_(histogram) {
    timer_.start();
}

ScopedTimer::~ScopedTimer() {
    histogram_->observe(timer_.stop());
}

} // namespace enterprise
} // namespace mcpp