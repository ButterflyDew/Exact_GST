#ifndef GST_ANSWER_TREE_H
#define GST_ANSWER_TREE_H

#include <string>
#include <vector>

#include "graph_io.h"

namespace gst
{

class AnswerTreeBase
{
public:
    explicit AnswerTreeBase(double total_weight);
    virtual ~AnswerTreeBase() = default;

    double total_weight() const
    {
        return total_weight_;
    }
    virtual std::string ToText() const = 0;

protected:
    double total_weight_ = 0.0;
};

class ConcreteAnswerTree : public AnswerTreeBase
{
public:
    ConcreteAnswerTree(double total_weight, std::vector<UndirectedEdge> tree_edges, std::vector<int> selected_vertex_per_group);

    const std::vector<UndirectedEdge>& tree_edges() const
    {
        return tree_edges_;
    }
    const std::vector<int>& selected_vertex_per_group() const
    {
        return selected_vertex_per_group_;
    }
    std::string ToText() const override;

private:
    std::vector<UndirectedEdge> tree_edges_;
    std::vector<int> selected_vertex_per_group_;
};

// 虚树节点信息：用于紧凑描述组斯坦纳树结构。
struct VirtualNodeInfo
{
    int id = -1;
    int original_vertex = -1;
    int parent = -1;
    std::vector<int> children;
    int leaf_count = 0;
    int depth = 0;
};

struct VirtualEdgeInfo
{
    int parent = -1;
    int child = -1;
    double w = 0.0;
};

enum class VirtualRootPolicy
{
    // 旧规则：先比较根最大孩子子树叶子数，再比较树高。
    kMinMaxChildThenHeight = 0,
    // 新规则：先比较树高，再比较根最大孩子子树叶子数。
    kMinHeightThenMaxChild = 1,
};

class VirtualTreeAnswer : public AnswerTreeBase
{
public:
    VirtualTreeAnswer(double total_weight, std::vector<VirtualNodeInfo> nodes, std::vector<VirtualEdgeInfo> edges, int root_id);

    static VirtualTreeAnswer FromConcrete(const ConcreteAnswerTree& concrete, VirtualRootPolicy root_policy);

    const std::vector<VirtualNodeInfo>& nodes() const
    {
        return nodes_;
    }
    const std::vector<VirtualEdgeInfo>& edges() const
    {
        return edges_;
    }
    int root_id() const
    {
        return root_id_;
    }
    std::string ToText() const override;

private:
    std::vector<VirtualNodeInfo> nodes_;
    std::vector<VirtualEdgeInfo> edges_;
    int root_id_ = -1;
};

}  // namespace gst

#endif  // GST_ANSWER_TREE_H
