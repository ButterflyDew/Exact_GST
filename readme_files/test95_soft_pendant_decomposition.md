# Test95：3/4 Pendant 树分解与 Soft 接口障碍

更新时间：2026-07-15。Test95 得到一个有效的树结构定理，同时找到了把它直接写成永久-anchor 标量 DP 时缺失的信息。准确结论如下：

1. **树分解成立。** 任意已选定 group witnesses 的可行树都能剥离成一个至多含 3 个标号叶的 core，以及若干大小为 3 或 4 的单接口 pendant blocks。
2. **现有标量 `SoftBlockTransform` 不能因此宣称完备。** 连续 blocks 可能连接到已付树的不同内部点；一条 rooted cost row 不记录哪些内部点已经免费存在，重新定位接口时会重复支付路径。
3. **固定 permanent anchor 的 bounded-D4/full-A 重写有精确反例。** g13 双候选组实例中 exact 与不固定 core 的 soft family 都为 `71`，固定 group 0 的 soft family和 D4-bounded A 都为 `73`；unrestricted A 仍为 `71`。

因此 Test95 不是可直接接入 Test80 的新 solver，而是把开放问题从“有没有 3/4 分解”收窄为“如何无损保存已付树的可重用 attachment profile”。

## 1. 标号树规范化

取一棵可行 GST 树，并为每个查询组选择一个已覆盖的 witness 顶点。若同一顶点满足多个组，就给它分配多个独立标号。把每个标号改成一片零长叶，并用零长边接回 witness；再把高于三度的内部点用零长边二叉化，压缩无标号的度 1/2 点。所有变换只用于证明，不改变原解代价，也不要求修改输入图。

规范化后得到一棵内部度至多 3、每个组标号对应一片叶子的树。组重叠和零权边都可以这样处理。

## 2. 3/4 Pendant Lemma

当标号叶数大于 4 时，任取一片叶定根，选择一个最深、且后代至少含 3 片标号叶的节点 `x`。`x` 的每个孩子至多含 2 片叶，否则更深的孩子也满足选择条件。二叉化后 `x` 至多有两个孩子，所以

```text
3 <= descendant_leaves(x) <= 4.
```

`x` 的后代子树通过一条父边与余树连接，因而是一个大小为 3 或 4 的单接口 pendant block。删除它、压缩新产生的度 1/2 点并重复。只剩 4 片叶时，可以把 3/4 片作为最后一个 block，留下不超过 3 片叶的 core；也可以把全部 4 片作为 block，留下空 core。

**结构结论：每棵标号树都存在 `core<=3 + 若干 3/4 pendant blocks` 的剥离顺序。** 这是 Test95 保留的已证明结果。

## 3. 为什么标量 Soft Row 仍不够

已有 `SoftBlockTransform(B,h)` 把输入 row `h(v)` 当作一个 soft terminal，并在 block `B` 内运行局部 subset DP：

```text
F(empty) = Close(h)
F(X) = Close(min over nonempty Y subset X of D(Y) + F(X-Y)).
```

对**单个** block，这能精确定价“已有树通过一个给定接口连接 B”。而且变换保持逐点最小：

```text
T_B(min(h1,h2)) = min(T_B(h1),T_B(h2)).
```

问题出现在连续 blocks。剥离顺序只保证每个 block 对当时余树有一个接口，并不保证所有逆序加入的 blocks 都连接到同一个接口或沿一条嵌套链。后一个 block 可能连接到已经付费树的另一个内部点 `u`。标量 row 只保存“树以当前根 `v` 表示时的最小代价”，没有保存 `u` 是否已经属于该树；`Close(h)` 从 `v` 移到 `u` 时可能再次支付一段已在树内的路径。

所以 pointwise-min 线性只能合并**已经由同一标量状态语义表示的候选**，不能恢复被状态定义丢掉的 paid geometry。缺少的是第二接口或等价的 attachment profile，而不是 block 顺序枚举本身。

## 4. g13 固定 Anchor 反例

seed `715613` 的双候选组模式在 iteration 3 给出：

```text
exact                      71
core<=3 unrestricted soft  71
fixed-group-0 soft         73
fixed-group-0 bounded D4 A 73
fixed-group-0 unrestricted A 71
best bounded D4 over anchors 71
```

最优解是以下代价 71 的路径：

```text
9 --2-- 7 --15-- 10 --13-- 5 --17-- 1 --20-- 0 --4-- 2
```

路径不同位置各承载两个左右的组标号。左、右 4-blocks 和中间 3-block 的 attachment points 不相同。树上确实存在第 2 节的 3/4 分解，但固定 group 0 的标量 soft 状态无法免费复用所有内部连接点；允许 D 的任意大小后，原 permanent-anchor A 回到 exact，证明 gap 不是 anchor 框架或实现精度问题。

完整边、组候选和诊断见 `archive/test96_bounded_d4_full_a_20260715.md`。

## 5. 当前探针事实

`tools/paid_half_state_probe` 现把 unrestricted soft family 的 core 正确收紧到 3，外层仍用 covered-mask subset DAG 对所有 core 与 block 顺序逐点取最小。Release/O2 当前通过 g9 singleton `200/200`（seed `715621`）、g13 singleton `20/20`（seed `715622`）和 g13 双候选组 `20/20`（seed `715623`）。

这些数字只说明尚未找到 unrestricted family 的反例。固定-anchor 反例已经证明，**不能再用这些随机结果声称一般完备，也不能把 unrestricted family 等同于 Test80 的 permanent-anchor 状态。**

## 6. 对后续状态定义的要求

树分解引理说明每一步只新增 3/4 个组，因此局部 subset 维度可以被限制；反例说明累计状态必须额外回答：

```text
给定候选 attachment u，u 是否已在 paid tree 中；若在，重根到 u 的边际代价是多少？
```

显式保存两个图顶点会恢复 `O(n^2)` 维度，不可接受。下一条理论主线应寻找可合并的 attachment profile、paid subtree 的小型 Pareto basis，或能证明 attachment points 形成受限集合的结构证书。此前 pair/half-profile 与 soft-terminal 探针正是在逼近这个信息边界。

在得到无损且次二次的 profile 表示前，不接入 Test80、不修改 ReleaseV4，也不启动 DBLP 长跑。

## 7. 论文边界

本轮没有新增外部论文引用。局部 recurrence 与 Dreyfus--Wagner/Steiner subset DP 同源；3/4 pendant tree lemma 及固定-anchor scalar-state barrier 是本项目当前推导。形成论文前仍需做系统新颖性检索，并严格区分：**树结构可分解**不等于**标量动态规划状态完备**。
