# Test13 时间复杂度与运行分析账本

记：

```text
n = 点数
m = 无向边数
g = 组数
H = floor(g/2)
S = 2^g
```

目标上界与 PrunedDP 一致：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

下文把代码模块、理论复杂度和统计字段一一对应。按照项目 `agent.md` 的
约定，具体实现使用 `std::priority_queue`，理论核算时将其视作与斐波那契堆
复杂度一致。

## 1. 组距离预处理

每组一次多源 Dijkstra：

```text
O(g(m+n log n))
```

字段：

```text
group_dist_ms
```

`group_pair` 汇总最多检查 `g` 次全部组顶点，最坏 `O(g^2 n)`。它被
`O(2^g g n)` 覆盖。

## 2. 初始上界

root-star：

```text
O(gn)
```

组件增长 greedy 最多连接 `g-1` 次，每轮一次多源 Dijkstra：

```text
O(g(m+n log n))
```

字段：

```text
greedy_ms
greedy_pops
```

该项被主式中的 `2^g(m+n log n)` 覆盖。

## 3. O(1) `far` 查询表

把 `g` 个组位分成两半，为每个顶点、每个半掩码存一个最远组编号
（`uint8`）：

```text
O(n(2^floor(g/2) + 2^ceil(g/2)))
```

每次 `far(v,mask)` 只做两次索引和一次比较，为 O(1)。因此 `3^g` 合并内层
可保留 `cand+far>=best`，而不会产生 `O(g3^g n)`。

## 4. 剩余组下界

单次 `LB(v,R)` 扫描组距离：

```text
O(g)
```

每个 `(mask,v)` 在当前 mask 内只缓存一次，因此：

```text
O(2^g g n)
```

剩余组 MST 对每个访问过的 mask 只计算一次，Prim 为 `O(g^2)`：

```text
O(2^g g^2) subset O(2^g g n), because g <= n.
```

字段：

```text
lb_calls
```

## 5. 在线 `h`

对当前 `S`，在线 `h` 枚举 `T subset (U^S)`，只接受
`1<=|T|<=|S|` 且 confirmed 的状态。固定根上所有不交有序对仍由三进制
归属计数：

```text
O(3^g n)
```

没有 `future-h` 写入。临时同根合并值不会进入 `h`。

字段：

```text
h_calls
h_checks
h_hits
active_seed
```

经验比值：

```text
h_checks / h_calls
h_hits / h_checks
```

分别表示每次在线查询的平均补集枚举量和有效可信状态命中率。

## 6. 同根合并

固定根 `v`，一次转移选择两个不交集合 `(A,B)`。每个组有三种归属：

```text
属于 A / 属于 B / 两者都不属于
```

所以全部不交有序对不超过 `3^g`。Test13 只处理其中 `|A|<=H` 且满足
live-size 条件的部分，因此：

```text
O(3^g n)
```

字段：

```text
merge_enum
live_dp_checks
prune_ge_best
prune_far
```

诊断比值：

```text
live_dp_checks / merge_enum
```

越低，说明 dense 补集枚举浪费越多，后续可考虑仍保持 `3^g` 上界的
size-aware 子掩码生成器。

特别约束：这里只能使用当前的 O(1) `far`，不得直接扫描组或调用 O(g) 的
完整 `LB`，否则最坏复杂度会变为 `O(g3^g n)`。

大于 `H` 的状态不再定义。live 合并只生成并集大小不超过 `H` 的状态。

## 7. 在线补集拼接

对每个弹出状态 `(v,S)`，扫描同根 finite 小状态 `X`，检查
`Y=(U^S)^X` 是否同样为小状态且 finite。全部不交三元归属 `(S,X,Y)` 的
理论总量为：

```text
O(3^g n)
```

因此删除大状态后，在线拼接仍不超过原总复杂度上界。

## 8. 分层图搜索

最多处理 `2^g` 个 mask。实现使用 `std::priority_queue`；按照
`agent.md` 的复杂度核算约定：

```text
O(2^g(m+n log n))
```

实际操作数由以下字段记录：

```text
pq_push
pq_pop
relax_try
relax_ok
inqueue
```

实验中建议分别观察：

```text
heap pressure = (pq_push + pq_pop) / inqueue
relax success = relax_ok / relax_try
```

## 9. 其他线性扫描

popcount、mask 分桶、active 列表、confirmed 位图和每个 mask 的临时 LB
数组：

```text
O(Mn + g2^g), M=sum_{k=0}^H C(g,k)
```

被主式覆盖。

## 10. 总复杂度

合并各项：

```text
预处理        O(g(m+n log n))
在线 h         O(3^g n)
LB             O(2^g g n)
分层 Dijkstra O(2^g(m+n log n))
同根合并      O(3^g n)
在线补集拼接   O(3^g n)
```

得到：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

## 11. 实验分析顺序

每条慢查询按以下顺序判断：

1. `group_dist_ms / prep_ms` 高：大图 SSSP 是主因。
2. `merge_enum` 高：同根子掩码枚举是主因。
3. `h_checks` 高：在线目标判定是主因。
4. `relax_try` 高：Dijkstra 扩展区域过宽。
5. `pq_push+pq_pop` 高但 `relax_try` 不高：堆重复项或 target 传播是主因。
6. `active_seed` 高：中间 mask 状态数量爆炸。

每次优化都应同时记录：

```text
wall time
prep_ms
dp_ms
上述操作计数
峰值 working set / RSS
```

这样可以区分“减少操作数”与“单次操作变贵”，也能验证论文所需的时间和
空间数量级优势。

## 12. 存储布局与 Test11 实测

Toronto `g10` query 1 的 5 次 Release A/B：

```text
Test13 扁平 dp[mask*(n+1)+v]  dp_ms 平均 604.0 ms
Test13 连续内存+行指针       dp_ms 平均 621.0 ms
Test13 vector<vector<double>> dp_ms 平均 740.8 ms
Test11                        dp_ms 平均 644.2 ms
```

扁平索引乘法不是瓶颈。预存行指针反而约慢 2.8%，分散二维 vector 约慢
22.6%；正式实现应保留单块连续 `dp`。

MovieLens `g10` query 1 中，Test13 相对 Test11：

```text
dp_ms        27047.7 / 22104.7 ms  (+22.4%)
valid_total  412555  / 359347      (+14.8%)
inqueue      413897  / 376474      (+9.9%)
relax_try    4.190B  / 3.779B      (+10.9%)
lb_calls     17.174M / 14.833M     (+15.8%)
```

Test13 的在线 `h` 为 25.9M 次子集检查，远小于 4.19B 次边松弛。主要慢因是
当前在线 `h` 产生了更宽的搜索范围，尤其 `k=5` 的 valid 状态从 Test11 的
71,503 增至 124,575，继而放大 Dijkstra、LB 和堆操作。

将在线 `h` 的可信子状态上限从 `|T|<=|S|-1` 放宽为 `|T|<=|S|` 后：

```text
MovieLens g10 q1:
valid_total  412,555 -> 365,936  (-11.3%)
k=5 valid   124,575 -> 78,135   (-37.3%)
relax_try    4.190B  -> 3.725B   (-11.1%)
dp_ms        27.05s  -> 23.73s   (-12.3%)

Toronto g10 q1:
valid_total  69,824 -> 70,903    (+1.5%)
dp_ms        约 0.60s
```

MovieLens 上状态明显回落并接近 Test11 的 `valid_total=359,347`。Toronto 的
valid 小幅上升，说明同层 confirmed 证据改变处理顺序后，valid 数不保证严格
单调，但整体运行没有恶化。Toronto 默认 160 条仍与 DPBF 在 `1e-6` 下全部
一致。

进一步对齐 Test11 的 `dp+LB` 堆键、同层 `std::sort` mask 顺序，并要求在线
候选的补侧当前 finite 后：

```text
MovieLens g10 q1: Test13 valid=359,279, Test11 valid=359,347
Toronto g10 q1:   Test13 valid=70,080,  Test11 valid=70,314
```

剩余差异来自时间语义：Test11 在 source `Modify` 事件发生时检查补侧；
Test13 在查询时检查，能追溯使用后来才具备补侧的旧 source，因此 `h` 略强。

## 13. Toronto g10 完整性能修正

旧 Test13 对 live 合并和在线 `h` 都枚举补集的全部子掩码。40 条聚合：

```text
Test11 dp_ms            38.67 s
旧 Test13 dp_ms         52.15 s

Test11 bucket_scan       1.111B
旧 Test13 merge_enum     1.785B
旧 Test13 h_checks       1.082B
```

两者的 `valid/inqueue/relax_try/pq` 数量误差均小于 1%，所以慢因不是搜索范围，
而是 Test13 多做了约 1.76B 次不存在状态的稠密 mask 检查。

修正后为每个 `(root,size)` 维护连续的 finite/confirmed bucket：

```text
Test11 完整 40 条       44.525 s
旧 Test13              57.405 s
新 Test13              30.000 s

新 finite bucket scan   0.530B
新 confirmed h scan     0.517B
```

新 Test13 比 Test11 快 32.6%，比旧版快 1.91 倍。曾尝试全局链式 bucket，
虽然扫描数相同，但 q5 因节点离散、缓存局部性差仍需 9.43 s；改为每个 bucket
连续 vector 后降到 5.79--6.42 s，低于 Test11 的 7.16 s。

## 14. 完全删除大状态

只为

```text
M=sum_{k=0}^H C(g,k)
```

个小 mask 分配 DP 行，live 合并仅生成并集大小不超过 `H` 的状态。每个堆顶
在线把补集分成两个小状态更新 best。

最初直接枚举全部基数可行子掩码时，Toronto g10 40 条为 45.93 s，仅略快于
同期 Test11 的 47.37 s；在线拼接执行了 540.8M 次二分检查。

改为扫描同根 finite bucket 后：

```text
Test11 40 条             47.367 s
half-only Test13         27.667 s
时间比                    0.584

complement_calls          6.541M
complement bucket scans   674.880M
```

虽然扫描计数更高，但连续 root-local bucket 避免了每次子掩码的 popcount、
行映射和大范围随机访问，实际时间下降明显。40 条权重一致，Toronto 默认
160 条也与 DPBF 在 `1e-6` 下全部一致。
