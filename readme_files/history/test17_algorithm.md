# Test17：Complete 前置过滤与补集二分预处理（历史）

Test17 是 Test16 的轻量增量版。它不改变 DP 状态语义，也不引入新的剪枝条件；目标是降低在线补集拼接的实际开销，并观察实际覆盖 `cover` 是否有继续利用空间。

Test16 的完整算法和证明见 `test16_algorithm.md`。

## 1. 与 Test16 的差异

Test17 只改动 `Complete` 相关部分：

1. 当当前弹出状态大小为 `k=|S|` 且 `3k<g` 时，跳过本次补集拼接。
2. 为每个 mask `S` 预存合法补集二分 `(X,Y)`。
3. 增加 `cover` 相关统计，但不把它作为算法条件。

其它部分，包括同根合并、图搜索、安全门控、稀疏状态行和 h 禁用原则，都与 Test16 相同。

## 2. early Complete

Test16 在每次弹出 `(S,v)` 时都会尝试拼完整答案：

```text
S union X union Y = U
|X|, |Y| <= H
```

但算法按 mask 大小递增处理。在处理大小为 `k` 的状态时，当前可查的非空状态大小最多也是 `k`。如果：

```text
3k < g
```

那么三个大小不超过 `k` 的块不可能覆盖全部 `g` 个组。本次补集枚举必然无法找到当前时刻可用的完整解，因此可以直接跳过。

这个判断只跳过一次上界更新尝试，不跳过：

```text
同根合并
图搜索
状态入堆
状态保存
后续 liveup
```

所以它不会删除任何最优构造链。

## 3. complete_pairs 预处理

对每个 mask `S`，预先整理所有合法二分：

```text
R = U ^ S
X union Y = R
X intersect Y = empty
|X|, |Y| <= H
```

运行中每次弹出 `(S,v)` 时直接遍历 `complete_pairs[S]`，避免对同一个 `S` 的每个 root 重复枚举子集。

预处理只是把枚举时机前移，枚举集合与 Test16 相同。

## 4. cover 诊断

Test17 统计：

```text
complement_cover_smaller
complement_cover_possible
```

含义是：当前弹出的树实际覆盖 `cover[v]` 可能大于名义 mask `S`，于是剩余集合：

```text
cover_rem = U ^ (cover[v] & U)
```

可能小于 `U^S`。若 `cover_rem` 足够小，理论上可能提前用两个已处理块拼出完整上界。

Test17 只记录这个现象，不使用它更新 `best`。真正使用该信息的是 Test18 的 cover-aware Complete。

## 5. 正确性

Test17 继承 Test16 的正确性。

early Complete 的安全性在于：它只跳过当前时刻不可能成功的完整拼接枚举；所有 DP 状态仍正常生成、传播和保存。当更大的块被处理出来后，后续弹出仍会执行 Complete。

`complete_pairs` 只是缓存枚举结果，不改变候选集合。

cover 相关字段只是统计，不影响算法行为。

## 6. 复杂度

early Complete 是纯跳过。

`complete_pairs` 的总规模由三进制归属计数控制：对每个组，它属于 `S`、`X` 或 `Y` 之一。因此预处理和遍历总账仍在：

```text
O(3^g n)
```

整体复杂度不超过 Test16：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

空间上额外保存 `complete_pairs`，规模与补集二分枚举账本一致。

## 7. 统计字段

Test17 在 Test16 基础上增加：

```text
complement_calls
complement_skip_early
complement_mask_pairs
complement_mask_empty
complement_cover_smaller
complement_cover_possible
```

重点看：

```text
complement_skip_early / complement_calls
complement_ms
```

若 skip 比例高且 `complement_ms` 明显下降，说明 Test17 的优化有效。

## 8. 已验证结果

```text
随机小图黑盒对拍：
  seed=271828   80 组通过

Toronto query_g10 前 5 条：
  Test16：7.590806s
  Test17：6.007595s
  speedup：20.86%
  bad=0
  max_diff=0

补集拼接统计：
  complement_skip_early / complement_calls = 2,646,549 / 2,966,386 = 89.22%
  complement_ms：Test16 2221.218ms -> Test17 950.121ms
```

结论：Test17 是一个干净的正优化，主要削掉大量过早且必然无效的补集枚举。
