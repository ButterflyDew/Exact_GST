# 半状态与全局标签混合：ReleaseV3 前置研究

更新时间：2026-07-11。本文是 ReleaseV2 之后的研究主文档，不是发行说明。目标是把 ReleaseV1 的 half rows、三块完成和离线无 hash 表示，与 ReleaseV2 的 dual-anchored global labels 组成一条统一 exact 路线，同时避免按数据集、固定 `g`、层级、密度或运行时刻特判。

详细负结果和原始探针数字见 `../archive/half_global_probe_log_20260710.md`。该研究主线已经收敛为 ReleaseV3；当前代码、证明、逐 `g` 实测与 full DBLP 证据统一见 `../release_v3.md`。

## 1. 当前结论

可保留的结构已经收敛为三段：

1. `g<=3` 使用精确 goal-root-star，不构造 `2^g n` 状态表；
2. 一般 `g` 先按 `|mask|` 生成 `|mask|<=floor(g/2)` 的 rooted rows，以有序 root 交集完成 join，并用二/三块同根拼接答案；
3. 当实际 row 工作达到构造强 potential 的可核算工作量时，升级到 dual-anchored global labels，而不是用固定 `g` 选择版本。

三段先在 `distance_epoch_solver_probe` 内形成同一进程的 exact 原型，随后收敛为 `methods/Release/release_v3.*`。ReleaseV3 使用一阶段 rent-or-buy：先按序构造 half rows；实际 row 工作达到一次 strong-search buy-work 时，构造 dual、释放 rows 与工作区，并用已经得到的 `gd`、group metric、TSP、dual、color 和 `best` 接续 farthest-goal anchored global labels。它没有从头调用 ReleaseV2，也没有按数据集或固定 `g` 选择版本。

初始 greedy 也采用独立工作事件：先用 root-star；只有 `row_work` 达到一次 greedy 的最坏构造工作时才购买 greedy 并 compact。该事件修复了“静态跳过 greedy 导致弱 `best` 提前触发 global”的问题。两阶段版本只保留为研究 A/B，一阶段版本进入 ReleaseV3。

验收口径允许少数受完整 group-distance 底座支配的低 `g` 放宽 `10x`，但逐 `g` 报告。ReleaseV3 precursor 的 small35 为 `6.51x`，fast20 严格优于 `29.7x`，small/fast solver 增量空间分别约 `16.1x/14.3x`；full DBLP g13 已在 `531.556s / 3870.7MiB` 完成。

## 2. 三个精确结构

### 2.1 `g<=3` 的 root-star 等式

令 `gd[a][v]` 为 `v` 到组 `a` 的最短距离。对 `g<=3`：

```text
OPT = min_v sum_a gd[a][v].
```

对任意 `v`，把 `v` 到每组的一条最短路取并集，得到真实可行 GST，因此 `OPT` 不大于右式。反向选择一棵最优树实际命中的组代表点；两个或三个代表点在树上存在一个中位根 `v`，从 `v` 到各代表点的树路径总长恰为该树代价，而 `gd` 不大于这些路径，故右式不大于 `OPT`。

探针不是先算完所有 `gd` 再扫描根。它并行推进各组的多源 Dijkstra，按“某根已有多少组距离定型”维护下界：

```text
base(mask) + sum_{a notin mask} frontier_min[a].
```

全局最小值达到当前完整根时精确停止。组间距离在波前首次命中其他组时顺便得到；其 TSP/2 下界没有减少当前五库定型数，故不作为发行收益点。

### 2.2 只需 half states

在一棵最优树中为每组固定一个实际命中代表点。按代表点数取树的 centroid，删除 centroid 后每个分量含至多 `floor(g/2)` 个代表点；这些分量可合并成至多三个、每个大小仍不超过 `floor(g/2)` 的块。于是存在同一根 `v` 和二或三个不交块 `S_i`：

```text
union_i S_i = U
|S_i| <= floor(g/2)
OPT = sum_i D(S_i,v).
```

因此只需保存 half masks。二/三块完成是 recurrence 的完备终点，不是针对 `g13` 或某一层的上界特判。

### 2.3 离线 mask 顺序无需状态 hash

`D(S,.)` 只依赖真子集 rows。按 `(|S|,mask)` 排序后，一条 row 可由已完成 rows 的 root 有序交集生成，再做一次图最短路闭包。持久化形式是：

```text
sparse row: sorted (root, distance)
dense row:  distance[1..n]
```

join 在两个有序 root 序列上做 merge/binary-search 成本比较，不需要 `(root,mask)` 动态 hash。补侧二块也可按实际 lookup 租金与一次 row 物化成本做 rent-or-buy；ReleaseV1 已实现这条无经验阈值规则。

## 3. 建议的统一执行路径

```text
goal-root-star (g<=3, exact return)
        |
shared group distances / group metric / feasible upper
        |
offline half rows, ordered by |mask|
        |
actual row work reaches remaining strong-search build work
        v
dual-anchored global labels
```

升级不是 `g>=k` 开关。当前原型使用统一的实际工作口径：

```text
row_work = 实际 root-intersection 比较
         + 实际 adjacency relax
         + (heap push + pop) * ceil(log2 n)
         + 实际 complement rent/build
buy_work = 2g * (m + n*ceil(log2 n))
```

ReleaseV3 在第一次 `row_work>=buy_work` 时购买 dual，并立即转入 global。单个 mask 至多造成一条 row 的预算越界；若 rows 提前完成，则完全不支付 dual/global 成本。历史两阶段版本会再租用一份 `buy_work`，不进入发行路径。

global 接续复用所有重预处理，并在切换前释放 `distance/heuristic/complement/rows`。已经完成的 rows 不转成 global labels：批量 seed 与“预结算”两种转交都没有减少净处理状态，反而增加 peak 或跨库 wall，源码已撤回。当前保留的是方法阶段共享，不是假装两种状态表示可以零成本互换。

## 4. 当前性能边界

下表是 ReleaseV3 precursor 的 small35 审计；每个 `g` 含五个数据版本各一条。正式版只做文件晋升，没有改变求解路径。

| g | current component | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 2 | `6.656ms` | `147.65ms` | `22.2x` |
| 3 | `22.964ms` | `168.71ms` | `7.35x` |
| 4 | `65.057ms` | `182.63ms` | `2.81x` |
| 5 | `84.480ms` | `231.38ms` | `2.74x` |
| 6 | `151.829ms` | `348.37ms` | `2.29x` |
| 7 | `371.772ms` | `2864.60ms` | `7.71x` |
| 8 | `1057.032ms` | `7513.43ms` | `7.11x` |
| **total** | **`1.760s`** | **`11.457s`** | **`6.51x`** |

`g=4..6` 的主要问题不是 half-row DP，而是完整 group distances。MovieLens g4 的 PrunedDP 自身约 `148.6ms`，其中复制/构图 `89.2ms`、组距离 `53.6ms`；本方法直接复用公共图，却仍需约同量级的 `gd`。多种无需完整 `gd` 的 exact/dual 探针均未跨过传播底座，见 archive 第 18--19 节，因此这些低 `g` 作为明确放宽项，而不是继续增加琐碎分支。

历史两阶段 paired fast20 在同一进程内依次运行 ReleaseV2 与共享 hybrid。ReleaseV2 合计约 `13.460s`，hybrid 约 `14.164s`，hybrid 慢 `5.2%`；20 条权重全部一致。14 条触发 global，6 条由 rows 直接完成。该结果只保存为接续实现的前置 A/B，当前成绩以 ReleaseV3 为准。

ReleaseV3 precursor fast20 为 `12.790s`；PrunedDP 完整下界 `>379.869s`，因此时间优势严格大于 `29.7x`。solver 增量峰值和约 `448.8MiB` 对 PrunedDP `6418MiB`，约 `14.3x`。完整进程空间受公共图限制，不能写成算法状态空间比例。

正式 ReleaseV3 的 full Toronto g13 为 `3.546s / 125.7MiB / 682410 settled`，权重 `0.704846702`。full DBLP g13 precursor 在 `k2 mask8` 转 global，选择第 12 组为 farthest goal，最终 `531.556s / 3870.7MiB / 12.593628` 完成；与既有精确值误差 `2.853e-7`。

## 5. 已排除的直接拼接

- **naive dual-half global labels：** 正确，但 fast g12 的状态数和 wall 普遍高于 anchored global；full Toronto g13 也从 `4.317s` 退到 `7.980s`。
- **只在搜索结束离线三块交集：** 去掉 hash，却失去早期 `best`；fast 五库合计 `22.294s`，明显慢于动态 half 和 anchored。
- **metric-only half search：** `group-TSP/2` 能显著减状态，但 `g>=5` 很快失去竞争力；MovieLens 的组度量与 witness 扫描仍重。
- **单源/最短组对 greedy witness：** 都是真实上界，但跨数据质量不稳，已从探针源码撤回。
- **把离线 rows 批量转成 global labels：** open seed 不减 settled；预结算 seed 在 g12 四库为 `0.92x--1.07x` 的混合结果，计入 seed 后总状态不降，已撤回。
- **用完整 `gd` 替代 singleton 传播：** 批量 hash 注入在 DBLP g12 慢 `13%--14%`；惰性 key stream 在 fast 略正，但 full-size Toronto g13 又比普通 hybrid 慢 `2.5%`，无法无参数启用。
- **搜索内生组度量：** singleton labels 首次命中其他组能给出精确组对距离，但 MovieLens g4 到 `30.9M` edge relax 才收齐，晚于独立预处理。
- **根截断 dual + half global/ordered rows：** 势函数正确且强，但 MovieLens g4 分别约 `145.8ms/155.2ms`；共享 residual 或逐 row 边传播的成本都过高，代码入口已删除。
- **并行 group distance + 静态上界门控：** 顺序中位数下 `g3..6` 仅约 `6.9x/4.5x/4.9x/3.4x`；门控还会因较弱 `best` 提前触发 global。并行是 baseline 同样可用的工程优化，两条入口均已撤回，详见 archive 第 16 节。
- **progressive upper without potential：** bare-half 在 MovieLens g4 到 `23.1M` edge relax 才首次得到 best；一次 Dijkstra 的 SPT upper 虽只需 `10.2ms` 且接近最优，仍不改变 `31.7M` 的最终传播。任意 `g` 的精确 root-star upper 在该项也需 `37.1ms`。失败入口已撤回，详见 archive 第 18 节。
- **packed moat potential：** 一次 Voronoi 可构造正确的非重叠 owner-cut 势，随机与固定 g13 全部通过；但 MovieLens g4 仍需 `677.9ms / 27.9M` edge relax，仅比 bare 少约 `12%`。Voronoi group-MST 还暴露了“组候选点不可在 primal 中免费互连”的反例。源码已撤回，详见 archive 第 19 节。
- **按固定 `g` 组合 ReleaseV1/ReleaseV2：** 工程上容易，违反 `agent.md` 第六条，也没有论文方法价值。

## 6. 正确性门槛

当前新增探针均为 Release/O2：

| check | result |
| --- | --- |
| goal-root-star，随机 `g=2..3` | `5000/5000`, seed `711273` |
| metric-half threshold envelope，随机 `g=2..10` | `5000/5000`, seed `711241` |
| metric-half threshold envelope，固定 `g=13` | `100/100`, seed `711243` |
| dynamic-dual offline rows，随机 `g=2..10` | `1000/1000`, seed `711301` |
| shared hybrid，随机 `g=2..10` | `3000/3000`, seed `711485` |
| shared hybrid，固定 `g=13` | `100/100`, seed `711487` |
| delayed / one-stage candidate，随机 `g=2..10` | `1000/1000`, seed `711781` |
| delayed / one-stage candidate，固定 `g=13` | `50/50`, seed `711783` |
| farthest-goal candidate，随机 `g=2..10` | `3000/3000`, seed `711841` |
| farthest-goal candidate，固定 `g=13` | `100/100`, seed `711843` |
| 正式 ReleaseV3 黑盒，随机 / 固定 `g=13` | `1000/1000` seed `711861` / `100/100` seed `711863` |
| 泛化 goal-root 实现，随机 `g=2..3` | `2000/2000`, seed `711765` |
| zero-metric threshold，随机 / 固定 `g=13` | `3000/3000` seed `711481` / `100/100` seed `711483` |
| dual-half / offline-half，随机与固定 `g13` | 全部 `mismatches=0`，详见 archive |

所有对拍以 DPBF 为 oracle、容差 `1e-6`。一阶段模式已收敛为无开关 ReleaseV3，并完成随机、Toronto 最后一次 DPBF 160 条、small35、fast20、full Toronto 与 full DBLP 审计。dual 位于 `methods/Common` 共享模块；不因纯重构再次运行 full DBLP。

## 7. 论文归属

| mechanism | source | relationship here |
| --- | --- | --- |
| rooted subset recurrence | [Dreyfus and Wagner, *The Steiner Problem in Graphs*](https://doi.org/10.1002/net.3230010302) | recurrence 的经典来源；本仓库把 terminals 适配为 GST groups |
| goal-oriented Dijkstra-Steiner、future costs、TSP bound | [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) | ReleaseV2/探针采用其 exact label-setting 与 lower-bound 理论接口 |
| directed-cut dual ascent | [Wong, *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*](https://doi.org/10.1007/BF02612335) | dual-ascent 路线来自论文；组汇点、subset splice 和 GST primal 是本仓库适配 |
| GST baseline 与 progressive bounds | [Efficient and Progressive Group Steiner Tree Search](https://doi.org/10.1145/2882903.2915217) | PrunedDP 是统一 baseline；其同样可用的普通图/query 压缩不计作本方法贡献 |

half centroid 的二/三块完备化、goal-root-star 的组版停止证书、按缺块数的 threshold envelope，以及“offline rows 租用后升级 dual-anchored global”的整体组合，是本仓库当前研究对象。本文只称其为仓库设计，不在完成更系统的文献检索前宣称论文原创。

## 8. 冻结状态

1. 当前发行代码为 `methods/Release/release_v3.*`；本页不再维护另一份候选入口。
2. farthest-goal anchor 的枚举与评分诊断只保存在 archive，临时输出字段已从探针撤回。
3. full DBLP 已完成；纯重构、格式调整或弱短测不触发重复长跑。
