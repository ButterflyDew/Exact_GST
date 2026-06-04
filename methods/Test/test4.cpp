#include "test4.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <queue>
#include <iostream>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test4
{
    SolveResult SolveOneQuery(const Graph& graph, const Query& query, dpbf::OutputMode /*output_mode*/)
    {
        const int g = static_cast<int>(query.groups.size());
        const int n = graph.n;
        SolveResult result;
        if (g == 0)
        {
            result.best_weight = 0.0;
            result.feasible = true;
            return result;
        }
        if (g >= 23)
        {
            throw std::runtime_error("Test4 currently supports group count <= 22.");
        }
        if (!IsQueryFeasible(graph, query))
        {
            result.best_weight = -1.0;
            result.feasible = false;
            result.stats.valid_by_size.assign(g + 1, 0);
            result.stats.total_by_size.assign(g + 1, 0);
            return result;
        }


        int H = g / 2, U = (1 << g) - 1;
        std::vector<int> popcnt(1 << g);
        for(int s = 0; s <= U; s++) 
            popcnt[s] = popcnt[s >> 1] + (s & 1);

        std::vector<std::vector<double>> dp(1 << g, std::vector<double>(n + 1, fp::kInf));
        std::vector<std::vector<double>> h(1 << g, std::vector<double>(n + 1, -1.0)); // h <- max dp
        for(int i = 1; i <= n; i++)
            dp[0][i] = 0.0;

        double best = fp::kInf;

        auto Modify = [&](int mask, int v, double w)
        {
            //std::cerr << mask << " " << v << " " << w << std::endl;
            dp[mask][v] = w;
            for(int t = U ^ mask; ; t = (t - 1) & (U ^ mask))
            {
                int nxt = mask | t;
                dp[nxt][v] = std::min(dp[nxt][v], dp[mask][v] + dp[t][v]);
                h[nxt][v] = std::max(h[nxt][v], dp[mask][v]);
                if(nxt == U && dp[nxt][v] < best)
                {
                    //std::cerr << "update best: " << dp[nxt][v] << std::endl;
                    best = dp[nxt][v];
                }
                if(t == 0) break;
            }
        };


        for (int gi = 0; gi < g; ++gi)
        {
            const int m = (1 << gi);
            for (int v : query.groups[gi])
            {
                if (v < 1 || v > n)
                {
                    throw std::runtime_error("Query vertex id out of range.");
                }
                Modify(m, v, 0.0);
            }
        }

        std::vector <int> order;
        for(int mask = 1; mask <= U; mask++)        
            order.push_back(mask);
        std::sort(order.begin(), order.end(), [&](int a, int b){return popcnt[a] < popcnt[b];});
        int now_size = 0;
        for(int mask : order)
        {
            if(popcnt[mask] > H)
                break;
            std::priority_queue<std::pair<double, int>, std::vector<std::pair<double, int>>, std::greater<std::pair<double, int>>> pq;
            
            int used_cnt = 0;
            std::vector <int> status(n + 1, 0);
            for(int i = 1; i <= n; i++)
            {
                if(dp[mask][i] + h[U^mask][i] <= best)
                    pq.push({dp[mask][i], i}), used_cnt++, status[i] = 1;
                else
                    pq.push({dp[mask][i], i});
            }
                
            while(!pq.empty() && used_cnt > 0)
            {
                auto [d, u] = pq.top();
                pq.pop();
                if(d > dp[mask][u])
                    continue;
                if(status[u])
                {
                    --used_cnt; 
                    Modify(mask, u, d);
                }    
                for(const auto& e : graph.adj[u])
                {
                    if(dp[mask][e.to] > d + e.w)
                    {
                        if(status[u] != status[e.to])
                        {
                            status[e.to] = status[u];
                            if(status[e.to] == 1)
                                ++used_cnt;
                        }
                        pq.push({dp[mask][e.to] = d + e.w, e.to});
                    }
                        
                }
            }
        }

        result.best_weight = best;
        result.feasible = true;
        return result;
    }
}