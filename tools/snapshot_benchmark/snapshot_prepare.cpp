#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace
{
struct Edge
{
    int u = 0;
    int v = 0;
    double w = 0.0;
};

struct Query
{
    std::vector<std::vector<int>> groups;
};

struct GraphData
{
    int n = 0;
    int m = 0;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> adj;
};

int Usage()
{
    std::cerr
        << "Usage:\n"
        << "  gst_snapshot_prepare <source_graph_dir> <dest_graph_dir> <mode> <max_vertices>\n"
        << "                       <seed> <queries_per_g> <g_csv> <query_pattern>\n"
        << "\n"
        << "mode: copy | bfs\n"
        << "query_pattern examples: query_g{g}.txt, query_g{g}_uniform.txt\n";
    return 2;
}

std::vector<int> ParseCsvInts(const std::string& s)
{
    std::vector<int> out;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        if (!token.empty())
            out.push_back(std::stoi(token));
    }
    return out;
}

std::string ReplaceG(std::string pattern, int g)
{
    const std::string key = "{g}";
    size_t pos = pattern.find(key);
    if (pos != std::string::npos)
        pattern.replace(pos, key.size(), std::to_string(g));
    return pattern;
}

GraphData ReadGraph(const fs::path& graph_file, bool need_adj)
{
    std::ifstream in(graph_file);
    if (!in)
        throw std::runtime_error("failed to open graph: " + graph_file.string());

    GraphData graph;
    in >> graph.n >> graph.m;
    if (!in || graph.n <= 0 || graph.m < 0)
        throw std::runtime_error("bad graph header: " + graph_file.string());

    graph.edges.reserve(graph.m);
    if (need_adj)
        graph.adj.assign(graph.n + 1, {});
    for (int i = 0; i < graph.m; ++i)
    {
        Edge e;
        in >> e.u >> e.v >> e.w;
        if (!in)
            throw std::runtime_error("bad graph edge in: " + graph_file.string());
        graph.edges.push_back(e);
        if (need_adj)
        {
            graph.adj[e.u].push_back(e.v);
            graph.adj[e.v].push_back(e.u);
        }
    }
    return graph;
}

std::vector<Query> ReadQueries(const fs::path& query_file)
{
    std::ifstream in(query_file);
    if (!in)
        throw std::runtime_error("failed to open query: " + query_file.string());

    int q = 0;
    in >> q;
    if (!in || q < 0)
        throw std::runtime_error("bad query header: " + query_file.string());

    std::vector<Query> queries(q);
    for (int i = 0; i < q; ++i)
    {
        int g = 0;
        in >> g;
        if (!in || g <= 0)
            throw std::runtime_error("bad query group count: " + query_file.string());
        queries[i].groups.resize(g);
        for (int j = 0; j < g; ++j)
        {
            int sz = 0;
            in >> sz;
            if (!in || sz <= 0)
                throw std::runtime_error("bad query group size: " + query_file.string());
            queries[i].groups[j].resize(sz);
            for (int k = 0; k < sz; ++k)
                in >> queries[i].groups[j][k];
        }
    }
    return queries;
}

void WriteGraph(const fs::path& file,
                const std::vector<Edge>& edges,
                const std::vector<int>& old_to_new)
{
    int n = 0;
    for (int x : old_to_new)
        n = std::max(n, x);

    std::vector<Edge> kept;
    kept.reserve(edges.size());
    for (const auto& e : edges)
    {
        int u = old_to_new[e.u], v = old_to_new[e.v];
        if (u && v && u != v)
            kept.push_back({u, v, e.w});
    }

    std::ofstream out(file);
    if (!out)
        throw std::runtime_error("failed to write graph: " + file.string());
    out << n << ' ' << kept.size() << '\n';
    for (const auto& e : kept)
        out << e.u << ' ' << e.v << ' ' << e.w << '\n';
}

std::vector<int> SelectAll(int n)
{
    std::vector<int> selected(n);
    std::iota(selected.begin(), selected.end(), 1);
    return selected;
}

std::vector<int> SelectBfs(const GraphData& graph, int max_vertices, std::uint64_t seed)
{
    if (max_vertices <= 0 || max_vertices >= graph.n)
        return SelectAll(graph.n);

    std::mt19937_64 rng(seed);
    std::vector<int> order(graph.n);
    std::iota(order.begin(), order.end(), 1);
    std::shuffle(order.begin(), order.end(), rng);

    std::vector<char> seen(graph.n + 1, 0);
    std::vector<int> selected;
    selected.reserve(max_vertices);
    std::queue<int> q;

    for (int start : order)
    {
        if (seen[start] || graph.adj[start].empty())
            continue;
        seen[start] = 1;
        q.push(start);
        while (!q.empty() && static_cast<int>(selected.size()) < max_vertices)
        {
            int u = q.front();
            q.pop();
            selected.push_back(u);

            std::vector<int> nb = graph.adj[u];
            std::shuffle(nb.begin(), nb.end(), rng);
            for (int v : nb)
                if (!seen[v])
                    seen[v] = 1, q.push(v);
            if (static_cast<int>(selected.size()) >= max_vertices)
                break;
        }
        if (static_cast<int>(selected.size()) >= max_vertices)
            break;
    }
    std::sort(selected.begin(), selected.end());
    return selected;
}

std::vector<int> BuildOldToNew(const std::vector<int>& selected, int n)
{
    std::vector<int> old_to_new(n + 1, 0);
    for (int i = 0; i < static_cast<int>(selected.size()); ++i)
        old_to_new[selected[i]] = i + 1;
    return old_to_new;
}

std::vector<Query> ProjectQueries(const std::vector<Query>& source,
                                  int target_g,
                                  int limit,
                                  const std::vector<int>& old_to_new)
{
    std::vector<Query> out;
    out.reserve(limit);
    for (const auto& q : source)
    {
        Query cur;
        for (const auto& group : q.groups)
        {
            std::vector<int> mapped;
            for (int v : group)
                if (v >= 0 && v < static_cast<int>(old_to_new.size()) && old_to_new[v])
                    mapped.push_back(old_to_new[v]);
            std::sort(mapped.begin(), mapped.end());
            mapped.erase(std::unique(mapped.begin(), mapped.end()), mapped.end());
            if (!mapped.empty())
                cur.groups.push_back(std::move(mapped));
            if (static_cast<int>(cur.groups.size()) == target_g)
                break;
        }
        if (static_cast<int>(cur.groups.size()) == target_g)
        {
            out.push_back(std::move(cur));
            if (static_cast<int>(out.size()) >= limit)
                break;
        }
    }
    return out;
}

void FillSyntheticQueries(std::vector<Query>& queries,
                          int target_g,
                          int limit,
                          int n,
                          std::uint64_t seed)
{
    std::mt19937_64 rng(seed + static_cast<std::uint64_t>(target_g) * 1000003ULL);
    std::uniform_int_distribution<int> vertex(1, n);
    std::uniform_int_distribution<int> group_size(1, std::min(8, n));
    while (static_cast<int>(queries.size()) < limit)
    {
        Query q;
        q.groups.resize(target_g);
        for (auto& group : q.groups)
        {
            int sz = group_size(rng);
            while (static_cast<int>(group.size()) < sz)
            {
                int v = vertex(rng);
                if (std::find(group.begin(), group.end(), v) == group.end())
                    group.push_back(v);
            }
            std::sort(group.begin(), group.end());
        }
        queries.push_back(std::move(q));
    }
}

void WriteQueries(const fs::path& file, const std::vector<Query>& queries)
{
    std::ofstream out(file);
    if (!out)
        throw std::runtime_error("failed to write query: " + file.string());
    out << queries.size() << '\n';
    for (const auto& q : queries)
    {
        out << q.groups.size() << '\n';
        for (const auto& group : q.groups)
        {
            out << group.size();
            for (int v : group)
                out << ' ' << v;
            out << '\n';
        }
    }
}

std::vector<fs::path> QueryCandidates(const fs::path& source_dir,
                                      const std::string& pattern,
                                      int target_g)
{
    std::vector<fs::path> candidates;
    for (int g = target_g; g <= 20; ++g)
    {
        fs::path p = source_dir / ReplaceG(pattern, g);
        if (fs::exists(p))
            candidates.push_back(p);
    }
    return candidates;
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 9)
            return Usage();

        fs::path source_dir = fs::absolute(argv[1]);
        fs::path dest_dir = fs::absolute(argv[2]);
        std::string mode = argv[3];
        int max_vertices = std::atoi(argv[4]);
        std::uint64_t seed = std::strtoull(argv[5], nullptr, 10);
        int queries_per_g = std::atoi(argv[6]);
        std::vector<int> groups = ParseCsvInts(argv[7]);
        std::string query_pattern = argv[8];

        if (queries_per_g <= 0 || groups.empty() || (mode != "copy" && mode != "bfs"))
            return Usage();

        fs::create_directories(dest_dir);
        GraphData graph = ReadGraph(source_dir / "graph.txt", mode == "bfs");
        std::vector<int> selected = mode == "copy" ? SelectAll(graph.n) : SelectBfs(graph, max_vertices, seed);
        std::vector<int> old_to_new = BuildOldToNew(selected, graph.n);
        WriteGraph(dest_dir / "graph.txt", graph.edges, old_to_new);

        int n_out = static_cast<int>(selected.size());
        for (int target_g : groups)
        {
            std::vector<Query> projected;
            for (const auto& file : QueryCandidates(source_dir, query_pattern, target_g))
            {
                auto source_queries = ReadQueries(file);
                auto more = ProjectQueries(source_queries, target_g, queries_per_g - static_cast<int>(projected.size()), old_to_new);
                projected.insert(projected.end(),
                                 std::make_move_iterator(more.begin()),
                                 std::make_move_iterator(more.end()));
                if (static_cast<int>(projected.size()) >= queries_per_g)
                    break;
            }

            const int extracted = static_cast<int>(projected.size());
            if (extracted < queries_per_g)
                FillSyntheticQueries(projected, target_g, queries_per_g, n_out, seed);
            WriteQueries(dest_dir / ("query_g" + std::to_string(target_g) + ".txt"), projected);
            std::cout << "query_g" << target_g
                      << " extracted=" << extracted
                      << " synthetic=" << (static_cast<int>(projected.size()) - extracted)
                      << '\n';
        }

        std::ofstream meta(dest_dir / "snapshot_meta.txt");
        meta << "source=" << source_dir.string() << '\n'
             << "mode=" << mode << '\n'
             << "seed=" << seed << '\n'
             << "source_n=" << graph.n << '\n'
             << "source_m=" << graph.m << '\n'
             << "snapshot_n=" << selected.size() << '\n';
        std::cout << "prepared " << dest_dir.string() << " n=" << selected.size() << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "snapshot_prepare error: " << e.what() << '\n';
        return 1;
    }
}
