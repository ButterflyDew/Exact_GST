# Test152：由普通搜索工作量购买的渐进式 Sequential Dual

更新时间：2026-07-18。Test152 是 ReleaseV5/Test149 之后的渐进 dual 研究来源，目标是消除小 `g` 上过重的 dual 固定成本，同时保留大 `g` 上的剪枝能力。该路线现已冻结进入 ReleaseV6；Test152 继续保留统计与消融，不回灌 ReleaseV5，也没有按数据集、固定 `g`、层号、状态密度、wall time 或查询编号切换策略。

本文只详细说明 Test152 新增的 dual 调度以及它与现有框架的接口。完整的 permanent-anchor `D/A/H` 状态定义、转置终端和完备性证明见 `release_v5_method_cn.md`；changed-arc residual closure 的不变量见 `test149_changed_arc_dual.md`。

## 1. 问题与设计结论

ReleaseV5 在搜索前一次性构造全部 `g` 个组的 sequential dual。该下界对大状态空间很有价值，但其构造成本近似随 `g(m+n)` 增长，且无论后续搜索多轻都会支付。三张 GPU4GST 小图的分阶段统计显示，`g=4` 的 dual 已占 V5 求解时间约 `40%--54%`；关闭 dual 后，`g=4` 只比 Strict PrunedDP++ 慢约 `11%--25%`，`g=5` 起在已测低 `g` 面板上不再慢于 Strict。因此，小 `g` 的主要问题不是框架 A 的状态本身，而是 **搜索开始前无条件购买完整 dual**。

直接延迟整个 dual 仍不合适。Test151 先不使用 dual，等普通搜索累计工作达到 `g(2m+n)` 后再一次性构造全部 `g` 层。它修复了 `g=4` 的固定成本，却在较大 `g` 上通常比 V5 慢 `20%--30%`：已经支付的无 dual 搜索无法收回，随后又支付完整 dual，形成明显的重复成本。

Test152 的结论是：**dual 不能只有“完全没有”和“全部构造”两个状态；应当把它拆成 `g` 个本来就顺序执行的合法前缀，并让普通搜索逐层购买。** 每个已完成前缀立即服务后续状态，而不是等待整个 dual 完成。

## 2. 主线流程

对一条含 `g` 个组的查询，Test152 按以下顺序执行。

1. 计算各组到所有顶点的最短距离，选择 permanent anchor，并构造与 ReleaseV5 相同的初始可行上界、junction 结构和 ordinary row 基础设施。此时不分配 dual 的 `g x n` 势表和 `2m` residual。
2. 从普通 `D(S,v)` 行开始求解。每完成一行，累计本行实际发生的 seed candidates、priority-queue pops 和 edge-relax attempts。
3. 每累计 `2m+n` 个普通工作单位，就购买 sequential dual 的下一个组。若余额足够购买多组，则连续推进多组；未花完的余额留给后续行。
4. 每个新组完成后，它与此前已完成组共同形成一个合法的 partial dual。该下界从下一张普通行开始参与 `directed-cut -> farthest -> group-tour` 的短路剪枝。
5. 若累计工作最终购买完全部 `g` 组，则恢复 dual primal 上界，并允许后续 residual packing 使用完整 residual。若搜索结束时只购买了前缀，则不补算剩余组，也不从 partial residual 恢复 primal。
6. 其余 ordinary `D`、低层 anchored `A`、高层伴随 `H` 和转置终端流程与 ReleaseV5 相同，最终仍由完整的精确状态递推返回最优解。

这里有两个清楚的结束条件。**dual 构造**在购买完 `g` 组时结束；若普通工作始终不足，则保持 partial dual 到求解结束。**整个算法**仍在 ReleaseV5 的精确 completion 完成后结束，dual 是否完整只影响剪枝强度，不影响是否能够得到答案。

## 3. 渐进式 Dual 的具体构造

### 3.1 固定顺序

所有组先按其到 permanent root 的距离降序排列，距离相同则按原组编号排序。这与 Test149 一次性构造 dual 时的顺序完全相同，运行中不会根据数据集、`g` 或搜索进度改变顺序。

设当前准备处理组 `i`。第一组直接以原图组距离作为 residual 最短路结果；后续组先读取原图组距离，再只从此前势函数改写过的有向弧产生初始松弛，随后在完整邻接表上执行 residual Dijkstra 闭包。该过程正是 Test149 的 changed-arc 初始化，只是把原来函数内部的 `g` 次循环暴露为一次推进一组的接口。

完成组 `i` 后，将其势函数截断到 root distance，扣减相应有向 residual capacity，并记录本轮新改写的弧。于是下一组看到的 residual 与 Test149 在同一位置看到的 residual 逐点相同。若最终完成全部组，Test152 得到的完整势、dual objective 和 residual 与同顺序的 Test149 一致。

### 3.2 Partial Dual 如何参与剪枝

已处理组使用刚构造的势，未处理组的势定义为零。查询剩余组集合 `R` 在顶点 `v` 上的 directed-cut 下界仍写为这些组势之和；未处理组只贡献零，因此 partial dual 可能较弱，但不会过大。

Test152 只在一张 ordinary row 完整存储后推进 dual。每张 row 内部的 `CutH`、`CheapH` 和 `H` 缓存因此始终对应同一个 dual 前缀，不会出现同一行内势函数变化而缓存失效。新前缀只用于尚未开始的行；已经求出的精确 `D` 值无需重算。

### 3.3 完整 Dual 才能恢复 Primal

dual primal recovery 和 residual packing 依赖全部组完成后的 residual 语义。Test152 因此严格区分两种状态：

- partial dual：只提供合法下界，不恢复 primal，不运行 packing；
- complete dual：恢复与 Test149 相同的 primal candidate，并按原生命周期运行 packing 或释放 residual。

这一限制避免把尚未覆盖全部组的 residual 错当成完整可行树证书。

## 4. 工作量购买规则

### 4.1 租用工作

对每张普通 row，记实际工作量为

\[
W_{row}=N_{seed}+N_{pop}+N_{relax},
\]

其中 `N_seed` 是尝试生成的 seed 数，`N_pop` 是 priority queue 弹出数，`N_relax` 是邻接边松弛尝试数。这些量都已经真实发生，不依赖事后估计，也不把无法由 incumbent 避免的图加载和组距离计算伪装成可回收收益。

### 4.2 单层购买成本

每个 dual 组的购买预算定义为

\[
C_{dual}=2m+n.
\]

它对应一次组势推进涉及的两个结构规模：至多 `2m` 条有向 residual arc 和 `n` 个顶点势。该值是算法自身的结构量，不是实验拟合阈值。实现只在 row 边界执行

\[
B \leftarrow B+W_{row}; \qquad
\text{while } B\ge C_{dual}: B\leftarrow B-C_{dual},\ \text{advance one group}.
\]

因此轻查询不会先支付完整 `gC_dual`；重查询则随已经发生的 ordinary 工作逐渐买回更强下界。规则中没有 `g<=x`、第几层开启、达到某个 wall time、某个数据集或某种 density 的分支，符合 `agent.md` 第六条。

**审计边界。** `2m+n` 不是一次 dual 推进真实运行时间的上界，因为 changed-arc seeds 之后仍可能执行 residual Dijkstra，并发生 priority-queue push/pop。故“购买”只是便于描述的确定性调度比喻，不能据此主张竞争比、严格摊销收益或理论最优开启时机。它的正确性来自每个 partial prefix 都合法，最坏复杂度来自每组至多推进一次；实际价值必须在 M5 中单独消融。

### 4.3 为什么按 Row 结算

按每次 pop 或 relax 立即切换 dual 会使当前 row 的启发式缓存前后不一致，并引入细碎的分支和计时干扰。row 是现有离线有序列表的自然原子：完成后精确值已经冻结，下一行尚未建立缓存。因此 row 边界既保持实现简单，也给出清楚的正确性边界。

## 5. 正确性

### 5.1 每个前缀都是合法下界

每处理一个组，构造都只在当前非负 residual capacity 内增长该组势，并从 residual 中扣除对应 reduced cost。因而已处理组的总势满足原 directed-cut dual 的边容量约束；未处理组取零不会增加任何边负载。故任意长度的前缀都是 dual feasible，其 root 势和不超过最优 GST 代价。

### 5.2 动态启用不删除精确推导

dual 在框架中只作为 admissible lower bound 拒绝不可能优于 incumbent 的候选，不参与 `D/A/H` 状态值定义。较短前缀只会少剪枝，不会错误剪枝。势只在 row 边界增强，当前行使用的缓存保持一致；以后行使用更强但仍合法的下界。因此动态启用不会删除任何可能产生最优解的递推。

### 5.3 不完整时仍然完备

若普通工作不足以购买全部组，剩余组势为零，算法退化为使用较弱 directed-cut 下界的同一个 ReleaseV5 精确 DP。最终 completion 不依赖 dual 完整性，所以仍返回精确答案。primal recovery 和 packing 只在完整前缀上执行，因而不会把 partial dual 当作可行上界。

## 6. 复杂度与空间

单个组推进包含 changed-arc seeds、residual Dijkstra 传播、`n` 个势值写入和对边 residual 的更新。使用 `priority_queue` 时可按 `O((m+n) log n)` 记；全部 `g` 组最坏仍为 `O(g(m+n) log n)`，不超过 ReleaseV5 原有 sequential dual 的渐进上界。工作量购买只增加每行常数次计数和至多 `g` 次总推进，不改变 `D/A/H` 的最坏复杂度。

若从未购买第一组，Test152 不分配组势和 residual。购买 `j` 组后只为这 `j` 组分配势向量，主要附加空间为 `O(jn)` 势、`O(m)` residual 和 `2m` bits 的 changed-arc 集合；完整时才达到 Test149 的 `O(gn+m)` 空间。渐进调度不会清零未购买的 `g-j` 组，也不会同时保存多份 dual。

## 7. 实验结果

### 7.1 正确性

- GPU4GST Musae、Twitch、Github 上 `g=4..10`、每个 `<dataset,g>` 20 条，共 `420` 条查询，Test152 与 V5 权重逐条一致。
- 在物理删除 quarter upper 并改为按组分配势向量后，最终源码使用 seed `718154`、`g=2..8` 的随机小图与 DPBF 对拍 `300/300`，误差均不超过 `1e-6`。
- Test156 的 30 条 no-quarter 高 `g` 查询同样保持权重一致。

### 7.2 `g=4..10` 配对面板

下表为每个单元 20 条查询的总 wall time，单位为秒。`Strict/Test152` 大于 1 表示 Test152 更快。

| 数据集 | `g` | Strict PrunedDP++ | V5/Test149 | Test152 | `Strict/Test152` |
| --- | ---: | ---: | ---: | ---: | ---: |
| Musae | 4 | 0.54 | 1.41 | 0.68 | 0.79x |
| Musae | 5 | 1.20 | 1.86 | 0.90 | 1.33x |
| Musae | 6 | 1.97 | 2.37 | 1.35 | 1.46x |
| Musae | 7 | 6.71 | 3.11 | 2.72 | 2.47x |
| Musae | 8 | 32.90 | 5.11 | 5.49 | 5.99x |
| Musae | 9 | 157.85 | 7.58 | 8.78 | 17.98x |
| Musae | 10 | 348.46 | 14.74 | 15.84 | 22.00x |
| Twitch | 4 | 1.28 | 2.64 | 1.44 | 0.89x |
| Twitch | 5 | 2.44 | 3.68 | 1.90 | 1.28x |
| Twitch | 6 | 8.04 | 4.78 | 3.40 | 2.36x |
| Twitch | 7 | 13.42 | 6.46 | 5.31 | 2.53x |
| Twitch | 8 | 14.61 | 7.39 | 6.44 | 2.27x |
| Twitch | 9 | 272.41 | 15.75 | 16.56 | 16.45x |
| Twitch | 10 | 518.64 | 24.58 | 27.68 | 18.74x |
| Github | 4 | 1.47 | 2.73 | 1.72 | 0.85x |
| Github | 5 | 2.23 | 3.88 | 2.17 | 1.03x |
| Github | 6 | 6.15 | 4.75 | 3.72 | 1.65x |
| Github | 7 | 32.88 | 7.73 | 9.06 | 3.63x |
| Github | 8 | 79.15 | 14.04 | 15.82 | 5.00x |
| Github | 9 | 348.52 | 22.85 | 29.11 | 11.97x |
| Github | 10 | 957.32 | 43.14 | 45.70 | 20.95x |

结果边界很明确：**`g=5..10` 的 18 个单元全部快于 Strict，`g=9..10` 仍保留约 `12x--22x` 的大 `g` 优势；`g=4` 仍慢 `12%--26%`。** 相对 eager V5，Test152 在低 `g` 大幅降低固定成本，在高 `g` 通常有小幅回退，最差已测单元约为 `1.27x`。这说明渐进购买解决了主要 fixed-cost 问题，但 `g=4` 尚不能宣称完全达到“不慢于 Strict”。

结果目录：

- `result_snapshot/v5_progressive_dual/20260718_test152_g4_g10_q20`
- `result_snapshot/v5_progressive_dual/20260718_test152_smoke`
- `result_snapshot/v5_progressive_dual/20260718_test152_smoke_lazy_init`

### 7.3 简化审计

| 实验 | 删除内容 | 观察 | 决定 |
| --- | --- | --- | --- |
| Test150 | 全部 dual | 证明 eager dual 是小 `g` 主要固定成本，但大搜索会失去关键剪枝 | 只保留证据，删除目标 |
| Test151 | 达到总预算后一次性买完整 dual | `420/420` 正确；高 `g` 通常比 V5 慢 `20%--30%` | 删除实现与目标 |
| Test153 | junction 及 anchor facilities | 小 `g` 最多改善约 `11%`；高 `g` 为 Test152 的 `1.00x--1.98x` | 保留 junction/facilities |
| Test154 | early 与 quarter partition uppers | 高 `g` 为 Test152 的 `1.02x--1.22x` | 继续拆分归因 |
| Test155 | facilities/tree，保留 junction | 短面板约 `0.99x--1.07x`；历史重查询仍有 `4%--22%` state 和 `7%--14%` wall 收益 | 保留摊销 facilities/tree |
| Test156 | quarter upper | 30 条中只 2 条更新，端到端无稳定收益 | 已从 Test80 物理删除 |

packing 在高 `g` 面板 `16/30` 条产生非零增强，不能视为无效；early upper 在 `18/30` 条更新 incumbent，其中 `8/30` 来自 early witness，总成本 `248.785ms`，继续保留。quarter upper 只有 Twitch g8 q1 与 Github g8 q1 两次更新，后续 witness 又给出更强上界，且这两条端到端均约慢 `18ms`；因此删除 quarter 是本轮唯一有充分证据的代码简化。

消融结果目录：

- `result_snapshot/v5_small_g_profile/20260718_test149_baseline`
- `result_snapshot/v5_small_g_profile/20260718_test150_no_dual`
- `result_snapshot/v5_lazy_dual/20260718_test151_g4_g8_q20`
- `result_snapshot/v5_lazy_dual/20260718_test151_g9_g10_q20`
- `result_snapshot/v5_simplification_ablation/20260718_test153_test154`
- `result_snapshot/v5_simplification_ablation/20260718_test155_no_anchor_facilities`
- `result_snapshot/v5_simplification_ablation/20260718_test156_no_quarter`
- `result_snapshot/v5_simplification_ablation/20260718_test152_upper_stats`

## 8. 冻结状态与未完成项

Test152/M5 已在新增同源面板中完成与 M4 的单一路线取舍：Musae/Twitch/Github `g=4..10` 共 420 条的 M4/M5 总时间为 `212.569/207.016s`，后续 Test 候选统一使用 M5，不按数据集或 `g` 回退 eager 路线。partial dual 和 packing 的直接证书、逐档负面结果与冻结理由见 `test157_production_audit.md`。

Test152/M5 已通过整算法贡献、正确性、复杂度、无特判和同源效果评审，并独立清理为 ReleaseV6。发行版删除本文件中的研究统计和消融开关，只保留同一渐进调度。GPU4GST 三图 `g=11..16` 冻结面板完成 176/180 条；传统五库完整冻结面板和同机 GPU4GST 仍是投稿实验缺口。除非方法结构发生实质变化，不运行五小时级 DBLP g13 全量。

## 9. 引用边界

渐进 dual 本身是 ReleaseV6 的执行优化，不单独承担论文核心。[Wong, 1984](https://doi.org/10.1007/BF02612335) 是 directed-cut dual-ascent 路线的来源；GST 的逐组势、sequential residual、changed-arc 初始化以及本节的按普通工作量渐进调度均是本仓库适配，不能归为 Wong 原文结论。完整方法贡献与引用边界见 `release_v6_method_cn.md` 和 `test152_integrated_method_review.md`。
