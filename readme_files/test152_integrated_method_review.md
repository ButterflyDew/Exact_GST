# Test152 整算法评审：锚定双向半状态 GST

更新时间：2026-07-18。本文评审的对象不是某一个公式是否首次出现，而是 **Test152 作为一套完整 exact GST 算法，是否与最接近的完整算法形成了清楚、可证明且有独立效果的方法差异**。逐组件排重仍以 `test152_novelty_audit.md` 为准；本文不会把已有的 meet-in-the-middle、inside/outside、packing product 或 dual ascent 重新包装成原创原语。

## 1. 评审结论

**通过“论文候选方法冻结”门，但只允许受限的方法 claim。** Test152 可以作为 ReleaseV6 的方法基础，原因是目前核对到的完整 exact Steiner/GST 算法都没有采用下面这套联合执行结构：

1. 固定一个**可选终端组**作为永久锚点，把普通组状态与唯一含锚侧分开；
2. 在平衡三分解允许的状态包络内，只正向求低层锚定 inside 状态；
3. 对高层锚定状态不再继续正向物化，而从完整解终端反向求 closure-aware outside 状态；
4. 把高层 outside 的大量补集终端转置为按根生成的一次不相交普通块事件，再沿低层／高层边界结算。

这是一项**新的 exact GST 求值组织与已知构件的非平凡专门化**，不是新的通用半环定理，也没有改善 `O(3^g)` 的最坏指数底数。允许的论文定位是“面向 exact GST 的 anchored bidirectional half-state evaluation”；不允许声称“首次发明 outside”“首次只保留半层子集”或“获得新的渐近复杂度”。

该判定足以允许下一步清理独立的 ReleaseV6 候选代码，但不等于论文实验已经完成。同机 GPU4GST、传统五库冻结面板和最终可复现实验仍是投稿前的实验门，而不是继续改变方法定义的理由。

## 2. 完整方法主线

给定查询组集合 `Gamma`，选择永久锚点组 `T_a`，其余 `k=g-1` 个组组成集合 `K`。令 `h=floor(g/2)`，并令 `r=floor((h-1)/2)`。算法在同一个根顶点 `v` 上使用三类状态：

- `D(S,v)`：覆盖普通组子集 `S subseteq K` 的最小 rooted GST 代价；
- `A(S,v)`：覆盖锚点组以及普通组子集 `S` 的最小 rooted GST 代价；
- `H(T,v)`：已经位于高层锚定状态 `T` 的合并根时，到达某个完整平衡结算的最小剩余代价。

算法按以下顺序执行并结束：

1. **普通阶段。** 对所有 `1<=|S|<=h` 计算 `D(S,.)`。每一行先离线合并已完成的真子集行形成 seed，再做一次图最短路闭包；闭包后只把 root-irreducible 值发布为后续普通块。普通阶段结束时，所有平衡分解可能使用的非锚块都已经可用。
2. **低层锚定阶段。** 从 `A(empty,v)=dist(v,T_a)` 出发，对 `1<=|S|<=r` 计算 `A(S,.)`。每次转移只合并一个较早的 `A` 行和一个不相交的普通 `D` branch，随后做图闭包。该阶段结束时，跨切分边的 inside 端全部可用。
3. **高层终端生成。** 对每个根 `v`，把一个普通块或一对不相交普通块 `L,R` 产生的代价按 `T=K-(L union R)` 分发给对应高层目标。directed-cut 组势只用于把这些事件按 reduced cost 排序和过滤，不改变事件的真实代价。
4. **高层反向阶段。** 按 `|T|` 从大到小计算 `F(T,.)` 与 `H(T,.)`。`F` 汇总完整结算终端以及所有 strict superset 后继；`H=C(F)` 再把后缀跨过一次图闭包拉回当前合并根。处理到 `|T|=r+1` 后，所有跨切分后缀都已闭合。
5. **边界结算。** 对每条 `S -> T=S union B` 且 `|S|<=r<|T|` 的第一条跨切分依赖，计算 `A(S,v)+Br(B,v)+H(T,v)`。取所有边界候选和普通阶段已得到的完整可行上界的最小值后结束。

这不是“先跑方案 A，再从头跑方案 B”。`D`、低层 `A`、高层 `H` 和终端事件属于同一依赖图的一次求值；高层 `A` 没有被另一套求解器重算。

## 3. 为什么不是已有算法的直接改名

| 最接近工作 | 已有内容 | 与 Test152 的整算法差异 |
| --- | --- | --- |
| PrunedDP++ | 完整 rooted `(mask,v)` grow/merge、best-first、upper/lower-bound pruning | 不固定永久组锚点，不把锚定 lattice 切成低层 inside 与高层 outside，也没有补集终端转置 |
| GPU4GST TrimCDP-WB | 同一完整 rooted 状态的并发松弛、GPU 负载均衡和并发安全剪枝 | 仍分配 `O(n2^g)` rooted 状态并迭代到无更新；没有半状态边界或 partial outside |
| Iwata--Shigemura MITM | 平衡三分解，只计算至多一半终端的 rooted 状态，以三个普通部分结束 | 三部分保持对称；没有可选组锚定的 `D/A` 组织、inside/outside 切分和高层终端反向传播 |
| Dijkstra Meets Steiner / DS* | 固定一个**顶点终端**作为 root，对其余终端建立子集搜索状态和 future lower bound | 不是可选代表的组锚点，也没有平衡半状态 completion 与 outside 边界 |
| 通用 hypergraph outside | 给定完整推导超图后，按超边反向传播 outside value | 不给出 GST 的平衡有限子图、低／高切分位置、闭包压缩后的 `F/H` 接口或补集终端的稀疏生成方法 |
| subset packing 与 dual ascent | 不相交集合对聚合；模势与 reduced-cost filtering | 不给出这些事件应如何成为 anchored GST outside 的终端，也不构成完整求解器 |

因此，**逐组件都有先例**与**完整算法已经存在**是两个不同结论。通用 outside 可以验证 `F/H` 递推的代数合法性，却不会自动给出一个只在平衡状态包络内运行、能替代 Iwata 三块终端并在真实 GST 图上取得收益的算法。这里的贡献应落在完整状态组织和求值边界，而不是任何单个原语。

## 4. 非平凡的 GST 专门化

### 4.1 从三块分解到永久组锚定

平衡三分解只保证存在三个 rooted 部分，每部分包含不超过 `h` 个查询组。Test152 先为最优树中的每个查询组固定一个代表命中，再把包含锚点组代表的唯一部分定向为 `A`，另外两部分定向为普通 `D`。该转换必须同时覆盖组重叠、一个顶点命中多个组、锚点代表恰为平衡根、空普通块和零权路径。它没有加强 Iwata 的渐近界，但建立了后续 inside/outside 切分所需的有向依赖图。

### 4.2 闭包感知的 partial outside

锚定递推包含两类依赖：同根的 `A+D` 合并，以及合并后的一次图最短路闭包。若直接把高层值统称为 outside，会把闭包前后的根语义混在一起。Test152 因此区分：

```text
F_T(v) = min(terminal_T(v), min_{U strict-superset T} Br_{U-T}(v)+H_U(v)),
H_T(x) = min_v dist(x,v)+F_T(v).
```

`F` 位于闭包后节点，`H` 是反穿闭包后回到前驱合并根的值。任一从低层 `A` 到完整终端的推导路径都有唯一第一条跨切分边，因此只需在该边组合 inside 与 outside，不需要物化高层 `A`。这个结论是通用 outside 在本 GST 依赖图上的专门化证明，也是发行实现不能随意合并 `F/H` 的接口约束。

### 4.3 终端转置是联合机制的必要部分

若为每个高层目标 `T` 单独枚举其补集的全部 `D(L,v)+D(R,v)`，outside 虽然正确，却会重复扫描相同普通行。Test152 改为固定根 `v`，枚举一条普通 entry 或一对不相交 entry，再一次性分发到 `T=K-(L union R)`。组可加 dual 势满足补集恒等式，因此排序与过滤可在 reduced cost 上完成，而写入终端的仍是真实代价。

这一转置不是新 packing product，但它是把 partial outside 变成实际有效 GST 算法的必要接口。消融中 M2 的 `F/H` 单独方向不稳，而 M3 的 `F/H + transpose` 在冻结的多库、多询问和反序重复中保持正方向，说明论文必须把二者作为一个联合机制，不能拆开宣传。

## 5. 状态数与复杂度边界

Test152 **没有减少 Iwata-equivalent M0 的符号行总数，也没有把最坏复杂度降到 `o(3^g)`**。对 `k=g-1` 个普通组，它保存：

```text
D rows:      1 <= |S| <= h
A/H rows:    0 <= |S| <= h-1
```

两部分合计与 M0 在全部 `g` 个组上保存 `1<=|S|<=h` 的半状态行数相同。真正变化的是行的**语义和求值方向**：普通 mask 少一个锚点维度，高层锚定 inside 被后缀 outside 替代，三块 completion 被转置为稀疏终端事件。完整实现仍保持 `O(poly(g)(n3^g+2^g C_G))` 时间和半状态级峰值空间；其中 `C_G` 表示一行图闭包的代价。

因此论文可以报告更少的高层 `A` values、终端事件和实际运行时间，但不能写“总状态行数渐近减少”“空间必然更低”或“指数底数改善”。当前 query peak RSS 的方向也确实不统一。

## 6. 独立效果证据

完整方法的两个结构增量都已有同源消融：

1. **M0 -> M1：永久锚定状态组织。** Twitch/Github `g=12` 各 20 条、两轮合计分别为 `3.187x` 和 `2.615x`，M1 逐查询胜 `39/40` 与 `38/40`；低档 Toronto `g=10` 只有 `1.119x`，应如实保留。
2. **M1 -> M3：`F/H + transposed terminal`。** Twitch/Github `g=12` 两轮合计为 `1.098x/1.202x`；Musae/Toronto/Github `g=12` 和 Musae `g=13` 的冻结结果也保持联合机制正方向。M2 单独不稳定，所以只认可 M3 联合增量。
3. **strict `Br` 不是核心。** Test158 两轮 240 对只有 `1.018x`，无空间收益；它保留为完备的规范化辅助，不承担论文新颖性。
4. **M4/M5 只是生产剪枝选择。** 420 条同源面板把 M5 冻结为单一路线，但 facility、tour、packing 与 progressive dual 均不进入核心 claim。

这些结果足以证明整算法差异不是只改符号；它们还不足以替代最终八库与 GPU 对照实验。

## 7. 允许与禁止的论文表述

### 7.1 允许的核心表述

> We introduce an exact GST evaluation framework that orients a balanced three-way decomposition around a permanent optional group anchor, evaluates the lower anchored lattice inside, evaluates the upper lattice outside across graph-closure edges, and transposes complement completion into sparse root-wise events.

中文可表述为：**本文提出一种面向精确 GST 的锚定双向半状态求值框架；它在永久组锚点下定向平衡三分解，以低层 inside 和高层 outside 共同完成同一状态依赖图，并用按根终端转置避免逐目标重复 completion。**

### 7.2 必须明确承认的已知起点

- 平衡三分解与半状态 MITM 来自 Iwata--Shigemura；
- inside/outside 的一般代数来自 Goodman、Li--Eisner 与 Gildea；
- 不相交子集事件来自 subset packing/convolution；
- directed-cut dual 与 reduced costs 来自 Steiner dual-ascent 路线；
- rooted grow/merge 与 lower/upper-bound pruning 来自 DPBF、PrunedDP++ 与 GPU4GST。

### 7.3 禁止的表述

- “首次提出 min-plus outside / backward DP”；
- “首次把 Steiner/GST 状态限制到一半子集”；
- “首次固定一个 terminal/anchor 消掉一维”；
- “总状态数或最坏 `3^g` 指数底数得到改进”；
- “M2 adjoint 单独稳定加速”或“实际空间稳定降低”；
- “所有辅助 upper/lower bounds 都是本方法独有”。

## 8. 冻结前检查

整算法新颖性、正确性、复杂度和内部效果已经足以冻结候选方法。创建 ReleaseV6 时仍必须满足以下实现条件：

1. 只保留 M5 的单一主线，不包含 M0--M4、probe、validator、进度输出或运行时回退；
2. 不包含数据集名、固定 `g`、query id、density、wall-time 或观测进度特判；所有边界只由 `g`、集合大小、已执行工作量和严格代价界推出；
3. 保留有序离线行与稀疏／位图／稠密布局的按字节最小选择，不引入 Hash 作为主结构；
4. 发行代码重新通过 DPBF 小图随机对拍、Test157 核心 verifier、Release `/O2` 构建和冻结真实查询；
5. 发行文档把核心 `D/A/F/H + transpose` 与通用 pruning modules 分开，避免再次把实现优化混入方法定理。

## 9. 主要原文

1. Yoichi Iwata and Takuto Shigemura. *Separator-Based Pruned Dynamic Programming for Steiner Tree*. AAAI 2019. <https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843>.
2. Yeow Meng Chee et al. *Efficient and Progressive Group Steiner Tree Search*. SIGMOD 2017. <https://doi.org/10.1145/2882903.2915217>.
3. Jiayu Li et al. *Fast Optimal Group Steiner Tree Search using GPUs*. PACMMOD/SIGMOD 2025. <https://doi.org/10.1145/3769792>.
4. Stefan Hougardy, Jannik Silvanus, and Jens Vygen. *Dijkstra Meets Steiner*. Algorithmica 2017. <https://arxiv.org/pdf/1406.0492>.
5. Joshua Goodman. *Semiring Parsing*. Computational Linguistics 1999. <https://aclanthology.org/J99-4004.pdf>.
6. Zhifei Li and Jason Eisner. *First- and Second-Order Expectation Semirings with Applications to Minimum-Risk Training on Translation Forests*. EMNLP 2009. <https://aclanthology.org/D09-1005.pdf>.
7. Daniel Gildea. *Efficient Outside Computation*. Computational Linguistics 2020. <https://aclanthology.org/2020.cl-4.2.pdf>.
8. Andreas Bjorklund et al. *Fourier Meets Mobius: Fast Subset Convolution*. STOC 2007. <https://arxiv.org/pdf/cs/0611101>.
9. Richard T. Wong. *A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph*. Mathematical Programming 1984. <https://doi.org/10.1007/BF02612335>.

