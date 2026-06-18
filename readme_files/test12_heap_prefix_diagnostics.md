# Test12 Dijkstra 前缀入堆诊断

`Test12` 从 `Test11` 派生，目标是研究当前 Dijkstra 阶段中“非目标前缀节点”的必要性。

背景：

```text
处理某个 mask 时，真正会调用 Modify 的目标点需要满足：
dp[mask][v] + h[U^mask][v] <= best
dp[mask][v] + LB(v,U^mask) <= best
```

但为了让目标点的最短路值正确，算法还会把一些非目标点加入堆作为路径前缀。

## Test12 改动

`Test11` 中，非目标前缀点满足：

```text
dp[mask][v] < mx_used
```

就会入堆。

`Test12` 改成：

```text
dp[mask][v] < mx_used
并且 dp[mask][v] + LB(v,U^mask) <= best
```

理由是当前 `LB` 是一致的。若某个前缀点已经：

```text
dp + LB > best
```

则经过它继续扩展出的节点也不应得到 `dp+LB<=best` 的候选路径。

## 新增统计

```text
target_seed          原始目标入堆数
prefix_considered    满足 dp<mx_used 的非目标前缀候选数
prefix_pushed        实际入堆的前缀数
prefix_pruned_lb     因 dp+LB>best 而不入堆的前缀数

target_promoted      relax 时由 target 状态传播成 target 的节点数
target_demoted       relax 时 target 被 prefix 更优路径覆盖的次数
modify_original      原始目标触发 Modify 次数
modify_promoted      传播得到的 target 触发 Modify 次数

pop_target           出堆时 status 为 target 的次数
pop_prefix           出堆时 status 非 target 的次数
pop_skip_stale       stale key 跳过次数
pop_skip_lb          出堆时 dp+LB>best 跳过次数
```

## 实测

对比 `Test11` 与 `Test12`。

### MovieLens g8_uniform q1

```text
Test11:
best=0.0012311587
time=2.507906s

Test12:
best=0.0012311587
time=2.507422s
prefix_considered=523
prefix_pushed=393
prefix_pruned_lb=130
modify_original=2247
modify_promoted=3604
```

### MovieLens g10_uniform q1

```text
Test11:
best=0.0004877212
time=4.126584s

Test12:
best=0.0004877212
time=3.861610s
prefix_considered=1535
prefix_pushed=1087
prefix_pruned_lb=448
modify_original=4292
modify_promoted=1160
```

### Toronto g10 q1

```text
Test11:
best=0.4475349050
time=0.736363s
dp_ms=569.309
pq_push=115810
pq_pop=113584

Test12:
best=0.4475349050
time=0.776413s
dp_ms=611.090
prefix_considered=14209
prefix_pushed=10334
prefix_pruned_lb=3875
pq_push=111935
pq_pop=111703
modify_original=25000
modify_promoted=43293
```

## 结论

1. `dp+LB>best` 的非目标前缀点可以不入堆，当前样本权重与 `Test11` 一致。
2. 该优化确实减少 `pq_push/pq_pop`，但节省量相对于总 Dijkstra 工作不一定大。
3. `modify_promoted` 很多，说明当前 status 传播会让大量非原始目标点最终调用 `Modify`。这是后续更值得研究的方向。
4. `target_demoted` 存在但数量较小，说明一些原始目标会被非目标前缀的更优路径覆盖。

下一步建议研究：

```text
是否能减少 promoted target 的 Modify；
或者把 promoted target 只用于修正路径，不立即做完整同根枚举。
```

## 同根枚举效果诊断

继续在 `Test12` 中增加 `Modify` 级别统计，用于判断哪些状态可以不做同根枚举，或延迟到之后批量处理。

新增字段：

```text
modify_calls          Modify 总次数
modify_init           初始化 singleton 的 Modify 次数
modify_original       原始目标触发 Modify 次数
modify_promoted       由 target 传播得到的 promoted 目标 Modify 次数

modify_no_effect      本次 Modify 没有产生 best/live-dp/h 任一有效更新
modify_best_effect    改善 best 的 Modify 次数
modify_live_effect    产生 live-dp 更新的 Modify 次数
modify_h_effect       产生 h 更新的 Modify 次数
modify_only_h         只产生 h 更新的 Modify 次数
modify_only_live      只产生 live-dp 更新的 Modify 次数
modify_multi          同时产生多类效果的 Modify 次数

no_best_cand          没有 best 候选
no_live_cand          没有 live-dp 候选
no_h_cand             没有 h 候选
no_cand_all           三类候选都不存在，可提前跳过同根枚举

init_no_effect        初始化阶段无效果
original_no_effect    原始目标无效果
promoted_no_effect    promoted 目标无效果
original_only_h       原始目标只产生 h
promoted_only_h       promoted 目标只产生 h
```

### Toronto g10 q1 结果

```text
modify_calls=70314
modify_init=2021
modify_original=25000
modify_promoted=43293

modify_no_effect=38199
modify_best_effect=15
modify_live_effect=23456
modify_h_effect=16762
modify_only_h=8656
modify_only_live=15344
modify_multi=8112

no_best_cand=68617
no_live_cand=11150
no_h_cand=42561
no_cand_all=10583

init_no_effect=2000
original_no_effect=11444
promoted_no_effect=24755
original_only_h=3624
promoted_only_h=5032
```

### 观察

1. 可提前判定完全无候选的状态不少：

```text
no_cand_all=10583
```

这些状态不需要扫描任何同根 bucket，可直接跳过 `Modify` 的同根枚举部分。

2. 真正无效果的状态更多：

```text
modify_no_effect=38199
```

但其中不少是“有候选，枚举后发现不能改善”，不能仅靠简单候选存在性提前判定。

3. promoted target 是主要问题：

```text
modify_promoted=43293
promoted_no_effect=24755
promoted_only_h=5032
```

也就是说，promoted 状态里约 57% 完全无效果，约 12% 只产生 h。它们很适合做延迟或批处理：

```text
promoted target 不立即做完整同根枚举；
先只登记为 pending；
在 layer 结束时批量处理 h-only 或有 live-dp 需求的状态。
```

4. 初始化 singleton 大多无效果：

```text
modify_init=2021
init_no_effect=2000
```

初始化阶段可以改成：

```text
只 SetDp(singleton,v,0)
不立即 Modify
等所有 singleton 初始化完后，再按 root 批量处理有多个 singleton 的 root。
```

这能减少大量无效同根枚举，尤其在组候选点分散时。

## 新的候选优化方向

### 1. no-candidate fast path

在 `Modify` 开头先判断：

```text
best candidate 是否存在
live-dp bucket 是否可能非空
h bucket 是否可能非空
```

若三者都不存在：

```text
只记录 valid 状态，不做任何同根枚举
```

这是安全且低成本的。

已做实现验证：对 `init/promoted` 状态启用 disjoint 精确预检查；`original target` 不做该预检查，避免给主路径增加过多额外扫描。

Toronto g10 q1：

```text
fast_no_candidate=9563
precheck_calls=45326
precheck_scans=83873
precheck_no_candidate=9563
bucket_scan=12895477
dp_ms=584.327
```

对比同一批逻辑未启用精确 precheck 时：

```text
fast_no_candidate=0
bucket_scan=12912765
dp_ms≈611~633
```

结论：

```text
no-candidate fast path 是有效的；
它能稳定跳过一批 promoted/init 的无候选 Modify；
预检查扫描量远小于主同根枚举量，值得保留。
```

### 2. 初始化批处理

初始化阶段先执行：

```text
SetDp(1<<gi, v, 0)
```

不调用完整 `Modify`。

所有 singleton 插入完成后，对每个 root：

```text
若 root 上 singleton 数 >= 2，再批量做同根合并
```

这样可以避免绝大多数 `init_no_effect`。

### 3. promoted 延迟处理

promoted target 的 `Modify` 可拆成两部分：

```text
原始目标: 立即 Modify
promoted 目标: 先 pending
```

layer 结束或 mask 处理结束时，批量处理 pending promoted：

```text
先按是否可能有 live-dp 候选分组
无 live-dp 但可能 h-only 的状态批量更新 h
完全无候选的丢弃
```

这需要证明不会影响当前 mask 的 Dijkstra 正确性。因为 `Modify` 只影响其他 mask 的同根合并，不影响当前 mask 的最短路扩展，所以延迟到当前 mask Dijkstra 结束后通常是安全的。

已做实现验证：

```text
promoted target 出堆时不立即 Modify；
先放入 pending_promoted；
当前 mask 的 Dijkstra 结束后，按 vertex 去重并批量 Modify。
```

Toronto g10 q1：

```text
pending_promoted=43305
pending_unique=43305
modify_promoted=43305
```

本样本中去重没有收益，说明同一 mask 下 promoted target 基本不会重复出堆。但延迟本身保持权重正确，说明：

```text
promoted Modify 可延迟到当前 mask Dijkstra 结束后；
但仅靠去重不能减少复杂度；
真正有效的批处理需要合并同 root 的同根枚举。
```

下一步如果继续研究批处理，应该按 root 聚合 pending：

```text
pending_by_root[v] = list of (mask, value)
```

然后对同一个 root：

```text
一次性扫描 root_masks_by_size[v]
批量处理多个 pending mask
```

这才可能减少同一个 root bucket 被重复扫描的次数。

已补充同层 root 重复度统计：

```text
pending_layer_roots
pending_layer_repeat
max_pending_roots_layer
```

Toronto g10 q1：

```text
pending_promoted=43305
pending_unique=43305
pending_layer_roots=11770
pending_layer_repeat=31535
max_pending_roots_layer=7532
```

解释：

```text
同一个 mask 内 promoted 几乎不重复，所以按单 mask 去重无效；
但同一 popcount 层内 promoted root 大量重复，约 73% 是重复 root。
```

这说明真正可能降低复杂度的延迟批处理应该跨越“同一 size 层”：

```text
pending_by_layer_and_root[v].push_back((mask, value))
```

层结束时对每个 root：

```text
一次扫描 root_masks_by_size[v]
批量服务该 root 下多个 pending mask 的 live-dp / h 需求
```

正确性注意点：

```text
不能随意延迟 original target；
promoted target 的 Modify 只影响其他 mask 的同根合并，不影响当前 mask 的 Dijkstra 最短路；
但跨整层延迟可能影响同层其他 mask 的 h 目标筛选，所以第一版应只延迟 live-dp 部分，h 部分仍需谨慎验证。
```


