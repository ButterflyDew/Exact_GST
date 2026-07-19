# Archive：历史与失败证据

本目录只保存旧长文、撤回机制、失败探针和未完成运行，不作为当前读者入口。当前文档从 `readme_files/test_series_overview.md` 开始。

## 文件索引

| 文件 | 内容 |
| --- | --- |
| `test79_v3_junction_control_20260713.md` | junction 接入 V3/B 的历史 full 对照；活跃代码已撤出 |
| `test76_first_order_block_anchor_20260713.md` | fixed-plan 一阶 block-anchor 精确定价、无 Hash 共享 rows、Toronto family 不完备否决 |
| `test75_pair_block_completion_20260713.md` | paid pair + 两个 half blocks 的正结果、anchor-first 边界与 skyline 完备性反例 |
| `test_series_overview_full_legacy_20260710.md` | 文档重构前的历史地图 |
| `test18_algorithm_full_legacy_20260710.md` | Test18 旧算法长文 |
| `test18_effect_report_full_legacy_20260710.md` | Test18 旧效果表和长跑记录 |
| `test18_research_log_full_legacy_20260710.md` | Test18 旧研究日志 |
| `test18_failed_attempts_20260710.md` | Test18 失败、撤回和第六条边界 |
| `test19_probe_archive_20260710.md` | Test19 降级探针、dual 接入前 global-label 负结果与未完成 g13 run |
| `group_replacement_future_bound_probe_20260710.md` | replacement cap、exact-pair 与代表点 degree bound 的证明和负结果 |
| `test20_pair_forest_prototype_20260710.md` | pair certificate 生产原型的正确性、small 守门与 fast 时间负结果 |
| `tsp_witness_row_probe_20260710.md` | 用 1-byte TSP endpoint witness 替代 need double 的 exact 空间收益与 fast 时间负结果 |
| `half_global_probe_log_20260710.md` | 从 ReleaseV2 后探针到 farthest-goal ReleaseV3 的完整研究明细 |
| `test21_anchor_half_experiments_20260711.md` | Test21 dense、Hash frontier 撤回与 ordered-row 收敛过程 |
| `test22_anchor_bidirectional_20260712.md` | A/D 补集双向 label-setting 的正确性、无 Hash 实现与 fast 状态否决 |
| `test23_global_d_anchor_half_20260712.md` | ordinary D 全局调度后冻结 ordered rows 的正确性与 fast 时间否决 |
| `test24_branch_basis_online_upper_20260712.md` | branch basis、在线三块、quarter witness 与 DBLP gate 演化记录 |
| `test25_27_anchor_state_probes_20260712.md` | fixed-mask full closure 反例、DBLP D2 分层与 generated-star 候选 |
| `test28_pair_matching_upper_20260712.md` | D2 在线 pair/singleton matching 的正确性与 fast DBLP 零更新否决 |
| `test29_variable_root_pair_forest_20260712.md` | 全部 fast D2 roots 上的 pair forest 复核及与 Test18 旧证据的关系 |
| `test30_split_seed_anchor_recurrence_20260712.md` | 用 local split seeds 替代 propagated D branches 的小图与 Toronto 反例 |
| `test31_pair_cherry_anchor_lattice_20260712.md` | 全 A lattice + D1/D2 cherry recurrence 的独立小图反例与改根障碍 |
| `test32_witness_saturated_anchor_20260712.md` | 单棵 D witness 免费改根仍丢失 A 历史 attachment 的 Pareto 反例 |
| `test33_first_half_global_warm_start_20260712.md` | half-label global 上界 oracle 的 fast 与 full DBLP bounded 否决证据 |
| `test34_dual_objective_certificate_20260712.md` | rooted dual objective 误作自由根全局停止下界的随机反例 |
| `test35_anchor_group_dual_20260712.md` | permanent anchor-group super-root dual 的合法性、替换实验与 fast 状态否决 |
| `test36_pair_vector_wave_20260712.md` | 多 pair source-profile 共享的精确性、异构 priority 与 fast 队列否决 |
| `test37_bounded_branch_recurrence_20260712.md` | singleton/pair-bounded ordinary recurrence 的 rooted state 反例 |
| `test38_local_seed_cone_20260712.md` | local seed-cone 支配定理、无参数购买、跨库正信号与 DBLP payload 边界 |
| `test39_one_third_backbone_20260712.md` | one-third closed components、backbone endpoint states、完备性与高层弱收益 |
| `test40_anchor_consumer_d2_20260712.md` | A1 consumer targets、exact threshold oracle 与 full DBLP D2 停止否决 |
| `test41_zero_residual_half_upper_20260712.md` | dual 零残量子图上的 half/三块 upper 与 full 无更新否决 |
| `test42_delayed_greedy_20260712.md` | D-layer rent-or-buy greedy 的 fast 正信号与 full DBLP 时间否决 |
| `test43_anchor_three_upper_20260712.md` | singleton-only anchor caterpillar rows、三块 upper 与 full C1 状态爆炸 |
| `test44_47_paid_backbone_upper_20260712.md` | paid anchor backbone 标量 blocks、witness lifting、fast 强收益与 full 时间硬失败 |
| `test48_branch_junction_closure_20260712.md` | triple roots 的共享父树 DP、fast D2 减半、full peak 改善与 query 时间硬失败 |
| `test49_pair_gradient_arc_dag_20260712.md` | pair gradient 定理、共享 arc-bit DAG、full 8.4% frontier 与 consumer 展开否决 |
| `test50_factorized_pair_mask_join_20260712.md` | arc factors、真实字节购买、29M events 与 125M bit probes 的接口二选一否决 |
| `test51_marginal_junction_endpoints_20260712.md` | paid/free path 两个 exact junction 端点、Test48 full 复现与同树 facility 扩张否决 |
| `test52_53_primal_skeleton_dual_feedback_20260712.md` | attachment centroid、双 dual max、full D2 replay 与 skeleton-order 单 dual 否决 |
| `test54_anchor_path_covering_dual_20260712.md` | 单 residual 的 anchor-path cover-all moat、Toronto-new 正信号与 DBLP 数量级反向 |
| `test55_barycentric_anchor_path_dual_20260712.md` | root-preserving path-average cap 与 sequential scalar-cap 模型否决 |
| `test56_58_balanced_residual_packing_20260712.md` | 同步 residual packing、D2-work 购买、progressive filling 与两次 full DBLP 硬失败 |
| `test59_balanced_irreducible_branch_20260712.md` | 较小侧不可拆分 branch 定理、随机精确性与 fast payload 不变否决 |
| `test60_top_closure_transposition_20260712.md` | top-D closure 交换、互补 half 奇偶边界与 Toronto A-half 成本否决 |
| `test61_three_function_scalar_completion_20260712.md` | 非 canonical A-half 的三函数标量消除与 canonical closure 剩余边界 |
| `test62_canonical_half_target_astar_20260712.md` | canonical half direct target A*、完整 top-closure 接口否决与低层回归边界 |
| `test63_pair_lipschitz_certificate_20260712.md` | D2 split function 的整张 1-Lipschitz 删除证书与五库零命中 |
| `test64_pair_work_anchor_20260712.md` | pair-work anchor 上界、full 正确信号与三库端到端退化 |
| `test65_recursive_anchor_order_20260713.md` | ordinary D 的递归 anchor 等价、顺序退化与双-anchor quarter 零收益 |
| `test66_dual_anchor_backbone_20260713.md` | 双-anchor paid backbone、跨库 upper 正信号与 nested 组合端到端否决 |
| `test67_skeleton_fixed_point_20260713.md` | attachment candidate tree 的无参数 fixed point、fast 结构规模与 conditional-skeleton 否决 |
| `test69_pair_path_profiles_20260713.md` | split-root path profiles、双 singleton transform 与 full 5300 万 profile 否决 |
| `test70_one_third_progressive_junction_20260713.md` | one-third endpoint + progressive junction 的 fast 新低、Toronto 强收益与 DBLP 硬门禁失败 |
| `test71_72_quarter_endpoint_20260713.md` | quarter endpoint 的完备性反例、exact-D 修正与 split 定向性能否决 |
| `test73_offline_d_global_a_20260713.md` | offline D + global A 的精确正交组合、状态计数与调度否决 |
| `test74_half_seed_global_20260713.md` | ordered half rows 作为 global 宏种子的逐条 frontier 否决 |
| `test78_pendant_cherry_two_interface_20260713.md` | macro cherry、三臂 block-anchor fixed-plan 定理、Toronto 单接口反例与第二接口边界 |
| `test78_soft_pendant_followup_20260713.md` | 嵌套闭包反例、soft-terminal block transform、g9 正证据与未证明边界 |
| `test81_d2_junction_local_facility_20260714.md` | D2 精确 pair 接入固定 junction tree，只获约 1.3% DBLP 前缀上界改善后撤回 |
| `test82_dual_row_certificate_20260714.md` | directed-cut 行级整次合并证书；不减状态且 fast20/Toronto g13 退化后撤回 |
| `test108_ordinary_component_minimum_20260715.md` | Test108–110 分量最小值在成员相交前后及 old→cut 顺序复核；拒绝不足且不减状态后撤回 |
| `test111_anchor_attachment_row_certificate_20260715.md` | permanent-anchor attachment 行标量在 join 前整块拒绝；昂贵路径低命中且不减状态后撤回 |
| `test112_113_completion_read_order_20260715.md` | Test107 component 证书的 singleton 门控与对称分量读序；工作量微降但 completion 不降后撤回 |
| `test114_bitmap_word_root_minimum_20260715.md` | bitmap root word 最小值整字拒绝；scans 降 6.30% 但局部时间中性后撤回 |
| `test120_online_anchor_facility_20260716.md` | Test119 设施上界的层内 rent-or-buy 反馈；D3 层末仅再少 0.20% values 后撤回 |
| `test122_projected_anchor_tree_profile_20260716.md` | 全 rooted row 到最近压缩锚树节点的 profile；Toronto g13 五询问候选逐项不变后撤回 |
| `test123_exact_triple_anchor_tree_20260716.md` | D3 精确 rooted triple 扩展最短路锚树；节点增至近 2 倍但上界与状态不变后撤回 |
| `test124_anchor_metric_closure_20260716.md` | junction 度量闭包放宽固定父树；q33 D2 values 降 2.45%，但 Toronto 五询问完整 wall 回退 1.29% 后撤回 |
| `test125_quarter_backbone_endpoint_20260716.md` | unrestricted quarter-backbone 修复旧 71/73 反例并过 130 次随机，但 Toronto g13 q3 返回错误权重后撤回 |
| `test126_dual_cancelled_word_certificate_20260716.md` | directed-cut 势消元后的 bitmap word 证书；q5 direct work 只降 0.4%--0.7%，wall/peak 回退后撤回 |
| `test127_paid_pair_witness_union_20260716.md` | D2 paid-pair skyline 见证并集上界；Toronto g13 q1--q5 仅 3/5 命中，q5 gap 10.93% 后撤回 |
| `test128_all_site_pair_envelope_20260716.md` | 每个 junction site 的 D2 min-plus 包络；Toronto g13 q3 仍有 6.76% gap 后立即撤回 |
| `test129_penultimate_a_completion_20260716.md` | 删除最终 A 层；随机门被已有 upper 掩盖，7/4/4 decoy-tree 给出 `27 -> 29` 反例 |
| `test130_penultimate_da_completion_20260716.md` | 同时删除最高 D/A 层；宽随机与固定 g15 均有 exact 反例后撤回 |
| `test131_final_consumer_mask_lower_20260716.md` | 最终 A consumer 的整 mask 行最小值下界；fast20 只拒绝 3.44%，重实例零命中后撤回 |
| `test132_transposed_final_consumer_20260716.md` | 最终 A consumer 的精确转置闭包；fast20 与 Toronto 五询问合计回退后撤回 |
| `test133_136_exact_row_cones_20260716.md` | exact rooted-row 度量锥与锚路径标量锥；理论合法但多询问面板状态收益不足、wall 回退后撤回 |
| `test137_early_anchor_pair_lower_20260716.md` | 提前 exact A1 作为普通 D 的 anchor-aware future；Toronto 五询问显著正向，但 DBLP g15 q33 D3 wall/peak 回退后撤回 |
| `test138_quarter_two_root_upper_20260716.md` | quarter witness 的 2+2 双汇合点上界；fast20 与 Toronto 五询问状态均不变，未进入 DBLP 即撤回 |
| `test139_role_directed_joint_word_certificate_20260716.md` | 严格支配 Test126 的角色定向同根 word 证书；跳过数百万根但 Toronto 五询问 wall/peak 回退后撤回 |
| `test140_anchor_tree_witness_union_20260716.md` | Test121 设施方案恢复真实边并去重；仅 q1 减状态，边际并集选择五询问全部更弱后撤回 |
| `test141_facility_guided_macro_lifting_20260716.md` | Test121 分块的 `3^(g/2)` 全图 macro lifting；fast20 states 小降但 wall 回退 52.86% 后撤回 |
| `test143_root_irreducible_adjoint_terminal_20260716.md` | root-irreducible 高层终端使 checks 下降但底层扫描不降，Toronto g15 伴随时间持平后撤回 |
| `test147_lazy_dual_20260717.md` | MovieLens eager dual 过重，但 lazy 信号到 D3 才出现，D3 五问回退后撤回 |
| `test148_indexed_ordinary_queue_20260717.md` | decrease-key heap 量化 DBLP/LinkedMDB stale 成本；属 baseline 可共享工程优化，源码与目标撤回 |
| `test150_151_153_156_dual_ablation_20260718.md` | small-`g` no-dual 归因、一次性 lazy dual 失败、重模块消融与 quarter upper 物理删除依据 |
| `test85_ordinary_last_use_reclamation_20260715.md` | 普通 D 行最终消费者公式；联合 payload 下降但 fast20 时间回退后撤回 |
| `test86_dual_reduced_prefix_join_20260715.md` | 对偶约化前缀与二维顶点交集冲突；理想 report 工作仍不足后撤回 |
| `test88_complement_dual_prefilter_20260715.md` | 补集 directed-cut 预筛选；fast 退化、q5 亚百分比收益且增加内存后撤回 |
| `test89_dual_prefix_short_circuit_20260715.md` | 同序 cut 前缀保持 exact double，但 q5 只少读约 9% terms 且 wall 退化 8.6% 后撤回 |
| `test91_paired_top_mask_order_20260715.md` | 补集成对让 Toronto D6 更早剪枝，但 fast20 跨库退化约 2% 后撤回 |
| `test92_online_a0_label_feedback_20260715.md` | A0 completion 前移到每个 accepted label；70 万次分散查找只少 101 pops 后撤回 |
| `test93_seed_boundary_a0_completion_20260715.md` | seed 自然边界批量有序相交；67 万扫描只少 15 pops 后撤回 |
| `test94_phase_boundary_dual_compaction_20260715.md` | D/A 边界以新上界重筛普通 row；payload 仅降 4.35%、A 状态几乎不变后撤回 |
| `test96_bounded_d4_full_a_20260715.md` | 3/4 树分解不能共享成固定-anchor D4/full-A；g13 双候选组 `71 -> 73` 反例 |
| `test97_progressive_a_prefix_20260715.md` | one-third 边界提前生成完整 A 前缀；D 高层下降但 A/peak 膨胀，跨库不稳定后撤回 |
| `test99_completion_root_epoch_20260715.md` | A completion 根集合 epoch 共享使局部降阶，但 DBLP q5 两次端到端退化后撤回 |
| `test100_bitmap_branch_semijoin_20260715.md` | occupancy 与 branch bits 融合半连接只获局部收益，fast20 端到端中性后撤回 |
| `test101_branch_exception_layout_20260715.md` | branch 补集例外表有额外 q25 空间上界，但 fast20 时间退化 10.7% 后撤回 |
| `test102_layout_independent_bitmap_semijoin_20260715.md` | 布局无关半连接使 fast direct work 降约三成，但 DBLP q5 两次端到端退化后撤回 |

引用归档数据时必须说明：是否为当前 Release/O2 重跑、是否来自已撤回二进制、是否产生最终权重。能代表当前实现的数字应整理到对应 `effect_report.md`，不能直接把 archive 当成主线结论。
