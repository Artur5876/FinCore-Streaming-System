#include <gtest/gtest.h>

#include "cli/process_metrics.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace fincore::cli {
namespace {

TEST(ProcessDeltaTest, CalculatesAllDifferences)
{
    const ProcessSnapshot before{
        .user_cpu_us = 1'000,
        .system_cpu_us = 500,
        .current_rss_kb = 2'000,
        .peak_rss_kb = 3'000,
        .minor_faults = 10,
        .major_faults = 3,
        .voluntary_context_switches = 20,
        .involuntary_context_switches = 4,
    };

    const ProcessSnapshot after{
        .user_cpu_us = 1'400,
        .system_cpu_us = 700,
        .current_rss_kb = 2'500,
        .peak_rss_kb = 3'500,
        .minor_faults = 17,
        .major_faults = 5,
        .voluntary_context_switches = 29,
        .involuntary_context_switches = 10,
    };

    const ProcessDelta delta = process_delta(before, after);

    EXPECT_EQ(delta.user_cpu_us, 400);
    EXPECT_EQ(delta.system_cpu_us, 200);
    EXPECT_EQ(delta.total_cpu_us(), 600);

    EXPECT_EQ(delta.rss_delta_kb, 500);
    EXPECT_EQ(delta.current_rss_kb, 2'500);
    EXPECT_EQ(delta.peak_rss_kb, 3'500);

    EXPECT_EQ(delta.minor_faults, 7);
    EXPECT_EQ(delta.major_faults, 2);
    EXPECT_EQ(delta.voluntary_context_switches, 9);
    EXPECT_EQ(delta.involuntary_context_switches, 6);
}

TEST(ProcessDeltaTest, SupportsNegativeRssDelta)
{
    ProcessSnapshot before;
    before.current_rss_kb = 4'096;

    ProcessSnapshot after;
    after.current_rss_kb = 2'048;

    const ProcessDelta delta = process_delta(before, after);

    EXPECT_EQ(delta.rss_delta_kb, -2'048);
    EXPECT_EQ(delta.current_rss_kb, 2'048);
}

TEST(ProcessDeltaTest, CalculatesCpuPercent)
{
    ProcessDelta delta;
    delta.user_cpu_us = 300;
    delta.system_cpu_us = 200;

    EXPECT_DOUBLE_EQ(delta.cpu_percent(2'000), 25.0);
}

TEST(ProcessDeltaTest, ZeroWallTimeReturnsZeroCpuPercent)
{
    ProcessDelta delta;
    delta.user_cpu_us = 500;
    delta.system_cpu_us = 500;

    EXPECT_DOUBLE_EQ(delta.cpu_percent(0), 0.0);
    EXPECT_DOUBLE_EQ(delta.cpu_percent(-1), 0.0);
}

TEST(ProcessSnapshotTest, ReadsCurrentProcessInformation)
{
    const ProcessSnapshot snapshot = read_process_snapshot();

    EXPECT_GE(snapshot.user_cpu_us, 0);
    EXPECT_GE(snapshot.system_cpu_us, 0);
    EXPECT_GE(snapshot.current_rss_kb, 0);
    EXPECT_GE(snapshot.peak_rss_kb, 0);
    EXPECT_GE(snapshot.minor_faults, 0);
    EXPECT_GE(snapshot.major_faults, 0);
    EXPECT_GE(snapshot.voluntary_context_switches, 0);
    EXPECT_GE(snapshot.involuntary_context_switches, 0);
}

TEST(OperationMetricsTest, PrintsCompleteSuccessfulOperation)
{
    OperationMetrics metrics;
    metrics.symbol = "IBM";
    metrics.success = true;
    metrics.api_cached = true;
    metrics.redis_quote_stored = true;
    metrics.redis_book_stored = true;

    metrics.api_us = 500;
    metrics.redis_quote_us = 1'500;
    metrics.book_build_us = 50;
    metrics.redis_book_us = 2'000;
    metrics.total_us = 4'000;

    metrics.process.user_cpu_us = 300;
    metrics.process.system_cpu_us = 200;
    metrics.process.current_rss_kb = 2'048;
    metrics.process.rss_delta_kb = 512;
    metrics.process.peak_rss_kb = 4'096;
    metrics.process.minor_faults = 4;
    metrics.process.major_faults = 1;
    metrics.process.voluntary_context_switches = 3;
    metrics.process.involuntary_context_switches = 2;

    std::ostringstream output;
    print_operation_metrics(output, metrics);

    const std::string text = output.str();

    EXPECT_NE(text.find("[metrics] IBM SUCCESS"), std::string::npos);
    EXPECT_NE(text.find("500 us"), std::string::npos);
    EXPECT_NE(text.find("1.500 ms"), std::string::npos);
    EXPECT_NE(text.find("2.000 ms"), std::string::npos);
    EXPECT_NE(text.find("4.000 ms"), std::string::npos);

    EXPECT_NE(
        text.find("AlphaVantageClient cache"),
        std::string::npos);

    EXPECT_NE(text.find("250.00 ops/s"), std::string::npos);
    EXPECT_NE(text.find("12.50%"), std::string::npos);

    EXPECT_NE(text.find("2.00 MiB"), std::string::npos);
    EXPECT_NE(text.find("+0.50 MiB"), std::string::npos);
    EXPECT_NE(text.find("4.00 MiB"), std::string::npos);
}

TEST(OperationMetricsTest, PrintsNotReachedForMissingStages)
{
    OperationMetrics metrics;
    metrics.symbol = "AAPL";
    metrics.success = false;
    metrics.total_us = 100;

    std::ostringstream output;
    print_operation_metrics(output, metrics);

    const std::string text = output.str();

    EXPECT_NE(text.find("[metrics] AAPL FAILED"), std::string::npos);
    EXPECT_NE(text.find("API get_quote"), std::string::npos);
    EXPECT_NE(text.find("not reached"), std::string::npos);
    EXPECT_EQ(text.find("API source"), std::string::npos);
}

TEST(SessionStatsTest, EmptyStatsHaveNoSamples)
{
    SessionStats stats;

    EXPECT_FALSE(stats.last().has_value());

    std::ostringstream output;
    stats.print_summary(output);

    const std::string text = output.str();

    EXPECT_NE(text.find("attempts             0"), std::string::npos);
    EXPECT_NE(text.find("successes            0"), std::string::npos);
    EXPECT_NE(text.find("failures              0"), std::string::npos);
    EXPECT_NE(text.find("cache hit rate       0.00%"), std::string::npos);
    EXPECT_NE(text.find("no samples"), std::string::npos);
}

TEST(SessionStatsTest, RecordsSuccessFailureAndCacheResults)
{
    SessionStats stats;

    OperationMetrics first;
    first.symbol = "IBM";
    first.success = true;
    first.api_cached = true;
    first.redis_quote_stored = true;
    first.redis_book_stored = false;
    first.api_us = 100;
    first.redis_quote_us = 200;
    first.book_build_us = 300;
    first.redis_book_us = 400;
    first.total_us = 1'000;
    first.process.user_cpu_us = 50;
    first.process.system_cpu_us = 25;
    first.process.current_rss_kb = 2'048;
    first.process.peak_rss_kb = 3'072;

    OperationMetrics second;
    second.symbol = "AAPL";
    second.success = false;
    second.api_cached = false;
    second.redis_quote_stored = false;
    second.redis_book_stored = true;
    second.api_us = 300;
    second.redis_quote_us = 400;
    second.book_build_us = 500;
    second.redis_book_us = 600;
    second.total_us = 2'000;
    second.process.user_cpu_us = 100;
    second.process.system_cpu_us = 50;
    second.process.current_rss_kb = 4'096;
    second.process.peak_rss_kb = 5'120;

    stats.record(first);
    stats.record(second);

    ASSERT_TRUE(stats.last().has_value());
    EXPECT_EQ(stats.last()->symbol, "AAPL");

    std::ostringstream output;
    stats.print_summary(output);

    const std::string text = output.str();

    EXPECT_NE(text.find("attempts             2"), std::string::npos);
    EXPECT_NE(text.find("successes            1"), std::string::npos);
    EXPECT_NE(text.find("failures              1"), std::string::npos);

    EXPECT_NE(text.find("cache hits           1"), std::string::npos);
    EXPECT_NE(text.find("cache misses         1"), std::string::npos);
    EXPECT_NE(text.find("cache hit rate       50.00%"), std::string::npos);

    EXPECT_NE(text.find("quote writes ok      1"), std::string::npos);
    EXPECT_NE(text.find("quote writes failed  1"), std::string::npos);
    EXPECT_NE(text.find("book writes ok       1"), std::string::npos);
    EXPECT_NE(text.find("book writes failed   1"), std::string::npos);

    EXPECT_NE(text.find("150 us"), std::string::npos);
    EXPECT_NE(text.find("5.00 MiB"), std::string::npos);
}

TEST(SessionStatsTest, CalculatesLatencyPercentiles)
{
    SessionStats stats;

    for (const std::int64_t value : {100, 200, 300, 400}) {
        OperationMetrics metrics;
        metrics.symbol = "IBM";
        metrics.success = true;
        metrics.api_us = value;
        metrics.total_us = value;

        stats.record(metrics);
    }

    std::ostringstream output;
    stats.print_summary(output);

    const std::string text = output.str();

    EXPECT_NE(text.find("n=4"), std::string::npos);
    EXPECT_NE(text.find("avg=250 us"), std::string::npos);
    EXPECT_NE(text.find("min=100 us"), std::string::npos);
    EXPECT_NE(text.find("p50=250 us"), std::string::npos);
    EXPECT_NE(text.find("p95=385 us"), std::string::npos);
    EXPECT_NE(text.find("p99=397 us"), std::string::npos);
    EXPECT_NE(text.find("max=400 us"), std::string::npos);
}

TEST(SessionStatsTest, PrintsLastOperation)
{
    SessionStats stats;

    std::ostringstream empty_output;
    stats.print_last(empty_output);

    EXPECT_NE(
        empty_output.str().find("No measured operation yet."),
        std::string::npos);

    OperationMetrics metrics;
    metrics.symbol = "MSFT";
    metrics.success = true;
    metrics.total_us = 500;

    stats.record(metrics);

    std::ostringstream populated_output;
    stats.print_last(populated_output);

    EXPECT_NE(
        populated_output.str().find("[metrics] MSFT SUCCESS"),
        std::string::npos);
}

TEST(SessionStatsTest, ResetClearsAllMeasurements)
{
    SessionStats stats;

    OperationMetrics metrics;
    metrics.symbol = "IBM";
    metrics.success = true;
    metrics.api_us = 100;
    metrics.total_us = 200;

    stats.record(metrics);
    ASSERT_TRUE(stats.last().has_value());

    stats.reset();

    EXPECT_FALSE(stats.last().has_value());

    std::ostringstream output;
    stats.print_summary(output);

    EXPECT_NE(
        output.str().find("attempts             0"),
        std::string::npos);
}

} // namespace
} // namespace fincore::cli