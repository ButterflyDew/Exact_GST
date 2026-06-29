# Test14：按 mask 批处理的稀疏半集合 DP

> **正确性警告（2026-06-25）**：Test14 继承了 Test13 的 confirmed/h 语义。
> Test13 已存在偏大 confirmed 导致非法 h 和最终答案错误的固定反例，因此在重新
> 建立 confirmed 精确性闭包之前，Test14 也不能视为已证明正确。

Test14 的目标是保留 Test13 的半集合正确性与剪枝语义，同时消除
`finite_bucket[root][size]` 和 `confirmed_bucket[root][size]` 的交叉扫描。
后者虽然实测连续且很快，但扫描后才检查 mask 是否不交，最坏扫描次数不能由
`3^g` 的精确三进制归属计数直接限制。

## 状态布局

只保存 `|S|<=H=floor(g/2)` 的状态。每个已处理 mask 保存两张按顶点编号排序的稀疏表：

```text
finite[S]    = (vertex, 当前可行上界)
confirmed[S] = (vertex, 已由 target 定型的精确值)
```

处理当前 `S` 时才分配（并复用）长度 `n+1` 的 `dist/h/comp/lb` scratch。
处理结束后把有限值和 confirmed 值冻结为有序稀疏表。

## 三类批量 join

### 1. 构造当前 mask 的初值

精确枚举无序二分 `A union B=S, A intersect B=empty`。由于
`confirmed[A]` 一定是 `finite[A]` 的子集，只需对每个二分做一次：

```text
finite[A] join finite[B]
```

在相同根顶点处检查 `A`、`B` 是否至少一侧 confirmed；若是，则生成 `dist[S]`。
枚举的是合法 mask 对，而不是先扫描同大小 bucket 再过滤不相交条件。每个无序
mask 二分只做一次有序线性归并，不再分别执行 `confirmed[A] join finite[B]` 和
`confirmed[B] join finite[A]`。

这与 Test13 的事件式 live 合并等价：交点上至少一侧 confirmed，恰好表示 Test13
至少会在其中一侧的 `Modify` 事件中读取另一侧 finite。Test14 在处理 `S` 前读取
所有较小 mask 的最终冻结表，因此不会遗漏该事件产生的候选，并且可能使用更晚
得到的更小 finite 值。

### 2. 批量计算在线 h

当前初值顶点表排好序后，精确枚举

```text
T subset U-S, |T|<=|S|, confirmed[T] 已可用
```

并把当前顶点表与 `confirmed[T]` 归并，在交集顶点上取最大值。每个候选 T 都是补集
的真实子集，因此由集合单调性：

```text
exact_dp[T][v] <= exact_dp[U-S][v]
```

所得最大值仍是安全下界。

### 3. 批量计算补集拼接

在当前 mask 开堆前精确枚举：

```text
X union Y=U-S, X intersect Y=empty, |X|,|Y|<=H
```

对 `finite[X]` 与 `finite[Y]` 做有序归并，并得到：

```text
comp[S][v] = min dp[X][v]+dp[Y][v].
```

之后每个非 stale 堆顶只需执行：

```text
best=min(best,dist[S][v]+comp[S][v]).
```

正确性仍由 Test13 的“最优树带权重心可合并成至多三个小块”证明：选择三个小块中
按算法顺序最后处理的 S，另外两个块在处理 S 时已经冻结，必被该二分枚举命中。

## 时间复杂度

对每个合法 mask 对，顶点归并至多 `O(n)`：

```text
pull:       O(3^g n)
h:          O(3^g n)
completion: O(3^g n)
```

每个 mask 的顶点排序总计至多 `O(2^g n log n)`；LB 与图搜索分别为
`O(2^g gn)` 和 `O(2^g(m+n log n))`。因此总上界为：

```text
O(3^g n + 2^g((g+log n)n+m)).
```

这里没有把“bucket 中恰好存在的状态数”误当成不交 mask 对数；三类瓶颈都先枚举
合法的 mask 组合，再在顶点维度做线性 join。
