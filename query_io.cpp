#include "query_io.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace gst
{

std::string ResolveQueryFile(const std::string& graph_folder, const std::string& query_selector)
{
    const fs::path folder_path = fs::path(graph_folder);
    if (!query_selector.empty())
    {
        std::vector<std::string> candidates;
        candidates.push_back(query_selector);
        if (fs::path(query_selector).extension().empty())
        {
            candidates.push_back(query_selector + ".txt");
            if (query_selector.rfind("query_", 0) != 0 && query_selector.rfind("Query_", 0) != 0)
            {
                candidates.push_back("query_" + query_selector + ".txt");
                candidates.push_back("Query_" + query_selector + ".txt");
            }
        }

        for (const auto& candidate : candidates)
        {
            fs::path query_path = folder_path / candidate;
            if (fs::exists(query_path))
            {
                return query_path.string();
            }
        }

        throw std::runtime_error("Query file not found under " + graph_folder + ": " + query_selector);
    }

    fs::path query_path = folder_path / "query.txt";
    if (fs::exists(query_path))
    {
        return query_path.string();
    }

    fs::path query_path_alt = folder_path / "Query.txt";
    if (fs::exists(query_path_alt))
    {
        return query_path_alt.string();
    }

    throw std::runtime_error("query.txt / Query.txt not found under: " + graph_folder);
}

std::vector<Query> LoadQueriesFromFolder(const std::string& graph_folder, const std::string& query_selector)
{
    fs::path query_path = ResolveQueryFile(graph_folder, query_selector);
    std::ifstream fin(query_path);
    if (!fin)
    {
        throw std::runtime_error("Failed to open query file: " + query_path.string());
    }

    int q = 0;
    fin >> q;
    if (!fin || q < 0)
    {
        throw std::runtime_error("Invalid query count in: " + query_path.string());
    }

    std::vector<Query> queries;
    queries.reserve(q);
    for (int qi = 0; qi < q; ++qi)
    {
        int g = 0;
        fin >> g;
        if (!fin)
        {
            throw std::runtime_error("Invalid group count in query " + std::to_string(qi));
        }
        Query query;
        query.groups.resize(g);
        for (int gi = 0; gi < g; ++gi)
        {
            int s = 0;
            fin >> s;
            if (!fin || s <= 0)
            {
                throw std::runtime_error("Invalid group size in query " + std::to_string(qi));
            }
            query.groups[gi].resize(s);
            for (int i = 0; i < s; ++i)
            {
                fin >> query.groups[gi][i];
                if (!fin)
                {
                    throw std::runtime_error("Invalid node id in query " + std::to_string(qi));
                }
            }
        }
        queries.push_back(std::move(query));
    }
    return queries;
}

}  // namespace gst
