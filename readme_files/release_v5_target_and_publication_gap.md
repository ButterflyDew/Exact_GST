# ReleaseV5 的扩展曲线、目标检验与论文缺口

更新时间：2026-07-18。本文只回答两个问题：ReleaseV5 是否达到“低 `g` 与 PrunedDP++ 持平，高 `g` 逐渐形成 10--100 倍时间和空间优势”的目标；现有方法和证据是否已经足以形成 VLDB/SIGMOD 论文。算法定义见 `release_v5_method_cn.md`，数据生成见 `gpu4gst_datasets.md`，PrunedDP++ 的复现歧义见 `pruneddp_reproduction.md`。

> **历史文档边界（2026-07-18）：** 后续 Test152 已用按 ordinary 工作量逐组购买 dual 的方式改善 small-`g` 固定成本，并经 Test157 审计后冻结为 ReleaseV6。本文所有表格和当时的投稿判断仍严格对应 ReleaseV5，不把 Test152/V6 数字回填为 V5，也不覆盖当前 V6 的方法评审结论；当前入口见 `release_v6_review_gate.md`，候选演化见 `test152_progressive_dual.md`。

## 1. 结论

**就 ReleaseV5 当时的目标而言，只部分达到。** 在本文三数据集相关查询面板上，ReleaseV5 在 `g=4,5` 分别约慢 `2.06x` 和 `1.61x`，只能称为同一数量级，不能称为逐项持平。优势随后随 `g` 增长：聚合求解时间与逐查询几何平均都在 `g=9` 首次超过 `10x`；`g=10..12` 的聚合时间、几何平均和绝对 query peak RSS 均稳定超过 `10x`。因此“从 `g=8` 起全部达到 10--100 倍”不成立，较准确的表述是：**`g=8` 已有约 3--5 倍优势，`g=9..12` 形成一到两个数量级的总体优势。**

`g=13..15` 时 ReleaseV5 完成全部选定查询，而 PrunedDP++ 分别只完成 `5/9`、`4/6`、`2/6`。这证明了明显的可解规模优势，但 completed-pair speedup 只来自基线仍能完成的较容易子集，不能当成全部查询的平均值。高 `g` 还存在单例反例，例如 Twitch `g13 q2` 只有 `2.19x`、Twitch `g14 q2` 只有 `5.33x`、Musae `g14 q1` 只有 `9.50x`；所以“一到两个数量级”只能作为聚合趋势，不能写成每条实例保证。

**现阶段还不够直接投稿 VLDB/SIGMOD。** 2025 年 GPU4GST 已公开 GPU 精确 GST 算法、8 个数据集和代码，并报告相对 PrunedDP++ 的 `48--2390x` 加速。ReleaseV5 仅证明“比 PrunedDP++ 快”不足以建立论文新颖性。可行的论文方向是把永久锚点状态分解、伴随高层求值和转置终端形式化为一个不同于 PrunedDP++、Dijkstra-Steiner/DS* 与 GPU4GST concurrent DP 的**新 CPU 精确算法**，再完成同机 GPU4GST 对照、全数据集分布实验、机制消融和可复现 artifact。

## 2. 实验问题与口径

### 2.1 被检验的目标

本文把原目标拆成三个可判定命题：

1. **低 `g`：** `g<=5` 时 ReleaseV5 与 PrunedDP++ 的求解时间和 query peak RSS 处于同一数量级，且没有高昂固定预处理导致的数量级回退。
2. **增长趋势：** 随 `g` 增大，ReleaseV5 相对优势应总体上升，而不是只在一个查询或一个数据集上偶然出现。
3. **高 `g`：** `g=8..15` 的代表性面板逐渐达到 `10--100x` 时间与空间优势；若基线超时，则同时报告完成率和 cutoff，不把 cutoff 伪造为完成时间。

“快 `10x`”可以指聚合总时间、逐查询中位数或几何平均，三者不是同一事实。本文同时给出聚合比与几何平均；空间主口径是每条查询求解期间的**绝对 query peak RSS**，与 `weights.txt` 第三列一致。

### 2.2 方法与基线

- `ReleaseV5`：当前纯框架 A 发行实现，不调用 V3/V4，不含数据集、`g`、密度或运行时特判。
- `PrunedDPStrict`：Hash 稀疏状态、逐状态 MST 上界、Algorithm 4 的 `lb_2` pathmax，作为最接近原文的默认 PrunedDP++ 复现。
- `PrunedDPReference`：关闭有正确性疑问的 pathmax 并允许状态 reopen，用于判断严格复现细节是否人为放大差距。
- `DPBF`：在可承受的低 `g` 面板上作为独立正确性 oracle。

严格版与 reference 版在已运行的 `g=4..11` 面板上给出相同答案和相同趋势，因此当前速度差异不是由 `lb_2` 争议开关单独制造的。三个 PrunedDP++ 开关及理论边界见 `pruneddp_reproduction.md`。

### 2.3 数据与查询

扩展曲线使用 GPU4GST 的完整 `Musae`、`Twitch` 和 `Github` 图，以及生成后冻结的相关组查询前缀。查询遵循共现图 BFS 协议，没有裁剪图、压缩查询或根据结果重选实例。面板规模随成本分阶段缩小：`g=4..10` 每库前 20 条，`g=11` 每库前 5 条，`g=12,13` 每库前 3 条，`g=14,15` 每库前 2 条。

这是一张**探索性扩展曲线**，不是论文最终统计。不同 `g` 的样本数不同；只覆盖 8 个新数据集中的 3 个；高 `g` 使用固定前缀而不是完整 300 条；它不能估计全部查询分布，也不能消除相关组重叠导致的零解和易例偏差。

### 2.4 运行与删失

`g=4..12` 使用 batch 模式，同一 `(dataset,g,method)` 只加载一次图；正式时间仍取逐查询 `weights.txt`。`g=13..15` 使用 instance 模式，每个查询独立进程并设置 1000 秒 cutoff，使 timeout 明确属于单条查询。instance 进程 wall 包含重复图加载，只用于执行控制；表中完成实例的时间来自 solver time。

只有双方都完成时才能计算精确 speedup。若基线超时，本文记录 completion rate；不会把 1000 秒当成它的真实求解时间，也不会把完成子集上的 speedup 外推到超时子集。

## 3. `g=4..15` 结果

| `g` | 查询数 | V5 / Strict 完成 | Strict 超时 | 完成对聚合加速 | 完成对几何平均加速 | 绝对 peak RSS 比 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 | 60 | 60 / 60 | 0 | `0.49x` | `0.47x` | `0.86x` |
| 5 | 60 | 60 / 60 | 0 | `0.62x` | `0.57x` | `1.00x` |
| 6 | 60 | 60 / 60 | 0 | `1.36x` | `0.89x` | `1.66x` |
| 7 | 60 | 60 / 60 | 0 | `3.06x` | `1.82x` | `2.62x` |
| 8 | 60 | 60 / 60 | 0 | `4.77x` | `2.83x` | `4.60x` |
| 9 | 60 | 60 / 60 | 0 | `16.86x` | `10.04x` | `12.17x` |
| 10 | 60 | 60 / 60 | 0 | `22.12x` | `13.84x` | `18.62x` |
| 11 | 15 | 15 / 15 | 0 | `32.50x` | `12.32x` | `37.49x` |
| 12 | 9 | 9 / 9 | 0 | `37.81x` | `19.70x` | `27.96x` |
| 13 | 9 | 9 / 5 | 4 | `41.52x` | `25.80x` | `17.32x` |
| 14 | 6 | 6 / 4 | 2 | `60.17x` | `35.31x` | `42.13x` |
| 15 | 6 | 6 / 2 | 4 | `152.26x` | `194.85x` | `36.20x` |

所有比值均为 `PrunedDPStrict / ReleaseV5`，大于 1 表示 ReleaseV5 更优。绝对 peak RSS 比取完成配对中的最大查询峰值之比，不是各查询比例的平均。`g=13..15` 三行只统计双方完成的 `5/4/2` 对，必须与完成数一起阅读；尤其 `g=15` 的 `194.85x` 几何平均只来自 Musae q1 和 Twitch q1，不能表述为六条查询的总体均值。

原始汇总位于 `result_snapshot/release_v5_vs_pruneddp/trend_g4_g15.csv`。各阶段目录为：

- `result_snapshot/release_v5_vs_pruneddp/runs/20260717_gpu4gst_small_g4_g8_q20/`
- `result_snapshot/release_v5_vs_pruneddp/runs/20260717_gpu4gst_small_g9_g10_q20/`
- `result_snapshot/release_v5_vs_pruneddp/runs/20260717_gpu4gst_small_g11_q5/`
- `result_snapshot/release_v5_vs_pruneddp/runs/20260717_gpu4gst_small_g12_q3/`
- `result_snapshot/release_v5_vs_pruneddp/runs/20260718_gpu4gst_small_g13_g15_q3_instance/`

## 4. 如何解释这条曲线

### 4.1 低 `g`

ReleaseV5 在 `g=4,5` 没有数量级回退，空间也与基线接近，但时间不是数值上的持平。永久锚点、dual、锚树和多种有序行准备在状态数很小时尚未被后续消维收益摊薄，因此 `g=4` 慢约一倍、`g=5` 慢约六成。论文可以诚实写成“低 `g` 同一数量级”，不能写“低 `g` 不慢于基线”。若要主张真正持平，还需降低 method-specific 固定成本；不能通过按 `g` 切换回 PrunedDP++ 达成。

### 4.2 `g=8` 与拐点

`g=8` 的聚合时间为 `4.77x`、几何平均为 `2.83x`、空间为 `4.60x`，已经出现清晰优势，但没有达到一个数量级。`g=9` 三项分别达到 `16.86x`、`10.04x`、`12.17x`，是当前面板上第一个同时跨过 10 倍的点。因此经验拐点是 `g=9`，不是 `g=8`。

### 4.3 `g=10..12`

这一区间没有 timeout，且时间和空间三种总体口径都超过 10 倍，是当前最干净的正证据。`g=11` 虽只有每库 5 条，Twitch 的逐查询几何平均只有 `3.71x`，说明跨库和跨查询方差仍大；论文不能只报三库合计。

### 4.4 `g=13..15`

高 `g` 的主要证据已从“完成对加速”转成“同一 cutoff 下的完成能力”。ReleaseV5 的完成率为 `100%`，PrunedDP++ 从 `55.6%` 降到 `33.3%`。完成对中仍有 Twitch `g13 q2=2.19x`、Twitch `g14 q2=5.33x`、Musae `g14 q1=9.50x`；Musae `g15 q1` 的空间比也只有 `7.42x`。这直接否定每条实例都达到 10 倍的强表述。

Github `g15 q1` 中 PrunedDP++ 在 1000 秒超时，而 ReleaseV5 的进程 wall 约 149 秒。由于 cutoff 只给出基线真实时间的下界，这一条目前只能证明约 6--7 倍的进程 wall 下界，不能证明 10 倍；延长 cutoff 或采用生存分析后才能给出更强结论。

## 5. 正确性证据

- `g=4..8` 的 300 个配对点均与 PrunedDPStrict、PrunedDPReference 和 DPBF 一致。
- `g=9..11` 的完成结果均与两种 PrunedDP++ 复现一致。
- `g=12..15` 所有双方完成的配对答案一致；高 `g` 未完成项没有被当作答案一致证据。
- ReleaseV5 另有随机图 DPBF `300/300` 和原五数据集冻结回归，见 `release_v5.md`。

这些结果支持实现正确性，但不能替代论文中的形式化状态不变量、完备性定理和复杂度证明。

## 6. 相关工作定位

### 6.1 精确 GST 主线

[PrunedDP++](https://doi.org/10.1145/2882903.2915217) 是当前 CPU GST 基线，核心仍是 rooted subset DP、状态扩展、future-cost 剪枝和可行上界。其源码未公开导致稀疏状态、逐状态 MST 和 `lb_2` 一致性三处复现不确定性；本仓库已把它们显式化为开关，不能把某个争议实现细节当作 ReleaseV5 的贡献。

[GPU4GST](https://doi.org/10.1145/3769792) 在 PACMMOD/SIGMOD 2025 提出 concurrent DP、GPU 工作量平衡和修改后的剪枝，并报告相对既有方法 `48--2390x` 加速。作者已经公开[代码与八个数据集](https://github.com/toziki/GPU4GST-sigmod)，实验使用每个设置 300 条查询、1000 秒单例 cutoff，并主要考察 `g=3,5,7`。因此它既是必须直接比较的现有最强系统，也是本项目不能回避的最接近工作。

### 6.2 邻近精确 Steiner 工作

[Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492) 把 future cost 引入 goal-oriented Dijkstra-Steiner；[DS*](https://arxiv.org/abs/2011.04593) 进一步允许任意 admissible lower bound，并提供开源求解器。它们求解普通 Steiner tree 而不是 group Steiner tree，但与 ReleaseV5 的状态搜索、future lower bound 和 reopen/一致性问题高度相关。论文必须说明永久锚点分解和伴随高层求值改变了什么状态组织，而不能只把“使用更强下界”当作新颖性。

[Iwata--Shigemura 2019](https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843) 已用平衡树分隔证明最优 Steiner tree 可由三个各含至多一半终端的 rooted Steiner trees 合并，并据此只处理半层子集；其实验还包含由 Group Steiner reduction 得到的实例。因此 ReleaseV5 的“三分块、平衡根、普通状态只到半层”与这项 meet-in-the-middle 技术直接重合，不能再列为候选原创点。永久组锚定、伴随 `H` 和转置终端必须以该结果为起点说明新增状态变换。

[Ge et al. 2023](https://doi.org/10.1155/2023/1974161) 在 temporal GST 中也使用 rooted dynamic programming、A* 与 grow/merge/path-grow。它求解的是带时间约束的不同问题，没有给出当前伴随／终端转置构造，但进一步说明“rooted GST DP”本身不是新颖性来源。

经典 rooted subset recurrence 来自 [Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302)，directed-cut dual-ascent 路线来自 [Wong](https://doi.org/10.1007/BF02612335)。这些基础技术、有序列表、位图和 Dijkstra 均不属于候选贡献。

### 6.3 当前可主张的候选差异

在排除已知 half-state meet-in-the-middle 后，ReleaseV5 最有希望形成论文主线的是以下整体状态变换：

1. 在已知半状态 MITM 上，用永久**组**锚点把普通分支 `D` 与锚定状态 `A` 分离，并只发布 root-irreducible ordinary branches；这更接近必要铺垫或 GST 适配，不能单独承担整篇论文的新颖性。
2. 只正向物化低层 `A`，把高层锚定状态 DAG 的求值转成伴随 `H` 反传，以第一条跨切分边结算。
3. 利用组势可加性，把 `A+D+D` 高层终端从逐目标枚举转成按根共享的转置生成，同时保持 `O(3^k)` mask 工作边界。

这些机制在当前仓库中有实现和证明草图，但尚未完成逐定理的论文级形式化，也尚未证明文献中没有等价构造。因而它们只能称为**候选原创组合**。

## 7. VLDB/SIGMOD 就绪度

SIGMOD 2027 明确要求 substantial novelty，并期望公开代码、数据、脚本和 notebook；[正式 CFP](https://2027.sigmod.org/calls_papers_sigmod_research.shtml) 也把 reproducibility 纳入评价。VLDB 2027 的 [Research Track CFP](https://vldb.org/2027/call-for-research-track.html) 要求算法论文有形式化基础或新算法，系统论文有原则化设计、可运行原型和端到端实证；其[提交指南](https://vldb.org/2027/submission-guidelines.html) 要求提交补充 artifact，EA&B 论文更必须公开全部数据和软件并参加复现评审。

按这些标准，当前状态是：

| 方面 | 当前状态 | 判断 |
| --- | --- | --- |
| 新算法主线 | 有 permanent-anchor、adjoint H、transposed terminal 候选组合 | 有潜力，但相关工作排重和形式化不足 |
| 正确性 | 有证明草图、DPBF 随机对拍和多库一致性 | 实现证据较强，论文定理链不足 |
| 相对 PrunedDP++ | `g=9..12` 有稳定数量级优势，高 `g` 完成率显著更高 | 支持价值，但不是对 GPU SOTA 的充分比较 |
| 相对 GPU4GST | 已接入其数据与查询，尚未同机运行其公开实现 | 关键空白 |
| 工作负载覆盖 | 3 个小图、分阶段固定前缀、`g=4..15` | 不足以支撑总体结论 |
| 消融与解释 | 历史 Test 很丰富，但没有统一发行版消融矩阵 | 不足 |
| artifact | 有生成器、runner、结果快照和发行源码 | 尚未形成 Linux 一键复现包 |

**结论：可以继续发展成论文，但现在提交风险很高。** 最自然的是 Regular Research / Foundations and Algorithms 路线，而不是仅凭性能结果投稿。EA&B 也不是捷径：它要求对现有方法强弱、工作负载和复现问题给出全新的系统性洞察，并公开可复用 benchmark artifact。

## 8. 投稿前必须完成的工作

### P0：决定论文是否成立

1. **形式化核心新颖性。** 给出精确状态域、重叠组平衡分解、递推算子、低/高层切分、伴随状态不变量、正反向路径双射、终端转置恒等式和完整复杂度定理；逐项对照 PrunedDP++、GPU4GST CDP、Dijkstra-Steiner、DS* 与 Iwata--Shigemura half-state MITM。当前硬性评审清单见 `release_v6_review_gate.md`。
2. **直接比较 GPU4GST。** 在同一台 Linux 机器上运行作者公开代码和 ReleaseV5，至少复现作者 `g=3,5,7` 的 8 数据集、每点 300 条查询和 1000 秒单例 cutoff。CPU 与 GPU 时间必须包含清楚的预处理/传输边界，空间分别报告 host peak RSS 与 device high-water，不能把 CPU RSS 直接和显存混为一个数字。
3. **完成分布实验。** 在 8 个数据集上运行冻结查询；完整报告 completion rate、总时间、中位数、几何平均、P90/P95、逐实例 speedup/cactus plot 和 query peak RSS。含 timeout 的点使用删失或性能曲线，不填充伪造时间。
4. **证明不是查询重叠偶然收益。** 报告组大小、组间 Jaccard/共现、零解比例、最优值和状态工作量与耗时的关系；增加预先定义的低重叠/受控重叠查询面板，但不改变图或把普通压缩包装成本方法贡献。

### P1：形成可信实验故事

1. 建立统一消融版：ordinary D 基础、永久锚点 A、adjoint H、transposed terminal、dual、anchor-tree upper、布局选择逐项加入；每项都报告时间、peak、states、merge/check/relax work。
2. 解释并尽量降低 `g=4,5` 的固定开销，但不能按 `g` 切换到 PrunedDP++；若理论上不可避免，就把“同一数量级”写成明确 trade-off。
3. 在同硬件、同编译器 `-O3`、固定 CPU affinity 和可复现环境下重复运行，报告方差；图加载时间、查询预处理、solver time 和批量 amortization 分开。
4. 完成 PrunedDP++ `hash/dense`、`MST on/off`、`lb_2 pathmax on/off` 的敏感性矩阵，说明主结论不依赖某个复现选择。
5. 对 ReleaseV5 的最坏复杂度与实际驻留状态给出可核对计数，解释为何 `g` 增大后曲线出现拐点，以及哪些数据集不满足 10 倍。

### P2：达到可审阅与可复现质量

1. 把约 2780 行发行实现按稳定数学组件拆分，保持单一路径和无研究开关，同时提供对应伪代码和源码索引。
2. 提供 Linux 构建、依赖锁定、查询生成、正确性检查、批量运行、timeout 恢复、汇总和绘图的一键脚本；保留原始 CSV 和环境元数据。
3. 为公开数据的来源、许可证、原文查询与扩展查询分别建立 manifest；发布时避免提交不允许再分发的原始数据，只提供下载与校验脚本。
4. 增加持续随机对拍、固定回归和 sanitizer 测试，确保后续论文重构不破坏精确性。

## 9. 下一轮实验协议

下一轮不应继续扩大当前 3 库前缀并把它当论文实验。优先顺序应是：

1. 在可用的 A6000 或同级 GPU 环境复现 GPU4GST 作者脚本，先用 Twitch 的作者 `g=3,5,7` 查询核对输出和计时定义。
2. 固定 8 库作者查询和扩展查询 manifest；先跑 ReleaseV5、PrunedDPStrict、PrunedDPReference 与 GPU4GST 的 `g=3,5,7` 全 300 条。
3. 对 `g=4..16` 使用单实例 1000 秒 cutoff；从低 `g` 向上推进，超时后仍保留逐实例状态，不因结果更换查询。
4. 同时收集答案、solver time、端到端 wall、query peak RSS、GPU device peak、状态数和关键工作计数；所有图表从机器可读 CSV 生成。
5. 完成 P0 的形式化与直接 GPU 对照后，再决定投稿故事是“新 CPU 精确算法”还是“精确 GST 的跨 CPU/GPU benchmark 与工作负载研究”。

本文当时的曲线已经证明 ReleaseV5 值得继续研究，也清楚划出了不能写进摘要的强表述。后续工作的价值不在于再挑几个更容易的高 `g` 查询，而在于把状态变换证明成完整的 exact GST 方法，并在 GPU4GST 的公开基准上经受同机、全分布比较；前者现已由 ReleaseV6 方法冻结完成，后者仍是投稿实验缺口。
