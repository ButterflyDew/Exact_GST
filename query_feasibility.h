#ifndef GST_QUERY_FEASIBILITY_H
#define GST_QUERY_FEASIBILITY_H

#include "graph_io.h"
#include "query_io.h"

namespace gst
{

// 判断当前询问是否存在可行解：
// 若存在某个连通分量与每个组至少有一个点相交，则该询问有解。
bool IsQueryFeasible(const Graph& graph, const Query& query);

}  // namespace gst

#endif  // GST_QUERY_FEASIBILITY_H
