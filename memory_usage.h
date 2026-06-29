#ifndef GST_MEMORY_USAGE_H
#define GST_MEMORY_USAGE_H

#include <cstdint>

namespace gst
{

struct ProcessMemoryUsage
{
    std::uint64_t current_rss_bytes = 0;
    std::uint64_t peak_rss_bytes = 0;
};

ProcessMemoryUsage GetProcessMemoryUsage();
double BytesToMiB(std::uint64_t bytes);

}  // namespace gst

#endif  // GST_MEMORY_USAGE_H
