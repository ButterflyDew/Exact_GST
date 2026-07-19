# 已冻结但仍有效的研究文档

本目录保存已经完成阶段使命、但仍包含有效证明、实现经验或实验数据的文档。它们不是当前运行入口，也不覆盖 ReleaseV6 的事实。

`research_status_pre_release_v6_20260718.md` 保存 ReleaseV6 冻结前的完整 V5/Test152 状态页；当前状态统一以 `../research_status.md` 和 ReleaseV6 文档为准。

| 主题 | 文档 | 保留内容 |
| --- | --- | --- |
| half-DP 演化 | `test16_algorithm.md`、`test17_algorithm.md` | half masks、Complete 与补集查询的早期稳定设计 |
| Test18 | `test18_algorithm.md`、`test18_effect_report.md` | row cache、exact reductions 与跨数据效果 |
| Test19 | `test19_algorithm.md`、`test19_effect_report.md`、`test19_maintenance.md` | TSP/2 half-DP、冻结基线与代码维护记录 |
| distance rows | `distance_epoch_rows.md` | 只保存精确 distance 的 row 表示与有界负结果 |
| dual potential | `dual_cut_potential.md` | directed-cut 势函数证明、旧 row solver A/B 和来源 |
| half/global | `half_global_hybrid.md` | ReleaseV3 的选择过程、排除路线和前置证据 |
| pair certificate | `pair_forest_certificate.md` | predecessor certificate 的精确表示与生产负结果 |
| paid attachment | `paid_attachment_representative_family.md` | 单边界 A 状态的指数 Pareto 反链与代表族适用边界 |
| paid half profiles | `paid_attachment_half_profiles.md` | 去 root 的 attachment profile、三块 exact completion、pair projection 与 pair-only block completion |
| two-attachment scalarization | `two_attachment_consumer_scalarization.md` | 固定 consumer 的双接口 macro-label 精确定价、无 `n^2` 表示与全量 `5^k` 边界 |
| explicit D2 barrier | `explicit_d2_output_barrier.md` | 先物化 pair rows 的输出下界与下一原型验收条件 |
| D2 symbolic boundary | `d2_symbolic_convolution_boundaries.md` | 实权 min-sum subset convolution 文献边界与 zero-residual quotient 上限 |
| skeleton-conditioned pair | `skeleton_conditioned_pair_boundary.md` | 固定 paid skeleton 下 pair 的精确标量化与无条件 exact family 边界 |
| pair tight-prefix | `pair_tight_prefix_frontier.md` | predecessor ancestor/tight-cone 支配定理、跨库 frontier 规模与 consumer-mask 剩余边界 |

阅读顺序：当前发行方法先读 `../release_v5_method_cn.md`，实现与证据读
`../release_v5.md`；需要对照首个纯 A 版本或框架 B 时，再读 `../release_v4.md`、
`../release_v3.md` 和 `../release_v3_implementation.md`。ReleaseV4 的研究阶段数据
见 `../test80_anchor_progressive.md`；失败尝试、撤回二进制和原始长日志位于
`../archive/`。
