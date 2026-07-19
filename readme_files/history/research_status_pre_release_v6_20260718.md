# ReleaseV5 / Test152 当前研究状态

更新时间：2026-07-18。本文只维护**当前仍有效的实现、证据与开放问题**。完整实验时间线见 `test_series_overview.md`；失败或已撤回方法见 `archive/README.md`；仍有效但不在当前源码中的理论结论见 `history/README.md`。这样旧实验不会与当前主线混写。

## 1. 当前结论

ReleaseV5 是当前纯框架 A 发行版，由 Test149 的正确主线独立冻结得到。它包含 permanent-anchor 有序 D、低层 A、伴随 H、转置终端和已改写弧 dual 闭包；没有调用 Test、ReleaseV3 或 ReleaseV4，也没有按数据集、固定 `g`、层号、密度或运行时刻切换算法。**ReleaseV5 本身没有改动。** Test152 是当前后继研究候选：它按 ordinary 搜索已经发生的结构工作量逐组购买 dual 前缀，用于消除小 `g` 的 eager fixed cost；Test149 继续保留发行来源统计，ReleaseV4 保持为较早的纯 A 冻结版。

| 版本 | 角色 | 当前有效证据 |
| --- | --- | --- |
| ReleaseV3 | 冻结的框架 B 对照 | DBLP g13 q1 `531.556s / 3870.7MiB` |
| ReleaseV4 | 较早的纯 A 冻结版 | fast20 `8.303s`；DBLP g13 q1 `544.379s / 2158.6MiB` |
| ReleaseV5 | 当前纯 A 发行版 | DPBF `300/300`；五库 g10 q1--q5 与 Test149 `25/25` 权重一致，总时间 `933.453s -> 900.820s` |
| Test80 / Test107 | ReleaseV4 的研究后继 | DBLP g13 q25 `8121.863s / 18498.1MiB`；DBLP g15 q1 `1953.203s / 3247.0MiB`；g15 q33 `11505.101s / 19228.4MiB` |
| Test119 / Test121 | permanent-anchor 层末上界候选 | q33 D3 截止 `1363.750s -> 1266.802s`；D3 values 相对 Test119 再降 `4.28%` |
| Test142 | 伴随锚点格基线 | DBLP g15 q33 `11505.101s -> 9058.212s`，peak `19228.398 -> 14342.977MiB`；未达 8,000 秒 |
| Test145 | directed-cut 转置终端活动候选 | DBLP g15 q33 `7594.956s / 14451.762MiB`，达到 8,000 秒目标；答案 `8.3053688653` |
| Test146 | 摊销锚树上界基线 | Toronto 冻结五问不再执行负收益 tree；五数据集 200 问规则统一 |
| Test149 | ReleaseV5 的研究来源：已改写弧 dual 闭包 | MovieLens 冻结五问 D3 `94.772s -> 86.254s`；随机 DPBF `200/200` |
| Test152 | 当前 Test 候选：按 ordinary 工作量渐进购买 dual | 三库 `g=4..10` 共 `420/420` 权重一致；`g=5..10` 全部快于 Strict，`g=9..10` 为 `11.97x--22.00x` |

GPU4GST 三个小图的冻结相关查询扩展曲线补充了发行版的跨 `g` 证据。ReleaseV5 在 `g=4,5` 与 PrunedDP++ 同一数量级但仍慢 `2.06x/1.61x`；`g=8` 的聚合优势为 `4.77x`，到 `g=9` 才在聚合和几何平均上同时超过 `10x`。`g=10..12` 的时间与绝对 query peak RSS 都稳定超过 `10x`；`g=13..15` ReleaseV5 全部完成，而 Strict 完成率降为 `5/9、4/6、2/6`。后者只能作为 completion 优势，不能把完成子集上的 `41.52x/60.17x/152.26x` 直接写成整体平均。完整口径和论文缺口见 `release_v5_target_and_publication_gap.md`。

Test150 的 no-dual 归因表明，V5 在 `g=4` 的主要额外成本来自搜索前无条件构造全部 dual。Test151 达到总预算后一次性购买完整 dual，因重复支付前期弱搜索和完整构造而在高 `g` 通常回退 `20%--30%`。Test152 将同一 sequential dual 拆成 `g` 个合法前缀：每累计 `2m+n` 个 ordinary seed/pop/relax 工作单位就购买下一组，并从下一张 row 起立即使用。三库 `g=4..10`、每点 20 条的 420-query 面板全部保持权重；`g=5..10` 的 18 个单元均快于 Strict，`g=9..10` 保留 `11.97x--22.00x` 优势，`g=4` 仍慢 `12%--26%`。完整规则、证明和表格见 `test152_progressive_dual.md`。

Test107 的 q5 wall 受同机控制漂移影响，只作为当次观测，不单独宣称稳定端到端比例。q32 同时给出状态、扫描量和分阶段时间，可以作为当前中等门槛。新的 q25 长门保持答案 `12.0889822532`，相对最后一次 Test98 q25 将 completion checks 从 `32.815B` 降到 `46.019M`、completion 从 `1211.875s` 降到 `488.765s`，wall `8652.038s -> 8121.863s`，peak RSS 基本不变。该长门包含 Test103/104/107，不能当作 Test107 单变量对照；DBLP g13 q1 仍未重跑。

Test115 随后仅增加编译期层完成日志，用当前 Test107 完整跑通 DBLP g15 q1：答案 `16.1062710559`，wall `1953.202679s`，query peak `3246.973MiB`。这只证明 q1 低于 10,000 秒。Test116 按四档查询规模选择 g13 历史困难编号 `q2/q14/q25/q33` 做 D3 压力筛查，发现 q33 显著更重；其完整运行得到答案 `8.3053688653`、wall `11505.101235s`、query peak `19228.398MiB`。因此“DBLP g15 每条询问都低于 10,000 秒”已被实际反例否定。q1 与 q33 的完整分层数据分别见 `test115_dblp_g15_progress.md` 和 `test116_g15_variance_panel.md`。

Test119 用普通 D row 构造到永久锚路径的无块数设施上界，把 q33 原本 D4 后才出现的 `10.0181663554` 前移到 D2，并在 D3 得到 `9.9770319196`；相对只读轨迹，D3 values 降 `21.16%`、D4 values 降 `30.25%`。Test121 进一步在 junction 压缩锚树上组合 rooted D-block，使多个块共享锚树边；它在 D2 就得到 `9.9770319196`，让 q33 D3 values 相对 Test119 再降 `4.28%`、D3 时间降 `10.84%`。Test120 的层内反馈只额外减少 `0.20%` D3 values，已经撤回。Test122/123 扩充同一父树的 block 信息仍无收益；Test124 放宽为 junction 度量闭包后，q33 D2 values 只再降 `2.45%`，Toronto g13 五询问完整 wall 反而回退 `1.29%`，也已撤回。当前尚未运行 Test121 的 D4、A 或完整 q33，不能据此声称达到 8,000 秒；下一步不再叠加同类设施，而要寻找能直接改变 D4/A6 的锚感知状态接口。

Test142 已实现该状态接口：在平衡切分以下正向生成原 A 行，在切分以上从 `A+D+D` 终端沿完整 A 依赖图做 min-plus 伴随反传，并用 `anchor union mask` 的 directed-cut、group-tour 与 farthest 前缀下界剪除反向值。它不删除任何推导、不调用框架 B、不引入 Hash 或数据特判。生产随机门、Toronto 默认 160 条和三个性能面板均保持精确；DBLP g13 q5 的 ordinary D 时间基本不变，anchored 阶段 `30.806s -> 14.833s`。DBLP g15 q33 长门得到精确答案 `8.3053688653`，wall `9058.212s`、query peak `14342.977MiB`；相对 Test107 分别改善 `21.27%/25.41%`，但仍比 8,000 秒目标多 `1058.212s`。反向行只有 `6.774M` 个值，伴随阶段却因 `54.808B` 次 join checks 耗时 `2083.777s`，因此下一突破口已经从“高层 A 状态驻留”转为“高层终端与反向边的组合枚举”。完整定义、证明、复杂度和逐门数据见 `test142_adjoint_anchor_lattice.md`。

Test143 的 root-irreducible 高层终端虽然让 Toronto g15 checks 下降约 `23.5%`，底层扫描量和伴随时间却没有下降，现已只保留归档。Test144 的逐 H 行即时边界调度安全地减少约 `5%` 的 H values/pops，并作为 Test145 的辅助调度保留。Test145 进一步利用 directed-cut 势的组可加性，把每个目标重复执行的终端下界转置为固定根上的统一约化预算；全部 ordinary mask 在该根只收集一次，再按目标聚合最小终端。逐根精确比较全局有序路线与补集子掩码路线的探测数，使 pair work 最坏仍为 `O(3^k)`，没有经验阈值。独立三路探针 `500/500`、生产宽范围 `100/100` 和固定 g15 `30/30` 均精确；Toronto g15 q1--q5 wall `522.320s -> 387.645s`，伴随 `201.209s -> 49.747s`。DBLP g15 q33 最终得到精确答案 `8.3053688653`、wall `7594.956s`、query peak `14451.762MiB`，比 8,000 秒目标少 `405.044s`；相对 Test142，伴随阶段 `2083.777s -> 276.020s`，即使本次 ordinary D 慢 `5.80%`，完整 wall 仍改善 `16.15%`。完整恒等式、证明、复杂度和分解见 `test145_directed_cut_transposed_terminal.md`。

Test145 长门说明当前瓶颈已转回 ordinary D：它耗时 `6493.381s`，占 wall `85.50%`；转置构造为 `170.217s`，转置加全部 H 为 `276.020s`。随后对 Toronto、DBLP、DBpedia、LinkedMDB 与 MovieLens 的全部 200 条 g15 D2 询问进行统一剖析。Toronto 的 eager anchor-tree 与 MovieLens 的 eager dual 分别暴露出跨库负优化；DBLP/LinkedMDB 的最重询问则由 ordinary closure 主导。完整分布见 `g15_five_dataset_profile.md`。

Test146 只保留其中可普遍修复的 method-specific 问题：用 ordinary queue pops 与 relax attempts 给一次锚树 DP 的精确工作量摊销，达到成本才重新求上界。它不计无法被事后 incumbent 避免的 seed candidates，也不使用数据集、固定 `g`、层号、密度或时间阈值。Toronto 冻结五问全部自然跳过 tree，随机 DPBF `100/100` 精确；重询问仍按同一规则购买。Test147 的 lazy dual 在 MovieLens D3 从 `96.570s` 回退到 `178.800s`，已撤回。Test148 的 decrease-key heap 在 DBLP q20 D2 得到 `26.0%` 工程收益，但 baseline 同样可用且活动实现规范要求 `priority_queue`，因此也已撤回，只保留诊断快照。

Test149 不再延迟或关闭 dual，而是利用 sequential residual 的精确不变量：原始组距离在未改写弧上已经满足三角不等式，因此每个后继组只需从此前势函数可能降低过的有向弧启动残量 Dijkstra，传播阶段仍扫描完整邻接表。该变化保持全部势、下界与状态逐点相同，额外空间仅为 `2m` bits。MovieLens 冻结五问 D3 的 paired wall/dual 为 `94.772s/68.956s -> 86.254s/61.615s`；全 40 条 D2 的 dual 总时间为 `562.484s -> 533.664s`，35 条单问更低，扫描比例中位数 `6.24%`，D2 状态逐问一致。DBLP q11、DBpedia q34 与 LinkedMDB q40 的 dual 分别改善 `21.1%/20.6%/21.3%`，Toronto 完整五问保持中性偏正。完整证明与逐库证据见 `test149_changed_arc_dual.md`。

Test152 在 Test149 的逐组顺序和 changed-arc 闭包上增加渐进调度：初始不分配势表与 residual，每累计 `2m+n` 个 ordinary seed/pop/relax 工作单位就推进一个组，任意已完成前缀立即作为合法下界服务后续 row；只有全部 `g` 组完成后才恢复 primal 和允许 packing。Test151 的一次性延迟购买已删除，因为高 `g` 重复支付前期弱搜索和完整 dual；quarter upper 也在独立消融后从 Test80 物理删除。Test153--155 说明 junction、early upper、摊销 facilities/tree 和 packing 仍有重查询价值，不能只为缩短代码而移除。

下一步不再优化高层终端常数，也不包装通用队列替换。Test152 解决的是 small-`g` eager fixed cost，并未减少 DBLP q20 或 LinkedMDB q40 的 ordinary 极端状态；进入发行版前先在冻结的中大 `g` 多查询面板复核渐进调度，不因当前结果触发约五小时的 DBLP g13 全量长跑。

Test125 随后尝试用 `q=ceil(h/2)` 的 unrestricted endpoint P 替代高层 closed D，并让 A 只消费小 D。它修复了 Test96 的旧 `71/73` 反例且通过 `130` 次随机，但 Toronto g13 q3 得到错误权重 `0.8062097156 > 0.7985684033`；读取全部低 D values 仍不修复。反例说明 fixed-anchor 主干旁的大 attachment 不能由单 endpoint 状态无损表示，源码已撤回且没有运行 DBLP。下一理论候选必须显式或隐式保留第二接口/paid geometry，不能继续缩小单 endpoint block 上限。

## 2. 发行主线与当前 Test 差异

ReleaseV5 冻结 Test149 的 permanent-anchor 主线：

1. 计算组距离、初始可行上界、directed-cut 势及其已改写弧集合；所有后续策略只由状态结构和可证明工作量决定。
2. 按 mask 大小离线生成 ordinary `D(S,v)` 有序行，并在自然边界用合法设施上界和残余容量打包刷新 incumbent。
3. 正向生成达到平衡切分前的低层 anchored `A(S,v)`；高层不再物化完整 A 格。
4. 将 `A+D+D` completion 改写为按根共享的转置终端，再沿 A 依赖图反向传播伴随 `H` 行。
5. 低层 A 直接完成，高层由 H 的 cross-cut 完成；两条路径覆盖原 A 状态 DAG 的全部可行推导并返回精确最优值。

当前保留的研究增强如下：

| 机制 | 作用 | 是否改变状态语义 |
| --- | --- | --- |
| Test83 / Test87 | `directed-cut -> farthest -> group-tour` 按需下界短路 | 否 |
| Test84 | A 顶层按依赖流式消费并释放 | 否 |
| Test90 | 最高 D 行完成时立即结算 A0 completion | 否 |
| Test98 | sparse/dense/ranked-bitmap 按精确逻辑字节取最小 | 否 |
| Test103 | bitmap branch 半连接放入独立热路径内核 | 否 |
| Test104 | bitmap-compatible completion 使用 word intersection | 否 |
| Test107 | completion 的分块、共同根、第二分量三级最小值下界 | 否 |
| Test119 | 已有 D rows 到永久锚路径的规范化设施上界 | 否 |
| Test121 | rooted D-block 在 junction 压缩锚树上共享边的上界 | 否 |
| Test142 | 高层 A 依赖改由 min-plus 伴随反传，低层 A 保持原定义 | 否，等价重排完整 A DAG |
| Test145 | directed-cut 约化后按根一次生成全部高层终端 | 否，等价重排 `A+D+D` 终端枚举 |
| Test146 | 用已发生的可规避 ordinary 工作摊销锚树上界 | 否，只改变合法上界的求值时机 |
| Test149 | 只从已改写有向弧启动 sequential dual 的残量闭包 | 否，势函数与残量闭包逐点相同 |
| Test152 | ordinary 工作每支付 `2m+n` 就购买一个合法 dual 前缀 | 否，只改变下界可用的 row 边界 |

截至 Test149 的机制已经冻结进入 ReleaseV5。Test152 只存在于 Test 入口，ReleaseV5 仍使用 eager 完整 dual；ReleaseV4 不再回灌后续研究改动。

## 3. Test107 的有效结果

Test105--107 是同一条 completion 下界主线，不是三套算法。每张现有 D/A 行保存精确最小值，并在三个自然边界依次拒绝：

1. `min A + min D(L) + min D(R) > best`：跳过整个 partition。
2. `A(v) + min D(L) + min D(R) > best`：跳过当前共同根。
3. 已读取一个精确 D 值后再加另一行最小值：在两侧都非空时跳过第二次 D 查询。

| 门槛 | 结果 | 相对 Test104 的结构变化 |
| --- | --- | --- |
| fast20 | `6.466501s` | checks `-87.08%`；第三级比 Test106 慢，未宣称小查询普遍加速 |
| DBLP g13 q5 | `148.154170s`，答案 `14.5867185184` | scans `-53.08%`，checks `-98.40%`，completion `-70.43%` |
| Toronto g13 | 三次 wall 中位数 `6.324362s` | checks `-99.12%`，completion 中位数 `-58.12%` |
| DBLP g13 q32 | `425.965791s`，答案 `6.3393307977` | scans `-58.12%`，checks `-98.37%`，completion `24.60s -> 7.05s` |
| DBLP g13 q25 | `8121.863337s`，答案 `12.0889822532` | 相对 Test98 checks `-99.86%`，completion `-59.67%`，wall `-6.13%`，peak 基本不变 |

所有门槛的 ordinary/anchored values 与 pops 均和 Test104 相同。完整证明、浮点边界与逐层数据见 `test105_completion_partition_minimum.md`。

## 4. 当前瓶颈

Test107 已把已测查询的 completion 从主要问题降为次要阶段。q32 的 `425.410s` solver 内部分解为：

| 阶段 | 时间 |
| --- | ---: |
| group distance | `15.138s` |
| initial dual | `31.893s` |
| junction upper | `3.134s` |
| residual packing | `24.986s` |
| ordinary D | `281.465s` |
| anchored A | `93.677s` |
| completion | `7.054s` |

ordinary D 已是首要时间项。q32 的 D4 单层有 `1.242B` 次边松弛尝试；D3/D4 仍是跨询问统计中的主要持久空间层。高层 D 的 seed candidates 很多，但绝大多数只在共同根枚举或图传播之后才被 incumbent 下界拒绝。

q25 进一步确认这一转移：completion 虽仍占 `488.765s`，但普通 D 已占 `3939.679s`，A 扣除 completion 后占 `3635.479s`。三级证书几乎消除了最终精确 checks，却只能把 scans 降低 `12.27%`。g15 q33 的 ordinary/A/completion 又分别为 `7018.495s / 4419.560s / 288.872s`，completion 只占 solver total 的 `2.51%`；D4 单层产生 `781.846M` values 并耗时 `1944.255s`，A6 即使流式也耗时 `1730.497s`。这些结果说明下一步不应继续叠加常数个 completion 标量，而应减少 D/A 状态生成、图传播或大段有序扫描。

因此下一次突破必须至少做到一项：**减少 D/A 保留状态、减少图传播、或在不逐根读取 future 的前提下批量跳过有实际扫描重量的有序区间。** 只减少最终 checks、只调整 cut 与第二行读取顺序，或只重排同一批状态，已经没有主要杠杆。

## 5. 本轮否定边界

Test108--110 将“精确 driver + 另一行最小值 + directed-cut future”放在普通 D 成员相交前、相交后以及 `old-value -> cut` 两级短路中。fast20 拒绝率只有 `6.9%--9.5%`，状态完全不变，时间持平或回退；三版全部撤回，见 `archive/test108_ordinary_component_minimum_20260715.md`。

Test111 又把 permanent-anchor 几何压成每张 D/branch 行的 `min(D+dist_anchor)`，在共同根枚举前整块拒绝 split。Release/O2 随机 `100/100 + 50/50` 正确，但 fast20 虽拒绝 `21.85%` 的 split，实际 D join work 只下降 `0.99%`，ordinary `2139.984ms -> 2230.339ms`，状态不变。昂贵的 Toronto g12 仅命中 `0.86%`。源码、统计和临时快照均已撤回，未运行真实 DBLP，见 `archive/test111_anchor_attachment_row_certificate_20260715.md`。

Test112--113 又复核了 Test107 第三级证书的读序。跳过 singleton 证书只把比较换成直接数组读取；按单次访问成本交换左右 D 分量虽再少 `66,686` 次 checks，却使 fast20 completion `219.400ms -> 234.796ms`，原因是破坏跨 partition 的递增列表局部性。两版均撤回，未运行真实 DBLP，见 `archive/test112_113_completion_read_order_20260715.md`。

q25 完成后，Test114 为现有 bitmap root 的每个 64-bit word 保存精确 `min A`，试图在展开共同根前整字拒绝。方法通过 Release/O2 `100/100 + 50/50`，fast20 scans 稳定减少 `6.30%`，但三次 completion 中位数 `219.604ms` 与 Test107 的 `219.400ms` 无差别；q5/q32/Toronto 又均不命中该路径。为避免仅凭 q25 预测保留机制或再次长跑，源码已撤回，见 `archive/test114_bitmap_word_root_minimum_20260715.md`。

Test126 随后把 directed-cut future 精确消元为两张 reduced rows 与一个全局势，再为 ranked-bitmap 保存逐 64-bit word minimum。恒等式正确并通过 `100/100 + fixed g15 30/30`，但 Toronto g13 q5 的普通/A direct work只降 `0.70%/0.42%`，query peak 增 `9.26%`；五询问总 wall 回退 `2.42%`。三个独立 minimum 通常在不同根取得，仍没有保留同根相关性，源码与目标已撤回，见 `archive/test126_dual_cancelled_word_certificate_20260716.md`。

Test127 再把 D2 paid-pair skyline 恢复为真实路径并集，按永久锚树的最近 attachment site 分别保留二维前沿，并在见证边并集与同顶点诱导子图上精确求上界。Toronto g13 q1/q2/q4 命中 exact，但 q3 gap `0.319%`、q5 gap `10.933%`；q1 单条会给出错误的乐观判断。最近 site 与单 attachment distance 不是完备第二接口，实验入口已撤回且没有启动 DBLP，见 `archive/test127_paid_pair_witness_union_20260716.md`。

Test128 随后为每个 `<D2 组对,anchor-tree site>` 保留 `min_v D2(v)+dist(v,site)`，排除“只看最近 site”这一限制。Toronto g13 q3 的诱导上界反而为 `0.8525860548`，比 exact 高 `6.764%`，因为逐 block 的独立标量最优根不能表达与其他 paid block 共享边的次优根。首个固定反例即否决，故未运行 q5/DBLP；实验入口已撤回，见 `archive/test128_all_site_pair_envelope_20260716.md`。

Test129 先保留全部 `D_h`、只跳过最终 `A_{h-1}`。虽然宽随机 `100/100` 与固定 g15 `300/300` 通过，但确定性的 7/4/4 decoy-tree 得到 `DPBF=27`、`Test129=29`，完整 Test80 只有 A6 consumer 恢复 `27`。历史 q33 的 `best_after_a_s5/best_after_a_s6` 相同，是流式 producer/consumer 结束后共享最终 best，并不表示 A5 独立命中。Test129 已撤回，见 `archive/test129_penultimate_a_completion_20260716.md`。

Test130 又检查能否在 `h=floor(g/2)` 时同时停于 `D_{h-1}` 与 `A_{h-2}`，从结构上整层删除最高 D/A。Release/O2 宽随机在 `g=10` 得到 `32 -> 34`，固定 g15 在 seed `717512` 第 90 例得到 `50 -> 51`。因此 `A_{h-2}+D_{h-1}+D_{h-1}` 也不是完备端点；该分支已撤回且未运行 Toronto/DBLP，见 `archive/test130_penultimate_da_completion_20260716.md`。

Test131 不删除状态，而在每个最终 A consumer 前用 producer 侧 `min A+min D` 与 completion 侧 `min D+min D` 构造整 mask 安全下界。随机 `200/200 + fixed g15 200/200` 正确，但 fast20 只拒绝 `147/4270=3.44%`，Toronto 两版全部零命中；收益集中在本来极轻的 DBLP g9 与 MovieLens g9--g11。独立行最小值仍丢失共同根相关性，分支已撤回且未运行大图，见 `archive/test131_final_consumer_mask_lower_20260716.md`。

Test132 利用无向最短路的 min-plus 对称性，把最终 consumer 从 `closure(F)+G` 精确转置为 `F+closure(G)`，并用现有规范 pivot/branch basis 生成补集侧临时种子。随机 `200/200 + fixed g15 100/100` 正确，但 fast20 合计回退 `0.82%`；同机 Toronto g13 q1--q5 只有 q4 改善，合计 `45.659s -> 47.192s`，回退 `3.36%`。DBLP q5 的转置最终段虽约 `6.510s -> 5.078s`，wall 仍回退且前置阶段存在计时漂移，不能作为整体正证据。恒等式保留在归档，实验入口已撤回，见 `archive/test132_transposed_final_consumer_20260716.md`。

Test133--136 随后构造 exact rooted-row 度量锥 `max_u(D(B,u)-dist(u,t))-dist(t,v)`，并分别使用两个端点、全部压缩锚树 hub、锚路径单标量以及全层子集聚合。该下界对行外根也合法，保留了同一行内的根相关性；但 Test133 fast20/Toronto 五询问 wall 分别回退 `2.65%/2.04%`，全部 hub 版回退 `11.73%`，全层路径版只减少 `0.03%` 的普通 values 而回退 `3.62%`。代码已撤回，理论式保留，见 `archive/test133_136_exact_row_cones_20260716.md`。

Test137 提前生成正式 `A({i},v)`，以 `max_{i notin S} A({i},v)` 替代普通 D 的 farthest 下界。该式 exact、anchor-aware 且一致，fast20 D values/pops 下降 `47.28%/48.17%`，Toronto g13 q1--q5 总 wall 改善 `17.17%`；但 DBLP g15 q2/q14 的 D2 状态完全不变，q33 D3 values 只降 `2.94%`，probe wall/peak 反而增 `3.25%/3.06%`。A1 被 directed-cut 大部覆盖，机制已撤回且没有进入 D4/full，见 `archive/test137_early_anchor_pair_lower_20260716.md`。

Test138 把原 quarter witness 的 `1+3` 汇合扩为 `2+2` 双汇合点：锚与两个 D-block 在一侧汇合，另外两个 D-block 在另一侧汇合，再用原图最短路连接。候选上界合法且通过宽随机 `300/300`、固定 g15 `100/100`；但 fast20 与 Toronto g13 q1--q5 的 D/A values 都逐项不变，Toronto 的 18 个非空方向零更新，合计 wall 回退 `2.79%`。该拓扑模板已撤回且没有启动 DBLP，见 `archive/test138_quarter_two_root_upper_20260716.md`。

Test139 随后把 Test126 的三个独立 minimum 改成角色定向联合界：bitmap accumulator 保存 `min(D-pi_covered)`，branch roots 保存 `min(D+pi_remaining)`，二者在每个现有 64-bit word 上组合。该界严格支配 Test126，并按首次真实 consumer 惰性物化；随机 `100+50+100+30` 全部正确。fast20 ordinary/A direct work 降 `12.98%/3.95%`，但 Toronto g13 q1--q5 合计 direct work 只降 `3.64%/2.12%`，wall `47.422s -> 48.114s`、q5 peak `146.441MiB -> 157.047MiB`。D3 截止又完全不命中，故没有运行 DBLP，源码已撤回，见 `archive/test139_role_directed_joint_word_certificate_20260716.md`。

Test140 把 Test121 选中的 singleton/D2 block、压缩锚树路径和 anchor path 全部恢复为原图 edge ids，再按真实边并集去重计费；同一方案的并集值可证明不大于原加法上界。进一步的无参数边际并集选择按“新增未付边代价/新覆盖组数”直接优化共享。随机 `100/100 + fixed g15 30/30` 正确，但 Toronto g13 q1--q5 只有 q1 的 D3 values/pops 降 `7.48%/7.88%`，其余四条状态不变；边际选择五条均弱于原加法方案，完整 wall 合计 `46.454s -> 52.862s`。未运行 DBLP，源码与目标已撤回，见 `archive/test140_anchor_tree_witness_union_20260716.md`。

Test140 撤回后，从 paid-profile 定义得到一条不依赖数据的正向结论：对固定 `(core S,B,C)` consumer，`min_T w(T)+phi_T(B)+phi_T(C)` 等于把 `S` 的单组 rows、`D(B,*)` 与 `D(C,*)` 当作 macro labels 的标准 subset DP。它联合选择两个 attachment，消除了显式 `n^2` 接口；现有小图探针已直接对拍显式 connected-subgraph 值，singleton/双候选组分别 `500/500`、`200/200`。逐一处理全部 consumers 的 join 总工作为 `5^k`；完整定理见 `history/two_attachment_consumer_scalarization.md`。

Test141 进一步只请求 Test121 最优设施方案恢复出的一个 block partition，把这些 blocks 与 permanent anchor 提升为至多 `ceil((g-1)/2)+1` 个 macro labels。该全图 DP 合法且理论上不弱于同一设施方案，多层调度仍为 `3^(g/2)` 型；随机 `100/100 + fixed g15 30/30` 正确。但 fast20 ordinary/A values 仅降 `3.72%/0.23%`，正式 pops 不变，macro 自身产生 `4.44M` pops，wall `8.090s -> 12.367s`。实现、目标和快照已撤回，未运行 Toronto g13 或 DBLP；见 `archive/test141_facility_guided_macro_lifting_20260716.md`。

这组结果说明：**单个全行或独立分块最小值无法保留 accumulator、branch 与 anchor 在同一根上的相关性；即使在单侧保留势与根的相关性并整字跳过数百万候选，固定机器 word 粒度仍只减少同一状态空间中的逐根调用，不能改变 D2--D4 或 A 高层主项；exact“anchor + 一个剩余组”状态、固定数量的 block 汇合模板以及标量方案的事后边去重，也没有表达 DBLP 高层所需的多组共享。** 下一批量接口若继续使用离线有序列表，必须让联合证书跨更多 row pairs 共享并一次跳过渐近上更大的有序区间，或显式保留 paid subtree 的多个 attachment profile 并直接消除状态维度；不能继续叠加常数个独立标量、hub 比较、固定拓扑模板、完整提前 A-lattice、word 粒度调参或独立 block/root 选择。

## 6. 后续研究门槛

1. 保持 permanent-anchor A 主干、root-irreducible D branch 和离线有序行的大框架，不回到框架 B。
2. 优先给出状态完备性、批量下界或受控 profile 的理论必要条件，再写探针；Test145 已证明跨目标共享的约化终端可以保持 `O(3^k)`，下一步只接受能继续共享高层反向转移或减少 ordinary D 状态/传播的结构机制，不做逐 consumer 的 `5^k` 实现。
3. 不使用数据集名、固定 `g`、固定层、状态密度、wall time 或完成进度特判。
4. 不把 baseline 同样可做的普通图/query 压缩计作本方法贡献。
5. 使用 Release/O2；先过 DPBF `1e-6` 随机门禁，再跑固定 fast20 和 Toronto g13 q1--q5 配对面板。不能因某条结果好而临时更换询问集合。
6. q1 只作 smoke 和历史兼容性对照，不作 `<dataset,g>` 整体性能证据。通过短面板后，一般候选最多使用 DBLP g13 q5/q32 做各自的逐询问配对门；这两条也不能外推为数据集整体。压力样本只用于找反例，不用于估计总体分布。只有新的状态级突破或发行候选才运行 q25 或 g15 q33。Test152 当前只是预处理调度候选，不触发全 DBLP g13。g13/g15 若需总体统计，应完整运行 40 条，或在看到候选结果前冻结按四个查询规模档等额抽取的 query ID；同一组 ID 必须逐询问配对运行基线与候选，并至少报告总时间、中位数、P90、标准差或变异系数，以及逐询问加速比分布。
7. 运行结束后清理随机目录、失败快照、空结果和残留进程。

## 7. 文档入口与论文边界

- `release_v5_method_cn.md`：当前发行算法的方法章节式说明。
- `release_v5.md`：ReleaseV5 的独立实现、正确性、复杂度与验证证据。
- `release_v5_target_and_publication_gap.md`：相对 PrunedDP++ 的 `g=4..15` 曲线、删失边界、GPU4GST 定位和投稿必需工作。
- `gpu4gst_datasets.md`：GPU4GST 八数据集转换、作者查询与扩展查询协议。
- `pruneddp_reproduction.md`：PrunedDP++ 原文、三个复现开关及正确性边界。
- `release_v4_method_cn.md`：较早纯 A 冻结版的方法说明。
- `test80_anchor_progressive.md`：Test80 的完整算法来源、证明与实现细节。
- `test80_dblp_g13_cross_query.md`：19 条 DBLP g13 查询的瓶颈统计。
- `test105_completion_partition_minimum.md`：Test105--107 的统一方法和证据。
- `test115_dblp_g15_progress.md`：DBLP g15 q1 的完整分层单例结果。
- `test142_adjoint_anchor_lattice.md`：高层 A 伴随反向框架、证明与 q33 基线。
- `test145_directed_cut_transposed_terminal.md`：跨目标终端转置、复杂度与当前门禁。
- `test149_changed_arc_dual.md`：已改写弧 residual closure 的不变量、证明与五库证据。
- `test152_progressive_dual.md`：按 ordinary 工作量逐组购买 dual 的主线、证明、small-`g` 曲线与简化审计。
- `test116_g15_variance_panel.md`：g15 跨询问压力筛查与 q33 完整反例。
- `test119_anchor_path_facility_upper.md`：普通 D rows 到永久锚路径的设施上界。
- `test121_anchor_tree_block_facility.md`：rooted D-block 在压缩锚树上共享边的结构上界。
- `history/two_attachment_consumer_scalarization.md`：固定 consumer 的双 attachment 消维定理与全量复杂度门槛。
- `test_series_overview.md`：全部 Test 的时间线与保留/撤回状态。
- `archive/README.md`：失败实验索引。
- `history/README.md`：仍有效的理论结论索引。

当前文档只把 permanent-anchor 状态组织、branch basis、工作量购买、junction facility、progressive packing、伴随 H 与转置终端称为仓库方法或候选组合，不直接宣称任一组合已经确认论文原创性。现有检索已覆盖 Dreyfus--Wagner、Dijkstra-Steiner、DS*、Wong dual ascent、PrunedDP++ 和 GPU4GST 主线；投稿前仍需逐定理排重。正式引用统一见 `release_v5.md`，论文就绪度判断见 `release_v5_target_and_publication_gap.md`。
