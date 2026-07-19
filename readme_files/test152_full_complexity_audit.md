# Test152 完整实现复杂度与策略分支审计

更新时间：2026-07-18。本文逐项审计 `gst_test152_progressive_dual_main` 当前启用的核心状态、下界、可行上界、调度与数据布局，回答两个问题：**完整实现是否在隐藏模块中超过声明的组合复杂度，以及是否存在违反 `AGENT.md` 第六条的数据特判或经验超参数。** 核心正确性证明见 `test152_core_correctness.md`；本文不是 ReleaseV6 方法文档。

## 1. 当前构建实际启用什么

Test152 由 `CMakeLists.txt:298-307` 固定启用：

```text
permanent-anchor D/A,
root-irreducible branch basis,
low-A / high-F/H adjoint,
eager low/high boundary settlement,
directed-cut transposed terminal,
anchor-path scalar facility upper,
amortized anchor-tree facility upper,
progressive sequential dual.
```

普通阶段还固定执行 root-star、junction、三块 early upper、最多一次 early witness closure，以及在完整 dual 可用时最多一次 residual packing。研究宏只决定不同 Test 目标的编译路径；Test152 二进制本身没有运行时回退到 V3/B、旧 Release 或其他求解器。

## 2. 记号

令 `n=|V|`、`m=|E|`、`g` 为查询组数、`k=g-1`、`h=floor(g/2)`、`a=h-1`、`c=floor(a/2)`。令

```text
N_D = sum_(i=1..h) C(k,i),
N_A = sum_(i=0..c) C(k,i),
N_H = sum_(i=c+1..a) C(k,i),
N = N_D+N_A+N_H.
```

以二叉堆实现的一次多源最短路记为 `C_G=O((m+n)log n)`。令 `t<=n` 为 junction 压缩锚树普通节点数，`s=sum_i |T_i|` 为查询组输入总大小，`Q` 为实际保存的 `D/A/H` 根值数，`E_T` 为转置后保留的 `<target,root>` 终端事件数。

## 3. 核心状态成本

ordinary split、低层 `A` 吸收、高层 `F/H` strict-superset 反边、全部 completion 分割和转置组合都受 `O(3^k)` mask 关系约束。每个真实状态行最多执行一次图闭包。因此核心时间为

```text
T_core = O(n*3^k + N*C_G),
```

核心空间为

```text
S_core = O(m + g*n + 2^k + Q + E_T).
```

逐定理证明与两条转置路线的精确 probe 上界见 `test152_core_correctness.md`。Test157 已独立核验 standard/strict-`Br`、显式 `F/H`、每个 cut 与逐目标转置。

## 4. 预处理与下界

### 4.1 组距离、组间表与 root-star

- 每组一次多源 Dijkstra：`O(g*C_G)` 时间、`O(g*n)` 常驻距离空间。
- 组间最短距离表：源码对每个左组扫描所有右组候选，时间 `O(g*s)`、空间 `O(g^2)`。
- root-star 可行上界与锚点选择：`O(g*n)` 时间。锚点是在 root-star 根上距离最远的组，平局由组序确定，不读取数据集或查询编号。

### 4.2 Group-tour 下界

`TourLowerBound::Build` 为每个起点、subset 和末端做 Held--Karp 型路径表，时间

```text
O(g^3 * 2^g),
```

临时路径表与持久 endpoint 表空间均为 `O(g^2*2^g)`。一次 `At(v,S)` 最坏扫描 `O(|S|^2)` 个 endpoint；同一状态行和顶点通过 stamp 至多求值一次，所以全部 `D/A/H` 行的额外查询成本为 `O(g^2*n*N)`，没有乘到每条边松弛上。

该下界不要求组间最短距离满足三角不等式。任意 rooted 完成树加倍后给出一条访问所需组代表的 walk，而组间集合距离不超过该 walk 相邻代表间的真实距离；因此路径值的一半不超过完成树成本。它同时是 1-Lipschitz 的：两个 root-distance endpoint 之和是 2-Lipschitz，除以二后取最小仍保持一致性。

### 4.3 Progressive sequential dual

初始化 residual、changed-arc bitset 与固定组顺序需要 `O(m+g log g)`。每推进一组，最坏执行 changed-arc seeds、一次 residual Dijkstra、`n` 个势写入和 `m` 条边的 residual 更新；全部 `g` 组加最终 primal recovery 为

```text
O(g*C_G)
```

时间与 `O(g*n+m)` 空间。每个 partial prefix 都是合法 dual：尚未推进组取零势，已经推进组的总有向载荷受 residual capacity 约束。

源码以 `2m+n` 个 ordinary seed/pop/relax 计数推进下一组。**`2m+n` 只是 residual arc 与势向量的结构规模，不是一次推进实际时间的上界**：它没有覆盖 priority-queue push/pop 和 residual Dijkstra 的完整传播。因此该规则无经验参数、无数据分支且不改变最坏复杂度，但不能称为竞争性或严格摊销定理。论文中只能把它列为确定性的预处理调度，并在 M5 单独消融；若收益不稳定，最终方法应删除它，而不是把它包装成核心贡献。

### 4.4 Residual packing

packing 只在 full dual 完成后执行至多一次。它先为每组做一次 residual Dijkstra，再进行至多 `g` 轮 max-min filling；每轮扫描组和有向边。因此时间与临时空间为

```text
O(g*C_G + g^2*m),
O(g*n+m).
```

若 full dual 在可用窗口内没有完成，packing 不运行并释放 residual；这只保留较松下界。`1e-12` 与按边权缩放的 `1e-10` 仅用于浮点饱和判定，不参与数据集或状态策略选择，但最终发行前仍需数值边界测试。

## 5. 可行上界模块

### 5.1 Junction 与初始锚树

`anchor_junction::BuildUpper` 恢复一条 root-to-anchor 紧路径，执行一次以该路径为多源的 Dijkstra，扫描所有普通组三元组的候选汇合根，再在含 `t` 个节点的压缩父树上做 subset convolution。其时间为

```text
O(C_G + k^3*n + t*3^k),
```

临时空间为 `O(n+t*2^k)`。返回值由真实组最短路、锚路径和压缩树边组成，是 primal upper；该模块不改变图或查询。

### 5.2 标量 anchor-path facility

每张完成 ordinary 行只额外维护

```text
attachment(S)=min_v D_S(v)+dist(v,anchor_path).
```

在每个自然 ordinary 层末，对全部非锚组做一次规范化 block partition DP。一次精确 probe 数为 `(3^k-1)/2`，最多执行 `h` 次，所以时间为 `O(Q+h*3^k)`、空间为 `O(2^k)`。它可能在低 `g` 形成固定成本，但没有改变最坏 `3^k` 基数；是否保留必须由 M4 辅助剪枝消融决定。

### 5.3 Anchor-tree facility 与购买规则

一次锚树设施求值在每个普通树节点做规范化 local partition，并在每条压缩树边做 disjoint-subset convolution。源码计算的

```text
B_tree = t * ((3^k-1)/2 + 3^k)
```

与这两组循环的 probe 数逐项对应，不是拟合阈值。调度只累计已经发生且理论上可能被更紧 incumbent 避免的 ordinary queue pops 与 edge-relax attempts；累计达到 `B_tree` 才求值，随后清零。因此若 ordinary 可收费工作为 `W`，重复锚树求值的 probe 总量为 `O(W+B_tree)`，不会把最坏时间抬出 ordinary closure 边界。单次临时空间为 `O(t*2^k)`。

这个结论只是操作数摊销，不宣称不同 probe 的 wall-time 常数相等。它比 progressive dual 的 `2m+n` 规则更强，因为 `B_tree` 与被调用函数中的实际组合循环逐项一致。

### 5.4 三块 early upper 与 witness

结构层

```text
q3 = ceil(k/3)
```

是把 `k` 个非锚组分成三个块所需的最小最大块大小，不是固定 `g` 特判。该层枚举全部合法三分并按共同根读取已有 `D` 值，工作为 `O(n*3^k)`；随后只对当前最佳三分的三个锚侧选择各做一次有界图闭包，所以另加 `O(C_G)`。所有候选由真实 rooted 行和路径组成，只降低 incumbent。

## 6. 数据布局与转置临时空间

每张 ordinary、anchored 或 backward 行在 sparse、dense 与 ranked-bitmap 中选择**精确估算字节数最小**的表示；选择不读取数据集、密度阈值或 wall time。三种表示保存同一 `<vertex,value>` 集合，branch 位图也只编码严格发布资格。

转置阶段按 64 个连续根流式读取行。常驻事件只为每个 `<target,root>` 保存一个最小真实值，故 `E_T<=n*N_H`；批内 entries、mask potential、cursor 和临时值为 `O(64*N_D+2^k)`。64 来自机器位宽与 bitmap word，不是算法策略超参数。

## 7. 完整最坏界

把以上模块相加，当前 Test152 的完整时间可写为

```text
O(
    n*3^k
  + N*C_G
  + g^2*n*N
  + g^3*2^g
  + h*3^k
  + t*3^k
  + g*C_G
  + g^2*m
).
```

由于 `t<=n`、`N<=2^k`，在参数化算法常用的省略 `poly(g)` 口径下仍为

```text
O(poly(g) * (n*3^k + 2^k*C_G)).
```

没有辅助模块引入 `4^k`、目标乘 pair 空间或超出 `3^k` 的隐藏 mask 枚举。完整空间上界为

```text
O(
    m + g*n + g^2*2^g + t*2^k
  + Q + E_T + 64*N_D
).
```

实际峰值需报告 query peak RSS，因为多项临时结构生命周期不完全重叠；理论式使用安全求和，不把 allocator 采样波动当作算法状态。

## 8. 重叠组下的剪枝口径

普通状态的 future bound 不能粗略表述为“当前最小 `D_S` witness 的真实追加成本下界”。该 witness 可能顺路命中 mask 外的重叠组。正确的完备性口径是：对平衡分解给出的至少一条最优推导，在每个 rooted 状态处，尚未分配部分本身构成一个以同根汇合的合法完成证书；farthest、tour 与 partial/full dual 均不超过该证书的成本。因此这条最优推导不会被 `partial+lower>best` 删除。已经顺路命中的额外组只可能让另一种 witness 更便宜，不破坏所选组分配证书的存在性。

高层反向剪枝更直接：`PrefixLower(T,v)<=A_T(v)`，而 `H_T` 已经把进入 `T` 的闭包拉回到当前合并根。三个前缀项均一致，最大值仍一致，所以被拒值不可能沿图传播后重新变得可用。等号保留。

生产 Release/O2 另用 seed `157606` 在 `n=4..11、g=2..10` 的 `1000/1000` 个含零权边和重叠组随机实例上与 DPBF 对拍通过。该证据覆盖实际 sparse/dense/bitmap、渐进 dual、设施上界、packing 触发和 incumbent 剪枝路径；临时目录已清理。

## 9. 第六条审计结论

当前 Test152 未发现以下策略分支：数据集名、固定 `g`、查询编号、已知答案、状态密度阈值、wall time 或经验常数切换。保留的结构选择为：

- `h=floor(g/2)`、`a=h-1`：来自平衡三分定理；
- `c=floor(a/2)`：低 `A` 与高 `F/H` 的状态格平衡切分，任意 cut 均正确；
- `q3=ceil(k/3)`：完整三块上界的必要块大小；
- 行布局：按实际字节数取最小表示；
- 转置路线：按本根精确 probe 数取较小等价计划；
- 锚树调度：按被调用卷积的精确 probe 数购买；
- progressive dual：按 `2m+n` 结构规模调度，但**不是理论摊销保证**。

因此“无无理特判”门与“完整渐进复杂度”门通过。生产行表示、packing 数值边界和 progressive partial-prefix 已由 `test157_production_audit.md` 的独立检查覆盖；M4/M5 的 420 条同源面板把 M5 冻结为唯一后续候选。progressive dual、scalar facility、anchor-tree facility、early upper 和 packing 仍都只是辅助剪枝模块，不能与 `D/A/F/H + transpose` 混写成原创核心；其中 `2m+n` 仍只表示确定性结构调度，不升级为理论摊销保证。
