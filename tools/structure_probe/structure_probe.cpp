#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include "graph_io.h"
#include "query_io.h"

namespace
{
constexpr double kInf = 1e100;

using Clock = std::chrono::steady_clock;
using HeapItem = std::pair<double, int>;

void PrintUsage(const char* argv0)
{
    std::cerr << "usage: " << argv0
              << " <data_root> <graph_selector> <query_selector> <query_1based>\n";
}

std::vector<double> MultiSourceDijkstra(const gst::Graph& graph, const std::vector<int>& sources)
{
    std::vector<double> dist(graph.n + 1, kInf);
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
    for (int v : sources)
    {
        if (v < 1 || v > graph.n)
            throw std::runtime_error("query contains vertex outside graph range");
        if (dist[v] > 0.0)
        {
            dist[v] = 0.0;
            heap.push({0.0, v});
        }
    }
    while (!heap.empty())
    {
        auto [d, u] = heap.top();
        heap.pop();
        if (d != dist[u])
            continue;
        for (auto e : graph.adj[u])
        {
            double nd = d + e.w;
            if (nd < dist[e.to])
            {
                dist[e.to] = nd;
                heap.push({nd, e.to});
            }
        }
    }
    return dist;
}

double Percent(long long part, long long total)
{
    return total ? 100.0 * static_cast<double>(part) / static_cast<double>(total) : 0.0;
}

int Popcount(int mask)
{
    int count = 0;
    while (mask)
    {
        mask &= mask - 1;
        ++count;
    }
    return count;
}

bool Solve3x3(std::array<std::array<double, 3>, 3> a, std::array<double, 3> b,
              std::array<double, 3>& x)
{
    constexpr double eps = 1e-9;
    for (int col = 0; col < 3; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row)
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
                pivot = row;
        if (std::fabs(a[pivot][col]) < eps)
            return false;
        if (pivot != col)
        {
            std::swap(a[pivot], a[col]);
            std::swap(b[pivot], b[col]);
        }
        const double div = a[col][col];
        for (int j = col; j < 3; ++j)
            a[col][j] /= div;
        b[col] /= div;
        for (int row = 0; row < 3; ++row)
        {
            if (row == col)
                continue;
            const double factor = a[row][col];
            for (int j = col; j < 3; ++j)
                a[row][j] -= factor * a[col][j];
            b[row] -= factor * b[col];
        }
    }
    x = b;
    return true;
}

bool QuadHubFeasible(const std::array<double, 16>& cost)
{
    constexpr double eps = 1e-8;
    const double quad = cost[15];
    if (quad >= kInf)
        return false;

    std::vector<std::array<double, 4>> inequalities;
    auto AddInequality = [&](double a0, double a1, double a2, double rhs)
    {
        inequalities.push_back({a0, a1, a2, rhs});
    };

    AddInequality(-1.0, 0.0, 0.0, 0.0);
    AddInequality(0.0, -1.0, 0.0, 0.0);
    AddInequality(0.0, 0.0, -1.0, 0.0);
    AddInequality(1.0, 1.0, 1.0, quad);

    for (int mask = 1; mask < 15; ++mask)
    {
        const int pc = Popcount(mask);
        if (pc < 2 || pc > 3 || cost[mask] >= kInf)
            continue;
        if (mask & 8)
        {
            double coeff[3] = {0.0, 0.0, 0.0};
            for (int i = 0; i < 3; ++i)
                if (!(mask >> i & 1))
                    coeff[i] = 1.0;
            AddInequality(coeff[0], coeff[1], coeff[2], quad - cost[mask]);
        }
        else
        {
            double coeff[3] = {0.0, 0.0, 0.0};
            for (int i = 0; i < 3; ++i)
                if (mask >> i & 1)
                    coeff[i] = -1.0;
            AddInequality(coeff[0], coeff[1], coeff[2], -cost[mask]);
        }
    }

    auto FeasiblePoint = [&](const std::array<double, 3>& x)
    {
        for (const auto& ineq : inequalities)
        {
            const double lhs = ineq[0] * x[0] + ineq[1] * x[1] + ineq[2] * x[2];
            if (lhs > ineq[3] + eps)
                return false;
        }
        return true;
    };

    for (int i = 0; i < static_cast<int>(inequalities.size()); ++i)
        for (int j = i + 1; j < static_cast<int>(inequalities.size()); ++j)
            for (int k = j + 1; k < static_cast<int>(inequalities.size()); ++k)
            {
                std::array<std::array<double, 3>, 3> a = {
                    std::array<double, 3>{inequalities[i][0], inequalities[i][1], inequalities[i][2]},
                    std::array<double, 3>{inequalities[j][0], inequalities[j][1], inequalities[j][2]},
                    std::array<double, 3>{inequalities[k][0], inequalities[k][1], inequalities[k][2]}};
                std::array<double, 3> b = {inequalities[i][3], inequalities[j][3], inequalities[k][3]};
                std::array<double, 3> x{};
                if (Solve3x3(a, b, x) && FeasiblePoint(x))
                    return true;
            }
    return false;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 5)
        {
            PrintUsage(argv[0]);
            return 2;
        }

        const std::string data_root = argv[1];
        const std::string graph_selector = argv[2];
        const std::string query_selector = argv[3];
        const int query_index = std::stoi(argv[4]) - 1;

        const auto total_start = Clock::now();
        const std::string graph_folder = gst::ResolveGraphFolder(data_root, graph_selector);
        const gst::Graph graph = gst::LoadGraphFromFolder(graph_folder);
        const std::vector<gst::Query> queries = gst::LoadQueriesFromFolder(graph_folder, query_selector);
        if (query_index < 0 || query_index >= static_cast<int>(queries.size()))
            throw std::runtime_error("query index out of range");
        const gst::Query& query = queries[query_index];
        const int n = graph.n;
        const int g = static_cast<int>(query.groups.size());
        if (g <= 0 || g > 22)
            throw std::runtime_error("structure probe supports 1..22 groups");
        const int S = 1 << g;
        const int U = S - 1;

        std::vector<int> group_size(g);
        int total_group_vertices = 0;
        for (int a = 0; a < g; ++a)
        {
            group_size[a] = static_cast<int>(query.groups[a].size());
            total_group_vertices += group_size[a];
        }

        const auto gd_start = Clock::now();
        std::vector<std::vector<double>> gd;
        gd.reserve(g);
        for (int a = 0; a < g; ++a)
            gd.push_back(MultiSourceDijkstra(graph, query.groups[a]));
        const double gd_ms = std::chrono::duration<double, std::milli>(Clock::now() - gd_start).count();

        std::vector<int> pc(S), first_bit(S);
        for (int s = 1; s < S; ++s)
        {
            pc[s] = pc[s >> 1] + (s & 1);
            first_bit[s] = (s & 1) ? 0 : first_bit[s >> 1] + 1;
        }

        std::vector<std::vector<double>> gp(g, std::vector<double>(g, kInf));
        for (int a = 0; a < g; ++a)
            for (int b = 0; b < g; ++b)
                for (int v : query.groups[b])
                    gp[a][b] = std::min(gp[a][b], gd[a][v]);

        std::vector<double> mst(S, -1.0);
        auto MstHalf = [&](int mask)
        {
            if (!(mask & (mask - 1)))
                return 0.0;
            if (mst[mask] >= 0.0)
                return mst[mask];
            double sum = 0.0;
            std::vector<double> d(g, kInf);
            std::vector<char> used(g);
            int z = 0;
            while (!(mask >> z & 1))
                ++z;
            d[z] = 0.0;
            for (int it = 0; it < pc[mask]; ++it)
            {
                int u = -1;
                for (int a = 0; a < g; ++a)
                    if ((mask >> a & 1) && !used[a] && (u < 0 || d[a] < d[u]))
                        u = a;
                used[u] = 1;
                sum += d[u];
                for (int a = 0; a < g; ++a)
                    if ((mask >> a & 1) && !used[a])
                        d[a] = std::min(d[a], gp[u][a]);
            }
            mst[mask] = sum * 0.5;
            return mst[mask];
        };

        auto LowerBound = [&](int v, int mask)
        {
            if (!mask)
                return 0.0;
            double far = 0.0, x = kInf, y = kInf;
            for (int t = mask; t; t &= t - 1)
            {
                double z = gd[first_bit[t]][v];
                far = std::max(far, z);
                if (z < x)
                {
                    y = x;
                    x = z;
                }
                else if (z < y)
                    y = z;
            }
            return !(mask & (mask - 1)) ? far : std::max(far, MstHalf(mask) + (x + y) * 0.5);
        };

        std::vector<int> color(n + 1);
        for (int a = 0; a < g; ++a)
            for (int v : query.groups[a])
                color[v] |= 1 << a;

        double root_star = kInf;
        int root_star_vertex = 1;
        for (int v = 1; v <= n; ++v)
        {
            double cur = 0.0;
            for (int a = 0; a < g; ++a)
                cur += gd[a][v];
            if (cur < root_star)
            {
                root_star = cur;
                root_star_vertex = v;
            }
        }

        std::vector<int> owner(n + 1, -1);
        std::vector<int> owner_size(g);
        std::vector<double> nearest(n + 1), second(n + 1);
        long long tie_vertices = 0;
        for (int v = 1; v <= n; ++v)
        {
            double best = kInf, next = kInf;
            int who = -1;
            int ties = 0;
            for (int a = 0; a < g; ++a)
            {
                double d = gd[a][v];
                if (d < best)
                {
                    next = best;
                    best = d;
                    who = a;
                    ties = 1;
                }
                else if (d == best)
                {
                    ++ties;
                }
                else if (d < next)
                    next = d;
            }
            owner[v] = who;
            nearest[v] = best;
            second[v] = next;
            if (who >= 0)
                ++owner_size[who];
            if (ties > 1)
                ++tie_vertices;
        }

        std::vector<char> boundary(n + 1);
        long long boundary_edges = 0;
        for (const auto& e : graph.edges)
        {
            if (owner[e.u] != owner[e.v])
            {
                ++boundary_edges;
                boundary[e.u] = 1;
                boundary[e.v] = 1;
            }
        }
        long long boundary_vertices = 0;
        for (int v = 1; v <= n; ++v)
            boundary_vertices += boundary[v] ? 1 : 0;

        std::vector<char> seen(n + 1);
        std::vector<int> components_by_owner(g);
        std::vector<int> largest_component_by_owner(g);
        long long components = 0;
        int largest_component = 0;
        long long terminal_vertices_in_components = 0;
        long long terminal_components = 0;
        long long removable_leaf_components = 0, removable_leaf_vertices = 0;
        long long no_terminal_components = 0, no_terminal_vertices = 0;
        std::vector<long long> no_terminal_portal_components(6), no_terminal_portal_vertices(6);
        std::vector<long long> terminal_portal_components(6), terminal_portal_vertices(6);
        long long total_portals = 0, total_portal_edges = 0;
        long long max_portals = 0;
        long long largest_no_terminal_p4_component = 0;
        long long no_terminal_p4_table_components = 0, no_terminal_p4_table_vertices = 0;
        long long no_terminal_p4_quad_hub_components = 0, no_terminal_p4_quad_hub_vertices = 0;
        long long portal_pairs = 0, metric_torso_edges_from_components = 0;
        std::vector<long long> portal_bins(7);
        std::vector<int> queue;
        std::vector<int> portals_seen;
        std::vector<int> portal_mark(n + 1);
        int portal_stamp = 0;

        std::vector<int> local_index(n + 1, -1);
        auto ComputeP4SteinerCosts = [&](const std::vector<int>& component,
                                         const std::vector<int>& portals)
        {
            std::vector<int> local_nodes = portals;
            local_nodes.insert(local_nodes.end(), component.begin(), component.end());
            for (int i = 0; i < static_cast<int>(local_nodes.size()); ++i)
                local_index[local_nodes[i]] = i;

            const int local_n = static_cast<int>(local_nodes.size());
            std::vector<std::vector<double>> dist(16, std::vector<double>(local_n, kInf));
            for (int i = 0; i < 4; ++i)
                dist[1 << i][i] = 0.0;

            for (int mask = 1; mask < 16; ++mask)
            {
                for (int sub = (mask - 1) & mask; sub; sub = (sub - 1) & mask)
                {
                    const int other = mask ^ sub;
                    if (!other || sub > other)
                        continue;
                    for (int i = 0; i < local_n; ++i)
                    {
                        const double merged = dist[sub][i] + dist[other][i];
                        if (merged < dist[mask][i])
                            dist[mask][i] = merged;
                    }
                }

                std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;
                for (int i = 0; i < local_n; ++i)
                    if (dist[mask][i] < kInf)
                        heap.push({dist[mask][i], i});
                while (!heap.empty())
                {
                    auto [d, ui] = heap.top();
                    heap.pop();
                    if (d != dist[mask][ui])
                        continue;
                    const int u = local_nodes[ui];
                    for (auto e : graph.adj[u])
                    {
                        const int vi = local_index[e.to];
                        if (vi < 0)
                            continue;
                        if (boundary[u] && boundary[e.to])
                            continue;
                        const double nd = d + e.w;
                        if (nd < dist[mask][vi])
                        {
                            dist[mask][vi] = nd;
                            heap.push({nd, vi});
                        }
                    }
                }
            }

            std::array<double, 16> cost{};
            cost.fill(kInf);
            cost[0] = 0.0;
            for (int mask = 1; mask < 16; ++mask)
                for (int i = 0; i < local_n; ++i)
                    cost[mask] = std::min(cost[mask], dist[mask][i]);

            for (int v : local_nodes)
                local_index[v] = -1;
            return cost;
        };

        for (int start = 1; start <= n; ++start)
        {
            if (boundary[start] || seen[start])
                continue;
            int own = owner[start];
            int size = 0;
            int terminals = 0;
            int portals = 0;
            int portal_edges = 0;
            ++portal_stamp;
            queue.clear();
            portals_seen.clear();
            queue.push_back(start);
            seen[start] = 1;
            for (size_t qi = 0; qi < queue.size(); ++qi)
            {
                int u = queue[qi];
                ++size;
                if (color[u])
                    ++terminals;
                for (auto e : graph.adj[u])
                {
                    int v = e.to;
                    if (boundary[v])
                    {
                        ++portal_edges;
                        if (portal_mark[v] != portal_stamp)
                        {
                        portal_mark[v] = portal_stamp;
                        portals_seen.push_back(v);
                        ++portals;
                    }
                    continue;
                    }
                    if (!boundary[v] && !seen[v] && owner[v] == own)
                    {
                        seen[v] = 1;
                        queue.push_back(v);
                    }
                }
            }
            ++components;
            terminal_vertices_in_components += terminals;
            total_portals += portals;
            total_portal_edges += portal_edges;
            max_portals = std::max<long long>(max_portals, portals);
            portal_pairs += static_cast<long long>(portals) * (portals - 1) / 2;
            metric_torso_edges_from_components +=
                static_cast<long long>(portals + terminals) * (portals + terminals - 1) / 2;
            int bin = portals == 0 ? 0
                    : portals == 1 ? 1
                    : portals == 2 ? 2
                    : portals <= 4 ? 3
                    : portals <= 8 ? 4
                    : portals <= 16 ? 5
                    : 6;
            ++portal_bins[bin];
            if (terminals)
            {
                ++terminal_components;
                const int capped = portals <= 4 ? portals : 5;
                ++terminal_portal_components[capped];
                terminal_portal_vertices[capped] += size;
            }
            else
            {
                ++no_terminal_components;
                no_terminal_vertices += size;
                const int capped = portals <= 4 ? portals : 5;
                ++no_terminal_portal_components[capped];
                no_terminal_portal_vertices[capped] += size;
                if (portals == 4)
                {
                    largest_no_terminal_p4_component =
                        std::max<long long>(largest_no_terminal_p4_component, size);
                    ++no_terminal_p4_table_components;
                    no_terminal_p4_table_vertices += size;
                    const std::array<double, 16> p4_costs =
                        ComputeP4SteinerCosts(queue, portals_seen);
                    if (QuadHubFeasible(p4_costs))
                    {
                        ++no_terminal_p4_quad_hub_components;
                        no_terminal_p4_quad_hub_vertices += size;
                    }
                }
                if (portals <= 1)
                {
                    ++removable_leaf_components;
                    removable_leaf_vertices += size;
                }
            }
            if (own >= 0)
            {
                ++components_by_owner[own];
                largest_component_by_owner[own] = std::max(largest_component_by_owner[own], size);
            }
            largest_component = std::max(largest_component, size);
        }

        long long alive_root_star = 0;
        long long alive_root_star_boundary = 0;
        std::vector<long long> lb_bins(5);
        for (int v = 1; v <= n; ++v)
        {
            double lb = LowerBound(v, U);
            if (lb <= root_star)
            {
                ++alive_root_star;
                if (boundary[v])
                    ++alive_root_star_boundary;
            }
            if (lb <= root_star)
                ++lb_bins[0];
            else if (lb <= root_star * 1.05)
                ++lb_bins[1];
            else if (lb <= root_star * 1.10)
                ++lb_bins[2];
            else if (lb <= root_star * 1.25)
                ++lb_bins[3];
            else
                ++lb_bins[4];
        }

        std::vector<int> sorted_owner = owner_size;
        std::sort(sorted_owner.begin(), sorted_owner.end());
        const double total_sec = std::chrono::duration<double>(Clock::now() - total_start).count();

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "graph_folder=" << graph_folder << "\n";
        std::cout << "query_index=" << (query_index + 1)
                  << " n=" << n
                  << " m=" << graph.m
                  << " g=" << g
                  << " group_vertices=" << total_group_vertices
                  << "\n";
        std::cout << "group_dist_ms=" << gd_ms
                  << " total_sec=" << total_sec
                  << "\n";
        std::cout << "root_star_upper=" << root_star
                  << " root_star_vertex=" << root_star_vertex
                  << "\n";
        std::cout << "alive_roots_by_root_star=" << alive_root_star
                  << " alive_pct=" << Percent(alive_root_star, n)
                  << " alive_boundary=" << alive_root_star_boundary
                  << " alive_boundary_pct=" << Percent(alive_root_star_boundary, std::max<long long>(alive_root_star, 1))
                  << "\n";
        std::cout << "lb_bins_le_B=" << lb_bins[0]
                  << " le_1.05B=" << lb_bins[1]
                  << " le_1.10B=" << lb_bins[2]
                  << " le_1.25B=" << lb_bins[3]
                  << " gt_1.25B=" << lb_bins[4]
                  << "\n";
        std::cout << "voronoi_boundary_vertices=" << boundary_vertices
                  << " boundary_vertex_pct=" << Percent(boundary_vertices, n)
                  << " boundary_edges=" << boundary_edges
                  << " boundary_edge_pct=" << Percent(boundary_edges, graph.m)
                  << " tie_vertices=" << tie_vertices
                  << "\n";
        std::cout << "non_boundary_components=" << components
                  << " largest_component=" << largest_component
                  << " largest_component_pct=" << Percent(largest_component, n)
                  << "\n";
        std::cout << "component_terminals=" << terminal_vertices_in_components
                  << " terminal_components=" << terminal_components
                  << " no_terminal_components=" << no_terminal_components
                  << " no_terminal_vertices=" << no_terminal_vertices
                  << "\n";
        std::cout << "component_portals_total=" << total_portals
                  << " avg_portals=" << (components ? static_cast<double>(total_portals) / components : 0.0)
                  << " max_portals=" << max_portals
                  << " portal_edges=" << total_portal_edges
                  << "\n";
        std::cout << "component_portal_bins_p0=" << portal_bins[0]
                  << " p1=" << portal_bins[1]
                  << " p2=" << portal_bins[2]
                  << " p3_4=" << portal_bins[3]
                  << " p5_8=" << portal_bins[4]
                  << " p9_16=" << portal_bins[5]
                  << " pgt16=" << portal_bins[6]
                  << "\n";
        std::cout << "removable_leaf_components=" << removable_leaf_components
                  << " removable_leaf_vertices=" << removable_leaf_vertices
                  << " removable_leaf_vertex_pct=" << Percent(removable_leaf_vertices, n)
                  << "\n";
        std::cout << "no_terminal_portal_components_p0=" << no_terminal_portal_components[0]
                  << " p1=" << no_terminal_portal_components[1]
                  << " p2=" << no_terminal_portal_components[2]
                  << " p3=" << no_terminal_portal_components[3]
                  << " p4=" << no_terminal_portal_components[4]
                  << " pge5=" << no_terminal_portal_components[5]
                  << "\n";
        std::cout << "no_terminal_portal_vertices_p0=" << no_terminal_portal_vertices[0]
                  << " p1=" << no_terminal_portal_vertices[1]
                  << " p2=" << no_terminal_portal_vertices[2]
                  << " p3=" << no_terminal_portal_vertices[3]
                  << " p4=" << no_terminal_portal_vertices[4]
                  << " pge5=" << no_terminal_portal_vertices[5]
                  << "\n";
        std::cout << "terminal_portal_components_p0=" << terminal_portal_components[0]
                  << " p1=" << terminal_portal_components[1]
                  << " p2=" << terminal_portal_components[2]
                  << " p3=" << terminal_portal_components[3]
                  << " p4=" << terminal_portal_components[4]
                  << " pge5=" << terminal_portal_components[5]
                  << "\n";
        std::cout << "terminal_portal_vertices_p0=" << terminal_portal_vertices[0]
                  << " p1=" << terminal_portal_vertices[1]
                  << " p2=" << terminal_portal_vertices[2]
                  << " p3=" << terminal_portal_vertices[3]
                  << " p4=" << terminal_portal_vertices[4]
                  << " pge5=" << terminal_portal_vertices[5]
                  << "\n";
        const long long no_terminal_p_le4_components =
            no_terminal_portal_components[0] + no_terminal_portal_components[1] +
            no_terminal_portal_components[2] + no_terminal_portal_components[3] +
            no_terminal_portal_components[4];
        const long long no_terminal_p_le4_vertices =
            no_terminal_portal_vertices[0] + no_terminal_portal_vertices[1] +
            no_terminal_portal_vertices[2] + no_terminal_portal_vertices[3] +
            no_terminal_portal_vertices[4];
        std::cout << "no_terminal_p_le4_components=" << no_terminal_p_le4_components
                  << " no_terminal_p_le4_vertices=" << no_terminal_p_le4_vertices
                  << " no_terminal_p_le4_vertex_pct=" << Percent(no_terminal_p_le4_vertices, n)
                  << " no_terminal_p4_local_table_entries="
                  << (no_terminal_portal_components[4] * 11)
                  << " largest_no_terminal_p4_component=" << largest_no_terminal_p4_component
                  << "\n";
        std::cout << "no_terminal_p4_table_components=" << no_terminal_p4_table_components
                  << " no_terminal_p4_table_vertices=" << no_terminal_p4_table_vertices
                  << " quad_hub_feasible_components=" << no_terminal_p4_quad_hub_components
                  << " quad_hub_feasible_vertices=" << no_terminal_p4_quad_hub_vertices
                  << " quad_hub_feasible_component_pct="
                  << Percent(no_terminal_p4_quad_hub_components, no_terminal_p4_table_components)
                  << " quad_hub_feasible_vertex_pct="
                  << Percent(no_terminal_p4_quad_hub_vertices, no_terminal_p4_table_vertices)
                  << "\n";
        std::cout << "metric_torso_vertices_est=" << (boundary_vertices + terminal_vertices_in_components)
                  << " metric_torso_vertex_pct=" << Percent(boundary_vertices + terminal_vertices_in_components, n)
                  << " component_portal_pairs=" << portal_pairs
                  << " metric_torso_component_edges_est=" << metric_torso_edges_from_components
                  << "\n";
        std::cout << "owner_size_min=" << sorted_owner.front()
                  << " owner_size_median=" << sorted_owner[g / 2]
                  << " owner_size_max=" << sorted_owner.back()
                  << "\n";
        for (int a = 0; a < g; ++a)
        {
            std::cout << "owner_" << a
                      << "_vertices=" << owner_size[a]
                      << " components=" << components_by_owner[a]
                      << " largest_component=" << largest_component_by_owner[a]
                      << " group_size=" << group_size[a]
                      << "\n";
        }
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
