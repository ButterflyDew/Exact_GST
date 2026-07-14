# Test62：Canonical Half Target A*

更新时间：2026-07-12。Test62 完成 Test60--61 的最后一个代数项：不再生成任何 A-half row，而从 noncanonical top generator `M_N` 运行带 anchor-aware lower bound 的 target A*，仅在弹出顶点点查询 canonical generator `Q_C(v)`。机制随机 `500/500` 精确；但 462 个独立 complement searches 与 point queries 使 fast20 为 `13.011s`、Toronto full 为 `23.786s`，仍慢于正式 Test21，故代码撤回，不运行 DBLP。

## 1. Direct Target 恒等式

Test61 剩余：

```text
min_x C(Q_C)(x) + M_N(x).
```

交换全局最小值：

```text
min_x C(Q_C)(x) + M_N(x)
= min_v Q_C(v) + C(M_N)(v).
```

因此可从 `M_N` 做多源 shortest-path closure，得到按需的 `D_N(v)=C(M_N)(v)`；每次弹出 `v` 时只点查询：

```text
Q_C(v) = min_{proper Y subset C} A_(C-Y)(v) + branch_D_Y(v).
```

整块异常 `anchor + D_C` 由已闭包 canonical `D_C(v)` 直接加入 point minimum。使用 full `D_C` 而非仅 branch sites 仍是合法可行树，且覆盖原异常候选。

## 2. A* 停止证书

对 covered set `anchor union C`，使用：

```text
H_C(v) = max(group farthest, TSP/2, directed-cut dual).
```

这些 bounds 与正式 Test21 相同，均 admissible 且 edge-consistent。队列 key 为：

```text
D_N(v) + H_C(v).
```

当最小 key 超过当前合法 `best` 时，未弹出顶点均不可能改善。每个 complement pair 独立复用 dense distance/touched workspace，不使用 Hash、经验时刻、数据集或固定 `g` 特判。

## 3. 正确性与 Fast20

随机 DPBF：

```text
seed          713291
iterations    500/500
n             4..14
g             2..13
tolerance     1e-6
build         Release/O2
```

候选快照：`result_snapshot/fast/20260712_225824`。

| metric | formal Test21 | Test61 | Test62 |
| --- | ---: | ---: | ---: |
| query summary | `12.380s` | `12.616s` | `13.011s` |
| stats total | `12.304s` | `12.536s` | `12.930s` |
| anchored merge probes | `40.41M` | `48.18M` | `43.89M` |
| target sources | `0` | `0` | `50,805` |
| target pops | `0` | `0` | `138,029` |
| target point probes | `0` | `0` | `2.366M` |
| top completion | `0` | `0.213s` | `0.536s` |

A-half merge probes基本消失，但大量小 priority queues 与 binary point lookups 使总时间进一步退化。

## 4. Toronto Full g13

| metric | formal Test21 | Test61 | Test62 |
| --- | ---: | ---: | ---: |
| weight | `0.7048467020` | `0.7048467020` | `0.7048467020` |
| wall | `20.787524s` | `23.838044s` | `23.785836s` |
| peak RSS | `102.949MiB` | `105.090MiB` | `105.035MiB` |
| D6 values | `223,812` | `124,010` | `124,010` |
| anchored masks | `1,586` | `2,048` | `1,586` |
| anchored merge probes | `43.40M` | `60.34M` | `45.54M` |
| target sources / pops | `0` | `0` | `12,649 / 22,513` |
| target point probes | `0` | `0` | `1.047M` |
| top completion | `0` | `0.812s` | `1.185s` |

Test62 把 A-half masks 完全恢复到正式数量，但 wall 与 Test61 基本相同。原因有两层：top split generators 仍全部构造，ordinary phase比正式慢约 `0.96s`；target operator 再增加约 `1.19s`，少做一半 D6 propagation 无法回本。

## 5. 系列结论

Test60--62 已覆盖 `k=2h` top closure 的三个精确接口：

1. 两侧显式 A-half rows；
2. 一侧 A-half + 一侧三函数 scalar completion；
3. 一侧 scalar completion + 一侧 direct target A*。

三者都真实减少 D6 rooted payload，但都慢于正式 Toronto。最高层传播不是当前值得继续交换的瓶颈；split generator 与互补方向消费本身已占主要成本。后续不再调整 canonical orientation、A* queue、point lookup 或 top mask parity。

下一主线应回到更低层的状态族，例如让 anchor skeleton 直接替代 D2/中层连接证书；只有在那里删状态才可能同时改善 DBLP g13 的时间与空间。

撤回后正式 Test21 已通过规范化 PATH 的 Release/O2 编译和随机 DPBF 对拍 `100/100`（seed `713301`）；top generator、target queue、point-query helper 与统计字段均未保留。

Test62 没有新增论文引用。A* lower bound 与 metric closure 背景仍来自 Dijkstra-Steiner/Dreyfus--Wagner；direct target operator 是本轮仓库候选，尚不宣称论文级原创性。
