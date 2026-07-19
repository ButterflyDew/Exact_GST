# 五数据集 g15 全量剖析与极端询问复核

更新时间：2026-07-17。本文回答三个问题：Test145 在五个原始数据集的 200 条 g15 询问上实际表现如何；已有优化是否在某些图或询问上成为负优化；最重询问由什么阶段主导。本文是实验与决策报告，不重新讲 Test145 的方法证明。

## 1. 实验协议

五个数据集均使用原始 `query_g15.txt` 的全部 40 条询问。所有二进制均为 Release/O2；不压缩图或询问，不按数据集、固定 `g`、层号、密度或运行时间改变策略。`Test145D2` 与 `Test145D3` 只在对应 ordinary D 层自然结束后停止，因此 `weight=-1` 是预期的前缀结果，不能当作完整答案。

第一阶段完整运行 200 条 Test145D2，用统一低成本前缀获得无偏的跨询问分布。第二阶段在每个数据集按 D2 values 排序，冻结 minimum、lower quartile、median、upper quartile 和 maximum 五条询问，再运行 D3 或完整消融。该五问面板用于覆盖难度层次，**不是总体均值的无偏估计**。

原始结果位于 `result_snapshot/g15_profile/runs/20260716_d2_all/Test145D2`，冻结面板为其中的 `frozen_d3_panel.csv`。剖析器、字段定义和复现实例见 `tools/g15_profile/README.md`。

## 2. 200 条 D2 前缀的总体表现

| 数据集 | 图规模 `(n,m)` | 40 问总 wall | 中位数 | P90 | 最大值 | 最大 query peak |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto | `(46,073, 68,353)` | `155.565s` | `3.389s` | `6.130s` | `7.989s` | `108.711MiB` |
| DBLP | `(2,497,782, 12,786,329)` | `13,475.760s` | `288.319s` | `599.226s` | `678.464s` | `3319.988MiB` |
| DBpedia | `(5,887,296, 18,338,729)` | `6,592.383s` | `140.816s` | `222.001s` | `294.608s` | `4522.043MiB` |
| LinkedMDB | `(1,326,784, 2,132,796)` | `1,610.886s` | `25.335s` | `71.902s` | `119.666s` | `1261.465MiB` |
| MovieLens | `(62,423, 35,323,774)` | `736.088s` | `18.451s` | `19.878s` | `20.820s` | `2802.164MiB` |

DBLP 与 LinkedMDB 的 D2 values 和 wall 的 Pearson 相关系数分别为 `0.9952/0.9963`，说明跨询问重尾主要来自 ordinary 状态规模，而不是固定载图成本。DBpedia 的相关系数只有 `0.7302`，MovieLens 为 `0.5750`；在这两个图上，大图预处理更容易掩盖状态规模差异。

## 3. 每个数据集的重询问

**Toronto q5。** D2 有 `2.211M` values，是该图最大值，但图本身很小。完整运行的主要成本逐渐转到高层 D/A；它不是内存极端。eager anchor-tree DP 在此类轻图上成为明显额外成本。

**DBLP q20。** D2 有 `202.265M` values、`450.539M` queue pops，其中 `248.273M` 是 stale，占 pops 的 `55.11%`；D2 prefix 为 `678.464s/3319.988MiB`。D3 进一步达到 `805.304M` values、`1.706B` pops 和 `900.388M` stale，完整 D3 prefix 为 `2713.912s/9700MiB`。这是 ordinary 图闭包的明确极端。

**DBpedia q34。** D2 有 `43.588M` values，prefix 为 `294.608s/4522.043MiB`；D3 values 反而只有 `12.896M`，但 D3 prefix 总 wall 达 `447.570s`。这里峰值和时间不仅由状态数决定，组距离、dual 与大图常驻结构占据更大比重。

**LinkedMDB q40。** D2 有 `83.661M` values，D3 有 `297.410M` values；原 D3 prefix 为 `465.448s/3670MiB`。它与 DBLP q20 一样由 ordinary closure 主导，但状态在各非锚组对之间较均匀，不存在一个可普遍删除的异常组。

**MovieLens q12。** D2 最大也只有 `31,675` values，D3 为 `12,241` values；总 wall 仍约二十秒，峰值约 `2.8GiB`。这是典型的预处理主导询问，不能靠继续压 ordinary 状态解决。

## 4. 已有优化的跨库反例

### 4.1 锚树上界不应逐层 eager 执行

Test145 的锚树设施上界在 Toronto 40 条 D2 前缀上累计 `69.956s`。冻结五问的 eager/no-tree 完整 wall 为 `174.909s/134.433s`，eager 慢 `30.1%`。Test146 改为按实际 queue pops 与 relax attempts 摊销购买；同一规则在 Toronto 五问中自然得到 `0/5` 次购买，而在 DBLP/DBpedia 大多数询问中仍会触发。该机制保留，详见 `test146_amortized_anchor_tree.md`。

### 4.2 dual-cut 在 MovieLens 上有效但不划算

MovieLens 的 eager dual 在 40 条 D2 前缀中累计约 `562.5s`，占总 wall 的 `76%`。冻结五问关闭 dual 后，D2 wall 从 `92.417s` 降到 `47.969s`，但状态增至 `24,247--130,659`，说明 dual 的剪枝有效、前置成本却与轻状态不匹配。

Test147 尝试用已发生工作量延迟构造 dual。D2 五问降到 `60.207s`，但信号出现太晚，D3 总 wall 从 eager 的 `96.570s` 回退到 `178.800s`。因此 lazy dual 已撤回；当前没有为了 MovieLens 添加数据集特判，也没有把 no-dual 当成普遍优化。

### 4.3 改选锚组不是统一答案

D2 pair incidence 诊断显示，LinkedMDB q40 的 14 个非锚组占比约 `6.25%--7.79%`，近似均匀，换锚无法系统消掉重状态。DBpedia q34 较集中，最高组 incidence 为 `15.96%`，理论上改锚可能少掉约 `32%` pair states；但该现象不能迁移到 LinkedMDB，且查询改写也不是本方法独有，因此不进入主线。

### 4.4 dual 不应为每个组重新扫描全部弧

MovieLens 的问题不是 dual 无效，而是构造成本被重复全弧扫描主导。处理一个后继组时，原实现先以原始组距离初始化 `distance`，再枚举全部 `2m` 条有向弧寻找残量三角不等式的违反点。原始组距离已经在未被前面势函数降低的弧上满足三角不等式，因此 Test149 只枚举累计改写弧，再从这些端点执行完整残量 Dijkstra；势、下界与后续状态逐点不变。

随机 DPBF 对拍为 `200/200`。同机配对的 MovieLens 冻结五问 D3 wall 从 `94.772s` 降到 `86.254s`，dual 从 `68.956s` 降到 `61.615s`，实际初始化弧检查仅为全扫描的 `2.3%--15.9%`；Toronto 完整五问 wall 为 `139.543s -> 137.721s`，没有因 bitset 与枚举开销出现系统回退。非同批参考下，DBLP q11、DBpedia q34 与 LinkedMDB q40 的 dual 分别为 `33.770s -> 26.658s`、`48.760s -> 38.737s`、`6.541s -> 5.151s`，普通状态逐项一致。

为排除冻结五问的选择偏差，Test149D2 又完整运行 MovieLens 全 40 条：dual 总时间 `562.484s -> 533.664s`，40 条中 35 条更低，初始化扫描比例中位数为 `6.24%`；全部 D2 values/pops 逐问一致。当前 Test149 相对 Test145 的总 wall 为 `736.088s -> 699.863s`，其中还包含 Test146 将 anchor-tree 总时间从 `9.533s` 降到 `1.646s` 的收益，不能全部归因于改写弧。最大 query peak 增加 `5.410MiB`。该机制作为当前 Test 主线保留，完整证明与分阶段归因见 `test149_changed_arc_dual.md`。

## 5. ordinary 队列重复项诊断

Test145 使用仓库规定的 `std::priority_queue` lazy-duplicate Dijkstra。完整 200 条 D2 中，各数据集 stale 情况为：

| 数据集 | D2 values | D2 pops | stale pops | stale/pops |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `15.504M` | `26.028M` | `10.523M` | `40.43%` |
| DBLP | `3.501B` | `5.780B` | `2.279B` | `39.43%` |
| DBpedia | `123.725M` | `138.388M` | `14.663M` | `10.60%` |
| LinkedMDB | `641.144M` | `846.749M` | `205.605M` | `24.28%` |
| MovieLens | `146.6K` | `295.5K` | `148.9K` | `50.39%` |

Test148 临时用数组位置的 decrease-key binary heap 消除队列内重复项。它通过随机 DPBF `200/200`，并得到以下诊断上界：

| 面板 | Test145/Test146 | Test148 | 变化 |
| --- | ---: | ---: | ---: |
| DBLP q20 D2 | `678.464s` | `502.171s` | `-26.0%` |
| DBLP q8 D3 | `716.484s` | `657.954s` | `-8.2%` |
| LinkedMDB q40 D3 | `465.448s` | `412.777s` | `-11.3%` |
| DBpedia q34 D2 | `294.608s` | `294.478s` | 中性 |
| Toronto 完整五问 | `142.504s` | `136.791s` | `-4.0%` |
| MovieLens D3 五问 | `96.570s` | `93.191s` | `-3.5%` |

DBLP q20 的 D2 pops 从 `450.539M` 降到 `202.265M`，峰值还从 `3319.988MiB` 降到 `3254.297MiB`；这证明极端 wall 中确有大量通用堆维护成本。另一方面，DBpedia 几乎不变，MovieLens 的绝对 ordinary 时间只减少不到一秒，说明它不是五数据集的共同瓶颈。

Test148 不保留：decrease-key heap 是 baseline 同样可以采用的通用工程优化，不是 permanent-anchor A 独有机制；仓库规范还要求具体实现使用 `priority_queue`。源码与 CMake 目标已经撤出，结果仅作为“极端询问还有多少通用队列开销”的诊断证据，详见 `archive/test148_indexed_ordinary_queue_20260717.md`。

## 6. 最终决策

1. **保留 Test146。** 它修复已有锚树上界在轻询问上的负优化，规则由精确工作量决定，属于本方法内部调度。
2. **在 Test146 上保留 Test149。** 它不关闭或延迟 dual，只消除 sequential residual 构造中可由三角不等式证明无效的全弧初始化检查。
3. **撤回 Test147。** MovieLens 的 dual 构造确实过重，但当前 lazy 信号在 D3 才出现，总代价更差。
4. **撤回 Test148。** 它确认 DBLP/LinkedMDB 极端询问含显著通用 priority-queue 重复项成本，但不作为论文贡献或主线代码。
5. **保留全量 profiler 与 pair 统计。** 后续优化必须同时看 40 问分布和冻结分层面板，不能再用单一 q1 代表整个数据集。
6. **下一理论问题仍是 ordinary D。** Test149 修复的是 dual 固定成本；ordinary 极端仍要求减少 method-specific states、seed candidates 或传播，不能改用数据集特判、查询压缩或 baseline 可共享的数据结构替换。

## 7. 文献说明

本报告没有引入新的论文方法。Test145/Test146 继承的 rooted subset DP 来自 [Dreyfus and Wagner, 1971](https://doi.org/10.1002/net.3230010302)；directed-cut 转置终端的贡献边界见 `test145_directed_cut_transposed_terminal.md`。Test149 只利用本仓库 Wong-style sequential dual 适配已经产生的残量改写集合；dual 消融、锚组 incidence、bitset 枚举与 decrease-key heap 均不在本文中单独主张原创性。
