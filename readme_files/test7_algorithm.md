# Test7 算法说明

本文记录 `methods/Test/test7.cpp` 的算法思路。Test7 基于 Test6：保留半侧 DP、组距离下界 `LB(v,rem)`、`h` 目标剪枝和 A* 式 Dijkstra key；新增两点：

1. 用 root-star 可行解初始化 `best`。
2. 同根合并只枚举同一根上已存在的 finite mask。

图为无向非负边权图，查询组数为 `g`，全集为 `U`。状态：

```text
dp[s][v] = 覆盖组集合 s，并以 v 为当前连接点的最小连通子树代价
```

只显式图上扩展 `|s|<=H=floor(g/2)` 的状态，完整答案通过同根合并更新 `dp[U][v]` 和 `best`。

## 继承自 Test6 的部分

Test7 先对每个组 `a` 跑一次多源 Dijkstra：

```text
group_dist[a][v] = dist(v, group_a)
```

并得到组间距离：

```text
group_pair[a][b] = min_{y in group_b} group_dist[a][y]
```

对剩余组 `rem=U^s` 使用 Test6 的下界：

```text
far(v,rem) = max_{a in rem} group_dist[a][v]
mst(rem) = 以 group_pair 为边权的 rem 组间 MST 代价
near2(v,rem) = rem 中距离 v 最近两个组的距离和
LB(v,rem) = max(far(v,rem), mst(rem)/2 + near2(v,rem)/2)
```

若 `rem` 只有一个组，则 `LB=far`。

`LB` 的正确性与 Test6 相同：`far` 是必须到达每个剩余组的下界；`mst/2+near2/2` 是把普通 Steiner tree 的 1-tree 下界弱化到组距离后的安全下界。多个安全下界取 `max` 仍安全。

Test7 仍在以下位置使用：

```text
dp[s][v] + LB(v,U^s) > best
```

来跳过入队、出队或边松弛。因为 `LB` 不超过从 `(s,v)` 完成全部剩余组的真实额外代价，所以该剪枝不会丢失优于 `best` 的完整解。

一致性也沿用 Test6：对任意边 `(v,w)`，`group_dist[a][v] <= c(v,w)+group_dist[a][w]`，从而 `far` 一致；`near2/2` 一致；`mst(rem)/2` 与节点无关；取 `max` 后仍一致。因此 `key=dp[s][v]+LB(v,rem)` 可用于 Dijkstra 队列排序。

## 新增剪枝 1：root-star 初始上界

Test7 在 DP 开始前设置：

```text
best = min_v sum_{a=0}^{g-1} group_dist[a][v]
```

并记录为 `initial_upper`。

### 正确性证明

固定一个节点 `v`。对每个组 `a`，取一条从 `v` 到 `group_a` 最近候选点的最短路。所有这些路径都经过同一个 `v`，它们的并集是连通子图，并且覆盖所有组。

该并集的真实边权不超过路径长度之和：

```text
cost(union of paths) <= sum_a group_dist[a][v]
```

所以 `sum_a group_dist[a][v]` 是一个合法可行解的上界。对所有 `v` 取最小仍是合法上界。因此用它初始化 `best` 只会提前收紧已有 `dp+LB>best` 剪枝，不会把 `best` 设到最优值以下。

### 额外代价

`group_dist` 已经由 Test6 预处理得到。新增代价为：

```text
时间 O(n g)
空间 O(1)
```

它的价值在于让第一批小 mask 状态也能受到 `LB` 限制，避免在 `best=inf` 时进行大半径扩展。

## 新增剪枝 2：稀疏同根合并

Test6 在 `Modify(mask,v)` 中枚举所有：

```text
t subset (U ^ mask)
```

但其中绝大多数 `dp[t][v]` 为 `inf`，不会产生有效转移。Test7 为每个根 `v` 维护：

```text
root_masks[v] = 当前 dp[*][v] 已经 finite 的 mask 列表
```

`SetDp(mask,v,value)` 仅在 `dp[mask][v]` 首次从 `inf` 变为 finite 时，把 `mask` 追加到 `root_masks[v]`。因此没有线性查重成本。

`Modify(mask,v)` 的同根合并改为：

```text
先处理 t=0
再遍历进入 Modify 时 root_masks[v] 的快照
只对 (t & mask)==0 的 t 执行合并
```

统计项 `dense_up_subset_would` 记录如果按 Test6 稠密枚举会访问多少个子集；`total_up_subset` 记录 Test7 实际访问的 finite 候选。

### 正确性证明

Test6 的稠密枚举中，若 `dp[t][v]=inf`，则：

```text
dp[mask][v] + dp[t][v] = inf
```

它不可能改善任何 `dp[mask|t][v]`，也不可能更新 `best`。因此跳过所有非 finite 的 `t` 与原算法等价。

Test7 的 `root_masks[v]` 精确记录已经 finite 的 mask：每次 `dp[x][v]` 首次变为 finite 时追加，之后即使数值变小也不重复追加。因此遍历 `root_masks[v]` 不会漏掉任何可能有效的 finite `t`。

使用快照长度也是安全的。一次 `Modify(mask,v)` 过程中，新生成的状态形如 `mask|t`，它们都包含 `mask`。这些新 mask 不可能与当前 `mask` 不相交，因此不属于本次稠密枚举中合法的 `t subset (U^mask)`。所以本次不立刻用新生成的 mask 继续合并，不会漏掉原 Test6 在同一次枚举中应做的转移；它们会在后续对应状态被处理时参与合并。

因此，稀疏同根合并只是跳过无效的 `inf` 转移，不改变 DP 可达状态和最优值。

### 额外代价

新增结构：

```text
root_masks: 所有 finite (v,mask) 各记录一次
```

空间为 `O(number of finite root states)`。同根合并的时间从 Test6 的：

```text
sum Modify(mask,v) 2^{|U^mask|}
```

变为：

```text
sum Modify(mask,v) |{t in root_masks[v] : t & mask == 0}|
```

在实际数据中，同一根上 finite mask 通常远少于所有互补子集，因此该剪枝主要减少 `total_up_subset`。

## Dijkstra 扩展流程

对每个 `|mask|<=H` 的 mask：

1. 预计算所有点的 `lb[v]=LB(v,U^mask)`。
2. 对满足 `dp[mask][v]+h[U^mask][v]<=best` 且 `dp[mask][v]+lb[v]<=best` 的点作为目标入队。
3. 额外加入 `dp[mask][v]<mx_used` 的非目标点，保证目标最短路前缀可被扩展到。
4. 出队或松弛时若 `dp+lb>best`，直接跳过。
5. 目标点出队时调用 `Modify(mask,v)`，触发稀疏同根合并。

这部分与 Test6 的逻辑一致，只是 `best` 更早有上界，且 `Modify` 内部合并更稀疏。

## 统计项

Test7 输出额外统计：

```text
initial_upper       root-star 初始上界
dense_would         若按 Test6 稠密同根枚举会访问的子集数
up_subset           Test7 实际稀疏同根枚举次数
lb_seed             seed 阶段被 LB 挡掉的目标候选数
lb_relax            边松弛阶段被 LB 挡掉的候选数
```

这些统计用于判断两条新增剪枝的有效性，不参与正确性。

## 总体代价

相对 Test6，Test7 新增：

```text
root-star 上界: 时间 O(n g)，空间 O(1)
root_masks: 空间 O(finite states)
稀疏同根合并: 用 finite mask 枚举替代互补子集全枚举
```

没有新增 APSP，也不做图结构规约。两条新增剪枝都不改变状态定义，只减少无效搜索或提前提供安全上界。
