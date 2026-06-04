#include "query_feasibility.h"

#include <queue>
#include <stdexcept>
#include <vector>

namespace gst
{

bool IsQueryFeasible(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (g == 0)
    {
        return true;
    }
    if (graph.n <= 0)
    {
        return false;
    }

    std::vector<int> comp(graph.n + 1, -1);
    int comp_cnt = 0;
    std::queue<int> q;
    for (int s = 1; s <= graph.n; ++s)
    {
        if (comp[s] != -1)
        {
            continue;
        }
        comp[s] = comp_cnt;
        q.push(s);
        while (!q.empty())
        {
            int u = q.front();
            q.pop();
            for (const auto& e : graph.adj[u])
            {
                if (comp[e.to] == -1)
                {
                    comp[e.to] = comp_cnt;
                    q.push(e.to);
                }
            }
        }
        ++comp_cnt;
    }

    std::vector<int> hit_count(comp_cnt, 0);
    std::vector<int> seen_comp(comp_cnt, -1);
    for (int gi = 0; gi < g; ++gi)
    {
        for (int v : query.groups[gi])
        {
            if (v < 1 || v > graph.n)
            {
                throw std::runtime_error("Query vertex id out of range.");
            }
            const int c = comp[v];
            if (seen_comp[c] != gi)
            {
                seen_comp[c] = gi;
                ++hit_count[c];
            }
        }
    }

    for (int c = 0; c < comp_cnt; ++c)
    {
        if (hit_count[c] == g)
        {
            return true;
        }
    }
    return false;
}

}  // namespace gst
