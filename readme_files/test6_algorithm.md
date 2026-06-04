# Test6 算法说明

本文记录 `methods/Test/test6.cpp` 的核心思路、剪枝条件和正确性依据。图为无向非负边权图，查询有 `g` 个组，全集记为 `U`。状态 `dp[s][v]` 表示一棵连通子树覆盖组集合 `s`，并以节点 `v` 作为当前连接点时的最小代价。

## 基本 DP

初始化时，对每个组 `a` 的候选节点 `v` 设置 `dp[1<<a][v]=0`。随后只显式处理 `|s|<=g/2` 的状态。对一个已确认的状态 `(s,v)`，代码执行两类转移：

1. 同根合并：用 `dp[s][v]+dp[t][v]` 更新 `dp[s|t][v]`。
2. 图上扩展：对固定 `s`，在图上做 Dijkstra，沿边把 `dp[s][u]` 扩展到 `dp[s][to]`。

只处理半侧状态的原因是：任意完整解在某个连接点可以看成两棵子树的合并，其中至少一侧覆盖组数不超过 `g/2`。因此算法只需要显式传播小侧状态，再用同根合并得到 `U` 的候选值。

## 多源组距离

Test6 为每个组 `a` 跑一次多源 Dijkstra：

```text
group_dist[a][v] = dist(v, group_a)
```

额外代价为 `g` 次 SSSP，即 `O(g(m+n log n))`。这不是 APSP。

同时计算组间距离：

```text
group_pair[a][b] = min_{x in group_a, y in group_b} dist(x,y)
                  = min_{y in group_b} group_dist[a][y]
```

该距离用于剩余组集合上的 MST 下界。

## Test6 新增下界

对状态 `(s,v)`，令 `rem = U ^ s`。若 `rem` 非空，定义：

```text
far(v,rem) = max_{a in rem} group_dist[a][v]
near2(v,rem) = rem 中距 v 最近的两个组距离之和
mst(rem) = 以 group_pair 为边权的 rem 组间 MST 代价
LB(v,rem) = max(far(v,rem), mst(rem)/2 + near2(v,rem)/2)
```

若 `rem` 只有一个组，则 `LB(v,rem)=far(v,rem)`。

### `far` 的正确性

任意完成方案必须从当前连通块连接到每个剩余组。对任意剩余组 `a`，完成代价至少为 `dist(v, group_a)`，因此至少为这些距离的最大值：

```text
completion(v,rem) >= max_a dist(v, group_a)
```

所以 `far` 是安全下界。

### MST/1-tree 下界的正确性

考虑任意完成方案，并从每个剩余组中取它实际覆盖的一个代表点。该完成方案连接了 `v` 和这些代表点。对普通 Steiner tree，有经典 1-tree 下界：

```text
completion >= (MST(代表点集合) + v 到两个最近代表点的距离和) / 2
```

Test6 使用的是组间最短距离和到组最短距离。它们不大于任意具体代表点之间的距离：

```text
group_pair[a][b] <= dist(rep_a, rep_b)
group_dist[a][v] <= dist(v, rep_a)
```

因此用组距离得到的 `mst(rem)/2 + near2(v,rem)/2` 不会大于代表点版本的 1-tree 下界，也就不会大于真实完成代价。故它是安全下界。

两个安全下界取 `max` 仍然是安全下界。

## 下界剪枝

Test6 在三个位置使用：

```text
dp[s][v] + LB(v,rem) > best
```

若成立，则 `(s,v)` 不可能扩展成比当前 `best` 更好的完整解，可跳过入队、出队处理或边松弛。

证明很直接：`LB(v,rem)` 是从 `(s,v)` 覆盖全部剩余组所需额外代价的下界，因此任何完整解代价至少为 `dp[s][v]+LB(v,rem)`。若它已超过 `best`，则该状态及其后续无法改善答案。

## 一致性

对任意边 `(v,w)`，权重为 `c`：

```text
group_dist[a][v] <= c + group_dist[a][w]
```

因此 `far(v,rem) <= c + far(w,rem)`。

`near2(v,rem)/2` 也一致。取在 `w` 处最近的两个组 `a,b`，则：

```text
group_dist[a][v] + group_dist[b][v]
<= group_dist[a][w] + group_dist[b][w] + 2c
```

两边除以 2 得到 `near2(v,rem)/2 <= near2(w,rem)/2 + c`。`mst(rem)/2` 与节点无关，不影响一致性。多个一致下界取 `max` 仍一致，所以 `LB` 可作为 Dijkstra 的 A* 式 key：

```text
key = dp[s][v] + LB(v,rem)
```

这保证沿边扩展时 key 不会因下界而破坏非负边权下的顺序性质。

## 继承自 Test5 的 `h` 剪枝

代码维护：

```text
h[x][v] = 已在同根合并中观察到的、可作为 x 一侧组成部分的最大已确认子状态代价
```

处理 `s` 时，若：

```text
dp[s][v] + h[U^s][v] <= best
```

则把 `(s,v)` 标记为目标状态并入队。Dijkstra 不需要永久化所有节点，只需要保证这些目标状态被处理；为了让通向目标的最短路可达，代码还会把 `dp[s][v] < mx_used` 的非目标节点加入候选队列，其中 `mx_used` 是当前目标状态的最大 `dp` 值。

这个规则的作用是减少对固定 mask 的全图扩展。它的安全性依赖于 `h[U^s][v]` 能代表当前已知互补侧对同根合并的可用性：若一个状态可能通过当前已知互补侧形成不超过 `best` 的完整解，它会成为目标；而任何目标的最短路径中，路径前缀距离不超过目标距离，因此会被 `dp < mx_used` 的候选范围覆盖。

Test6 没有改变这部分逻辑，只在其外层叠加了严格下界 `LB`。因此新增剪枝不会削弱原有正确性前提。

## 代价汇总

新增预处理：

```text
g 次多源 Dijkstra: O(g(m+n log n))
组间距离汇总: O(sum_a sum_b |group_b|)
MST(mask) 懒计算: 每个访问到的 mask O(g^2)，结果缓存
每个处理的 mask 预计算 lb[v]: O(n g)
```

新增空间：

```text
group_dist: O(g n)
group_pair: O(g^2)
mst_cache: O(2^g)
每个 mask 临时 lb: O(n)
```

整体上，Test6 用 `g` 次 SSSP 和轻量级 mask 缓存换取对 `(v,s)` 状态入队、出队和边扩展的剪枝，不需要 APSP。
