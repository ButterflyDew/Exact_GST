# Test67：Anchor Skeleton Fixed Point

更新时间：2026-07-13。Test67 检查能否把 Test48 的单轮 branch-junction tree 提升为一个无参数 fixed point，从而在 ordinary D2 前得到明显更强的 anchor-aware incumbent。独立 probe 使用 Release/O2；它没有接入 Test21，也没有运行 Toronto full 或 DBLP full。

## 1. 候选机制

从 root-star 根到 permanent anchor group 的最短路 `P` 出发，维护一棵 attachment candidate tree `T`。令 `k=g-1`，`q=ceil(k/4)`。每轮：

1. 从 `T` 做 multi-source Dijkstra，得到 `d_T` 和一棵确定 parent forest；
2. 对每个 `1 <= |S| <= q` 顺序扫描

   ```text
   argmin_x [d_T(x) + sum(i in S) gd_i(x)];
   ```

3. 把这些 argmin roots 到旧 `T` 的 parent paths 加入 candidate tree；
4. 若本轮没有新增顶点则停止。

候选路径并不全部预付。fixed point 后只保留候选 roots、分叉点和连接路径，在压缩父树上做 subset facility DP：某 child 服务非空 mask 时才支付该压缩边。最后恢复所选设施到 groups、设施到 backbone 的真实路径，并按 edge id 去重。

停止条件是有限树闭包的固定点；没有轮数、数据集、`g`、密度或 wall-time 特判，也没有 Hash。每个 DP 解都恢复为一棵真实连通子图，所以结果始终是合法 upper。随机图与 DPBF 的 `upper >= exact` 检查为 `500/500`（seed `713421`）。

## 2. Fast 结构结果

五库 g12 q1 均在 `2--4` 轮稳定；压缩树为 `4--62` 个顶点，单条 wall 为 `0.028--0.192s`：

| dataset | fixed-point upper | exact | tree / compressed |
| --- | ---: | ---: | ---: |
| Toronto | `0.9688685500` | `0.9616227800` | `110 / 62` |
| Toronto-new | `4.0032872900` | `3.8735458400` | `231 / 55` |
| DBLP | `12.2261440000` | `12.1663030000` | `7 / 4` |
| DBLP-new | `10.9954928000` | `10.3147548000` | `15 / 9` |
| MovieLens | `0.0202272821` | `0.0202189774` | `16 / 11` |

与 Test48 单轮 junction tree 相比，Toronto 从 `0.971477` 小幅降到 `0.968869`，DBLP-new 从 `11.003405` 降到 `10.995493`，其余基本不变。真实路径去重仅在 fast20 的 Toronto-new g11 有可见变化：`4.034333 -> 3.981629`，仍高于 exact `3.873546`。

## 3. 否决结论

fixed point 证明了候选父树可以无参数地迭代扩展而不在 fast 上失控，但没有改变关键几何边界：所有 facilities 仍来自同一个条件 parent tree，无法覆盖一般最优 paid skeleton family。DBLP g12 upper 完全不动，五条 g12 仍有约 `0.04%--6.60%` 缺口；这不足以预期 full D2 数量级下降。

因此 Test67 不集成、不触发 full，并删除 probe 源码、CMake 入口和生成目录。后续不再迭代同一 conditional skeleton；仍需把 Test49 的 exact pair-path sharing 与 consumer-mask sharing 放进同一状态表示。

Test67 没有采用新的论文算法。multi-source Dijkstra、parent-tree subset DP 和 paid-anchor 语义均来自仓库既有机制；fixed-point candidate closure 是本轮实验设计，不宣称论文级原创性。
