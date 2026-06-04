#include "test3.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <queue>
#include <iostream>

#include "../../float_compare.h"
#include "../../query_feasibility.h"

namespace gst::methods::test3
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
            throw std::runtime_error("Test3 currently supports group count <= 22.");
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
        std::vector<std::vector<double>> f(1 << g, std::vector<double>(n + 1, fp::kInf));// f <- min subsetsum h
        for(int i = 1; i <= n; i++)
            f[0][i] = 0.0, dp[0][i] = 0.0;

        std::vector<std::vector<int>> vis_count(1 << g, std::vector<int>(n + 1, 0)); // update time of h
        std::vector<std::vector<std::pair <int,int>>> h_upt(H + 1); // h_upt[i] <- (mask, v) that popcnt[mask] = i and update h[mask][v]

        double best = fp::kInf;

        auto Modify = [&](int mask, int v, double w)
        {
            //std::cerr << mask << " " << v << " " << w << std::endl;
            dp[mask][v] = w;
            for(int t = U ^ mask; ; t = (t - 1) & (U ^ mask))
            {
                int nxt = mask | t;
                dp[nxt][v] = std::min(dp[nxt][v], dp[mask][v] + dp[t][v]);
                if(nxt == U)
                {
                    //std::cerr << "update best: " << dp[nxt][v] << std::endl;
                    best = std::min(best, dp[nxt][v]);
                }
                if(fp::Lt(h[nxt][v], dp[mask][v]))
                {
                    h[nxt][v] = dp[mask][v];
                    int cnt = popcnt[nxt];
                    if(cnt <= H && vis_count[nxt][v] != popcnt[mask])
                    {
                        // std::cerr << "update h: " << nxt << " " << v << std::endl;
                        // std::cerr << "dp[mask][v]: " << dp[mask][v] << std::endl;
                        vis_count[nxt][v] = popcnt[mask];
                        h_upt[cnt].push_back({nxt, v});
                    }
                }    
                if(t == 0) break;
            }
        };

        auto updata_f = [&](int size)
        {
            std::cerr << "update f size: " << size << std::endl;
            for(auto [mask, v] : h_upt[size])
            {
                std::cerr << "update f: " << mask << " " << v << std::endl;
                std::cerr << "h[mask][v]: " << h[mask][v] << std::endl;
                for(int t = U ^ mask; ; t = (t - 1) & (U ^ mask))
                {
                    if(fp::Lt(h[mask][v] + f[t][v], f[mask | t][v]))
                    {
                        f[mask | t][v] = h[mask][v] + f[t][v];
                        // if((mask | t) == U)
                        // {
                        //     std::cerr << '\n' << "update best: " << h[mask][v] << " " << f[t][v] << " " << f[mask | t][v] << '\n' << std::endl;
                        //     best = std::min(best, f[mask | t][v]);
                        // }   
                    }
                    if(t == 0) break;
                }
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
            // if(popcnt[mask] > now_size)
            // {
            //     updata_f(now_size);
            //     now_size = popcnt[mask];
            // }
            if(popcnt[mask] > H)
                break;
            std::priority_queue<std::pair<double, std::pair <int, int>>, std::vector<std::pair<double, std::pair <int, int>>>,
            std::greater<std::pair<double, std::pair <int, int>>>> pq;
            
            for(int i = 1; i <= n; i++)
                if(now_size == 1 || fp::Lt(h[mask][i] + h[U^mask][i], best))
                    pq.push({dp[mask][i], {mask, i}});
            while(!pq.empty())
            {
                auto [d, p] = pq.top();
                auto [s, u] = p;
                pq.pop();
                if(d > dp[s][u])
                    continue;
                Modify(s, u, d);
                for(const auto& e : graph.adj[u])
                {
                    if(fp::Lt(d + e.w, dp[s][e.to]))
                        pq.push({dp[s][e.to] = d + e.w, {s, e.to}});
                }
            }
        }

        result.best_weight = best;
        result.feasible = true;
        return result;
    }
}