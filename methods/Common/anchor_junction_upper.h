#ifndef GST_METHODS_COMMON_ANCHOR_JUNCTION_UPPER_H
#define GST_METHODS_COMMON_ANCHOR_JUNCTION_UPPER_H

#include <vector>

#include "../../graph_io.h"
#include "../../query_io.h"

namespace gst::methods::anchor_junction
{
struct Result
{
    double upper = 0.0;
    std::vector<int> anchor_path;
    int candidate_roots = 0;
    int tree_vertices = 0;
    long long triple_scans = 0;
    long long convolutions = 0;
    long long work = 0;
};

Result BuildUpper(const Graph& graph,
                  const Query& query,
                  const std::vector<std::vector<double>>& group_distance,
                  int root,
                  int anchor_group);

}  // namespace gst::methods::anchor_junction

#endif
