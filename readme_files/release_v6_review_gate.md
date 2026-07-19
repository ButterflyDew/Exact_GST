# ReleaseV6 论文评审门：当前判定与剩余投稿证据

更新时间：2026-07-18。本文档回答两个分开的门：**当前 Test152 是否足以冻结为论文候选方法，以及是否已经可以创建 ReleaseV6？** 方法冻结门检查主线、正确性、复杂度、无工程特判、整算法贡献和同源效果；投稿实验门检查传统五库与同机 GPU 对照。后者可以在方法冻结后补齐，不能反过来否定已经成立的方法结构。

## 1. 当前总判定

**方法冻结门与发行实现门均已通过，ReleaseV6 已创建。** 这一判定不要求每个局部操作都是首次提出，而要求已知构件的组合形成一套此前完整算法没有采用、推导非平凡、主线清楚、无工程特判且有稳定独立效果的 exact GST 方法。Test152 当前满足该尺度：

1. **整算法贡献成立，局部 claim 主动收窄。** Iwata--Shigemura 已给出平衡三分解和半状态 MITM；Goodman、Li--Eisner 与 Gildea 覆盖通用 outside；packing 与 dual reduced-cost 也有先例。Test152 的贡献不在重新发明这些原语，而在永久可选组锚定下，把低层 inside、高层 closure-aware outside 和补集终端转置组织成同一 exact GST 求值过程。最近的 PrunedDP++、GPU4GST、Iwata MITM 和通用 outside 都没有给出这套完整执行结构。
2. **正确性与复杂度证据已经闭合。** Test152 已覆盖重叠组语义、strict branch、`F/H` 闭包位置、第一条跨切分边和 directed-cut 终端双射，并由 Test157 独立核验；完整实现级求和没有隐藏超过 `O(3^g)` 的 mask 工作。
3. **整算法两个结构增量均有同源效果。** 公平 M0 到永久锚定 M1 在 Twitch/Github `g=12` 两轮合计为 `3.187x/2.615x`；M1 到 `F/H + transpose` 的 M3 为 `1.098x/1.202x`，且反序重复方向不变。M2 单独不稳定、Test158 strict `Br` 只有 `1.018x`，因此二者都不被夸大为独立核心。

传统五库冻结面板和同机 GPU4GST 仍是投稿实验缺口；实际空间方向混合，因此论文不能声称稳定省内存。它们不阻止清理 V6，但必须在最终实验章节如实补齐。若发行清理暴露状态语义、无特判或性能回退问题，修改只能回到 Test152/Test157 脉络验证，不能直接在 Release 中试错。

## 2. 可以讲清楚的候选主线

当前实现可以用下面这条主线清楚描述。它已经通过**完整方法贡献**评审，但每个局部构件仍须按第 4 节承认已有来源：

1. 选择一个永久锚点组 `T_a`，把非锚组记为 `K`。
2. 正向计算普通 rooted 状态 `D(S,v)`，只保存平衡分解会使用的半层子集。
3. 用包含锚点的 `A(S,v)` 表示锚定侧。低层 `A` 正向计算；高层 `A` 不物化，而把完整解终端沿同一依赖 DAG 反向传播。反向递推先形成闭包后状态的原始后缀 `F(S,v)`，再保存经图闭包拉回的 `H(S)=C(F(S))`。
4. 完整解在平衡根处由一个锚定部分和两个普通部分汇合。低层目标直接用 `A+D+D` 结算；跨越低/高切分的目标通过 `A/H` 第一条跨切分依赖结算。
5. directed-cut 势的组可加性把高层目标反复执行的 `D+D` 终端改写为固定根上的一次约化枚举，再把事件分发到相应 `H` 目标。

这五步可以等价解释为：在既有半状态 MITM 的 min-plus deduction hypergraph 上执行截断的 outside computation，再以模 dual 势重加权 packing events。论文贡献是这套面向 GST 的联合求值组织；组距离、tour/cut lower bound、junction、锚路径／锚树设施上界、剩余容量打包、有序行和渐进 dual 只作为正确且可消融的优化模块。

## 3. 硬性评审矩阵

| 评审门 | 当前证据 | 当前判定 | 通过条件 |
| --- | --- | --- | --- |
| 单一算法主线 | Test152 不调用 V3/B，也没有运行时回退 | 基本通过 | 发行伪代码只保留 `D/A/H + transpose` 主线，其他模块单列 |
| 无无理特判 | 没有数据集、固定 `g`、query、density 或 wall-time 分支；quarter upper 已从活动 Test 撤出 | 基本通过 | 审计未来发行代码仍无这些分支；所有结构边界有公式与证明 |
| 精确性 | 源码对齐证明稿、Test157 独立 oracle、1000 个生产 DPBF 随机实例、三类 row validator、3000 个 packing 与 2000 个 progressive 随机实例均通过 | 通过 | 核心或辅助模块变化后重跑相应 verifier |
| 完备分解 | Test157 逐格检查 strict `Br`、显式 `F/H`、每个 cut、转置目标与 oracle 最优树分箱 | 通过 | 若核心状态定义变化则重新运行同一独立核验 |
| 复杂度 | `test152_full_complexity_audit.md` 已逐项求和 core、tour、dual、packing、junction 与 facilities；无隐藏 `4^k` 枚举 | 通过 | 核心或辅助模块变化后重新逐项求和 |
| 整算法贡献 | 逐组件排重否定宽 claim；整算法对照又确认最近完整方法均没有“永久可选组锚定 + 低层 inside + 高层 closure-aware outside + 补集终端转置”的联合执行结构，且两个结构增量都有同源效果 | 方法冻结通过 | 只主张新的 exact GST 求值组织；完整边界见 `test152_integrated_method_review.md` |
| 相对 CPU baseline | `g=5..10` 三库全部快于 Strict PrunedDP++；M5 三图 `g=11..16` 完成 176/180 条，唯一 timeout 为 Github g16 q2，但高档尚无可完成的 Strict 对照 | 局部通过 | 完成传统五库冻结多询问面板；高档无可比 baseline 时明确标为可运行性而非加速比 |
| 相对 GPU4GST | 数据与查询已接入，但没有同机作者代码对照 | 投稿实验待补，不阻止冻结 | Linux 同机 8 库 `g=3,5,7` 作者查询直接比较 |
| 候选机制消融 | 公平 M0/M1 两轮合计为 `3.187x/2.615x`；Twitch/Github M1/M3 为 `1.098x/1.202x`，Musae `g=13` 为 `1.164x`；420 条 M4/M5 面板冻结 M5，三图高档面板完成；Test158 strict `Br` 两轮 240 对为 `1.018x` 且无空间收益 | 方法冻结通过，投稿广度待补 | 补传统五库；不宣称 M1--M3 稳定省空间、M5 逐档更快或 `Br` 构成强独立贡献 |
| 可审阅发行实现 | `release_v6.cpp` 约 2556 行；独立冻结 M5，已删除研究开关、统计、probe、validator、quarter upper 和调试输出；不调用 Test/旧 Release | 通过 | 后续核心变化必须先回到 Test 脉络验证 |

只有标为“方法冻结”或“发行实现”的门会阻止创建 ReleaseV6；标为“投稿实验待补”的项目允许在发行候选冻结后继续完成，但最终论文不能省略。

## 4. 相关工作重合矩阵

### 4.1 已知方法，不能主张原创

| 当前机制 | 已知工作 | 评审结论 |
| --- | --- | --- |
| rooted subset DP 与同根合并／图闭包 | Dreyfus--Wagner；PrunedDP++；GPU4GST CDP | 基础框架 |
| 固定一个根终端、对其余终端做子集状态 | Dijkstra Meets Steiner | 永久锚点思想本身不足以构成贡献 |
| 三个 rooted 子树在平衡点汇合、每块至多一半 | Iwata--Shigemura 2019, Corollary 1 | 与当前半状态三分解直接重合 |
| 只处理至多半大小的子集并在终端合并 | Iwata--Shigemura 2019, Meet in the Middle | 不能把删掉上半层本身称为新颖 |
| 在 min-plus deduction hypergraph 上计算 reverse/outside values | Goodman 1999；Li--Eisner 2009；Gildea 2020 | 交换半环 outside 直接覆盖 min-plus；`F/H` 是把 closure edge 压成最短路后的专门状态 |
| 不相交子集对及按并集／目标聚合 | Bjorklund et al. 2007 subset convolution / packing product | completion 的配对关系本身不是新转置 |
| admissible future lower bound | Dijkstra Meets Steiner；DS* | farthest/tour/cut 的“用作下界”不是贡献 |
| directed-cut dual ascent 与 reduced-cost pruning | Wong 1984；Polzin--Vahdati Daneshmand 2001 | 势下界、约化代价和界删除路线不是贡献 |
| 稀疏表、位图、有序列表、Dijkstra、changed-arc seeds | 通用算法与工程实现 | 只能报告实现贡献与消融 |

Iwata--Shigemura 还明确在实验中包含由 Group Steiner reduction 得到的实例。因此“我们处理的是 GST 而不是普通 Steiner”不能自动排除其 meet-in-the-middle 结果；同样，outside 文献不以 Steiner 命名，也不影响其通用递推对当前推导超图的覆盖。

### 4.2 逐组件结论与整算法贡献

1. **永久组锚定不能单独 claim。** `A(empty,v)=dist(v,T_a)` 以一个可选终端组为锚，并把普通 `D` 与唯一含锚侧 `A` 分开；固定 root 后消掉一维已有先例。它在整算法中负责定向平衡分解，M0/M1 又证明这种组织具有强实际效果。
2. **closure-aware `F/H` 不能单独 claim。** `F` 是闭包后节点的 outside value，`H=C(F)` 是 outside 反穿 closure edge 后在合并根的值；其正确性属于通用 outside 的专门化。它在整算法中负责只正向求低层 `A`，并以唯一第一条跨切分边接入高层后缀。
3. **directed-cut 约化终端事件不能单独 claim。** 事件关系、模函数消去与 reduced-cost filtering 分别已有先例；在整算法中，它解决逐高层目标重复 completion 的实际瓶颈。M2/M3 结果表明转置是 outside 联合机制获得稳定收益的必要部分。
4. **root-irreducible ordinary branch basis 只作辅助。** Test158 中 strict 发布数减少 `15.69%`、普通拼接工作减少 `11.42%`，但两轮 240 对仅为 `1.018x`，query peak RSS 与最坏复杂度均无改善。

逐组件排重后的论文贡献是**四者的受限联合执行结构**，不是其中任何一个局部公式。完整算法与最近完整方法的差异、非平凡 GST 专门化、状态数边界和允许 claim 见 `test152_integrated_method_review.md`。

### 4.3 不应作为论文核心的机制

- Test152 的渐进 dual、Test146 的上界购买、剩余容量打包属于工作量调度；它们可以改善低 `g`，但不改变状态空间或最坏复杂度。
- `2m+n` 表示 residual arc 与势向量的结构规模，却没有精确覆盖 priority-queue residual Dijkstra 的全部实际工作。除非给出竞争性或摊销定理，论文只能称其为无数据依赖的确定性调度规则，不能称为理论最优购买策略。
- junction、锚路径／锚树设施和三块早期上界都是合法 primal upper bound。它们应在实验中作为 pruning modules 消融，不应混入 `D/A/H` 的完备性证明。

## 5. 已完成的正确性与复杂度定理链

源码对齐的完整证明见 `test152_core_correctness.md`，实现级复杂度求和见 `test152_full_complexity_audit.md`，独立小图核验见 `tools/test157_core_verifier/`。本节记录已经成立的核心结论与仍需保持的实现边界；整算法贡献和效果分别由独立文档与消融证据评审。

### 5.1 Group-balanced decomposition

证明稿先固定每个查询组在一棵最优树中的一个代表命中顶点，以组标签数作为顶点权重取加权重心，再用三箱合并引理得到一个含锚部分和两个普通部分。它覆盖锚组代表就是平衡根、一个顶点同时命中多个组、零权路径与空普通块；Test157 又在 brute-force oracle 最优树上独立枚举根与分量分箱，`6050/6050` 个随机实例及全部手工边界通过。本引理当前通过。

### 5.2 Branch-basis completeness

证明稿把 `Br(Q,v)` 写成严格不可拆分条件，并用按 mask 大小归纳的重新结合证明：任意未发布的等值侧都可把其累计子侧并入原 pivot 侧，同时保留一个更小的已发布 branch。由此受限 seed 与标准 canonical seed 逐点相等；等值拆分、零权边和共享代表不会因严格 `<` 丢解。Test157 已独立比较 `1,956,213` 个 standard/restricted mask-root 单元。未剪枝 branch-basis 引理当前通过；生产剪枝后的“可多发布、不可少发布”仍留在实现审计门。

### 5.3 Adjoint equivalence

旧表述把 `H` 直接称为“从闭包后状态出发的附加代价”，少区分了一次闭包。源码对齐定义为：`F_T(v)` 是已经得到闭包后 `A_T(v)` 时的原始后缀，`H_T=C(F_T)` 是把该后缀拉回到进入 `T` 的合并根。递推为

```text
F_T = min(G_T, min_(U strict-superset T) [Br_(U-T)+H_U]),
H_T = C(F_T).
```

四点已在证明稿中逐项给出，并由 Test157 的显式正向后缀逐行核验：

1. 反向递推逐边对应正向 `A` 依赖；
2. 无向最短路闭包的自伴随恒等式允许跨过 closure；
3. 每条从低层到高层终端的正向路径存在唯一第一条跨切分边；
4. 在该边结算 `A_low + Br + H_high` 与物化全部高层 `A` 的最优值完全相同；边界不能误用尚未拉回闭包的 `F_high`。

本定理继续作为实现正确性的核心接口，但扩大引文链已将其映射到 min-plus deduction hypergraph 的 outside recurrence；正确性通过不再意味着新颖性候选。

### 5.4 Transposed-terminal identity

证明稿已在 partial/full directed-cut 势下给出同一组可加恒等式，并建立“原无序普通二分 <-> 单 entry 或不相交 entry 对”的双射；同一 `<T,v>` 只聚合真实成本的最小值。Test157 已比较 `13,044,460` 个 exhaustive/sorted/submask 逐目标单元，转置恒等式当前通过。其不相交事件属于 packing product，势消去属于模函数重加权，因此也只保留正确性与实现价值。

### 5.5 Complexity theorem

核心草稿已经分开计数：

- `D`、低层 `A` 与 `H` 的 mask 行数；
- ordinary split、adjoint reverse edge 与 completion partition 的总关系数；
- transposed global-pair/submask 两条路线的事件数；
- 每张非空行的一次多源图闭包；
- group distance、dual、tour、facility upper 与 packing 的预处理时间和峰值空间。

核心行、反边、completion 与转置两路线已经证明没有在隐藏事件空间中重新引入超过 `O(3^k)` 的 mask 工作。`test152_full_complexity_audit.md` 又完成 group distance、tour、junction、progressive dual、packing、scalar facility 与 anchor-tree facility 的实现级求和；完整口径仍为 `O(poly(g)(n3^k+2^k C_G))`。需要保留的表述边界是：progressive dual 的 `2m+n` 只是不含经验参数的结构调度，不是严格摊销定理。

## 6. 效果评审需要的冻结证据

### 6.1 贡献阶梯消融

在同一源码、同一编译器、同一查询顺序下固定以下阶梯：

| 阶梯 | 目的 |
| --- | --- |
| M0：Iwata-equivalent half-state MITM | 给出已知半状态起点 |
| M1：permanent group anchor + branch basis，物化完整所需 A | 隔离锚定组织本身 |
| M2：M1 + adjoint H | 测量消掉高层 A 的时间／空间价值 |
| M3：M2 + transposed terminal | 隔离转置对 H 终端的价值 |
| M4：M3 + pruning modules | 形成完整 Test149 主线 |
| M5：M4 + Test152 progressive dual | 只评估低 `g` 固定成本调度 |

Test157 现已为 M0--M5 建立同一个 `SolveOneQuery` 的编译期阶段，配置和当前结果见 `test157_same_source_ablation.md`。公平 M0 使用全部 `g` 个组的 `floor(g/2)` ordinary 行和规范三块终端，独立 verifier、300 个生产随机实例和 101 组真实 M0/M1 比较均无差异。Twitch/Github `g=12` 两次 M0 与两次重建 M1 的总时间倍率为 `3.187x/2.615x`。历史 Half_DPBF 继续保留，但不进入该消融；详细审计见 `test157_m0_fair_reference.md`。

结果给出三个重要边界。第一，M2 在 Musae/Toronto `g=12` 上更快，却在 Github/Twitch 扩展面板和 Musae `g=13` q20 上不稳定，不能单独主张 adjoint 加速。第二，M3 联合机制在早期三个 `g=12` 数据集上相对 M1 为 `1.053x--1.428x`，Musae `g=13` q20 为 `1.164x`；新增 Twitch/Github `g=12` q20 反序重复后，两轮合计分别为 `1.098x/1.202x`，四次比较方向均为正。第三，query peak RSS 仍然方向混合。由此 **`F/H + transpose` 的时间效果门通过；空间收益 claim 不通过，投稿实验广度仍待补。**

Test158 随后检查是否能把剩余的 strict `Br` 提升为独立核心。三图 `g=9,10` 各 q20 的两轮反序结果共 240 对，权重全部一致；strict 版本减少 `11.42%` 的普通拼接工作，但首轮 `all/M5=1.0371`、反序轮 `0.99995`，合并仅 `1.0182`，平均 query peak RSS 还略高。该结果支持 M5 继续保留 `Br`，同时关闭其论文核心候选，详见 `test158_root_irreducible_branch_ablation.md`。

### 6.2 冻结运行面板

在启动昂贵全量前先运行：

1. GPU4GST Musae/Twitch/Github：`g=4..12` 每点前 20 条；`g=13..16` 每点冻结 5 条，1000 秒单实例 cutoff。
2. Toronto/DBLP/DBpedia/LinkedMDB/MovieLens：使用已有固定多询问清单，覆盖低、中、高 group overlap，不以 q1 代表整档。
3. correctness oracle：所有可由 DPBF 完成的实例逐条 `1e-6` 对拍；高 `g` 至少与 V5/Test149 逐条一致，并保留随机小图反例生成。
4. 每个阶梯报告 solver wall、各阶段 wall、query peak RSS、状态／事件数、完成率和 timeout；结果文件按最后一次 run 解析。

当前冻结进度见 `test157_frozen_panel.md`。公平 M0/M1 已完成 Twitch/Github `g=12` q20 两次运行；M1--M4 完成首轮，M1/M3/M4 完成反序重复；三库 `g=4..10` 共 420 条 M4/M5 配对的总时间为 `212.569/207.016s`，据此冻结 M5 为唯一候选。冻结 M5 又在 Musae/Twitch/Github 的 `g=11,12` 各跑前 20 条、`g=13..16` 各跑前 5 条，共完成 176/180 条；Musae/Twitch 均为 60/60，Github g16 q2 超过 1000 秒后终止，后三条未启动。Musae 相同 q1--q5 的 M4/M5 在 g13--16 为 `0.979x/1.063x/1.132x/1.082x`，没有系统性高档回退，也不构成逐档统治。完整散列、逐档时间和 RSS 见 `test157_m5_g11_g16_frozen.md`。这些结果通过了 GPU4GST 三图高档收集子门，仍不能替代传统五库或同机 GPU baseline。

除非方法结构再次发生突破，不运行五小时级 DBLP g13 全量。先用上述冻结多询问门判断贡献与最大可运行 `g`，再决定论文级 8 库 300 条全量。

## 7. 下一步顺序

1. **先冻结方法，不再无限追逐局部原语新颖性。** 当前整算法贡献、正确性、复杂度和同源效果已经通过；不得为寻找“每一步都原创”继续堆叠机制。
2. **先在 Test 脉络解决发行审计暴露的问题。** 若清理时发现状态语义、复杂度、工程特判、低档固定成本或真实查询回退，先修改 Test152/Test157，重跑相应 verifier 和同源面板；Release 只接收已验证结果。
3. **保持 ReleaseV6 冻结。** M5 已独立清理为单一 `D/A/F/H + transpose` 路线；后续研究机制不得直接回灌发行源码。
4. **补齐投稿实验。** 传统五库使用冻结多询问，不以 q1 代表整档；GPU4GST 同机对照等待合适的 Linux/CUDA 硬件。除非方法结构发生实质变化，不运行五小时级 DBLP g13 全量。

## 8. ReleaseV6 已满足的创建条件

创建 V6 前必须满足以下**方法与实现条件**：

- 第 3 节所有方法冻结门通过；投稿实验待补项不阻止代码冻结；
- 第 5 节定理链有完整中文证明稿，随机和真实数据均无反例；
- 逐组件排重与整算法评审同时通过，论文 claim 明确落在完整 GST 求值组织，而不是把已知原语改名；
- 第 6 节 M0--M5 同源消融和 `g=11..16` 冻结面板已经证明效果不是单查询偶然值；
- 活动 Test 源码中没有数据集、固定 `g`、询问、密度、wall-time 或经验超参数分支；
- 明确哪些优化进入最终方法，删除未确定、有开关、重复、调试和无贡献代码；
- V6 源码不调用任何 Test 或旧 Release，且通过 DPBF 随机对拍、冻结真实查询与 Release/O2 构建。

上述方法与实现条件均已成立。ReleaseV6 已通过 Release/O2 构建、随机 DPBF `500/500`、Twitch `g=10` q20 和 Musae `g=13` q5 的 M5 同源对拍；验证与复现命令见 `release_v6.md`。传统五库完整冻结面板和同机 GPU4GST 仍属于投稿实验待补项。

## 9. 主要原文

1. Yoichi Iwata and Takuto Shigemura. *Separator-Based Pruned Dynamic Programming for Steiner Tree*. AAAI 2019. Meet-in-the-middle、平衡三分解与半层状态见第 4 页：<https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843>。
2. Stefan Hougardy, Jannik Silvanus, and Jens Vygen. *Dijkstra Meets Steiner: A Fast Exact Goal-Oriented Steiner Tree Algorithm*. Algorithmica 2017：<https://arxiv.org/pdf/1406.0492>。
3. Johannes K. Fichte, Markus Hecher, and Andre Schidler. *Solving the Steiner Tree Problem with Few Terminals*. SAT 2021：<https://arxiv.org/pdf/2011.04593>。
4. S. E. Dreyfus and R. A. Wagner. *The Steiner Problem in Graphs*. Networks 1971：<https://doi.org/10.1002/net.3230010302>。
5. Jiayu Li et al. *Fast Optimal Group Steiner Tree Search using GPUs*. PACMMOD/SIGMOD 2025：<https://doi.org/10.1145/3769792>。
6. Yeow Meng Chee et al. *Efficient and Progressive Group Steiner Tree Search*. SIGMOD 2017：<https://doi.org/10.1145/2882903.2915217>。
7. Ai Azuma, Masashi Shimbo, and Yuji Matsumoto. *An Algebraic Formalization of Forward and Forward-backward Algorithms*. 2017：<https://arxiv.org/pdf/1702.06941>。其正式 backward 部分要求 cancellative semiring，并明确不覆盖 tropical 变体。
8. Joshua Goodman. *Semiring Parsing*. Computational Linguistics 1999：<https://aclanthology.org/J99-4004.pdf>。
9. Zhifei Li and Jason Eisner. *First- and Second-Order Expectation Semirings with Applications to Minimum-Risk Training on Translation Forests*. EMNLP 2009：<https://aclanthology.org/D09-1005.pdf>。
10. Daniel Gildea. *Efficient Outside Computation*. Computational Linguistics 2020：<https://aclanthology.org/2020.cl-4.2.pdf>。
11. Liang Huang. *Advanced Dynamic Programming in Semiring and Hypergraph Frameworks*. COLING 2008 tutorial：<https://aclanthology.org/C08-5001.pdf>。
12. Andreas Bjorklund et al. *Fourier Meets Mobius: Fast Subset Convolution*. STOC 2007：<https://arxiv.org/pdf/cs/0611101>。
13. Richard T. Wong. *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*. Mathematical Programming 1984：<https://doi.org/10.1007/BF02612335>。
14. Tobias Polzin and Siavash Vahdati Daneshmand. *Improved Algorithms for the Steiner Problem in Networks*. Discrete Applied Mathematics 2001：<https://doi.org/10.1016/S0166-218X(00)00319-X>。

逐组件映射与页码见 `test152_novelty_audit.md`，整算法差异和投稿表述边界见 `test152_integrated_method_review.md`。当前判定同时承认两点：通用 outside 足以否定“`F/H` 是新通用原理”的宽 claim；已知完整算法又没有给出 Test152 的联合 GST 执行结构。论文贡献采用后者，不能隐去前者。
