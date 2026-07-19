# Test152 扩大引文链与逐定理新颖性复核

更新时间：2026-07-18。本文只做**逐组件与逐定理排重**，不重复正确性证明，也不把实验加速等同于新颖性。完整算法是否构成新的 exact GST 求值组织，另由 `test152_integrated_method_review.md` 评审；两份文档必须同时阅读。

## 1. 当前结论

**逐组件新颖性门未通过。** rooted subset DP、永久锚点、平衡三分解、半层状态、一般 forward/backward、不相交子集聚合和 directed-cut dual 均有先例，不能把其中任一项单独写成 Test152 的原创原理。

原审计已经确认 rooted subset DP、永久锚点、平衡三分解、半层状态、一般 forward/backward、不相交子集聚合和 directed-cut dual 都有先例。本轮扩大引文链又找到更直接的抽象覆盖：Goodman 1999 对交换半环定义 reverse values；Li--Eisner 2009 在加权超图上给出通用 inside/outside 递推；Gildea 2020 证明交换半环可在线性于实例化超图规模的时间内计算 outside values。**min-plus 是交换半环，所以 Test152 的 `F/H`、反向 strict-superset 递推和第一条跨切分边可以机械地解释为其推导超图上的 partial outside computation。** Azuma 2017 的 cancellative 限制因此不再构成新颖性保护。

directed-cut 终端转置也不足以单独成为核心：不相交集合对及按目标聚合由 subset convolution/packing product 覆盖；`pi_X=sum_(i in X)pi_i` 是模集合函数，其补集消去恒等式是直接代数重加权；Steiner 精确算法使用 dual ascent 的 reduced costs 做界与删除也已有先例。当前实现没有给出超越这些通用构件的新渐近界，也不能把某个局部公式写成新的通用定理。

这个结论不等于完整算法的新颖性失败。扩大复核后，PrunedDP++、GPU4GST、Iwata MITM、Dijkstra Meets Steiner 和通用 outside 分别覆盖了局部构件，却没有给出 Test152 的**永久可选组锚定 + 低层 inside + 高层 closure-aware outside + 补集终端转置**联合执行结构。该整算法差异、非平凡专门化、同源效果和受限 claim 已在 `test152_integrated_method_review.md` 中单独通过候选冻结评审。

## 2. 逐定理对照

| Test152 定理或机制 | 最接近的外部结果 | 本轮映射 | 剩余差异 | 评审结论 |
| --- | --- | --- | --- | --- |
| group-balanced decomposition | Iwata--Shigemura 2019, Corollary 1 | 三个 rooted 部分、每块至多一半；其数据还包含 GST reduction 实例 | Test152 补足重叠组、共享代表、零边和空块 | 正确性推广，不是新核心 |
| 永久组锚点与 `D/A` 双状态 | Dijkstra Meets Steiner；DS* | 固定一个 root terminal 后只对其余终端建 mask | 锚点是可选代表的组而非固定顶点 | GST 适配，单独不足 |
| strict root-irreducible `Br` | canonical split；PrunedDP++ conditional merging | 都规范化或限制同根合并 | `Br` 不依赖 incumbent，证明 restricted seed 等于 standard seed | Test158 证明能减工作量，但两轮仅 `1.018x`、无空间或复杂度收益；只保留为辅助引理 |
| adjoint equivalence：`F_T`、`H_T=C(F_T)` | Goodman 1999 reverse values；Li--Eisner 2009 hypergraph outside；Gildea 2020 efficient outside | `F` 是闭包后节点的 outside，`H` 是 outside 反穿最短路 closure 后在合并根的值 | Test152 给出 GST 状态命名、稀疏实现和切分位置 | 直接属于通用 outside 的专门化，不再列为原创候选 |
| 第一条低到高跨切分边 | 通用 inside/outside 在任意边界组合 inside 与 outside | `A_low + Br + H_high` 正是边界超边上“已算 inside + 未算 outside”的组合 | 边界按 mask 大小选择，并只物化两侧所需行 | partial outside 的实现策略，不是独立定理贡献 |
| transposed terminal identity | Bjorklund et al. 2007 subset convolution/packing product | 单 entry 与不相交 entry 对按并集或补集目标聚合 | 真实 rooted cost 事件直接送入高层 `H` | 配对与转置已知；专门接口可算实现贡献 |
| dual-potential complement identity | Wong 1984 dual ascent；模集合函数重加权；Polzin--Daneshmand 2001 reduced-cost pruning | `pi_L+pi_R+pi_T=pi_K` 使目标势消去，按 reduced cost 排序过滤 | 势按 GST 组构造并与 completion 事件共用 | 合法且实用的剪枝恒等式，当前不足以单独成为新算法核心 |
| `O(3^k)` 组合事件界 | Dreyfus--Wagner 总 split 计数；packing product 的不相交赋值计数 | 每个元素属于左、右或剩余目标，共三种角色 | Test152 同时承载低/高锚定边界 | 没有改善已知最坏指数底数 |
| progressive dual、facility upper、packing lower | PrunedDP++、DS*、Wong 及通用 primal/dual 方法 | 都属于 admissible lower、feasible upper 或工作量调度 | 具体组合、稀疏实现和无经验参数的调度不同 | 只作工程消融，不进入核心新颖性 |
| GPU 并发 grow/merge | GPU4GST CDP/TrimCDP-WB | 并发 rooted state 松弛与负载均衡 | Test152 是 CPU partial outside，不采用其 GPU 框架 | 不重合，但也不是 Test152 贡献 |

逐定理结果中，正确性定理仍然成立；变化的是“定理是否原创”的判断。尤其不能因为外部论文没有使用 `F/H` 这两个符号，就忽略它们与 inside/outside 超图递推的结构同构。

## 3. 直接映射：`F/H` 是推导超图上的 outside

### 3.1 通用结果

Goodman 把 item-based deduction 放在交换半环上，并把 outside 对应量称为 reverse values；原文明确包含 reverse Viterbi。Li--Eisner 对任意无环加权超图给出如下 outside 更新：对产生节点 `v` 的超边 `e`，向一个 antecedent `u` 传播 `outside(v)`、超边权和其他 antecedent 的 inside 值。Gildea 进一步从函数组合角度证明：交换半环的 outside pass 可按实例化超图边线性计算，并明确列出 max-product/max-sum Viterbi 情形。

min-plus 的加法为代价相加、加性聚合为取最小，两个运算满足交换半环条件；它与 max-sum 也可通过符号变换对应。因此不能再用“Azuma 的 backward 要求 cancellative semiring”排除通用 outside 先例。Azuma 仍说明一般 forward/backward 的代数背景，但 Goodman、Li--Eisner 和 Gildea 是更直接的排重来源。

原文位置：[Goodman 1999](https://aclanthology.org/J99-4004.pdf)，第 19--21 页的 reverse values；[Li--Eisner 2009](https://aclanthology.org/D09-1005.pdf)，Figure 2--3；[Gildea 2020](https://aclanthology.org/2020.cl-4.2.pdf)，第 4--9 页、Equation (7) 与 Algorithm 3。

### 3.2 Test152 状态的机械对应

把锚定递推展开为一个 min-plus deduction hypergraph，并把一次吸收普通块和一次图闭包分成两类节点：

```text
merge edge:
    A_S(x) + Br_B(x) -> pre-A_(S union B)(x)

closure edge:
    pre-A_T(x) + dist(x,v) -> A_T(v)

completion edge:
    A_T(v) + G_T(v) -> goal
```

在这个超图上，从 goal 反向计算：

- `F_T(v)` 是闭包后节点 `A_T(v)` 的 outside value；
- `H_T(x)=min_v dist(x,v)+F_T(v)` 是 outside 反穿 closure edge 后，在前驱合并根 `x` 的值；
- `Br_B(x)+H_(T union B)(x)` 是反穿 merge edge、向 `A_T(x)` 传播的 outside 候选；
- `A_S(x)+Br_B(x)+H_T(x)` 是在低/高切分边上合并已物化 inside 与 outside 的总推导值。

因此 Test152 定理 3 的“唯一第一条跨切分边”仍是严谨的完备性证明，但其算法结构可以从通用 outside recurrence 机械导出。`F` 与 `H` 必须区分，是因为实现把 closure edge 压成一次多源最短路；这是一项重要实现边界，不再是原创性边界。

## 4. 终端转置与 dual 重加权

### 4.1 不相交事件已经有通用形式

Bjorklund 等把 subset convolution 定义为对互补子集对聚合，并定义 packing product 对不相交子集对聚合；论文还给出 min-sum 嵌入和 Steiner DP 应用。Test152 的单 entry、无序不相交 entry 对以及按 `T=K-(L union R)` 分发，正是同一三角色关系的稀疏事件实现。它不调用快速卷积并不改变该关系已有先例的事实。

原文位置：[Bjorklund et al. 2007](https://arxiv.org/pdf/cs/0611101)，subset convolution、packing product 与 Steiner application。

### 4.2 势消去是模函数重加权

对固定根 `v`，`pi_X(v)=sum_(i in X)pi_i(v)` 是集合 `X` 上的模函数。若 `L,R,T` 两两不交且并为 `K`，则

```text
pi_L(v) + pi_R(v) + pi_T(v) = pi_K(v).
```

所以

```text
D_L + D_R + pi_(a union T)
= (D_L-pi_L) + (D_R-pi_R) + pi_(a union K).
```

该式对当前剪枝很有用，但它来自模可加性的直接代数消去。Wong 已给出 directed-cut dual ascent；Polzin--Vahdati Daneshmand 又系统地使用 dual ascent 的 lower bound 与 reduced costs 做精确 Steiner reduction。当前文献核对未发现与 Test152 **完全相同的 GST completion 代码路径**，但“dual reduced cost + 不相交集合事件”作为组合不足以自动产生新的理论贡献。

原文位置：[Wong 1984](https://doi.org/10.1007/BF02612335)；[Polzin--Vahdati Daneshmand 2001](https://doi.org/10.1016/S0166-218X(00)00319-X)，dual-ascent lower bounds、reduced costs 与 reduction tests。

## 5. 其他已知起点

### 5.1 半状态 MITM

Iwata--Shigemura 的 Corollary 1 把最优 Steiner tree 在一个顶点处分成三个 rooted trees，每块终端数至多一半；其 MITM 随后只处理半层子集。原文实验还包含从 GST reduction 得到的实例。故平衡三分块、半层状态及其 `O(3^k)` 计数都必须作为 M0，而不是 Test152 贡献。

原文位置：[Iwata--Shigemura 2019](https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843)，第 1、4--5 页。

### 5.2 固定根与 future cost

Dijkstra Meets Steiner 固定一个 root terminal，在 `V x 2^(R-{r0})` 上搜索；DS* 使用同一 rooted subset network，并把 heuristic 条件放宽为 admissibility。故消掉一个 mask 维度和给 rooted state 添加 future lower bound 都是已知起点。组锚点的语义适配有实际价值，但不能承担论文原创性。

原文位置：[Dijkstra Meets Steiner](https://arxiv.org/pdf/1406.0492)；[DS*](https://arxiv.org/pdf/2011.04593)。

### 5.3 PrunedDP++ 与 GPU4GST

PrunedDP++ 和 GPU4GST 均覆盖 rooted `(mask,v)` 的 grow/merge、可行解上界及 lower-bound pruning；GPU4GST 进一步覆盖并发 CDP 与负载均衡。两者没有实现 Test152 的 partial outside，但这只能说明代码路径不同，不能抵消第 3 节的通用超图先例。原文分别位于仓库 `readme_files/Efficient and Progressive Group Steiner Tree Search.pdf` 和 `data_origin/Li 等 - 2025 - Fast Optimal Group Steiner Tree Search using GPUs.pdf`。

### 5.4 strict branch 的独立效果不足以恢复主线

Test158 把冻结 M5 的 strict `Br` 与“发布全部 settled 普通状态”的版本作了单变量比较。独立 verifier 已有 `1,956,213` 个 standard/restricted 单元一致；全分支生产版本又通过 500 个 DPBF 随机实例。Musae、Twitch、Github 的 `g=9,10` 各 `q1..q20` 运行两轮反序，共 240 对权重全部一致。

strict 版本把每轮发布的普通 branch 从 `84,775,411` 降为 `71,473,045`，普通拼接工作减少 `11.42%`，锚定 merge probes 减少 `5.59%`。但首轮 `all/M5=1.0371`，反序轮为 `0.99995`，两轮合并仅为 `1.0182`；平均和最大 query peak RSS 都没有改善，最坏 `O(3^k)` 事件界也不变。故 `Br` 是有效的规范化辅助，不是具有强独立效果或新复杂度的论文核心。完整协议和计数见 `test158_root_irreducible_branch_ablation.md`。

进一步的 branch-only 推导也已闭合：每个 `D_S(v)` 都等于 `S` 的 `Br` 分区和最小值，但恢复全部未来消费值仍需保存等价的累计 `D/P` 状态或重算同一 min-plus packing closure。它没有给出比 `O(n2^k)` 状态、`O(n3^k)` 关系更强的界。高层终端的大多数补集又超过普通半层，不能用现成 `D_U` 单行替掉 `D_L+D_R`；补算这些行会撤销 MITM。故按根分区只是等价求值次序，不构成新的结构突破。

## 6. 投稿表述边界

### 6.1 现在必须撤回的表述

- “首次对 min-plus/GST DP 做反向或伴随求值。”
- “`F/H` 与第一条跨切分边本身构成新的反向 DP 原理。”
- “首次只保存一半子集状态或用外侧值替代另一半。”
- “首次转置、批处理或按目标分发不相交子集对。”
- “首次用 directed-cut 势或 reduced cost 剪枝精确 Steiner/GST。”
- “未找到 exact Steiner/GST 同样代码，所以通用 outside 结果不构成先例。”

### 6.2 允许的整算法方法表述

> 我们提出一种面向精确 GST 的锚定双向半状态求值框架：在永久可选组锚点下定向平衡三分解，正向求低层锚定 inside，反向求跨图闭包的高层 outside，并把补集 completion 转置为按根稀疏事件。

这段话可以作为**完整算法层面**的受限方法 claim，但必须紧邻相关工作声明：MITM、outside、packing 和 dual 都是已知起点；贡献在 GST 状态组织和求值边界，不在这些通用原语。不能声称新的渐近界，也不能把 `F/H` 或转置单独写成首次提出。

## 7. 与整算法评审的关系

本文件的结论是“**没有单个新通用原语**”，不是“只有出现更强复杂度定理才能发表”。算法论文也可以贡献一个此前不存在、需要多项专门推导并有独立效果的完整求值组织。整算法评审采用以下额外条件：

1. 最近的完整算法中不存在同一状态组织和执行边界；
2. 从已知原语到可运行 GST 算法仍需要明确的分解、状态语义、边界完备性和终端生成定理；
3. 同源消融能把完整结构的效果从通用 pruning 中隔离出来；
4. 投稿 claim 主动承认全部已知原语，不暗示新的渐近复杂度。

Test152 当前满足这四项，因此 `test152_integrated_method_review.md` 允许冻结论文候选方法。Test158 仍然排除了“仅靠 strict `Br` 恢复核心贡献”的说法；M2 单独不稳定也仍然禁止把 outside 单项宣传为稳定加速。

## 8. 检索范围与局限

本轮沿原文与引用链核对了 exact Steiner/GST、weighted deduction、semiring parsing、inside/outside、hypergraph DP、subset convolution/packing product、directed-cut dual ascent 和 reduced-cost reduction。重点新增 Goodman 1999、Li--Eisner 2009、Gildea 2020 与 Polzin--Vahdati Daneshmand 2001。

文献检索不能证明世界上不存在更直接的实现。可以确定的是：通用 outside 足以否定“`F/H` 是新的通用反向原理”这一宽 claim；目前核对到的完整 exact Steiner/GST 方法又都没有给出 Test152 的联合执行结构。正式投稿仍应由领域外研究者同时复核本文件与整算法评审，不能只比较符号或只比较摘要。

## 9. 主要原文

1. S. E. Dreyfus and R. A. Wagner. *The Steiner Problem in Graphs*. Networks 1(3), 1971. <https://doi.org/10.1002/net.3230010302>.
2. Yoichi Iwata and Takuto Shigemura. *Separator-Based Pruned Dynamic Programming for Steiner Tree*. AAAI 2019. <https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843>.
3. Stefan Hougardy, Jannik Silvanus, and Jens Vygen. *Dijkstra Meets Steiner*. Algorithmica 2017. <https://arxiv.org/pdf/1406.0492>.
4. Johannes K. Fichte, Markus Hecher, and Andre Schidler. *Solving the Steiner Tree Problem with Few Terminals*. SAT 2021. <https://arxiv.org/pdf/2011.04593>.
5. Joshua Goodman. *Semiring Parsing*. Computational Linguistics 25(4), 1999. <https://aclanthology.org/J99-4004.pdf>.
6. Zhifei Li and Jason Eisner. *First- and Second-Order Expectation Semirings with Applications to Minimum-Risk Training on Translation Forests*. EMNLP 2009. <https://aclanthology.org/D09-1005.pdf>.
7. Daniel Gildea. *Efficient Outside Computation*. Computational Linguistics 46(4), 2020. <https://aclanthology.org/2020.cl-4.2.pdf>.
8. Ai Azuma, Masashi Shimbo, and Yuji Matsumoto. *An Algebraic Formalization of Forward and Forward-backward Algorithms*. 2017. <https://arxiv.org/pdf/1702.06941>.
9. Liang Huang. *Advanced Dynamic Programming in Semiring and Hypergraph Frameworks*. COLING 2008 tutorial. <https://aclanthology.org/C08-5001.pdf>.
10. Andreas Bjorklund, Thore Husfeldt, Petteri Kaski, and Mikko Koivisto. *Fourier Meets Mobius: Fast Subset Convolution*. STOC 2007. <https://arxiv.org/pdf/cs/0611101>.
11. Richard T. Wong. *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*. Mathematical Programming 28, 1984. <https://doi.org/10.1007/BF02612335>.
12. Tobias Polzin and Siavash Vahdati Daneshmand. *Improved Algorithms for the Steiner Problem in Networks*. Discrete Applied Mathematics 112, 2001. <https://doi.org/10.1016/S0166-218X(00)00319-X>.
13. Yeow Meng Chee et al. *Efficient and Progressive Group Steiner Tree Search*. SIGMOD 2017. <https://doi.org/10.1145/2882903.2915217>.
14. Jiayu Li et al. *Fast Optimal Group Steiner Tree Search using GPUs*. PACMMOD/SIGMOD 2025. <https://doi.org/10.1145/3769792>.
