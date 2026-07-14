# Skeleton-Conditioned Pair：精确标量化边界

更新时间：2026-07-13。本文整理 Test44--66 反复触及但容易混淆的边界：固定一棵已付费 skeleton 后，ordinary pair attachment 确实可以精确消掉 root 维度；困难不在 pair 标量本身，而在如何用受控 family 表示最优树可能拥有的 paid skeleton。本文是后续机制的验收条件，不是当前 Test21 的运行入口。

## 1. 固定 Paid Skeleton

令 `P` 是一个已付费的连通子图，并把它的全部顶点视为一个零代价 group。记：

```text
d_P(x) = min distance(x,p), p in V(P).
```

对两个 query groups `i,j`，定义：

```text
E_P(i,j) = min_x [d_P(x) + gd_i(x) + gd_j(x)].
```

把 `P`、`i`、`j` 看成三个 groups。至多三个 groups 的 GST 等于 goal-root-star minimum，因此 `E_P(i,j)` 正是“在 `P` 已付费后，把 groups i,j 连到 P”的精确最小附加成本。它不是近似，也不需要保存 attachment root。

所以固定 `P` 后，原来一整张：

```text
D({i,j},v), v in V
```

对“最终直接接入 P”这一消费方式可以压成一个 scalar `E_P(i,j)`。计算全部 pairs 只需一次 `d_P` 与 `O(C(k,2)n)` 顺序扫描，适合离线有序 arrays，不需要 Hash。

## 2. 为什么这还不能替代 Test21 D2

Test21 的 D2 不只被最终 backbone 读取。它还会先接到某个 ordinary/A accumulator，随后新的分支可能接到这条 pair path 的内部顶点。若提前固定 `P`，等价于要求 pair component 直接连到 `P`；一般最优树的真实 paid trunk 未必包含该 `P`。

因此：

```text
fixed P + scalar pair attachments
```

精确求解的是“最优树包含 P”的条件问题。对无条件 GST，它只给一棵合法可行树 upper。Test44--48 与 Test66 的 scalar/facility constructions 都处在这一侧。

Test31--32 的反例解释了不能让单个标量 `A(S,v)` 动态充当未知 `P`：更贵但包含更多内部 attachment 的 partial tree 会被更便宜的标量值删除。`paid_attachment_representative_family.md` 又给出一般图上的 `2^r` Pareto 反链，排除了每个 `(S,v)` 只显式保留常数或多项式棵 skeleton 的通用主张。

## 3. 与 Branch-Junction 的关系

Test48 在一个固定 `P` 上加入多个 facility roots，并把它们到 `P` 的 parent-path union 压成小树。该树 DP 让不同 attachments 的公共路径前缀只付费一次，所以比独立 `sum E_P(S)` 更强；但它仍然条件于这一棵 parent skeleton，只提供 upper。

Test49 的 pair predecessor arc-DAG 则是无条件 exact 表示：它保留每张 D2 的 split-to-attachment paths，并能把单 consumer frontier 压到 full pair states 的 `8.416%`。失败发生在 consumer masks 展开，而不是 pair path 语义。

Test69 进一步按 `(split root,attachment)` 聚合 pair labels，并利用 `gd_i(x)+gd_j(x)` 的 rank-1 结构把固定 profile 的 mask join 化成两次 singleton transform；但 full 仍有 `53.17M` profiles，只比 `106.31M` pair states 小约 2 倍。它说明“共享 pair 数值”与“让每个共享对象承担 mask transform”仍是两种不同规模要求。

两者的差别是：

```text
Test48: mask sharing 强，skeleton 只是一棵条件 upper
Test49: pair path exact，consumer-mask sharing 弱
```

下一突破必须把这两种共享放在同一表示里，不能继续只增强其中一侧。

## 4. 后续候选的必要证明

任何声称“用 skeleton 消掉 D2”的新状态都必须逐项回答：

1. **覆盖性。** 为什么至少一棵被表示的 skeleton 属于某棵最优 GST，而不是只产生可行 upper？
2. **共享规模。** skeleton family 如何隐式共享，为什么不落回 paid-attachment 的指数反链？
3. **第二边界。** split root 到 attachment root 的路径由什么对象保存，为什么不产生 `n^2`？
4. **mask 维。** pair bits 与 A/D consumer masks 如何同时共享，为什么不重现 Test49 的 events 或 Test50 的 membership probes？
5. **输出阶。** full DBLP 上实际产生的对象必须少于 `106M--125M` 个 rooted pair values；只压字节或换扫描顺序不算通过。

在这五点没有统一答案前，不再把固定 backbone、更多 anchors、更多 facility roots或更深 witness union接入正式 Test21。
