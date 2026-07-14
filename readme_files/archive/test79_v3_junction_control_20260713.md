# Test79 历史对照：Branch-Junction Work-Credited Global

冻结时间：2026-07-13。Test79 把 Test48 的 paid-anchor branch-junction 上界接到 ReleaseV3 的 farthest-goal global，并把 junction 工作计入 V3 rent-or-buy。它属于框架 B 对照，不能作为“优化框架 A”的候选。Test80 完成后，Test79 的 CLI、源码和 V3 prepared 接口已撤出；本文与 `result/*/Test79` 仅保留有效实验信息。

## 1. 当前结论

| 项目 | Test79 | ReleaseV3 | 结论 |
| --- | ---: | ---: | --- |
| 随机 DPBF | `200/200` | - | `1e-6` 内一致 |
| 固定 g13 DPBF | `50/50` | - | `1e-6` 内一致 |
| fast20 | `11.790s` | `12.790s` | Test79 快 `7.8%` |
| Toronto full g13 | `3.386s / 125.5MiB` | `3.546s / 125.7MiB` | Test79 快 `4.5%` |
| DBLP full g13 q1 | `521.055s / 3744.6MiB` | `531.556s / 3870.7MiB` | Test79 快 `2.0%`、峰值低 `3.3%` |

DBLP 最终权重为 `12.5936282853`，与既有精确记录一致。该结果只说明 junction 与 B 的组合有效，不说明 A 已 anchor-aware 或 A 能完成 full。

## 2. 主流程

```text
一次 group-distance 预处理
          |
          v
root-star 根 + 最远 permanent anchor
          |
          v
paid anchor path P
          |
          v
全部 nonanchor triples 的最优 attachment roots
          |
          v
roots 到 P 的 parent-path union + 压缩树 subset DP
          |
          +----> 合法 junction incumbent
          |
          v
junction 实际工作写入 V3 row_work
          |
          v
同一个 work >= dual/global buy_work 判据
          |
          v
ReleaseV3 ordered rows 或直接 farthest-goal global
```

Test79 不把 A rows 转成 B labels。两阶段只共享 group distances、incumbent 和统一工作账本；global recurrence 仍从自身 singleton labels 完整启动。

## 3. Branch-Junction 上界

固定 root-star 根 `r`，选择离 `r` 最远的组为 anchor，并恢复 `r` 到该组的一条确定性最短路 `P`。`P` 的代价只支付一次。

对每个 nonanchor triple `S` 扫描：

```text
x_S = argmin_x d_P(x) + sum(i in S) gd_i(x)
```

从 `P` 做一次 multi-source Dijkstra，固定每个 `x_S` 到 `P` 的 parent path。所有候选路径的并只保留 triple roots、`P` 顶点和分叉点；连续非候选 degree-2 路径压成带长度的树边。

对压缩树结点 `x` 定义：

```text
F_x(M) = 在 x 的子树内服务 nonanchor mask M，
         并把所有启用设施连到 x 的最小代价
```

结点 `x` 可用同根 star 服务任意子集，代价为对应 `gd_i(x)` 之和。合并 child `y` 时：

```text
F'_x(M) = min_{R subset M}
          F_x(M-R) + F_y(R) + [R != empty] length(x,y)
```

最终值加上 paid path `P` 的代价。每个 DP 方案都能恢复为真实图路径的并，因此它只提供合法 upper，不参与 lower-bound 证明。

## 4. 统一工作账本

最初版本先构造 junction，却让 V3 的 `row_work` 仍从零开始。它在 full DBLP 得到正确结果，但为：

```text
547.478s / 3757.2MiB
```

global settled 与 V3 完全相同，说明“更强初始上界”本身没有带来主收益。

当前版本把下列实际操作都计入 `junction_work`：

```text
root-star probes
anchor-path edge scans
multi-source Dijkstra edge scans
(heap push + pop) * ceil(log2 n)
triple-root scans
active parent-tree operations
compressed-tree subset convolutions
```

然后令：

```text
row_work = junction_work + 后续实际 row work
buy_work = 2g * (m + n*ceil(log2 n))
```

第一次 `row_work >= buy_work` 就购买 dual/global。没有新系数或阈值；junction 是已经付出的同类理论工作，必须抵扣后续租金。

Toronto full 的 `junction_work=30.37M > buy_work=20.94M`，因此不生成 row，直接进入 global。DBLP 的 `junction_work=750.77M`，随后只再租用约 `1.01B` row work，在 size-2 第 6 张 row 后切换；V3 原来在第 8 张后切换。

## 5. 正确性

1. branch-junction 只合并真实 paid path、parent-tree edges 和 group shortest paths，所得值必为可行树 upper。
2. upper 只通过 `best=min(best,candidate)` 使用，不会删除任何 lower-bound 允许的最优状态。
3. work credit 只改变 exact rows 与 exact global 的切换时刻；ReleaseV3 global 自身完整，不依赖已生成 rows。
4. prepared-distance 接口只避免重复计算 group distances；正式 `SolveOneQuery` 仍走原 ReleaseV3 路径，并在接口重构后通过随机 DPBF `100/100`。

最终 Release/O2 对拍：

```text
seed 713801  g=2..10   200/200
seed 713803  fixed g13  50/50
```

此前未计费实现还通过 `300/300 + fixed g13 100/100`；这些结果支持 junction 实现，但当前代码以最后两组对拍为准。

## 6. Fast20

快照：`result_snapshot/fast/20260713_161638`。ReleaseV3 对照：`20260711_230000`。

| dataset | Test79 | ReleaseV3 | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `1.444s` | `1.493s` | `0.967x` |
| Toronto-new | `4.403s` | `4.704s` | `0.936x` |
| DBLP | `0.581s` | `0.581s` | `1.000x` |
| DBLP-new | `0.415s` | `0.484s` | `0.858x` |
| MovieLens | `4.947s` | `5.247s` | `0.943x` |
| **total** | **`11.790s`** | **`12.790s`** | **`0.922x`** |

20 条权重全部一致。最大单进程峰值为 MovieLens g12 的 `183.6MiB`，与 V3 `183.5MiB` 基本相同；Toronto 两版峰值下降，DBLP 两版没有系统性增加。

## 7. Full DBLP 分解

```text
weight                    12.5936282853
wall                         521.055s
peak                        3744.6MiB
junction upper                13.0199888930
junction wall                  3.084s
junction work                750,772,703
total row work             1,764,703,964
buy work                  1,761,175,858
released row bytes           118,268,456
switch                         size 2, mask 6
global settled                11,192,686
global peak open              18,429,024
```

互斥阶段近似为：

| component | Test79 | ReleaseV3 |
| --- | ---: | ---: |
| group distances | `16.933s` | `17.430s` |
| junction | `3.084s` | `0` |
| rows excluding dual | `23.499s` | `44.108s` |
| dual | `35.940s` | `35.223s` |
| global | `436.665s` | `429.785s` |
| solver total | `520.097s` | `531.556s` |

global settled 没有下降；收益来自 junction 的已付工作缩短 row rent，约省 `20.6s` rows，扣除 `3.1s` junction 后仍覆盖本次 global 波动。不能把这次加速表述为 junction 直接压缩了 B frontier。

## 8. 冻结位置

| 文件 | 责任 |
| --- | --- |
| `archive/test79_v3_junction_control_20260713.md` | 本历史说明与完整数字 |
| `result/DBLP/Test79`、`result/Toronto/Test79` | 已完成的原始结果 |
| `methods/Common/anchor_junction_upper.*` | 只保留与 B 无关的 junction 可行上界，供 Test80/A 使用 |

## 9. 论文关系

- [Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302)：rooted subset recurrence 的经典来源；压缩父树上的 facility subset DP 是本仓库组合，不宣称已完成原创性检索。
- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：goal-oriented Dijkstra-Steiner 与 admissible future 的理论背景；V3 global 延续该接口。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 来源；GST group potential 是仓库适配。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：统一 GST baseline；Test79 不把普通图/query 压缩计作自身贡献。

Test79 的具体候选是 paid-anchor junction、压缩 parent-tree upper 与统一实际工作 rent-or-buy 的组合。本文只陈述仓库设计与实验结果，不在系统文献检索完成前声称论文级原创。
