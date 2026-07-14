# ReleaseV2：Dual-Anchored Global Labels

更新时间：2026-07-13。ReleaseV2 是历史 fixed-anchor 大组数发行实现。它从已经跑通 full DBLP g13 q1 的独立原型重写而来，只有一条执行路径，不包装 Test19，也不依赖 `tools/` 中的探针代码。当前纯 A 发行版见 `release_v4.md`。

```text
methods/Release/release_v2.h      唯一公开接口与精简统计
methods/Release/release_v2.cpp    完整算法实现
build/Release/gst_release_v2_main.exe
```

原型的完整逐层计数保存在 `release_v2_evidence.md`。本文只维护发行代码当前事实、证明、性能边界和论文归属。

## 1. 发行定位

ReleaseV2 解决的是 ReleaseV1/Test19 在大 `g` 上逐层物化大量 rows 的状态洪峰。核心结果是：不压缩普通图和 query，固定任意一个必达组作为 anchor，并用全局 Dijkstra-Steiner label-setting 只生成有希望的 `(subset,root)`。

| property | ReleaseV2 |
| --- | --- |
| execution path | 固定第一组为 anchor，dual/TSP/2/global labels 全部无条件启用 |
| runtime switches | 无 |
| debug/progress output | 无 |
| ordinary graph/query compression | 无 |
| numeric storage | 全部 `double`，无量化近似 |
| supported group count | `1 <= g <= 20` |
| full DBLP g13 evidence | precursor exact run `12.5936282853`, `1427.625s`, `9.015GiB` |

ReleaseV2 不是 ReleaseV1 的无条件替代。最终 small suite 中 g3--g6 的 dual 预处理存在负收益；仓库不增加 `g>=7` 或数据集特判来自动切换。ReleaseV1 继续作为小 `g` 更稳健的独立发行入口。

## 2. 删除了什么

原型约 991 行，ReleaseV2 主实现约 740 行。发行重写删除了：

- half、anchored、dual-anchored 三套模式和所有命令行策略开关；
- 随机图/query 生成器、dataset probe main 和进度环境变量；
- 40 余个调试计数、逐 mask-size 输出和 bounded-run 日志；
- 已撤回的第二 anchor dual、group-entry threshold、path-union completion；
- `tools/dual_cut_probe` 与 Test19 TSP helper 的源码依赖；
- heap 中重复保存的 cost 和旧 `Entry -> LabelRecord` 包装；
- greedy upper 与 zero-residual primal 的两份图生长代码，统一为 `GrowTree`。

保留的两个 root-star 检查承担不同职责：新 label 第一次出现时，顺便使用 cheap-bound 已经完成的组扫描尽早生成完整 witness；同一 open label 后续可能得到更低 cost，settle 时再用最终 cost 检查一次。后者不是重复预处理，而是避免把旧 tentative cost 当成最终上界。

## 3. 问题与记号

输入为非负无向带权图 `G=(V,E)` 和组集合 `A_0,...,A_{g-1}`。目标是最小化一个连通子图的边权和，使它至少命中每个组一个顶点。组可以重叠。

```text
n, m             图的点数与边数
U                 全部组的 bitmask
A0                固定 anchor group，当前为输入第一组
gd[a][v]          v 到组 a 的最短距离
D(S,v)            覆盖 S、以 v 为连接根的最优 rooted tree cost
best              已知完整可行 GST 的最小费用
h(v,R)            从 v 补齐剩余组 R 的安全下界
```

## 4. 算法流程

### 4.1 组距离与组度量

对每个组做一次多源 Dijkstra，得到 `gd[a][v]`。组间距离为

```text
gp[a][b] = min { gd[a][v] : v in A_b }.
```

它只在 query 给出的组上构建，不改变原图。后续 group-MST、TSP/2、root-star 和 dual 都复用同一份 `gd`。

### 4.2 Anchor 状态变换

固定 `A0`。任意可行 GST 必然包含某个 `x in A0`；把树视为以 `x` 为根后，只需显式记录其余 `g-1` 个组：

```text
empty != S subseteq U without A0
answer = min { D(U without A0, x) : x in A0 }.
```

算法不是预先猜一个 anchor 顶点，而是在同一全局 label space 中同时覆盖所有 `x in A0`。因此 mask 数从 `2^g-1` 降为 `2^(g-1)-1`，且不损失任何最优解。固定第一组只是确定性规范；没有按组大小或数据集试跑多个版本。

### 4.3 全局 Dijkstra-Steiner Labels

singleton 初始化：

```text
D({a},v)=0, for v in A_a and a != 0.
```

两类精确转移：

```text
edge:  D(S,u) <= D(S,v) + w(v,u)
join:  D(S|T,v) <= D(S,v) + D(T,v), S&T=empty.
```

所有 masks 共用一个按 `D+h` 排序的 `std::priority_queue`。label 只有在弹出项仍等于 root-local hash 中当前 `(cost,lower)` 时才定型。consistent/splice lower bound 保证定型值正确。

目标是 full non-anchor mask 到达任意 anchor 顶点。若 heap 的最小 key 已满足

```text
min(D+h) >= best,
```

则任何未定型或尚未生成的完成路径都不可能改善可行上界，返回 `best`。

### 4.4 初始可行上界

ReleaseV2 依次构造三类真实 GST：

1. `min_v sum_a gd[a][v]` 的 root-star；
2. 从 root-star 根及第一棵 greedy tree 中实际命中组的顶点出发，反复接入最近未覆盖组；
3. directed-cut dual ascent 后，只沿零残量弧生长的 primal tree。

这些值都是显式路径 union 的费用上界，不参与 lower-bound 证明。ReleaseV2 不使用已知最优值或 dataset-specific warm start。

### 4.5 Cheap Lower Bounds

对剩余组集合 `R`，先计算：

```text
far(v,R)       = max_a gd[a][v]
near1, near2   = gd[a][v] 中最小的两个
mst(R)         = 组间度量 gp 上的 MST
h_cheap        = max(far, (mst(R)+near1+near2)/2).
```

`far` 是任意 future tree 必须到达的最远组。把 future tree 翻倍得到从 `v` 出发的 closed walk；去掉 root 的两条连接后，组间部分至少覆盖一棵 group MST，而两条 root 连接至少是 `near1+near2`，所以第二项也不超过 future tree cost。

在查询 root-local hash 前，还使用更便宜的 `max(mst(R)/2, gd[first(R)][v])`。两级 cheap bound 的作用是避免对明显无效候选做 hash 和 TSP endpoint 扫描，不改变可接受状态集合。

### 4.6 Group TSP/2

subset Hamiltonian-path DP 预计算：

```text
path[R][a][b] = 覆盖 R、从组 a 到组 b 的最短 group path
tour(v,R)      = min gd[a][v] + path[R][a][b] + gd[b][v]
h_tsp(v,R)     = tour(v,R)/2.
```

任意连接 `v` 与各剩余组的 tree 翻倍后是一条 closed walk。保留每组一个实际命中点并把相邻段替换为 `gd/gp` 最短连接只会降成本，所以 `tour <= 2*OPT_future`。root 沿一条边移动时 tour 的两条 root incident connections 总共至多变化两倍边费，除以二后满足 edge consistency；同样的 doubled-walk 拼接给出 subset splice。

实现构建结束后只保留每个 mask 的 endpoint triples，完整 `path` 工作表立即释放。

### 4.7 Directed-Cut Dual Potential

每条无向边替换为两条同费用有向弧；每个组视为一个只有入弧的组汇点。ReleaseV2 从 root-star 根运行一次确定性的 farthest-first dual ascent：组按原图 `gd[a][root]` 降序，平局按组编号。

处理组 `a` 时，在当前残量弧费用上计算到该组的反向最短距离 `d_a`，并截断为

```text
p_a(v) = min(d_a(v), d_a(root)).
```

随后从每条有向弧残量中扣除正势差。最短路三角不等式保证残量始终非负；所有组共享同一份残量，所以对任意组子集 `R`：

```text
h_dual(v,R) = sum_{a in R} p_a(v)
h_dual(u,R) <= w(u,v) + h_dual(v,R).
```

势对不交组集合可加，因此也满足 subset splice。实际搜索使用

```text
h(v,R) = max(h_cheap, h_tsp, h_dual).
```

三个下界分别满足所需条件，取最大值仍安全。

### 4.8 Generated Root-Star Completion

任意 tentative label cost 都来自 singleton、edge 或 join，因此本身代表一棵可行 partial tree。将 root 到每个剩余组的独立最短路并入：

```text
upper(S,v) = candidate_cost(S,v) + sum_{a notin S} gd[a][v].
```

路径重叠只会让这个求和值高于真实 union cost，不会产生非法偏小上界。新 label 的 cheap-bound 扫描已经遍历 `R`，所以第一次检查不增加渐近工作。full g13 中该机制在约一百万 settled labels 前把 best 从 `15.0174` 降到 `12.6821`，使 frontier 随后进入平台和回落。

### 4.9 Label 与 Join 表示

每个 root 有一个开放寻址 flat hash，slot 直接保存：

```text
(mask, cost, lower, settled)
```

heap node 只保存 `(D+h,root,mask)`；不重复保存 cost。每个 root 的 settled masks 另有一个按组 bit 的倒排位图。join 在以下两种完整枚举之间选择理论操作数较小者：

```text
all submasks of available groups
bitmap words * |S|
```

这个比较不使用 dataset、`g`、层级、密度或运行时间阈值；两条分支都枚举所有已定型且与 `S` 不交的 mask。

## 5. 正确性闭环

1. **Anchor 等价。** 每个可行 GST 命中 `A0`，以该命中点为根即对应一个 full non-anchor label；反向把该 label 与 anchor root 合起来仍是 GST。
2. **Label 可行。** singleton、edge、join 都只构造真实 partial tree，tentative cost 从不低估某棵 witness。
3. **Recurrence 完备。** Dreyfus-Wagner 的边传播和同根不交拆分全部被枚举，最优 rooted tree 的分支可递归重建。
4. **Heuristic 安全。** cheap、TSP/2、dual 都不超过 future optimum，并满足 edge consistency 与 subset splice；`max` 保持这些性质。
5. **Upper 安全。** root-star、greedy、zero-residual primal 和 generated completion 都是显式可行 tree union。
6. **Label-setting 安全。** consistent/splice key 保证弹出的非 stale label 已达到精确 `D(S,v)`；已定型 label 不需 reopen。
7. **终止安全。** `min key >= best` 时 lower-bounded 未完成路径不能优于当前真实 witness，因此 `best=OPT`。

所有数值比较使用统一 `1e-9` 内部容差；外部正确性比较使用 `agent.md` 要求的 `1e-6`。

## 6. 复杂度

实现使用 `std::priority_queue`；按 `agent.md`，理论核算采用斐波那契堆口径。主要项：

```text
group distances + dual       O(g(m+n log n))
initial upper bounds          O(g^2(m+n log n))
Hamiltonian-path build        O(g^3 2^g) time, O(g^2 2^g) temporary space
group-MST subsets             O(g^2 2^g) time, O(2^g) space
anchored labels/edges         O(2^(g-1)(n+m))
all disjoint joins            O(3^(g-1)n)
```

由于任意固定次数的 `g` 多项式乘 `2^g` 都可由 `3^g` 吸收，上述项不超过项目目标：

```text
O(3^g n + 2^g((g+log n)n+m)).
```

label hash 最坏 `O(2^(g-1)n)`；倒排位图为 `O(g2^(g-1)n)` bit/word 口径。binary heap 使用 lazy stale nodes，理论峰值由成功松弛数界定；不宣称 polynomial space。

## 7. Release/O2 正确性

最终源码在显式 `/O2` 构建后完成：

| check | result |
| --- | --- |
| final ReleaseV2 random `g=2..10`, seed `711011` | `300/300`, `ALL_OK` |
| final ReleaseV2 random fixed `g=13`, seed `711013` | `30/30`, `ALL_OK` |
| Toronto existing DPBF last run, q `1/40/80/120/160` | 5/5 exact to printed precision |
| small five dataset versions, `g=2..8` | 35/35，max abs diff `1e-10` |
| fast five dataset versions, `g=9..12` | 20/20，max abs diff `1e-10` |
| DBLP fast g12 vs precursor | weight、`19,618` settled、`10,725` peak open 全部相同 |

Toronto baseline 读取 `result/Toronto/DPBF/default/weights.txt` 的最后一个 160-query run，而不是文件第一批追加结果。

## 8. Small 边界

最终目录：

```text
ReleaseV2  result_snapshot/small/20260710_221757
PrunedDP   result_snapshot/small/20260710_142204
```

表中为五个数据版本的 solver `total_ms` 合计：

| g | ReleaseV2 | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 2 | `0.108s` | `0.148s` | `1.37x` |
| 3 | `0.182s` | `0.169s` | `0.93x` |
| 4 | `0.379s` | `0.183s` | `0.48x` |
| 5 | `0.444s` | `0.231s` | `0.52x` |
| 6 | `0.552s` | `0.348s` | `0.63x` |
| 7 | `0.654s` | `2.865s` | `4.38x` |
| 8 | `0.880s` | `7.513s` | `8.54x` |
| **total** | **`3.199s`** | **`11.457s`** | **`3.58x`** |

35 条中有 9 条慢于 PrunedDP；毫秒级单条会受调度噪声影响，但 g3--g6 的汇总负收益稳定存在，且 dual 构建约占 ReleaseV2 small 总时间的一半。ReleaseV2 不满足 ReleaseV1 的“小 g 每条都不反噬”发行目标。

## 9. Fast 时间与空间

最终目录：

```text
ReleaseV2  result_snapshot/fast/20260710_221829
ReleaseV1  result_snapshot/fast/20260710_190652
PrunedDP   result_snapshot/fast/20260710_135619
```

| dataset version | ReleaseV2 | ReleaseV1 | speedup vs V1 | PrunedDP lower bound | speedup vs PrunedDP |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `1.500s` | `6.792s` | `4.53x` | `14.495s` | `9.67x` |
| Toronto-new | `5.564s` | `11.176s` | `2.01x` | `70.126s` | `12.60x` |
| DBLP | `0.658s` | `2.828s` | `4.30x` | `41.775s` | `63.49x` |
| DBLP-new | `0.628s` | `2.160s` | `3.44x` | `35.507s` | `56.56x` |
| MovieLens | `4.456s` | `11.755s` | `2.64x` | `>217.967s` | `>48.91x` |
| **total** | **`12.806s`** | **`34.712s`** | **`2.71x`** | **`>379.869s`** | **`>29.66x`** |

PrunedDP 的 MovieLens g12 在 100 秒内未完成，表中把 100 秒计为下界。对双方完成的 19 条：

| space metric | ReleaseV2 | PrunedDP | ratio |
| --- | ---: | ---: | ---: |
| average whole-process peak RSS | `50.355MiB` | `365.562MiB` | `7.26x` |
| average solver peak increment | `22.592MiB` | `337.808MiB` | `14.95x` |

MovieLens 公共图本身约 `147MiB`；因此完整进程 ratio 低于 solver 增量 ratio。ReleaseV2 的 dual residual 会提高小图预处理峰值，但 fast 平均 solver 增量空间仍优于 PrunedDP 一个数量级。

## 10. Full DBLP g13

发行重写前的同算法 precursor 在原图 `n=2,497,782, m=12,786,329` 上完整运行：

```text
best          12.5936282853
solver wall   1427.6247319s
runner wall   1459.076s
peak RSS      9231.500MiB (9.015GiB)
settled       57,198,969 / 10,228,417,290 possible labels
```

上一合规 Test18 完整成绩为 `20193.624s / 25185.023MiB`；precursor solver wall 快 `14.15x`，峰值 RSS 为其 `36.65%`。Test18 使用普通图缩减，ReleaseV2/precursor 没有。

ReleaseV2 删除模式和诊断、内联同一 dual/TSP 计算，并在 DBLP fast g12 复现完全相同的 label trajectory；没有改变求解策略。遵循“不因纯代码整理重复长跑”的规则，本轮没有再次运行 full q1，不能把 precursor 时间冒充为 ReleaseV2 二进制的新计时。完整逐 size 证据见 `release_v2_evidence.md`。

## 11. 论文来源与本仓库贡献

| mechanism | literature source | ReleaseV2 relationship |
| --- | --- | --- |
| Dijkstra-Steiner global labels、future costs、consistency/splice | [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) | 采用其 exact label-setting 理论接口；把普通 terminals 改写为 GST groups |
| admissible heuristic search background | [DS*: Solving the Steiner Tree Problem with few Terminals](https://arxiv.org/abs/2011.04593) | 只作为一般 lower-bound 搜索背景；ReleaseV2 使用更强的 consistent/splice 条件，不直接实现 DS* reopen 规则 |
| TSP/2 future lower bound 与 Hamiltonian-path subset DP | [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) | 下界思想不是本项目原创；Test19/ReleaseV2 将 endpoint 定义适配为 group-to-group metric |
| directed-cut dual ascent | [Wong, A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph](https://doi.org/10.1007/BF02612335) | dual-ascent 路线来自论文；组汇点、per-group capped potentials、subset splice 接口和 zero-residual GST primal 是本仓库适配 |
| GST baseline、progressive A* 与 group lower-bound 背景 | [Efficient and Progressive Group Steiner Tree Search / PrunedDP](https://doi.org/10.1145/2882903.2915217) | 作为主 baseline 和 GST 对照；ReleaseV2 不把 baseline 同样可用的图/query 压缩计为贡献 |

本仓库当前可单独陈述的实现/组合贡献是：必达组 anchor 的 `g-1` 状态变换、GST dual 势与 global labels 的 splice、生成即用的 root-star witness、root-local flat labels、理论工作量选择的 disjoint bitmap join，以及这些机制在未压缩 DBLP g13 上的完整证据。论文写作时应把“借鉴的下界/label-setting理论”和“本仓库的 GST 适配与组合”分开表述。

## 12. 构建与运行

```powershell
cmake --build build --config Release --target gst_release_v2_main gst_random_compare

.\build\Release\gst_release_v2_main.exe `
  Toronto result query data 1 1

python tools\snapshot_benchmark\snapshot.py `
  --method ReleaseV2 --suite fast --no-prepare
```

输出：

```text
result/<graph>/ReleaseV2/<query>/weights.txt
result/<graph>/ReleaseV2/<query>/releasev2_stats.txt
```

结果文件按 run header 追加；比较时必须读取最后一次 run。
