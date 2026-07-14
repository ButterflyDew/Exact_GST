# Paid-Attachment Representative Family：一般规模边界

更新时间：2026-07-12。本文形式化 Test31--32 暴露的状态缺口，并说明为什么“同一 `A(S,v)` 多保留几棵 witness”在一般图上没有小规模保证。它是否决边界，不是否定所有隐式表示。

## 1. 标量 A 状态丢失了什么

当前状态只保存：

```text
A(S,v) = 覆盖 anchor 与 S、并包含 root v 的最小成本。
```

两棵 partial trees 即使覆盖相同 groups、具有相同 root，也可能对未来 attachment 不可比：

- `T1` 更便宜，但只包含一条短主干；
- `T2` 稍贵，却已经支付了通往若干内部顶点的边；
- 后续分支若接到这些内部顶点，`T2` 可以免费改 attachment，`T1` 必须重新支付路径。

因此仅按 cost 保留最小树不满足未来扩展所需的 dominance。

## 2. 一个充分支配关系

令 `P(T)` 为 partial tree 已经支付的顶点集合，更一般地可用距离轮廓

```text
delta_T(x) = min distance(x,y), y in P(T)
```

表示未来顶点接入该树的最小额外路径成本。一个直接且安全的支配条件是：

```text
cost(T1) <= cost(T2)
and
delta_T1(x) <= delta_T2(x) for every graph vertex x.
```

更强但更容易检查的充分条件是 `cost(T1)<=cost(T2)` 且 `P(T1)` 包含 `P(T2)`。不满足这些条件不代表一定不可支配，但任何通用代表族至少要区分可能被未来 query group 访问的不同 attachment 轮廓。

## 3. 指数反链构造

在固定 anchor `a` 与固定 root `v` 之间串联 `r` 个独立 diamond。第 `i` 个 diamond 有两条连接同一对端点的路径：

```text
cheap bypass                     cost c_i
detour through attachment p_i    cost c_i + epsilon_i, epsilon_i > 0
```

对每个子集 `X subset {1,...,r}`，选择恰好经过 `p_i, i in X` 的 detour，其余选择 bypass，得到 partial tree `T_X`。所有 `T_X`：

- 覆盖同一 anchor/group mask；
- 具有同一 root `v`；
- 成本为公共常数加 `sum(i in X) epsilon_i`；
- 已支付 attachment 集恰好额外包含 `{p_i : i in X}`。

若 `X` 严格包含 `Y`，则 `T_X` attachment 更多但成本严格更高；若二者互不包含，则各自拥有对方没有的 attachment。故任意两个 `T_X,T_Y` 都不满足“成本不高且 attachment 不少”的支配关系，形成大小 `2^r` 的 Pareto 反链。

再为每个 `p_i` 设置一个可能的未来 singleton group。包含 `p_i` 的 partial tree 对该组增量为零；走 bypass 的树必须支付正距离接入。这说明不同 attachment 不是无关装饰，而能被合法未来 query 区分。

## 4. 对 Test21 的含义

这个构造排除了以下一般性主张：

- 每个 `(S,v)` 只保留常数棵或多项式棵 witness 即可精确；
- 用单棵 predecessor tree、第二 root 或固定数量 attachment points 修复 Test31 的反例；
- 显式维护所有 cost/attachment Pareto 项仍能保持当前 `O(Pn)` row 空间。

它不排除：

- 利用当前 query groups 而不是全部图点做更小的等价签名；
- 用代数、位集或共享 DAG 隐式表示指数族，而不逐项物化；
- 对具有有界边界、树宽或其它可证明结构的实例做代表集压缩；
- 直接改变分解，使未来 attachment 不再需要记住整条 paid trunk。

下一候选必须明确回答“指数反链被怎样隐式共享或由哪条结构定理消掉”，不能只报告平均 witness 数较小。

## 5. 与代表集文献的边界

Fafianie、Bodlaender 与 Nederlof 的 rank-based representative-set 路线可在 tree decomposition 的有界 bag 上压缩 Steiner Tree 的 connectivity partitions；其规模由边界宽度控制，而不是由整张一般图的所有潜在 attachment 控制：

- [Fafianie, Bodlaender, Nederlof, *Speeding-up Dynamic Programming with Representative Sets*](https://arxiv.org/abs/1305.7448)
- [Bodlaender, Cygan, Kratsch, Nederlof, *Deterministic Single Exponential Time Algorithms for Connectivity Problems Parameterized by Treewidth*](https://arxiv.org/abs/1211.1505)

Test21 当前没有 bounded-treewidth decomposition；paid attachment 可以分布在全图。因此不能直接引用 rank-based reduction 来声称每张 A row 存在小代表族。若后续引入新的有界边界，必须同时核算构造边界的时间与 `n` 因子，并说明与 Fuchs 等人的 separator-terminal 路线区别。

