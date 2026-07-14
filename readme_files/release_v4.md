# ReleaseV4：纯 Anchor Ordered-Rows 发行版

> 只阅读算法方法时，请直接进入 `release_v4_method_cn.md`。该文档按学术论文的方法章节组织，不包含实验、构建和代码导读；本文继续维护发行实现、验证与结果。

更新时间：2026-07-13。ReleaseV4 是从 Test80 收敛得到的纯框架 A 发行版。它不调用 Test80、ReleaseV3 或 global-label 框架 B；没有功能开关、数据集特判、wall-time 门禁和调试统计。公开入口是 `methods/Release/release_v4.cpp`，公开 API 只有 `best_weight` 与 `feasible`。

## 1. 主线流程：从 query 到精确答案

先给出记号：图有 `n` 个顶点、`m` 条无向边、`g` 个终端组；选定一个 permanent anchor 后，其余 `k=g-1` 个组用 mask 表示；`h=floor(g/2)`。

```text
可行性与组距离
    -> 初始上下界与 permanent anchor
    -> branch-junction 可行上界
    -> ordinary D rows，期间执行 early upper 和按工作量购买 packing
    -> anchored A rows，并逐 row 执行 A+D+D completion
    -> 当前 best 即精确最优值
```

### 第 1 步：检查 query，并计算组距离

**做什么：**先检查所有终端组是否可能在同一个连通分量中被连接。随后对每个组做一次 multi-source Dijkstra，得到 `gd_i(v)`，即顶点 `v` 到组 `i` 最近终端的距离。

**如何结束：**若 query 不可行，立即返回 `feasible=false`；若没有终端组，返回权重 `0`；若只有一个可行终端组，组距离完成后返回权重 `0`。其余 query 在全部 `g` 次 Dijkstra 完成后进入第 2 步。

**留下什么：**一张 `g x n` 的 group-distance 表。后续 singleton row、上下界、anchor 选择和 junction 全部复用这张表，不重新计算组距离。

### 第 2 步：建立初始 best、lower bounds 与 permanent anchor

**做什么：**扫描所有顶点，计算把每个组独立接到同一根的 root-star 可行解，得到初始 `best` 和 root-star 根 `r`。在 `r` 上距离最远的组被选为 permanent anchor。随后在组间 metric 上建立 tour lower bound，并以 `r` 为根构造 directed-cut dual；dual 同时给出一个可行 primal upper，用它继续降低 `best`。

**如何结束：**root-star 扫描、tour subset DP 和 directed-cut dual 都各执行一次；三者完成后不再改变 anchor。不存在“试若干 anchor 再选最快者”的过程。

**留下什么：**固定的 anchor、当前可行上界 `best`、tour lower bound、directed-cut potential，以及暂时保留的 residual capacities。

### 第 3 步：构造 branch-junction 可行上界

**做什么：**恢复从 `r` 到 anchor 组的一条最短路 `P`，把它视为已经付费的主干。对每个 nonanchor triple 找到接入 `P` 代价最小的汇合根，将这些根到 `P` 的 parent paths 合成一棵候选树，再把连续 degree-2 路径压成带长度的边。最后在这棵压缩树上做 subset facility DP，得到一棵真实可行树并更新 `best`。

**如何结束：**所有 nonanchor triples 都扫描完、压缩树 subset DP 的 full mask 值完成后结束。junction 只运行一次，不根据运行时间重复扩展候选树。

**留下什么：**更小的 `best` 和 paid anchor path `P`。压缩树只服务本次上界计算；后续 rows 不转成压缩树状态。

### 第 4 步：按层生成 ordinary D rows

**做什么：**`D(S,v)` 表示覆盖 nonanchor 组集合 `S`、以 `v` 为根的最小普通树。算法按 `|S|=1,2,...,h` 生成 rows。singleton row 直接引用 `gd_i(v)`；非 singleton row 先在共同根合并较小 rows，得到一批 seeds，再运行一次带 lower bound 的 A* 图闭包。传播结束后，只把可能被后续 recurrence 使用的 rooted values 保存到当前 row，并标记其中 root-irreducible 的 branch values。

**单个 row 如何结束：**该 row 的 priority queue 为空，或剩余节点都满足 `distance + lower > best` 时，图闭包结束。row 随后按顶点编号排序并保存，scratch distances 立即复位。

**整个 D 阶段如何结束：**所有 `1<=|S|<=h` 的 masks 都处理完后结束。算法不生成更大的 ordinary masks，因为最终 completion 只需要两个大小至多 `h` 的普通侧。

**D 阶段中发生的两类界更新：**当 rows 已覆盖四块或三块 partition 所需的 block size 时，算法用已有真实 D trees 构造 quarter/three-block 可行上界，并只对最佳 witness 做有限次 lifting；这些步骤只降低 `best`。处理 D2 时还会累计实际 seed checks、heap push/pop 和 adjacency scans；若这些工作足以支付一次 residual packing，就加强 lower bound，否则在 D2 结束时直接释放 residual。两者都不改变 D 状态定义。

### 第 5 步：按层生成 anchored A rows，并立即尝试 completion

**做什么：**`A(S,v)` 表示覆盖 anchor 和 nonanchor 集合 `S`、以 `v` 为根的最小 anchor tree。`A(empty,v)` 由 anchor group distance 隐式给出。其余 rows 按 `|S|=1,2,...,h-1` 生成：每次将一个已经完成的较小 A row 与一个 ordinary D branch 在共同根相加，再通过与 D 阶段相同的 `RowSearch` 做图闭包。

**单个 A row 如何结束：**priority queue 为空，或剩余节点全部被 `distance + lower > best` 剪掉时结束。closure 得到的每个有效 root 会先尝试 star upper；整张 row 完成后立即执行 `A+D+D` completion。completion 更新 `best` 后，当前 A row 再按新 best 过滤并决定是否保存给更高层。

**整个 A 阶段如何结束：**隐式 `A(empty)` 和所有 `1<=|S|<=h-1` 的 masks 都完成 completion 后结束。最高 A 层只参与 completion，不再保存，因为不会有更高 row 使用它。

**留下什么：**最终 `best`。此时它既是某棵真实可行树的权重，又因为 D/A 状态和 completion 覆盖所有最优树而不可能高于最优值，所以它就是精确答案。

### 第 6 步：返回结果

若最终 `best` 有限，返回 `{best_weight=best, feasible=true}`；否则返回不可行。发行 API 不返回研究计数、阶段计时或内部状态。

## 2. 状态主干与完整化为什么成立

### 2.1 D 与 A 的职责

设 anchor 组为 `a`，其余组集合为 `K`：

```text
D(S,v) = 覆盖 S subset K、根为 v 的最小普通树，1 <= |S| <= h
A(S,v) = 覆盖 a union S、根为 v 的最小 anchor tree，0 <= |S| <= h-1
```

所有 A 状态天然包含 anchor，因此 A mask 只使用 `g-1` 个 nonanchor bits，这就是 anchor-aware 降维。D 与 A 分开后，完整树中唯一含 anchor 的一侧始终由 A 表示，其余部分始终由普通 D 表示，不需要在运行中猜测哪个 partial tree 应当成为主干。

### 2.2 为什么 D 只发布 root-irreducible branches

构造 `D(S,v)` 时固定 `S` 的最低 bit 为 pivot。含 pivot 的一侧作为 accumulator，另一侧只读取在根处不能继续拆分的 D branch。任何在根处可拆分的 D tree 都能递归拆成更小的 root-irreducible branches，再逐个接回 accumulator，因此 branch-only 消费不会删除最优 rooted tree。

这个组织方式的作用不是近似，而是避免同一个可拆分 rooted value 在多层 recurrence 中反复作为完整右侧参与合并。发行实现仍会对每个生成的 D row 做一次完整图闭包，保证 row 内保存的是精确 rooted costs。

### 2.3 为什么只生成半层 rows

任意最优树都存在一个按终端组 token 计数的平衡结点。以该结点为共同根，可以把整棵树分成唯一含 anchor 的一侧和至多两个普通侧；三侧包含的组数分别不超过 `h-1`、`h`、`h`。因此最终扫描

```text
A(S,v) + D(L,v) + D(R,v)
```

并枚举剩余 mask 的无序分割 `L union R`，足以覆盖任意最优树。算法不需要生成 full-mask row，也不需要切换到另一套 global-label 搜索。

## 3. 上下界的细节

### 3.1 row closure 使用的 lower bound

对当前 rooted partial tree 尚未覆盖的 original-group mask `R`，ReleaseV4 使用三种安全 future 的最大值：到最远未覆盖组的距离、组间 tour endpoint lower bound、directed-cut potential。三者都是 admissible，并满足图传播所需的一致性；取最大值仍然 admissible。

所有目标相关剪枝都使用同一个条件：

```text
partial_distance + lower_bound > best
```

这里 `best` 始终来自真实可行树，lower 始终不超过真实剩余成本，因此该条件不会删除可能达到最优值的状态。

### 3.2 branch-junction 如何得到合法 upper

paid path `P` 只支付一次。每个 triple candidate root 通过真实 shortest path 接到 `P`，candidate parent paths 的并仍然是一棵真实子图。压缩只把 degree-2 路径替换成等长树边，不改变成本。压缩树上的 subset facility DP 决定哪些子树边被哪些组共享；每个 DP 方案都可以展开回 `P`、parent-tree edges 和组到根的 shortest paths，所以输出一定是可行 upper。

令压缩树结点数为 `|C'|`。junction 的时间为 `O(m log n + C(g-1,3)n + |C'|3^(g-1))`，空间为 `O(n + |C'|2^(g-1))`。

### 3.3 quarter/three-block upper 在何时结束

设 `k=g-1`。quarter 阶段在 `ceil(k/4)` 层完成且该层严格早于 three-block 层时执行：先在 root-star 根上把 nonanchor mask 分成四块，记录最佳真实 partition，然后最多对四种 anchor-side 选择做 lifting closure。three-block 阶段在 `ceil(k/3)` 层执行：rows 生成期间在线结算可用三块，层结束后最多对最佳 witness 的三种 anchor-side 选择做 lifting。

两阶段都只组合已经完成的真实 D rows。固定的四次或三次不是经验超参数，而是一个已选 partition 中可能成为 anchor-side 的全部块数；全部选择处理后该阶段自然结束。它们不发布新状态，因此即使没有改善 best，也不会影响 D/A 完备性。

### 3.4 progressive residual packing 在何时执行和结束

directed-cut dual 建立后保留 directed residual capacity。对每个组，在同一 residual graph 上计算到该组的 directed distance，并沿 paid path `P` 截断，得到额外 anchor-aware potential direction。所有 active groups 同步增加 scale；某条 residual arc 饱和后，冻结在该 arc 上有正梯度的组，其余组继续。每轮至少有一组完成或被冻结，因此最多 `g` 轮结束。

packing 的额外最短路在低 `g` 或容易 query 上可能不划算，所以它不是无条件预处理。D2 累计自己已经实际发生的全顶点 seed checks、heap push/pop 和 settled adjacency scans；只有满足

```text
pair_work >= g * (2m+n)
```

才购买 packing。右侧是一次 packing 的静态工作上界，不读取数据集、固定 `g`、row density 或 wall time。若阈值在最后一张 D2 row 前仍未达到，packing 永远不会被后续 rows 使用，residual 在 D2 层结束时释放。

packing 保证每条 directed arc 上所有新增 potentials 的总消耗不超过 residual capacity，所以加强后的 potential 仍是 admissible lower bound。它只改变第 3.1 节中的 lower 数值，不改变任何 D/A recurrence。

## 4. Ordinary D rows 的实现细节

### 4.1 singleton 与非 singleton

singleton `D({i},v)` 直接读取 `gd_i(v)`，不分配独立 row。对非 singleton mask，发行版枚举 pivot-oriented splits，在共同根处合并 accumulator value 与 branch value。所有候选根都送入统一 `RowSearch`，因此同一个 mask 只进行一次图闭包。

closure 开始前的最小 seed cost 被记录为 split value；传播后若某个 root 的最终 cost 严格小于 split value，说明该 rooted tree 的最优表示使用了至少一条传播边，不能在当前根直接拆回本层 seeds，于是它被标记为可供后续消费的 branch。branch 标记与 row payload 对齐存入 bitset。

### 4.2 sparse/dense row 与有序相交

非 singleton row 只有两种精确表示：

```text
sparse: sorted vertices[] + distances[]
dense:  distances[1..n]
```

保存 row 时只比较两种布局的真实字节数，选择更小者，不使用 density 常量。两个 sparse rows 通过递增列表归并；当 branch 数或一侧 row 明显更短时，代码按显式比较次数在归并和二分查找之间选择；dense row 直接按顶点编号访问。整个实现没有 `unordered_map`、`(mask,vertex)` Hash 或全局 label id。

### 4.3 统一 RowSearch

Test80 中普通 D、witness lifting 和 anchored A 的图传播曾分别展开。ReleaseV4 把它们统一为一个 `RowSearch`：`Begin` 指定剩余组，`Seed` 收集共同根候选，`Run` 执行 priority-queue closure，`Reset` 只清理本 row 触及的顶点。该统一只删除重复代码和热路径统计，不改变 seeds、队列顺序、lower bound 或停止条件。

## 5. Anchored A rows 与 completion 的实现细节

### 5.1 A row 如何生成

对给定 `S`，枚举一个 ordinary side `B subset S`，其余 `S-B` 由已经完成的 A row 表示。共同根候选为

```text
A(S-B,v) + D_branch(B,v)
```

所有候选共同进入一次 `RowSearch`。当 `S-B` 为空时，A 值来自 anchor group distance；当 `B` 是 singleton 时，D 值直接来自 group distance。其余情况都使用第 4.2 节的有序 row 相交。

### 5.2 completion 如何扫描

当前 A row 完成后，剩余 nonanchor mask 被无序分成 `L` 与 `R`。roots、D(L) 和 D(R) 三个递增列表中选择最短者作为 driver，其余列表只向前移动，不做 Hash lookup。每个共同 root 更新

```text
best = min(best, A(S,v) + D(L,v) + D(R,v))
```

completion 扫描完全部无序分割后结束。更新后的 best 会立即用于过滤当前 A row；最后一层 A row 之后不再有消费者，所以只完成扫描而不保存 payload。

## 6. 正确性结论

ReleaseV4 沿用 Test80 已验证的状态语义，发行重构没有增加新的剪枝条件。结论汇总如下，详细证明来源见 `test80_anchor_progressive.md` 与 Test21 文档：

1. pivot/branch D recurrence 与完整同根 `D+D` recurrence 等价；
2. A recurrence 始终保留最优树中唯一含 anchor 的一侧；
3. 平衡结点分解保证 `A+D+D` completion 覆盖任意最优树；
4. root-star、junction、quarter 和 three-block witness 都由真实路径组成，因此只产生可行 upper；
5. farthest、tour、directed-cut 与 packed potentials 都是 admissible lower bounds；
6. 所有剪枝均为 `partial+lower>best`，因此不会删除最优状态；
7. 最终 best 同时是可行解上界和完备状态空间中的最小值，所以 ReleaseV4 返回精确最优权重。

## 7. 理论时间与空间复杂度

令 `P` 为实际生成的非 singleton D/A row 数，最坏 `P=O(2^(g-1))`；令 `L` 为 sparse/dense rows 实际保存的总 payload；令 `|C'|` 为 branch-junction 压缩树结点数。priority queue 按二叉堆复杂度核算。

| 阶段 | 时间 | 空间 |
| --- | --- | --- |
| group distances | `O(g(m+n)log n)` | `O(gn)` |
| group metric tour lower | `O(2^g g^3)` | `O(2^g g^2)` |
| directed-cut dual | `O(g(m+n)log n)` | `O(gn+m)` |
| branch-junction | `O(m log n + C(g-1,3)n + |C'|3^(g-1))` | `O(n+|C'|2^(g-1))` |
| D/A subset joins 与 completion | `O(3^(g-1)n)` 保守上界 | 包含在 rows payload 中 |
| P 张 row closures | `O(P(m+n)log n)` | worst-case `O(Pn)`，实际为 `O(L)` |
| optional packing | `O(g(m+n)log n + g^2m)` | `O(gn+m)` |

将各阶段相加，保守总时间为

```text
O(2^g g^3
  + (g+P)(m+n)log n
  + g^2m
  + (C(g-1,3)+3^(g-1))n
  + |C'|3^(g-1))
```

其中 `g^2m` packing 项只在工作量购买条件满足时出现。保守总空间为

```text
O(Pn + 2^g g^2 + gn + m + |C'|2^(g-1))
```

实际 row 空间以 `O(L)` 代替 `O(Pn)`；这正是 ordered sparse/dense rows 在 DBLP 上显著小于 global-label frontier 的来源。实现公开支持 `g<=16`，该限制来自整数 mask 与当前 tour table 的存储边界，不是数据集特判。

## 8. 发行代码与 review 顺序

推荐按主线而不是按行号阅读：

| 代码位置 | 对应主线 |
| --- | --- |
| `OrderedAnchorSolver::Run` | 第 1 至第 6 步的唯一顶层流程 |
| `GroupDistances`、`TourLowerBound` | 第 1、2 步的距离与静态 lower |
| `anchor_junction_upper.cpp` | 第 3 步的 paid path 与压缩树 DP |
| `BuildOrdinaryRows`、`BuildOrdinaryRow` | 第 4 步的 D rows、early upper 与 packing 购买 |
| `RowSearch` | 所有 row/witness 共用的图闭包 |
| `BuildAnchoredRows`、`BuildAnchoredRow`、`Complete` | 第 5 步的 A rows 与精确完成 |
| `dual_cut_potential.h` | directed-cut dual 与 progressive packing |

ReleaseV4 不调用 Test80 或任何旧 Release。公共 junction/dual helper 接收已经计算好的 group distances 和 root，不重复执行组距离或 root-star。源码中没有研究 stats、阶段计时、runtime feature flag 和调试输出；CLI 只写统一的 weight、wall time 与 RSS。

## 9. Release/O2 正确性与测试效果

### 9.1 正确性

```text
seed 714001  g=2..13    100/100
seed 714003  g=2..13    300/300
seed 714007  fixed g13   50/50
```

全部随机实例与 DPBF 在 `1e-6` 内一致。small35 和 fast20 的每条权重也逐项与 Test80 快照一致。

### 9.2 相对 Test80 的发行清理效果

| suite | ReleaseV4 | Test80 | 变化 |
| --- | ---: | ---: | ---: |
| small35 | `2.148s` | `2.488s` | `-13.7%` |
| fast20 | `8.303s` | `9.099s` | `-8.7%` |
| Toronto full g13 | `7.857s / 58.0MiB` | `8.722s / 58.0MiB` | `-9.9%` time |
| DBLP full g13 q1 | `544.379s / 2158.6MiB` | `666.509s / 2161.9MiB` | `-18.3%` time |

DBLP 两版权重都为 `12.5936282853`。ReleaseV4 的主要差异是删除热路径计数和统一 closure，状态定义与算法结果不变。

### 9.3 相对 PrunedDP

| suite | ReleaseV4 time | PrunedDP time | speedup | V4 solver MiB | Pruned solver MiB | reduction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| small35 | `2.148s` | `11.457s` | `5.33x` | `274.281` | `~990` | `3.61x` |
| fast20 | `8.303s` | `>379.869s` | `>45.7x` | `218.905` | `6418` | `29.3x` |

solver MiB 对每个独立进程取 `peak_rss-rss_before` 后求和。PrunedDP 的 fast MovieLens g12 在 `100s` 内未完成，所以 fast 时间只写严格下界。ReleaseV4 在 `g=9..12` 的主目标区间明显超过时间和空间 10x；small `g=2..8` 为既定放宽项，不能用 fast 总量掩盖。

### 9.4 Full DBLP 与 ReleaseV3/B

ReleaseV4 full DBLP g13 q1 位于 `result/DBLP/ReleaseV4/query_g13`：

```text
weight    12.5936282853
wall         544.378637s
peak        2158.637MiB
```

相对框架 B 的 ReleaseV3 `531.556s / 3870.7MiB`，ReleaseV4 时间只慢 `2.4%`，峰值低 `44.2%`。这次 full 是当前最终算法与 Release/O2 二进制的正式结果；后续纯文档或格式调整不重复运行。

快照目录：small35 为 `result_snapshot/small/20260713_185040`，fast20 为 `result_snapshot/fast/20260713_185007`，Toronto full 为 `result/Toronto/ReleaseV4/query_g13`。

## 10. 构建与运行

```powershell
cmake --build build --config Release --target gst_release_v4_main
./build/Release/gst_release_v4_main.exe Toronto result g13 data 1 1
```

Release 配置显式启用 MSVC `/O2 /Ob2`，非 MSVC Release/RelWithDebInfo 显式启用 `-O2`。

## 11. 论文关系

- [Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302)：rooted subset DP 的经典来源；pivot/branch ordered rows 是本仓库的组织方式。
- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：Dijkstra-Steiner label setting 与一致 future 的理论背景；ReleaseV4 将其落实为离线 row closure。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 路线来源；GST group potential 与 residual progressive packing 是仓库适配。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：统一 baseline；ReleaseV4 不把 baseline 同样可做的普通图/query 压缩计作贡献。

branch-junction parent-tree facility DP、D2-work purchase 与同步 progressive packing 是本仓库候选组合。文档只陈述当前实现、证明结论和实验边界，在系统文献检索完成前不宣称论文级原创性。
