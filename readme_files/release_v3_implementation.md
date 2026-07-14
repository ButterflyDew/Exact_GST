# ReleaseV3 实现导读：主流程与方法切换

> ReleaseV3 现为冻结的框架 B 对照。当前纯 A 发行实现及其单一路径导读见
> `release_v4.md`；本文只解释 ReleaseV3 自身，不代表当前推荐入口。

本文解释 `methods/Release/release_v3.cpp` 的实际执行路径。阅读重点不是类和字段本身，而是：

1. 一个 query 先做什么、后做什么；
2. ordered half rows 何时继续，何时直接完成；
3. 何时从 half-row 方法切换到 global-label 方法；
4. 切换时保留什么、释放什么，为什么仍然精确。

性能数字统一维护在 `release_v3.md`。本文只讲源码结构、算法语义和 review 边界。

## 1. 先看完整流程

### 1.1 三条执行路径

ReleaseV3 只有一个公开入口：

```cpp
SolveResult SolveOneQuery(const Graph& graph, const Query& query);
```

进入后只可能走以下三条路径：

```text
空 query
  -> 返回 0

g <= 3
  -> GoalRootStar
  -> 精确返回

g >= 4
  -> 公共预处理
  -> Ordered Half Rows
       |
       |-- 所有 half rows 已按顺序完成
       |     -> half-row 完备性已经得到精确答案
       |     -> 直接返回
       |
       `-- row_work 足以支付 strong search
             -> 构造 directed-cut dual
             -> 释放全部 half-row 状态和工作区
             -> 选择 farthest-goal anchor
             -> Anchored Global Labels
             -> 精确返回
```

这里没有“根据 `g` 选择 ReleaseV1 或 ReleaseV2”。small、half-row 和 global 都是 `release_v3.cpp` 内部的具名阶段。

### 1.2 一般路径的时间线

对 `g>=4`，对象按以下顺序创建和使用：

| 顺序 | 阶段 | 主要产物 |
| ---: | --- | --- |
| 1 | query feasibility | query 是否可能连通 |
| 2 | group distances | `gd[a][v]` |
| 3 | root-star upper | 初始 `best` 和根 `root` |
| 4 | group metric + TSP/2 | row/global 共用 future lower |
| 5 | ordered half rows | 精确 `D(S,v)` rows、completion、`row_work` |
| 6a | rows 自己完成 | 直接返回精确 `best` |
| 6b | global 购买事件 | 构造 dual，释放 rows |
| 7 | anchored global | 从 singleton labels 独立启动并完成 |

接下来先完整解释第 6b 步，因为这是源码中最容易看不清的部分。

## 2. Half Rows 如何切换到 Global Labels

### 2.1 切换不是经验阈值

row 阶段累计实际执行过的理论操作：

```text
row_work =
    row 交集或 dense 扫描
  + 图邻接松弛
  + complement lookup 或 cache build
  + (priority_queue pushes + pops) * max(1, ceil(log2 n))
```

代码同时计算两个购买成本：

```text
greedy_buy_work = g  * (m + n * max(1, ceil(log2 n)))
global_buy_work = 2g * (m + n * max(1, ceil(log2 n)))
```

它们来自图搜索工作量，不读取数据集名、固定 `g`、mask 层级、row density、运行秒数或完成百分比。

### 2.2 Greedy 事件不是换方法

第一次满足：

```text
row_work >= greedy_buy_work
```

代码只做三件事：

1. 从 root-star 根运行一次 `GreedyUpper`；
2. 记录 greedy 新命中的候选根，供真正切换前再尝试；
3. 如果 `best` 下降，用新上界调用 `CompactRows`。

此后仍继续 ordered half rows。greedy 只是购买一个更强可行上界，不是第三套 solver，也不会改变 DP 状态。

### 2.3 真正的切换条件

每个 mask 完成、保存并计入工作量后，代码检查：

```cpp
if (!dual_ready && stats.row_work >= stats.dual_cut_build_work)
```

其中 `dual_cut_build_work` 就是上面的 `global_buy_work`。条件第一次成立时依次执行：

```text
BuildDual()
CompactRows()
FinishWithGlobal(current_size, masks_in_current_size)
return result
```

切换只发生一次，global 结束后不会回到 rows。

### 2.4 `BuildDual` 做什么

`BuildDual` 使用已经存在的 graph、query、`gd` 和 root-star 根：

1. 构造 `DualCutPotential`；
2. 保存可查询的 subset future potential；
3. 用 dual 的真实 primal tree 更新 `best`；
4. 标记 `dual_ready=true`。

从此刻到释放 rows 前，`FutureBound(v,R)` 从单独 TSP/2 变成：

```text
max(TSP/2(v,R), dual(v,R)).
```

紧接着执行一次 `CompactRows`，删除在更强下界和新 `best` 下已不可能改进答案的保存状态。

### 2.5 `FinishWithGlobal` 是真正边界

`FinishWithGlobal` 的顺序是：

1. 记录切换发生的 mask size 和该 size 内序号；
2. 结束 row 阶段计时；
3. 释放 row 专用 scratch；
4. 释放 `rows` 本身；
5. 对 greedy 收集到的候选根再运行确定性 greedy upper；
6. 调用 `ContinueAnchoredGlobal`；
7. 写入最终答案和总计时。

释放的对象包括：

```text
distance
heuristic / heuristic_stamp
touched / settled
complement_pairs / complement_row
rows
```

代码使用 empty-vector swap 归还容量，而不只是 `clear()`。这样 global 大 frontier 启动前，half-row payload 不再占据峰值内存。

### 2.6 切换时究竟传递了什么

保留并传给 global 的是公共 query 信息：

| 保留对象 | global 用途 |
| --- | --- |
| graph / query | 边传播与 terminal 初始化 |
| `gd[a][v]` | anchor、star upper、future lower |
| group metric | group-MST-half |
| TSP/2 endpoints | 强 future lower |
| directed-cut dual | 强 future lower |
| terminal color | 判断 root 命中哪些组 |
| root-star 根 | 选择 farthest anchor |
| 当前 `best` | 停止和 pruning |

不传递的是任何 `D(S,v)` row 或未完成 row scratch。

这不是丢弃必要状态。global recurrence 从所有非 anchor 组的 singleton terminals 重新、完整地启动；half rows 只负责在切换前争取直接完成或改善上界，不承担切换后正确性。

### 2.7 为什么不把 rows 转成 global labels

历史探针试过批量 row seed 和预结算 transfer，但没有减少净处理状态，并增加了峰值或 wall。发行实现因此选择清晰边界：

```text
共享预处理和上界
不共享两种状态表示
```

这也让正确性更容易 review：half-row 和 global 各自完整，切换只决定何时停止前者并开始后者。

## 3. 入口函数 `SolveOneQuery`

### 3.1 输入检查

入口首先填入 `n/m/g`：

- `g=0`：返回权重 0；
- `g>20`：抛出明确错误，避免位掩码溢出；
- `IsQueryFeasible=false`：返回 infeasible；
- `g<=3`：进入 `GoalRootStar`；
- 其余 query：进入一般路径。

`SolveResult` 包含：

```text
best_weight   精确答案
feasible      query 是否可行
stats         统一 main 实际输出的必要统计
```

### 3.2 一般路径共享状态

一般路径先创建：

```text
subset_count = 1 << g
full_mask    = subset_count - 1
half         = floor(g/2)
best         = 当前合法完整树上界
```

`best` 只由真实完整树更新，可以偏大，不能偏小。TSP/2 和 dual 只作为下界，不能直接写入答案。

## 4. 公共预处理

### 4.1 `GroupDistances`

每个组执行一次多源 Dijkstra，组内全部候选点以距离 0 入队：

```text
gd[a][v] = 图点 v 到组 a 的最短距离。
```

这张 `g*n` 表被 root-star、rows、global 和 dual 共同使用，不会在切换时重算。

### 4.2 Terminal color

`color[v]` 是点 `v` 所属组的 bit mask。一个点可以同时属于多个组。

### 4.3 `RootStarUpper`

扫描所有图点：

```text
star(v) = sum_a gd[a][v].
```

各组到 `v` 的最短路之并是一棵合法连接结构，所以最小 star 是安全上界。函数同时返回最优根：

- half 阶段用它启动 greedy；
- dual 用它作根；
- global 用它选择 farthest-goal anchor。

### 4.4 Group metric

```text
gp[a][b] = min_{v in group b} gd[a][v].
```

它只描述组间最短距离，用于 TSP/2 和 group-MST-half，不替代原图。

### 4.5 `TourLowerBound`

临时 DP：

```text
paths[mask,start,last]
  = 从 start 组出发，访问 mask，结束于 last 组的最短 group-metric path。
```

Build 完成后只保留每个 mask 的 endpoint triples：

```text
(left, right, path_cost).
```

大型 `paths_` 工作表立即释放。查询为：

```text
R empty       -> 0
R singleton   -> gd[a][v]
otherwise     -> 1/2 * min(gd[a][v] + path[R,a,b] + gd[b][v])
```

任意 future tree 翻倍后给出访问各组的闭合游走，因此 TSP/2 是安全下界，并满足 edge consistency 与 subset splice。

## 5. Small Path：`GoalRootStar`

对至多三个组：

```text
OPT = min_v sum_a gd[a][v].
```

原因是最优树中至多三个实际命中点存在树中位点；中位点到这些点的树路总长等于树长。反向从任意 `v` 取到各组最短路的并，又给出合法上界。

实现没有先物化完整 `g*n` 表，而是并行推进各组多源 Dijkstra：

| 状态 | 含义 |
| --- | --- |
| `distance[a]` / `frontier[a]` | 第 a 组的 Dijkstra |
| `settled_mask[v]` | 在 v 已定型的组 |
| `settled_sum[v]` | 已定型距离之和 |
| `base_by_mask` | 相同 settled mask 下最小 settled sum |

未定型组至少还需支付各自 frontier minimum，因此全局停止下界是：

```text
min_mask(
    min settled_sum(mask)
  + sum frontier_min[group outside mask]
).
```

它达到当前完整 root 值时即可精确停止。这个路径结束后不会进入 half rows 或 global。

## 6. General Path A：Ordered Half Rows

### 6.1 状态

```text
D(S,v) = 覆盖组集合 S、以图点 v 为根的最优 rooted tree cost。
```

只生成：

```text
1 <= |S| <= floor(g/2).
```

singleton 不创建 `Row`，直接读取 `gd`。非 singleton 才物化为 sparse 或 dense row。

### 6.2 Mask 顺序

所有 half masks 按：

```text
(|S|, mask)
```

递增处理。因此构造当前 row 时，需要的真子集 row 已经完成。

`needed_later` 预先判断 half-size row 是否还会被后续 recurrence 或二/三块 completion 读取。不再需要的 row 仍完成当前搜索和 completion，但不落盘。

### 6.3 一个 mask 如何完成

单个 mask 的生命周期是：

```text
枚举同根子集划分并产生 seeds
  -> 用 H(v,U-S) 过滤不可能改进 best 的 seed
  -> priority queue 做图闭包
  -> 每次有效弹出先尝试完整答案 completion
  -> 沿边松弛
  -> 收集精确定型 roots
  -> 再按 distance+H<=best 过滤
  -> 若 needed_later，保存为 sparse/dense Row
  -> 计入 row_work
  -> 检查 greedy/global 购买事件
```

size 2 直接扫描两个 singleton 距离之和。更大 mask 枚举无序真子集划分 `left<right`，通过统一的 `ForEachPairSum` 生成同根 seed。

### 6.4 A* 式图闭包

对当前 `S`：

```text
H(v) = FutureBound(v, U-S)
key  = distance(v) + H(v).
```

TSP/2，以及 dual 构造后的 `max(TSP/2,dual)`，满足 edge consistency。因此队列弹出的有效 distance 是当前 row 的精确 rooted value。

`touched` 只记录本 mask 实际访问的点，使结束清理不必扫描整个 `distance` 数组。

### 6.5 二/三块同根完成

half-row 完备性来自最优树 centroid。删除 centroid 后，各分量包含的选中组代表点不超过 `floor(g/2)`，并可合并为至多三个仍不超过 half 的块。

因此某个最优解可写成：

```text
OPT = D(A,v) + D(B,v) [+ D(C,v)]
```

其中块互不相交且并为全部组。

处理当前 `S` 时，代码把 `U-S` 枚举成至多两个 available blocks。只有 `3|S|>=g` 时三块才可能覆盖全部组，因此更早层无需做无效 completion。

### 6.6 Complement cache

对每个 settled root 直接查补侧 rows 是 rent；一次物化：

```text
complement_row[v] = min D(left,v)+D(right,v)
```

是 buy。代码比较累计 lookup 工作与一次构建工作，在下一次 rent 将达到 build cost 时才构建。

cache 只活在当前 mask，seed 和 cache 都调用同一个 `ForEachPairSum`，避免 singleton/dense/sparse 语义分叉。

### 6.7 Row 表示与 compact

`Row` 字段：

```text
vertices    sparse root ids，严格递增
distances   sparse payload 或 dense[n+1]
count       有限状态数
ready       后续 mask 是否可读取
dense       当前表示类型
```

保存时比较真实字节：

```text
sparse = count * (sizeof(int)+sizeof(double))
dense  = (n+1) * sizeof(double)
```

只选较小者，不使用 density 阈值。

`CompactRows` 删除：

```text
D(S,v) + FutureBound(v,U-S) > best
```

的保存状态。它只删不可能改进答案的 root，不重算或近似 DP 真值。

### 6.8 Pair partition upper

pair rows 完成后，代码在 root-star 根和候选数最少组的所有顶点上尝试 singleton/pair 分块。所有 block 共享同一 root，它们的树取并是真实 GST，因此只用于安全降低 `best`。

### 6.9 Rows 如何直接结束

如果所有 ordered masks 在 global 购买事件前处理完，centroid 二/三块 completion 已保证最优答案被某次命中。此时：

- 不构造 dual；
- 不启动 global；
- 直接返回 `best`。

这就是小型一般 query 不支付重预处理的路径。

## 7. General Path B：Anchored Global Labels

### 7.1 选择 anchor

`ContinueAnchoredGlobal` 选择：

```text
anchor = argmax_a gd[a][root_star_root].
```

比较使用严格 `>`，平局保留编号最小组。任意可行解都必须命中 anchor，所以可把该组作为 permanent goal，从显式 mask 中移除。

```text
label_full = full_mask ^ anchor_bit
```

farthest 规则只改变等价状态表示，不试跑多个 anchor。

### 7.2 Label 语义

`GlobalLabel(root,mask)` 表示：

- 当前 partial tree 连接在 `root`；
- 已覆盖 `mask` 中的非 anchor 组；
- `cost` 是当前最小 tentative rooted cost；
- `lower` 是该 label 首次创建时计算并缓存的 future lower；
- `settled` 表示已经定型。

anchor 不在显式 mask 中，但始终位于：

```text
remaining = full_mask ^ mask
```

直到 full non-anchor label 到达一个命中 anchor 的图点。

### 7.3 初始化

除 anchor 外，每个组的全部候选点初始化 singleton label，cost 为 0。global 不读取任何 half row。

### 7.4 `Relax` 的固定顺序

`Relax(root,mask,cost)` 依次执行：

1. 拒绝 empty mask 或包含 anchor 的 mask；
2. 用 cheap prehash lower 在访问 hash map 前剪枝；
3. 查找旧 label，拒绝 settled 或不改善 cost 的候选；
4. 对新 label 用 star extension 产生真实完整树上界；
5. 计算并缓存 `max(cheap,TSP/2,dual)`；
6. 再次检查 `cost+lower<best`；
7. 插入或降低 label，并压入 lazy heap。

heap 节点只保存 `key/root/mask`。弹出时回查 label，用：

```text
node.key == label.cost + label.lower
```

识别 stale item。

### 7.5 Global lower 与 upper

对 remaining groups，一次扫描得到：

```text
star_extension = sum gd[a][root]                  // 可行上界
farthest       = max gd[a][root]
near1, near2   = 两个最小 root-group distance
cheap lower    = max(
                    farthest,
                    MST(remaining)/2 + (near1+near2)/2
                 )
```

访问 label map 前只使用更便宜的：

```text
max(MST(remaining)/2, gd[first remaining group][root]).
```

新 label 的最终 lower 是：

```text
max(cheap, TSP/2, directed-cut dual).
```

### 7.6 主循环

heap minimum 满足 `key+kEps>=best` 时停止，因为 `best` 已是可行完整树。

一个 label 定型后：

1. 用 star extension 更新上界；
2. 若 mask 覆盖全部非 anchor 组且 root 命中 anchor，用当前 cost 完成答案；
3. 插入 root-local disjoint index；
4. 沿原图边传播相同 mask；
5. 与同 root 已 settled 的不交 masks 合并。

### 7.7 两种不交集合并

代码比较：

```text
submask_work = 2^|available| - 1
bitmap_work  = word_count * |current mask|
```

- submask 较小：枚举 available 的所有非空子集并查 root-local map；
- bitmap 较小：用 `GlobalDisjointIndex` 排除含冲突 bit 的 settled labels。

两条路径都完整枚举全部已 settled 不交伙伴，只按输入规模选择较少理论工作，不是 density 特判。

## 8. 内部索引与基础 helper

### 8.1 `GlobalLabelMap`

每个 root 一个 open-addressing map：

- key 是非零 mask；
- 容量保持为 2 的幂；
- 负载超过 1/2 时扩容；
- mask 0 是空槽哨兵。

global labels 在线到达，必须按 `(root,mask)` 查找。它与 half rows 不同：half rows 按 mask 数组和有序 root 序列处理，不使用动态 row hash。

### 8.2 `GlobalDisjointIndex`

每个 root 保存 settled `(mask,cost)` 序列。每 64 个 labels 为一个 word：

```text
contains[word*g+bit]
```

指出该 word 中哪些 labels 含 group bit。把当前 mask 的对应 bitmaps 取并得到 blocked labels，取反后得到不交候选。

### 8.3 Row 遍历和求和

`ForEachValue` 统一遍历：

- singleton `gd`；
- dense row；
- sparse row。

`ForEachPairSum` 统一处理：

- empty side；
- singleton + singleton；
- singleton + row；
- dense + dense；
- dense + sparse；
- sparse + sparse。

sparse + sparse 在 linear merge、从较小侧 binary search 两种方向中按 `JoinCost` 选择较少工作。

### 8.4 Bit helper

`FirstBit` / `FirstBit64` 在 MSVC 使用 `_BitScanForward`，其他编译器使用 `ctz`。所有调用点都保证输入非零。

## 9. 上下界各自能做什么

| 组件 | 类型 | 能否更新 `best` | 能否剪枝 |
| --- | --- | ---: | ---: |
| root-star | 可行完整树 | 是 | 间接 |
| greedy | 可行完整树 | 是 | 间接 |
| pair partition | 可行完整树 | 是 | 间接 |
| dual primal | 可行完整树 | 是 | 间接 |
| star completion | 可行完整树 | 是 | 间接 |
| TSP/2 | future lower | 否 | 是 |
| group-MST-half / farthest | future lower | 否 | 是 |
| directed-cut potential | future lower | 否 | 是 |

核心规则是：上界可以降低 `best`，下界只能证明某个 partial state 无法优于 `best`。源码没有把二者混用。

## 10. 内存生命周期

| 对象 | 创建 | 最后使用/释放 |
| --- | --- | --- |
| group distances | 公共预处理 | query 返回 |
| TSP `paths_` | `TourLowerBound::Build` | Build 内释放，只留 endpoints |
| half rows | row phase | rows 完成返回，或 global 切换前释放 |
| row scratch | row phase | global 切换前释放 |
| dual residual | `DualCutPotential::Build` | Build 内 primal 恢复后释放 |
| dual potentials | global 购买事件 | query 返回 |
| global maps/indexes/heap | global phase | global 返回 |

切换处同时存在的是公共预处理、dual potential 和当前 `best`，不会同时保留 half rows 与完整 global frontier。

## 11. `ReleaseStats`

| 字段 | 含义 |
| --- | --- |
| `n/m/g` | 图和 query 规模 |
| `distance_bytes` | 本 query 已物化 row payload 的累计字节，不是瞬时 RSS |
| `row_work` | row 阶段累计理论工作 |
| `dual_cut_build_work` | global 购买成本 |
| `global_switch_*` | 切换发生的 size 和 size 内 mask 序号 |
| `global_anchor_*` | 1-based anchor 组和 root-star 距离 |
| `global_used` | 是否真正进入 global |
| `global_*labels` | created、settled、peak open |
| `*_ms` | group distance、TSP、upper、rows、dual、global、total；`rows_ms` 包含切换前执行的 `dual_ms` |

完整进程 RSS 由统一 main 的 `memory_usage` 记录，不在 solver 中重复采样。

需要做互斥 A/B 时间拆分时使用：

```text
shared = total_ms - rows_ms - global_ms
A-side = rows_ms - dual_ms
B-side = dual_ms + global_ms.
```

实测比例统一见 `release_v3.md` 第 9.3 节，避免在实现导读中复制数据表。

## 12. 正确性闭环

ReleaseV3 的 exact 性可以按执行路径分别检查：

1. `g<=3`：goal-root-star 等式直接给出 OPT。
2. rows 直接完成：Dreyfus-Wagner 同根 merge + 图闭包给出精确 `D(S,v)`；centroid 证明保证二/三块 completion 命中最优解。
3. rows 切换 global：切换前所有 `best` 都是真实完整树；global 从 singleton labels 独立完整启动，不依赖已释放 rows。
4. global：边传播和同根不交 merge 枚举完整 anchored recurrence；任意可行树都命中 anchor。
5. pruning：TSP/2、MST/farthest 和 dual 都是 admissible，并满足所需 consistency/splice 条件。
6. rent-or-buy：只改变何时构造等价 cache、greedy、dual/global，不改变状态定义和答案条件。

## 13. 复杂度

```text
time  O(3^g n + 2^g((g+log n)n+m))
space O(2^g n + 2^g g^2 + gn+m).
```

- half-row subset partitions 总计受 `3^g` 控制；
- 每个 row 至多一次图闭包；
- anchored global 至多 `2^(g-1)n` labels；
- global 不交 joins 受 `3^(g-1)n` 控制；
- TSP endpoints、MST 和 mask tables 属于 `2^g poly(g)`；
- 具体实现使用 `std::priority_queue`，理论式按项目要求使用对应 Fibonacci-heap 口径。

## 14. 建议的源码阅读顺序

不要从第一个 struct 顺序向下硬读。建议按控制流：

1. `SolveOneQuery`：只看输入分流、预处理、mask 主循环和两个购买事件；
2. `FinishWithGlobal` 局部函数：确认真正切换边界；
3. `ContinueAnchoredGlobal`：看 anchor、`Relax`、主循环；
4. 回到 row 主循环：看 seed、completion、保存和 `row_work`；
5. `GoalRootStar`：单独核对 small path；
6. 最后看 `Row`、`GlobalLabelMap`、`GlobalDisjointIndex` 和 join helpers。

这样先确认“运行哪条方法”，再核对每条方法内部的数据表示。

## 15. Review 结论与清单

`release_v3.cpp` 约 1390 行，不是小文件，但它包含 small exact、公共 lower bounds、half rows 和 global continuation 四段完整算法。当前发行源码具备以下 review 边界：

- 一个公开入口，一条自动执行路径；
- 没有 mode、实验开关、调试输出；
- 没有数据集、固定 `g`、层级、density 或 wall-time 特判；
- seed 与 complement 共享 pair-sum 实现；
- row/global 状态不混存；
- 公开统计都由统一 main 输出；
- 重型临时对象有明确最后使用点。

修改时应检查：

1. 新 `best` 是否来自真实完整树；
2. 新 pruning 是否只依赖合法 lower；
3. row 是否只读已完成真子集；
4. sparse roots 是否保持递增；
5. direct/cached complement 是否仍共享同一语义；
6. global lower 是否保持 edge consistency 和 subset splice；
7. 切换是否仍先释放 row-only storage；
8. 决策是否仍按理论工作而非 wall time；
9. Release/O2 随机对拍后是否清理临时目录；
10. 纯重构是否避免重复 full DBLP 长跑。

## 16. 文献来源

| 机制 | 来源 | ReleaseV3 中的关系 |
| --- | --- | --- |
| rooted subset recurrence | [Dreyfus and Wagner, *The Steiner Problem in Graphs*](https://doi.org/10.1002/net.3230010302) | GST group 适配、half-row 调度和二/三块完成 |
| goal-directed Dijkstra-Steiner / TSP future cost | [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) | consistent future bound 与 label-setting 接口 |
| directed-cut dual ascent | [Wong, *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*](https://doi.org/10.1007/BF02612335) | dual-ascent 理论来源；group sink、subset splice 和 primal 恢复为仓库适配 |
| GST baseline | [Efficient and Progressive Group Steiner Tree Search](https://doi.org/10.1145/2882903.2915217) | PrunedDP 对照；不把 baseline 可共享的普通压缩计作本方法收益 |

dual 的历史证明见 `history/dual_cut_potential.md`；算法效果、数据集统计和复现命令见 `release_v3.md`。
