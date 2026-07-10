# GST Framework：Test 系列文档入口

ReleaseV1 是当前可审查发行版；Test19 继续作为实验主线，Test18 只保留为可复现历史快照。本文提供导航、共同理论和运行纪律，不重复算法细节或实验表格。

## 1. 文档地图

| 文档 | 唯一职责 |
| --- | --- |
| `RUN.md` | 构建、运行、随机对拍和输出格式 |
| `readme_files/release_v1.md` | ReleaseV1 独立算法、证明、边界与 Release/O2 基线 |
| `readme_files/test19_algorithm.md` | Test19 当前算法、公式、开关和合规边界 |
| `readme_files/test19_effect_report.md` | 当前 Release/O2 正确性与性能基线 |
| `readme_files/test19_research_directions.md` | 下一理论问题和进入代码的证据门槛 |
| `readme_files/dual_anchored_global_labels.md` | 当前大 g 主成果、精确性证明与 full DBLP g13 完整结果 |
| `readme_files/distance_epoch_rows.md` | distance-only row 与同根 pair 分块上界的证明、门槛和 full g13 有界结果 |
| `readme_files/dual_cut_potential.md` | directed-cut 势函数、splice 证明与旧 distance solver 有界证据 |
| `readme_files/pair_forest_certificate.md` | 已通过 full g13 pair 探针的 predecessor certificate 机制 |
| `readme_files/test19_maintenance.md` | 纯代码拆分顺序与每步验证 |
| `readme_files/test18_algorithm.md` | Test18 合规历史实现 |
| `readme_files/test18_effect_report.md` | Test18 可引用的 O2 历史数据 |
| `readme_files/snapshot_benchmark.md` | 跨数据 snapshot 工具 |
| `readme_files/archive/` | 旧长文、失败尝试、降级探针和未完成运行 |

阅读发行实现先看 `release_v1.md`；阅读大 `g` 突破先看 `dual_anchored_global_labels.md`；继续研究再看 research，并按需进入 dual-cut、distance-only 基底或 Test19。archive 只用于追溯，不代表当前实现。

## 2. 版本定位

| version | role |
| --- | --- |
| ReleaseV1 | 从已验证机制重写的单路径可审查发行版 |
| Test16 | 简单、安全的 half-DP 基线 |
| Test17 | 优化 Complete 前置过滤和补集二分预处理 |
| Test18 | exact reductions、row cache 等历史主线；不再增加实验机制 |
| Test19 | 当前实验主线；默认 exact group-tour TSP/2 order+save |
| distance epoch probe | 已完成阶段使命的低空间 row 基底；独立于 Test19/Release |
| dual-cut probe | 当前 global-label 势函数组件；旧 distance solver 的 full run 停在 k3 |
| dual-anchored global labels | 当前大 g 独立原型；full DBLP g13 q1 已精确完成 |
| Test20 | pair-row certificate 原型已因 fast 时间退化撤出，当前不存在生产版本 |

Test19 可同时设置 `GST_TEST19_TSP_LB_ORDER=0` 和 `GST_TEST19_TSP_LB_SAVE=0` 回到未启用 TSP/2 的 Test18-family 路径。硬编码 k=3/k=4 frontier 已从 Test18/Test19 撤出。

`Test19` 默认路径本身没有完成 full g13；完成目标的是不做普通图/query 压缩的独立 dual-anchored global-label 原型。两者不能在统计或实现归属上混写。

## 3. 问题与复杂度

一次 Group Steiner Tree 查询给出 `g` 个候选点组；目标是找代价最小的连通子图，使每组至少命中一个点。共同符号：

```text
G=(V,E)       无向非负边权图，|V|=n，|E|=m
U             全部组的 bitmask
H             floor(g/2)
D*(S,v)       覆盖 S、以 v 为同根连接点的精确最优值
dp[S][v]      算法当前构造的可行上界
best          当前完整可行解上界
gd[a][v]      v 到组 a 的最短距离
```

目标复杂度不超过 PrunedDP 口径：

```text
O(3^g n + 2^g((g + log n)n + m))
```

实现使用 `std::priority_queue`；理论核算按 `agent.md` 把堆操作视为斐波那契堆口径。

## 4. Half-DP 三块基础

给最优答案树的每个组选择一个命中标记。带权树重心保证存在根 `r`，使删除 `r` 后每个分支包含的组标记数不超过 `H`。

这些分支可以合并成至多三个大小不超过 `H` 的块：反复合并任意两块，只要和不超过 `H`。若最后仍有至少四块，最小四块满足 `x1+x2>H` 且 `x3+x4>=x1+x2>H`，总组数至少 `2H+2`，与 `g` 只能为 `2H` 或 `2H+1` 矛盾。

因此某个最优解可写成同根三块：

```text
A union B union C = U
|A|, |B|, |C| <= H
```

这支持 Test16-Test19 只保存 half masks，再在线完成完整答案。

## 5. 正确性边界

- lower bound 必须有统一证明；当前算法状态的上界不能冒充精确 witness。
- 只有已证明等于 `D*(T,v)` 的值才能安全用于 `h(R,v)=max_{T subset R} D*(T,v)`。
- “Dijkstra 弹出”只证明相对当前源集合最短，不自动证明全局 rooted DP 精确；Test13 的旧反例已归档。
- 不使用数据集名、固定组数、固定层级、row size、运行时间点或完成进度特判。
- 不做 baseline 同样可用的普通图/query 压缩，除非证明为本方法特有或原创。

## 6. 运行纪律

1. 所有关键构建使用 Release/O2；旧的非 O2 时间必须显式标注。
2. 随机小图与 DPBF 黑盒对拍，误差 `1e-6`；Toronto 优先比较已有 DPBF 结果。
3. 结果文件是追加式的，比较最后一次运行。
4. 运行后清理 `.tmp_random_compare*`、`result_tmp*` 和无效结果目录。
5. 探针先于长跑；full DBLP g13 q1 已完成，没有新的数量级机制或重要输出时不重复启动。
6. 结构重构与算法实验分开验证，避免结果无法归因。
