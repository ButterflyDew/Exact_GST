# Test16：稀疏状态 + 收缩 liveup

Test16 是当前将 Test14 空间优化和 Test15 安全转移语义融合后的实现。核心取舍是：只保存后续仍可能
有用的 `|S|<=H=floor(g/2)` 状态；不预计算整行 `h/complement`；当前也不在热路径启用 naive
online h。

## 状态语义

每个已处理的 `mask S` 保存一条按 root 升序的稀疏行：

```text
v[]      root
d[]      dp[S][v] 的当前最优可行值
cover[]  该树实际覆盖的组集合，用于同根合并后传播 cover
```

singleton 行由组到点最短路直接给出，是全根精确值。其余 mask 在本层搜索时会临时维护 `dist/cov`，
但只有真正从堆中弹出且通过 `best/Far/LB` 门槛的 root 会写回稀疏行，参与后续 liveup。

安全性依据很简单：若状态在当前 `best` 下被 `Far` 或 `LB` 挡住，则它与任意补集拼出的答案都不会优于
当前上界；之后 `best` 只会下降，因此这些未弹出的候选可以丢弃。

## liveup

pair 层仍会临时枚举完整 own-merge source：

```text
seed[{a,b}][v] = gd[a][v] + gd[b][v]
```

这样 pair 状态可以作为 Dijkstra 源传播；但 Test16 不再把所有 pair source 永久写入 row，只保留弹出态。
这避免高阶 same-root merge 反复扫描大量已被证明无用的 root。

高阶 same-root merge 使用自适应 join：

- 两行规模接近时，使用双指针线性交；
- 一行显著更小时，枚举小行并在大行二分。

二分路径只在估算代价低于线性扫描时启用，因此不改变理论上界，只改善稀疏行常数。

## best 拼接

只求到 `H` 层时，完整答案通过堆顶弹出的 `(S,v)` 在线拼补集：

```text
R = U - S
R = X union Y, |X|<=H, |Y|<=H
best = min(best, dp[S][v] + dp[X][v] + dp[Y][v])
```

`Lookup(mask,v)` 对 singleton 直接读 `gd`，对其它 mask 在稀疏行中二分；未处理或已丢弃的状态视为
不可用。这一步只更新上界，不作为 h witness。

## h 当前结论

安全 h 的语义仍应是：

```text
h(R,v)=max { E[T,v] | T subset R, T 已认证 exact }
```

但当前 sparse exact 信息太弱，naive online exact-h 在 Toronto `query_g10` 前 5 条几乎不减少
`pq_pop/inqueue`，却会带来大量查询；hard query 5 曾从约 `4.2s` 增至约 `8.2s`。因此 Test16
默认不启用 h，也不物化 exact witness；`h_*` 统计字段保留为后续实验接口。

## 当前验证与性能

正确性：

- 随机小图黑盒对拍：
  - `seed=271828`，100 组：通过；
  - `seed=424242`，100 组：通过；
  - `seed=20260628`，100 组：通过。
- Toronto default 最后一轮 160 条与已有 DPBF 结果逐条一致：
  - `bad=0`
  - `max_diff=0`
- Toronto `query_g10` 40 条与已有 Test15 结果逐条一致：
  - `bad=0`
  - `max_diff=0`

性能快照：

```text
Toronto query_g10 40 条：
  Test16 当前版：25.14s，peak 56.02 MiB
  Test15 既有结果：53.45s

Toronto default 160 条：
  Test16 当前版：16.85s，peak 31.66 MiB
```
