# Test16：半集合稀疏 DP 稳定版

Test16 是当前 Test 系列的稳定基线。它吸收 Test14 的稀疏空间布局，并保留 Test15 后得到的正确性约束：不使用未经 exact 证明的 h 剪枝。

共同背景和历史结论见 `test_series_overview.md`。本文只描述 Test16 本身。

## 1. 核心思路

Test16 只显式保存 `|S|<=H=floor(g/2)` 的 rooted 状态。完整答案不再通过求解大 mask 得到，而是在小状态弹出时在线拼接：

```text
best = min(best, dp[S][v] + dp[X][v] + dp[Y][v])
where X union Y = U-S, X intersect Y = empty, |X|,|Y|<=H
```

这样既避免大 mask 的状态膨胀，也保留由三块分解保证的最优性。

## 2. 数据结构

对每个非空小 mask `S`，Test16 保存一条按 root 升序的稀疏行：

```text
state[S].v[]      root 编号
state[S].d[]      dp[S][root]，可行上界
state[S].cover[]  该可行树实际命中的组集合
```

`cover` 主要作为低成本 provenance 保留；Test16 中不依赖它做剪枝。

处理当前 mask 时使用临时数组：

```text
dist[v]  当前 S 在 root v 的候选值
cov[v]   候选树实际覆盖的组集合
lb[v]    LowerBound(v,U-S) 的当前 mask 缓存
```

只有从堆中弹出并通过门控的 root 会写回稀疏行，参与后续同根合并和补集拼接。

## 3. 预处理

### 3.1 组距离

对每个组执行一次多源 Dijkstra：

```text
gd[a][v] = dist(v, Group_a)
```

它同时提供 singleton 精确值：

```text
D*({a},v)=gd[a][v]
```

### 3.2 初始上界

先计算 root-star：

```text
best = min_v sum_a gd[a][v]
```

再从 root-star 最优根执行组件增长 greedy。二者都只用于得到合法完整解上界；`best` 越小，后续安全门控越强。

### 3.3 Far 与 LowerBound

```text
Far(v,R)=max_{a in R} gd[a][v]
```

任何从 `v` 出发覆盖 `R` 的树都至少要到达每个剩余组，因此 `Far(v,R)<=D*(R,v)`。

`LowerBound(v,R)` 取两类安全下界的最大值：

```text
Far(v,R)
MST(R)/2 + nearest_two(v,R)/2
```

其中 `MST(R)` 是组间最短路度量闭包上的 MST 代价，`nearest_two(v,R)` 是 `v` 到 `R` 中最近两个组的距离和；当 `|R|=1` 时退化为 `Far`。

安全性来自标准加倍游走论证。任取一棵从 `v` 覆盖 `R` 的树，代价为 `C`。把边加倍得到经过 `v` 和所有代表组的闭游走，代价 `2C`；shortcut 后仍不超过 `2C`。删去环上与 `v` 相邻的两条边，剩余部分连接所有代表组，代价至少 `MST(R)`；这两条边总代价至少 `nearest_two(v,R)`。所以：

```text
MST(R) + nearest_two(v,R) <= 2C
```

于是该表达式的一半是安全下界。

## 4. 同根合并

处理 `S` 时，先构造 Dijkstra 源。

### pair 层

若 `S={a,b}`，枚举全部 root：

```text
dist[v] = gd[a][v] + gd[b][v]
```

pair 层必须保留完整临时源。历史实验表明，只把 pair 写入 bucket 而不作为图搜索源会错：最优结构可能需要 pair 源先沿图传播，再和其它子树同根合并。

### 高阶层

若 `|S|>=3`，枚举无序二分：

```text
A union B = S
A intersect B = empty
```

在相同 root 上 join：

```text
dist[v] = min(dist[v], dp[A][v] + dp[B][v])
```

两条稀疏行均按 root 排序。实现根据规模选择双指针线性交或“小行枚举 + 大行二分”，只在估算比较次数更少时使用二分，不改变理论上界。

## 5. 图搜索和门控

当前 mask 的补集记为 `R=U-S`。候选源和边松弛都必须满足：

```text
d + Far(v,R) <= best
d + LowerBound(v,R) <= best
```

若不满足，当前侧结构与任何补侧可行树拼接都不可能优于 `best`，可以安全丢弃。

这个判断不会剪掉最优构造链。设最优解在同根 `v` 处分成当前侧代价 `d_opt` 和补侧代价 `c_opt`，则 `d_opt+c_opt=OPT`。由于 `Far` 和 `LowerBound` 都不超过补侧真实代价：

```text
d_opt + Far(v,R)        <= OPT <= best
d_opt + LowerBound(v,R) <= OPT <= best
```

因此最优链上的候选不会被门控挡掉。

实现上，为控制常数，边松弛热路径优先用 `nd>=best` 和 O(1) 的 `Far`；`LowerBound` 在弹出准备扩展时按 root 缓存。

## 6. 在线补集拼接

每当 `(S,v)` 从堆中弹出并通过门控，Test16 枚举补集二分：

```text
R = U ^ S
X union Y = R
X intersect Y = empty
|X|, |Y| <= H
```

然后查询：

```text
other = min dp[X][v] + dp[Y][v]
best = min(best, dist[S][v] + other)
```

`Lookup(mask,v)` 规则：

```text
mask=0       返回 0
singleton    返回 gd[a][v]
其它小 mask  在 state[mask] 中二分查 root
不可用状态    返回 INF
```

这一步只更新合法完整上界，不生成新状态，也不提供 h witness。

## 7. 正确性摘要

第一，所有 `dp` 都是可行上界。singleton 来自最短路；同根合并是两棵同根树取并；边松弛是在图上把根移动一条边。

第二，门控只使用补集的安全下界。由第 5 节证明，最优构造链不会被剪掉。

第三，最优答案一定会被某次补集拼接命中。由三块分解，存在同根 `r` 和三个小块 `A,B,C` 覆盖全集。取三块中最后处理并弹出的那块为当前 `S`，其它两块已可由 `Lookup` 查询，`Complete` 会枚举到对应二分并更新到最优值。

因此 Test16 在非负边权下是精确算法，且不依赖 h。

## 8. 复杂度

记 `M=sum_{k<=H} C(g,k)`。主要模块为：

```text
组距离预处理             O(g(m+n log n))
root-star + greedy        O(gn + g(m+n log n))
Far 半掩码表              O(n(2^floor(g/2)+2^ceil(g/2)))
LowerBound 缓存           O(2^g g n)
同根合并 pull             O(3^g n)
在线补集拼接              O(3^g n)
分层 Dijkstra             O(2^g(m+n log n))
排序/稀疏行冻结           O(2^g n log n)
```

合并后仍为：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

同根合并和补集拼接都由三进制归属计数控制：对固定 root，每个组属于左块、右块或不参与当前二分。

## 9. 空间

Test16 不分配稠密 `dp[mask][v]` 或 `h[mask][v]`。持久空间主要是：

```text
gd                 O(gn)
Far 半掩码表        O(n 2^(g/2))
稀疏 state 行       O(F)，F 为实际保存的 finite 状态数
当前 mask scratch   O(n)
mask 元数据          O(2^g)
```

空间优势来自只保存通过门控、后续仍可能有用的 root。

## 10. 统计字段

常用字段：

```text
valid_total / inqueue
pull_pairs / pull_scan / pull_hits
complement_pairs / complement_scan / complement_hits
prune_ge_best / prune_far
active_seed / finite_states / confirmed_states
lb_calls / pq_push / pq_pop / relax_try / relax_ok
prep_ms / group_dist_ms / greedy_ms / pull_ms / complement_ms / search_ms / dp_ms
```

诊断顺序建议：

1. `group_dist_ms` 高：组最短路是瓶颈。
2. `pull_scan` 高：同根合并稀疏 join 是瓶颈。
3. `complement_scan` 高：补集拼接是瓶颈。
4. `relax_try` 高：图搜索区域过宽。
5. `finite_states` 高：需要更强上界或安全状态削减。

## 11. 已验证结果

正确性：

```text
随机小图黑盒对拍：
  seed=271828   100 组通过
  seed=424242   100 组通过
  seed=20260628 100 组通过

Toronto default 最后一轮 160 条与已有 DPBF 结果逐条一致：
  bad=0
  max_diff=0

Toronto query_g10 40 条与已有 Test15 结果逐条一致：
  bad=0
  max_diff=0
```

性能快照：

```text
Toronto query_g10 40 条：
  Test16：25.14s，peak 56.02 MiB
  Test15：53.45s

Toronto default 160 条：
  Test16：16.85s，peak 31.66 MiB

Snapshot fast：
  total_query_time_sec=98.388
  wall_seconds=114.749
  timeout=0
```

结论：Test16 是正确性清楚、空间显著下降、时间不回退的稳定基线。
