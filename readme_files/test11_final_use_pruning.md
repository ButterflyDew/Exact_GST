# Test11 final-use far 剪枝说明

`Test11` 从 `Test10` 派生，仍然使用 dense：

```text
dp[mask][v]
h[mask][v]
```

并保留 `Test10` 的三类同根枚举器：

```text
best_checks
live_dp_checks
future_h_checks
```

新增的是 live-dp 枚举器中的三个高收益剪枝。

## 新增规则

### 1. 跳过 `t=0`

`Test10` 的 live-dp 枚举器本来已经从 `sz=1` 开始，因此 `Test11` 继续保持：

```text
t=0 不参与 live-dp
```

### 2. 跳过 `cand >= best`

```text
cand = dp[mask][v] + dp[t][v]

if cand >= best:
    skip dp[nxt][v]
```

因为后续边权和同根合并代价非负，该状态不可能导出更优解。

### 3. 跳过 `cand + far(v,U^nxt) >= best`

```text
far(v,rem) = max_{a in rem} group_dist[a][v]

if cand + far(v, U^nxt) >= best:
    skip dp[nxt][v]
```

这是上一轮诊断中命中率最高的 final-use 条件。它比完整 `LB(v,U^nxt)` 便宜，而实测命中数几乎等同完整 LB。

## 统计字段

`test11_stats.txt` 中新增：

```text
prune_ge_best   cand >= best 剪掉的 live-dp 候选
prune_far       cand + far(v,U^nxt) >= best 剪掉的 live-dp 候选
closed_layers   live-dp 枚举器在该 Modify 中无可用 t-size 的次数
```

## 实测对比

使用 Release，对比 `Test10` 与 `Test11`。

### MovieLens g8_uniform q1

```text
Test10:
best=0.0012311587
total=2.475283s
dp_ms=723.235
up_subset=400662
live_dp_checks=189573
future_h_checks=211089

Test11:
best=0.0012311587
total=2.416119s
dp_ms=755.303
up_subset=152063
live_dp_checks=33335
future_h_checks=118728
prune_ge_best=12697
prune_far=71710
```

权重一致。`up_subset` 大幅下降，但 `dp_ms` 略升，总时间略降，说明收益和额外影响接近。

### MovieLens g10_uniform q1

```text
Test10:
best=0.0004877212
total=3.973603s
dp_ms=1709.586
up_subset=864977
live_dp_checks=430020
future_h_checks=434957

Test11:
best=0.0004877212
total=3.982307s
dp_ms=1840.187
up_subset=346469
live_dp_checks=98018
future_h_checks=248451
prune_ge_best=10679
prune_far=172908
```

权重一致。`up_subset` 大幅下降，但 `dp_ms` 变慢。

可能原因：

```text
剪掉 live dp 后，未来某些 h 证据不会再通过该 dp 状态产生。
当前 h 缺省值为 -1，会让 dp+h<=best 更宽松，反而可能增加 Dijkstra 探索。
```

可以看到：

```text
Test10 lb_calls=9449562
Test11 lb_calls=9668797
Test10 relax_try=96431908
Test11 relax_try=102183278
```

### Toronto g10 q1

```text
Test10:
best=0.4475349050
total=1.016648s
dp_ms=836.672
up_subset=9452884
live_dp_checks=4927005
future_h_checks=4525879

Test11:
best=0.4475349050
total=0.815815s
dp_ms=633.193
up_subset=1872395
live_dp_checks=420021
future_h_checks=1452374
prune_ge_best=189869
prune_far=1285229
```

权重一致，且明显加速。

## 当前结论

`cand + far` final-use 剪枝确实能大幅减少同根枚举与状态生成，但时间收益依赖数据。

在 Toronto g10 上：

```text
收益明显
```

在 MovieLens g10 上：

```text
up_subset 大幅减少，但 Dijkstra 探索增加，dp_ms 变慢
```

因此该剪枝不是无条件正优化。后续要继续研究：

```text
1. 被剪掉的 dp 是否会产生未来 h 证据；
2. 是否需要为被剪掉的 dp 保留 h-only ghost 信息；
3. h 缺省值 -1 是否导致目标筛选过宽；
4. 能否把 h 语义改成更直接的 complement availability，避免缺省 h 放宽搜索。
```

## 建议下一步

`Test11` 暂时作为对比实验版本保留。

若继续优化，优先研究：

```text
Test12 = Test11 + h-only ghost
```

思路：

```text
当 live dp 因 cand+far>=best 被剪掉时，不写 dp[nxt][v]，
但如果该状态未来能提供 h[nxt][v] 目标证据，则写入 h-only 信息。
```

目标是保留 `Test11` 减少 dp/root_masks 的收益，同时避免 h 缺失导致 Dijkstra 探索变宽。

