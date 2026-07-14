# 当前研究边界

更新时间：2026-07-14。ReleaseV4 是当前纯框架 A 发行版，见 `release_v4.md`；
ReleaseV3 冻结为框架 B 对照。ReleaseV4 保留 Test21 的离线有序 D/A rows，合并
branch-junction incumbent 与 D2-work delayed progressive residual packing，全程不
调用 Test80、ReleaseV3 或框架 B。

ReleaseV4 的 Release/O2 结果为：fast20 `8.303s`，small35 `2.148s`，Toronto full
g13 `7.857s / 58.0MiB / 0.7048467020`，DBLP full g13 q1
`544.379s / 2158.6MiB / 12.5936282853`。相对 Test80 full，发行清理使时间降低
`18.3%`；相对 ReleaseV3/B，时间只慢 `2.4%`，峰值低 `44.2%`。

2026-07-14 的 Test80 跨询问剖析按截止时间完成 19 条新 DBLP g13 询问。wall/peak 中位数为 `2042.8s/5874.1MiB`，最大值为 q25 的 `7619.9s/25541.4MiB`；历史 q1 只有两项新中位数的 `32.6%/36.8%`，不能代表整批询问。D 在 16/19 条中比 A 更耗时，D3/D4 是主要空间层；A5 虽流式消费仍是最大的 A 时间层。合并候选有 `94%--97%` 在生成后才被下界拒绝，下一主线是保持无 Hash 的离线有序归并，同时让归并在物化候选前感知合法下界。完整事实、逐询问 CSV 与逐层 CSV 见 `test80_dblp_g13_cross_query.md`。

Test21 仍是冻结的纯 A 基线：permanent anchor 形成唯一 A 主干，ordinary D 只以根不可拆分 branch basis 接入。ReleaseV4 在同一状态语义上增强上下界，不恢复 soft-pendant、one-third endpoint 或任何未证明接口。paid second-interface/half-profile 仍是开放理论问题。

Test21 的冻结 Release/O2 结果为：fast20 `12.380s`，small35 `2.252s`，full Toronto g13 `20.788s / 102.9MiB`；历史 DBLP gate 未完成。这些数字只描述基线，不覆盖 ReleaseV4/Test80 的结果。

Test44--47 撤回后的正式源码已重新通过 Release/O2 编译、随机 DPBF 对拍 `100/100`（seed `712921`）和 fast20 `12.743s`（`20260712_173406`）。这一快集只验证撤回后行为，原 `12.380s` 仍是同一算法的基准；没有重新启动 full DBLP。

branch basis、在线 D_t 和 root-quarter 已把 Toronto D4 retained values 从 `4.333M` 降到 `1.684M`，D5/D6 仍为约 `0.60M`。但 DBLP gate 证明继续叠加 common-root 上界不足以解决大图；下一机制必须在图传播前减少 rooted states。balanced-only split、root-major convolution、全局 D/A 调度和全图 quarter envelopes 均已有否决证据，不能重复包装。

最新 Test26 分层把 DBLP 边界进一步定位到 ordinary D2：公共预处理与隐式 D1 在 `53.8s` 完成，但直接运行约三分钟时 D2 仍未结束，RSS 已约 `2.12GiB`，A 尚未开始。这与历史 `125.6M` pair states / `235.1s` pair search 一致。Test25 同时给出 fixed-mask full closure 的反例：anchor trunk 会沿途吸收 nonanchor groups，不能用一次 full D closure 删除 A。Test27 的 single-pair generated-star、Test28 的 fixed-root multi-pair matching 和 Test29 的 variable-root pair forest 在 fast DBLP/DBLP-new 上均为 0 次 best 更新，D2 payload 完全不变；代码均已撤出，且没有据此启动 full DBLP。Test29 也确认该方向与 Test18 旧负实验重复。详见 `archive/test25_27_anchor_state_probes_20260712.md`、`archive/test28_pair_matching_upper_20260712.md` 与 `archive/test29_variable_root_pair_forest_20260712.md`。

Test30 进一步检查能否用 D row 的 local split seeds 替代 A+D 中的 propagated branch values。小随机图曾通过，但扩大到 `n=8..16,g=6..12` 后在 seed `712611` iteration 55 得到 `exact=35 / candidate=36`；fast Toronto g12 也从精确 `0.96162278` 退到 `0.96886855`。这证明单边界 A 状态不能总把 ordinary branch 的连接路径重新排序进 anchor trunk；删除 D closure 需要第二边界或等价共享主干证书，而显式 `n^2` 边界枚举不可接受。详见 `archive/test30_split_seed_anchor_recurrence_20260712.md`。

Test31 随后把 A 扩展到完整 `2^(g-1)` lattice，只允许接入完整 D1/D2 pendant cherries，试图删除 D3+。seed `712621` iteration 39 的 `n=9,g=10` 反例为 `exact=74 / candidate=75`；读取完整未剪 D2、关闭 A/D 剪枝以及独立重写 recurrence 均复现 75。原因是 A 单根状态不知道已付费主干中的内部顶点：沿一侧接入 pair 后改根到该侧更深 attachment 会重复支付已有边。详见 `archive/test31_pair_cherry_anchor_lattice_20260712.md`。

Test32 为每次接入的 D1/D2 重建确定 witness，并把同一树成本免费播种到 witness 的全部顶点；同一 `74/75` 反例仍失败。决定性缺口来自 A 自身的图传播：较高成本但包含更长已付费路径的 witness 在旧 root 会被较低标量 cost 删除，二者对未来 attachment 实际 Pareto 不可比。所以下一步不能只加 predecessor、单个第二 root 或单棵 witness，而需要可证明受控的 paid-attachment representative family。详见 `archive/test32_witness_saturated_anchor_20260712.md`。

Test33 转而让 farthest-anchor global 在首次 half-size label 改善 incumbent 时停止，只作为 A 的上界 oracle。DBLP-new fast 把 `11.0064` 降到 `10.3638`，但 Toronto 两库改善很弱；full DBLP g13 bounded 在 `64.764s / 32,568 settled / 5.47M created` 时只把 `15.0174` 降到 `14.8912`。收益不足以支付预热，继续选择更深停止点又会变成经验时刻，因此模式已撤出。详见 `archive/test33_first_half_global_warm_start_20260712.md`。

Test34 检查 dual objective 与 incumbent 相等时能否直接证明最优。seed `712701` iteration 5 得到 `objective=incumbent=28`、DPBF `27`，说明当前 root-star-rooted objective 不能作为自由根 GST 的全局下界；fast DBLP 的数值相等只是实例现象。提前返回已撤回，详见 `archive/test34_dual_objective_certificate_20260712.md`。

Test35 把 permanent anchor group 的所有 terminals 作为零代价 super-root roots，直接替换单根 dual。该 objective 是合法自由根 GST 下界，扩展后的随机自检为 `300/300`；但 super-root 免费连接多个 anchor terminals，使 future potential 丢失单棵实际主干的几何约束。fast20 从 `12.380s` 退到 `27.792s`；DBLP-fast g12 在强制复用正式精确 `best` 后仍保留 `1.173M` ordinary values，而正式单根 dual 只保留 `0.050M`。替换与双 dual 均不进入 Test21，也不触发 full DBLP，详见 `archive/test35_anchor_group_dual_20260712.md`。

A 的 paid-attachment 缺口也已得到一般边界：串联 `r` 个独立 diamond 可在同一 `(S,v)` 下产生 `2^r` 个 cost/attachment Pareto 不可比 partial trees。所以下一步不能假定每个 row 只需常数或多项式棵显式 witness；必须利用 query、共享 DAG、代数表示或新的有界边界隐式消掉反链。证明及与 rank-based/treewidth 文献的区别见 `history/paid_attachment_representative_family.md`。

Test36 尝试在 D2 直接共享传播：一个 source wave 同时携带全部 pair offsets，只把实际改善的 pair bits 继续沿边传播。方法精确且 DBLP/MovieLens 的 accepted events 分别只有独立 pair states 的 `28.6%/9.0%`；但 pair-specific offsets 无法共享一个 label-setting key，最终产生 `146x/2711x` pair checks 和 `1.26M/17.42M` queue peak，wall 比逐 pair Dijkstra 慢 `22x/45x`。源码与目标已删除，见 `archive/test36_pair_vector_wave_20260712.md`。

Test37 检查能否只用 singleton/pair pendant branches 生成 ordinary rows。只比较自由根答案曾出现 10,000 个随机假正信号，但逐 `(mask,root)` 后在 seed `712761` iteration 11 得到 `exact=44 / pair-bounded=45`；精确 seed 必须做 `3+3` split。固定小 branch 不能替代完整 rooted accumulator，见 `archive/test37_bounded_branch_recurrence_20260712.md`。

Test38 给出一个有效但不足的离线优化：若邻点 seed cone 全局支配当前 seed，则只保留局部极小 source；邻边扫描由 `sum degree <= S log S` 的无参数工作量规则购买。它在同机 fast20 改善 `1.6%`，Toronto full wall 改善 `3.4%`，但不减少 retained D2 values。bounded DBLP 在约 `495.7 CPU-s / 3.98GB` 时仍无最终结果，已接近 V3 完整时间线，故机制撤出并归档于 `archive/test38_local_seed_cone_20260712.md`。

Test39 用 one-third closed components 沿 backbone path 构造 ordinary `P` 与 anchored `A` endpoint rows，只需三处限制即可复用当前有序列表和 `A+P+P` completion。完备性与随机对拍均成立，但只把 Toronto full 的 size-6 values/pops 减少约 `4.3%/4.5%`，fast 与 full wall 没有改善；D2 丝毫不变。该状态解释保留于 `archive/test39_one_third_backbone_20260712.md`，不进入当前代码，也不触发 DBLP。

Test40 检查 explicit D2 barrier 的 consumer-driven 出路。exact-best A1 targets 在 fast DBLP 很稀疏，但 full DBLP pair union 平均占 `59.1% n`；加入 `best-A1-future` 的逐 root threshold 后，三对代表 pair 仍 settle 完整正式 row 的 `99.7%--99.9%`，wall 慢 `1.03x--1.33x`。工具与构建入口已删除。与此同时，exact best 把这三对正式 D2 降到 `11k--126k` states，说明下一主线应转向 D2 前的 anchor-aware feasible upper，而不是惰性 D2 输出。详见 `archive/test40_anchor_consumer_d2_20260712.md`。

Test41 把 half/三块 upper 前移到 dual 零残量有向子图。fast 两库有更新且 rows 仅 `18--67ms`，但 full DBLP 的 `162k` zero arcs 上运行 `2.16s / 1.33M values` 后仍为 `17.360814`，没有超过 dual primal。restricted topology 过窄，代码已撤回；随后 Test42 复用 ReleaseV3 的 work-triggered ordinary-graph greedy，检查 Test21 在 D2 前的 `17.36` 对 `15.0174` incumbent 差距。详见 `archive/test41_zero_residual_half_upper_20260712.md`。

Test42 实现了该 rent-or-buy greedy：fast20 为 `12.182s`，比正式 Test21 快 `1.6%`，但 full DBLP 在 `558.7 CPU-s / 4.31GB sampled peak` 时仍没有最终输出，已经越过 V3 的 `531.556s`。因此共享 greedy、work counters 与临时结果均撤回；这条结果把下一候选进一步限定为 D2 前的 anchor-aware 强 incumbent 或真正的 D2 状态消除，而不是继续调整通用 greedy/compact。详见 `archive/test42_delayed_greedy_20260712.md`。

Test43 令 `q=ceil((g-1)/3)`，只用 singleton branches 构造 anchored caterpillar C rows，并把三个 C blocks 在同根求和。fast 两库的 C-row star completion 有更新，但三块本身五库均无更新；full 在 dual `17.36` 和合法 greedy `15.0174` 两种初值下都未完成 size 1，后者终止于 `234.8 CPU-s / 2.33GB sampled peak`。因此 early A1/C1 显式 rows 也已否决；下一结构必须共享一次可证明的 anchor backbone，而不是累加多棵独立 A tree。详见 `archive/test43_anchor_three_upper_20260712.md`。

Test44--47 首次把该要求实现为 paid anchor backbone：一条 root-star-to-anchor path 只付费一次，每个 singleton/pair/triple/提升 block 用一个 `min_x` scalar attachment 表示，partition blocks 可挂到主干不同位置。fast20 最低 `10.112s`，full upper `17.3608 -> 13.1619`，sampled peak 最低约 `2.01--2.18GB`；但 pair/triple/q-full/witness-lift 四个集成版分别在约 `545.3/543.1/551.5/555.0 CPU-s` 仍无最终结果，全部超过 V3。正式代码和工具撤回，保留结论是：scalarization 有效，下一步应扩共享 skeleton 几何而非 block arity。详见 `archive/test44_47_paid_backbone_upper_20260712.md`。

Test48 随后只扫描 triple attachment argmin roots，把它们到 paid backbone 的确定性 shortest-path parent union 压成小树，并以 subset DP 让公共前缀只付费一次。full upper 继续降到 `13.019989`；集成版随机 `300/300`、fast20 `9.758s`，D2 values/pops 为正式版 `0.50x/0.45x`，full sampled peak 约 `2087MiB`。但修正图加载计时后，唯一有效 gate 在 `535.507s query` 仍无最终权重，超过 V3 `531.556s`；恢复 junction witness 后 edge union 也没有再下降。代码与工具撤回，下一方向必须让 skeleton 替代/隐式表示 D2，而不能继续只加强 incumbent。详见 `archive/test48_branch_junction_closure_20260712.md`。

Test49 给出一个精确 D2 接口定理：pair forest child `v` 仅在 consumer `A(v)<A(parent)` 时可能成为 `closure(A+D2)` 的必要 source。full exact-best 下 permanent-anchor consumer 只保留 `8.947M/106.310M=8.416%` sites；把全部 pair parents 聚合为 `(directed edge,pair bits)` 只需 `449,127,056` bytes，full 枚举与回溯 0 error。但逐 pair join、arc-batched A、arc-batched ordinary+A 三种 solver 形态 fast20 为 `16.834/13.245/14.941s`，最后一种产生 `28.95M` 显式 target events 和 Toronto-new g12 `109.7MiB` peak。说明第二边界已能共享，consumer mask 维仍未共享；所有实验代码撤回，见 `archive/test49_pair_gradient_arc_dag_20260712.md`。

Test50 保持 `(consumer,arc,pair-bitset)` 因子化，不创建 target events，并改用 arc bytes 与实际 sparse/dense pair certificate bytes 的精确比较购买。扩大随机 `500/500` 通过；fast20 `14.080s`，虽把 Toronto-new g12 peak 从 Test49 的 `109.7MiB` 降到 `63.4MiB`，但 `5.42M` factors 被 targets 查询 `125.0M` 次、只有 `27.1M` 命中。故显式 events 与 factor membership 都不能消除 consumer-mask 乘数；代码撤回，见 `archive/test50_factorized_pair_mask_join_20260712.md`。

Test51 检查 Test48 是否漏掉“共享路径已由别的 block 支付”时的 junction：每个 triple 同时保留 `argmin(q+d_P)` 与 `argmin(q)`，再运行同一压缩父树 DP。fast Toronto/DBLP-new upper 有改善，但 full DBLP 的候选/压缩树从 `24/20` 扩到 `84/86` 后仍严格为 `13.019988893`，卷积增至 `45.70M`。因此同一 parent tree 上扩 facility Pareto 端点不再是主线；探针与目标均撤回，见 `archive/test51_marginal_junction_endpoints_20260712.md`。

Test52--53 首次把 paid skeleton 用作 dual orientation：恢复的 full attachment centroid `211842` 与 root-star `24492` 不同，两个 dual 的 max 把 Test48 best 下 D2 settled/pushes 降 `9.6%/11.1%`。但 max replay 本身由 `40.31s` 变慢到 `43.62s`，第二 dual 另需 `39.12s`；fast20 也为 `10.130s`。用 attachment 深度重排一次 root-star dual 虽随机 `300/300`，fast20 更退到 `11.565s`。所有代码/API 撤回；后续 primal/dual coupling 必须共享一次 residual construction，见 `archive/test52_53_primal_skeleton_dual_feedback_20260712.md`。

Test51--53 撤回后的正式 Test21 已重新通过 Release/O2 编译与随机 DPBF 对拍 `100/100`（seed `713131`）。没有启动新的 full solver；正式基准仍为 fast20 `12.380s` 与前述失败的 DBLP gate。

Test54 在一次 residual construction 内把每个 group moat 增长到覆盖 root-star-to-anchor paid path，试图免去 Test52 的第二 dual。随机 `300/300` 通过；Toronto-new D2 values/pops 降到正式版 `0.51x/0.46x`，但 DBLP/DBLP-new 分别膨胀到 `18.38x/4.40x`，fast20 为 `15.347s`。cover-all cap 让早处理 groups 过度占用 residual，机制撤回且不触发 full，见 `archive/test54_anchor_path_covering_dual_20260712.md`。

Test54 撤回后的正式 Test21 已重新通过 Release/O2 编译和随机 `100/100`（seed `713151`）。当前代码仍是 Test21 正式基线，不含 covering-root API、anchor path dual 或实验统计。

Test55 用 root-preserving path-average cap 平衡 Test54 的最远点 charge，随机 `300/300` 通过；fast20 为 `13.198s`，DBLP/DBLP-new D2 values 仍膨胀到 `16.31x/2.48x`。这证明跨库反向来自 sequential scalar-cap 语义，而不只是 max 聚合过激；代码撤回且不触发 full，见 `archive/test55_barycentric_anchor_path_dual_20260712.md`。

Test55 撤回后的正式 Test21 已重新通过 Release/O2 编译与随机 `100/100`（seed `713171`），不含 barycentric-root API 或路径统计。

Test56--58 在正式 root dual 的剩余 residual 上同步 packing 所有 group 的 anchor-path 增量势，按实际 D2 工作购买，并接回 Test48 junction upper；历史 Test58 fast20 `9.390s`、Toronto full `9.201s/58.0MiB`，但在 `531.671s` 门禁被停止。当前 Test80 按同一已证明机制重建，不再以 B 的时间作为强制停止条件，并于 `666.509s` 完成 full。历史门禁与当前完成结果分别保留，见 archive 与 Test80 主文档。

Test21 仍保持撤回后的冻结源码；junction helper、progressive dual API、购买逻辑与统计只进入独立 Test80，不通过开关回灌 Test21。

Test59 随后证明 ordinary rooted tree 总有一个不超过半集的根不可拆分分支，并用较小侧/等分 pivot 消歧替换固定-pivot accumulator。随机 `500/500`（seed `713231`）精确，但 fast20 为 `13.016s`；ordinary values/pops、A values 与 completion checks 相对正式版变化都小于 `0.02%`。该定向没有删除 rooted payload，只改变等价 split 顺序，源码已撤回且不触发 full，见 `archive/test59_balanced_irreducible_branch_20260712.md`。

Test60 首次真正删除 top rooted states：利用 `min C(f)+g=min f+C(g)`，在 `k=2h-1` 时只保留最高 D split generator；fast g12 汇总改善 `3.8%`。但 `k=2h` 需要互补 A-half rows 承担 anchor 方向，Toronto full 的 D6 values/pops 虽下降约 `45%`，A merge probes 却由 `43.40M` 增至 `73.42M`，wall `20.788s -> 25.121s`。这与 DBLP g13 属同一结构类，故代码撤回且不运行 DBLP，见 `archive/test60_top_closure_transposition_20260712.md`。

Test61 用第二次全局 min 交换把非 canonical `A_N=C(Q_N)` 精确改写为 `Q_N+D_C`，以 canonical closed-D roots 驱动 `A_X+branch-D_Y+D_C` 三路 scalar completion。随机 `500/500`（seed `713271`）通过；Toronto 的 anchored masks 从 Test60 `2510` 降到 `2048`，wall 回收到 `23.838s`，但 canonical A-half 仍使 merge probes 达 `60.34M`，正式版仅 `43.40M`。因此仍未通过大实例前置门，代码撤回，见 `archive/test61_three_function_scalar_completion_20260712.md`。

Test62 直接从 `M_N` 运行以 anchor+C lower bound 排序的 target A*，只在弹出点查询 `Q_C(v)`，从而完全删除 A-half rows。随机 `500/500`（seed `713291`）精确；Toronto 只弹出 `22,513` 个 target states，但 top generator 物化与 `1.047M` point probes 仍使 wall 为 `23.786s`。至此 top closure 的 row、scalar、target 三种接口均慢于正式 `20.788s`，全部撤回且不跑 DBLP，见 `archive/test62_canonical_half_target_astar_20260712.md`。

对 Test49--50 提出的 mask-side symbolic convolution 进一步完成文献与结构核验：经典 fast subset convolution 对 min-sum 的 exact 加速含有界整数值域 `M`，本仓库实权/`1e-6` 口径不能直接使用；现有 strongly polynomial 结果是近似算法。另一方面，full DBLP 只有 `162,166` 条 directed zero-residual arcs，相对 `2,497,782` 个顶点的任意 contraction 最多减少 `6.49%`。generic tropical transform 与 zero-SCC quotient 均不足以消除 D2，见 `history/d2_symbolic_convolution_boundaries.md`。

Test63 又检查最强的整张 pair 预证书：若 `gd_i+gd_j` 已为 1-Lipschitz，则 D2 closure 等于 split seeds且无 branch values。五库 fast g12 的 `275/275` pairs 均有大量严格 violation，`closed_pairs=0`；最少也有 `2528` 条违反边。该方向无需 full 即否决，临时 probe 已撤回，见 `archive/test63_pair_lipschitz_certificate_20260712.md`。

Test64--66 检查 anchor 是否能直接消除 D2 维度。pair-work anchor 在 full 结构上准确选中最少 settled 的组，但三库端到端退化；recursive `D0+B` 虽由 Pascal 恒等式精确保持状态族，fast20 仍为 `13.755s`；双-anchor paid backbone 在 `18/20` 条改善 upper，却只能把单独/组合版做到 `12.867/12.889s`。三组结果共同说明：换 anchor、递归 mask 顺序和增加 anchor paths 都只是移动状态或加强 incumbent，不能替代 Test48 已证明有效的 junction-prefix sharing。详见 `archive/test64_pair_work_anchor_20260712.md`、`archive/test65_recursive_anchor_order_20260713.md` 与 `archive/test66_dual_anchor_backbone_20260713.md`。

Test67 又把 Test48 attachment tree 迭代到无参数 fixed point。fast 上候选树规模稳定，但 DBLP g12 upper 不变，五条 g12 仍有至多 `6.60%` exact gap；这说明迭代扩展同一 conditional parent tree 仍不具最优 skeleton 覆盖性。该 probe 已撤回且不触发 full，见 `archive/test67_skeleton_fixed_point_20260713.md`。

Test49 gradient 现已严格加强为 predecessor ancestor/tight-cone dominance：单 consumer 只保留沿 tight path 的 prefix minima，随机逐点闭包 `2000/2000`。Toronto 等三库 sites 可再降约 `28%--31%`，但 DBLP/MovieLens 仍保留 parent frontier 的 `98.15%/99.61%`；因此该 exact 算子进入 history，不单独运行 full，剩余问题仍是同时共享 pair bits 与 consumer masks。见 `history/pair_tight_prefix_frontier.md`。

Test69 证明固定 `(split root,attachment)` profile 的 pair-mask join 可由两次 singleton transform 精确完成。fast DBLP 的 profile/state 为 `7.52%`，但 full DBLP 反弹到 `53.17M/106.31M=50.02%`；平均 profile 只承载 2 个 pair labels，无法承担 `O(k2^k)` mask transform。该 factorization 已撤回，见 `archive/test69_pair_path_profiles_20260713.md`。

Test70 把 Test39 one-third endpoint 与 Test58 junction/progressive packing 正交组合。fast20 达到 `9.161s`，Toronto full 为 `9.64--9.69s/58.1MiB`；但 DBLP 有效 gate 在 `531.831s/2160.4MiB` 仍无 weight，严格未越过 V3 `531.556s`。这否决了“只删除 P5/P6 branch publication即可跨线”，详见 `archive/test70_one_third_progressive_junction_20260713.md`。
Test71--72 进一步核验 quarter endpoint：禁止高层 row 作为 branch 会在 seed `713581` iteration `428` 丢失最优解；恢复 exact D 语义后虽累计随机 `3000/3000`，fast20 仍退化到 `12.843s`。因此当前不再微调 endpoint 阈值或 split orientation，后续候选必须真正删除/隐式共享 D2 与 consumer-mask 乘积，详见 `archive/test71_72_quarter_endpoint_20260713.md`。
Test73 又补齐 offline D + global A：状态精确且无 Hash，但 Toronto-fast g12 只减少 `6.7%` settled A，created/point-completion 增长使 wall 退化 `3.5x`。所以 Test22、Test23、Test73 已共同否决四处调度搬移；后续不再把 B 的 heap 顺序单独包装进 A，见 `archive/test73_offline_d_global_a_20260713.md`。
Test74 尝试真正共享 A/B 公共工作：把 V3 切换前 exact half rows 注入 global。fast20 的 settled labels 逐条与 V3 完全一致，说明这些值不携带 B 无法重建的结构证书；created 多 `15,592` 且 wall 退化 `5.3%`。因此后续 half-state 必须增加 paid attachment/边界语义，不能只是复用标量 `(mask,root,cost)`，见 `archive/test74_half_seed_global_20260713.md`。
当前正向 Test75 基底已定义为 root-free paid-attachment profile：`phi_T(B)=min_{x in T}D(B,x)`，并以 `cost(T)+phi(B)+phi(C)` 做 exact 三块 completion。穷举随机共 `1250/1250`；fixed g8 将 `7.52M` declared subtrees 压到 `42,963` Pareto states，最大 front `117`。该结果解决了状态语义和 completion，但未解决大图生成；下一步只做 pair predecessor path 的 profile frontier 探针，未出现数量级压缩前不实现完整 solver或运行 DBLP query，见 `history/paid_attachment_half_profiles.md`。
pair-profile 大图门槛现已通过：单-anchor profile 可由 `(D2(v),gd_anchor(v))` skyline exact 投影，五库 fast g12 仅保留 `0.407%--1.672%` roots；full DBLP g13 为 `821/106,310,433`、每对最多 `23`，结构测量 `139.710s`。更严格的全 singleton root-vector 在 DBLP/MovieLens 仍为 `4.3%--6.9%`，但 Toronto 两版回升到 `49.5%--62.1%`，所以不能直接作为生产状态。当前 Test75 进入“从 anchor skyline seeds 按 completion 需求列生成内部多点 block profiles”阶段；仍未运行完整 DBLP query solver，见 `history/paid_attachment_half_profiles.md`。

确定性内部骨架的 singleton completion 在 fast 五库上很强，但 full DBLP g13 只得到 `13.3957483032`，相对 exact gap `6.3693%`；其 dense `g^2 n` profile 探针还使用约 `4.81GiB`、`262.877s`。这明确把下一接口限定为按需求生成 block 列，并以 `paid tree + half block + half block` 捕获剩余组之间的共享路径；singleton 求和不进入正式 Test21。

pair-only block completion 已形成更强正证据：五库 fast g12 的 `paid pair + half + half` 全部 exact，而先付 anchor 在三库留有 gap；早期 unrestricted rooted-pair 小图穷举在 `g<=7` 为 `3000/3000`。permanent-anchor skyline 在五库同样 exact，但随机仅 `2998/3000`，seed `713731` iteration `365` 为 `59 -> 60` 反例。后续 fixed g13 又发现 unrestricted pair-only 本身的 `160 -> 161` 反例，因此这些结果只说明初始列质量，不能再写成一般完备性。详见 `archive/test75_pair_block_completion_20260713.md`。

full DBLP g13 的 incidence-only 回溯显示 `821` 棵 skyline trees 总计仅 `3,302` 个 tree-vertex incidences、`836` 个 unique targets、单树最多 `7` 点；结构时间 `166.382s`、峰值约 `1.32GiB`。这通过了 target-set 规模门，下一原型直接围绕 `(block,target vertex)` 输出，而不是继续压 root row 容器。

把这些 profiles 旁挂到旧 A rows 已被实测否决：Toronto/Toronto-new g12 的 A values、merge probes 与 completion checks 逐项不变，wall 从 `1.791/3.609s` 退到 `1.909/4.039s`。代码已撤回并以随机 `100/100`、Toronto `1.816s/18.2MiB` 确认正式版恢复。下一实现必须从生产接口删除 full-root A rows，而不是增加一个 upper consumer。

仅从 paid pair incidences 播种的 global A 也已否决：fast DBLP 虽 exact，但 `2,047` seeds 重新扩成 `1.483M` created / `1.193M` settled labels，global 阶段 `18.039s`。逐 root 图传播会重建 consumer-mask 乘积，故实验模式已撤回；下一接口只能直接产生 tree-profile scalars。

Test75 low-core 现只需 `q=ceil(h/2)` 阶 rooted rows：rooted pair 按 balanced low blocks 做真实 edge union，达到 `g-2q` core 后以两个 q-blocks 完成。unrestricted 生成穷举 `3000/3000`、fixed g8 `100/100`；DBLP-fast 用 D1--D3 exact，core `15,779`、max front `45`。但 Toronto/Toronto-new 仍差 `0.1982%/0.00689%` 且 fronts 为 `578/1,208`；预展开全 pair profiles 在 `6.73GiB` 未完成，单一 incumbent-plan pricing 又无改善。故不触发 full，下一问题精确收束为 partition-level column pricing。

partition pricing 现已加入 exact strong lower。fast g12 五库闭合所需 plan calls 为
`1313/2253/929/370/4`，但前四库覆盖 `43--55` 个 pair，说明必须按 pair 批量传播
requested columns。Toronto g13 的 `3-core+5+5` skyline upper 为 `0.7401396340`；同一
plan 全 root 定价在 `1.062s` 内得到 exact `0.7048467020`。临时 Test77 在 A4 后也命中
exact，但 paid phase `6.097s` 且不能证停，端到端 `20.788s -> 29.113s`；component
lower 仍有 `28,150/36,036` plans 低于 exact。故 Test77 代码已撤回，不运行 DBLP。
下一突破点明确为“strong plan lower + pair-batched root pricing”的完整证书；在证书闭合
前不得删除 D6/A5，也不得把 exact upper 命中写成维度消除。

完备性审计现进一步否定 q5/core3：fixed g13 有 `106 -> 107` 反例，允许 singleton
在 pair tree 全部内部点接入仍失败。相邻 `q5/core4+4+5` 用 sequential singleton 与
single 2-block 两种生成在 g13 累计 `720/720`；扩展 odd-g、Steiner 顶点、多终端组和
高环数后 targeted 总计 `13,520/13,520`，仍是证据而非 decomposition 定理。Toronto
g13 以 `23,864` core states、max front `70` 命中 exact。两棵 pair forests 的
requested-profile 离线合并现已实现：persistent argmin lists 保留全部并列 attachment，
dense stamp 去重，全程无 Hash。五库 fast 的 `4,387,627` 个 attachment sets 与 `8,250`
个 plan prices 对显式实现均为零误差；Toronto g13 同一 `5,476` plans 从 `39.158s` 降到
`18.721s`，dense-oracle 总时间约 `145.7s -> 111.0s`。但 oracle 使用 supplied optimum
筛选 plans，core4 覆盖性也未证明，故正式 Test21、D6/A5 均不修改，不触发 full DBLP。

rooted-D4 是更简洁但已被否决的替代：Toronto g13 同一 plans 定价只需 `8.023s`，但
fast MovieLens g12 的 deterministic D4、全部 tight D4 witnesses 三标量闭包、六标签
macro DP 分别只到 `0.0202203614/0.0202203614/0.0202203613`，均高于 exact
`0.0202189774`。因此缺失是非最优 paid-core 的 cost/profile Pareto geometry，不是 tie、
括号顺序或 D4 witness 恢复常数；这些模式只保留在 probe，不能据此接入 Test21。

Toronto exact pair+half partition 在 `18,480` 个 plans 中，以 root-star exact-high-row score 排第 `162`，仅用 low-row top seeds 则排第 `804`；当前 incumbent partition 的 `210,000` 个 root plans 全扫仍无改善。因此 low score 只能排序，不能形成 full 可接受的 exact pricing 证书。

完整 priced-value 次序进一步确认：low score 下首个 exact plan 在 Toronto/Toronto-new 分别为 `804/3293`。q2--q5 扫描也没有跨库折中：q2 状态爆炸，q4 质量变差；q5 在 Toronto/DBLP 两版 exact，但 Toronto-new skyline 差 `0.5751%`，扩到 `77,606` 个 all-root paid states 才 exact。故当前不修改正式 Test21，也不触发 Toronto/full DBLP。

Test20/26/36/38/39 的共同必要条件已整理为 `history/explicit_d2_output_barrier.md`：只要接口仍先物化全部 `D({i,j},v)`，就至少支付 full DBLP 已观测的 `125.6M` value productions；压字节、压 heap sources 或削 D3+ 都不能改变该输出规模。下一原型必须是 consumer-driven targets、可批量查询的隐式 exact oracle，或从完备性中彻底删除任意-root D2。

A/D 补集双向搜索也已在 Test22 中完成闭环：它精确、无 Hash，并把单个 fast g12 从初版 `79.5s` 优化到 `4.0s`；但 fast20 仍为 `21.715s`，且 settled labels 相对 V3 在四库多 `1.44x--2.66x`。该代码和快照已撤出，证据见 `archive/test22_anchor_bidirectional_20260712.md`。这排除了“只把 B 的全局调度换成双向 A/D orientation”的路线。

随后 Test23 只全局调度 ordinary D，停止后转回 ordered rows 并继续原离线 A。它把 Toronto fast g12 的 D payload 从 `0.837M` 降到 `0.584M`，但 global-D 耗时 `3.61s`，fast20 总计 `25.511s`。该代码同样撤出，见 `archive/test23_global_d_anchor_half_20260712.md`。因此下一机制必须在图传播前减少 rooted components；仅改变 offline/global 调度已经有两组跨库否决证据。

## 1. 当前状态

| component | role | evidence |
| --- | --- | --- |
| ReleaseV1 | 历史 small `g` half-DP 发行版 | small35 无单条反噬 |
| ReleaseV2 | 历史 fixed-anchor global 发行版 | full DBLP g13 `1427.625s / 9.015GiB` |
| ReleaseV3 | 冻结框架 B 发行对照 | fast20 `12.509s`；full DBLP g13 `531.556s / 3870.7MiB` |
| ReleaseV4 | 当前纯 A 发行版 | fast20 `8.303s`；full DBLP `544.379s / 2158.6MiB` |
| Test80 | ReleaseV4 研究来源 | full DBLP g13 `666.509s / 2161.9MiB` |
| Test19 / probes | 历史研究对照 | 不包装当前发行版，不通过开关进入 ReleaseV3 |

旧文档中的“full 未完成”只描述对应冻结 solver。当前事实以 `release_v4.md` 为首要入口，历史 full 仍按各 Release/Test 文档的明确归属读取。

## 2. 已解决问题

1. **half 与 global 的有机接续。** ReleaseV3 先生成 ordered half rows，实际工作达到 strong-search 构造工作后释放 rows，在同一 query 预处理中转 global。
2. **无经验成本判据。** greedy/global 购买事件只核算 recurrence、图松弛和 priority-queue 的理论操作，不使用固定 `g`、数据集、层级、密度或 wall time。
3. **anchor 结构选择。** 选择离 root-star 根最远的组作为 permanent goal；fast g12 五库均不增加 settled，full DBLP 峰值由历史 fixed-anchor bounded 的约 `6973MiB` 降到 `3520MiB`。
4. **DBLP 可完成性。** ReleaseV3 precursor 在完整查询得到与既有精确值误差 `2.853e-7` 的结果，时间和峰值相对 ReleaseV2 分别改善 `2.69x/2.38x`。
5. **纯 A 可完成性。** Test80 用 branch-junction incumbent 把 full D2 values 降到 `7.76M`，并以 ordered D/A rows 在 `666.509s / 2161.9MiB` 完成同一精确查询；该单一路径已发行成 ReleaseV4。

## 3. 剩余边界

1. **低 `g` 的额外 A 预处理。** ReleaseV4 small35 `2.148s`，比 Test21 快 `4.6%`、比 V3 慢 `12.8%`；用户已允许少量理论上难以达到 10x 的 `g` 放宽，后续仍须逐 `g` 报告。
2. **global frontier 空间。** ReleaseV3 full peak 已降到 `3.78GiB`，但 label map、lazy heap 与 disjoint index 仍保存相关 mask/key。只考虑无损表示，不降低 double 精度，不按 density 特判。
3. **lower-bound 形式化核算。** TSP endpoint 扫描和 dual subset sum 必须继续包含在显式 `O(3^g n + 2^g((g+log n)n+m))` 中，不能只写 `O*`。
4. **文献原创性边界。** 当前只宣称仓库适配与组合；在系统文献检索完成前，不把 farthest-goal 或整体组合直接写成论文原创定理。
5. **A 的高层时间。** Test80 的阶段统计显示 anchored A 仍用 `317.1s`；ReleaseV4 删除热路径日志后 full 已只比 V3/B 慢 `2.4%`，但不改变该状态族。下一机制仍应减少有序 A 高层工作，不能回到 B 或依赖经验切换。

## 4. 已排除方向

| direction | decisive evidence | record |
| --- | --- | --- |
| old anchored entry wrapper | full 约 `23.27GB` 且未完成 | `archive/test19_probe_archive_20260710.md` |
| replacement cap / exact-pair future | DBLP pair states 删除 `0` | `archive/group_replacement_future_bound_probe_20260710.md` |
| pair predecessor production row | full pair bytes 缩小 `3.226x`，但 fast 慢 `38.5%` | `archive/test20_pair_forest_prototype_20260710.md` |
| TSP endpoint witness row | bytes 缩小 `1.54x--1.69x`，但 fast 慢 `32.4%` | `archive/tsp_witness_row_probe_20260710.md` |
| second anchor dual / row transfer | 收益不跨数据，或增加 peak/wall | `release_v2_evidence.md`、half/global archive |
| largest/smallest anchor group | DBLP-new 或其他数据反向 | half/global archive 第 21 节 |
| packed moat / progressive upper | 正确但未跨过 MovieLens 传播底座 | half/global archive 第 18--19 节 |

这些机制不以开关留在 ReleaseV3，也不因单个数据集局部正信号恢复。

## 5. 后续门槛

1. 先给统一证明或结构探针，再改发行代码。
2. 不使用数据集名、固定 `g`、固定层、状态密度、运行时间或完成进度特判。
3. 不把 baseline 同样可做的普通图/query 压缩计作方法收益。
4. 随机小图与 DPBF 在 `1e-6` 内一致，并比较 Toronto 最后一次完整 DPBF run。
5. small 逐 `g`，fast 跨五个数据版本，同时报告时间与 solver 增量空间。
6. full DBLP 已完成；只有数量级新机制或决定性结构输出才重复长跑。
7. 使用 Release/O2，运行后清理临时目录、空结果和残留进程。

## 6. Test76 当前边界

Test76 已证明 fixed `B/C + core4` plan 的完整六标签定价可精确改写为
`A_B(D0/D1) + A_C(D0/D1/D2) + D(R)`，并通过 singleton `500/500`、双候选组
`200/200`。实现使用排序 `(block, subset)` keys 和二分查找，不使用 Hash。

该结论没有接入正式 Test21：Toronto-fast g12 即使对 `1,269` 个算法自身筛出的 plans
全部精确定价，family best 仍为 `0.9688685500`，高于 exact `0.9616227800`；优化掉
逐点二分查找后，row 构造与定价仍需 `2.955s + 0.263s`。因此缺口是“两块 + core4”
family 不完备，不是定价公式或高阶 anchor row。详见
`archive/test76_first_order_block_anchor_20260713.md`。

Test78 进一步证明了 3--5 macro labels 的 cherry 定价和固定三块的三臂 block-anchor
定价；fixed g13 singleton/双候选组分别为 `1000/1000`、`500/500`。但把三个顺序提取
的 pendant blocks 同时视作 leaves 时，Toronto-fast 全量 dense 定价仍为
`0.9688685500 > 0.9616227800`；其 exact witness 去重边成本为 `0.9616227800`，同一 token
分组的普通/三臂价格却为 `1.0658278500/1.1662525000`。纯组合上，12-token 三度
caterpillar 的 3/4-token 单接口块只能是 spine 前缀或后缀，最多同时选两个；因此
simultaneous three-arm family 一般不完备。若继续顺序提取，下一状态必须保存第二接口
或等价 paid-backbone geometry。
正式 Test21 不改，full DBLP 未运行，详见
`archive/test78_pendant_cherry_two_interface_20260713.md`。

## 7. 论文关系

- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：Dijkstra-Steiner label-setting、future cost 和 consistency/splice 理论来源；ReleaseV3 适配 GST group subsets。
- [DS*](https://arxiv.org/abs/2011.04593)：一般 admissible lower bounds 的 exact-search 背景；ReleaseV3 使用满足 consistency/splice 的路径，不实现 DS* reopen。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 路线来源；group sink potential 与 subset splice 是本仓库适配。
- [Improved Algorithms for the Steiner Problem in Networks](https://www.sciencedirect.com/science/article/pii/S0166218X0000319X)：dual ascent/reduced costs 在 exact Steiner solver 中的历史位置；ReleaseV3 不复制其完整 branch-and-cut 框架。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：当前 GST baseline；ReleaseV3 不把 baseline 可共享的普通压缩算作自身贡献。
- [Dynamic Programming for Minimum Steiner Trees](https://doi.org/10.1007/s00224-007-1324-4)：额外 separator terminals 与小组件拼接的已知 exact 路线；用于界定 Test21 开放主干研究的非原创部分。
