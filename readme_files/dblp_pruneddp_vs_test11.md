# DBLP 上 PrunedDP 与 Test11 表现差异分析

本文解释为什么在多数数据集上 `Test11` 明显快于 baseline `PrunedDP`，但在 DBLP，尤其 `query_g10_uniform` 上，`PrunedDP` 反而可能更快。

## 新增诊断字段

`PrunedDP` 原本没有实际统计输出，现已补充：

```text
n / m / g
group_vertices / max_group
build_ms / dist_ms / calw_ms / search_ms / total_ms
initial_pushes
pq_push / pq_pop / stale / finalized
update_calls / update_finalized_skip / update_bound_pruned / update_push
best_full / best_expect
edge_relax
merge_enum / merge_hit / merge_gate
k=<size> final=<...> push=<...>
```

`Test11` 也补充：

```text
n / m / g
group_vertices / max_group
group_dist_ms / group_pair_ms / greedy_ms
```

这些字段用于大查询本地运行时拆分瓶颈。

## 小样本对比

本轮运行：

```powershell
.\build\Release\gst_pruned_dp_main.exe DBLP weight result_dblp_reason debug child_first g4_uniform data_new 1 1
.\build\Release\gst_test11_main.exe DBLP weight result_dblp_reason2 debug child_first g4_uniform data_new 1 1

.\build\Release\gst_pruned_dp_main.exe MovieLens weight result_dblp_reason debug child_first g4_uniform data_new 1 1
.\build\Release\gst_test11_main.exe MovieLens weight result_dblp_reason2 debug child_first g4_uniform data_new 1 1
```

### DBLP g4_uniform q1

```text
DBLP:
n=2497782
m=12786329
group_vertices=424
max_group=249
```

`PrunedDP`：

```text
total=10.309s
build_ms=2482.354
dist_ms=5528.131
calw_ms=881.973
search_ms=92.989
initial_pushes=424
pq_push=79702
pq_pop=79702
stale=76678
finalized=3024
edge_relax=1167975
merge_enum=21124
```

`Test11`：

```text
total=7.669s
prep_ms=6208.245
group_dist_ms=5721.477
greedy_ms=446.491
dp_ms=674.137
active_seed=2770
relax_try=2537159
up_subset=8102
```

结论：

```text
DBLP g4 上 Test11 仍快于 PrunedDP，但优势只有约 1.3x。
两者都主要被大图上的多源/单源 Dijkstra 预处理拖住。
```

### MovieLens g4_uniform q1

```text
MovieLens:
n=62423
m=35323774
group_vertices=19368
max_group=7719
```

`PrunedDP`：

```text
total=3.506s
build_ms=1310.866
dist_ms=792.090
calw_ms=21.171
search_ms=458.328
initial_pushes=19368
update_calls=27793135
update_bound_pruned=25658308
edge_relax=27791512
```

`Test11`：

```text
total=0.912s
prep_ms=721.971
group_dist_ms=707.640
greedy_ms=11.718
dp_ms=33.291
active_seed=19383
relax_try=993229
up_subset=1736
```

结论：

```text
MovieLens 上 Test11 明显快，因为 n 小得多，group_dist 预处理便宜；
PrunedDP 则在高边数图上 edge_relax/update_calls 很大。
```

## 结构差异解释

### 1. DBLP 的 n 极大

DBLP：

```text
n≈2.50M
m≈12.79M
```

MovieLens：

```text
n≈62K
m≈35.32M
```

虽然 MovieLens 边更多，但 `Test11` 的主要预处理是：

```text
g 次 group_dist Dijkstra over vertices
```

因此大 `n` 的 DBLP 对 `Test11` 很不友好。

### 2. DBLP 的 query group 很小

DBLP g4 q1：

```text
group_vertices=424
```

MovieLens g4 q1：

```text
group_vertices=19368
```

`PrunedDP` 的初始标签数与 group vertices 相关：

```text
DBLP initial_pushes=424
MovieLens initial_pushes=19368
```

这解释了为什么 DBLP 上 `PrunedDP` 的 search 阶段很小：

```text
DBLP search_ms=92.989
MovieLens search_ms=458.328
```

DBLP 的候选终端少，PrunedDP 的 label search 很容易被 predict 下界压住。

### 3. g10 时 Test11 的半集合状态爆炸

已有 DBLP `Test8 query_g10_uniform` 统计：

```text
query=2:
valid_total=258,587,165
up_subset=38,370,908,726
active_seed=1,327,953,962
dp_ms=5,007,798 ms
```

已有 DBLP `Test11 query_g10` 统计也显示：

```text
query=2:
valid_total=354,740,790
up_subset=20,505,116,935
bucket_scan=145,399,752,726
dp_ms=3,587,796 ms
```

`Test11` 的剪枝减少了很多 `up_subset`，但 DBLP g10 的状态规模仍然是十亿级扫描。

相对地，`PrunedDP query_g10_uniform` 现有权重文件显示可行查询耗时约：

```text
1165s ~ 2265s
```

虽然也很慢，但通常小于 `Test8/Test11` 的多千秒级 DP 爆炸。

## 为什么 uniform 上更明显

`uniform` 查询通常让组候选分布更均匀，更容易导致：

```text
更多 root 上出现更多 mask 组合
更广的 active_vertices
更大的同根合并扫描
```

对 `Test11` 来说，这会放大：

```text
active_seed
bucket_scan
up_subset
relax_try
```

而 `PrunedDP` 的下界 `predict` 是围绕 terminal/组构造的全局 lower bound，可能更容易在 DBLP 这类稀疏大图、少候选终端场景中剪掉标签。

## 当前判断

DBLP 慢的核心不是某一个局部常数，而是两类算法的瓶颈不同：

```text
Test11:
    大 n 导致 group_dist/greedy 预处理重；
    大 g 时半集合状态和同根合并仍爆炸。

PrunedDP:
    对 DBLP 小 group_vertices 很友好；
    search label 数少，predict 下界有效；
    主要成本在 dist/calw 预处理。
```

因此 DBLP 上 PrunedDP 可能更快，尤其在 g10 uniform 这类 Test11 状态爆炸时。

## 本地大数据建议观察字段

### PrunedDP

重点看：

```text
dist_ms
calw_ms
search_ms
initial_pushes
finalized
update_bound_pruned / update_calls
edge_relax
merge_enum / merge_hit
k=<size> final / push
```

如果：

```text
search_ms 很低
update_bound_pruned / update_calls 很高
```

说明 PrunedDP 的 predict 下界在该查询上非常有效。

### Test11

重点看：

```text
group_dist_ms
greedy_ms
dp_ms
active_seed
bucket_scan
up_subset
live_dp_checks
future_h_checks
prune_ge_best
prune_far
relax_try
k=<size> active / merge / valid
```

如果：

```text
bucket_scan 和 up_subset 上十亿
```

说明瓶颈仍是半集合同根合并爆炸。

如果：

```text
group_dist_ms + greedy_ms 占总时间大头
```

说明 DBLP 的大 n 预处理成本是主因。

## 后续优化方向

针对 DBLP，更可能有效的方向：

```text
1. 减少 group_dist/greedy 的大图 Dijkstra 成本。
2. 对 root bucket 做更强的 value-ordered 提前停止。
3. 对 DBLP 使用 PrunedDP 风格的全局 label-setting / predict 下界，而不是继续扩大半集合 DP。
4. 针对 group_vertices 很小的查询，考虑自动选择 PrunedDP。
```

短期工程上可以加一个选择策略：

```text
if graph == DBLP and g >= 10 and group_vertices small:
    prefer PrunedDP
else:
    prefer Test11
```

这不是算法改进，但符合当前数据表现。

## 大 g 完整对比后的更新

用户已运行：

```powershell
.\run_large_g_compare.ps1 -Build
```

覆盖：

```text
data_new/DBLP/query_g10_uniform.txt
data_new/DBLP/query_g10_nonuniform.txt
PrunedDP vs Test11
```

### g10_uniform

`PrunedDP` 可行查询耗时：

```text
q2  1404s
q3  1704s
q5  1163s
q6  1885s
q7  2147s
q8  1406s
q10 1661s
```

`Test11` 可行查询耗时：

```text
q2  2937s
q3  2392s
q5  1636s
q6  4927s
q7  2287s
q8  2559s
q10 2969s
```

结论：

```text
uniform 上 Test11 多数慢于 PrunedDP，q6 特别明显。
```

### g10_nonuniform

`PrunedDP` 耗时：

```text
约 785s ~ 1145s
```

`Test11` 耗时：

```text
约 33s ~ 44s
```

结论：

```text
nonuniform 上 Test11 仍然远快于 PrunedDP。
```

这说明“DBLP 不适合 Test11”并不是绝对结论；更准确地说：

```text
DBLP + uniform + 小 group_vertices + 弱上界/弱局部剪枝
会触发 Test11 的半集合状态爆炸。

DBLP + nonuniform + 大 group_vertices / 组重叠 / 更强早期上界
反而非常适合 Test11。
```

## uniform 与 nonuniform 的关键差异

### uniform 的 Test11 状态爆炸

DBLP g10 uniform 的 `Test11` 典型统计：

```text
q2:
valid_total=258,590,295
up_subset=15,692,438,429
bucket_scan=111,123,275,271
active_seed=379,688,163
dp_ms=2,913,390 ms

q6:
valid_total=441,166,020
up_subset=27,011,649,678
bucket_scan=200,821,813,160
active_seed=495,777,441
dp_ms=4,901,064 ms
```

虽然 `Test11` 的 `prune_ge_best/prune_far` 已经剪掉很多：

```text
q6:
prune_ge_best=5,090,520,598
prune_far=15,908,225,182
```

但剩余同根枚举仍是百亿级。

### nonuniform 的 Test11 被强力剪住

DBLP g10 nonuniform 的 `Test11` 典型统计：

```text
q1:
valid_total=149,885
up_subset=11,210
bucket_scan=243,518
dp_ms=13,819 ms

q2:
valid_total=973,671
up_subset=1,462,763
bucket_scan=12,909,792
dp_ms=17,307 ms

q7:
valid_total=2,840,652
up_subset=2,030,266
bucket_scan=16,780,761
dp_ms=20,195 ms
```

与 uniform 相比差了四到五个数量级。

原因可能是：

```text
nonuniform 组更大，组间更容易有近距离/重叠结构；
root-star / greedy upper 更容易贴近最优；
best 很早变小，cand+far 剪枝和 LB 剪枝更有效；
许多查询 best 接近 0~2，Dijkstra 扩展半径小。
```

uniform 则相反：

```text
group_vertices 较少；
组分散；
最优树权重较大；
早期上界不够紧；
大量中间 mask 在广泛 root 上保持“看似有希望”。
```

## 不能简单采用的方向

用户要求该方法需要形成新的论文，因此不建议：

```text
1. 按数据集混合选择 PrunedDP / Test11；
2. 直接照搬 PrunedDP 的全局 label-setting + predict 框架；
3. 把 PrunedDP 作为 DBLP fallback。
```

下面的方案尽量保持在“半集合 + 同根合并 + 组距离下界”的独立路线内。

## 建议的新研究方向

### 1. 用“状态价值排序”替代盲目按层全处理

uniform 的问题是：

```text
同一层内大量 mask/root 状态都会被处理，
但最终有效贡献很少。
```

可以保持 `popcount` 外层不变，但在同一层内部引入优先级：

```text
priority(mask,v) = dp[mask][v] + far(v,U^mask)
或 dp[mask][v] + LB(v,U^mask)
```

处理策略：

```text
同一 k 层中，先处理低 priority 的 mask/root；
一旦 best 被改善，后续同层大量状态被剪掉。
```

这不同于 PrunedDP 的全局 label-setting，因为：

```text
仍保持半集合分层；
不跨 popcount 混合状态；
只改变同层内调度顺序。
```

论文角度可以称为：

```text
Layer-preserving best-first half-set DP
```

### 2. 以 root 为单位的批量同根合并

当前 `Test11` 对每个 `(mask,v)` 都扫描同一个 root bucket：

```text
bucket_scan 可达 100B+
```

uniform 慢的直接原因就是 root bucket 被重复扫描。

可以在每个层 `k` 内按 root 分组：

```text
states_by_root[v] = all active / target masks at this root in current layer
```

然后对同一个 root：

```text
一次扫描 root_masks_by_size[v][s]
批量服务多个 mask
```

关键数据结构：

```text
pending masks of same root
按 mask 的 forbidden bits 建索引
扫描 t 时，找所有 (mask & t)==0 的 mask
```

这仍然是半集合 DP，不借用 PrunedDP。

论文角度：

```text
Root-batched same-root convolution for half-set GST DP
```

### 3. root bucket 按 value 排序，做 `best - dp` 提前停止

上一轮已经发现：

```text
cand + far(v,U^nxt) >= best
```

在 DBLP uniform 上剪枝命中非常高，但当前仍要枚举后才知道。

若对每个 root/size bucket 按：

```text
dp[t][v]
```

升序维护，则对固定 `(mask,v)`：

```text
dp[t][v] >= best - dp[mask][v]
```

之后可直接停止。

更强但更贵：

```text
dp[t][v] + far(v, U^(mask|t)) >= best
```

第一版只做 value cutoff，避免复杂。

这不是 PrunedDP 的 predict，而是同根合并枚举的数据结构优化。

### 4. uniform 专用的强初始上界

DBLP uniform 的根源之一是 `best` 不够早变紧。

当前 greedy 从 root-star root 出发，可能对均匀稀疏组不够强。可以尝试：

```text
多 root greedy：
    从 top-K root-star roots 启动组件增长；
    或从每组代表点附近启动；

MST-on-groups heuristic：
    在 group_pair metric closure 上求 MST；
    按 MST 边在图上连通实际组代表；
```

目标是只改上界，不改 DP 正确性。若 uniform 上 best 提前降低，`cand+far` 会直接剪掉更多同根枚举。

论文角度：

```text
upper-bound driven half-set pruning
```

### 5. 面向 DBLP 大 n 的 group_dist 局部化

DBLP 大 n 导致：

```text
group_dist_ms ≈ 13~15s per query
```

这不是 uniform 反转的最大头，但对所有 DBLP 都是固定成本。

可以研究：

```text
只对可能进入 DP 的 root 区域计算 group_dist；
按 best 半径截断多源 Dijkstra；
复用同一 query file 中多个查询的 group_dist（若组重复/相近）。
```

这条更多是工程/索引方向，论文性不如 root-batched merge 强。

## 推荐优先级

为了形成独立论文路线，建议优先：

```text
1. root-batched same-root merge
2. same-layer priority scheduling
3. value-ordered root buckets
4. stronger initial upper bounds
```

其中最直接针对 DBLP uniform 反转的是：

```text
root-batched same-root merge
```

因为 uniform 的最大痛点就是：

```text
bucket_scan / up_subset 爆炸
```

而不是预处理或单纯 Dijkstra。

## 建议下一步实验字段

为了验证 root-batched merge 是否值得做，建议在 `Test11/Test12` 再输出：

```text
per layer:
    distinct roots touched
    states per root distribution
    bucket_scan contributed by top roots
    repeated root bucket scan count

per root:
    root_masks_by_size sizes
    number of current-layer masks at this root
    estimated batched scan cost
    current scan cost
```

如果：

```text
current scan cost / estimated batched scan cost >> 1
```

就说明批量同根合并有论文级优化潜力。


