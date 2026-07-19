# D2 Symbolic Convolution 与 Residual Quotient 边界

更新时间：2026-07-15。本文记录 Test49--50 之后两个无需新长跑即可判定的边界：一般实权 min-sum subset convolution 不能直接套用经典快速 Möbius 变换；full DBLP 的零 residual 子图也不足以形成数量级 quotient。它们是后续 D2 隐式表示研究的约束，不是新 solver 结果。

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

## 3. 同一边界也约束 A+D+D Completion

Test80 对固定 A mask `S` 和 root `v` 枚举剩余组集 `U` 的无序分割 `L,R`，计算

```text
A(S,v) + min over L subset U:
           D(L,v) + D(U-L,v)。
```

若对每个 root 定义 `d_v(T)=D(T,v)`，内层最小值就是 `d_v` 与自身的 exact min-sum subset convolution。把所有 A masks 一起展开，则每个 nonanchor group 被分到 A、左 D、右 D 三类，正好对应当前保守 `O(3^(g-1)n)` completion 枚举。先构造 `F(U,v)=min_L D(L,v)+D(U-L,v)` 再与 A 相交，只是改变循环顺序；在没有更强结构时，它仍需处理同一批 min-sum 候选，还会物化新的 `F` rows。

full DBLP g13 q25 的旧统计有 `37.790B` completion scans 和 `33.384B` exact checks；Test98 当前主线重跑后仍有 `37.155B/32.815B`，所以这里确实是主导工作，不只是符号上的最坏界。然而第 2 节的文献边界仍然适用：当前 `double` 权值不能使用带 `M` 因子的有界整数 exact embedding，`(1+epsilon)` convolution 又不能替代精确求解。通用 dense transform 也可直接用于 baseline，不应作为框架 A 的原创贡献。

Test104 后来只解决其中的物理相交部分：当 A roots 与两张多组 D 行都已有 bitmap 时，以 word 三路相交替代逐顶点成员查询。它保持 partition 数和 exact checks 不变；当时由旧布局严格推出 q25 至少有 `41,265` 个兼容 partitions。随后 Test105–107 的分块、根、分量三级最小值证书实际把当前组合版 q25 的 checks 从 `32.815B` 降到 `46.019M`，但 scans 仍有 `32.598B`，completion 仍需 `488.765s`。因此 checks 障碍已经大幅缓解，剩余 min-sum convolution 障碍更准确地表现为大规模共同根扫描和状态物化，而非最终加法本身。

因此下一 completion 机制必须利用至少一种额外结构：A 侧唯一 permanent anchor、D 侧不可拆分 branch、已经存在的稀疏有序 root 支撑，或可证明的 partition dominance。仅把 `A+D+D` 写成 subset convolution、预先生成完整 `F(U,v)`、缩放 double 后做 Möbius transform，均不构成可执行的新主线。

### 3.1 与 DB 方向 DPconv 的关系

[Stoian 与 Kipf 的 DPconv](https://arxiv.org/abs/2409.08013)（Proc. ACM Manag. Data 2024 / SIGMOD 2025，[DOI](https://doi.org/10.1145/3698809)）已经把 fast subset convolution 系统地用于 join ordering。它对 `Cmax` 给出 `O(2^g g^3)` 精确算法：选定阈值 `gamma` 后，将 DP entry 变为布尔可行性 `[cost<=gamma]`，在普通环中做 layered subset convolution，再对候选阈值二分。对加法型 `Cout`，其 exact 路线仍通过 polynomial embedding，复杂度依赖最大整数 cardinality 值域；一般值域则转向近似算法。

这两条精确路线都不能直接替换本仓库的 completion。`A(S,v)+D(L,v)+D(R,v)<B` 是三个实数代价的**和阈值**，不能像 `Cmax<=gamma` 那样拆成三张独立布尔表；图权和 DP 值又是 `double`，没有已证明的多项式小整数值域可供 embedding。DPconv 的 layered zeta 缓存依赖 ring transform，也不能原样搬到 tropical semiring。

DPconv 在结论中把 sparse subset convolution 列为稀疏 query graph 的未来方向，这与本仓库的稀疏 row 现象相呼应，但二者的稀疏维度不同：DPconv 关注可行 subset masks，本仓库 q25 的主要稀疏性位于每个 mask 的 root support。若后续借鉴该方向，必须给出同时利用 permanent anchor 和 root support 的 exact 稀疏卷积接口；仅引用 DPconv 或复制 layered FSC 不能构成当前方法。

## 4. Zero-Residual Quotient 的硬上限

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

## 5. 后续允许的结构

仍未被排除的是：

1. pair factor 在 query/anchor skeleton 下出现可证明的低 tropical rank；
2. 一个对象同时编码多个 pair、root 与 consumer masks，但不执行一般 min-sum convolution；
3. 改变状态定义，使 pair extension targets 不再存在；
4. 有明确边界宽度的第二 attachment certificate，而不是一般图上的显式顶点对。
5. completion partitions 在 permanent-anchor/branch 语义下出现可证明的 dominance 或共享证书，而不是通用 dense min-sum transform。

下一候选必须明确利用上述额外结构之一。仅做 FFT/Möbius、权值缩放、zero-SCC contraction、bitset 排序或 event 去重均不构成新主线。
