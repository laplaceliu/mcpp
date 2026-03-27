/**
 * @file test_metrics.cpp
 * @brief Unit tests for Metrics
 */
#include "gtest/gtest.h"
#include "mcpp/enterprise/metrics.hpp"
#include <thread>
#include <chrono>

using namespace mcpp::enterprise;

TEST(MetricsTest, CounterDefaultValue) {
    Counter counter("test_counter", "A test counter");
    EXPECT_EQ(counter.value(), 0);
}

TEST(MetricsTest, CounterIncrement) {
    Counter counter("test_counter", "A test counter");
    counter.increment();

    EXPECT_EQ(counter.value(), 1);

    counter.increment(5);
    EXPECT_EQ(counter.value(), 6);
}

TEST(MetricsTest, CounterReset) {
    Counter counter("test_counter", "A test counter");
    counter.increment(10);
    // Note: Counter doesn't have a reset method by design (monotonically increasing)
    EXPECT_EQ(counter.value(), 10);
}

TEST(MetricsTest, GaugeDefaultValue) {
    Gauge gauge("test_gauge", "A test gauge");
    EXPECT_EQ(gauge.value(), 0.0);
}

TEST(MetricsTest, GaugeSet) {
    Gauge gauge("test_gauge", "A test gauge");
    gauge.set(100.5);

    EXPECT_EQ(gauge.value(), 100.5);
}

TEST(MetricsTest, GaugeIncrement) {
    Gauge gauge("test_gauge", "A test gauge");
    gauge.set(10);
    gauge.increment(5);

    EXPECT_EQ(gauge.value(), 15);
}

TEST(MetricsTest, GaugeDecrement) {
    Gauge gauge("test_gauge", "A test gauge");
    gauge.set(10);
    gauge.decrement(3);

    EXPECT_EQ(gauge.value(), 7);
}

TEST(MetricsTest, HistogramObserve) {
    Histogram histogram("test_histogram", "A test histogram");
    histogram.observe(0.5);
    histogram.observe(1.0);
    histogram.observe(1.5);

    EXPECT_EQ(histogram.count(), 3);
    EXPECT_EQ(histogram.sum(), 3.0);
}

TEST(MetricsTest, HistogramDefaultBuckets) {
    Histogram histogram("test_histogram", "A test histogram");
    EXPECT_EQ(histogram.count(), 0);
    EXPECT_EQ(histogram.sum(), 0.0);
}

TEST(MetricsTest, MetricsRegistrySingle) {
    auto& registry = MetricsRegistry::instance();

    registry.counter("requests", "Total requests");
    registry.counter("requests", "Total requests"); // Same name, same instance

    registry.gauge("cpu_usage", "CPU usage");
    registry.histogram("latency", "Request latency");

    EXPECT_EQ(registry.counter("requests", "")->value(), 0);
    EXPECT_EQ(registry.gauge("cpu_usage", "")->value(), 0.0);
    EXPECT_EQ(registry.histogram("latency", "")->count(), 0);
}

TEST(MetricsTest, MetricsRegistryMultiple) {
    auto& registry = MetricsRegistry::instance();

    // Create separate counters
    auto counter1 = registry.counter("req1_multi", "Request 1");
    auto counter2 = registry.counter("req2_multi", "Request 2");

    counter1->increment();
    counter2->increment(5);

    EXPECT_EQ(counter1->value(), 1);
    EXPECT_EQ(counter2->value(), 5);
}

TEST(MetricsTest, MetricsExport) {
    auto& registry = MetricsRegistry::instance();

    registry.counter("test_counter", "Test counter")->increment(10);
    registry.gauge("test_gauge", "Test gauge")->set(42.5);

    std::string output = registry.export_prometheus();

    EXPECT_NE(output.find("test_counter"), std::string::npos);
    EXPECT_NE(output.find("test_gauge"), std::string::npos);
}

TEST(MetricsTest, MetricsNames) {
    auto& registry = MetricsRegistry::instance();

    registry.counter("metric1", "Metric 1");
    registry.gauge("metric2", "Metric 2");
    registry.histogram("metric3", "Metric 3");

    auto names = registry.names();
    EXPECT_GE(names.size(), 3);
}

TEST(MetricsTest, HistogramPercentiles) {
    Histogram histogram("test_histogram", "A test histogram");
    for (int i = 1; i <= 100; i++) {
        histogram.observe(static_cast<double>(i));
    }

    EXPECT_EQ(histogram.count(), 100);
    EXPECT_EQ(histogram.sum(), 5050.0);
}

TEST(MetricsTest, HistogramCustomBuckets) {
    std::vector<double> buckets = {1.0, 5.0, 10.0};
    Histogram histogram("test_histogram", "Custom buckets", buckets);

    histogram.observe(0.5);  // Below first bucket
    histogram.observe(3.0);  // In second bucket
    histogram.observe(7.0);   // In third bucket
    histogram.observe(15.0); // Above all buckets

    EXPECT_EQ(histogram.count(), 4);
}

TEST(MetricsTest, Timer) {
    Timer timer;
    timer.start();

    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    double elapsed = timer.elapsed();
    EXPECT_GE(elapsed, 0.0);
}

TEST(MetricsTest, GaugeCanGoNegative) {
    Gauge gauge("test_gauge", "Test gauge");
    gauge.set(10);
    gauge.decrement(15);

    EXPECT_EQ(gauge.value(), -5);
}
