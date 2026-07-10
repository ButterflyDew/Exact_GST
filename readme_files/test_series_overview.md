# GST Framework 文档入口

本文只做导航、版本定位和共同纪律。算法细节与实验数字只在各自唯一主文档维护。

## 1. 推荐阅读顺序

| 目的 | 文档 |
| --- | --- |
| 构建、运行、输出 | `RUN.md` |
| 当前大 g 发行算法 | `readme_files/release_v2.md` |
| ReleaseV2 full 原始计数 | `readme_files/release_v2_evidence.md` |
| 小 g 稳健发行算法 | `readme_files/release_v1.md` |
| 下一研究问题 | `readme_files/test19_research_directions.md` |
| snapshot 工具 | `readme_files/snapshot_benchmark.md` |
| 旧失败与撤回机制 | `readme_files/archive/README.md` |

需要追踪算法演化时，再读：

```text
test16_algorithm.md
test17_algorithm.md
test18_algorithm.md / test18_effect_report.md
test19_algorithm.md / test19_effect_report.md / test19_maintenance.md
distance_epoch_rows.md
dual_cut_potential.md
pair_forest_certificate.md
```

后三份是形成 ReleaseV2 前的研究证据，不代表当前发行热路径。

## 2. 版本定位

| version | role | current status |
| --- | --- | --- |
| ReleaseV1 | half-DP 单路径发行版 | small 35 条均不慢于 PrunedDP；未跑 full g13 |
| ReleaseV2 | dual-anchored global labels | 当前大 g 发行版；precursor 已精确完成 full DBLP g13 |
| Test16 | 简单 half-DP 基线 | 历史稳定入口 |
| Test17 | Complete/补集优化 | 历史增量入口 |
| Test18 | exact reductions 与 row cache | 冻结历史主线 |
| Test19 | TSP/2 half-DP 实验线 | 保留作对照与后续研究，不承载 ReleaseV2 |

ReleaseV2 不是通过 Test19 开关启用的模式；它有独立源码、可执行文件、结果目录和 stats 文件。

## 3. 两条精确 Recurrence

ReleaseV1/Test16--Test19 使用 half-DP。最优树存在一个根，可把组标记分成至多三个大小不超过 `floor(g/2)` 的同根块，因此只持久化 half masks，再在线完成答案。

ReleaseV2 固定任意必达组 `A0`。任何 GST 都命中某个 `x in A0`，以 `x` 为根后只需处理其余 `g-1` 组，因此状态上界为 `2^(g-1)n`，join 上界为 `3^(g-1)n`。两个 recurrence 都精确，但实际状态调度和内存形态不同。

共同目标复杂度：

```text
O(3^g n + 2^g((g+log n)n+m)).
```

实现使用 `std::priority_queue`；按 `agent.md` 以斐波那契堆口径核算理论复杂度。

## 4. 当前结论

- ReleaseV2 precursor 在未做普通图/query 压缩的 DBLP g13 q1 上得到 `12.5936282853`，solver wall `1427.625s`，peak `9.015GiB`。
- 最终 ReleaseV2 fast20 为 `12.806s`，相对 PrunedDP 下界快 `>29.66x`；平均 solver peak 增量小 `14.95x`。
- ReleaseV2 small35 权重全部正确，总体快 `3.58x`，但 g3--g6 有稳定预处理负收益。
- ReleaseV1 仍是 small 更稳健的独立入口；仓库没有按 `g` 或数据集自动选择发行版。

## 5. 研究证据分层

| document | retained information |
| --- | --- |
| `release_v2_evidence.md` | full g13 逐层状态、frontier 与撤回项 |
| `dual_cut_potential.md` | directed-cut 势函数推导和旧 row solver A/B |
| `distance_epoch_rows.md` | distance-only row 阶段结果 |
| `pair_forest_certificate.md` | predecessor certificate 表示收益与生产负结果 |
| `archive/` | 旧长文、失败探针、撤回代码路径 |

当前结论优先级：发行文档高于研究证据，研究证据高于 archive。旧文档中“full 未完成”只描述对应旧 solver，不得覆盖 ReleaseV2 当前状态。

## 6. 论文引用规则

发行文档 `release_v2.md` 第 11 节逐项列出 Dijkstra-Steiner、DS*、TSP/2、Wong dual ascent 与 PrunedDP 的来源，并区分“文献机制”“GST 适配”“本仓库组合/实现”。其他文档引用论文时也必须在同一条目说明它与当前代码的关系；只列标题而不说明用途的引用不作为算法归属证据。

## 7. 共同纪律

1. 所有关键计时使用 Release/O2。
2. 随机小图与 DPBF 在 `1e-6` 内一致；Toronto 比较现有 DPBF 最后一次完整 run。
3. 追加结果按最后一个 run header 比较。
4. 不使用数据集名、固定 `g`、固定层级、row density、运行时间或完成进度特判。
5. 不把 baseline 同样可做的普通图/query 压缩计作本方法收益。
6. full DBLP g13 已完成；纯重构、常数优化或弱短测不触发重复长跑。
7. 运行后清理临时目录、空结果目录与残留进程。
