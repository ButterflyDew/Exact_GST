# ReleaseV3：Half Rows 与 Farthest-Goal Global Labels

更新时间：2026-07-13。ReleaseV3 是冻结的框架 B 发行对照：它先按 `|mask|` 有序生成至多 `floor(g/2)` 的 rooted rows，以二/三块同根拼接完成答案；当实际 row 工作足以支付强搜索时，一次性释放 rows，并转入 farthest-goal anchored global labels。当前纯 A 发行入口是 ReleaseV4，见 `release_v4.md`。

它不是按 `g` 调用 ReleaseV1/ReleaseV2，也没有算法模式、数据集开关、固定层级、密度阈值、运行时刻特判、调试输出或普通图/query 压缩。

本文维护算法定位、正确性和实测结果。逐类型、逐函数、逐阶段的源码导读见 `release_v3_implementation.md`；两份文档分工明确，性能数字只在本文维护。

## 1. 文件与接口

```text
methods/Release/release_v3.h               唯一公开接口与必要统计
methods/Release/release_v3.cpp             完整单路径算法
methods/Common/dual_cut_potential.h         共享的 directed-cut 势函数
build/Release/gst_release_v3_main.exe       统一数据集入口
```

公开接口只有：

```cpp
SolveResult SolveOneQuery(const Graph& graph, const Query& query);
```

ReleaseV3 由完成 full DBLP g13 的 candidate 原地晋升而来，没有复制第二份实现。candidate 专用 CLI 已删除；随机回归使用统一 `gst_random_compare`。

## 2. 单一路径

```text
g <= 3: exact goal-root-star，直接返回

g >= 4:
  group distances + group metric + TSP/2
  root-star upper
  size-ordered half rows
      |- row_work 达到 greedy_buy_work：购买 greedy，压紧 rows
      |- rows 已精确完成：直接返回
      `- row_work 达到 global_buy_work：
           构造 dual，释放 rows
           选择离 root-star 根最远的组为 permanent goal
           进入 anchored global labels
```

切换只发生一次。global 复用 `group_distance`、group metric、TSP/2、dual、terminal color 和当前合法上界，但不把 half rows 强行编码成 global labels。批量 row transfer 的历史探针没有降低净状态，反而增加峰值或 wall，因此发行源码不保留该路径。

## 3. Exact 结构

### 3.1 `g<=3` goal-root-star

令 `gd[a][v]` 为 `v` 到组 `a` 的最短距离。对至多三个组：

```text
OPT = min_v sum_a gd[a][v].
```

任取 `v`，各组最短路的并给出合法 GST。反向在最优树中固定每组一个命中点；至多三个点的树有一个中位根，根到各点的树路总长等于树长，而 `gd` 不大于对应树路。

实现并行推进各组多源 Dijkstra，并用未定型组的 frontier minimum 维护精确停止证书，不先物化完整 `g*n` 表。

### 3.2 Ordered half rows

`D(S,v)` 表示覆盖 `S` 且以 `v` 为根的最小树。只生成：

```text
1 <= |S| <= floor(g/2).
```

singleton 直接读取 `gd`；非 singleton 由同根真子集拼接后做一次图最短路闭包。mask 按 `(|S|,mask)` 排序，所以依赖 row 总在当前 row 之前。row 使用按 root 排序的 sparse 数组或 dense distance 数组；join 在线性 merge、从较小侧 binary search 和 dense scan 中按理论操作数选择，不使用 `(root,mask)` 动态 hash。

half 完备性来自最优树 centroid：删除 centroid 后，每个分量至多含 `floor(g/2)` 个已选组代表点；这些分量能合并为至多三个仍不超过 half 的块。因此存在同一根 `v` 和二或三个不交块 `S_i`：

```text
union_i S_i = U
OPT = sum_i D(S_i,v).
```

当前 row 在传播时在线尝试缺侧的一或两块。补侧 lookup 的累计租金达到一次物化成本时才构造 complement row。half-size row 若不可能被后续 recurrence 或完成式读取，会按 mask 顺序立即释放。

### 3.3 Farthest-goal anchor

任意必达组都可作为 anchored recurrence 的目标。ReleaseV3 在已经得到 root-star 最优根 `r` 后确定：

```text
anchor = argmax_a gd[a][r]，平局取组编号最小者。
```

任意可行树都命中某个 `x` 属于 anchor；以 `x` 为根，只需在 label mask 中表示其余 `g-1` 组。anchor 从显式 mask 维度消失，但在每个未完成 label 的 remaining set 与 future lower bound 中始终存在，直到 label 到达 anchor 顶点。

选择最远组不影响 exact 性和 `2^(g-1)n` 状态上界。它把 root-star 视角下最难到达的组变成 permanent goal，避免为该组传播 singleton labels，同时保留其较强的 goal distance/dual 势。规则只读取本方法已经计算的 `gd` 和 root-star 根，不试跑多个 anchor，也没有经验参数。

### 3.4 Global labels 与下界

global label `(v,S)` 通过边传播和同根不交集合并生成，在 `v` 命中 anchor 且 `S=U\{anchor}` 时完成。搜索使用：

```text
max(group farthest / group-MST-half, TSP/2, directed-cut dual).
```

这些 future bounds 都满足 admissibility、edge consistency 和不交 subset splice。`best` 只来自 root-star、greedy、dual primal 或已完成 labels，均为真实可行树上界。half rows 可以在切换时全部释放，因为 global recurrence 从非 anchor singleton labels 完整启动。

## 4. 无参数 Rent-or-Buy

row 阶段不按 wall time 决策，而累计复杂度口径内的实际操作：

```text
row_work = root intersection / dense scan
         + adjacency relaxation
         + (priority_queue push + pop) * ceil(log2 n)
         + complement lookup / build

greedy_buy_work = g  * (m + n*ceil(log2 n))
global_buy_work = 2g * (m + n*ceil(log2 n)).
```

达到前者时购买一次 greedy 并按新上界 compact；达到后者时构造 dual、释放 row 工作区并立即转 global。单个 mask 最多造成一条 row 的预算越界。公式不依赖数据集、固定 `g`、层级、密度、已运行秒数或拟合倍率，符合 `agent.md` 第六条。

## 5. 复杂度

```text
time  O(3^g n + 2^g((g+log n)n+m))
space O(2^g n + 2^g g^2 + gn+m).
```

half masks 的二/三块枚举不超过 `3^g`，每条 row 至多做一次图闭包。global 只有 `2^(g-1)n` 个 labels，不交 join 总枚举受 `3^(g-1)n` 控制。TSP/2 与 group-MST 工作表包含在 `2^g poly(g)` 项中。

具体实现使用 `std::priority_queue`；按仓库规则，理论最短路项以对应 Fibonacci-heap 口径核算。

## 6. 正确性

全部为 Release/O2，DPBF 为 oracle，容差 `1e-6`：

| check | result |
| --- | ---: |
| candidate 内部随机 `g=2..10`，seed `711841` | `3000/3000` |
| candidate 内部固定 `g=13`，seed `711843` | `100/100` |
| 正式 ReleaseV3 黑盒随机 `g=2..10`，seed `711861` | `1000/1000` |
| 正式 ReleaseV3 黑盒固定 `g=13`，seed `711863` | `100/100` |
| dual admissibility / consistency / splice，seed `711831` | `60244 / 101814 / 6173322` checks |
| Toronto DPBF 最后一次完整 run，`query.txt` | `160/160`，最大误差 `4.942e-7` |
| full Toronto g13 | `0.704846702` |
| full DBLP g13 | 打印值 `12.593628`，与既有 `12.5936282853` 相差 `2.853e-7` |

## 7. Small35

每个 `g` 含五个数据版本各一条。时间为 farthest-goal candidate 的 Release/O2 实测；正式 ReleaseV3 仅做文件晋升，没有改变求解路径。

| g | ReleaseV3 precursor | PrunedDP | speedup | global queries |
| ---: | ---: | ---: | ---: | ---: |
| 2 | `6.656ms` | `147.65ms` | `22.2x` | 0/5 |
| 3 | `22.964ms` | `168.71ms` | `7.35x` | 0/5 |
| 4 | `65.057ms` | `182.63ms` | `2.81x` | 0/5 |
| 5 | `84.480ms` | `231.38ms` | `2.74x` | 0/5 |
| 6 | `151.829ms` | `348.37ms` | `2.29x` | 2/5 |
| 7 | `371.772ms` | `2864.60ms` | `7.71x` | 2/5 |
| 8 | `1057.032ms` | `7513.43ms` | `7.11x` | 4/5 |
| **total** | **`1.760s`** | **`11.457s`** | **`6.51x`** | **8/35** |

solver 增量空间逐 `g` 为：

| g | ReleaseV3 precursor | PrunedDP | reduction |
| ---: | ---: | ---: | ---: |
| 2 | `1.266MiB` | `~83MiB` | `65.6x` |
| 3 | `3.039MiB` | `~85MiB` | `28.0x` |
| 4 | `1.895MiB` | `~88MiB` | `46.4x` |
| 5 | `2.242MiB` | `~96MiB` | `42.8x` |
| 6 | `5.875MiB` | `~112MiB` | `19.1x` |
| 7 | `4.641MiB` | `~213MiB` | `45.9x` |
| 8 | `42.379MiB` | `~313MiB` | `7.39x` |
| **total** | **`61.336MiB`** | **`~990MiB`** | **`16.1x`** |

该表是 2026-07-14 统一空间口径以前的历史结果：每个独立进程取 `peak_rss-rss_before` 后求和，不能与当前 `weights.txt` 第三列的绝对 query peak RSS 直接混比。g8 的主要增量来自 MovieLens 公共图上的 dual residual；它是该历史 small 口径下唯一低于 10x 的明确放宽项。后续运行以 `RUN.md` 的 query peak RSS 为准。

`g=3..6` 是明确放宽项，`g=7..8` 接近但未达到 10x。主要底座是完整 group distances；在这些小查询上没有足够 DP 工作摊销。当前口径不要求每个低 `g` 硬达 10x，但保留逐 `g` 表，不能用 fast 总体掩盖。

## 8. Fast20

| g | V3 time | Pruned time | speedup | V3 solver MiB | Pruned solver MiB | reduction |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 9 | `1.320s` | `21.540s` | `16.3x` | `53.957` | `590` | `10.9x` |
| 10 | `1.872s` | `53.074s` | `28.4x` | `77.312` | `1163` | `15.0x` |
| 11 | `2.935s` | `89.054s` | `30.3x` | `107.680` | `1802` | `16.7x` |
| 12 | `6.662s` | `>216.707s` | `>32.5x` | `209.816` | `>2863` | `>13.6x` |
| **total** | **`12.790s`** | **`>379.869s`** | **`>29.7x`** | **`448.766`** | **`6418`** | **`14.3x`** |

PrunedDP 的 MovieLens g12 在 `100s` timeout 内未完成，所以 g12 与 total 只写严格下界。ReleaseV3 20 条全部完成且权重一致。

PrunedDP 的 g12 空间只汇总四条已完成查询，未把 MovieLens timeout 的公共图峰值冒充 solver 状态，因此 `>13.6x` 是保守下界。完整进程还包含公共图，不能写成算法状态空间比例。

farthest-goal 相对固定第一组的 fast g12 settled 比例为：

```text
DBLP       0.390x     DBLP-new  0.364x
MovieLens  1.000x     Toronto   0.878x
Toronto-new 0.662x
```

## 9. Full-Size 结果

### 9.1 Toronto g13

```text
weight              0.704846702
formal wall          3.546s
solver total         3.540s
switch              k=2, mask 9
anchor              group 13, gd=0.232502
global settled      682410
peak RSS            125.7MiB
```

固定第一组候选为约 `4.164s / 771315 settled / 134.4MiB`。farthest-goal 三项均改善。

### 9.2 DBLP g13 q1

2026-07-11，干净 candidate 二进制在最多 1800 秒的 runner 内正常完成：

```text
printed weight       12.593628
solver total         531.556s
group distances       17.430s
rows envelope         79.331s（包含下列 dual 35.223s）
dual                  35.223s
global               429.785s
switch               k=2, mask 8
anchor               group 12, gd=2.030117
row work             1944889157
buy work             1761175858
released row bytes   159858112
global settled       11192686
global peak open     19786865
sampled peak RSS     3870.7MiB
```

正式 ReleaseV3 由该源码原地移动并接入统一 main，没有改变算法；遵循“不因纯整理重复长跑”的规则，不再次运行 full。

对比 ReleaseV2 precursor：

| method | solver wall | peak RSS | result |
| --- | ---: | ---: | ---: |
| ReleaseV2 precursor | `1427.625s` | `9231.5MiB` | `12.5936282853` |
| ReleaseV3 precursor | `531.556s` | `3870.7MiB` | `12.593628` |

ReleaseV3 在同一 full 查询上快 `2.69x`、峰值小 `2.38x`。相对 PrunedDP，后者没有该 full 查询的完成记录；本页不虚构速度比。

### 9.3 A/B 执行比例与互斥时间口径

实现输出的 `rows_ms` 从 row phase 开始计时，到 `BuildDual`、最后一次 compact 完成并进入 `FinishWithGlobal` 才停止，因此它**包含** `dual_ms`，不能直接当成纯 half-row 时间。为避免重复计时，本文采用：

```text
shared = total_ms - rows_ms - global_ms
A-side = rows_ms - dual_ms
B-side = dual_ms + global_ms
```

其中 A-side 包含 ordered rows、delayed greedy 和 row compact；B-side 把为 global 准备的 dual 与 global label search 放在一起。full DBLP g13 q1 的互斥拆分为：

| component | time | total ratio |
| --- | ---: | ---: |
| shared preprocessing / remainder | `22.440s` | `4.22%` |
| A-side（rows，扣除 dual） | `44.108s` | `8.30%` |
| B preparation（dual） | `35.223s` | `6.63%` |
| B search（global） | `429.785s` | `80.85%` |
| **B-side total** | **`465.008s`** | **`87.48%`** |

所以 DBLP 上 A 不是主要耗时；即使把 dual 也算进切换前 envelope，`79.331s` 也只占 `14.92%`。B 仍从 singleton labels 完整启动，A rows 不转移进去。

按“query 是否进入 B”统计：small35 中 `8/35=22.9%` 进入 global，另外 `27/35=77.1%` 由 small/rows 路径直接完成。现存正式 Toronto 批次为：

| batch | complete queries | entered B | shared time | A-side time | B-side time |
| --- | ---: | ---: | ---: | ---: | ---: |
| g10 | 40 | 38（95%） | `22.3%` | `44.1%` | `33.6%` |
| g12 | 40 | 40（100%） | `10.0%` | `19.8%` | `70.3%` |
| g14 | 40 | 40（100%） | `3.1%` | `5.4%` | `91.5%` |
| **g10/g12/g14 total** | **120** | **118（98.3%）** | **`5.28%`** | **`9.81%`** | **`84.91%`** |

Toronto g15 当前结果目录只有 6 条，不能当完整批次；这 6 条全部进入 B，A/B 为 `0.56%/98.60%`，仅作已有样本。当前 fast20 的正式逐 query phase 日志没有保留；历史 `14/20` 是已经撤回的两阶段候选，不能冒充 ReleaseV3 比例。

DBLP 加速的主要证据不在 A 占比，而在 B 本身的 farthest-goal anchor：相对 ReleaseV2 固定第一组，full settled labels 从 `57,198,969` 降到 `11,192,686`（`19.57%`，约少 `5.11x`），peak open 从 `61,729,007` 降到 `19,786,865`（`32.05%`，约少 `3.12x`）。这足以覆盖 A-side 的 `44.108s` 租金，并使总时间从 `1427.625s` 降到 `531.556s`。现有 full 日志没有单独保存 switch 时的 `best`，因此不把该加速归因于“A 先得到更强 best”；当前直接证据指向 anchor 后 global frontier 的显著缩小。

## 10. 版本关系

| implementation | role |
| --- | --- |
| ReleaseV1 | half-DP 单路径，小 `g` 历史发行版 |
| ReleaseV2 | fixed-anchor global labels，历史大 `g` 发行版 |
| ReleaseV3 | ordered half rows -> farthest-goal global，冻结框架 B 对照 |
| ReleaseV4 | 当前纯 A 发行版；不调用或修改 ReleaseV3 路径 |
| Test80 | ReleaseV4 的带阶段统计研究来源 |

ReleaseV3 的价值不只是“先运行一小段 ReleaseV1 再调用 ReleaseV2”：half row 阶段按 ordered representation 独立完成小查询；global 阶段共享同一 query 预处理与上界，并用理论工作事件切换；farthest-goal anchor 把最难组从状态维度转成持续目标势。

## 11. 论文来源与归属

| mechanism | source | relationship here |
| --- | --- | --- |
| rooted subset recurrence | [Dreyfus and Wagner, *The Steiner Problem in Graphs*](https://doi.org/10.1002/net.3230010302) | 经典 Steiner subset recurrence；ReleaseV3 适配 GST groups 并只持久化 half masks |
| goal-oriented Dijkstra-Steiner、future cost、TSP bound | [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) | 提供 exact label-setting 与 lower-bound 理论接口；ReleaseV3 使用其思想组织 TSP/2 与 global ordering |
| directed-cut dual ascent | [Wong, *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*](https://doi.org/10.1007/BF02612335) | dual-ascent 来源；group sink、subset splice 与 GST primal 是本仓库适配 |
| GST baseline | [Efficient and Progressive Group Steiner Tree Search](https://doi.org/10.1145/2882903.2915217) | PrunedDP 的论文来源和统一 baseline |

half centroid 的二/三块完成、goal-root-star 的 group 停止证书、ordered row 生命周期、按实际 row 工作升级 global，以及 farthest-goal anchor 的整体组合，是本仓库当前研究对象。本文描述仓库实现与组合，不在系统文献检索完成前宣称论文原创。

## 12. 构建与复现

```powershell
cmake --build build --config Release --target gst_release_v3_main gst_random_compare

.\build\Release\gst_release_v3_main.exe Toronto result g13 data 1 1

.\build\Release\gst_random_compare.exe `
  .\build\Release\gst_release_v3_main.exe `
  .\build\Release\gst_dpbf_main.exe `
  ReleaseV3 711861 1000 4 14 2 10 `
  .tmp_random_compare_release_v3 0
```

运行结束后删除 `.tmp_random_compare*`、`result_tmp*`、空结果目录和残留 solver 进程。
