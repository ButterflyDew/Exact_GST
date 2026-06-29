# Test15：带精确证书的安全 h

Test15 是独立实现，不调用 Test13，也不采用 Test14 的 mask-major 布局。

## 状态

```text
dp[S][v]     已构造出的可行上界
exact[S][v]  该值是否已被证明等于真实 rooted DP
cover[S][v]  当前可行树实际命中的组集合
```

普通 `dp` 可参与上界转移；只有 `exact` 状态可作为在线 h 的 witness。

## DP 语义下的 exact 种子

Test15 不再使用额外的 `O(g)` pair 骨架预处理。exact 状态只来自 DP 语义本身或独立下界夹逼。

### singleton

预处理每个组到每个点的最短路时已经得到：

```text
gd[a][v] = D*({a},v).
```

因此所有 singleton rooted 状态都可直接写入 `exact` 和 `exact_bucket`。这些状态只作为后续
同根合并与 h witness 使用，不需要全部作为 singleton 层的堆源重新跑图搜索。

### pair

当处理 `|S|=2, S={a,b}` 时，所有根上的同根 merge 种子都已由 singleton exact 完整给出：

```text
seed[v] = D*({a},v)+D*({b},v).
```

当前实现仍为所有 pair 写入 `C(g,2)n` 个 own-merge 源，并把它们作为 pair 层图搜索的 active
source。这里尝试过“只写入 bucket、不作为 active source”的惰性方案，但随机小图会错：
最优解可能需要某个 pair 源先沿图传播，再与其它子树同根合并。因此，若保留 h/LB 对 singleton
扩展的剪枝，pair 源的完整性目前仍是正确性所需。

实现上不再通过通用 `SetDp` 慢路径逐个插入 pair 源，而是在初始化时直接写入 `dp/cover/active/bucket`
连续结构，降低 `C(g,2)n` 这部分不可避免成本。后续 pair 状态若被其它转移改善，仍会顺手枚举自己的
两个 singleton 子集，执行同根 own-merge：

```text
dp[S][v] = min(dp[S][v], gd[a][v]+gd[b][v]).
```

因为 pair 层源集是完整的，且堆 key 使用 `dp` 本身，若当前弹出值不大于所有被 h/LB/Far 门控跳过的
候选距离，则可按 Dijkstra 性质标为 exact；否则它仍只能通过 `LB` 夹逼或实际覆盖升格成为 exact
witness。

### 更大 mask

更大 mask 不能仅因“某个弹出值由若干 exact 子集更新过”就标记 exact；除非能证明该 mask
在所有根上的 merge seed 都完整，否则 Dijkstra 的源集合可能缺失真实最优源。当前实现对
`|S|>=3` 继续使用安全下界夹逼和实际覆盖升格来认证 exact。

## 夹逼认证

对可行上界 `w=dp[S][v]`：

```text
L=LowerBound(S,v).
```

若：

```text
w<=L+eps
```

则由 `L<=D*(S,v)<=w` 得到 `w=D*(S,v)`，标记 exact。

## 实际覆盖升格

可行树在连接名义集合 S 时可能沿途命中额外组。Test15 以常数代价维护：

```text
source: color[v] | S
merge:  cover[A] | cover[B]
edge:   cover[S,u] | color[v]
```

若实际覆盖 `C=cover[S][v]` 满足 `|C|<=H` 且同一代价 w 与 `L(C,v)` 夹逼相等，
则 w 是 `D*(C,v)` 的精确值。Test15 将其直接写入并认证 `(C,v)`。

这项操作不增加图搜索，只在每次合并和边松弛时增加一次整数 OR。更大的 exact C
既能成为更强的 h witness，也能提前为后续 mask 提供精确种子。

## 在线 h

```text
h(R,v)=max(
    exact_dp[T][v] for T subset R
).
```

每个组成项都不超过 `D*(R,v)`，所以 h 是严格安全的下界。

exact 状态使用独立 `(root,size)` 连续 bucket，避免扫描普通 finite 状态后再过滤。

## bucket 与子集枚举取较小者

`Modify`、`OnlineH` 和 `Complete` 都需要在某个根 `v` 上枚举与当前 mask 兼容的状态。若只扫
`bucket[v][size]` 再过滤，bucket 稠密时可能枚举大量不属于 `U-S` 的状态，复杂度口径会偏离
按子集数计的 `3^g` 分析。

当前实现对每次枚举都比较两种代价：

```text
bucket_count = 当前 root/size 桶中的已有状态数
subset_count = U-S 中满足 size 约束的子集数
```

选择较小者执行。若选子集枚举，则直接用 `D[T][v]` 或 `exact[T][v]` 做 O(1) 随机访问；若选
bucket 枚举，则保持原来的稀疏状态优势。这样在稀疏和稠密两端都不会明显吃亏。

## 已删除的方案

- target 弹出即 exact：存在固定反例。
- certified 子集参与认证下界：统计中几乎从不主导。
- 同 mask 锚点传播：没有增加认证总数。
- 高权环启发式：Toronto q5 的 pair 认证反而下降。
- 逐 pair 延迟构造：困难查询中证书到达过晚，运行时间恶化。
- 独立 pair 骨架预处理：已改为 singleton exact 诱导的大小 +1 liveup，不再全量求 pair 行。

## 堆搜索门控

`dp` 和 `exact` 分离后，若某个 `(S,v)` 满足：

```text
dp[S][v] + h(U-S,v) > best
```

或：

```text
dp[S][v] + LowerBound(U-S,v) > best
```

则以它为中间结构继续向外扩展也不可能产生更优完整答案。证明要点是 rooted DP 关于根满足
1-Lipschitz：沿边 `v-u` 扩展时，

```text
D*(U-S,v) <= w(v,u)+D*(U-S,u).
```

所以如果当前根处的安全补集下界已经超过 `best`，任何经由该状态扩展到新根的完整解同样
超过 `best`。

实现上不把昂贵的 `LB/H` 检查放在每条边松弛上。初始源和弹出状态会先用 `Far(U-S,v)` 这个
O(1) 下界短路，只有通过后才计算 `LB/H`；边松弛只做 `D` 改善、`nd>=best` 和
`nd+Far(U-S,to)>best` 这类常数级判断，入堆 key 为 `dp`。真正弹出准备执行
`Complete / Modify / relax` 前，再计算必要的 `LB/H` 并决定是否扩展。h 值按
`(mask,root,exact_version)` 缓存；当新的 exact witness 出现后，后续检查会重新计算对应 root
的 h，避免用旧的较弱 h 继续放行新状态。弹出后若因 best 更新或 exact witness 增强导致上述门控
失效，则该点不执行
`Complete / Modify / relax`。

## 当前实验

Toronto g10：

```text
q1:
cover upgrade  9,077 / 17,734
valid          62,049 -> 48,160
DP             272.7 ms -> 249.2 ms

q5:
cover upgrade  136,843 / 302,216
valid          938,151 -> 738,693
DP             5785.7 ms -> 4698.5 ms

q36:
cover upgrade  5,852 / 7,230
valid          22,660 -> 21,326
```

历史白盒审计中，两组随机小图共 60,000 个实例同时检查：

```text
所有 exact 值不超过完整 DP 精确值
所有 h 不超过真实补集 rooted DP
最终答案与 DPBF 一致
```

全部通过。当前工程的常规随机回归使用 `gst_random_compare` 黑盒比较主程序输出与 DPBF。
固定的 Test13 错误实例中：

```text
DPBF=55, Test13=56, Test15=55.
```
