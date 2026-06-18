# Test9 多数据诊断对照记录

本文记录 `Test8` 与当前 `Test9` 在若干小批查询上的 Release 对照。目的不是验证最终优化，而是判断当前 `Test9` 的“状态存储过滤”是否真的改善时间。

## 运行样本

已完成样本：

```text
example/query q1
data_new/DBLP/query_g4_uniform q1
data_new/MovieLens/query_g4_uniform q1
data_new/MovieLens/query_g5_uniform q1
data/Toronto/query_g10 q1
data/Toronto/query_g10 q2
data/Toronto/query_g10 q3
```

中止样本：

```text
data/DBLP/query_g10 q1
```

该样本在 `Test8` 上超过 5 分钟仍未完成首条输出，不适合作为这轮快速诊断样本。

## 结果摘要

```text
example q1
Test8 total=0.000190s
Test9 total=0.000078s
结论：样本太小，仅说明结果一致。

data_new/DBLP g4_uniform q1
Test8 total=7.851104s prep_ms=6332.479 dp_ms=940.548
Test9 total=7.002061s prep_ms=6100.426 dp_ms=367.460
Test9 结果：更快。dp/h finite cell 明显减少。

data_new/MovieLens g4_uniform q1
Test8 total=0.889380s prep_ms=729.957 dp_ms=24.527
Test9 total=0.863738s prep_ms=705.662 dp_ms=23.836
Test9 结果：基本持平，略快。

data_new/MovieLens g5_uniform q1
Test8 total=1.262911s prep_ms=1000.809 dp_ms=102.473
Test9 total=1.256638s prep_ms=1029.979 dp_ms=81.178
Test9 结果：总时间基本持平，dp 阶段略快，但 prep 波动抵消收益。

data/Toronto g10 q1
Test8 total=0.974136s prep_ms=86.286 dp_ms=782.788
Test9 total=1.114935s prep_ms=100.278 dp_ms=940.073
Test9 结果：更慢。

data/Toronto g10 q2
Test8 total=4.591187s prep_ms=87.052 dp_ms=4428.968
Test9 total=6.335293s prep_ms=90.335 dp_ms=6174.175
Test9 结果：明显更慢。

data/Toronto g10 q3
Test8 total=3.635817s prep_ms=95.955 dp_ms=3462.919
Test9 total=4.916482s prep_ms=85.356 dp_ms=4758.259
Test9 结果：明显更慢。
```

## 关键统计观察

### Toronto g10 q2

`Test8`：

```text
up_subset=50531547
merge_scan=11747801
merge_dense=46678745
dp_ms=4428.968
```

`Test9`：

```text
up_subset=50531547
merge_scan=11747801
merge_dense=46678745
dead_nxt_skip=3802804
dead_h_skip=12403826
dp_seen=4705949
h_seen=2676853
dp_ms=6174.175
```

重要结论：

```text
up_subset / merge_scan / merge_dense 完全没有下降。
Test9 只是枚举之后少写 dp/h。
Toronto 的瓶颈主要是同根合并枚举次数，而不是写表次数。
因此 Test9 的额外分支、popcount、统计计数会直接变成净开销。
```

### Toronto g10 q3

`Test8`：

```text
up_subset=37378362
merge_scan=12886635
merge_dense=33939769
dp_ms=3462.919
```

`Test9`：

```text
up_subset=37378362
merge_scan=12886635
merge_dense=33939769
dead_nxt_skip=2619309
dead_h_skip=9477627
dp_ms=4758.259
```

同样说明：当前过滤没有减少枚举，因此慢。

### data_new/MovieLens g5_uniform q1

`Test8`：

```text
up_subset=18751
dp_ms=102.473
```

`Test9`：

```text
up_subset=18751
dead_h_skip=18059
h_seen=309
dp_ms=81.178
```

这里 `up_subset` 很小，`h` 写入减少明显，Test9 在 DP 阶段略快。但总时间被预处理和波动抵消。

## 结论

当前 `Test9` 的实现确实可能更慢，尤其在 Toronto g10 这类同根合并枚举占主导的数据上。

原因不是理论过滤方向错，而是过滤落点太晚：

```text
当前 Test9:
    先枚举 t
    计算 nxt
    再判断 dp/h 是否要存

真正需要:
    枚举前就按用途过滤 t
    或者把同根合并拆成 best / live-dp / future-h 三个枚举器
```

也就是说，`dead_nxt_skip` 和 `dead_h_skip` 证明空间生命周期确实存在，但没有转化为时间收益。要转化为时间收益，必须减少：

```text
up_subset
merge_scan
merge_dense
```

而不是只减少：

```text
dp_seen
h_seen
```

## 下一步建议

优先不要继续当前这种“枚举后过滤”的 Test9 方向。

下一步应改成“枚举前过滤”：

```text
root_masks_by_size[v][s]
```

然后把同根合并拆成三类：

```text
1. best pairs
   只检查 t == U^mask

2. live dp
   只枚举 |t| <= g - 2|mask|

3. future h
   只枚举 g-H-|mask| <= |t| <= g-|mask|
```

对于 scan 模式，只扫描相关 size 桶。  
对于 dense submask 模式，只枚举相关 size 的 submask。

预期这才会降低：

```text
merge_scan
merge_dense
up_subset
```

而不是只降低写入数量。

## 对延迟同根合并的影响

本轮诊断也说明，完整的 pull-based delayed merge 需要非常谨慎。

如果 `QueryCover` 只是把 eager merge 的枚举换到查询时做，但查询次数很多，它也可能更慢。

因此延迟同根合并的第一步不应直接实现 `cover[v][S]`，而应先做：

```text
root_masks_by_size
用途拆分枚举器
hot-root scan share 统计
```

只有当这些统计显示“相关 size 桶很小”时，再考虑 `QueryCover`。

