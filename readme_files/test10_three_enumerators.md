# Test10 三类同根枚举器说明

`Test10` 是从 `Test8` 派生的实验版本，目标是只研究“三类同根枚举器”本身，不引入 sparse `dp/h`。

## 与 Test8 的关系

保留：

```text
dense dp[mask][v]
dense h[mask][v]
active_vertices[mask]
按 popcount(mask) 分层处理
Test8 的 LB / root-star / greedy upper
```

改变：

```text
root_masks[v] -> root_masks_by_size[v][size]
Modify(mask,v) 中的同根合并拆成三类枚举器
```

## 三类枚举器

设当前处理 `|mask| = k`。

### 1. best 枚举器

只检查：

```text
t = U ^ mask
```

若 `dp[t][v]` finite，则直接更新：

```text
best = min(best, dp[mask][v] + dp[t][v])
```

不写 `dp[U][v]`，也不写 `h[U][v]`。

### 2. live-dp 枚举器

只枚举：

```text
|t| <= g - 2k
```

这样：

```text
|mask| + |t| <= g-k
```

生成的 `nxt = mask | t` 仍可能作为未来状态参与同根合并。若 `nxt != U` 且 disjoint，则更新：

```text
dp[nxt][v]
```

### 3. future-h 枚举器

只枚举未来可能被 `h[U^mask]` 查询到的范围：

```text
g-H-k <= |t| <= g-k
```

若 `nxt = mask | t` 满足 `nxt != U` 且 disjoint，则更新：

```text
h[nxt][v]
```

## 统计字段

`test10_stats.txt` 新增：

```text
bucket_scan       三类枚举器扫描的 size bucket 总长度
best_checks       best 枚举器实际检查次数
live_dp_checks    live-dp 枚举器实际候选次数
future_h_checks   future-h 枚举器实际候选次数
```

注意：

```text
up_subset = live_dp_checks + future_h_checks
```

它不再能和 `Test8` 的 `up_subset` 逐项等价比较，因为同一个 `t` 可能同时属于 live-dp 和 future-h 的需求范围，在 Test10 中会按两种用途分别计数。

更应该比较：

```text
dp_ms
bucket_scan
live_dp_checks / future_h_checks
best_checks
最终 best 是否与 Test8 一致
```

## 已做 g>7 冒烟

命令：

```powershell
.\build\Release\gst_test10_main.exe MovieLens weight result_test10 debug child_first g8_uniform data_new 1 1
.\build\Release\gst_test8_main.exe MovieLens weight result_test10_baseline debug child_first g8_uniform data_new 1 1
```

结果：

```text
Test8:
best=0.0012311587
total=2.509615s
dp_ms=780.031
up_subset=265830

Test10:
best=0.0012311587
total=2.564123s
dp_ms=755.606
bucket_scan=1602754
best_checks=4734
live_dp_checks=189573
future_h_checks=211089
```

解释：

- 权重一致。
- `dp_ms` 略低，但总时间受预处理波动影响略高。
- 当前 `bucket_scan` 偏大，说明 size bucket 分拆后仍有重复扫描，后续需要把 live-dp 和 future-h 的交集合并扫描，或者继续引入 hot-root index。

## 当前判断

`Test10` 目前是 dense 基线上的“三类枚举器”原型。它比之前的 `Test9_A/B` 更符合当前方向，因为没有 sparse `dp/h` 和 cover 查询成本。

下一步若继续优化，应优先减少：

```text
bucket_scan
```

而不是继续压缩 `dp/h` 存储。

