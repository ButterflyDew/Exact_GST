# ReleaseV1：可审查的 Group Steiner Tree 发行实现

更新时间：2026-07-10。

ReleaseV1 是当前稳定算法的独立实现。它只有一条执行路径，不包装 Test18/Test19，也不接入正在研究的 dual-cut、distance-row 等探针。发行目标是：在跨数据集平均口径上，运行时间与 solver 增量空间均优于 PrunedDP 约一个数量级，同时不让较重预处理在小 `g` 上反噬。

## 1. 发行文件

```text
methods/Release/release_v1.cpp   完整实现，当前约 745 行
methods/Release/release_v1.h     求解接口与最小验收统计
readme_files/release_v1.md       本说明
gst_release_v1_main              独立命令行目标
```

源码内部没有环境变量、算法开关、数据集名判断、固定层/固定时刻特判、进度输出或调试输出。保留的统计只有总时间、各预处理时间、保存状态数和峰值 live states，用于发行验收，不参与算法决策。

## 2. 固定算法路径

设全部组的 mask 为 `U`，`H=floor(g/2)`，`D(S,v)` 是覆盖组集 `S` 且以 `v` 为共同根的最小代价。

1. 每个组做一次多源 Dijkstra，得到 `gd[a][v]`。
2. 计算 root-star 上界；`g>=4` 时从该根再构造一次合法 greedy tree。
3. 在组间最短距离上构建 exact Hamiltonian-path 表，由此得到 TSP/2 future lower bound。
4. 仅生成 `1<=|S|<=H` 的 rooted DP rows。同根子集 merge 后，在原图上做 A* 闭包。
5. 当前 row 与至多两个已生成 half rows 在同一根点拼接，持续更新完整解上界。
6. row 保存 `need=D(S,v)+h(v,U-S)`；上界下降后，`need>best` 的状态不再可见，并在层边界物理删除。

这是发行版的全部主线。没有图/询问压缩，也没有实验候选的隐藏入口。

### `g<=3` 直返

连接至多三个组的最优树存在一个中位点 `v`。删除 `v` 后，通向各组命中点的路径互不相交，因此：

```text
OPT = min_v sum_a gd[a][v]
```

这正是 root-star 值。`g=1,2,3` 在组距离完成后直接返回，不构建 TSP 表和 DP rows。该分支是精确结构结论，不是性能参数。

### TSP/2 下界

对未覆盖组集 `R`，Held-Karp 表给出指定端点的最短组 Hamiltonian path：

```text
path[R][a][b] = 覆盖 R、从组 a 到组 b 的最短 Hamiltonian path
h(v,R) = 1/2 * min_{a,b in R}(gd[a][v] + path[R][a][b] + gd[b][v])
```

任意连接 `v` 与 `R` 的 future tree 翻倍后是一条 closed walk；把命中点之间的路段替换为组间最短距离只会降价，所以 `h` admissible。沿边 `uv` 移动根时，两条 root incident 路段合计至多变化 `2w(u,v)`，故：

```text
h(u,R) <= w(u,v) + h(v,R)
```

同一个 `h` 因而可安全用于 A* 排序、seed/relax 剪枝和持久 `need`。

Held-Karp 逐起点复用一张 `2^g * g` 临时表，不再同时保留所有起点。临时空间由 `O(g^2 2^g)` 降为 `O(g 2^g)`；构建结束后只保留 lower-bound 查询需要的 endpoint paths。

### Half-DP 与 Complete

最优树可在某个根处分成至多三块，每块覆盖的组数不超过 `H`。因此只需保存 half masks，并枚举：

```text
S union X union Y = U
|S|, |X|, |Y| <= H
```

当 `3|S|<g` 时，当前及更早的 rows 不可能由三块覆盖 `U`，所以不做 Complete。这里没有固定层级或数据相关阈值。

DP merge 与 Complete 共用同一个 `ForEachSum` 遍历器；singleton、dense/dense、dense/sparse 和 sparse/sparse 四种组合只实现一次。补侧查询先做 binary lookup；累计查询成本达到整行构建成本时才物化 complement row。该 rent-or-buy 判据只使用当前容器长度与查找成本，不读取数据集、`g`、层号或运行时间。

### Row 存储

- sparse row 保存按顶点排序的 `(vertex, distance, need)`；
- dense row 保存长度为 `n+1` 的 `distance/need` 数组；
- 仅当 dense 的实际字节数更小时选择 dense，不使用密度超参数；
- 不再被后续 Complete 引用的 half row 不持久化；
- 上界下降后按 `need>best` 统一压缩已有 rows。

## 3. 正确性

全部使用 Release/O2，误差阈值为 `1e-6`。

| check | result |
| --- | --- |
| 随机小图，`g=2..10` | `ALL_OK seed=710931 iterations=300` |
| 随机小图，固定 `g=13` | `ALL_OK seed=710933 iterations=30` |
| Toronto 默认询问 | 当前 ReleaseV1 160/160 与现有 DPBF 最后一次完整运行逐项相同，最大差 `0` |
| small suite | 35/35 与 PrunedDP 相同，最大差约 `1e-10` |
| fast suite | 20/20 与 Test19 相同；PrunedDP 完成的 19 条也相同 |

随机对拍工具每例分别运行 DPBF 与 ReleaseV1，并比较结果文件最后一次运行的权重，符合 `agent.md` 的增量结果口径。

## 4. 复杂度

组距离预处理为：

```text
O(g(m+n log n)) time, O(gn) space
```

组 tour 预处理为 `O(g^3 2^g)` 时间；逐起点工作表为 `O(g2^g)` 临时空间，保留的 endpoint paths 最坏为 `O(g^2 2^g)` 空间。

half-DP 的论文分析口径保持：

```text
O(3^g n + 2^g((g+log n)n+m)) time
O(2^g n + g^2 2^g) space
```

按 `agent.md`，理论口径把优先队列视为 Fibonacci heap；实现使用 `std::priority_queue`，因此实际 push/pop 有二叉堆的对数常数。代码不超过上述 DP 状态与 split 枚举边界。

## 5. 小 `g` 预处理审计

当前 ReleaseV1：

```text
result_snapshot/small/20260710_190652
```

冻结的 PrunedDP O2 基准：

```text
result_snapshot/small/20260710_142204
```

每个 `g` 汇总五个数据版本各一条查询：

| g | ReleaseV1 | PrunedDP | speedup | ReleaseV1 TSP prep |
| ---: | ---: | ---: | ---: | ---: |
| 2 | `0.0404s` | `0.1746s` | `4.32x` | `0ms` |
| 3 | `0.0674s` | `0.1951s` | `2.90x` | `0ms` |
| 4 | `0.0980s` | `0.2085s` | `2.13x` | `0.040ms` |
| 5 | `0.1197s` | `0.2592s` | `2.17x` | `0.115ms` |
| 6 | `0.1676s` | `0.3784s` | `2.26x` | `0.277ms` |
| 7 | `0.3518s` | `2.9016s` | `8.25x` | `0.621ms` |
| 8 | `0.8598s` | `7.5610s` | `8.79x` | `1.947ms` |

35 条中没有一条慢于 PrunedDP；最弱单条为 Toronto-new `g=5` 的 `1.50x`。因此当前重预处理没有在小 `g` 上形成负收益。`g<=3` 完全跳过 TSP；`g=4..8` 的五数据集 TSP 总成本仍低于 `2ms`。

## 6. Fast 时间与空间

当前 ReleaseV1：

```text
result_snapshot/fast/20260710_190652
```

冻结的 PrunedDP O2 基准：

```text
result_snapshot/fast/20260710_135619
```

| dataset version | ReleaseV1 | PrunedDP lower bound | speedup |
| --- | ---: | ---: | ---: |
| Toronto | `6.815s` | `14.577s` | `2.14x` |
| Toronto-new | `11.210s` | `70.238s` | `6.27x` |
| DBLP | `2.841s` | `41.867s` | `14.74x` |
| DBLP-new | `2.176s` | `35.598s` | `16.36x` |
| MovieLens | `11.797s` | `>218.096s` | `>18.49x` |
| **total** | **`34.838s`** | **`>380.375s`** | **`>10.92x`** |

PrunedDP 的 MovieLens `g=12` 在 100 秒保护时间内未完成；表中只把这 100 秒计入下界。

对双方都完成的 19 条：

| space metric | ReleaseV1 | PrunedDP | ratio |
| --- | ---: | ---: | ---: |
| 平均完整进程 peak RSS | `44.696MiB` | `365.562MiB` | `8.18x` |
| 平均 solver peak 增量 | `16.947MiB` | `337.789MiB` | `19.93x` |

完整进程口径包含双方共同加载的图，MovieLens 图本身约占 `147MiB`，所以该口径未达到十倍；扣除求解前公共图 RSS 后，solver 增量空间达到约二十倍。发行目标采用跨 fast suite 的总时间与 solver 增量空间，不声称每个数据版本都达到十倍。

## 7. 发行边界

- ReleaseV1 不包含 dual-cut potential、distance-only rows、dynamic/anchor dual 等后续候选。
- 不包含 degree/Voronoi/portal 等 baseline 同样可用的普通图或询问压缩。
- 不包含 one-tree、pair partition、额外 greedy roots、诊断下界或历史环境开关。
- 当前接口保护上限为 `g<=20`；已做精确随机验证的最高组数为 `g=13`。
- 本轮没有运行 full DBLP `g=13` q1；源码整理和预处理降内存不足以触发五小时长跑。

## 8. 构建与复现

CMake 对 Release/RelWithDebInfo 显式启用 O2；MSVC Release 同时显式保留 `/Ob2`。

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_release_v1_main gst_random_compare

.\build\Release\gst_release_v1_main.exe Toronto result
python tools\snapshot_benchmark\snapshot.py --method ReleaseV1 --suite small --no-prepare
python tools\snapshot_benchmark\snapshot.py --method ReleaseV1 --suite fast --no-prepare
```

完整随机命令及 seed 见本节正确性表。随机工具生成的 `.tmp_random_compare*` 目录已在验证后清理。
