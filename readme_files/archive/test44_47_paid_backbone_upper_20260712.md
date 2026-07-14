# Test44--47：Paid Anchor Backbone 标量分块上界

更新时间：2026-07-12。Test44--47 是本轮最强的正向结构：固定一条已付费 anchor backbone 后，把 ordinary pair/root rows 压成每个 group block 一个标量 attachment cost，再做 partition DP。它在 fast20 上把 Test21 从 `12.380s` 降到最低 `10.112s`，full early upper 从 `17.3608` 降到最低 `13.1619`，sampled peak 也降到约 `2.18GB`；但所有集成版 full solver 都没有在 V3 的 `531.556s` 时间线内完成，故正式代码最终撤回。本文保留证明、演化和硬门槛，不保留工具、统计字段或开关。

## 1. Paid Backbone 消维

固定 farthest permanent anchor `a` 与 root-star 根 `r`，取一条从 `r` 到 anchor group 的确定性最短路 `P`。`P` 的边先支付一次。令：

```text
d_P(x) = min_{p in P} dist(x,p).
```

对 nonanchor block `S`，定义 common-root attachment：

```text
E(S) = min_x [d_P(x) + sum_{i in S} gd_i(x)].
```

`E(S)` 对应从某个 `x` 分别连接 backbone 与 `S` 中各组的真实路径并；它不是下界，而是一棵合法 attachment tree 的成本。若 blocks `S_1,...,S_t` 划分全部 nonanchor groups，则：

```text
cost(P) + sum_j E(S_j)
```

是合法 GST 上界。不同 blocks 可在 backbone 的不同位置接入，全部通过同一条已付费 `P` 连通；这正是 Test28 固定单 root 和 Test29 variable-root forest 没有的共享对象。每个 block 只保存 `(cost,argmin-root)`，没有 `D(S,v)` rows、Hash 或 pair-specific priority queue。

对选中 blocks 还会恢复 root-to-backbone 与 root-to-group shortest paths，按 edge id 去重求真实 union cost。MovieLens 的零权 tight-edge plateau 不能用简单距离下降回溯；正式探针改为在 tight-edge 子图上做带 visited 的确定性搜索，再回溯 witness。修复后随机对拍无错误。

## 2. Test44：Singleton/Pair Blocks

pair attachment `E({i,j})` 还是连接 `P`、group `i`、group `j` 的三组 root-star，因此在给定 paid backbone 后对该三组子问题是精确的。singleton/pair partition 只需 `O(g^2 n+2^g g^2)` 标量工作。

fast g12 独立 probe：

| dataset | dual primal | pair-backbone upper | exact | extra backbone wall |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `1.120571` | `1.020737` | `0.961623` | `1.25ms` |
| Toronto-new | `5.717173` | `4.048502` | `3.873546` | `1.24ms` |
| DBLP | `12.166303` | `12.226144` | `12.166303` | `1.92ms` |
| DBLP-new | `11.006435` | `10.995493` | `10.314755` | `1.58ms` |
| MovieLens | `0.0202190` | `0.0202656` | `0.0202190` | `24.18ms` |

候选只通过 `min(current,candidate)` 使用，弱库不反向。full DBLP：

```text
backbone vertices       4
scalar scans            194,826,996
pair-backbone upper     13.943288235
extra backbone wall     2.997s
```

集成 Test21 后通过随机 `300/300`（seed `712871`），fast20 为 `10.324s`；full 在约 `545.3 CPU-s / 3.68GB sampled peak` 时仍无最终结果，已越过 V3，Test44 单独不保留。

## 3. Test45：Triple Blocks 与路径并集

加入统一的 size-3 common-root attachment。它仍是合法上界但不再声称局部精确。Toronto-fast 的标量和为 `1.012770`，路径并集去重后进一步降到 `0.971477`，距 exact 约 `1.0%`；这是“共享 backbone + 三支路”捕获真实重叠的直接证据。

full DBLP：

```text
scalar scans            744,339,036
triple-backbone upper   13.365625917
selected blocks         1 singleton + 1 pair + 3 triples
extra backbone wall     4.639s
```

集成版修复零权 witness 后通过随机 `300/300`（seed `712891`；此前另有 `500/500` seed `712881`），fast20 为 `10.112s`。full 内存轨迹首次明显平台化，但在约 `543.1 CPU-s / 2.27GB sampled peak` 时仍无最终结果，硬门槛失败。

## 4. Test46：完整 `q=ceil(k/3)` Blocks

令 `k=g-1`，完整购买所有 `|S|<=q` 的 block scalars，使三个 blocks 足以覆盖全部 nonanchor groups。full 上界继续改善：

```text
q                       4
scalar scans            1,980,741,126
q-block upper           13.161911611
selected blocks         sizes 1,2,2,3,4
extra backbone wall     29.581s
```

但全列扫描过重。集成版随机 `300/300`（seed `712901`）、fast20 `10.650s`；full 在 `551.5 CPU-s / 2.01GB sampled peak` 仍无最终结果。内存继续下降，但额外预处理没有被后续节省覆盖。

## 5. Test47：Partition-Witness Block Lifting

Test47 只预购 size 1--3 blocks。每轮：

1. 用当前可用 blocks 求最优 partition；
2. 对每个选中 block，购买“加入一个未覆盖 token”的扩张；
3. 也购买两个当前选中 blocks 的并，只要大小不超过 `q`；
4. 重算 partition，直到没有新 block。

没有 wall/数据阈值，也不按固定 row 时刻停止。full 只需两轮、28 个 lifted blocks：

```text
upper                   13.161911611
scalar scans            814,276,932
extra backbone wall     12.191s
```

即保留 Test46 的上界，较全列少约 `59%` scans。集成版随机 `300/300`（seed `712911`）、fast20 `10.513s`；但 full 在 `555.0 CPU-s / 2.18GB sampled peak` 仍无最终输出，未跨 V3。

## 6. 最终结论

1. paid backbone 是有效原创候选：它让不同 block 共享一次 anchor path，并把 pair/root 维度压成 scalar；fast、full upper 和 full peak 都有强正信号。
2. 当前固定 root-star-to-anchor backbone 仍太短。full 中它只有 4 个 vertices；`13.1619` 仍未越过 D2 的时间临界区。
3. 继续扩 block size 只增加 `O(n)` scalar columns，不再作为下一步。更有价值的是扩展**共享 skeleton 的几何覆盖**，例如用有证明的少量主干分叉替代单路径，同时保持一个共同 paid object 和标量 attachments。
4. 因用户要求 DBLP g13 相对 V3 不退化，不能仅凭 fast `15%+`、peak 约减半或“只差二十秒”保留；Test44--47 全部从正式 Test21 撤回。

该机制没有直接采用新的论文算法；论文归属仍为已有 Dreyfus-Wagner、Dijkstra-Steiner 与 Wong dual 背景，本文不新增引用，也不宣称完成系统文献检索后的论文原创性。

## 7. 撤回验证与 Anchor 归因边界

正式 Test21 已删除 paid-backbone 调用、辅助函数、统计字段、CLI 输出和独立探针构建入口。撤回后 Release/O2 编译通过，随机 DPBF 对拍为 `100/100`（seed `712921`），fast20 为 `12.743s`（快照 `20260712_173406`）；没有再次运行 full DBLP。

这些实验能直接支持的因果结论是：当 anchor 被表示为一条只付费一次的共享 backbone 时，block root 维度可以压成 scalar attachment，因而 fast 上界阶段和 full 内存轨迹显著改善。它们不能支持“V3 或 A 的全部加速都来自 Anchor”：V3 的最终 DBLP 路径仍混有 B 与公共预处理，普通 permanent-anchor A 也仍受 D2 传播支配。准确说法应是 **Anchor 已被证明能让特定 A 内机制变快，但尚未被证明让完整 A/full DBLP 相对 V3 变快**。
