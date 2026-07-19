# ReleaseV5 实现说明：永久锚点有序行与伴随高层求值

> 本文说明 `release_v5.cpp` 的发行实现、代码结构、验证和复现方式。只希望理解算法方法时，请阅读独立的 `release_v5_method_cn.md`；该文档按数据库顶会论文的方法章节组织，不包含代码导读和实验。

更新时间：2026-07-17。

## 1. 发行版定位

ReleaseV5 是 Test149 研究路径的独立冻结版。它完整保留 permanent-anchor 框架 A，不调用 ReleaseV3 的框架 B，也不调用 ReleaseV4、Test80 或任何 Test 目标。与 ReleaseV4 相比，ReleaseV5 的主要变化不是再增加一条备用求解路线，而是把高层锚定状态改成等价的伴随反向求值，并纳入转置终端生成、按工作量摊销的锚树上界和改写弧 dual 初始化。

发行源码只有以下公开接口：

```text
methods/Release/release_v5.h
methods/Release/release_v5.cpp

SolveResult SolveOneQuery(const Graph& graph, const Query& query)
```

`SolveResult` 只包含 `best_weight` 和 `feasible`。源码中没有研究开关、数据集名判断、固定 `g` 判断、运行时间门禁、密度阈值、阶段日志、计数器或调试输出。唯一的规模限制是 `g<=16`，原因是组集合使用 32 位整数位掩码且当前发行配置只面向这一范围；这不是算法路径切换条件。

## 2. 一次查询的主流程

下面是 `SolveOneQuery` 的实际执行顺序。每一步结束后才进入下一步，不存在“先跑方案 A，代价大时从头改跑方案 B”的回退机制。

1. **可行性与组距离。** 检查各组是否能处于同一连通分量；对每个组执行多源 Dijkstra，得到该组到所有顶点的距离。
2. **初始上界与永久锚点。** 用共同根星形方案建立第一个完整可行解，在该根上选择距离最远的组为永久锚点；锚点之后不再改变。
3. **下界与锚主干上界。** 构造组间路径下界、directed-cut 势函数、根到锚点的主干以及 junction 候选树；这些结构建立统一剪枝函数并尽早收紧当前最优上界。
4. **普通状态 `D`。** 按组子集大小递增生成不含锚点的有序状态行。每行先离线合并已完成行，再用 `std::priority_queue` 完成带下界的图闭包。
5. **普通阶段的界增强。** 已完成的小块普通行用于三块/四块真实可行解；dual 剩余容量与锚树上界只在已经发生的相关工作足以支付其精确成本时执行。
6. **低层锚定状态 `A`。** 只正向物化平衡切分以下的锚定行；每张行完成后立即执行 `A+D+D` 完整解结算。
7. **转置终端生成。** 顺序扫描 ordinary 行，按根一次生成所有高层 `A+D+D` 终端事件，而不是为每个高层目标重复读取相同的 `D` 值。
8. **高层伴随后缀 `F/H`。** 按组子集大小递减生成原始后缀 seed `F`，经图闭包保存为反向行 `H=C(F)`，覆盖原高层 `A` 的全部依赖边；每张 `H` 行完成后立即与低层 `A` 边界相交并更新上界。
9. **返回。** 高层依赖图全部处理结束后，当前上界就是精确最优值。

## 3. 代码结构与数据表示

### 3.1 公共预处理

`GroupDistances` 对每个终端组执行多源 Dijkstra。`RootStarUpper` 找到使组距离之和最小的共同根，并返回一个真实可行解上界。`TourLowerBound` 在组间度量上建立子集路径表，为任意“当前根加剩余组集合”提供路径型下界。

`DualCutPotential::BuildKeepingResidualChangedArcs` 位于 `methods/Common/dual_cut_potential.h`。ReleaseV5 直接调用这个固定 API，不通过宏选择旧实现。它依次为各组分配有向边容量，保存组势函数，并维护一个累计 bitset：只有残量曾被前面势函数改写的有向弧才可能违反原始组距离的三角不等式，因此后继组只从这些弧启动残量 Dijkstra；启动后仍扫描完整邻接表。该改变不修改势、残量闭包或下界值。

`anchor_junction::BuildUpper` 恢复根到锚点组的一条最短路径，并在 tight-edge 子图中用 visited 保证零权边也能终止。它同时返回压缩锚树和一个真实可行解上界。MovieLens 默认查询曾触发的零权边循环由这个公共实现统一修复，ReleaseV5 不含数据集补丁。

### 3.2 `OrdinaryRow`

`OrdinaryRow` 是 `D`、低层 `A` 和高层 `H` 共用的行容器。它支持三种等价布局：

- **sparse：** 递增的顶点编号和同序距离；
- **dense：** 以顶点编号直接索引的距离数组；
- **ranked bitmap：** 顶点存在位图、机器字前缀 rank 和紧凑距离数组。

行完成时，代码比较三种布局的实际字节数并选择最小者，没有固定密度阈值。多行求交时使用有序列表归并、二分查找或 bitmap word intersection；所有选择都由待扫描元素数或精确比较次数决定，不使用 Hash。

每张普通行还保存 `branch_bits`。它标记哪些根值在图闭包后严格优于该根上的直接子集拆分，因此可以作为不可继续在当前根拆开的完整分支交给更高层。这个位图是 ordinary 递推的语义组成，不是调试信息。

### 3.3 队列与图闭包

具体实现遵循仓库规范，统一使用 `std::priority_queue`。队列允许同一顶点存在旧条目；弹出时比较当前距离并跳过 stale 条目。队列键是“已付成本加剩余成本下界”，所以当队首已经超过当前完整解上界时，该行可以结束。

理论复杂度可以按 Fibonacci heap 的标准最短路口径书写，但源码没有 decrease-key heap。Test148 曾验证 indexed heap 对极端 DBLP/LinkedMDB 有帮助，因其属于 baseline 同样可采用的通用数据结构优化且违反仓库具体实现约束，ReleaseV5 没有纳入它。

## 4. 普通状态阶段

### 4.1 状态与种子

设锚点之外的组集合为 `K`，`k=|K|`，`h=floor(g/2)`。普通状态为

```text
D(S,v) = 连接 S 中所有组并以 v 为根的最小树成本，S subseteq K。
```

ReleaseV5 只生成 `1<=|S|<=h` 的 `D` 行。单组行直接引用组距离；多组行固定最小组位为枢轴，累计侧必须含枢轴，另一侧必须是已发布的 root-irreducible branch。代码中的 `ForEachPivotBranchPair` 负责按两张行的布局选择有序求交方式，`StoreOrdinaryRow` 负责保存闭包结果并重新判定分支资格。

### 4.2 行闭包与统一剪枝

一张行先把所有合法同根合并写入 `row_distance`，再从这些种子执行图闭包。种子写入、队列弹出和边松弛都检查同一条件：

```text
partial_cost + max(farthest_group, tour_bound, dual_cut_bound) <= best.
```

`CutH`、`CheapH` 和 `H` 是同一逻辑的分阶段缓存：先计算便宜的 farthest 与 directed-cut 项，候选通过后才懒计算 tour 项。它们不代表不同算法路径，也不会改变下界值。

### 4.3 普通阶段的上界和下界增强

`q3=ceil(k/3)` 与 `q4=ceil(k/4)` 来自把全部非锚组完整分成三块或四块的必要块大小。相应普通行完成后，代码用真实 `D` 树和锚组最短路拼出可行解，并可对最佳分块执行有限次精确图闭包以允许共享接入路径。它们只降低 `best`，不改变状态定义。

dual 的额外剩余容量打包只执行至多一次。一次加强的工作上界由组数、顶点数和边数直接计算；每张二组 ordinary 行累计自己实际发生的 seed、堆和边扫描工作，累计工作覆盖加强成本后才执行。这里使用二组行是因为加强所消费的正是 pair 行已揭示的剩余连接结构，不是按固定数据集或固定 `g` 设置的层特判。

锚树设施上界也采用无参数的买入规则。一次树 DP 的循环次数由压缩树节点数 `t` 与 `k` 精确给出；只有 ordinary 图闭包已经发生的 queue pop 和 edge relaxation 累计达到该成本时才求值，求值后清零累计工作。merge seed 枚举不计入预算，因为更紧上界无法事后避免已经完成的 seed 枚举。

## 5. 低层 `A` 与完整解结算

锚定状态为

```text
A(S,v) = 连接锚点组、S 中所有非锚组并以 v 为根的最小树成本。
```

完整正向框架只需要 `|S|<=a=h-1`。ReleaseV5 在 `c=floor(a/2)` 处分割锚定格，只物化 `|S|<=c` 的低层 `A`。`A(empty,v)` 直接引用锚组距离；非空行由一张更小的 `A` 行吸收一张 ordinary branch 后做图闭包。`ForEachAnchoredSum` 与 ordinary 阶段一样根据有序布局选择归并、二分或 bitmap 求交。

每张低层 `A(S)` 完成后，`CompleteRoots` 枚举剩余组的无序二分 `L,R`，计算 `A(S,v)+D(L,v)+D(R,v)`。它会先用各行最小值拒绝不可能改善的分割，再选择 roots、ordinary 行或 bitmap 交集中的较小扫描域。所有分割仍被完整覆盖；这些选择只减少读取次数。

## 6. 转置终端与高层 `H`

### 6.1 为什么不继续正向生成全部 `A`

若继续物化 `c<|S|<=a` 的高层 `A`，每张行会被多个后继行和多个最终 `A+D+D` 分割读取。ReleaseV5 将这部分看作一个 min-plus 依赖 DAG。无向图最短路闭包在 min-plus 内积下自伴随，因此可以从最终 completion 反向传播，而不物化高层正向行。

为明确闭包位置，记 `F(T,v)` 为已经得到闭包后高层状态 `A(T,v)` 时，从同一根继续完成全部组的原始后缀；实际保存的反向行是 `H(T)=C(F(T))`。`H(T,x)` 已把进入 `T` 的正向图闭包反向拉回到合并根 `x`。高层 mask 按大小递减处理，且枚举每个真实 strict-superset 依赖，包括一次加入多个组的 ordinary block；所以它不是只沿相邻层传播的近似。

### 6.2 按根转置终端

高层原始后缀 `F_T` 的直接终端来自剩余组的普通二分：`G_T(v)=min D(L,v)+D(R,v)`。逐个目标 `T` 枚举会反复读取相同 ordinary 值。ReleaseV5 利用 directed-cut 势的组可加性，把每个 ordinary 值改写为

```text
reduced(S,v) = D(S,v) - potential(S,v),
budget(v) = best - potential(anchor union K,v).
```

于是合法终端的必要条件统一为 `reduced(L,v)+reduced(R,v)<=budget(v)`，与目标 `T` 无关。代码按 64 个连续顶点流式读取所有 ordinary 行，在每个根上把值按 reduced cost 排序，然后一次生成全部目标的最小终端事件。

每个根有两条完全等价的枚举路线：排序后的预算前缀对，或对每个 `L` 枚举其补集子掩码。代码先计算两条路线将执行的精确 probe 数并选择较小者。选择不依赖经验常数；实际 probe 数始终受补集枚举的 `O(3^k)` 上界控制。

### 6.3 反向闭包和边界结算

每张反向行先形成原始 `F(T)` seed：它由转置阶段生成的 `G_T` 事件，以及更大 mask `U` 的 `H(U)` 加 ordinary branch `U-T` 两部分取最小。seed 随后执行与正向行相同的图闭包，得到并保存 `H(T)=C(F(T))`；剪枝前缀改为锚点加已纳入组的下界。

`EvaluateBackwardBoundary` 在每张 `H(T)` 完成后立即与所有可用低层 `A(S)` 结算第一条跨切分边 `A(S)+Br(T-S)+H(T)`。边界必须使用已经拉回闭包的 `H`，而不是同根原始后缀 `F`。这样新上界能被之后尚未开始的反向行使用。所有高层 mask 完成后，低层直接 completion 与跨切分 completion 已覆盖完整正向 `A` DAG 的全部推导。

## 7. 正确性检查表

ReleaseV5 的精确性依赖以下不变量，源码清理没有改变它们：

1. `best` 始终来自一棵真实可行树，因此只能安全降低。
2. farthest、tour 和 directed-cut 都是不超过真实剩余成本的下界；只在 `partial+lower>best` 时删除状态。
3. root-irreducible branch 只是消除同根等价重复；递归展开后仍覆盖标准 subset recurrence 的全部分解。
4. `D/A/H` 行的 sparse、dense 和 bitmap 只改变布局，不改变保存的顶点和值。
5. 转置终端的 reduced-cost 公式是 directed-cut 势可加性的恒等变换；两条枚举路线覆盖同一组不相交 mask 对。
6. `H` 反向传播覆盖高层 `A` DAG 的全部依赖边；每条反向候选都对应一条原正向推导。
7. changed-arc dual 只省略由原始最短路三角不等式证明不可能违反的初始化弧；残量闭包仍读取完整图。
8. 跳过尚未摊销的锚树或 dual 加强只保留较松的合法界，不删除任何状态定义或转移。

因此 ReleaseV5 与完整 permanent-anchor 正向格返回相同最优值。

## 8. 复杂度与空间

记 `n=|V|`、`m=|E|`、`k=g-1`、`h=floor(g/2)`、`a=h-1`、`c=floor(a/2)`，并定义

```text
N_D = sum(i=1..h) C(k,i)
N_A = sum(i=1..c) C(k,i)
N_H = sum(i=c+1..a) C(k,i).
```

最坏情况下每张状态行在全部顶点上稠密。忽略低阶多项式项，ordinary 合并、completion 和伴随依赖的 mask 关系均受 `O(3^k)` 控制；每张实际生成的行至多执行一次图闭包。因此总时间可写为

```text
O(g(m+n log n)
  + 3^k n
  + (N_D+N_A+N_H)(m+n log n)
  + t 3^k),
```

其中 `t` 是压缩锚树节点数。按仓库理论口径使用 Fibonacci heap 时，单次闭包可写成 `O(m+n log n)`；具体代码仍使用 `std::priority_queue`。changed-arc dual 不改变 `O(gm)` 的最坏初始化界，但把实际初始化弧检查从全弧数降为累计改写弧数。

状态行最坏空间为

```text
O((N_D+N_A+N_H)n + gn + m + E),
```

其中 `E` 是保存的转置终端事件数。实际空间由每行最小字节布局决定；changed-arc bitset 额外占 `2m` bits。低层 `A` 与高层 `H` 的平衡切分减少实际驻留，但本文不把它表述为对所有实例都固定减半。

## 9. 构建、运行与输出

Release/O2 构建：

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_release_v5_main -- /m:1
```

CMake 对 MSVC Release 显式启用 `/O2 /Ob2`，对其他编译器启用 `-O2`。运行接口与其他发行版一致：

```powershell
.\build\Release\gst_release_v5_main.exe DBLP result g10 data 1 5
```

参数依次为图选择器、结果根目录、查询选择器、数据根目录、1-based 起始询问和询问数。图与查询文件只加载一次。`weights.txt` 每条结果为

```text
time_seconds best_weight query_peak_rss_mb
```

第三列是该询问求解期间以 1ms 采样的绝对 peak RSS，不是进程生存期前缀最大值。`releasev5_stats.txt` 只由统一 CLI 写入运行时间和内存口径，不包含算法内部探针。

## 10. 验证与当前证据

### 10.1 正确性

- Release/O2 随机图对 DPBF：`300/300`，seed `150001`，覆盖 `n=4..11`、`g=2..9`。
- 原始五数据集 `g10 q1..q5`：25 条 ReleaseV5 与 Test149 权重逐项一致，最大绝对误差为 0。
- Test149 此前另有 `200/200` DPBF 对拍，并已验证 changed-arc 与原 dual 的状态轨迹一致。

### 10.2 发行抽取回归

以下配对使用五个原始图、同一 `query_g10.txt` 的连续五条查询、同一 Release/O2 构建和同一多询问进程口径。它验证从 Test149 删除统计、日志和开关后没有引入跨库回退；它不是 g15 全查询分布的替代品。

| 数据集 | 查询数 | Test149 总时间 | ReleaseV5 总时间 | V5/149 | V5 更快或持平 | Test149 / V5 最大 query peak |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto | 5 | `3.974s` | `3.424s` | `0.862` | `5/5` | `28.383 / 28.719MiB` |
| DBLP | 5 | `447.979s` | `421.423s` | `0.941` | `4/5` | `2002.504 / 2003.723MiB` |
| DBpedia | 5 | `399.673s` | `396.879s` | `0.993` | `4/5` | `2733.055 / 2738.141MiB` |
| LinkedMDB | 5 | `39.041s` | `36.585s` | `0.937` | `5/5` | `472.125 / 471.848MiB` |
| MovieLens | 5 | `42.785s` | `42.509s` | `0.994` | `3/5` | `2782.426 / 2782.926MiB` |
| **合计** | **25** | **`933.453s`** | **`900.820s`** | **`0.965`** | **`21/25`** | **`2782.426 / 2782.926MiB`** |

五库总时间均未回退。peak RSS 的逐库差异为 `-0.277..+5.086MiB`，相对大图常驻内存处于采样和分配波动范围；ReleaseV5 的算法状态与 Test149 相同且删除了统计数组，因此没有新增结构性空间。原始结果和逐查询 CSV 位于 `result_snapshot/release_v5_validation/20260717_release_v5_g10_q1_5/`。

### 10.3 已有 g15 证据的继承边界

ReleaseV5 固定的是 Test149 的同一算法路径，因此继承其已完成的五库阶段证据：Toronto 冻结五问完整求解、MovieLens 全 40 条 D2、DBLP/DBpedia/LinkedMDB 的冻结难度点均未回退。DBLP g15 查询方差很大，已有 D2 全 40 条和冻结分层面板；本次发行清理没有结构性突破，因此按运行纪律没有重新执行五小时级 full DBLP 长跑，也不拿 q1 单条代表整个数据集。完整分布见 `g15_five_dataset_profile.md`。

### 10.4 GPU4GST 数据上的扩展曲线

Musae、Twitch、Github 的冻结相关查询面板已经覆盖 `g=4..15`。ReleaseV5 在 `g=4,5` 与 PrunedDP++ Strict 同一数量级但分别慢约 `2.06x/1.61x`；`g=8` 聚合快 `4.77x`，到 `g=9` 才在聚合和逐查询几何平均上同时超过 `10x`。`g=10..12` 没有 timeout，时间和绝对 query peak RSS 都稳定超过 `10x`；`g=13..15` ReleaseV5 全部完成，而 Strict 只完成 `5/9、4/6、2/6`。高 `g` 的完成对统计存在删失偏差，不能当作全部查询均值。完整实验口径、逐查询反例、原始结果入口和投稿缺口见 `release_v5_target_and_publication_gap.md`。

## 11. 代码审阅结论

最终发行源码满足以下审阅条件：

- 不包含 `GST_TEST*`、`GST_DUAL_CHANGED_ARC_SEEDS` 等研究开关；
- 不包含 Test80 stats、计时分段、progress/probe 输出或调试文件；
- 不调用 ReleaseV3、ReleaseV4、Test80 或 PrunedDP；
- 不按数据集、固定 `g`、wall time、密度或已知询问编号选择算法；
- ordinary、anchored、backward 共用同一有序行结构，不重复维护等价容器；
- changed-arc dual 通过命名明确的公共 API 固定启用；
- 具体最短路全部使用 `std::priority_queue`；
- 热路径中保留的工作量计数只参与无参数的买入决策，不用于日志。

源码比 ReleaseV4 长，原因是 ReleaseV5 内含低层 `A`、转置终端和完整高层伴随 `H`，而不是隐藏调用研究目标。已经删除的内容包括所有未保留分支、宏包围路径、统计字段、探针计数和无副作用参数；继续压缩只能通过拆分正式公共组件完成，不能靠删除完备依赖或把实现重新藏回 Test80。

## 12. 论文引用与贡献边界

- [Dreyfus and Wagner, 1971](https://doi.org/10.1002/net.3230010302)：rooted subset dynamic programming 的经典来源。
- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：goal-oriented Dijkstra-Steiner 与一致 future-cost 下界的理论背景。
- [DS*](https://arxiv.org/abs/2011.04593)：允许任意 admissible lower bound 的 Dijkstra-Steiner 扩展及开源普通 Steiner 求解器。
- [Wong, 1984](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 路线来源；GST 组势、sequential residual、剩余容量打包和 changed-arc 初始化是本仓库适配。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：本文统一比较的 GST baseline；源码未公开带来的 Hash/dense、逐状态 MST 和 `lb_2` pathmax 三项复现口径见 `pruneddp_reproduction.md`。
- [GPU4GST](https://doi.org/10.1145/3769792)：PACMMOD/SIGMOD 2025 的 concurrent DP 与 GPU 工作量平衡方法；其[公开实现和八个数据集](https://github.com/toziki/GPU4GST-sigmod)是论文级直接对照，而不是可忽略的旁线。

经典同根 subset merge、多源 Dijkstra、一般 directed-cut 下界、位图、有序列表和优先队列不作为原创贡献。当前仓库候选机制是 permanent-anchor 的 `D/A` 状态组织、root-irreducible ordered rows、完整高层 `A` DAG 的伴随求值、利用组势可加性的按根转置终端，以及锚树/dual 加强的精确工作量摊销组合。现有检索支持把这组状态变换作为待形式化的候选主线，但不能仅凭相对 PrunedDP++ 的加速宣称论文级新颖性；完整目标检验、GPU4GST 竞争关系和投稿缺口见 `release_v5_target_and_publication_gap.md`。
