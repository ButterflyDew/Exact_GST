# Test 系列总览

本文档只解释 Test 系列的研究脉络和文档入口。具体算法细节分别放在：

```text
readme_files/test16_algorithm.md   Test16：稳定版半集合稀疏 DP
readme_files/test17_algorithm.md   Test17：Complete 前置过滤与补集二分预处理
readme_files/test18_algorithm.md   Test18：当前主线 handoff，先读这里
readme_files/test18_research_log.md Test18：完整实验记录与失败路线归档
```

## 1. 目标

问题是精确求解组斯坦纳树。一次查询给出 `g` 个组，每组包含若干候选点；目标是找一棵代价最小的连通子图，使每个组至少命中一个点。

论文目标是相对主 baseline PrunedDP 同时取得实际时间和空间上的数量级优势，并尽量跑动更大的组数。复杂度分析必须不超过 PrunedDP 口径：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

按照 `agent.md`，实现里使用 `std::priority_queue`；理论核算时把堆操作视作与斐波那契堆同阶。

## 2. 共同符号

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

## 3. 共同理论基础

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

## 4. h 的结论

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

## 5. 版本脉络

Test11 建立了半集合 DP、组距离预处理、初始上界、同根合并、图搜索和 h 剪枝的基本框架，但使用稠密二维表，空间压力大。

Test12 主要做诊断，确认只保留目标点而不保留必要传播前缀会影响后续超集。

Test13 尝试在线 h 和删除大于 `H` 的状态，证明了只保存小状态的方向可行，但也暴露了非 exact h 的正确性问题。

Test14 引入按 mask 存储的稀疏行，空间显著下降，但仍继承了 h 语义风险。

Test15 严格区分 `dp` 和 `exact`，只允许 exact 参与 h。正确性恢复，但 witness 强度不足，h/LB 查询放在热路径后收益不抵开销。

Test16 是回到简单安全主线后的稳定版：稀疏小状态、不物化 h、pair 层保留完整临时源、弹出时在线拼补集。

Test17 不改变状态语义，只优化 Complete：过早时跳过、预存补集二分，并增加 cover 诊断。

Test18 继续减少实际状态数和稠密行空间：虚拟 singleton、全根 corridor、stale 状态 compact、dense-light/packed-dense 行表示、cover-aware 完整上界更新，以及 pair 层完成后的 pair/single 同根分区上界。`test18_algorithm.md` 现在是交接用主文档；历史实验数据和失败路线集中在 `test18_research_log.md`。

## 6. 当前状态

当前仓库保留 Test16、Test17、Test18 三个入口。Test16 是最清晰的稳定基线；Test17 是补集拼接优化；Test18 是当前继续冲击大组数的实验主线。

验证要求仍按 `agent.md`：

- 随机小图黑盒对拍 DPBF；
- Toronto 使用已有 DPBF 结果逐条比对，不重新跑完整 DPBF；
- 结果文件是追加式的，比较时看最后一次运行；
- 运行后清理临时文件。
