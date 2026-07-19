# 固定 Consumer 的双 Attachment 标量化

更新时间：2026-07-16。本文记录 Test125--140 之后得到的一条正向理论结论：**对一个已经确定的 `(core, B, C)` 完成方案，不必保存 paid core 关于所有 future blocks 的 Pareto profile，也不必枚举两个 attachment vertices；可以把 `B`、`C` 当作两个带 rooted cost 的 macro labels，用一次标准 subset DP 精确定价。** 该结论只解决单个 consumer，尚未解决怎样以可接受的总复杂度处理全部 consumers。后续 Test141 用一个规范设施分块绕过了 consumer 枚举，但作为附加上界 oracle 的工作仍大于状态收益；实现已撤回，本文的 fixed-consumer 定理继续保留。

## 1. 要解决的固定方案

给定非负权无向图。`S` 是已经分配给 paid core 的 groups，其中包含 permanent anchor；`B`、`C` 是两个互不相交、且与 `S` 不交的 future blocks。记 `D(X,v)` 为连接 block `X` 并包含接口顶点 `v` 的精确 rooted GST cost。对任意命中 `S` 的连通子图 `T`，定义：

```text
phi_T(B) = min_{x in V(T)} D(B,x)
phi_T(C) = min_{y in V(T)} D(C,y)
```

这个 fixed consumer 的精确定价值是：

```text
Psi(S;B,C)
  = min connected T hitting S
      w(T) + phi_T(B) + phi_T(C).
```

这里采用 paid-profile completion 的**加法计费**：core、`B` witness 与 `C` witness 即使在原图中共享边，公式仍分别计费。因此每个值都是合法可行上界；恢复真实边并集后只可能更便宜。本文要精确计算的是 `Psi`，不是声称任意一个 fixed plan 都等于全局最优 GST。只有对完备的 plan family 取最小值，才由 paid-half completion 定理恢复全局最优值。

直接写成双接口形式需要考察 `(x,y)`：

```text
min_{x,y} D(B,x) + D(C,y) + Core(S,x,y),
```

其中 `Core(S,x,y)` 是命中 `S` 且包含 `x,y` 的最小连通 core。显式保存该表会引入 `n^2` 接口维，这正是此前第二 attachment 路线不能接受的形式。

## 2. 把两个接口改成两个 macro labels

构造标签集合：

```text
L = {S 中的每个单独 group} union {beta, gamma}.
```

每个标签只提供一张 rooted seed row：

```text
f_i(v)     = 顶点 v 到 core group i 的组距离
f_beta(v)  = D(B,v)
f_gamma(v) = D(C,v).
```

随后在这些标签上运行标准 rooted subset DP。对标签子集 `X`，先在同一根合并两个真子集，再做一次图上的度量闭包：

```text
join_X(v) = min over Y proper subset X
              M(Y,v) + M(X-Y,v)

M(X,v) = min_u join_X(u) + dist(u,v).
```

实现中对 `Y` 使用固定 pivot 去重；每张闭包 row 仍可由 `priority_queue` 生成有序 labels，不需要 Hash。最终答案为 `min_v M(L,v)`。两个 attachment vertices 没有成为状态维度：`beta` 和 `gamma` 在各自 seed row 中隐式选择进入 macro skeleton 的位置。

## 3. 精确定理

**定理。** 对固定且两两不交的 `S,B,C`，上述 macro-label DP 的答案恰好等于 `Psi(S;B,C)`。

**从 paid core 到 macro DP。** 任取 `Psi` 中的连通 core `T`，并取达到两个 profile minimum 的 `x,y in V(T)`。把 `T` 作为连接 core groups、`x` 与 `y` 的 skeleton，在 `x` 处接入成本 `D(B,x)` 的 `beta` seed，在 `y` 处接入成本 `D(C,y)` 的 `gamma` seed，就得到一个合法 macro derivation。因此 macro DP 不大于 `Psi`。

**从 macro DP 到 paid core。** 任取一棵 macro DP derivation。把 `beta`、`gamma` 的内部 rooted witnesses 只视为附着在各自 seed root 上的抽象付费项；保留两条 seed-to-merge 路径、所有 core group 分支和其余 merge skeleton。剩余部分的边并集是一个命中 `S` 的连通子图 `T`，并包含两个 seed roots `x,y`。若 derivation 中不同分支重复经过同一条边，取并集只会使 `w(T)` 更小，因此 `w(T)+D(B,x)+D(C,y)` 不大于该 derivation cost；再由 `phi_T(B)<=D(B,x)`、`phi_T(C)<=D(C,y)`，得到 `Psi` 不大于该 derivation。与前一方向合并即得等式。

这个证明解释了为何 Test125/127/128 的单 attachment 失败不否定本定理：那些方法先为每个 block 独立选根，再尝试拼接；macro DP 则让两个 block 的根与 paid core geometry **联合决定**。它也不要求只保留 rooted-optimal core witness，所以避开了固定 g13 反例中 `160 -> 161`、`106 -> 107` 所暴露的“更贵 core 拥有更好 future attachment”问题。

## 4. 单个 consumer 的复杂度

令 `s=|S|`，macro labels 数为 `r=s+2`。在 `D(B,*)`、`D(C,*)` 已存在时，标准图上 subset DP 可在下列界内完成：

```text
time   O(3^r n + 2^r (m + n log n))
space  O(2^r n).
```

与显式 `Core(S,x,y)` 相比，它消除了 `n^2` 双端点表。这个结论是**按 consumer 标量化**，不是一张可被任意 future block 查询的通用 paid profile；因此也绕开了通用 profile Pareto family 的指数反链，但只绕开一次。

## 5. 为什么不能逐 Consumer 生产化

设一共有 `k` 个可分配标签。若枚举所有 core `S`，再把其余标签分给有方向的 `B/C`，并为每个方案独立执行 macro DP，则仅 subset joins 的总工作已经是：

```text
sum_s C(k,s) * 2^(k-s) * 3^s = 5^k.
```

即使忽略方向对称和两个额外 macro labels 的常数，这也比当前 A 框架的 `3^k` 型主项更差。若把各 plan 的 rows 全部物化，累计 row cells 还会出现 `4^k n` 量级；逐 plan 释放只能降低 peak，不能降低总工作。另一方面，试图让所有 consumers 完全共享这些 joins，会重新得到一般实权 min-sum subset convolution，现有边界文档已经说明不能假设存在通用 exact 快变换。

因此，真正的下一步不是实现一个逐 plan 循环，而是先证明下面二者之一：

1. **可证明的小请求集。** 在生成昂贵 macro rows 前，用 exact lower certificate 证明未请求 plans 的下界已经不小于 incumbent；请求规则不能依赖数据集、固定 `g`、层号、密度、运行时刻或已知 optimum。
2. **批量共享算子。** 让一批 `(S,B,C)` 复用离线有序 rows，并证明总工作回到 `3^k` 型或更低，同时真正删除现有高层 D/A 状态，而不是在其上追加定价。

Test141 随后验证了第一种边界的一个特例：只请求 Test121 最优设施方案恢复出的一个规范 block partition，并把全部 blocks 一次提升为 macro labels。请求数为一、标签数至多减半，因而避免 `5^k`；但 fast20 wall 回退 `52.86%`，说明“请求集足够小”仍不等于“附加 oracle 能回本”。完整结果见 `../archive/test141_facility_guided_macro_lifting_20260716.md`。下一候选必须让 macro rows 替代高层 A/D 输出，不能只追加定价。

## 6. 已有验证

现有 `tools/paid_half_state_probe` 已经包含所需的独立小图核验。`macro-plan` 模式固定 core4 与两个 future blocks，对每个 plan 同时比较：

1. 穷举 connected edge subgraphs 得到的 `w(T)+phi_T(B)+phi_T(C)`；
2. 完整六标签 macro DP；
3. Test76 的全量 block-anchor 公式；
4. Test76 的一阶约化公式。

Release/O2 现有记录为 singleton groups `500/500`、双候选 groups `200/200`，四者逐 plan 相同。该证据验证了实现与公式，但正确性依据仍是第 3 节的双向证明。由于现有探针已经直接覆盖本文等式，本轮没有复制工具、没有新增随机目录，也没有运行真实数据集。

## 7. 与论文方法的边界

[Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302) 是 rooted subset DP 的经典来源；本文使用它作为 fixed consumer 的计算内核，不把该 DP 本身称为原创。本文新整理的是 paid attachment completion 到“两张 rooted block rows + macro labels”的标量化视角，以及它与 permanent-anchor A 消维目标之间的接口。该表述是否具有论文原创性仍需系统文献检索；在完成检索和全量算法前，只称为仓库内的候选理论结论。
