# Half / Global 混合探针日志（2026-07-10）

本文保存 `../history/half_global_hybrid.md` 的详细数字和失败路线。它不是发行成绩；除明确写出 full 的条目外，所有 dataset 运行均为 snapshot query 1、Release/O2。

## 1. Naive half 与 anchored

fast g12：

| dataset | anchored wall / settled / peak | dynamic half wall / settled / peak |
| --- | --- | --- |
| Toronto | `1.556s / 424159 / 228466` | `3.295s / 543095 / 206002` |
| Toronto-new | `4.459s / 1129033 / 370901` | `7.749s / 1229806 / 407741` |
| DBLP | `0.347s / 19618 / 10725` | `0.516s / 30967 / 16219` |
| DBLP-new | `0.349s / 33100 / 65030` | `0.462s / 45116 / 76318` |
| MovieLens | `1.677s / 10882 / 2672` | `1.750s / 11059 / 3018` |

Toronto full-size snapshot g13 q1：anchored `4.317s / 771315 settled / 606284 peak`；half `7.980s / 990006 / 590553`，权重同为 `0.704846702`。这不是 full DBLP。

动态 half 的 Toronto g12 还产生 `33,120,556` 个 disjoint pairs、`1,101,700` 次 two-block 更新和 `312,693` 次 completion，说明三块 hash 不是免费附属操作。

## 2. 结束时离线三块

只在 A* 停止点把 settled labels 转成 mask-major、有序 root rows，再做二/三行交集：

| dataset | wall | settled | offline checks |
| --- | ---: | ---: | ---: |
| Toronto | `4.969s` | `896310` | `93,990,284` |
| Toronto-new | `14.567s` | `2553997` | `198,249,858` |
| DBLP | `0.536s` | `30967` | - |
| DBLP-new | `0.553s` | `52120` | - |
| MovieLens | `1.669s` | `11059` | - |
| **total** | **`22.294s`** | **`3544453`** | **`292,240,142`** |

离线交集本身约 `2.817s`，更大的退化来自没有早期 ternary `best`，导致搜索继续定型状态。结论是“离线表示正确”，不是“只在末尾离线即可”。

## 3. Bare rental 信号

small bare-anchored 的累计工作 `edge_relax + disjoint` 与完整 `g*m` 的比值：

| g | work / (`g*m`) |
| ---: | ---: |
| 2 | `0.0409` |
| 3 | `0.552` |
| 4 | `2.007` |
| 5 | `3.731` |
| 6 | `6.708` |

它支持 rent-or-buy 研究：小查询可能在购买重 potential 前结束，较大查询会自然越过一次全图构造成本。不能把该表改写为 `g>=4` 开关。

## 4. Metric-only 与 threshold envelope

metric-only 使用提前终止的精确组间距离、group-TSP/2、真实 greedy upper、half global labels，以及按“缺 1/2/3 块”维护的 `base + max(C,k*lambda)` 精确停止包络。

g4 分阶段：

| dataset | bounds | upper | search | wall | settled | edge relax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto | `0.50ms` | `0.30ms` | `10.28ms` | `14.51ms` | `4573` | `13812` |
| Toronto-new | `0.90ms` | `0.70ms` | `11.38ms` | `14.31ms` | `15307` | `47606` |
| DBLP | `3.59ms` | `9.74ms` | `2.71ms` | `16.91ms` | `1200` | `98226` |
| DBLP-new | `1.53ms` | `0.32ms` | `1.04ms` | `3.39ms` | `114` | `3206` |
| MovieLens | `44.02ms` | `55.48ms` | `13.27ms` | `113.28ms` | `503` | `1381184` |

group-TSP/2 相比 MST/2 把第一版 MovieLens g4 的 `27.9M` edge relax 降到 `1.38M`，但 bounds + upper 仍占约 `88%`。g5--g8 的 MovieLens wall 为 `0.127/0.190/0.630/1.509s`，其余库也从 g6 起快速扩张。因此 metric-only 不进入主线。

threshold envelope 正确性：随机 `g2..10` seed `711241` 为 `5000/5000`；固定 g13 seed `711243` 为 `100/100`。它没有改变五库 g4 的最终轨迹，因为局部 TSP pruning 已在相同边界耗尽 open labels。

## 5. 双向组对距离

独立多源双向 Dijkstra 用 `min_front_a + min_front_b >= best_pair` 精确停止。g4：

| dataset | 6 pairs wall | edge relax |
| --- | ---: | ---: |
| Toronto | `0.573ms` | `4028` |
| Toronto-new | `0.772ms` | `7392` |
| DBLP | `0.976ms` | `78739` |
| DBLP-new | `0.199ms` | `5116` |
| MovieLens | `27.223ms` | `8481271` |

MovieLens g4--g8 分别为 `24.6/36.8/47.3/62.0/75.6ms`。它优于完整 group distances 的对应阶段，但仍不足以单独达到 small 的数量级目标。用最短组对路径再 greedy 接组的上界质量跨数据不稳，源码已撤回。

## 6. Goal-root-star

最终保留版本通过随机 `5000/5000`，seed `711273`。三轮代表中位数汇总：

| g | five datasets | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 2 | `5.57ms` | `147.65ms` | `26.5x` |
| 3 | `23.15ms` | `168.71ms` | `7.3x` |

`g=3` MovieLens 单条约 `15.2ms`、`5,354,577` edge relax，是当前小 g 的主要剩余成本。

## 7. 两级 Rent-or-Buy

distance-epoch row probe 的统一工作口径：

```text
row_work = root intersection estimate
         + actual adjacency scans
         + (heap push + pop) * ceil(log2 n)
         + actual complement rent/build
buy_work = 2g(m + n ceil(log2 n))
```

第一次达到 `buy_work` 时动态构造 dual，保留并 compact 已有 rows；dual 后新增 row work 再达到一次 `buy_work` 时报告 global 升级。没有倍率参数。

small 的第一次升级分布：g4/g5 全部离线完成；g6 仅 Toronto-new；g7 两个 Toronto；g8 为 Toronto、Toronto-new、DBLP-new、MovieLens。dynamic-dual five-dataset 总计：

| g | dynamic dual | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 4 | `75.19ms` | `182.63ms` | `2.4x` |
| 5 | `102.29ms` | `231.38ms` | `2.3x` |
| 6 | `152.32ms` | `348.37ms` | `2.3x` |
| 7 | `312.88ms` | `2864.60ms` | `9.2x` |
| 8 | `868.59ms` | `7513.43ms` | `8.6x` |

fast20 中，第一预算除 DBLP g9 外均触发；dynamic-dual 完整跑完约 `15.25s`。第二预算结果为 14 条请求 global、6 条直接完成：DBLP g9/g10 和 MovieLens g9--g12。14 条停止项的累计 rental wall 约 `0.80s`；把它们从头交给 ReleaseV2 的保守组合约 `13.4s`，尚未扣除可共享的 group distance/TSP/dual，也尚未实现真正接续。

代表停止点：Toronto g12 在 `k2 mask 22`、约 `52.6ms` 请求 global；Toronto-new g12 在 `k2 mask 16`、约 `55.1ms`；DBLP g12 在 `k3 mask 136`、约 `105.5ms`；DBLP-new g12 在 `k2 mask 66`、约 `70.4ms`。MovieLens g12 在第二预算前完成，约 `1.43s`。

动态 dual 正确性：随机 `g2..10` seed `711301`，`1000/1000` 同时比较默认 distance solver、dynamic solver 与 DPBF。

## 8. 未执行项

- 没有创建新的 release 源码。
- 没有运行 full DBLP g13；最后可信 full 仍是 ReleaseV2 precursor 的 `1427.625s / 9.015GiB`。
- 没有把普通图/query 压缩加入任何新探针。
- 没有保留数据集名、固定 `g`、固定层、固定 rows 或运行时刻分支。

## 9. 零下界停止包络

已有 metric threshold 的证明并不要求 completion 下界严格为正。令所有尚未得到的块下界为 `0`，仍可按当前最小未定型代价 `lambda` 维护：

```text
one settled block + one missing block: base + lambda
one settled block + two missing blocks: base + 2 lambda
three missing blocks:                 3 lambda
```

取所有可行二/三块分解的最小值仍是 exact 全局停止下界。它通过随机 `g=2..10` seed `711421` 的 `5000/5000` 和固定 g13 seed `711423` 的 `100/100`。但 five-dataset g4 wall 约为 `11.1/23.4/48.3/18.8/770.8ms`；MovieLens 仍扫描 `31.73M` 条邻接边。结论是证明保留，零下界本身不足以替代强 potential。

## 10. 搜索内生组度量

singleton label 首次在另一组顶点定型时，精确给出对应组对距离。探针在收齐所有组对后回灌 group-TSP/2、重建二/三块停止包络，并用 tentative singleton labels 生成真实同根上界。正确性为随机 `g=2..10` seed `711441` 的 `5000/5000`、固定 g13 seed `711443` 的 `100/100`。

g4 的决定性负例是 MovieLens：最后一个组对直到 `17,771` labels、`30.920M` edge relax 后才定型，最终 wall `809.5ms`。它没有消除预处理，只把独立组距离推迟成更昂贵的 label 搜索，因此实现开关撤出。

## 11. 根截断势函数

对确定性查询根 `r`，可不计算完整组距离，只保留：

```text
p_a(v) = min(d_a(v), d_a(r)).
```

多源 Dijkstra 在 `r` 定型时即可停止；根半径外统一取 `d_a(r)`。共享 residual 的版本与 Wong-style dual 使用同一势函数语义，独立版本则只能安全取 `max_a p_a(v)`，不能把各组独立容量直接求和。两者均满足 admissibility、edge consistency 与 subset splice，并分别通过：

| mode | random g2--10 | fixed g13 |
| --- | --- | --- |
| shared capped dual | `5000/5000`, seed `711461` | `100/100`, seed `711463` |
| independent capped max | `5000/5000`, seed `711471` | `100/100`, seed `711473` |

g4 five-dataset wall：

| mode | Toronto | Toronto-new | DBLP | DBLP-new | MovieLens | total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| shared capped dual | `5.6ms` | `11.1ms` | `26.1ms` | `12.2ms` | `145.8ms` | `200.8ms` |
| independent capped max | `3.8ms` | `10.7ms` | `32.2ms` | `3.0ms` | `171.9ms` | `221.6ms` |

共享版本在 MovieLens 只定型 `992` 个 half labels，说明势函数很强；但 capped build `98.4ms`、搜索 `46.7ms`，仍慢于 ordered rows 的约 `60.4ms`。独立 `max` 势 build 降到 `26.6ms`，搜索却回升到 `144.5ms`。两条路线都没有达到 small 门槛，源码开关撤出；保留的有效信息是“截断势强度足够，当前共享容量构造成本不够低”。

## 12. 截断 dual + ordered half rows

独立 probe 曾把共享截断 dual 与纯 `(|S|,mask)` row recurrence 直接组合：singleton 也物化为有序稀疏 row；每个 row 的 dual 势按 vertex 只计算一次；二/三块 completion 预先分配给最后就绪的 row；全程没有 `(root,mask)` hash。它通过随机 `g=2..10` seed `711501` 的 `5000/5000` 和固定 g13 seed `711503` 的 `100/100`。

g4 结果：

| dataset | total | dual | rows | row edge relax |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `3.27ms` | `0.65ms` | `2.60ms` | `26,436` |
| Toronto-new | `4.29ms` | `1.02ms` | `3.25ms` | `34,550` |
| DBLP | `15.06ms` | `5.89ms` | `9.16ms` | `737,852` |
| DBLP-new | `3.21ms` | `1.28ms` | `1.91ms` | `144,648` |
| MovieLens | `155.17ms` | `81.39ms` | `73.77ms` | `10,903,094` |

五库合计约 `181.0ms`，明显慢于当前 dynamic-dual ordered-distance 路线约 `75ms`。关键不是 merge/hash，而是每个 pair row 重新传播稠密图边；global capped-half 的同一 MovieLens 查询只扫描约 `2.81M` 条搜索边。结论：离线有序 rows 必须复用便宜的完整 singleton 距离和强 TSP，或与全局传播共享 closure；不能简单把全局 labels 再拆成逐 row Dijkstra。probe 源码与 CMake 入口已删除。

## 13. 第一次预算立即转 global

共享 hybrid 原型通常在第一次 `row_work>=buy_work` 时构造 dual，再租用一份 `buy_work` 后才转 global。曾测试在第一次事件构造 dual 后立即转入同一个共享 global；small 未触发预算，语义不变。该模式通过随机 `g=2..10` seed `711521` 的 `3000/3000` 和固定 g13 seed `711523` 的 `100/100`。

paired fast20：

| g | ReleaseV2 | early global | ratio |
| ---: | ---: | ---: | ---: |
| 9 | `1.041s` | `1.289s` | `1.239x` |
| 10 | `1.490s` | `1.829s` | `1.228x` |
| 11 | `2.608s` | `2.877s` | `1.103x` |
| 12 | `7.681s` | `7.909s` | `1.030x` |
| **total** | **`12.820s`** | **`13.905s`** | **`1.085x`** |

等待第二预算的共享 hybrid 约慢 ReleaseV2 `5.2%`，early 反而慢 `8.5%`。MovieLens g9--g12 在 rows 路线可直接完成，early 却全部转入 global，单条慢约 `20%--36%`。因此第一预算只购买 dual、第二预算才购买 global 的两级事件有实际必要性；early 模式源码已撤回。

## 14. 精确 singleton row 注入 global

global 已经持有完整 `gd[a][v]`，所以 singleton label 的精确值可直接读取，不必再次从组顶点传播。探针把所有能通过当前 bound 的 singleton `(a,v)` 预插入 heap，并在其定型时跳过边传播；pair 及以上 labels 完全不变。它通过随机 `g=2..10` seed `711561` 的 `3000/3000` 和固定 g13 seed `711563` 的 `100/100`。

fast g12 paired：

| dataset | ReleaseV2 | exact-singleton hybrid | ratio | inserted singleton | settled |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `1453.4ms` | `1398.7ms` | `0.962x` | `19,315` | `424,159` |
| Toronto-new | `4022.1ms` | `4062.9ms` | `1.010x` | `29,333` | `1,129,033` |
| DBLP | `325.2ms` | `371.4ms` | `1.142x` | `1,490` | `19,618` |
| DBLP-new | `332.8ms` | `375.0ms` | `1.127x` | `4,477` | `33,100` |
| MovieLens | `1602.0ms` | `1440.9ms` | rows 完成 | `0` | `0` |

四条 global 查询的 settled trajectory 与 ReleaseV2 完全相同；预插入只把 singleton 的边传播换成 `g*n` 扫描、hash 插入和 heap 项，peak open 还略升。五库没有稳定净收益，因此未继续跑 g9--g11，源码模式已撤回。有效边界是：`gd` 的确与 singleton recurrence 重复，但要获益必须提供惰性有序视图，不能把完整 row 一次性灌入 global。

## 15. 惰性 singleton key stream

为避免第 14 节的批量 hash，后续把所有通过 bound 的精确 singleton 保存为轻量 `(key,cost,root,mask)` 外部 heap。global 主循环比较 composite heap 与 singleton heap 的最小 key；singleton 真正轮到时才写入 root-local label、直接定型且不传播边。该模式通过随机 `g=2..10` seed `711581` 的 `3000/3000` 和固定 g13 seed `711583` 的 `100/100`。

fast20 相对普通 shared hybrid 约快 `1.5%`，与同批 ReleaseV2 基本持平；候选最多约 `29k`，heapify 每条 global 查询约 `3--6ms`。但 full-size Toronto g13 q1 为：

```text
ReleaseV2               3707.5ms
lazy-singleton hybrid   4448.3ms
ordinary hybrid         4339.7ms
singleton candidates    121,609
singleton build         99.6ms
main peak open           606,284 -> 593,016
settled                  771,315 (不变)
```

惰性视图降低主 heap 峰值，却在较大 `n` 上增加总 wall；没有无参数证据能保证其收益。不能按图规模或数据集启用，源码模式已撤回。保留的研究结论是：完整 `gd` 与 singleton 传播确有语义重复，但轻量外部排序本身仍不是免费共享。

## 16. 并行 group distance 与初始上界门控

为确认 `g=3..6` 的剩余时间是否只是串行预处理常数，探针曾并行运行每组的独立多源 Dijkstra。另一个实验用统一工作量比较决定是否构造初始 greedy upper：预计 dense half-join 工作不小于一次 greedy 构造工作时才构造，否则只保留 root-star upper。两者都没有数据集名、固定 `g`、层级或经验倍率；组合模式通过随机 `g=2..10` seed `711641` 的 `1000/1000` 和固定 `g13` seed `711643` 的 `50/50`。

五库顺序运行、每项三次取中位数后，组合模式为：

| g | five datasets | PrunedDP | speedup |
| ---: | ---: | ---: | ---: |
| 3 | `24.37ms` | `168.71ms` | `6.9x` |
| 4 | `40.28ms` | `182.63ms` | `4.5x` |
| 5 | `46.93ms` | `231.38ms` | `4.9x` |
| 6 | `103.46ms` | `348.37ms` | `3.4x` |

高 `g` 的一次五库并发筛查为 `g7=277.21ms`、`g8=755.02ms`，约 `10.3x/10.0x`，但并发测量不作为正式成绩。低 `g` 仍明显没有达到逐 `g` 的 `10x` 门槛；MovieLens `g4` 的顺序中位数单项仍为 `26.44ms`。

上界门控也不是单调收益：Toronto-new `g6` 跳过 greedy 后触发 shared global，单项约 `35.68ms`。这说明只比较构造工作与静态 dense-join 上界，遗漏了更好 `best` 对保留状态和阶段切换的价值。并行 Dijkstra 又是 PrunedDP 同样可采用的通用工程优化，不能作为本方法贡献。故 `std::async`、上界门控、全部 CLI 模式和临时计数器均已从主探针撤回；只保留“低 `g` 瓶颈确在最短路传播，且普通并行不足以跨越门槛”这一负结论。

## 17. Delayed greedy upper 的 rent-or-buy

静态跳过 greedy 的问题不是“不能延迟”，而是没有把弱 `best` 造成的额外 row 工作计入决策。新模式先只构造 root-star upper，并累计与两级升级相同口径的实际 `row_work`；达到一次 greedy 的最坏工作上界时才构造 greedy：

```text
greedy_buy_work = g * (m + n*ceil(log2 n))
```

一次 greedy 最多连接 `g` 个新组，每轮是一次 priority-queue 多源 Dijkstra，因此实际工作不超过该口径。若 rows 在购买前精确完成，则不再需要 greedy；若未完成，已付 rental 不超过一次 buy，构造后立即按新 `best` compact。它与后续 `dual_buy_work=2*greedy_buy_work`、第二次 dual 预算转 global 形成三个按实际工作的事件，没有经验倍率或固定 `g`。

正确性不依赖上界质量：root-star 和 greedy 都是真实可行树；较弱上界只会暂时多保留 exact rows。模式通过随机 `g=2..10` seed `711681` 的 `1000/1000` 和固定 `g13` seed `711683` 的 `50/50`，均在 `1e-6` 内等于 DPBF。

small 五库对普通 shared hybrid 交替运行三轮、每项取中位数：

| g | ordinary hybrid | delayed upper | ratio | delayed / PrunedDP speedup |
| ---: | ---: | ---: | ---: | ---: |
| 4 | `81.60ms` | `59.47ms` | `0.729x` | `3.07x` |
| 5 | `100.26ms` | `82.10ms` | `0.819x` | `2.82x` |
| 6 | `155.06ms` | `148.33ms` | `0.957x` | `2.35x` |
| 7 | `369.63ms` | `358.98ms` | `0.971x` | `7.98x` |
| 8 | `864.79ms` | `892.48ms` | `1.032x` | `8.42x` |

fast20 单轮交替 A/B 为：

| dataset | ordinary hybrid | delayed upper | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `1.757s` | `1.691s` | `0.963x` |
| Toronto-new | `6.060s` | `6.033s` | `0.996x` |
| DBLP | `0.659s` | `0.673s` | `1.022x` |
| DBLP-new | `0.739s` | `0.738s` | `0.999x` |
| MovieLens | `4.434s` | `4.309s` | `0.972x` |
| **total** | **`13.648s`** | **`13.445s`** | **`0.985x`** |

这是可保留的统一正结果，但还不是候选 release：`g4..6` 仍只有约 `2.4x--3.1x`，`g8` 有轻微退化，且尚未测统一 RSS。它只减少不必要的初始上界工作，没有消除完整 group distances 的低 `g` 底座。本轮没有运行 full DBLP g13。

## 18. Progressive upper 的必要条件探针

为判断能否在完整 `gd` 前结束，先测了 bare-half 首次产生可行树的位置。g4 的 `(first best edge relax / final edge relax)` 为：Toronto `8,846/28,064`、Toronto-new `27,351/71,210`、DBLP `331,912/808,947`、DBLP-new `8,184/283,926`、MovieLens `23,065,406/31,732,028`。除 DBLP-new 外，首解本身已经太晚；MovieLens 不能采用“先 bare，得到 best 后再购买距离证书”。

随后把 goal-root 停止证书泛化到任意 `g<=20`。它始终精确计算：

```text
min_v sum_a gd[a][v]
```

但只有 `g<=3` 时等于 GST 真值；更大 `g` 时是合法 root-star upper。MovieLens g4--g8 的 wall 为 `37.1/47.7/56.5/74.4/76.5ms`，边扫描为 `11.87M/15.84M/19.81M/23.78M/27.75M`。它少于完整 `gd`，但 g4 单独已超过相对 PrunedDP `10x` 的整项预算，因此只能作为独立证书工具，不能成为低 `g` 必经预处理。

另一个探针从确定性组顶点只做一次 Dijkstra，把到各组首个命中的 shortest-path-tree 路径取并，得到合法 SPT upper。它通过随机 `g=2..10` seed `711701` 的 `1000/1000` 和固定 g13 seed `711703` 的 `50/50`。MovieLens g4 的 upper 构造仅 `10.2ms`，初值 `0.1325933524`，接近最优 `0.1286234523`；但 zero-potential half search 的 settled 与边扫描仍和 bare 完全相同，最终约 `729.8ms / 31.73M`。上界只删除 open 候选，不能抬高按原始 cost 定型的停止下界。SPT 模式和 CLI 已撤回。

结论：下一步必须是**无需完整 `gd` 的 mask-aware admissible potential**，不能继续只换上界，也不能先支付 bare 搜索。该 potential 至少要在 MovieLens g4 的 `10--15ms` 总预算内显著减少 pair-label 边传播；否则逐 `g` 数量级目标在当前基线下不可达。

## 19. Packed moat potential

尝试用一次全组多源 Voronoi 构造无需完整 `gd` 的 owner-additive cut potential。对每个不属于重叠组分量的组定义：

```text
r_a = 0.5 * min_{b != a} dist(A_a,A_b)
p_a(v) = min(dist(v,A_a), r_a)
h(v,R) = sum_{a in R} p_a(v)
```

重叠分量内的组取 `r_a=0`。任意组对都有 `r_a+r_b<=dist(A_a,A_b)`，因此正半径 balls 不相交；一条有向边至多承受一个 owner 的正势差，shared arc capacity 不超边权。由同一 owner-cut 论证，`h` admissible、edge-consistent 并满足 subset splice。Voronoi 表示只保存每点最近 owner/distance，mask 查询为 `sum_radius[R]` 减去至多一个 owner discount，空间 `O(n+2^g)`。

最初错误地把 Voronoi 组边界做 MST 并回溯路径作为 primal。随机 seed `711721` 第 11 个反例得到伪 upper `7`，而 DPBF 为 `10`：同一组的不同候选点被 group supernode 错当成免费连通。组汇点只可用于 cut relaxation，不能用于 primal 图内连通。该恢复已删除，正确性测试改用真实单根 SPT 路径并集上界。

修正后，非均匀 moat 模式通过随机 `g=2..10` seed `711731` 的 `2000/2000` 和固定 g13 seed `711733` 的 `50/50`。g4 五库 wall 约为 `10.1/23.7/20.5/6.0/677.9ms`；MovieLens 构造 `27.0ms`、半径和 `0.06424`，搜索仍定型 `14,713` labels、扫描 `27.90M` 条边。对比 bare 的 `18,119 / 31.73M` 只减少约 `12%` edge work，远慢于 delayed ordered rows。

结论：半径 packing 是简洁且正确的 GST 专用势，但最近组约束过保守，无法替代完整 group-distance/TSP。源码类、统计字段与 CLI 已撤回；保留的理论信息是“不相交 owner moats 可用 `O(n+2^g)` 表示”，以及 group supernode 不能恢复 primal 的反例。

## 20. 放宽低 g 后的一阶段候选审计

用户允许少量从理论底座上难以达到 `10x` 的 `g` 放宽要求，但必须逐 `g` 透明报告。候选执行路径改为：delayed greedy 仍按一次最坏构造工作购买；rows 的累计工作第一次达到 `dual_buy_work` 时，立即构造 dual、释放 rows 并接续 shared anchored global，不再租用第二份 `dual_buy_work`。这是统一的一阶段 rent-or-buy，不含固定 `g` 或数据集判断。

正确性：随机 `g=2..10` seed `711781` 为 `1000/1000`，固定 g13 seed `711783` 为 `50/50`。small35 一阶段结果：

| g | candidate | PrunedDP | speedup | candidate solver increment sum | Pruned increment sum |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | `6.43ms` | `147.65ms` | `23.0x` | `~0MiB` | `83MiB` |
| 3 | `26.40ms` | `168.71ms` | `6.4x` | `3MiB` | `85MiB` |
| 4 | `85.06ms` | `182.63ms` | `2.1x` | `1MiB` | `88MiB` |
| 5 | `82.08ms` | `231.38ms` | `2.8x` | `2MiB` | `96MiB` |
| 6 | `162.65ms` | `348.37ms` | `2.1x` | `6MiB` | `112MiB` |
| 7 | `352.66ms` | `2864.60ms` | `8.1x` | `5MiB` | `213MiB` |
| 8 | `961.12ms` | `7513.43ms` | `7.8x` | `43MiB` | `313MiB` |
| **total** | **`1.676s`** | **`11.457s`** | **`6.83x`** | **`60MiB`** | **`990MiB`** |

完整进程 peak 的逐 g 比例只有约 `1.5x--2.3x`，因为 MovieLens 公共图本身约 `147MiB`。g8 的主要增量是 dual residual：MovieLens 从 `147.3MiB` 到 `182.5MiB`，PrunedDP 同项到约 `354.1MiB`。因此空间结论必须同时给出完整进程与 solver 增量，不能把公共图重复归因于算法。

fast20 一阶段总计 `13.716s`，solver 增量峰值和约 `467MiB`。PrunedDP 完成的 19 条已有 `279.869s`，第 20 条 MovieLens g12 至少约 `100s` 超时；相同 19 条 PrunedDP 完整 peak 和 `6946MiB`、增量和 `6418MiB`，候选去掉自身 MovieLens g12 后约 `956MiB / 430MiB`。因此相同口径为时间至少 `23.4x`、完整 peak 约 `7.3x`、solver 增量约 `14.9x`；20 条时间严格大于 `27.7x`。

full Toronto g13：

```text
ReleaseV2                3.953s
two-stage delayed        4.660s, switch k2 mask44
one-stage delayed        4.008s, switch k2 mask9
weight                   0.704846702 (all equal)
one-stage sampled peak   134.4MiB
```

full DBLP g13 做了两次有界而非完整运行。两阶段 180 秒在 `k2 mask10` 前后，`best=15.0174`，采样峰值 `2009.9MiB`。一阶段 240 秒只输出进入 k2，采样峰值 `6973.0MiB`；结合一阶段会在第一次 buy 后释放 rows，这与进入 global 相符，但当时尚无显式 switch 日志，不能作为已切换或已完成的直接证据。随后 verbose 模式已增加 `buy_greedy/buy_dual/switch_global` 事件行，未再次运行 DBLP。

当前判断：一阶段比两阶段 small 慢约 `8.5%`、fast 慢约 `2.3%`，但 full Toronto 快约 `14%`，结构更简单，并更早保护 DBLP。它是下一干净候选；ReleaseV2 的 `1427.625s / 9231.5MiB / 12.5936282853` 仍是唯一 full DBLP 完成证据，不能把 bounded 轨迹改写成新算法完成成绩。

## 21. Farthest-goal anchor 与 ReleaseV3

anchored recurrence 原先固定输入第一组。fast g12 枚举全部 12 个 anchor 后，事后最优组与组大小没有稳定关系；“选最大组”在 DBLP-new 反向 `25.3%`，因此不能启用。随后诊断 root-star 根距离、dual 单组分量和 group-metric 中心性。唯一跨五库不反向的简单规则是：

```text
anchor = argmax_a gd[a][root_star_root]
```

在当前 dual 构造中，该组通常也是 root 上最大的单组 dual 分量。它把 root-star 视角下最难组从显式 mask 维度变成 permanent goal，同时让该组继续留在每个 future remaining set 中。规则不试跑多个搜索，不含经验参数，且只使用方法已有的 `gd`。

fast g12 相对固定第一组的 settled 比例：

| dataset | fixed first | farthest-goal | ratio |
| --- | ---: | ---: | ---: |
| DBLP | `19,618` | `7,655` | `0.390x` |
| DBLP-new | `33,100` | `12,057` | `0.364x` |
| MovieLens | `10,882` | `10,882` | `1.000x` |
| Toronto | `424,159` | `372,598` | `0.878x` |
| Toronto-new | `1,129,033` | `747,129` | `0.662x` |

五库合计从 `1,616,792` 降到 `1,150,321`。用于筛选规则的 `anchor_root_distance/dual/metric` 临时字段随后从 `global_half_probe` 撤回。

正确性：candidate 内部随机 `g=2..10` seed `711841` 为 `3000/3000`，固定 g13 seed `711843` 为 `100/100`；晋升后的统一 ReleaseV3 黑盒 seed `711861/711863` 分别为 `1000/1000` 和 `100/100`。

farthest-goal fast20 总时间 `12.790s`，按 `g=9..12` 分别为 `1.320/1.872/2.935/6.662s`，相对 PrunedDP 为 `16.3x/28.4x/30.3x/>32.5x`。solver 增量峰值和约 `448.8MiB`，相对 PrunedDP `6418MiB` 为 `14.3x`。full Toronto g13 为 `3.887s / 125.5MiB / 682,410 settled`。

该信号满足“突破后才复核 full DBLP”的条件。240 秒 bounded 直接输出：

```text
switch_global size=2 masks_in_size=8
anchor_group=12 anchor_distance=2.03012
released_row_bytes=159858112
sampled_peak_mb=3520.016
```

随后干净 candidate 在 1800 秒 runner 内正常完成 full DBLP g13 q1：

```text
printed weight       12.593628
solver total         531.556s
half rows             79.331s
dual                  35.223s
global               429.785s
global settled       11192686
global peak open     19786865
sampled peak RSS     3870.7MiB
```

打印值与既有精确值 `12.5936282853` 相差 `2.853e-7`。相对 ReleaseV2 precursor，时间快 `2.69x`、峰值小 `2.38x`。该源码原地移动为 `methods/Release/release_v3.*`，不因文件晋升重复 full 长跑。
