#include "memory_usage.h"

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

}  // namespace gst
