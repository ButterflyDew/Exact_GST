#include "memory_usage.h"

#include <chrono>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <unistd.h>

#include <fstream>
#endif

namespace gst
{

ProcessMemoryUsage GetProcessMemoryUsage()
{
    ProcessMemoryUsage usage;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc)))
    {
        usage.current_rss_bytes = static_cast<std::uint64_t>(pmc.WorkingSetSize);
        usage.peak_rss_bytes = static_cast<std::uint64_t>(pmc.PeakWorkingSetSize);
    }
#elif defined(__linux__)
    long page_size = sysconf(_SC_PAGESIZE);
    std::ifstream statm("/proc/self/statm");
    std::uint64_t pages = 0;
    if (statm)
    {
        std::uint64_t ignored = 0;
        statm >> ignored >> pages;
        usage.current_rss_bytes = pages * static_cast<std::uint64_t>(page_size);
    }
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        usage.peak_rss_bytes = static_cast<std::uint64_t>(ru.ru_maxrss) * 1024ULL;
#else
    usage.current_rss_bytes = 0;
    usage.peak_rss_bytes = 0;
#endif
    return usage;
}

double BytesToMiB(std::uint64_t bytes)
{
    return static_cast<double>(bytes) / 1048576.0;
}

QueryPeakRssSampler::QueryPeakRssSampler()
{
    Sample();
    worker_ = std::thread([this]
    {
        while (!stop_.load(std::memory_order_acquire))
        {
            Sample();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(kQueryRssSampleIntervalMs));
        }
        Sample();
    });
}

QueryPeakRssSampler::~QueryPeakRssSampler()
{
    Stop();
}

std::uint64_t QueryPeakRssSampler::Stop()
{
    if (!stopped_)
    {
        stop_.store(true, std::memory_order_release);
        if (worker_.joinable())
            worker_.join();
        Sample();
        stopped_ = true;
    }
    return peak_rss_bytes_.load(std::memory_order_relaxed);
}

void QueryPeakRssSampler::Sample()
{
    const std::uint64_t current = GetProcessMemoryUsage().current_rss_bytes;
    std::uint64_t observed = peak_rss_bytes_.load(std::memory_order_relaxed);
    while (current > observed &&
           !peak_rss_bytes_.compare_exchange_weak(
               observed, current, std::memory_order_relaxed))
    {
    }
}

}  // namespace gst
