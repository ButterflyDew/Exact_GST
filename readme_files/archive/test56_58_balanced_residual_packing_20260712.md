# Test56--58：Balanced Anchor-Path Residual Packing

更新时间：2026-07-12。Test56--58 是 Test54--55 sequential scalar-cap 失败后的同步 residual 路线。Test56 在正式 root dual 的剩余 residual 上，同时构造所有 groups 的 anchor-path 增量势，以全局最大可行统一 scale 一次性 packing；Test57 用实际 D2 工作无参数购买，并接回 Test48 branch-junction upper；Test58 把 uniform packing 扩为 max-min progressive filling。该系列在 fast20 与 Toronto full 取得当前研究线最好结果，full DBLP peak 也降到约 2.05GiB；但两次有效 DBLP gate 均未在 V3 `531.556s` 内产生最终 weight，因此所有代码/API/统计字段最终撤回。

> 后续状态：这组已证明机制已在独立纯 A 的 Test80 中重建；当前 full 于 `666.509s / 2161.9MiB` 完成。本文保留的是旧 531 秒门禁证据，不覆盖 Test80 的完成结果。

## 1. Test56：统一同步 Packing

先构造正式 root-star directed-cut dual，保留其 residual `r_e`。对每个 group `i`，在该共同 residual graph 上计算距离，截断到 paid anchor path `P`：

```text
q_i(v) = min(d_i^r(v), max(p in P) d_i^r(p)).
```

令 `a_{e,i}` 为 `q_i` 在 directed arc `e` 上的正梯度。统一 scale 为：

```text
lambda = min_e r_e / sum_i a_{e,i}.
```

于是 `lambda q_i` 的总梯度不超过每条 arc 的剩余 capacity，可直接加到原 potential row；future query 仍只做一次 group subset sum。它与 group order 无关，没有第二 dual，也没有经验系数。

随机 `300/300`（seed `713181`）通过。无条件版本五库 D2 states 全部下降，fast DBLP/MovieLens values 分别到正式版 `0.429x/0.325x`；但第二轮 residual SSSP 与 `g*n` 临时 rows 使 fast20 为 `15.132s`，MovieLens peak 升到约 `218MiB`。Toronto full 为：

```text
20.992s / 100.8MiB
formal Test21 20.788s / 102.9MiB
```

状态与 peak 改善真实，但固定构造成本不回本。

## 2. Test57：D2-Work 延迟购买

augmentation 必做的静态工作事件为：

```text
budget = g * (2m + n)
```

ordinary D2 实际累计：

- 全顶点 seed checks；
- heap pushes/pops；
- settled vertex adjacency scans。

仅当累计事件达到 budget、且仍有未处理 pair rows 时购买 packing。该规则不读取数据集、`g` 特例、density threshold 或 wall time。MovieLens 与 fast DBLP 因 D2 工作不足自然不买；Toronto 两版购买。

单独接入 Test56 时 fast20 从 `15.132s` 恢复到 `12.610s`。随后恢复 Test48 的 branch-junction upper：只扫描 triple argmin roots，在其到 paid anchor path 的压缩父树上做 subset facility DP。组合版通过随机 `300/300`（seed `713201`）：

```text
fast20                 9.626s
Test48                 9.758s
formal Test21         12.380s
Toronto full           9.403s / 58.1MiB
formal Toronto full   20.788s / 102.9MiB
```

Toronto full 在第 `30/66` 个 pair 后购买；junction upper 为 `0.8334348456`，ordinary values 从 `6.575M` 降到 `2.709M`。

## 3. Test58：Progressive Filling

uniform lambda 只完成 max-min packing 第一轮。Test58 继续 progressive filling：

1. 所有 active groups 同速增长；
2. 某条 residual arc 饱和时，冻结在该 arc 上有正梯度的 groups；
3. 其余 groups 继续增长；
4. 最多 `g` 轮，直到全部冻结或 scale 达到 1。

每轮都只消耗尚存 residual，故最终 group-specific scales 仍形成合法、order-independent packing。随机 `300/300`（seed `713211`）通过。Toronto g12 用两轮把 min/average/max scale 从 uniform `0.0948` 扩为 `0.0948/0.1702/1.0`。

结果：

```text
fast20                 9.390s
Toronto full           9.201s / 58.0MiB
Toronto full rounds    3
Toronto scales         min 0.0610 / avg 0.0872 / max 0.3293
```

这是 Test21 研究线当前最好的 fast 与 Toronto full 结果。

## 4. Full DBLP 硬门禁

所有有效运行均以前台 solver 启动，从本次 `weights.txt` header 写入后计 query wall，并由独立监控在 V3 `531.556s` 附近强制停止。

第一次用 `Start-Process` 的 Test57 monitor 没有活跃 solver，stdout/stderr 为空，属于无效空跑，不计结果。随后两次有效 gate：

| candidate | stop wall | sampled peak | final weight |
| --- | ---: | ---: | --- |
| Test57 uniform delayed packing | `531.952s` | `2287.8MiB` | absent |
| Test58 progressive filling | `531.671s` | `2094.1MiB` | absent |

两次 CPU/elapsed 轨迹在约 476 秒时几乎相同；Test58 主要继续降低 peak，没有让高层 exact search 在门禁前完成。因此不能声称 DBLP 不退化，也不能因为只差不到一秒的监控采样误差而保留：V3 在 `531.556s` 已有完整结果，这两个候选都没有最终 weight。

## 5. 结论

1. 同步 residual packing 解决了 Test54--55 的跨库状态反向；五库 D2 states 均不增加。
2. branch-junction upper、实际工作购买和 progressive filling 可以自然组合，fast/Toronto 收益很强。
3. full DBLP peak 从正式 Test21 约 `4.97GB` 降到 `2.05GiB`，但时间门槛仍失败；当前瓶颈已经不只是 D2 payload 或 augmentation 构造。
4. 下一机制必须减少购买后的高层 ordinary/A exact states，或改变 half completion 的状态接口；继续强化同一 residual packing、调 budget、增加 progressive rounds都不构成新主线。

撤回后已删除 junction helper、balanced residual 公共 API、D2-work 购买逻辑、统计字段与 CMake 入口。正式 Test21 重新通过 Release/O2 编译及随机 DPBF 对拍 `100/100`（seed `713221`）；未再次运行 full DBLP。

Test56--58 没有直接采用新的论文算法。directed-cut residual 背景仍来自 Wong；同步 uniform packing、实际 D2-work 购买和 progressive filling 是本轮仓库候选，尚未完成系统文献检索，因此不宣称论文级原创性。
