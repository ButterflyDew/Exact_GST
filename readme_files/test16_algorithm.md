# Test16：组斯坦纳树半集合稀疏 DP

本文档是 Test 系列的合并文档。旧的 Test11--Test15 代码和独立文档已经从仓库中删除；这里保留研究脉络、失败原因、当前 Test16 的算法细节和证明。

## 1. 问题、目标和符号

输入是无向非负边权图 `G=(V,E)`，点数 `n`，边数 `m`。一次查询给出 `g` 个组，每组包含若干候选点。目标是求最小代价连通子图，使每个组至少命中一个候选点。

记：

```text
U          全部组的 bitmask
H          floor(g/2)
D*(S,v)   覆盖 S 中所有组、并以 v 作为同根连接点的真实最优 rooted DP
best       当前完整可行解上界，单调不增
gd[a][v]   点 v 到第 a 个组的最短距离，即 D*({a},v)
```

论文目标是相对主 baseline PrunedDP 同时取得时间和空间上的数量级优势，并能在较大组数数据上运行。理论时间复杂度不能超过 PrunedDP 口径：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

按照 `agent.md`，实现中使用 `std::priority_queue`，理论核算时将堆操作视作与斐波那契堆同阶。

## 2. 为什么只需要求到 H 层

Test 系列的基本结构是“半集合 DP”：只显式维护 `|S|<=H` 的 rooted 状态，完整答案通过同根拼接得到。

关键引理是树的组数重心。对任意一棵可行答案树，先为每个组选择一个在树中的命中点；若一个点命中多个组，则该点带多个组标记。树上存在一个点 `r`，删除 `r` 后每个连通分量包含的组标记数都不超过 `g/2`。这是树重心的标准结论：若某个分量含有超过 `g/2` 个标记，就沿该分量方向移动；该过程严格减少最大侧标记数，最终停止。

以这个 `r` 为同根，答案树被分解成若干个从 `r` 出发的分支，每个分支覆盖的组数都不超过 `H`。这些分支可以合并成至多三个小块，每块大小仍不超过 `H`：

1. 初始每个分支是一个大小不超过 `H` 的块。
2. 若存在两个块大小之和不超过 `H`，就合并它们。
3. 若最终还剩至少四个块，按大小排序为 `x1<=x2<=x3<=x4<=...`。由于不能再合并，`x1+x2>H`；又有 `x3+x4>=x1+x2>H`。块大小是整数，所以前四块总大小至少 `2H+2`，但 `g` 只可能是 `2H` 或 `2H+1`，矛盾。

所以存在一个最优解可以写为同根 `r` 上的：

```text
A union B union C = U
|A|, |B|, |C| <= H
```

其中某些块可以为空。算法按 `|S|` 升序处理 mask，并在堆顶弹出 `(S,v)` 时在线枚举补集 `U-S` 的二分 `X,Y`，只要 `|X|,|Y|<=H` 就尝试：

```text
best = min(best, dp[S][v] + dp[X][v] + dp[Y][v]).
```

选择上述三块中按处理顺序最后完成的一块作为 `S`，其它两块已经可查，因此最优答案会被某一次弹出事件拼出。

## 3. Test 系列研究脉络

### Test11：稠密半集合 DP 和原始 h

Test11 建立了后续所有版本的基本框架：

- 预处理每个组到每个点的多源最短路 `gd[a][v]`；
- 用 root-star 和 greedy 给出初始 `best`；
- 只对 `|S|<=H` 做图上 Dijkstra；
- 同根合并生成更大的小 mask；
- 用 `Far/LB/h` 判断某个 rooted 状态是否值得入堆。

Test11 使用稠密二维表 `dp[mask][v]` 和 `h[mask][v]`，空间约为 `n * sum_{k<=H} C(g,k)`，这是后续必须削减的主要对象。

原始 h 的意图是：如果同根 `v` 上补集中已有某个可信子状态，则

```text
dp[S][v] + h(U-S,v) > best
```

说明当前结构不可能进入更优完整解，可以剪掉。这个方向后来被证明必须非常小心，不能把“当前算法算出的值”自动当成可信下界。

### Test12：诊断阶段

Test12 主要用于诊断 Dijkstra 前缀、target、promoted target 和统计字段。它帮助确认若只保留目标点而不保留必要前缀，可能丢失后续能改善超集或 h 的传播路径。Test12 的价值是输出账本，而不是最终算法结构。

### Test13：在线 h 与大状态删除

Test13 尝试删除稠密 h 表，改为在线查询：

```text
h(R,v)=max { dp[T][v] | T subset R, T 已确认 }
```

并尝试只定义 `|S|<=H` 的状态，在堆顶弹出时在线拼补集。这个阶段得到两个重要结论：

1. 只显式保存小状态是可行的，完整答案可以通过三块同根拼接恢复。
2. h 不能读取未经严格证明精确的 `dp` 值，否则会把偏大的上界误当成下界，产生错误剪枝。

固定反例中出现：

```text
DPBF                 55
Test13               56
Test13（关闭 h）      55
```

污染链是：

1. 某个小状态被 h 排除，没有执行后续 modified/liveup；
2. 某个超集缺少真实最优同根种子；
3. Dijkstra 在不完整源集合上弹出了偏大的值；
4. 该偏大值被标记为 confirmed；
5. 后续 h 把这个偏大值作为 witness，导致真正可能达到最优的状态被剪掉。

这说明 Dijkstra 弹出只能证明“相对于当前源集合最短”，不能证明“全局 rooted DP 精确”。因此 `target pop => exact` 是错误命题。

### Test14：稀疏 mask-major 布局

Test14 把稠密 DP 改成按 mask 存储的稀疏有序表：

```text
finite[S] = (root, value)
```

同根合并、h、补集拼接都先枚举合法 mask 组合，再对两条按 root 排序的表做 join。这一版证明了空间优化方向是对的：在 Toronto g10 query 5 上，历史峰值工作集约为：

```text
Test11    772.9 MiB
Test13    289.6 MiB
Test14     66.5 MiB
```

但 Test14 继承了 Test13 的 confirmed/h 语义，所以在 h 精确性闭包重新建立前不能视为正确算法。

### Test15：safe h 与 exact/dp 分离

Test15 严格区分：

```text
dp[S][v]     已构造出的可行上界，因此 dp[S][v] >= D*(S,v)
exact[S][v]  已证明等于 D*(S,v) 的值
```

只有 exact 能作为 h witness：

```text
h(R,v)=max { exact[T][v] | T subset R }.
```

因为 `T subset R` 时有 `D*(T,v)<=D*(R,v)`，所以该 h 是安全下界。Test15 还尝试了夹逼认证：

```text
LowerBound(S,v) <= D*(S,v) <= dp[S][v]
dp[S][v] <= LowerBound(S,v)+eps  => exact
```

以及实际覆盖 `cover` 升格、pair exact 证书等方案。正确性回归通过，但性能不理想：把 h/LB 放到边松弛热路径、全量 pair 预处理或过晚延迟 pair 都会造成负优化。最终结论是：safe h 的方向正确，但当前 exact witness 强度不足，naive online safe h 的收益小于查询代价。

### Test16：保留稀疏空间，收缩 liveup，默认禁用 h

Test16 融合了 Test14 的稀疏空间思路和 Test15 的安全性教训：

- 只保存 `|S|<=H` 的小状态；
- 不再保存或求解 `|S|>H` 的状态；
- 不物化 h，不让非 exact 值参与 h 剪枝；
- pair 层允许完整临时源以保持传播能力，但只把通过门控并弹出的状态写回稀疏行；
- 高阶同根合并只读取已保存的小状态；
- 每个弹出状态在线拼补集更新 `best`。

这使当前实现回到简单、安全、空间友好的主线。

## 4. 当前 Test16 状态语义

每个已处理的 `mask S` 保存一条按 root 升序的稀疏行：

```text
v[]      root
d[]      已构造出的 dp[S][root] 可行上界
cover[]  该可行树实际命中的组集合；当前主要作为低成本 provenance 保留
```

singleton 行不以稀疏方式丢点，而是直接用 `gd[a][v]` 查询：

```text
D*({a},v)=gd[a][v].
```

其它 mask 在本层处理时用临时数组：

```text
dist[v]  当前 S 在 root v 的候选值
cov[v]   候选树实际覆盖的组集合
lb[v]    当前补集 LowerBound 缓存
```

只有从堆中弹出并通过门控的 root 会写回 `state[S]`，参与后续同根合并和补集拼接。未写回的候选被视为在当前 `best` 下无用；由于 `best` 只会下降，之后也不会重新变得必要。

`total_valid/confirmed_states/certify_*` 目前是诊断统计，不作为 h 剪枝依据。也就是说，Test16 当前 correctness 不依赖 exact witness。

## 5. 预处理、上界和下界

### 5.1 组距离

对每个组执行一次多源 Dijkstra：

```text
gd[a][v] = dist(v, Group_a).
```

这既提供 singleton 精确 rooted DP，也提供下界。

### 5.2 初始上界

Test16 先计算 root-star：

```text
best = min_v sum_a gd[a][v].
```

再从 root-star 最优根执行组件增长 greedy。二者都只用于得到合法上界；上界越小，后续剪枝越强。

### 5.3 Far

```text
Far(v,R)=max_{a in R} gd[a][v].
```

任何从 `v` 出发覆盖 `R` 的树都至少要到达每个剩余组，因此 `Far(v,R)<=D*(R,v)`。为了让它能在同根合并和边松弛中作为 O(1) 热路径剪枝，Test16 把组位分成两半，为每个顶点和半掩码预存最远组编号。

### 5.4 LowerBound

`LowerBound(v,R)` 取两类标准安全下界的最大值：

```text
Far(v,R)
MST(R)/2 + nearest_two(v,R)/2
```

其中 `MST(R)` 是组间最短路度量闭包上的 MST 代价，`nearest_two` 是 `v` 到 R 中最近两个组的距离和；若 `|R|=1`，这一项退化为 `Far(v,R)`。

安全性证明如下。任取一棵从 `v` 覆盖 `R` 的 rooted 可行树，代价为 `C`，并在每个组中选一个被命中的代表点。把这棵树的边加倍得到一条从 `v` 出发、经过全部代表点并回到 `v` 的闭游走，代价 `2C`。在图的最短路度量中 shortcut 后，得到一条经过 `v` 和这些代表点的环，代价仍不超过 `2C`。删去环上与 `v` 相邻的两条边，剩余部分连接了所有代表组，因此代价至少为 `MST(R)`；这两条与 `v` 相邻的边的总代价又至少为 `v` 到最近两个组的距离和。因此：

```text
MST(R) + nearest_two(v,R) <= 2C
```

于是 `MST(R)/2 + nearest_two(v,R)/2 <= C`。再加上 `Far(v,R)<=C`，二者取最大仍是安全下界。

实现中 `LowerBound` 只在当前 mask 内按 root 缓存一次，避免边松弛热路径反复做 O(g) 扫描。

## 6. 同根合并与 liveup

处理 `S` 时，Test16 先用同根合并构造 Dijkstra 源。

### pair 层

对 `S={a,b}`，直接枚举全部 root：

```text
dist[v] = gd[a][v] + gd[b][v].
```

这是必要的。历史上尝试过只把 pair 写入 bucket 而不作为图搜索源，但随机小图会错：最优解可能需要 pair 源先沿图传播，再与其它子树同根合并。因此 pair 层保留完整临时源；区别在于这些源不会全部永久写入稀疏状态，只有弹出并通过门控的 root 会保存。

### 高阶层

对 `|S|>=3`，枚举无序二分：

```text
A union B = S
A intersect B = empty
```

若 `state[A]` 和 `state[B]` 都可用，就在相同 root 上 join：

```text
dist[v] = min(dist[v], dp[A][v] + dp[B][v]).
```

`JoinRows` 对两条 root 升序稀疏行自适应选择：

- 两行规模接近：双指针线性交；
- 一行显著更小：枚举小行并在大行二分。

二分只在估算比较次数低于线性扫描时使用，因此不改变 `O(3^g n)` 理论上界。

## 7. 图搜索和安全丢弃

对当前 `S` 和补集 `R=U-S`，初始源和边松弛都经过门控：

```text
d + Far(v,R) <= best
d + LowerBound(v,R) <= best
```

若某个候选在 root `v` 处不满足上述条件，则它与任何补集 rooted 树拼接的代价都不会优于 `best`。这类下界关于 root 是 1-Lipschitz 的：沿一条边 `(v,u)` 移动时，`Far` 最多下降 `w(v,u)`；`nearest_two/2` 最多下降 `w(v,u)`；`MST(R)/2` 与 root 无关。因此：

```text
LB(u,R) >= LB(v,R) - w(v,u)
```

一个已经在当前 root 被安全下界挡住的结构，沿边继续扩展后也不可能绕开该完整解下界产生更优答案。实际实现为控制常数，在边松弛时优先使用 O(1) 的 `Far` 和 `nd>=best`，只有弹出准备扩展时才计算 `LowerBound`。

注意剪枝使用的是安全下界，不使用 h。历史 h 反例已经说明，不能用未经证明精确的 `dp` 上界冒充补集下界。

## 8. 在线补集拼接

每当 `(S,v)` 从堆中弹出并通过门控，Test16 在线计算补侧：

```text
R = U ^ S
other = min dp[X][v] + dp[Y][v]
        where X union Y = R, X intersect Y = empty, |X|,|Y|<=H
best = min(best, dist[S][v] + other)
```

`Lookup(mask,v)` 的规则：

- `mask=0` 返回 0；
- singleton 直接返回 `gd[a][v]`；
- 其它小 mask 在稀疏行 `state[mask]` 中二分查 root；
- 未处理或已丢弃状态视为不可用。

这一步只更新完整答案上界，不生成新状态，也不作为 h witness。

## 9. 正确性证明摘要

证明分三层。

第一，所有保存和传播的 `dp` 值都是可行上界。singleton 来自到组最短路；同根合并是两棵同根可行树的并；边松弛是在图上把根移动一条边；因此不会产生低于真实可行代价的非法值。

第二，安全门控不会删除最优解所需的那条构造链。设最优完整解在某个同根 `v` 处分成当前侧 `S` 和补侧 `U-S`，当前侧在这棵最优树中的代价为 `d_opt`，补侧代价为 `c_opt`，于是 `d_opt+c_opt=OPT`。由于 `Far` 和 `LowerBound` 都不超过任意补侧可行树代价，特别是不超过 `c_opt`，且当前 `best` 是一个不小于 `OPT` 的上界，有：

```text
d_opt + Far(v,U-S)        <= OPT <= best
d_opt + LowerBound(v,U-S) <= OPT <= best
```

所以最优构造链上的候选不会被这些门控剪掉。其它较差的可行上界即使被剪掉，也不会影响最优性。

第三，最优答案一定会被某次在线补集拼接命中。由第 2 节的树重心和三块分解，引入同根 `r` 和三个小 mask `A,B,C`。算法按 mask 大小处理并保存所有未被安全剪掉的必要小状态；选择 `A,B,C` 中最后处理并弹出的那一块作为当前 `S`，其它两块已经可由 `Lookup` 读到，于是 `Complete` 会枚举到对应二分并把 `best` 更新到最优值。

因此，在非负边权和查询可行的前提下，Test16 不依赖 h 也能保持精确性。

## 10. h 的当前结论

安全 h 的唯一可接受语义是：

```text
h(R,v)=max { E[T,v] | T subset R, E[T,v]=D*(T,v) 已被证明精确 }.
```

此时：

```text
E[T,v]=D*(T,v)<=D*(R,v)
```

所以 h 是补集 rooted DP 的下界，`dp[S][v]+h(R,v)>best` 才是安全剪枝。

历史失败恰恰来自把 `Dhat(T,v)>=D*(T,v)` 的算法上界当成 `E[T,v]`。Test15 的 exact/dp 分离修复了正确性，但在当前数据上 exact witness 强度不足，online h 查询开销大于收益。Test16 因此默认禁用 h；`h_*` 统计字段仅保留为后续实验接口。

## 11. 时间复杂度

记 `M=sum_{k<=H} C(g,k)`。各模块复杂度如下：

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

合并后：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

其中同根合并和补集拼接都可以用三进制归属计数解释：对固定 root，每个组只会属于左集合、右集合或不参与当前二分，因此总组合数由 `3^g` 控制。

## 12. 空间复杂度

Test16 不再分配稠密 `dp[mask][v]` 或 `h[mask][v]`。持久空间主要是：

```text
gd                 O(gn)
Far 半掩码表        O(n 2^(g/2))
稀疏 state 行       O(F)，F 为实际保存的 finite 状态数
当前 mask scratch   O(n)
mask 元数据          O(2^g)
```

相对 Test11 的稠密 `n * M`，空间优势来自只保存后续仍可能有用并且通过门控的 root。

## 13. 统计字段如何读

Test16 的 stats 行包含：

```text
valid_total / inqueue
pull_pairs / pull_scan / pull_hits
complement_pairs / complement_scan / complement_hits
prune_ge_best / prune_far
active_seed / finite_states / confirmed_states
lb_calls / pq_push / pq_pop / relax_try / relax_ok
prep_ms / group_dist_ms / greedy_ms / pull_ms / complement_ms / search_ms / dp_ms
```

建议诊断顺序：

1. `group_dist_ms` 高：大图上的组最短路是瓶颈。
2. `pull_scan` 高：同根合并稀疏 join 是瓶颈。
3. `complement_scan` 高：在线补集拼接是瓶颈。
4. `relax_try` 高：图搜索区域过宽。
5. `pq_push/pq_pop` 高但 `relax_try` 不高：堆重复项或门控时机是瓶颈。
6. `finite_states` 高：稀疏状态本身膨胀，需要更强上界或安全剪枝。

## 14. 已验证结果和性能快照

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

Snapshot fast（3500 点子图，5 个 dataset version，g=9..12，各 1 条）：
  total_query_time_sec=98.388
  wall_seconds=114.749
  timeout=0
```

这些结果说明 Test16 目前是“正确性优先、空间显著下降、时间不回退”的稳定版本。后续如果继续研究 h，应从 exact witness 强度和低成本认证入手，而不能恢复 Test13 式的非精确 h。
