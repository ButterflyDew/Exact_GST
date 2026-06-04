#ifndef GST_GRAPH_IO_H
#define GST_GRAPH_IO_H

#include <string>
#include <vector>

namespace gst
{

struct UndirectedEdge
{
    int id = -1;
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct AdjEdge
{
    int to = 0;
    int edge_id = -1;
    double w = 0.0;
};

struct Graph
{
    int n = 0;
    int m = 0;
    std::vector<UndirectedEdge> edges;
    std::vector<std::vector<AdjEdge>> adj;
};

std::vector<std::string> ListGraphFolders(const std::string& data_root);
std::string ResolveGraphFolder(const std::string& data_root, const std::string& graph_selector);
Graph LoadGraphFromFolder(const std::string& graph_folder);

}  // namespace gst

#endif  // GST_GRAPH_IO_H
