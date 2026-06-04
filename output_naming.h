#ifndef GST_OUTPUT_NAMING_H
#define GST_OUTPUT_NAMING_H

#include <filesystem>
#include <string>

namespace gst
{

// 根据已解析的查询文件路径得到结果子目录名（如 default、query_g10）。
std::string RunSubdirFromQueryFile(const std::filesystem::path& query_file_path);

std::string WeightsFilename();
std::string StatsFilename(const std::string& method_name);

}  // namespace gst

#endif  // GST_OUTPUT_NAMING_H