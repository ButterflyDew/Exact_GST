# Test76: First-Order Block-Anchor Factorization

更新时间：2026-07-13。Test76 研究的是一个严格限定的问题：给定两个已经选定的完成块
`B/C` 和四组 core `Q`，怎样精确定价这个 fixed plan，同时不生成 D3/D4 block-anchor
rows。它不是 Test21 的新正式路径，也没有运行 full DBLP g13。

## 1. 状态与结论

记：

```text
D(S,v)     = 连接组集 S、以 v 为接口的精确 rooted GST cost
A_B(K,v)   = 把整个块 B 作为永久 macro anchor，再连接 core 子集 K 的精确 cost
C(f)(v)    = min_x f(x) + dist(x,v)
```

完整六标签 macro DP 把 `B/C` 与四个 core groups 当作六个标签。Test76 证明其 fixed-plan
最优值等于：

```text
min A_B(J,v) + A_C(K,v) + D(R,v)
 v,J,K,R

J, K, R 两两不交，J union K union R = Q
|J| <= 1, |K| <= 2, R 非空
```

因此固定方向下只需：

```text
B side: D0/D1 block-anchor rows
C side: D0/D1/D2 block-anchor rows
ordinary side: 已有的 D1--D4 rows
```

不需要 D3/D4 block-anchor row，也不需要 Hash。

## 2. 为什么公式完备

在任意最优 macro tree 中取 `B` 到 `C` 的唯一树路径。把每个 core 标签所在子树投影到
这条路径，并从 `B` 端向 `C` 端扫描。选择“累计遇到至少两个 core 标签”的第一个路径
点 `v`。

- `v` 的 `B` 侧严格少于两个 core 标签，所以得到 `|J|<=1`；
- `v` 的 `C` 侧至多剩两个 core 标签，所以得到 `|K|<=2`；
- 使累计数第一次达到二的标签位于 `v` 的挂接分支，故 `R` 非空。

三部分只在 `v` 相交。分别替换成精确的 `A_B(J,v)`、`A_C(K,v)` 和 `D(R,v)` 不会
增加成本；反向把三棵 rooted tree 在 `v` 合并又总是合法。因此该式与完整六标签 macro
DP 等价。证明不读取数据集名、`g` 特判、密度或运行时刻。

## 3. 实现

研究入口：

```text
tools/paid_half_state_probe/paid_half_state_probe.cpp
tools/paid_core_growth_probe/paid_core_growth_probe.cpp
```

`paid_half_state_probe` 用完整六标签 DP 和显式连通边子集枚举作双重 oracle。
`paid_core_growth_probe --price-first-order-anchor-plans` 的共享-row 原型执行：

1. 按 plan 的两个 block mask 作确定性方向选择；
2. 收集全部必需 `(block, core-subset)` key；
3. 排序、去重后依次构造 D1、D2 block-anchor rows；
4. 用二分查找读取依赖 row；
5. 离线扫描三函数和，给每个候选 plan 精确定价。

整个接口只使用有序数组、连续 dense rows 和 `priority_queue`，没有 Hash 或经验门槛。

## 4. 正确性验证

Release/O2：

```text
singleton groups       seed 714511   500/500
two-candidate groups   seed 714521   200/200
```

每轮都同时比较：显式连通边子集最优值、完整六标签 DP、全量 block-anchor DP，以及只保留
D1/D2 anchor rows 的一阶公式；误差为零。

MovieLens-fast g12 的唯一 strong-lower plan 仍得到：

```text
first-order fixed-plan price   0.0202203613
known exact                    0.0202189774
```

这与此前完整 macro DP 一致，说明缺口不在 block-anchor row 截断，而在 fixed-plan family
本身。

## 5. Fast 边界

下面的 plan 集只由合法的 `three_block.upper` 筛选，不使用 known exact 作为算法入口：

| dataset | candidate plans | D1 rows | D2 rows | build | price | family best | exact |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto g12 | 1,269 | 1,063 | 2,049 | 2.955s | 0.263s | 0.9688685500 | 0.9616227800 |
| DBLP g12 | 337 | 505 | 769 | 1.461s | 0.068s | 12.1663030000 | 12.1663030000 |

Toronto 给出决定性否决：定价器对每个 fixed plan 都是精确的，但“两块 + core4”plan family
并不覆盖所有最优树。DBLP 虽在 rank 11 命中 exact，额外约 1.53 秒也远慢于 ReleaseV3
该条约 0.206 秒。故该 dense 原型不能接入 Test21。

## 6. 保留与撤出

保留：

- fixed-plan 一阶 block-anchor 分解定理；
- 无 Hash 的排序 key 与共享 D1/D2 row 原型；
- MovieLens 边界和 Toronto family 不完备反例。

撤出正式主线：

- 把 core4 两块 family 当作完整 solver；
- 用已知 exact 筛 plan；
- 为该 family 运行 full DBLP g13。

下一条结构方向是先证明完备的“三个单接口块 + 小 core”树分解，再研究其共享状态，而不是
继续优化这个不完备 family 的扫描常数。

## 7. 论文关系

本轮没有引入新的论文算法。metric closure 与 rooted subset DP 的背景仍属于既有
Dreyfus--Wagner 系列；这里的一阶路径分解是仓库内推导，目前只作为研究候选，不宣称
论文级原创性。
