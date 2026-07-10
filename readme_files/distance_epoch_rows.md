# Distance-only Epoch Rows：统一低空间候选

更新时间：2026-07-10。本文说明独立探针 `tools/distance_epoch_solver_probe`，它从 ReleaseV1 复制出一条无开关执行路径，用于验证两个相互配合但逻辑独立的机制：

> 状态更新：本文记录 dual-cut 之前的表示层基底。其后 distance solver 加入 directed-cut potential，但 900 秒仍未完成；最终 full g13 由 ReleaseV2 的 dual-anchored global-label recurrence 完成。本文中的 `30.749s` fast 与 900 秒 full 结果只代表冻结阶段，当前结论见 `release_v2.md`。

1. DP row 只持久化精确 `distance`，不持久化 `need=distance+h`；
2. pair 层结束后，用同根 singleton/pair 分块构造一次可行上界。

该实现尚未进入 `methods/Release` 或 Test 主线。它已经通过正确性、small 和 fast 门槛，但自身的 full DBLP g13 900 秒有界运行未完成；它现在是已完成阶段使命的表示层证据，不是新发行版，也不是最终跑通 g13 的实现。

## 1. Distance-only row

ReleaseV1 的每个存活状态长期保存两个 double：

```text
(D(S,v), D(S,v)+h(v,U-S))
```

第二项只用于判断当前上界 `best` 下该状态是否仍可参与搜索，不属于 DP 真值。新表示只保存精确 `D(S,v)`：

```text
sparse: (vertex, distance)       约 12 bytes/state
dense:  distance[1..n]           约 8(n+1) bytes
```

保存 row 时仍用当时的 `D+h<=best` 过滤。以后 `best` 下降，旧 row 可能暂时多留状态，但这些值仍是精确距离；读取它们最多增加工作，不会制造错误答案。每次进入新的 mask 大小时，如果 `best` 自上次压缩后下降，则统一重算已有 row 的 `h` 并删除 `D+h>best` 的逻辑状态。

与 ReleaseV1 的 row payload 相比：

```text
ReleaseV1     min(16(n+1), 20s)
distance-only min( 8(n+1), 12s)
```

其中 `s` 是该 row 的存活状态数。探针不会为某个数据集、`g`、层级、密度或运行时刻选择不同策略。

### 正确性

- row 中保存的始终是由 exact subset merge 和图最短路得到的 `D(S,v)`；删除 `need` 不改变 recurrence。
- 旧上界下留下、在新上界下已经无用的状态仍是精确可行代价。它参与 merge 或 Complete 只可能产生一个不优的可行候选。
- 压缩删除条件 `D(S,v)+h(v,U-S)>best` 安全，因为 admissible `h` 不超过从 `(S,v)` 完成剩余组所需的代价。
- 等号状态保留；浮点权重比较与 ReleaseV1 使用同一口径。

## 2. 同根 singleton/pair 可行上界

pair rows 完成后，对一个根 `r` 定义 block cost：

```text
c({a},r)   = gd[a][r]
c({a,b},r) = D({a,b},r)
```

用 monomer-dimer subset DP 把全部组划分成 singleton 和 pair，使 block cost 之和最小。每个 block 都有一棵包含同一根 `r` 的可行树；这些树的并连通且覆盖全部组，因此 block cost 之和是合法上界。树之间即使重边，真实并集只会更便宜。

根集合统一取：

```text
root-star 的最优根
union
候选点数量最少的必经组中的全部点
```

任一 GST 解都必须命中每个组，所以枚举某个完整必经组不会漏掉最优解；选候选点最少的组只是对等价必经组做确定性的工作量最小化，不是经验阈值。重复顶点会先去重。

pair row 若没有保存 `D({a,b},r)`，该 pair 只是不参与分块，singleton 分块仍保证上界有限。更强地，若它是被 `D+h>best` 删除的，则任何使用该 pair、并在 `r` 完成其他组的分块也不可能改善当前 `best`。

对每个根，分块 DP 为 `O(g2^g)` 时间、`O(2^g)` 临时空间；根数至多 `n+1`。所有 pair lookup 与分块总成本仍包含在项目目标口径

```text
O(3^g n + 2^g((g+log n)n+m))
```

内。`g<=3` 继续使用已证明精确的 root-star 直接返回，不构造 pair rows 或分块表。

## 3. 正确性与 small 门槛

全部构建为 Release/O2。

| check | result |
| --- | --- |
| 当前版本随机小图，`g=2..10` | `ALL_OK seed=710731 iterations=500` |
| 当前版本固定 `g=13` | `ALL_OK seed=710733 iterations=60` |
| small，5 个数据版本、`g=2..8` | 35/35 与 ReleaseV1 权重一致 |
| fast，5 个数据版本、`g=9..12` | 20/20 与 ReleaseV1 权重一致 |

small 按 `g` 汇总并与 `release_v1.md` 中同一 O2 PrunedDP 批次比较：

| g | distance-only + pair | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 2 | `0.0333s` | `0.1746s` | `5.24x` |
| 3 | `0.0552s` | `0.1951s` | `3.54x` |
| 4 | `0.1022s` | `0.2085s` | `2.04x` |
| 5 | `0.1009s` | `0.2592s` | `2.57x` |
| 6 | `0.1589s` | `0.3784s` | `2.38x` |
| 7 | `0.3190s` | `2.9016s` | `9.10x` |
| 8 | `0.8301s` | `7.5610s` | `9.11x` |

因此没有观察到小 `g` 重预处理反噬 PrunedDP。另一方面，本轮 A/B 中 distance-only 在 small 上多数组合略慢于冻结的 ReleaseV1；空间收益不能写成无条件时间收益。

## 4. Fast 时间与空间

同一进程内先运行冻结 ReleaseV1、再运行当前探针；每个数据版本包含 `g=9..12` 各一条查询：

| dataset version | ReleaseV1 | distance-only + pair | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `6.307s` | `5.525s` | `0.876x` |
| Toronto-new | `10.519s` | `9.814s` | `0.933x` |
| DBLP | `2.541s` | `2.577s` | `1.015x` |
| DBLP-new | `1.920s` | `1.513s` | `0.788x` |
| MovieLens | `10.216s` | `11.320s` | `1.108x` |
| **total** | **`31.502s`** | **`30.749s`** | **`0.976x`** |

总时间快 `2.4%`，但 DBLP 与 MovieLens 仍退化，不能宣称逐数据集胜出。fast g12 的生成 row payload 缩小 `1.67x--1.88x`；进程 peak RSS 的代表值如下：

| dataset version | ReleaseV1 peak RSS | distance-only peak RSS |
| --- | ---: | ---: |
| Toronto | `~55.0MiB` | `35.14MiB` |
| Toronto-new | `~99.7MiB` | `56.19MiB` |
| DBLP | `~34.1MiB` | `23.34MiB` |
| DBLP-new | `~33.9MiB` | `15.04MiB` |
| MovieLens | `~153.4MiB` | `153.07MiB` |

MovieLens 的公共图本身约占 `147MiB`，所以完整进程 RSS 几乎不变；row payload 仍缩小 `1.67x`。空间结论应同时报告 solver row 与公共图两种口径。

## 5. Full DBLP g13 有界结果

2026-07-10 使用当前 Release/O2 二进制运行 900 秒，脚本持有并在超时后回收实际 PID。运行未产生最终权重：

```text
pair masks                    78
enter k=3 after compact       404.684s
pair-partition unique roots   165
best                          15.0174 -> 14.7395
pair-partition time           106.268ms
live states before compact    168,924,473
live states after compact     167,476,094
logical reduction             1,448,379 = 0.8574%
row payload allocated         1,558,616,592 bytes
known optimum                 12.5936282853
```

对照不带 pair 上界的上一轮：`k=3` 在 `389.265s` 进入，存活 `168,924,473`。本轮上界与重算使层切换多约 `15.4s`，只删除 `0.86%`；到 900 秒仍未走出 `k=3`。压缩改变 row 的逻辑长度，但 `vector` 容量不收缩，所以已经分配的 `1,558,616,592` bytes 不会立即回收。真正的空间收益来自不再持久化 `need`，不能把逻辑压缩量重复计作 RSS 收益。

所以 pair 分块是正确、低开销的辅助上界，但没有改变 full DBLP g13 的数量级瓶颈，不构成再次无界长跑的依据。

## 6. 当前结论

- distance-only row 是目前唯一同时通过随机、small、fast 时间和空间检查的新表示；值得保留为后续候选基底。
- pair 分块上界在 DBLP-new 和 MovieLens fast g12 更新过 `best`，但 full DBLP g13 只复现根星点的 `14.7395`，没有找到更强根。
- full g13 的核心仍是状态数量：仅 pair 层就有约 1.69 亿逻辑存活状态，去掉一个 double 不能消除 `k=3` 累积。
- 下一步必须从 recurrence 依赖、row lifetime/recomputation 或更强且廉价的 GST 特有下界减少状态数量；不继续靠表示常数或数据特判推进。

## 7. 复现

```powershell
cmake --build build --config Release --target gst_distance_epoch_solver_probe
.\build\tools\distance_epoch_solver_probe\Release\gst_distance_epoch_solver_probe.exe --self-check 710731 500 2 10 10
.\build\tools\distance_epoch_solver_probe\Release\gst_distance_epoch_solver_probe.exe data_snapshot/generated_fast DBLP_data_bfs g12 1 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\distance_epoch_solver_probe\run_bounded.ps1 -Seconds 900
```
