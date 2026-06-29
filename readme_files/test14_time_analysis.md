# Test14 时间、空间与实测分析

## 1. 理论上界

记点数为 `n`，无向边数为 `m`，组数为 `g`，`H=floor(g/2)`。

Test14 的三个集合瓶颈均先枚举合法 mask 组合，再对两个按顶点排序的稀疏表做
至多 `O(n)` 的线性 join：

```text
当前 mask 初值 pull    O(3^g n)
在线 h 批处理          O(3^g n)
补集二分批处理         O(3^g n)
```

其余部分为：

```text
组距离预处理           O(g(m+n log n))
LB                     O(2^g g n)
每个 mask 的排序/冻结   O(2^g n log n)
分层 Dijkstra          O(2^g(m+n log n))
```

总时间不超过：

```text
O(3^g n + 2^g((g+log n)n+m)).
```

持久空间只与实际有限状态数 `F` 和 confirmed 状态数 `C` 有关：

```text
finite 顶点和值        O(F)
finite exact 标记      O(F)
confirmed 顶点和值     O(C)
当前 mask scratch      O(n)
mask 元数据             O(2^g)
```

即 `O(F+C+n+2^g)`，不再分配 `n * sum_{k<=H} C(g,k)` 的稠密 DP。

## 2. Toronto g10 完整 40 条

Release，Toronto `query_g10.txt` 全部 40 条：

```text
Test14 wall time       23.389 s
Test13 wall time       28.486 s
Test11 wall time       41.604 s

Test14 / Test13         0.821
Test14 / Test11         0.562

Test14 DP time         18.269 s
Test13 DP time         22.902 s
Test11 DP time         35.440 s
```

Test14 相对当前 Test13 的完整墙钟时间降低约 `17.9%`，DP 阶段降低约 `20.2%`；
相对 Test11 的 DP 阶段接近 `1.94x`。

40 条答案与既有 PrunedDP 精确结果在 `1e-6` 下全部一致，最大差值为 0。

Test14 各模块在 40 条上的累计 DP 时间：

```text
pull             5.051 s
h                4.906 s
completion       2.507 s
graph search     3.501 s
其他布局/排序     约 2.305 s
```

后续优化应优先关注 pull 与 h；图搜索已不再是首要瓶颈。

## 3. canonical pull 的实际效果

初版分别执行两个方向：

```text
confirmed[A] join finite[B]
confirmed[B] join finite[A]
```

改为一次 `finite[A] join finite[B]`，并在交点检查至少一侧 exact 后，Toronto g10
query 5：

```text
初版 wall / DP / pull    4.871 / 4.732 / 1.992 s
canonical 版本           4.044 / 3.891 / 1.117 s
```

该优化不改变候选集合，把 pull 的无序 mask 二分数与表扫描量近似减半。

曾测试在表长度高度不平衡时对小表逐项二分大表。虽然估算比较次数下降，但 query 5
的 DP 时间由约 `4.73s` 上升到 `5.05s`；连续线性归并具有更好的缓存局部性，因此
正式实现保留线性 join。

## 4. 峰值工作集

Toronto g10 query 5，以 20ms 间隔采样进程工作集：

```text
Test14     66.5 MiB
Test13    289.6 MiB
Test11    772.9 MiB
```

Test14 相对 Test13 约节省 `4.35x`，相对 Test11 约节省 `11.6x`，已经在该样本上
达到相对主要旧实现一个数量级的空间优势。

该查询 Test14 的最大持久状态数为：

```text
finite       2,053,625
confirmed    1,678,628
```

这也说明稀疏持久化的收益来自实际状态数，而不是把稠密二维 DP 换一种索引方式。

## 5. 正确性检查

按照 `agent.md` 的要求，Toronto 默认 160 条使用已有
`result/Toronto/DPBF/default/weights.txt` 校验，不重新运行 DPBF。Test14 的 160 条
结果在 `1e-6` 下全部一致。

另外：

```text
Toronto g10 40 条：全部与已有 PrunedDP 一致
Toronto g13 query 1：0.7048467020，与已有精确结果一致
```

Toronto g13 query 1 的墙钟时间：

```text
Test14      23.031 s
PrunedDP   147.441 s
```

即当前 Test14 在该较大组数样本上约快 `6.4x`。
