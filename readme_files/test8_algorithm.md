# Test8 算法说明

本文记录 `methods/Test/test8.cpp` 的算法思路与剪枝正确性。Test8 仍保持按 `popcount(mask)` 的 DP 顺序；每次图上 Dijkstra 只服务当前 `mask`，没有把不同大小的状态放进同一个全局堆。

状态定义：

```text
dp[mask][v] = 覆盖 mask 中所有组，并以 v 为当前连接点的最小连通子树代价
```

令 `U` 为查询组全集，`H=floor(g/2)`。算法只显式图上扩展 `|mask|<=H` 的状态，完整答案通过同根合并更新 `dp[U][v]` 和 `best`。

## 预处理

对每个组 `a` 跑一次多源 Dijkstra：

```text
group_dist[a][v] = dist(v, group_a)
```

同时维护：

```text
vertex_group_mask[v] = v 本身属于哪些组
group_pair[a][b] = min_{y in group_b} group_dist[a][y]
```

`group_pair` 用于剩余组 MST 下界。该预处理是 `g` 次 SSSP，不是 APSP。

## 初始上界剪枝

Test8 先计算 root-star 上界：

```text
root_star = min_v sum_a group_dist[a][v]
```

固定 `v` 时，把 `v` 到每个组最近候选点的最短路并起来，得到连通且覆盖所有组的子图。并集边权不超过路径长度之和，所以 `root_star` 是合法上界。

随后在 root-star 选出的 `best_root` 上运行组件增长上界：

```text
covered = vertex_group_mask[best_root]
selected = {best_root}
while covered != U:
    从 selected 多源 Dijkstra
    遇到第一个属于未覆盖组的节点 x
    total += dist(selected, x)
    selected 加入 x
    covered |= vertex_group_mask[x]
```

每轮加入的是当前连通组件到某个未覆盖组的最短路。加入后仍连通，并且至少多覆盖一个组。因此循环结束时得到合法 GST 可行解；路径长度之和是其上界。用它更新 `best` 不会低于最优值，只会加强后续 `dp+LB>best` 剪枝。

## 剩余组下界剪枝

对状态 `(mask,v)`，令 `rem=U^mask`。Test8 使用：

```text
far(v,rem) = max_{a in rem} group_dist[a][v]
near2(v,rem) = rem 中距离 v 最近两个组的距离和
mst(rem) = 以 group_pair 为边权的 rem 组间 MST 代价
LB(v,rem) = max(far(v,rem), mst(rem)/2 + near2(v,rem)/2)
```

若 `rem` 只有一个组，则 `LB=far`。

`far` 正确性：任意完成方案必须连接到每个剩余组，故代价至少为到每个剩余组距离的最大值。

`mst/2+near2/2` 正确性：任意完成方案实际会在每个剩余组中选择某个代表点。对这些代表点和 `v`，普通 Steiner tree 的 1-tree 下界给出：

```text
completion >= (MST(代表点集合) + v 到两个最近代表点距离和) / 2
```

而 `group_pair` 和 `group_dist` 不大于具体代表点距离，所以组版本只会更小，仍是安全下界。两个安全下界取 `max` 仍安全。

剪枝条件：

```text
dp[mask][v] + LB(v,U^mask) > best
```

成立时，该状态不可能扩展成优于当前上界的完整解，可跳过入队、出队或边松弛。

一致性：`group_dist[a][v] <= w(v,u)+group_dist[a][u]`，所以 `far` 一致；两个最近组距离和除以 2 后也一致；`mst(rem)` 与节点无关。故 `LB` 一致，可以用于 `key=dp+LB` 的 Dijkstra 队列排序。

## `h` 目标剪枝

代码维护：

```text
h[x][v] = 同根合并中观察到的、可作为 x 一侧组成部分的最大已确认子状态代价
```

处理 `mask` 时，若：

```text
dp[mask][v] + h[U^mask][v] <= best
```

则 `(mask,v)` 是需要被当前 Dijkstra 触达的目标点。为了保证到目标点的最短路前缀可被探索，代码还把 `dp[mask][v] < mx_used` 的非目标点加入堆。这里 `mx_used` 是目标点中的最大 `dp` 值。

该逻辑继承自 Test5/Test6。Test8 只在其外层叠加安全上界和安全下界，不改变它的前提。

## Active Vertices

Test8 为每个 mask 维护：

```text
active_vertices[mask] = 当前 dp[mask][v] finite 的所有 v
```

`SetDp(mask,v,value)` 仅在 `dp[mask][v]` 首次从 `inf` 变为 finite 时，把 `v` 加入该列表。

处理某个 `mask` 时，算法只扫描 `active_vertices[mask]`，并且只对访问到的节点按需计算 `LB`。

正确性：若 `dp[mask][v]=inf`，它不可能入队、扩展或改善答案。跳过所有非 active 点等价于跳过无效状态，不改变最优值。

## 混合同根合并

当一个状态被确认后，`Modify(mask,v)` 做同根合并。原始枚举是：

```text
t subset (U^mask)
```

但只有 `dp[t][v]` finite 的 `t` 会产生有效转移。Test8 同时维护：

```text
root_masks[v] = 当前 dp[*][v] finite 的 mask 列表
```

在 `Modify(mask,v)` 中选择较小的枚举方式：

```text
若 root_masks[v].size() <= 2^{|U^mask|}-1:
    扫 root_masks[v]，只处理 t&mask==0
否则:
    枚举 t subset (U^mask)，检查 dp[t][v] 是否 finite
```

正确性：两种方式枚举到的有效集合相同，都是：

```text
{ t | (t & mask)==0 且 dp[t][v] finite }
```

区别只在于访问无效候选的数量。因此混合枚举只减少同根合并检查次数，不改变任何可产生的 `dp[mask|t][v]`。

`SetDp` 首次 finite 才追加 `root_masks[v]`，所以同一个 `(mask,v)` 不会重复出现在列表中。

## 图上扩展流程

对每个 `|mask|<=H` 的 mask：

1. 遍历 `active_vertices[mask]`。
2. 对满足 `dp+h<=best` 且 `dp+LB<=best` 的点标记为目标并入堆。
3. 把 `dp<mx_used` 的非目标点也入堆，以覆盖目标最短路前缀。
4. 堆内 key 为 `dp+LB`。
5. 出队时若 `dp+LB>best` 跳过。
6. 目标点出队时调用 `Modify(mask,v)`。
7. 边松弛若 `new_dp+LB>best` 则跳过，否则更新 `dp[mask][to]`。

整个过程仍按 `popcount(mask)` 外层顺序执行。

## 统计项

Test8 输出核心与诊断统计：

```text
valid_total / valid_by_size    被确认并调用 Modify 的状态数
up_subset / merge_by_size      同根合并次数
inqueue / inqueue_by_size      Dijkstra 出队有效状态数
active_seed / active_by_size   各 mask 扫描的 active 顶点数
full_seed                      若全图扫描会访问的顶点数
lb_calls                       实际计算 LB 的次数
pq_push / pq_pop               堆操作次数
relax_try / relax_ok           边松弛尝试与成功次数
star_ub / greedy_ub            两个初始上界
prep_ms / dp_ms                预处理与 DP 阶段时间
```

统计不参与正确性，只用于分析瓶颈。

## 代价汇总

相对 Test7，Test8 新增或强化：

```text
vertex_group_mask: O(n + 总组节点数)
greedy 上界: 最多 g 轮多源 Dijkstra，通常提前停止
active_vertices: O(finite states) 空间，减少每个 mask 的全图扫描
混合同根合并: 在 root_masks 扫描和互补子集枚举之间取较小者
```

这些优化都不改变状态定义和 DP 顺序；它们只提供更强安全上界，或跳过原算法中不会产生有效转移的状态/枚举。
