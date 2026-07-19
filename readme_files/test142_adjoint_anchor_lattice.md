# Test142：伴随锚点格消维

更新时间：2026-07-16。Test142 是沿 Test121 的 permanent-anchor 框架 A 发展出的**伴随基线**，不是发行版。它保留 ordinary D、root-irreducible branch、永久锚、离线有序行和 `A+D+D` 完备分解，只改变高层 anchored A 格的求值方向：低层 A 正向生成，高层 A 不再物化，而由 completion 终端函数沿同一依赖图反向传播。当前活动后继是 Test145；ReleaseV4 与默认 Test80/Test121 均未改变。

## 1. 动机与主线

令锚组之外共有 `k=g-1` 个组，`h=floor(g/2)`，原框架只需生成到 `a=h-1` 的 anchored 状态 `A(S,v)`。Test107 的 DBLP g15 q33 中，A4/A5/A6 合计耗时约 `3729s`；其中 A6 即使流式消费也需要 `1730s`。删除最终层并不完备，Test129 已给出 `27 -> 29` 反例，因此需要**等价替代高层 A，而不是跳过它们**。

Test142 取结构性平衡切分

```text
c = floor(a/2).
```

算法先照旧生成所有 `|S|<=c` 的 A 行并执行它们的直接 completion；随后从所有 `c<|T|<=a` 的 completion 终端开始，反向穿过高层 A 依赖。最后把反向函数与切分以下的 A 行相交。切分只由 anchored 格深度决定，不依赖数据集、固定 `g`、层密度、wall time 或已知答案。

## 2. 原 A 递推

对非空 ordinary block `B`，记 `R_B(v)` 为框架 A 允许发布给 anchored 合并的 rooted cost。单组时它就是组距离；多组时只在 ordinary D 行的 root-irreducible branch 根上取 `D(B,v)`，其余根为无穷。令图上的最短路闭包为

```text
C(f)(v) = min_u f(u) + dist(u,v).
```

原递推可写成

```text
A(T) = C(min over nonempty B subseteq T of A(T-B) + R_B).
```

对 anchored mask `T`，剩余组由两个 ordinary block 完成，其终端函数为

```text
G_T(v) = min D(L,v) + D(R,v),
```

其中 `L union R` 是 `full-T`，两侧大小不超过 `h`。原算法在每张 A 行完成后计算 `min_v A(T,v)+G_T(v)`。

## 3. 伴随反向递推

在 min-plus 内积

```text
<f,g> = min_v f(v)+g(v)
```

下，无向图距离对称，因此闭包算子自伴随：

```text
<C(f),g> = <f,C(g)>.
```

于是一个正向 block 转移 `T_B(f)=C(f+R_B)` 的伴随为

```text
T_B*(q) = R_B + C(q).
```

为把闭包位置写清楚，先定义 `F_T(v)` 为“闭包后的正向状态 `A_T(v)` 已经得到时，从同一根继续到终端的原始后缀”，再定义保存行 `H_T=C(F_T)`：

```text
F_T = min(
    G_T,
    min over U strict-superset T, |U|<=a of R_(U-T) + H_U),
H_T = C(F_T).
```

源码没有单独保存 `F_T`：它先把上式的 `F_T` 候选写入工作数组，执行一次 Dijkstra 闭包后才保存 `H_T`。高层 mask 按大小递减处理，因此所有 `H_U` 已经存在。这里枚举的是**每条真实依赖边**，包括一次加入多个组、直接跨过切分的大 block；不能只反向 singleton 或相邻层，否则会漏掉原 A 的合法推导。

对每个低层 `S`，原流程已经结算直接终端 `G_S`。所有曾进入高层的路径则由下式结算：

```text
min over T strict-superset S, c<|T|<=a, v
    A(S,v) + R_(T-S)(v) + H_T(v).
```

`S=0` 时直接读取锚组距离。实现仍以 sparse、dense 或 ranked-bitmap 有序行相交，不使用 Hash。

## 4. 完备性结论

把原 A 计算看成一个 min-plus DAG：节点是 anchored mask，边 `S -> T` 的算子是 `T_(T-S)`，每个节点都有终端 `G_T`。任意完整推导要么在切分以下结束，这部分由原 completion 结算；要么存在唯一的第一条跨切分边。将该边之后的所有算子按相反顺序换成伴随，原始同根后缀形成 `F`，跨过进入当前状态的闭包后形成 `H=C(F)`；再与边之前的正向 `A(S)` 做一次内积，值保持不变。反过来，每条反向边都对应原 DAG 中的一条正向边，因此不会产生原算法不存在的推导。完整的源码对齐证明见 `test152_core_correctness.md`。

**结论：只要反向覆盖所有跨切分和高层依赖边，任意切分 `0<=c<=a` 都与完整正向 A 格等价。** 独立小图探针对每个可能切分都做了比较；生产实现另外保留 root-irreducible branch 语义，并由黑盒 DPBF 对拍验证。

## 5. Anchor-aware 前缀下界

只用“根到已纳入组的最远距离”约束 `H_T` 会产生大量没有用的反向值。Test142 对 `anchor union T` 使用与原 A 对剩余组同源的三个安全下界：

```text
P_T(v) = max(
    farthest distance to anchor union T,
    group-tour lower bound at v,
    directed-cut potential of anchor union T at v).
```

三项都不超过 rooted anchored cost `A(T,v)`。若 `P_T(v)+H_T(v)>best`，任何经过 `(T,v)` 的完整解都不能改善 incumbent，因此该值可删除。若一个被删的原始反向源还想经最短路传播到别的根 `u`，闭包性质给出 `A(T,v)<=A(T,u)+dist(u,v)`，同一拒绝仍然成立。实现按 `<mask,vertex>` 懒缓存 `P_T(v)`，没有经验阈值。

这一步不是常数优化。没有该前缀界的早期实现虽然正确，但 fast20 为 `44.381s`，反向值明显膨胀；加入对称 anchor-aware 界后，同一面板降至 `7.961s`。因此有效机制是**伴随消维与已纳入组下界的组合**。

## 6. 复杂度与存储

设 `N_H=sum_{i=c+1}^a C(k,i)`。忽略已有 ordinary D 和低层 A 的成本，所有 `T subset U` 关系与所有 `T/L/R` completion 分配合计均在 `O(3^k)` 个 mask 关系内。若行最坏为全顶点稠密，则候选阶段满足

```text
time  O(3^k n + N_H (m + n log n)),
space O(N_H n).
```

实际实现只保存通过 incumbent 下界的有序值，行表示按精确字节选择 sparse、dense 或 ranked-bitmap。平衡切分同时限制正向与反向依赖深度，但它不是“最坏空间一定减半”的声明；实际收益必须报告正反向值数、pops、row bytes 与 wall。

## 7. 当前验证

所有结果使用 Release/O2。

| 门禁 | 结果 |
| --- | --- |
| 独立伴随格探针 | Test96 `71`；随机 `500/500`；固定 g13 `100/100`；固定 g15 `10/10`；所有切分一致 |
| 生产黑盒 DPBF | 宽范围 `100/100`；固定 g13 `20/20`；固定 g15 `10/10`；包含零权边 |
| Toronto 默认查询 | `160/160` 逐条一致，最大绝对误差约 `1.0e-10` |
| fast20 | Test121 `8.128s`，Test142 `7.961s`，改善 `2.05%`；20 条权重逐项一致 |
| Toronto g13 q1--q5 | `50.369s -> 49.407s`，改善 `1.91%`；最大 query peak `146.281 -> 140.988MiB` |
| DBLP g13 q5 | `159.977s -> 143.363s`，改善 `10.38%`；peak `2150.184 -> 2149.660MiB` |
| DBLP g15 q33 | `11505.101s -> 9058.212s`，改善 `21.27%`；peak `19228.398 -> 14342.977MiB`；答案保持 `8.3053688653` |

DBLP q5 中 ordinary D 基本不变：`76.473s -> 76.449s`。Test121 anchored 阶段为 `30.806s`、保留 `3.999M` 个 A 值；Test142 只正向生成到 A2，再用 `1507` 张反向行保存 `28,687` 个值，anchored 阶段降到 `14.833s`，其中 adjoint 为 `8.347s`。peak 几乎不变，因为该查询仍由共同 ordinary D 行主导。

DBLP g15 q33 的 solver 内部分解为 ordinary D `6137.372s`、anchored 总阶段 `2846.594s`、单独 completion `122.986s`。低层 A0--A3 合计 `762.562s`；高层伴随阶段为 `2083.777s`，生成 `6006` 张反向行、`6.774M` 个值和 `9.576M` 次 pops，仅占 `77.52MiB` 行存储。**本次没有达到 8,000 秒目标**：实际超出 `1058.212s`，即 `13.23%`。不过它相对 Test107 减少 `2446.889s`，并把峰值 RSS 降低 `25.41%`，因此伴随消维本身仍是有效主线。

长门同时暴露了新的主瓶颈：伴随阶段虽然只保留 `6.774M` 个值，却执行 `54.808B` 次 join checks，另有 `144.555M` 次边界检查。也就是说，空间和值数已被消掉，但 `G_T=D(L)+D(R)` 终端以及高层反向边仍按大量组合逐根枚举。下一步应寻找保持完整 A DAG 语义的批量组合消除；只调 cut、行表示或循环常数不足以补回约 `1058s`。

Test145 已沿这个瓶颈继续：利用 directed-cut 势的组可加性，把目标相关的 `G_T` 下界改写成固定根上的统一约化预算，再一次生成全部高层终端。它在同一 q33 上得到 `7594.956s / 14451.762MiB`，达到 8,000 秒目标。Test142 本文继续作为状态定义、伴随证明和 q33 对照；后继机制、`O(3^k)` 有界枚举及新门禁只维护在 `test145_directed_cut_transposed_terminal.md`，不回写成 Test142 自身能力。

fast20 中并非逐条改善，例如 MovieLens g12 回退 `11.34%`；因此当前只确认机制在多面板合计和已测真实 DBLP 查询上为正，不声称普遍加速，也不会据 q33 单条压力样本估计整个 g15 查询集的均值。

## 8. 文献与原创性边界

[Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302) 是经典 Steiner rooted subset DP 来源；Test142 沿用其“同根合并后做图闭包”的计算内核，不把该内核称为原创。本文的候选贡献是：把 permanent-anchor A 的完整高层依赖图写成 min-plus 算子 DAG，以自伴随闭包反向求值，并用 `anchor union T` 的前缀证书使反向行可实际剪枝。尚未完成系统文献检索，因此当前只称“仓库内推导的论文候选机制”，不作新颖性结论。

实现入口为 `gst_test142_adjoint_anchor_main`；独立核验工具位于 `tools/adjoint_anchor_probe/`。ReleaseV4 不调用 Test142。
