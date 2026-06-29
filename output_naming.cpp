#include "output_naming.h"

#include <algorithm>
#include <cctype>

namespace gst
{

std::string RunSubdirFromQueryFile(const std::filesystem::path& query_file_path)
{
    std::string stem = query_file_path.stem().string();
    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lower == "query")
    {
        return "default";
    }
    return stem;
}

std::string WeightsFilename()
{
    return "weights.txt";
}

std::string StatsFilename(const std::string& method_name)
{
    std::string base = method_name;
    std::transform(base.begin(), base.end(), base.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return base + "_stats.txt";
}

}  // namespace gst
