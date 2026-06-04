#ifndef GST_QUERY_IO_H
#define GST_QUERY_IO_H

#include <string>
#include <vector>

namespace gst
{

struct Query
{
    // groups[i] 存储第 i 组候选节点集合。
    std::vector<std::vector<int>> groups;
};

std::string ResolveQueryFile(const std::string& graph_folder, const std::string& query_selector = "");
std::vector<Query> LoadQueriesFromFolder(const std::string& graph_folder, const std::string& query_selector = "");

}  // namespace gst

#endif  // GST_QUERY_IO_H
