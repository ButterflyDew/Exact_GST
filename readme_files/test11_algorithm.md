# Test11 算法

## 问题

无向非负边权图，查询含 `g` 个组，每组若干候选点。求最小代价连通子图，使每组至少命中一个候选点。

## 半集合 DP

设 `H = floor(g/2)`，`U = (1<<g)-1`。

```text
dp[mask][v] = 覆盖 mask 中所有组、并以 v 为同根连接点的最小代价
```

只对 `|mask| <= H` 做图上 Dijkstra；完整答案通过同根合并得到。

辅助量 `h[x][v]`：记录同根 v 上，来自更小 mask 的 dp 证据，用于 Dijkstra 目标筛选：

```text
dp[mask][v] + h[U^mask][v] <= best
```

缺省 `h = -1` 表示无证据，不收紧筛选。

## 预处理

每组一次多源 Dijkstra：

```text
group_dist[a][v] = dist(v, group_a)
group_pair[a][b] = min_{x in group_b} group_dist[a][x]
```

`group_dist` 用于上下界；`group_pair` 用于 `mst(rem)/2`。

## 上界

root-star：

```text
best = min_v sum_a group_dist[a][v]
```

从 root-star 最优根做组件增长 greedy，上界合法，只用于剪枝。

## 下界

剩余组 `rem = U ^ mask`：

```text
far(v,rem)   = max_{a in rem} group_dist[a][v]
near2(v,rem) = rem 中离 v 最近两个组的距离和
mst(rem)     = group_pair 上 rem 的 MST 代价
LB(v,rem)    = max(far, mst/2 + near2/2)
```

若 `dp[mask][v] + LB(v, rem) > best`，该状态无法导出更优完整解。

## 数据结构

- 稠密表 `dp[mask][v]`、`h[mask][v]`
- `active_vertices[mask]`：当前 mask 下 dp 有限的顶点
- `root_masks_by_size[v][sz]`：同根 v 上、`|mask|=sz` 且 dp 有限的 mask 列表

`SetDp` 在首次写入时维护后两者。

## 同根合并

每次 `Modify(mask, v, w)` 写入 `dp[mask][v]=w` 后，按三类用途枚举同根 t：

**best** — 只查补集：

```text
t = U ^ mask
若 dp[t][v] 有限：best = min(best, w + dp[t][v])
```

**live-dp** — 扩展仍可能进堆的 dp 状态：

```text
枚举 |t| <= g - 2|mask| 的 t（来自 root_masks_by_size[v]）
nxt = mask | t
cand = w + dp[t][v]
```

写入前剪枝（边权与合并代价非负，安全）：

```text
cand >= best                          → 跳过
cand + far(v, U^nxt) >= best          → 跳过
```

若 `|mask| = H` 且 g 为偶数，`live_limit = g - 2H = 0`，不再产生新 live-dp。

**future-h** — 为后续 mask 提供 h 证据：

```text
枚举 g - H - |mask| <= |t| <= g - |mask| 的 t
h[mask|t][v] = max(h[mask|t][v], w)
```

h 不做 far 剪枝：实测命中率低，额外计算不划算。

## 主循环

按 `popcount(mask)` 升序处理 `|mask| <= H`：

1. **初始化**：每组候选点 `(1<<gi, v, 0)` 调用 `Modify`。
2. **Dijkstra 入堆**（当前 mask，补集 `rem = U^mask`）：
   - **目标点**：`dp + h[rem] <= best` 且 `dp + LB <= best`，标记为 target，`used_cnt++`。
   - **前缀点**：不满足目标条件但 `dp < mx_used`（mx_used 为当前目标点最大 dp），也入堆。它们不能改善 best，但可能松弛出更优 dp，进而修正 h，避免错误剪枝。
3. **Dijkstra 弹出**：跳过 stale / `dp+LB>best`；target 弹出时调用 `Modify`；边松弛用 `dp+LB<=best` 门控。
4. 重复至堆空或 `used_cnt=0`。

## 统计字段

输出在 `<method>_stats.txt`，主要字段：

```text
valid_total / up_subset / inqueue
bucket_scan
live_dp_checks / future_h_checks
prune_ge_best / prune_far
active_seed / lb_calls
pq_push / pq_pop
relax_try / relax_ok
prep_ms / group_dist_ms / greedy_ms / dp_ms
```

按 mask 大小的分桶：`valid_k / total_k / active_k / inqueue_k / merge_k`。
