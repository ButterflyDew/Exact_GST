#ifndef GST_MEMORY_USAGE_H
#define GST_MEMORY_USAGE_H

#include <atomic>
#include <cstdint>
#include <thread>

namespace gst
{

struct ProcessMemoryUsage
{
    std::uint64_t current_rss_bytes = 0;
    // Process-lifetime peak.  Do not use this as a per-query peak when a
    // process executes more than one query.
    std::uint64_t peak_rss_bytes = 0;
};

constexpr int kQueryRssSampleIntervalMs = 1;

ProcessMemoryUsage GetProcessMemoryUsage();
double BytesToMiB(std::uint64_t bytes);

class QueryPeakRssSampler
{
public:
    QueryPeakRssSampler();
    ~QueryPeakRssSampler();

    QueryPeakRssSampler(const QueryPeakRssSampler&) = delete;
    QueryPeakRssSampler& operator=(const QueryPeakRssSampler&) = delete;

    std::uint64_t Stop();

private:
    void Sample();

    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> peak_rss_bytes_{0};
    std::thread worker_;
    bool stopped_ = false;
};

}  // namespace gst

#endif  // GST_MEMORY_USAGE_H
