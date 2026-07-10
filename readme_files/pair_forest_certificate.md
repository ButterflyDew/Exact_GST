# Pair-Row Predecessor Certificate

更新时间：2026-07-10。本文说明一个已经通过 full DBLP g13 独立探针、但尚未接入完整 solver 的 exact row 表示。它只改变 half-DP 的 pair-row 存储与读取，不改变状态、递推、图、query 或上下界语义。

## 1. 动机

full DBLP g13 的 78 个 pair masks 在已知最优值与 TSP/2 下仍产生：

```text
settled pair states = 125,637,681
queue pushes        = 225,221,651
```

ReleaseV1 的普通 row 为每个状态长期保存 `distance` 与 `need` 两个 double。按其 dense/sparse 精确选型公式，这些 pair rows 需要约 `2.51GB`，是当前最明确的 g13 空间瓶颈。

## 2. Certificate 定义

pair mask `{a,b}` 的 Dijkstra 初始值可随时重算：

```text
seed_ab(v) = gd[a][v] + gd[b][v]
```

闭包搜索中为每个存活 vertex 保存一个 32 位 code：

```text
-1          absent
 0          该点保留直接 seed_ab(v)
 edge_id+1  最后一次严格改善来自这条 predecessor edge
```

若 code 指向 parent `p`，则：

```text
D({a,b},v) = D({a,b},p) + w(p,v)
```

从任一点沿 predecessor 回溯到 code 0，再用 `seed_ab(root)` 即可精确重建距离。parent 总是已经从队列弹出的点；即使存在零权边，严格改善和弹出顺序也不会形成 predecessor cycle。

## 3. Compact 闭包

令 pair row 的 future heuristic 为 `h`，持久剪枝值为：

```text
need(v) = D({a,b},v) + h(v)
```

沿 predecessor edge `p-v` 有 `D(v)=D(p)+w(p,v)`。TSP/2 edge-consistent，因此：

```text
h(p) <= w(p,v) + h(v)
need(p) <= need(v)
```

所以上界下降后，若 child 仍满足 `need<=best`，其 parent 必然也满足。删除 dead states 后，live certificate 仍然 predecessor-closed，不需要保留隐藏 parent。

## 4. 空间与时间

对包含 `s` 个状态、图有 `n` 个点的 pair row：

```text
ReleaseV1 row = min(16(n+1), 20s) bytes
certificate   = min( 4(n+1),  8s) bytes
```

dense/sparse 仍只比较真实字节数，没有密度超参数。sparse certificate 保存排序后的 `(vertex,parent_code)`。

使用 row 时，以 memoized forest traversal 在线性时间恢复到临时 double workspace。一个 certificate 在一次 recurrence 使用中每个 parent 最多解码一次。所有 disjoint subset pairs 的总数为 `O(3^g)`，所以新增最坏时间为 `O(3^g n)`，临时空间为 `O(n)`；不会重复 Dijkstra，也不增加 `m` 项。整体仍在：

```text
O(3^g n + 2^g((g + log n)n + m))
```

之内。

## 5. 正确性检查

Release/O2 独立工具：`tools/pair_forest_probe`。

| check | result |
| --- | --- |
| random graphs, `g=3..8` | `ALL_OK seed=710531 iterations=5000` |
| five fast g12 queries | closure/decode errors `0/0` |
| full DBLP g13 q1, all 78 pair rows | closure/decode errors `0/0` |

每行同时保存普通 Dijkstra distance，仅用于探针比较；certificate 解码值逐状态以相对 `1e-9` 检查。该工具不修改 Test19/ReleaseV1。

## 6. Fast g12

| dataset | states | row/cert compression | roots % | avg/max depth | total decode |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `155,570` | `3.37x` | `11.51%` | `7.28 / 37` | `7.70ms` |
| Toronto-new | `218,517` | `3.99x` | `6.49%` | `17.94 / 90` | `8.61ms` |
| DBLP | `139,044` | `3.53x` | `3.30%` | `1.02 / 3` | `3.72ms` |
| DBLP-new | `26,892` | `2.50x` | `19.81%` | `1.04 / 3` | `1.82ms` |
| MovieLens | `19,340` | `2.50x` | `2.04%` | `1.82 / 5` | `1.62ms` |

所有数据版本都有至少 `2.5x` 的持久 pair-row 缩减；没有按数据集选择是否启用。

## 7. Full DBLP g13

运行入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\pair_forest_probe\run_bounded.ps1 -Seconds 900
```

runner 持有实际 PID，运行正常完成：

```text
bounded_timeout     = 0
wall                = 305.6s
probe total         = 273.146s
pair search         = 235.072s
certificate decode  = 14.289s
settled             = 125,637,681
roots               = 611,964 (0.487%)
avg/max depth       = 2.726 / 7
ReleaseV1 row bytes = 2,512,753,620
certificate bytes   =   778,821,060
compression         = 3.226x
```

decode 时间约为 pair search 的 `6.08%`。外部观测进程峰值约 `1.35GB`，因为 probe 逐 row 验证后释放；它不是 78 个 certificate 同时常驻的完整 solver 峰值。关键结论应使用上面的 `778,821,060` certificate bytes，而不能把 probe RSS 当成 Test20 RSS。

状态数和 queue pushes 与此前独立 pair-layer 探针完全相同，说明该运行复现了同一目标层，而不是更换筛选口径。

## 8. 生产原型结果

曾从 ReleaseV1 建立只替换 pair-row 表示的 Test20 原型。它通过 DPBF 随机 `g=2..10` 300 个、固定 g13 30 个、small 35 条与 fast 20 条权重检查；small 35 条也没有一条慢于 PrunedDP。

但 fast 总时间为 `47.384s`，ReleaseV1 为 `34.207s`，慢 `38.5%`。重复解码约 `56k--72k` 次/数据版本，累计 `18.281s`；3500 点图上的 RSS 只下降约 `0.3--2.4MiB`。因此生产原型已撤出，未运行 full solver。详情见 `archive/test20_pair_forest_prototype_20260710.md`。

按当前 size-ordered recurrence，g13 每个 pair 会被 build/Complete 分别读取 `561/462` 次，共 `1023` 次；78 个 pairs 共 `79,794` 次。k5 时 size6 complement 尚不可用，pair Complete 只发生在 k6。结合 full 单次全 pair 解码 `14.289s`，仅 decode 即约 `4.06h`。这不是运行时猜测，而是 subset dependency 的精确计数。

## 9. 当前边界

- full pair 结果证明 certificate 表示有效，但不证明重复解码的完整 solver 有效；当前没有 Test20，也没有宣称 full DBLP g13 已跑通。
- 已撤出的原型只压缩 pair rows。更高 mask 的 Dijkstra roots 不能全部由 `gd[a]+gd[b]` 重算，需要额外 root witness，不在本轮结论中。
- forest 解码必须复用 `O(n)` 工作区，不能为每次 join 新建长期数组。
- ReleaseV1 保持冻结；只有先解决重复解码而且重新通过 fast A/B，才考虑下一实验版本。
- 这不是普通图/query 压缩，baseline 不能在不采用本 DP row 语义的情况下直接复用。
