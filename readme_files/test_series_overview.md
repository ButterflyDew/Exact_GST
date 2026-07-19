# GST Framework 文档入口

本文只负责导航和事实优先级，不重复算法证明或实验表。

## 1. 从这里开始

| 需要了解 | 唯一入口 |
| --- | --- |
| 构建、CLI、输出格式 | `../RUN.md` |
| PrunedDP++ baseline 复现口径与三个开关 | `pruneddp_reproduction.md` |
| 当前 ReleaseV6 方法章节 | `release_v6_method_cn.md` |
| ReleaseV6 发行实现与验证 | `release_v6.md` |
| ReleaseV6 方法评审门 | `release_v6_review_gate.md` |
| ReleaseV5 历史方法与实现 | `release_v5_method_cn.md` / `release_v5.md` |
| ReleaseV5 的 `g=4..15` 曲线与论文缺口 | `release_v5_target_and_publication_gap.md` |
| GPU4GST 八数据集接入与查询协议 | `gpu4gst_datasets.md` |
| ReleaseV4 首个纯 A 冻结版 | `release_v4.md` |
| ReleaseV3 框架 B 冻结对照 | `release_v3.md` |
| Test80 研究选择与阶段数据 | `test80_anchor_progressive.md` |
| Test80 DBLP g13 跨询问剖析 | `test80_dblp_g13_cross_query.md` |
| Test83 按需 group-tour 下界 | `test83_lazy_tour_bound.md` |
| Test84 锚定顶层流式调度 | `test84_anchored_top_stream.md` |
| Test87 directed-cut 优先求值 | `test87_cut_first_bound.md` |
| Test90 顶层 D 中的流式 A0 completion | `test90_streamed_a0_completion.md` |
| Test98 带 rank 的 bitmap 有序行 | `test98_ranked_bitmap_rows.md` |
| Test103 独立 bitmap branch 半连接 | `test103_out_of_line_bitmap_semijoin.md` |
| Test104 completion 共享位图相交 | `test104_completion_bitmap_intersection.md` |
| Test105–107 completion 分层最小值下界 | `test105_completion_partition_minimum.md` |
| Test115 DBLP g15 q1 分层进度门 | `test115_dblp_g15_progress.md` |
| Test116 DBLP g15 跨询问压力筛查与 q33 完整结果 | `test116_g15_variance_panel.md` |
| Test142 伴随锚点格消维 | `test142_adjoint_anchor_lattice.md` |
| Test145 有向割约化的转置终端 | `test145_directed_cut_transposed_terminal.md` |
| Test146 按实际工作量摊销锚树上界 | `test146_amortized_anchor_tree.md` |
| Test149 只扫描已改写弧的 dual 闭包 | `test149_changed_arc_dual.md` |
| Test152 按普通工作量渐进购买 dual | `test152_progressive_dual.md` |
| 五数据集 g15 全量剖析 | `g15_five_dataset_profile.md` |
| Test95 3/4 pendant 树分解与接口障碍 | `test95_soft_pendant_decomposition.md` |
| 当前 anchor-aware half 研究 | `test21_anchor_half.md` |
| Test21 高 D generator 否决边界 | `test21_high_row_generators.md` |
| A 状态 paid-attachment 规模边界 | `history/paid_attachment_representative_family.md` |
| 显式 D2 输出门槛 | `history/explicit_d2_output_barrier.md` |
| 当前未解决问题与实验门槛 | `research_status.md` |
| snapshot benchmark | `snapshot_benchmark.md` |

## 2. 发行版

| version | 定位 | 状态 |
| --- | --- | --- |
| ReleaseV6 | anchored D/A/F/H + transposed terminal + progressive dual | 当前发行入口；由 Test152/M5 独立冻结，不调用 Test 或其他 Release |
| ReleaseV5 | permanent-anchor ordered D + low A + adjoint H + eager dual | 历史纯 A 对照；由 Test149 独立冻结 |
| ReleaseV4 | permanent-anchor ordered D/A rows | 首个纯 A 冻结版；由 Test80 清理得到，不调用 B |
| ReleaseV3 | ordered half rows -> farthest-goal global | 冻结的框架 B 发行对照；full DBLP g13 已完成 |
| ReleaseV2 | fixed-anchor global labels | 历史大 `g` 发行版；保留独立源码与 full 证据 |
| ReleaseV1 | half-DP | 历史 small `g` 发行版；保留独立源码与结果 |

ReleaseV1--V6 都有独立源码，不通过 Test 开关启用。ReleaseV6 只复用 Common 中的图、junction 与 dual helper，不调用 Test 或其他 Release；Test152/Test157 继续保留发行来源统计与同源消融。ReleaseV5 保持历史冻结，不回灌渐进 dual。

## 3. 文档分层

```text
readme_files/
  release_v6_method_cn.md       当前发行算法的方法章节
  release_v6.md                 当前发行实现与验证
  release_v6_review_gate.md     方法、实现与投稿实验门
  test152_integrated_method_review.md 完整算法贡献评审
  test152_novelty_audit.md      逐组件与逐定理排重
  release_v5_method_cn.md       历史 eager-dual 方法章节
  release_v5.md                 历史发行实现、验证与结果
  release_v5_target_and_publication_gap.md 扩展曲线与投稿缺口
  gpu4gst_datasets.md           GPU4GST 数据、查询与接入测试
  release_v4.md                 首个纯 A 冻结版
  release_v3.md                 冻结的框架 B 发行对照
  release_v3_implementation.md  ReleaseV3 源码导读
  test80_anchor_progressive.md  ReleaseV4 来源与阶段统计
  test80_dblp_g13_cross_query.md Test80 跨询问瓶颈与后续依据
  test83_lazy_tour_bound.md     Test80 当前按需求值优化
  test84_anchored_top_stream.md Test80 当前 A 顶层流式调度
  test87_cut_first_bound.md     Test80 当前三级下界短路
  test90_streamed_a0_completion.md Test80 当前顶层 completion 调度
  test98_ranked_bitmap_rows.md  Test80 当前三布局有序行
  test103_out_of_line_bitmap_semijoin.md  当前布局无关 branch join
  test104_completion_bitmap_intersection.md 当前 A+D+D 位图共同根接口
  test105_completion_partition_minimum.md 当前 A+D+D 三级下界级联
  test142_adjoint_anchor_lattice.md 当前高层 A 伴随反向框架
  test145_directed_cut_transposed_terminal.md 当前跨目标终端共享候选
  test149_changed_arc_dual.md 当前 dual 残量闭包初始化优化
  test152_progressive_dual.md ReleaseV6 渐进 dual 的研究来源
  test95_soft_pendant_decomposition.md 已证明的树分解与标量状态反例
  research_status.md            当前开放问题
  release_v1.md / release_v2.md 历史发行版
  history/                      已冻结但仍有效的研究与证明
  archive/                      失败尝试、撤回路径、原始长日志
```

事实优先级：ReleaseV6 文档 > 当前 Test152/Test157 文档 > 较早 Release 文档 > `history/` > `archive/`。
旧文档中的“当前”“未完成”只描述对应冻结 solver，不得覆盖 ReleaseV6 的当前事实。

## 4. 当前结论

- ReleaseV6 已从冻结 M5 独立清理：单一 `D/A/F/H + transpose + progressive dual` 路线，不调用 Test/旧 Release，不含研究开关、quarter upper、调试输出或数据特判。Release/O2 构建通过；随机小图对 DPBF 为 `500/500`，Twitch `g=10` q20 与 Musae `g=13` q5 对 M5 共 `25/25` 权重一致。方法见 `release_v6_method_cn.md`，实现与验证见 `release_v6.md`。
- 完整方法贡献评审已通过：论文 claim 落在永久可选组锚定、低层 inside、高层 closure-aware outside 和补集终端转置形成的 exact GST 联合求值组织；MITM、通用 outside、packing 和 dual 等局部构件均明确承认已有来源。传统五库冻结面板和同机 GPU4GST 是投稿实验待补项，不再被混写成方法失败。
- GPU4GST 的 8 个公开数据集已经转换为仓库图/查询接口，并按相关组共现 BFS 协议生成 `g=4..16`、每点 300 条冻结查询。ReleaseV5 与 PrunedDP++ 的三库扩展面板显示：`g=4,5` 同一数量级但 V5 分别慢约 `2.06x/1.61x`；`g=8` 约快 `4.77x`，不是 10 倍；聚合时间和几何平均从 `g=9` 起跨过 10 倍，`g=10..12` 的时间与绝对 peak RSS 均稳定超过 10 倍。`g=13..15` 中 V5 全部完成、Strict 只完成 `5/9、4/6、2/6`，completed-pair 比值存在删失偏差。完整表、单例反例和 VLDB/SIGMOD 必需工作见 `release_v5_target_and_publication_gap.md`。
- 历史 ReleaseV5 是 Test149 正确主线的独立纯 A 冻结版：不调用 V3/V4，没有运行时开关、数据集特判或调试输出。Release/O2 随机对拍为 `300/300`；五个原始数据集固定 g10 q1--q5 共 25 条询问与 Test149 权重逐条一致，总时间 `933.453s -> 900.820s`，其中 `21/25` 条不慢于来源版本。该面板用于验证当时冻结没有回退，不把 q1 外推为整个数据集结论。
- Test152 把 eager sequential dual 拆成按固定顺序推进的合法组前缀，每累计 `2m+n` 个 ordinary seed/pop/relax 工作单位购买一组；规则不读数据集、固定 `g`、层号、密度或 wall time。GPU4GST 三库 `g=4..10` 共 `420/420` 条权重一致，`g=5..10` 全部快于 Strict，`g=9..10` 仍有 `11.97x--22.00x` 优势，`g=4` 尚慢 `12%--26%`。该路线现已冻结进入 ReleaseV6；Test152/Test157 保留研究统计，ReleaseV5 保持冻结。
- ReleaseV4 是较早的纯 A 冻结版：small35 `2.148s`，fast20 `8.303s`，Toronto full
  `7.857s / 58.0MiB / 0.7048467020`，DBLP full
  `544.379s / 2158.6MiB / 12.5936282853`；每条 snapshot 权重均与 Test80 一致。
- Test80 的 full DBLP g13 q1 为 `666.509s / 2161.9MiB / 12.5936282853`；它是
  ReleaseV4 的算法来源和阶段统计对照，发行清理使 full wall 降低 `18.3%`。
- 当时的 Test80 研究版在该状态框架上保留 Test83/84/87/90/98/103/104/107。Test107 把 Test105 的分块证书增强为“分块→根→分量”三级 completion 下界：DBLP q32 最终 checks 从 `204.52M` 降到 `3.33M`，completion `24.60s -> 7.05s`；q5 与 Toronto checks 也分别下降 `98.40%/99.12%`。该组合版 q25 为 `8121.863s/18498.1MiB/12.0889822532`，相对 Test98 completion `1211.875s -> 488.765s`、checks `32.815B -> 46.019M`，全部 D/A 状态逐项相同。该阶段尚未更新 ReleaseV4 与 DBLP q1。
- Test80 又按截止时间完成 19 条新 DBLP g13 询问；wall/peak 中位数为
  `2042.8s/5874.1MiB`，D3/D4 主导空间，A5 与完整化主导后段时间。q1 相对容易，
  后续研究依据见 `test80_dblp_g13_cross_query.md`。
- ReleaseV3 的 full DBLP g13 为 `531.556s / 3870.7MiB`，继续作为框架 B 时间对照。
- ReleaseV4 不使用数据集名、固定 `g`、row density 或运行时刻特判，也不使用
  baseline 可共享的普通图/query 压缩；D2 packing 购买由可证明的工作量比较决定。
- 当前 Test21 研究版为 anchor 主干 + branch basis + 在线分块上界；fast20 `12.380s`、Toronto full `20.788s / 102.9MiB`，但 DBLP g13 仍未跨过 V3 时间门槛。
- anchor-group super-root dual 的 objective 合法但 future 过松；Test35 fast20 `27.792s`，已撤回。一般显式 paid-attachment family 存在 `2^r` 反链，下一机制必须提供隐式共享或新的受控边界。
- D2 多 pair 向量波虽减少 accepted profiles，却因异构 priority 在 fast 慢 `3.8x--45x`；固定 singleton/pair branch recurrence 又有 rooted 反例。两条入口均已清理，证据见 archive Test36--37。
- local seed-cone dominance 在 fast/Toronto full 有 `1.6%/3.4%` 正收益，但不减少 DBLP D2 payload，bounded gate 仍未完成；作为辅助定理归档，不进入当前 Test21。
- one-third backbone states 精确且不增加 mask 数，但只减少 full Toronto 高层约 `4%`、不动 D2；Test39 已撤回，不触发 DBLP。
- A1 consumer-driven D2 在 full DBLP 三对上仍 settle 完整 row 的 `99.7%--99.9%`，Test40 已撤回；exact-best 样本表明下一杠杆是 D2 前的 anchor-aware incumbent。
- dual zero-residual half/三块 upper 很轻且 fast 两库有更新，但 full DBLP 仍停在 `17.360814`；Test41 已撤回。
- work-triggered ordinary greedy 把 Test42 fast20 降到 `12.182s`，但 full 在 `558.7 CPU-s` 仍未完成；不满足大实例门槛，已撤回。
- singleton-only anchor C rows 在 fast 两库改善 upper，但三块无更新且 full C1 在 `234.8 CPU-s` 仍未完成；Test43 已撤回。
- paid anchor backbone 把 fast20 最低降到 `10.112s`、full upper 降到 `13.1619`、peak 约减半，但 full 仍在 `543--555 CPU-s` 无最终结果；Test44--47 因 V3 硬门槛撤回。
- branch-junction closure 只扫描 triple argmin roots，再在其到 anchor backbone 的压缩父树上做 subset DP，使公共路径前缀只付费一次。Test48 fast20 为 `9.758s`、D2 values/pops 降到 `0.50x/0.45x`、full upper 为 `13.01999`，但有效 full gate 在 `535.507s query / 2087MiB peak` 仍无最终权重；已撤回。
- pair gradient 定理把 full exact-best 下单个 anchor consumer 的 D2 sites 压到 `8.416%`，并可用 `449MB` arc-to-pair bit DAG 精确枚举；但逐 consumer/target 展开后 fast20 为 `16.834/13.245/14.941s`，均退化。Test49 已撤回，下一步必须同时共享 pair bits 与 consumer masks。
- factorized arc records 避免 target events，但 Test50 fast20 仍为 `14.080s`：`5.42M` factors 触发 `125.0M` membership probes、仅 `27.1M` 命中。event 展开与 target 探测两种接口均已否决。
- Test51 同时保留 triple 在接入路径“首个使用者付费”和“共享前缀已付费”两种边际状态下的精确 junction。fast Toronto/DBLP-new upper 有改善，但 full DBLP 把候选从 24 扩到 84、压缩树从 20 扩到 86 后仍严格等于 Test48 的 `13.019988893`；同一 parent tree 上继续扩 facility roots 已否决。
- Test52--53 让 Test48 primal skeleton 反向决定 dual：attachment-centroid 与 root-star 两个 potentials 在 full 各自约 42%/47% positions 更强，max 使 D2 settled/pushes 降 `9.6%/11.1%`；但 replay wall `40.31s -> 43.62s`，还需第二次 dual `39.12s`。按 attachment 深度重排一次 dual 的 fast20 又退到 `11.565s`，全线撤回。
- Test54 在一次 residual construction 中把每个 group moat 增长到覆盖整条 paid anchor path。Toronto-new D2 降到 `0.51x/0.46x`，但 DBLP/DBLP-new values 膨胀到 `18.38x/4.40x`，fast20 `15.347s`；sequential cover-all charge 的跨库方向翻转已否决。
- Test55 改用 root-preserving path-average cap，fast20 回升到 `13.198s`，但 DBLP/DBLP-new D2 values 仍为 `16.31x/2.48x`。Test54--55 共同否决 sequential ascent 中所有“每组一个 paid-path scalar cap”的继续微调。
- Test56--58 的 order-independent residual packing、D2-work 购买与 Test48 junction 后来按纯 A 边界重建为 Test80。历史 gate 在 `531.671s` 无 weight；当前不设 B 门禁的 full 于 `666.509s / 2161.9MiB` 完成，二者口径不能混写。
- Test59 证明每个可拆 `D(S,v)` 都有大小至多 `|S|/2` 的根不可拆分分支，并据此把 split 定向到较小侧。随机 `500/500` 精确，但 fast20 `13.016s`，ordinary/anchored/completion payload 与正式版均在 `0.02%` 内；只改 split orientation 不改变状态族，已撤回。
- Test60 用 `min C(f)+g=min f+C(g)` 把最高 `D_h` closure 转到已闭包 anchor side。`k=2h-1` 的 fast g12 改善 `3.8%`，但 DBLP g13 同类的 `k=2h` 必须补 A-half generators；Toronto full 虽让 D6 values/pops 减约 `45%`，wall 仍从 `20.788s` 退到 `25.121s`，已撤回且不触发 DBLP。
- Test61 再用 `min C(Q_N)+D_C=min Q_N+D_C` 消掉非 canonical A-half rows，以 closed-D roots 驱动三函数 scalar completion。Toronto 相对 Test60 回收 `1.28s`，但 canonical A-half 仍增加约 `16.9M` merges，最终 `23.838s/105.1MiB`，继续慢于正式版；已撤回。
- Test62 用 target A* 直接计算最后的 `min C(Q_C)+M_N`，彻底删除 A-half rows。Toronto 仅需 `22.5k` target pops，但 top generator 与 `1.05M` point probes 仍使 wall 为 `23.786s`；Test60--62 三种 top closure 接口全部否决，研究重心返回低层状态族。
- Test63 检查 `gd_i+gd_j` 是否已为 1-Lipschitz，从而整张删除无需 closure 的 D2。五库 fast g12 共 `275/275` pairs 全部失败，每对至少有 `2,528` 条严格 violation edges；该 exact certificate 零命中，probe 已撤回。
- Test64 的 pair-work anchor 在 full 结构上准确选中最少 D2 的组，但 fast 的 DBLP/DBLP-new/MovieLens 分别退化约 `26.0%/9.1%/13.2%`；单独换 anchor 只是重分配状态族，已撤回。
- Test65 的 recursive `D0+B` 状态排序由 Pascal 恒等式保持精确，随机 `500/500`，但 fast20 为 `13.755s`；双-anchor四块 upper 修正后仍为 `13.617s`。没有共享几何时，递归 anchor 只推迟有效上界。
- Test66 的双-anchor paid backbone 在 `18/20` 条改善初值，并减少 DBLP-new/Toronto-new ordinary payload；单独/与 nested 组合的 fast20 仍为 `12.867/12.889s`，group-TSP 全局证书 `0/20` 命中。增加 anchor 数不能替代 junction 前缀共享。
- Test67 将 attachment candidate tree 无参数迭代到 fixed point；fast g12 在 `2--4` 轮、`4--62` 个压缩点稳定，但 DBLP upper 不动，跨库仍有至多 `6.60%` exact gap。继续扩同一 conditional skeleton 已否决，未触发 full。
- pair ancestor/tight-cone dominance 将 Test49 frontier 精确缩为 consumer prefix minima；三库可再降约 `28%--31%`，但 DBLP/MovieLens 仅降 `1.85%/0.39%`。该定理进入 history，单独接口不触发 full。
- Test69 用 `(split root,attachment)` rank-1 profiles 将 pair join 化为两次 singleton transform。fast DBLP 显示 `13.3x` 共享，但 full 仍有 `53.17M/106.31M` profiles；profile-mask 乘积过大，已撤回。
- Test70 组合 one-third endpoint、junction upper 与 delayed progressive packing，fast20 创 A 线新低 `9.161s`、Toronto full `9.64s/58.1MiB`；DBLP 在 `531.831s` 仍无 weight，未越过 V3，全部撤回。
- Test71 的 quarter endpoint 禁止高层 branch publication 后出现 `DPBF=20 / candidate=21` 的完备性反例；Test72 恢复 exact D 后随机累计 `3000/3000`，但 fast20 `12.843s` 慢于正式 `12.380s`。只改 split orientation 不会删除状态族，两版均撤回且不触发 full。
- Test73 保留 offline D、只把 A 改成无 Hash global consumer；随机 `100/100` 精确，但 Toronto-fast g12 settled 仅降 `6.7%`、wall `1.791s -> 6.288s`。调度象限已补齐并撤回，不与 Test70 组合。
- Test74 将 V3 切换前的 exact half rows 注入 global；随机 `300/300`，但 fast20 每条 settled frontier 与 V3 完全相同，created 增加、wall `12.509s -> 13.176s`。half value 只是可重建路径缩写，已撤回。
- Test75 理论基底首次把 half state 改成 root-free paid subtree profile；`paid-half+D+D` completion 已证明 exact。小图累计 `1250/1250`，fixed g8 Pareto ratio `0.572%`、max front `117`。方向保留，下一门槛是 pair-path 大图生成，不直接写完整 solver。
- Test75 pair projection 又在 full DBLP 将 `106.31M` D2 roots 压成 `821` 个单-anchor skyline states（每对最多 `23`）；五库 fast 比例均低于 `1.7%`。全 singleton root-vector 在 Toronto 回升到约 `50%--62%`，所以下一步是内部多点 profile/列生成，而非高维 root 向量。
- Test75 paid pair-block completion 将 anchor 留在两个 half blocks 之一，五库 fast g12 全部 exact；先付 anchor 在三库留有 gap。早期 `g<=7` unrestricted 穷举为 `3000/3000`，但 fixed g13 已有 pair-only `160 -> 161` 反例；anchor skyline 另有 `59 -> 60`。因此 `821` 个 full skyline states 只作初始列，不能宣称一般完备。
- Test75 profiles 直接旁挂旧 A rows 时，Toronto/Toronto-new 的 A values/merges/checks 完全不变，g12 wall `1.791/3.609s -> 1.909/4.039s`；该集成已撤回。tree-target 必须替代 A row 输出，不能只做额外 upper 扫描。
- paid incidences 作为 global-A 唯一种子时，fast DBLP exact，但 `2,047` seeds 膨胀为 `1.483M/1.193M` created/settled labels、耗时 `18.039s`。逐 root 传播会重建已删除维度，模式已撤回。
- Test75 low-core 只保留 `D<=q=ceil(h/2)`，以真实 edge-union 从 pair 生成 `g-2q` core；unrestricted 穷举 `3000/3000`。DBLP-fast D1--D3 即 exact、core `15,779`，但 Toronto 两版仍有 gap 且 front 反弹，故当前只保留 partition-pricing 基底，不跑 full。
- Test75 strong lower 将 fast g12 exact 证书压到 `4--2253` 次 plan pricing；Toronto g13 的 skyline `3+5+5` plan 全 root 定价命中 exact，但临时 Test77 因仍需 D6/A5 证停而从 `20.788s` 退到 `29.113s`。代码已撤回；下一步只研究 pair-batched pricing 与完整停止证书，不优化该 upper 的局部常数。
- fixed g13 进一步给出 q5/core3 `106 -> 107` 反例；放开全部 attachment points 仍失败。q5/core4 的 g13 证据为 `720/720`，扩展 odd-g/Steiner/multi-group/high-cycle 后 targeted 共 `13,520/13,520`。双 pair-forest requested-profile 因子化把 Toronto `5,476` plans 从 `39.158s` 降到 `18.721s`；rooted-D4 又降到 `8.023s`，但 deterministic/all-tight-D4/六标签 DP 在 fast MovieLens 均留下约 `1.38e-6` gap，证明 minimum D4 geometry 不完备。core4 exchange 与 stopping certificate 仍缺，正式 Test21 不改、不跑 full DBLP。
- Test76 将完整六标签 fixed-plan DP 精确压成一侧 D0/D1、另一侧 D0/D1/D2 的 block-anchor 三函数交；singleton `500/500`、双候选组 `200/200`。但 Toronto-fast g12 对算法自身筛出的 `1,269` plans 全部定价后仍为 `0.9688685500 > 0.9616227800`，并需 `2.955s` 构造共享 rows。定理保留，core4 两块 family 否决；正式 Test21 不改，full DBLP 未运行。
- Test78 证明 3--5 macro labels 的 cherry 定价与固定三块的三臂 block-anchor 定价；fixed g13 singleton/双候选组分别 `1000/1000`、`500/500`。三度 12-token caterpillar 证明 simultaneous three-arm family 一般不完备；Toronto-fast 的 one-root-core family 也停在 `0.9688685500 > 0.9616227800`。保留 fixed-plan 定理，顺序提取转向第二接口/paid backbone。正式 Test21 不改，full DBLP 未运行。
- Test78 后续的 soft-terminal block transform 用每个 block 至多 `2^4` 个临时 rows 隐式表达接口；固定块数连续出现 `72->78`、`54->55` 反例，放开 block 数并用 subset DAG 合并顺序后成为 Test95 的前身。Test95 已将 core 收紧到 3 并补上一般分解证明，但大图共享求值仍未解决。
- Test79 曾将 junction 接入 ReleaseV3/B，full 为 `521.055s / 3744.6MiB`。它不满足优化框架 A 的目标，活跃源码和 prepared 接口已撤出；有效数据冻结在 `archive/test79_v3_junction_control_20260713.md`。
- Test81 在完整 D2 后把精确 pair rooted costs 接入原 branch-junction tree 的局部 facility。随机 `300/300`，fast20 有 `7/20` 条改善，但 D/A 状态只降 `3.48%/0.26%`；DBLP g13 q21/q25 上界只改善约 `1.3%`，远未复现 D4 的结构收益。固定 skeleton 上追加局部标量被否决，代码与临时工具已撤出，未跑完整 DBLP。
- Test82 把 directed-cut 约化代价压成每行两个精确标量证书，在有序连接前证明整对 D+D 或 A+D 行不可能改善 incumbent 时跳过。随机 `100/100` 正确，fast20 跳过 `68,937/35,434` 次 D/A 行合并，但 D 状态完全不变，总时间 `9.099s -> 9.734s`；真实 Toronto g13 q1 又由 `8.641s` 退化到 `10.310s`。机制未降低主导状态空间，已完整撤回且未触发 DBLP 长跑。
- Test83 保留三个精确下界，但先用 `max(farthest,directed-cut)` 拒绝候选，只对幸存者计算 group-tour。DBLP g13 q5 的 D/A tour 求值率仅 `1.72%/1.10%`，wall `276.117s -> 249.146s`，答案与四项 D/A values、pops 均不变。机制已合入 Test80；ReleaseV4 未改，完整说明见 `test83_lazy_tour_bound.md`。
- Test84 把最后一个需保存的 A 层改为依赖驱动流式调度：A4 保持原整数 `mask` 顺序，A5 依赖齐备即消费，A4 最后一次使用后释放。DBLP g13 q5 的 A4 payload 峰值下降 `26.8%`，wall 为 `242.886s`；最坏渐进阶不变。字典序调度导致 A pops 增加 `73.5%` 的反例已记录并撤回，见 `test84_anchored_top_stream.md`。
- Test85 推导普通 D 行在最终 A 层的完整消费者计数，并尝试最后一次使用即释放。fast20 联合 row payload 下降 `10.01%`，但 solver time 回退 `7.45%`；Toronto 的 payload 只下降 `2.675%`。当前调度下 D3/D4 最后消费者过晚，机制已撤回，见 `archive/test85_ordinary_last_use_reclamation_20260715.md`。
- Test86 将 directed-cut 下界分解为可排序的约化代价前缀。前缀只保留 D/A branch 的 `26.49%/40.89%`，但顶点交集的第二排序维使逐点二分工作膨胀；理想 Cartesian report 也只有 D `0.931x`、A `1.101x`。二维索引路线已撤回，见 `archive/test86_dual_reduced_prefix_join_20260715.md`。
- Test87 将下界求值改为 `directed-cut -> farthest -> group-tour` 三级短路。DBLP g13 q5 的 D/A farthest 求值率仅 `1.724%/1.054%`，wall `242.886s -> 186.896s`，答案与状态计数严格不变；fast20/Toronto 基本持平。机制已合入 Test80，见 `test87_cut_first_bound.md`。
- Test88 尝试以全组 dual 总和的补集表达式预筛选 exact cut。最终版状态不变，但 fast20 退化约 `2.8%`，q5 仅改善约 `0.43%` 且 peak 增加约 `19.3MiB`，已完整撤回，见 `archive/test88_complement_dual_prefilter_20260715.md`。
- Test89 在原 bit 顺序内逐项累加 directed-cut，并在非负前缀已拒绝时停止；走到末尾的 double 与旧 `dual.At` 完全相同。fast 虽少读约 `32%--34%` terms 仍退化，DBLP q5 更只少读约 `9%`，wall `186.896s -> 202.975s`，已撤回，见 `archive/test89_dual_prefix_short_circuit_20260715.md`。
- Test90 利用每个 A0 平衡 completion 必含一张最高层 D row，在该 row 完成时立即做补集有序相交。DBLP g13 q5 的 D6 values/pops 下降 `35.37%/37.99%`，wall `186.896s -> 183.914s`；A 状态不变。机制已合入 Test80，见 `test90_streamed_a0_completion.md`。
- Test91 将 odd-g 顶层 masks 改成补集成对顺序，Toronto D6 进一步减少，但 fast20 两次均退化约 `2%`；跨库反向后撤回，只保留 Test90 的原整数 mask 顺序，见 `archive/test91_paired_top_mask_order_20260715.md`。
- Test92 把 A0 上界反馈推进到每个 accepted seed/relax label。方法正确且无 Hash，但 fast20 的 `710,011` 次补集查找只少 `101` pops、时间增加 `1.92%`，已撤回，见 `archive/test92_online_a0_label_feedback_20260715.md`。
- Test93 改为在 seed 阶段结束后做一次批量有序相交并重新过滤建堆 seed；`674,564` 扫描只少 `15` pops、时间增加 `2.20%`，已撤回。两次否决共同终止 A0 触发时机微调，见 `archive/test93_seed_boundary_a0_completion_20260715.md`。
- Test94 在 D/A 相位边界用最终普通上界重新执行 directed-cut 证书。fast20 删除 `4.38%` 普通值、row payload 降 `4.35%`，但 A values/pops 仅各少 `5` 且时间增加 `2.04%`；全量重扫不能改变后续状态数量级，已撤回，见 `archive/test94_phase_boundary_dual_compaction_20260715.md`。
- Test95 证明标号树可分解为 `core<=3` 与 3/4 单接口 pendant blocks，但 Test96 随即否决“因此标量 soft/permanent-anchor D4 完备”的推论：g13 双候选组上 exact `71`，固定 group 0 soft 与 bounded-D4 A 均为 `73`，unrestricted A 为 `71`。缺失信息是 paid tree 内部 attachment profile，不是 block 顺序，见 `test95_soft_pendant_decomposition.md`。
- Test97 在严格边界 `q=ceil((g-1)/3)` 后提前生成完整 `A_1..A_q`，再把可行上界反馈给高层 D。Toronto g13 的 D5/D6 values 降低，但 A values 从 `493,596` 增到 `752,813`、peak 从约 `57.6MiB` 升到 `59.1MiB`，总时仅改善约 `0.4%--0.8%`；Toronto-new fast 又回退。完整 A 前缀只是在 D/A 间搬移状态工作，已撤回且未运行 DBLP q5/q1，见 `archive/test97_progressive_a_prefix_20260715.md`。
- Test98 为 D/A 有序行加入带 per-word rank 的 bitmap 布局，并逐行按 sparse/dense/bitmap 的精确逻辑字节数取最小，不使用 density threshold。随机宽范围 `200/200`、固定 g13 `50/50` 正确；fast20 solver time `8.261585s -> 7.333835s`，DBLP g13 q5 wall `183.914s -> 158.150s`，状态计数不变。q25 压力长门中，Test98 相对历史 Test80 peak `25541.4MiB -> 18493.9MiB`，wall `7619.9s -> 8652.0s`；空间突破成立，但重查询时间有明确代价。机制已合入 Test80，该阶段尚未更新 ReleaseV4 与 DBLP q1，见 `test98_ranked_bitmap_rows.md`。
- Test99 把同一 A row 的 completion roots 一次性写入复用 epoch，根成员判定由最坏 `O(Pk)` 降为 `O(k)` 构造加常数读取。fast/Toronto completion 明显下降，但 DBLP g13 q5 两次 wall 为 `161.587s/162.903s`，均慢于 Test98 的 `158.150s`；eager 标记已撤回，见 `archive/test99_completion_root_epoch_20260715.md`。
- Test100 用 `bitmap occupancy AND branch bits` 融合 branch 半连接。fast20 ordinary 局部时间下降，但两次 solver 为 `7.311s/7.355s`，中位数与 Test98 的 `7.334s` 基本相同，且收益只出现在 bitmap rows 很多的 Toronto 两版；作为局部常数优化已撤回，见 `archive/test100_bitmap_branch_semijoin_20260715.md`。
- Test101 在 branch bitset 与 nonbranch exception list 间逐行按精确字节取小者。随机宽范围 `200/200`、固定 g13 `50/50` 正确，旧 q25 聚合统计可再推出约 `472MiB` row payload 上界改善；但 fast20 只少 `0.222MiB`，solver 却退化 `10.7%`，已撤回且未触发长门，见 `archive/test101_branch_exception_layout_20260715.md`。
- Test102 让 bitmap accumulator 与顶点域 branch bits 做布局无关的 word 半连接。fast20 ordinary/anchored direct work 降 `30.7%/32.1%`、solver 两次均改善，Toronto 也小幅正向；但 DBLP q5 两次为 `162.064s/162.291s`，均慢于 Test98 的 `158.150s`，故实现撤回且不增加按数据集或 bitmap 数量开关，见 `archive/test102_layout_independent_bitmap_semijoin_20260715.md`。
- Test103 将同一 branch 半连接缩为独立 `noinline` 内核，保住未命中热路径。fast20 direct work 仍降 `30.7%/32.1%`、solver 两次为 `6.962s/6.906s`；DBLP q5 两次恢复到 `152.286s/151.027s`。该单变量阶段未重跑 q25；机制已合入 Test80，后续组合长门见 Test105--107，见 `test103_out_of_line_bitmap_semijoin.md`。
- Test104 为 bitmap-compatible 的 `A+D+D` completion 按需构造 A-root bitmap，并用三路 word 相交枚举共同根。fast20 completion `870.8ms -> 472.8ms`、checks 不变；当前重编译的 q5/q32 均零调用且无回退。该单变量阶段未重跑 q25，但 D3/D4 布局严格给出至少 `41,265` 个兼容 partitions；后续组合版 q25 实际命中 `83,048` 个。机制已合入 Test80，见 `test104_completion_bitmap_intersection.md`。
- Test105–107 为每张现有 D/A row 保存精确最小值，并依次在分块、共同根和第二个普通分量读取前做单调增强的安全拒绝。DBLP q32 completion checks 降 `98.37%`、局部时间降 `71.32%`，q5/Toronto checks 降 `98.40%/99.12%`。当前组合版 q25 相对 Test98 checks 降 `99.86%`、completion 降 `59.67%`、wall 降 `6.13%`、peak 不变；该长门还包含 Test103/104，单变量证据仍以 q5/q32/Toronto 为准。当前保留 Test107，完整统一说明见 `test105_completion_partition_minimum.md`。
- Test108–110 依次检查普通 D 分量最小值界在成员相交前、相交后和 `old-value→cut` 顺序下的效果。fast20 拒绝率仅 `6.9%--9.5%`，ordinary 状态始终不变；相交后版本至多一次运行约 20ms 噪声，增强后又回退。三版均撤回，见 `archive/test108_ordinary_component_minimum_20260715.md`。
- Test111 为每张普通 D 行保存接到 permanent anchor 的最小代价，在共同根枚举前整块拒绝 split。Release/O2 随机 `100/100 + 50/50` 正确，但 fast20 虽拒绝 `21.85%` 的 split，实际 D join work 只降 `0.99%`，ordinary `2139.984ms -> 2230.339ms`，状态完全不变。单个 attachment 标量丢失根相关性，机制已撤回且未运行真实 DBLP，见 `archive/test111_anchor_attachment_row_certificate_20260715.md`。
- Test112--113 分别去掉 singleton 未读侧的 component 证书，并按直接访问成本交换两个对称 D 分量。两版均为随机 `100/100 + 50/50`；前者只是比较与数组读取互换，后者虽再少 `66,686` 次 checks，却使 fast20 completion `219.400ms -> 234.796ms`。逐 partition 贪心读序破坏有序行局部性，均已撤回，见 `archive/test112_113_completion_read_order_20260715.md`。
- Test114 由当前 q25 的 `30.208B` 根级拒绝触发，为 bitmap root 的每个 64-bit word 保存精确 `min A` 并在逐根展开前整字拒绝。随机 `100/100 + 50/50` 正确，fast20 scans 稳定减少 `6.30%`，但三次 completion 中位数 `219.604ms` 与 Test107 的 `219.400ms` 无差别；未再次运行 q25，源码撤回，见 `archive/test114_bitmap_word_root_minimum_20260715.md`。
- Test115 不改变算法，只为同一 Test107 源码增加编译期 D/A 层完成日志。DBLP g15 q1 完整运行得到 `16.1062710559 / 1953.203s / 3246.973MiB`，只有 10,000 秒目标的 `19.53%`；该结果只证明 q1，观测目标保留，见 `test115_dblp_g15_progress.md`。
- Test116 用 D3 截止诊断筛查四个查询规模档中的 g13 历史困难编号 `q2/q14/q25/q33`，q33 的前三层显著最重。随后完整 q33 得到 `8.3053688653 / 11505.101s / 19228.398MiB`，实际超出 10,000 秒 `15.05%`；D4 与 A6 分别耗时 `1944.255s/1730.497s`，completion 仅占 solver total 的 `2.51%`。这否定了全 g15 的统一时间结论，但压力样本不能用于估计总体均值，见 `test116_g15_variance_panel.md`。
- Test119 把每张已有普通 D row 归约为到永久锚路径的最小合法接入代价，再用无块数限制的规范化 subset partition 构造可行上界。q33 在 D2/D3 后得到 `10.0181663554/9.9770319196`，D3/D4 values 相对原轨迹下降 `21.16%/30.25%`。每行归约融合进 sorted row 存储，无 Hash；机制当前保留，见 `test119_anchor_path_facility_upper.md`。
- Test120 尝试用状态工作量 rent-or-buy 在层内重复反馈同一设施上界；q33 D2 有 `1.94%` 额外 values 收益，D3 只剩 `0.20%`，wall 落在波动内。调度与统计已撤回，见 `archive/test120_online_anchor_facility_20260716.md`。
- Test121 在已有 junction 压缩锚树上做节点内 D-block partition 与子树卷积，让多个 rooted block 共享锚树边。Toronto g13 q1--q5 panel 的 D3 values 合计下降 `8.61%`；q33 用 D2 rows 提前得到 `9.9770319196`，D3 values/pops 相对 Test119 再降 `4.28%/4.53%`，D3 截止 wall `1363.750s -> 1266.802s`。随机宽范围 `100/100`、固定 g15 `30/30` 正确；尚未运行 D4 或完整 q33，见 `test121_anchor_tree_block_facility.md`。
- Test122/123 扩充同一父树的 block 来源或 attachment roots 后候选逐项不变；Test124 再把父树放宽为 junction 度量闭包，q33 D2 values 下降 `2.45%`，但 Toronto g13 q1--q5 完整 wall 回退 `1.29%`。三条分支均已撤回，说明下一步不能继续在同一设施上叠加来源或拓扑，而要直接改变 D4/A6 的状态生成或扫描，见 `archive/test122_projected_anchor_tree_profile_20260716.md`、`archive/test123_exact_triple_anchor_tree_20260716.md` 与 `archive/test124_anchor_metric_closure_20260716.md`。
- Test125 取消 Test71 的固定 pivot，用 `q=ceil(h/2)` unrestricted endpoint P 和只接小 D 的 A 尝试消掉高层 closed-D 接口。它修复 Test96 的 `71/73` 反例并通过 `100+30` 次随机，但 Toronto g13 q3 返回 `0.8062097156`，高于 exact `0.7985684033`；让 P/A 读取全部低 D values 仍不修复。固定 anchor 路径旁的大组件需要第二 attachment，候选已撤回，见 `archive/test125_quarter_backbone_endpoint_20260716.md`。
- Test126 利用 directed-cut 的按组可加性，把 target-dependent future 化为两张 reduced rows 与一个全局势，并尝试用 ranked-bitmap 的逐字最小值在共同根展开前拒绝。随机 `100/100 + fixed g15 30/30` 正确，但 Toronto g13 q1--q5 总 wall `46.425s -> 47.549s`，q5 direct work 只降 `0.70%/0.42%`、peak 增 `9.26%`。独立 minima 丢失同根相关性，源码已撤回，见 `archive/test126_dual_cancelled_word_certificate_20260716.md`。
- Test127 将每个 attachment site 的 D2 paid-pair skyline 恢复为真实路径并集，并在见证边并集与同顶点诱导子图上精确求可行上界。快速 g12 单询问曾有 `4/5` 命中，但 Toronto g13 q1--q5 只有 `3/5` 命中；q3/q5 gap 分别为 `0.319%/10.933%`。最近 site 与单一 attachment distance 仍不足以保留第二接口，实验入口已撤回且未运行 DBLP，见 `archive/test127_paid_pair_witness_union_20260716.md`。
- Test128 不再把 D2 根限制在最近 site，而为每个 `<组对,anchor-tree site>` 恢复 `min_v D2(v)+dist(v,site)` 的根。Toronto g13 q3 的 3,564 个 site choices 只形成 619 个诱导顶点，上界 `0.8525860548`，比 exact 高 `6.764%`，还弱于 Test127。逐 block 独立最小化无法保留 block 间共享边，探针已撤回且未继续 q5/DBLP，见 `archive/test128_all_site_pair_envelope_20260716.md`。
- Test129 只跳过最终 A 层并保留全部 D 层。宽随机 `100/100` 与固定 g15 `300/300` 一度通过，但 7/4/4 decoy-tree 得到 `27 -> 29`；完整 Test80 只有 A6 consumer 恢复 exact。此前 q33 的 A5/A6 同值来自流式统计共享最终 best，不能视为 A5 独立贡献。探针已撤回，见 `archive/test129_penultimate_a_completion_20260716.md`。
- Test130 同时停于 `D_{h-1}` 与 `A_{h-2}`。宽随机在 g10 得到 `32 -> 34`，固定 g15 在 seed `717512` 第 90 例得到 `50 -> 51`；最高 D 层不能由同一三块完成族直接删除，源码已撤回且未运行真实数据，见 `archive/test130_penultimate_da_completion_20260716.md`。
- Test131 将四张独立行的 minimum 组合成最终 A consumer 的整 mask 下界。随机 `200+200` 正确，但 fast20 只拒绝 `147/4270=3.44%`，Toronto 两版均为零；未减少有实际重量的顶层生成，已撤回，见 `archive/test131_final_consumer_mask_lower_20260716.md`。
- Test132 用无向 min-plus 对称性把最终 consumer 从闭包生产侧精确转置为闭包补集侧。随机 `200+100` 正确，但 fast20 合计回退 `0.82%`，同机 Toronto g13 q1--q5 合计 `45.659s -> 47.192s`、仅 q4 改善；DBLP q5 最终段局部下降但 wall 回退。机制不跨询问稳定，源码已撤回，见 `archive/test132_transposed_final_consumer_20260716.md`。
- Test133--136 把 exact rooted D row 压缩为 hub 度量锥或已支付锚路径标量锥，并尝试增强最高层或全部 D/A 层。下界定理正确，随机门全部通过；但 Test133 的 fast20/Toronto 五询问分别回退 `2.65%/2.04%`，全 hub 版回退 `11.73%`，全层路径版只少 `0.03%` 普通 values 且回退 `3.62%`。实现已撤回，理论式见 `archive/test133_136_exact_row_cones_20260716.md`。
- Test137 将 exact `A({i},v)` 提前生成并复用为正式 A1，同时用其逐点最大值增强普通 D future。宽随机 `300/300`、固定 g15 `100/100` 正确，fast20 D values 减少 `47.28%`，Toronto g13 五询问 wall 改善 `17.17%`；但 DBLP g15 q2/q14 D2 状态不变，q33 D3 values 仅降 `2.94%`，probe wall/peak 回退 `3.25%/3.06%`。实现已撤回，见 `archive/test137_early_anchor_pair_lower_20260716.md`。
- Test138 把 quarter witness 从 `1+3` 扩为 `2+2` 双汇合点，并对原四块 partition 的六种锚侧二块方向做精确最短路闭包。随机 `300+100` 正确；fast20 与 Toronto g13 q1--q5 的 D/A values 均逐项不变，Toronto 18 个非空方向零更新、wall 合计回退 `2.79%`。未启动 DBLP 即撤回，见 `archive/test138_quarter_two_root_upper_20260716.md`。
- Test139 将 Test126 的独立 minima 改为角色定向的 `reduced accumulator + complete branch` 同根 word 证书，并按首次真实 consumer 惰性物化。随机 `100+50+100+30` 正确，fast20 D/A direct work 降 `12.98%/3.95%`；但 Toronto g13 q1--q5 合计 wall 回退 `1.46%`、q5 peak 增 `10.606MiB`，且 D3 截止完全不命中。未运行 DBLP 即撤回，见 `archive/test139_role_directed_joint_word_certificate_20260716.md`。
- Test140 恢复 Test121 标量设施方案的真实 D2/singleton witness 与锚树路径，并按原图 edge id 去重；同一方案的并集值理论上支配加法值。随机 `100+30` 正确，但 Toronto g13 q1--q5 只有 q1 的 D3 values/pops 降 `7.48%/7.88%`，其余状态不变；直接按边际并集目标选择组件又在五条上全部更弱。wall 合计 `46.454s -> 52.862s`，未运行 DBLP即撤回，见 `archive/test140_anchor_tree_witness_union_20260716.md`。
- Test140 后保留一条理论结果：固定 `(core,B,C)` 的 paid-profile consumer 可由两个 rooted block macro labels 精确定价，无需 `n^2` 双端点表；现有显式小图对拍为 `500/500 + 200/200`。但独立枚举全部 consumers 会产生 `5^k` join 工作，见 `history/two_attachment_consumer_scalarization.md`。
- Test141 只请求 Test121 最优设施方案的一个规范分块，并把 block 数限制到至多 `ceil((g-1)/2)`，用 `3^(g/2)` 型全图 macro DP 联合选择 attachments。随机 `100+30` 正确，fast20 D/A values 降 `3.72%/0.23%`，但正式 pops 不变，新增 `4.44M` macro pops，wall `8.090s -> 12.367s`。作为附加 upper oracle 已撤回，未进入 Toronto g13 或 DBLP，见 `archive/test141_facility_guided_macro_lifting_20260716.md`。
- Test142 不再追加 upper oracle，而把高层 permanent-anchor A 格本身改为伴随反向求值：低层 A 正向生成，高层从 `A+D+D` 终端沿所有原依赖边反传，并用 `anchor union mask` 的前缀下界剪枝。独立探针覆盖 Test96、随机图和所有切分；生产黑盒宽范围 `100/100`、固定 g13 `20/20`、固定 g15 `10/10`，Toronto 默认 `160/160`。fast20 为 `8.128s -> 7.961s`，Toronto g13 q1--q5 为 `50.369s -> 49.407s`，DBLP g13 q5 为 `159.977s -> 143.363s`。DBLP g15 q33 长门精确得到 `8.3053688653 / 9058.212s / 14342.977MiB`，相对 Test107 改善 `21.27%/25.41%`，但仍超出 8,000 秒 `13.23%`；伴随阶段的 `54.808B` join checks 是新主瓶颈，见 `test142_adjoint_anchor_lattice.md`。
- Test143 只把高层终端一侧改读 root-irreducible branch，Toronto g15 checks 下降约 `23.5%` 但伴随时间持平，已完整撤出活动源码。Test144 将边界候选提前到每张 H 行完成时，Toronto g15 伴随时间小幅改善，为 Test145 保留的调度组成。
- Test145 利用 directed-cut 势的组可加性，把目标相关的 `D(L)+D(R)+pi_T` 判断改写为根顶点上的统一约化预算，一次生成所有高层终端。逐根按精确探测数在全局有序与补集子掩码路线间择优，使最坏 pair work 保持 `O(3^k)`。独立三路探针 `500/500`、生产宽范围 `100/100` 与固定 g15 `30/30` 均通过；Toronto g15 q1--q5 从 Test144 的 `522.320s` 降到 `387.645s`。DBLP g15 q33 精确得到 `8.3053688653 / 7594.956s / 14451.762MiB`，低于目标 `405.044s`；相对 Test142，完整 wall 改善 `16.15%`，伴随阶段 `2083.777s -> 276.020s`，见 `test145_directed_cut_transposed_terminal.md`。
- Test146 针对五数据集全量 D2 发现的 eager-tree 回退，只累计 incumbent 理论上可避免的 ordinary queue pops 与 relax attempts；累计工作达到一次锚树 DP 的精确成本后才购买求值。Toronto 冻结五问从 eager `174.909s` 的负优化恢复到 no-tree 轨迹，最终实现随机对拍 `100/100`。规则不使用数据集、固定 `g`、层号或时间阈值，见 `test146_amortized_anchor_tree.md`。
- 五数据集 200 条 g15 D2 显示 DBLP/LinkedMDB wall 与 ordinary values 高度相关，Toronto 的 eager tree 和 MovieLens 的 eager dual 则分别成为跨库负优化。Test147 lazy dual 因 D3 信号过晚撤回；Test148 decrease-key heap 虽使 DBLP q20 D2 快 `26.0%`，但属于 baseline 可共享的通用工程替换且违背活动实现采用 `priority_queue` 的约定，已完整撤出。全量分布、极端询问和保留边界见 `g15_five_dataset_profile.md`。
- Test149 利用原始组距离已经满足未改写弧三角不等式，只从此前 dual 势可能降低过的有向弧启动残量 Dijkstra。随机 DPBF `200/200`，五库面板的普通状态逐项不变；MovieLens 全 40 条 D2 的 dual `562.484s -> 533.664s`、扫描比例中位数 `6.24%`，冻结五问 D3 wall `94.772s -> 86.254s`。DBpedia q34 与 LinkedMDB q40 的 dual 分别下降 `20.6%/21.3%`。该机制无数据特判、不压缩图且只服务本方法的 sequential residual，已作为 ReleaseV5 的 dual 构造基础冻结，并由 Test152 继续复用，见 `test149_changed_arc_dual.md`。
- Test150 关闭完整 dual 后确认 `g=4` 的主要回退来自 eager fixed cost；Test151 再按总预算一次性延迟购买，虽为 `420/420` 正确，却在高 `g` 通常比 V5 慢 `20%--30%`，因为前期弱搜索与完整 dual 被重复支付。两者只保留归因证据，完整目标和一次性 lazy 分支均已删除。
- Test152 将同一 sequential dual 拆成 `g` 个合法前缀，并由 ordinary row 已发生的 seed candidates、queue pops 和 relax attempts 每累计 `2m+n` 购买一组。三库 `g=4..10`、每点 20 条共 `420/420` 权重一致；`g=5..10` 的 18 个单元均快于 Strict，`g=9..10` 优势为 `11.97x--22.00x`，`g=4` 仍慢 `12%--26%`。物理删除 quarter 后又通过 DPBF `300/300`，见 `test152_progressive_dual.md`。
- Test153--155 分别删除 junction/facilities、early+quarter uppers 和 facilities/tree。前两者在高 `g` 可回退到 `1.98x/1.22x`，后者虽在短面板近中性，但既有重查询仍有 `4%--22%` state 与 `7%--14%` wall 收益，因此均保留。Test156 单独证明 quarter 仅在 `2/30` 条短暂更新且无端到端收益，相关源码、统计和临时目标已删除；packing 在 `16/30` 条非零，early upper 在 `18/30` 条更新，继续保留。归档见 `archive/test150_151_153_156_dual_ablation_20260718.md`。

## 5. 共同纪律

1. 关键计时使用 Release/O2。
2. 正确性以 DPBF、`1e-6` 和 Toronto 最后一次完整 run 为准。
3. 追加结果只比较最后一个 run header 后的记录。
4. 策略只能由证明、状态结构或理论工作量推出，不能拟合数据集、组数、层级、密度或 wall time。
5. 论文引用必须同时说明“文献机制”和“本仓库适配”；来源见 ReleaseV5 实现文档。
6. ReleaseV3--V5 与 Test80/Test149/Test152 的 full DBLP 证据只在有重要算法或发行节点时更新；Test152 当前的预处理调度和代码删减不触发重复长跑。
7. 运行后清理临时目录、空结果目录和残留进程。

## 6. 历史入口

有效研究文档索引见 `history/README.md`；失败与撤回机制索引见 `archive/README.md`。需要追踪 ReleaseV3 的形成过程时，先读 `history/half_global_hybrid.md`，再按其中引用进入 archive 原始日志。
