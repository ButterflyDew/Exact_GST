# Test146：按实际工作量摊销的锚树上界调度

更新时间：2026-07-17。Test146 是 Test145 的无参数调度后继，保留 ordinary D、permanent-anchor A、伴随高层 H、directed-cut 转置终端和离线有序行的全部定义。它只改变一个问题：**何时重新执行锚树设施上界**。ReleaseV4 与默认 Test80 均未改变。

## 1. 问题

Test121 的锚树设施上界会把当前已经完成的 ordinary D blocks 放到 junction 压缩锚树上组合，允许不同 blocks 共享锚树边。候选是一棵真实可行树，因此只能降低 incumbent，不会删除最优解。Test145 在每个 ordinary D 层结束后都重新执行它，这在重询问上可能提前得到更紧上界，但在 Toronto 一类轻询问上，树 DP 本身比随后能够节省的 ordinary 工作更贵。

五数据集全部 200 条 g15 D2 前缀给出了明确反例。Test145 在 Toronto 40 条询问上累计花费 `69.956s` 执行锚树上界；冻结五问中，eager tree 为 `174.909s`，完全关闭 tree 为 `134.433s`。因此“上界正确且偶尔有效”不足以支持逐层无条件执行。

## 2. 可计算的买入成本

设非锚组数为 `k=g-1`，压缩锚树有 `t` 个普通节点。一次锚树设施求值的循环边界完全由 `k` 与 `t` 决定，其实现工作量记为

```text
B = t * ((3^k - 1) / 2 + 3^k).
```

第一项覆盖树上不相交子集合并，第二项覆盖树边传播。`B` 在进入 ordinary 分层前即可精确计算，不使用运行时间、数据集名、固定 `g`、层号、密度或经验常数。

## 3. 哪些工作可以给上界“付费”

更紧 incumbent 只能避免尚未发生的 bound-sensitive 工作。对一个已经完成的 ordinary D 层，Test146 只累计：

```text
R += priority-queue pops + edge-relax attempts.
```

**seed candidate 枚举不计入 `R`。** 当前层的 merge candidates 在新上界出现前已经生成，事后收紧 incumbent 无法省掉它们。把这部分计入购买预算会高估潜在收益。

D1 直接来自组距离，不执行图闭包，所以其 `R=0`；同时 D1 可提供给锚树的内容与初始 junction 构造等价。因此 Test146 自然跳过 D1 tree evaluation，代码不需要“当层号为 1”之类的特判。

## 4. 调度规则

每个 ordinary D 层自然结束后执行以下规则：

```text
accumulate the layer's queue pops and relax attempts into R
if R >= B:
    evaluate the anchor-tree facility upper bound
    best = min(best, tree_upper)
    R = 0
```

这里比较的是**已经发生的可规避工作**与**下一次上界求值的精确工作量**。轻询问若始终没有积累到 `B`，就不会购买树 DP；重询问达到 `B` 后仍会获得原有上界。一次求值后清零 `R`，防止同一段 ordinary 工作重复为后续求值付费。

## 5. 正确性与复杂度

跳过一次锚树求值只意味着暂时保留较松的合法 incumbent。ordinary D、A、H 和 completion 的状态定义、转移与终止条件完全不变，因此不会漏掉可行解；执行求值时得到的仍是 Test121 已证明合法的可行树上界。由此 Test146 与 Test145 返回相同最优值。

设全部 ordinary 阶段累计的可收费工作为 `W`。除最后可能尚未摊销的一次 `B` 外，每次树求值都能向此前至少 `B` 的 ordinary 工作收费，因此树求值总工作为 `O(W+B)`。单次树 DP 的原始界仍为 `O(t*3^k)` 时间与 `O(t*2^k)` 工作空间；调度没有增加新的渐近状态空间。

## 6. 实验结论

所有运行均为 Release/O2。最终调度实现通过随机图与 DPBF 的 `100/100` 对拍（seed `146004`）。Toronto 冻结五问的答案逐项精确，五问均未购买 tree，完整 wall 为 `142.504s`；其状态轨迹与 no-tree 版本一致。作为直接消融，Test145 eager/no-tree 为 `174.909s/134.433s`，说明逐层 eager tree 在该面板确实是负优化。

用完整 200 条 D2 统计离线重放购买条件时，触发询问数分别为 Toronto `0/40`、DBLP `39/40`、DBpedia `39/40`、LinkedMDB `22/40`、MovieLens `11/40`。这不是数据集分支，而是同一工作量比较在不同实例上的自然结果。DBLP 轻面板中，q11 的 D2/D3 都会购买 tree，D3 prefix wall 为 `132.182s`，原 Test145 为 `138.485s`；q5 则到 D3 才购买。MovieLens 冻结五问的 D2/D3 状态与 Test145 逐项一致，总时间差处于机器波动范围。

因此 Test146 的保留结论是：**继续使用锚树上界，但只在已经发生的、理论上可能被更紧 incumbent 避免的工作足以覆盖其精确成本后求值。** 它修复了已有上界在轻询问上的跨数据集回退，没有引入第六条禁止的经验阈值。

## 7. 文献边界

Test146 继承 Test145 对 Dreyfus--Wagner rooted subset DP 的使用，来源见 [Dreyfus and Wagner, 1971](https://doi.org/10.1002/net.3230010302)。本调度只是对仓库内锚树 DP 与 ordinary 工作的精确摊销，不在本文中引用或主张新的外部算法结果；锚树设施上界的状态与证明见 `test121_anchor_tree_block_facility.md`，转置终端见 `test145_directed_cut_transposed_terminal.md`。
