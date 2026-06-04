#include "graph_io.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gst
{

std::vector<std::string> ListGraphFolders(const std::string& data_root)
{
    std::vector<std::string> folders;
    if (!fs::exists(data_root))
    {
        return folders;
    }
    for (const auto& entry : fs::directory_iterator(data_root))
    {
        if (entry.is_directory())
        {
            folders.push_back(entry.path().filename().string());
        }
    }
    std::sort(folders.begin(), folders.end());
    return folders;
}

std::string ResolveGraphFolder(const std::string& data_root, const std::string& graph_selector)
{
    if (!graph_selector.empty())
    {
        const fs::path direct_graph_folder = fs::path(graph_selector);
        if (fs::exists(direct_graph_folder) && fs::is_directory(direct_graph_folder))
        {
            return direct_graph_folder.string();
        }
    }

    const auto folders = ListGraphFolders(data_root);
    if (folders.empty())
    {
        throw std::runtime_error("No graph folders found under data root: " + data_root);
    }

    // 未显式指定图时，优先选择 example，便于快速测试。
    if (graph_selector.empty())
    {
        for (const auto& name : folders)
        {
            if (name == "example")
            {
                return (fs::path(data_root) / name).string();
            }
        }
        return (fs::path(data_root) / folders.front()).string();
    }

    bool is_number = !graph_selector.empty() &&
                     std::all_of(graph_selector.begin(), graph_selector.end(), [](char c)
                     {
                         return c >= '0' && c <= '9';
                     });
    if (is_number)
    {
        int idx = std::stoi(graph_selector);
        if (idx < 1 || idx > static_cast<int>(folders.size()))
        {
            throw std::runtime_error("Graph index out of range: " + graph_selector);
        }
        return (fs::path(data_root) / folders[idx - 1]).string();
    }

    for (const auto& name : folders)
    {
        if (name == graph_selector)
        {
            return (fs::path(data_root) / name).string();
        }
    }
    throw std::runtime_error("Graph folder not found: " + graph_selector);
}

Graph LoadGraphFromFolder(const std::string& graph_folder)
{
    fs::path graph_path = fs::path(graph_folder) / "graph.txt";
    if (!fs::exists(graph_path))
    {
        fs::path graph_path_alt = fs::path(graph_folder) / "Graph.txt";
        if (fs::exists(graph_path_alt))
        {
            graph_path = graph_path_alt;
        }
        else
        {
            throw std::runtime_error("graph.txt / Graph.txt not found under: " + graph_folder);
        }
    }

    std::ifstream fin(graph_path);
    if (!fin)
    {
        throw std::runtime_error("Failed to open graph file: " + graph_path.string());
    }

    Graph g;
    fin >> g.n >> g.m;
    if (!fin || g.n <= 0 || g.m < 0)
    {
        throw std::runtime_error("Invalid graph header in file: " + graph_path.string());
    }

    // 预分配并建立无向图的双向邻接表。
    g.edges.reserve(g.m);
    g.adj.assign(g.n + 1, {});
    for (int i = 0; i < g.m; ++i)
    {
        int u = 0;
        int v = 0;
        double w = 0.0;
        fin >> u >> v >> w;
        if (!fin)
        {
            throw std::runtime_error("Invalid edge line at index " + std::to_string(i) + " in: " + graph_path.string());
        }
        if (u < 1 || u > g.n || v < 1 || v > g.n || w < 0.0)
        {
            throw std::runtime_error("Invalid edge value at index " + std::to_string(i) + " in: " + graph_path.string());
        }
        UndirectedEdge edge;
        edge.id = i;
        edge.u = u;
        edge.v = v;
        edge.w = w;
        g.edges.push_back(edge);
        g.adj[u].push_back({v, i, w});
        g.adj[v].push_back({u, i, w});
    }
    return g;
}

}  // namespace gst
