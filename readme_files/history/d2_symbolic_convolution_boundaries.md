# D2 Symbolic Convolution 与 Residual Quotient 边界

更新时间：2026-07-12。本文记录 Test49--50 之后两个无需新长跑即可判定的边界：一般实权 min-sum subset convolution 不能直接套用经典快速 Möbius 变换；full DBLP 的零 residual 子图也不足以形成数量级 quotient。它们是后续 D2 隐式表示研究的约束，不是新 solver 结果。

## 1. Pair-Mask 接口对应 Min-Sum Subset Convolution

对某个 root/gradient arc，令 `A(S)` 为 closed consumer mask 的值，`P(R)` 为 pair mask 的值。所有 pair 扩展目标同时满足：

```text
F(T) = min over R subset T, |R|=2:
       A(T-R) + P(R).
```

Test49 把每个有效 `(S,R)` 展开成 event；Test50 保留 `(S,pair-bits)` 因子，由 target 测试 membership。二者分别产生约 `29M events` 与 `125M probes`。要在 mask 维真正共享，必须计算上式的 exact min-sum subset convolution，而不是更换 bitset 容器。

## 2. 为什么经典 Fast Subset Convolution 不能直接使用

[Björklund、Husfeldt、Kaski、Koivisto](https://arxiv.org/abs/cs/0611101) 的 fast subset convolution 在普通环上达到 `O(k^2 2^k)`；对 min-sum/max-sum semiring，论文通过 polynomial embedding 处理值域在 `[-M,M]` 的整数，复杂度包含 `O*(2^k M)`。

本仓库图权与中间 DP 值为 `double`，验收要求误差不超过 `1e-6`。把值统一缩放为整数既不能证明所有输入本来就在有限小整数格上，又会让 `M` 至少乘 `10^6`，不满足当前时间目标。环上的 Möbius inversion还包含加减法，不能在 tropical semiring 中原样执行。

[Stoian 的近似 min-sum subset convolution](https://arxiv.org/abs/2404.11364) 给出 strongly/weakly polynomial 的 `(1+epsilon)` 近似算法；其结果本身也把一般 min-sum convolution与环上 exact transform 区分开。Test21 是 exact solver，不能用近似 convolution 替换 recurrence。

因此后续不得仅以“fast subset convolution”为名把 Test49/50 的乘数写成 `O(k^2 2^k)`。若要使用该文献，必须先证明本实例存在小整数值域，或发现 pair factor 的额外可分结构；当前 `D_ij(v)=min_x(dist(v,x)+gd_i(x)+gd_j(x))` 的 facility minimum并不满足 singleton 可加分解。

## 3. Zero-Residual Quotient 的硬上限

Test41 已统计 full DBLP g13：

```text
n                    2,497,782
directed zero arcs     162,166
```

不论 SCC 如何分布，每次把一个非平凡零弧连通关系用于 contraction，顶点数最多减少一。因此最乐观 quotient 仍至少有：

```text
2,497,782 - 162,166 = 2,335,616 vertices
```

即顶点数最多减少 `6.49%`。实际只收缩 directed SCC 时不会优于这个上限。它无法把 `106M--125M` pair-root 输出缩小一个数量级，也不足以触发新的 full DBLP run。

zero-residual arcs 仍可作为 primal witness、upper oracle 或 lower-bound 证明使用；但不能把 SCC/zero-DAG contraction 当作 D2 状态消除。扩成 `residual<=epsilon` corridor 又需要数值阈值和额外正确性证明，违反当前无经验超参数边界。

## 4. 后续允许的结构

仍未被排除的是：

1. pair factor 在 query/anchor skeleton 下出现可证明的低 tropical rank；
2. 一个对象同时编码多个 pair、root 与 consumer masks，但不执行一般 min-sum convolution；
3. 改变状态定义，使 pair extension targets 不再存在；
4. 有明确边界宽度的第二 attachment certificate，而不是一般图上的显式顶点对。

下一候选必须明确利用上述额外结构之一。仅做 FFT/Möbius、权值缩放、zero-SCC contraction、bitset 排序或 event 去重均不构成新主线。
