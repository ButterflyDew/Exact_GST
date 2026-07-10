# Test 系列文档地图

这份文档只做导航和共同背景。当前应把 Test18 和 Test19 分开读：

- Test18 是合规主线快照，保留可复现历史状态。
- Test19 是从 Test18 复制出的独立实验线，后续新机制优先在这里做。
- 失败尝试、撤回特判和旧长跑日志不混在入口文档里，只放在归档文件中查证。

## 1. 先读什么

| 文件 | 用途 | 状态 |
| --- | --- | --- |
| `RUN.md` | 编译、运行、随机对拍命令 | 运行入口 |
| `readme_files/test_series_overview.md` | 当前这份文档，解释文档结构和共同理论 | 导航 |
| `readme_files/test18_algorithm.md` | Test18 当前合规快照和安全边界 | 历史主线 |
| `readme_files/test18_effect_report.md` | Test18 可引用效果、O2 后关键计时、DBLP g13 状态 | 历史证据 |
| `readme_files/test18_archive_failed_attempts.md` | 已撤回和失败路线，尤其是第六条相关内容 | 不进入主线 |
| `readme_files/test19_algorithm.md` | Test19 当前实现、开关、代码瘦身状态 | 当前实验线 |
| `readme_files/test19_effect_report.md` | Test19 短验证和 one-tree LB 结果 | 当前实验线证据 |
| `readme_files/test19_migration_plan.md` | 从 Test18 复制到 Test19 后的拆分计划 | 维护计划 |
| `readme_files/test19_research_directions.md` | DS*、one-tree LB、separator 等大机制候选 | 研究候选 |

旧长文统一在 `readme_files/archive/`。除非要查原始证据，否则不要从 archive 开始读。

## 2. 版本定位

| 版本 | 定位 |
| --- | --- |
| Test16 | 稳定版 half-DP，最清晰的安全基线 |
| Test17 | 在 Test16 上优化 Complete 前置过滤和补集二分预处理 |
| Test18 | 当前合规历史主线，保留 exact reductions、row cache、frontier 候选和 O2 后结果 |
| Test19 | Test18 的独立副本，承接后续 DS* / one-tree LB / 代码拆分实验 |

Test18 不再继续堆新探针。Test19 可以改，但默认路径仍应保持可对拍、可回退、无数据集/组数/时间点特判。

## 3. 问题和复杂度口径

问题是精确求解组斯坦纳树。一次查询给出 `g` 个组，每组包含若干候选点；目标是找一棵代价最小的连通子图，使每个组至少命中一个点。

论文目标是相对主 baseline PrunedDP 同时取得实际时间和空间上的数量级优势，并尽量跑动更大的组数。复杂度分析必须不超过 PrunedDP 口径：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

按照 `agent.md`，实现里使用 `std::priority_queue`；理论核算时把堆操作视作与斐波那契堆同阶。

## 4. 共同符号

```text
G=(V,E)       无向非负边权图
n, m          点数、边数
g             组数
U             全部组的 bitmask
H             floor(g/2)
D*(S,v)       覆盖 S 并以 v 为同根连接点的真实最优 rooted DP
dp[S][v]      算法构造出的可行上界
best          当前完整可行解上界，单调不增
gd[a][v]      点 v 到第 a 个组的最短距离，也等于 D*({a},v)
```

## 5. 共同理论基础

任意一棵最优答案树中，为每个组选择一个命中点；一个点可以带多个组标记。树上存在一个点 `r`，删除 `r` 后每个连通分量中的组标记数都不超过 `g/2`。这是带权树重心结论：如果某个分量包含超过一半标记，就沿该分量方向移动；最大侧标记数会严格下降，最终停止。

以这个 `r` 为同根，最优树被分成若干个从 `r` 出发的分支，每个分支覆盖组数都不超过 `H`。进一步，这些分支可以合并成至多三个大小不超过 `H` 的块：

1. 初始每个分支是一块。
2. 只要存在两块大小之和不超过 `H`，就合并它们。
3. 如果最终还剩至少四块，设最小四块大小为 `x1<=x2<=x3<=x4`。不能再合并意味着 `x1+x2>H`，又有 `x3+x4>=x1+x2>H`，所以总组数至少 `2H+2`，与 `g` 只能是 `2H` 或 `2H+1` 矛盾。

因此存在某个最优解可写成同一根 `r` 上的三块拼接：

```text
A union B union C = U
|A|, |B|, |C| <= H
```

这就是 Test16 之后只保存小状态、在线拼完整答案的理论基础。

## 6. h 的结论

历史上 Test11--Test15 大量围绕 h 剪枝展开。最终保留下来的原则是：

```text
h(R,v)=max { E[T,v] | T subset R, E[T,v] 已证明等于 D*(T,v) }
```

只有已证明精确的 `E[T,v]` 能作为 h witness。因为 `T subset R` 时有：

```text
D*(T,v) <= D*(R,v)
```

所以这样的 h 才是补集 rooted DP 的安全下界。若把算法当前得到的上界 `dp[T][v] >= D*(T,v)` 当成 h witness，就可能把偏大的值误用为下界，导致错误剪枝。

Test13 曾出现固定反例：

```text
DPBF                 55
Test13               56
Test13（关闭 h）      55
```

污染链是：某个小状态被 h 剪掉后不再 liveup；其超集只能从不完整源集合中搜索出偏大的值；这个偏大值又被当成 h witness，进一步剪掉真正最优路径。这个反例说明，“Dijkstra 弹出”只能证明相对当前源集合最短，不能证明全局 rooted DP 精确。

因此 Test16--Test18 当前主线都不使用非 exact h。若未来重新研究 h，必须先解决低成本 exact 认证，而不能恢复 Test13 式写法。

## 7. 历史脉络

Test11 建立了半集合 DP、组距离预处理、初始上界、同根合并、图搜索和 h 剪枝的基本框架，但使用稠密二维表，空间压力大。

Test12 主要做诊断，确认只保留目标点而不保留必要传播前缀会影响后续超集。

Test13 尝试在线 h 和删除大于 `H` 的状态，证明了只保存小状态的方向可行，但也暴露了非 exact h 的正确性问题。

Test14 引入按 mask 存储的稀疏行，空间显著下降，但仍继承了 h 语义风险。

Test15 严格区分 `dp` 和 `exact`，只允许 exact 参与 h。正确性恢复，但 witness 强度不足，h/LB 查询放在热路径后收益不抵开销。

Test16 是回到简单安全主线后的稳定版：稀疏小状态、不物化 h、pair 层保留完整临时源、弹出时在线拼补集。

Test17 不改变状态语义，只优化 Complete：过早时跳过、预存补集二分，并增加 cover 诊断。

Test18 继续减少实际状态数和稠密行空间：虚拟 singleton、全根 corridor、stale compact、成本触发 dense-light、正确 DP 顺序保存条件、cover-aware 完整上界、补侧 Complete row 物化、pair/single 同根分区上界、事件触发的 frontier k=3/k=4 同根分区候选，以及查询级 exact graph reductions。固定 rows 三分块、固定组数/层级特判、非 dense 高阶行轻量表示和失败下界都已退出当前主线，只在 `test18_archive_failed_attempts.md` 中保留证据。2026-07-10 后新增 `future_lb_probe` 作为 DS* / zero-star Steiner 方向的下界筛查工具；`GST_TEST18_ASTAR_ORDER=1` 是默认关闭的 A* ordering 探针。Test19 已作为 Test18 的独立镜像建立，后续新实验优先在 Test19 上拆分和推进。

## 8. 当前运行纪律

当前仓库保留 Test16、Test17、Test18、Test19 四个入口。Test16 是稳定基线；Test17 是补集拼接优化；Test18 是可复现历史主线；Test19 是继续目标前的实验承接版本。

验证要求仍按 `agent.md`：

- 随机小图黑盒对拍 DPBF，精度 `1e-6`。
- Toronto 使用已有 DPBF 结果逐条比对，不重新跑完整 DPBF。
- 结果文件是追加式的，比较时看最后一次运行。
- Release / RelWithDebInfo 必须显式 O2。
- 运行后清理 `.tmp_random_compare*`、`result_tmp*` 和只含 header 的无效结果目录。
- 没有突破性理论机制或非常重要短探针输出时，不启动 full DBLP g13 query 1。
- 不做 baseline 也能使用的普通图/询问压缩，除非能证明是本方法特有或原创。
