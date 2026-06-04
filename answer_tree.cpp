#include "answer_tree.h"

#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace gst
{

namespace
{

struct WeightedLink
{
    int to = -1;
    double w = 0.0;
};

// 在虚树上 DFS，统计每个点子树里的叶子数以及深度信息。
void DfsBuildMeta(int u,
                  int p,
                  const std::vector<std::vector<WeightedLink>>& adj,
                  const std::vector<int>& is_terminal,
                  std::vector<int>* subtree_leaf_count,
                  std::vector<int>* depth,
                  int* max_height)
{
    (*subtree_leaf_count)[u] = is_terminal[u] ? 1 : 0;
    for (const auto& e : adj[u])
    {
        if (e.to == p)
        {
            continue;
        }
        (*depth)[e.to] = (*depth)[u] + 1;
        *max_height = std::max(*max_height, (*depth)[e.to]);
        DfsBuildMeta(e.to, u, adj, is_terminal, subtree_leaf_count, depth, max_height);
        (*subtree_leaf_count)[u] += (*subtree_leaf_count)[e.to];
    }
}

}  // namespace

AnswerTreeBase::AnswerTreeBase(double total_weight)
    : total_weight_(total_weight)
{
}

ConcreteAnswerTree::ConcreteAnswerTree(double total_weight,
                                       std::vector<UndirectedEdge> tree_edges,
                                       std::vector<int> selected_vertex_per_group)
    : AnswerTreeBase(total_weight),
      tree_edges_(std::move(tree_edges)),
      selected_vertex_per_group_(std::move(selected_vertex_per_group))
{
}

std::string ConcreteAnswerTree::ToText() const
{
    std::ostringstream oss;
    oss << "total_weight " << total_weight_ << "\n";
    oss << "edge_count " << tree_edges_.size() << "\n";
    for (const auto& e : tree_edges_)
    {
        oss << e.u << " " << e.v << " " << e.w << "\n";
    }
    oss << "selected_group_vertices " << selected_vertex_per_group_.size() << "\n";
    for (size_t i = 0; i < selected_vertex_per_group_.size(); ++i)
    {
        oss << "group_" << i + 1 << " " << selected_vertex_per_group_[i] << "\n";
    }
    return oss.str();
}

VirtualTreeAnswer::VirtualTreeAnswer(double total_weight,
                                     std::vector<VirtualNodeInfo> nodes,
                                     std::vector<VirtualEdgeInfo> edges,
                                     int root_id)
    : AnswerTreeBase(total_weight), nodes_(std::move(nodes)), edges_(std::move(edges)), root_id_(root_id)
{
}

VirtualTreeAnswer VirtualTreeAnswer::FromConcrete(const ConcreteAnswerTree& concrete, VirtualRootPolicy root_policy)
{
    std::set<int> used_vertices;
    for (const auto& e : concrete.tree_edges())
    {
        used_vertices.insert(e.u);
        used_vertices.insert(e.v);
    }
    for (int v : concrete.selected_vertex_per_group())
    {
        if (v > 0)
        {
            used_vertices.insert(v);
        }
    }
    if (used_vertices.empty())
    {
        std::vector<VirtualNodeInfo> nodes = {{0, -1, -1, {}, 0, 0}};
        return VirtualTreeAnswer(concrete.total_weight(), nodes, {}, 0);
    }

    std::unordered_map<int, std::vector<WeightedLink>> original_adj;
    for (const auto& e : concrete.tree_edges())
    {
        original_adj[e.u].push_back({e.v, e.w});
        original_adj[e.v].push_back({e.u, e.w});
    }

    std::unordered_map<int, int> terminal_cnt;
    for (int v : concrete.selected_vertex_per_group())
    {
        if (v > 0)
        {
            terminal_cnt[v]++;
        }
    }

    // 压缩树：仅保留“关键点”（度不等于 2，或被某组选择的终端点）。
    std::unordered_set<int> key_vertices;
    for (int v : used_vertices)
    {
        int deg = static_cast<int>(original_adj[v].size());
        if (deg != 2 || terminal_cnt.count(v))
        {
            key_vertices.insert(v);
        }
    }

    std::vector<int> key_list(key_vertices.begin(), key_vertices.end());
    std::sort(key_list.begin(), key_list.end());
    std::unordered_map<int, int> key_id;
    for (int i = 0; i < static_cast<int>(key_list.size()); ++i)
    {
        key_id[key_list[i]] = i;
    }

    std::vector<std::vector<WeightedLink>> virtual_adj(key_list.size());
    std::set<std::pair<int, int>> used_key_edge;
    for (int start_vertex : key_list)
    {
        int start_id = key_id[start_vertex];
        for (const auto& out : original_adj[start_vertex])
        {
            int prev = start_vertex;
            int cur = out.to;
            double acc_w = out.w;
            while (!key_vertices.count(cur))
            {
                const auto& neigh = original_adj[cur];
                if (neigh.size() != 2)
                {
                    throw std::runtime_error("Unexpected non-key degree in compressed tree.");
                }
                int next = (neigh[0].to == prev) ? neigh[1].to : neigh[0].to;
                double next_w = (neigh[0].to == prev) ? neigh[1].w : neigh[0].w;
                prev = cur;
                cur = next;
                acc_w += next_w;
            }
            int end_id = key_id[cur];
            if (start_id == end_id)
            {
                continue;
            }
            auto edge_key = std::minmax(start_id, end_id);
            if (used_key_edge.insert(edge_key).second)
            {
                virtual_adj[start_id].push_back({end_id, acc_w});
                virtual_adj[end_id].push_back({start_id, acc_w});
            }
        }
    }

    std::vector<int> is_terminal(key_list.size(), 0);
    for (int i = 0; i < static_cast<int>(key_list.size()); ++i)
    {
        is_terminal[i] = terminal_cnt.count(key_list[i]) ? 1 : 0;
    }

    // 选根规则支持两种策略，通过参数控制优先级顺序。
    auto evaluate_root = [&](int root) -> std::tuple<int, int, int>
    {
        std::vector<int> subtree_leaf_count(key_list.size(), 0);
        std::vector<int> depth(key_list.size(), 0);
        int max_height = 0;
        DfsBuildMeta(root, -1, virtual_adj, is_terminal, &subtree_leaf_count, &depth, &max_height);
        int max_child_leaf = 0;
        for (const auto& e : virtual_adj[root])
        {
            max_child_leaf = std::max(max_child_leaf, subtree_leaf_count[e.to]);
        }
        if (root_policy == VirtualRootPolicy::kMinHeightThenMaxChild)
        {
            return {max_height, max_child_leaf, key_list[root]};
        }
        return {max_child_leaf, max_height, key_list[root]};
    };

    int best_root = 0;
    std::tuple<int, int, int> best_score = evaluate_root(0);
    for (int i = 1; i < static_cast<int>(key_list.size()); ++i)
    {
        auto score = evaluate_root(i);
        if (score < best_score)
        {
            best_score = score;
            best_root = i;
        }
    }

    std::vector<VirtualNodeInfo> nodes(key_list.size());
    for (int i = 0; i < static_cast<int>(key_list.size()); ++i)
    {
        nodes[i].id = i;
        nodes[i].original_vertex = key_list[i];
    }
    std::vector<VirtualEdgeInfo> edges;
    std::vector<int> subtree_leaf_count(key_list.size(), 0);
    std::vector<int> depth(key_list.size(), 0);

    std::function<void(int, int)> build = [&](int u, int p)
    {
        nodes[u].parent = p;
        nodes[u].depth = depth[u];
        subtree_leaf_count[u] = is_terminal[u] ? 1 : 0;
        for (const auto& e : virtual_adj[u])
        {
            if (e.to == p)
            {
                continue;
            }
            depth[e.to] = depth[u] + 1;
            nodes[u].children.push_back(e.to);
            edges.push_back({u, e.to, e.w});
            build(e.to, u);
            subtree_leaf_count[u] += subtree_leaf_count[e.to];
        }
        nodes[u].leaf_count = subtree_leaf_count[u];
    };

    build(best_root, -1);
    return VirtualTreeAnswer(concrete.total_weight(), std::move(nodes), std::move(edges), best_root);
}

std::string VirtualTreeAnswer::ToText() const
{
    std::ostringstream oss;
    oss << "total_weight " << total_weight_ << "\n";
    oss << "virtual_node_count " << nodes_.size() << "\n";
    oss << "virtual_edge_count " << edges_.size() << "\n";
    oss << "root_id " << root_id_ << " original_vertex "
        << (root_id_ >= 0 ? nodes_[root_id_].original_vertex : -1) << "\n";
    for (const auto& n : nodes_)
    {
        oss << "node " << n.id << " ov " << n.original_vertex << " parent " << n.parent << " depth " << n.depth
            << " leaf_count " << n.leaf_count << "\n";
    }
    for (const auto& e : edges_)
    {
        oss << "edge " << e.parent << " " << e.child << " " << e.w << "\n";
    }
    return oss.str();
}

}  // namespace gst
