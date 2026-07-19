# Test96：Bounded-D4 Full-A 重写反例

更新时间：2026-07-15。Test96 尝试把 Test95 的 3/4 pendant tree decomposition 全局共享成永久-anchor recurrence：为每个 covered mask 只保留一条 `A(mask,v)`，普通侧只允许 `|D-side|<=4`，从而希望删除 D5/D6。g13 双候选组给出精确反例，该重写已从探针主路径删除，没有接入 Test80。

## 1. 被否决的等价式

候选 recurrence 为：

```text
A(empty) = rooted anchor row
A(M) = Close(min over nonempty Y subset M, |Y|<=4 of A(M-Y) + D(Y)).
```

直觉是把每个 `SoftBlockTransform` 的局部中间 rows 按全局 covered mask 合并。若成立，D 只需保留 1--4 层，A 再扩展到 full lattice。

## 2. 精确反例

Release/O2 探针命令使用 seed `715613`、fixed g13、双候选组；iteration 3 得到：

```text
exact=71
unrestricted_core3_soft=71
fixed_group0_soft=73
fixed_group0_bounded_D4_A=73
fixed_group0_unrestricted_A=71
best_bounded_D4_over_all_anchors=71
```

图边为：

```text
(1,0,20) (2,0,4) (3,0,17) (4,1,18)
(5,1,17) (6,2,10) (7,4,20) (8,7,3)
(9,4,4) (10,5,13) (11,2,13) (12,3,19)
(12,4,12) (9,7,2) (12,0,18) (7,10,15)
```

组候选为：

```text
g0={5,3}   g1={9,8}   g2={8,5}   g3={3,9}
g4={7,12}  g5={1,7}   g6={0,4}   g7={10,11}
g8={2,6}   g9={12,10} g10={11,1} g11={6,0}
g12={4,2}
```

最优边集是 `(9,7,2),(7,10,15),(10,5,13),(5,1,17),(1,0,20),(0,2,4)`，总成本 71。它是一条路径，多个 block 的合法 attachment points 分布在路径不同位置。

按 `core={g0,g10}`、中间 3-block、右侧 4-block、左侧 4-block 的树分解顺序逐次执行标量 transform，row 最小值为 `30 -> 71 -> 87`；真实 paid tree 在第二步只需 `54`。这直接显示第二个 block 连接到已付 core 的另一内部点时，标量重根重复支付了路径。

## 3. 原因与结论

一条标量 rooted row 不记录 paid tree 的内部顶点集合。连续加入 blocks 时，后续 attachment 可能已经位于 paid path 内，但 `Close` 只能把它视作从当前 root 新走一段路径，造成重复计费。允许更大的普通 D-side 后，unrestricted A 可以在一次分支合并中恢复所需结构；换另一个 anchor 在该实例上也可绕开 gap，但这不能证明仓库按既定 permanent anchor 选择时安全。

因此 `3/4 tree decomposition => fixed-anchor D<=4` 的推理无效。后续不能删除 D5/D6，除非新状态显式或隐式保留可免费重根的 attachment profile。
