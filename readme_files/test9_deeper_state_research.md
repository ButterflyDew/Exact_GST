# Test9 深入状态优化理论研究

本文继续研究 `Test8/Test9` 的状态空间和转移顺序优化，重点不再只看 `mask` 大小，而是同时考虑：

- 状态 `(mask, v)` 中的根顶点 `v` 是否可剪。
- 同根合并和图上扩展的顺序是否可以重排。
- subset 自身的结构是否可以用更好的数据结构组织。
- 哪些方案能严格保证正确性，哪些只能作为启发式或诊断。

本文中的“安全”指不会改变最优值；“候选”指需要额外证明或只适合先做统计。

## 1. 当前依赖回顾

当前 `Test9` 仍沿用 `Test8` 的主流程：

```text
dp[mask][v] = 覆盖 mask，并以 v 为当前连接点的最小代价
h[x][v]     = 同根合并中观察到的、可作为 x 一侧组成部分的最大已确认子状态代价
```

对每个 `|mask| <= H=floor(g/2)` 的小侧状态：

1. 遍历 `active_vertices[mask]`。
2. 用 `dp[mask][v] + h[U^mask][v] <= best` 找目标点。
3. 用 `dp[mask][v] + LB(v,U^mask) <= best` 做安全剪枝。
4. Dijkstra 扩展当前 mask。
5. 目标点出队时调用 `Modify(mask,v)`，同根合并 disjoint mask。

因此，状态 `(mask,v)` 的用途有三类：

```text
图上扩展:      作为当前/未来 Dijkstra 的 source label
同根合并:      与同一个 v 上的 disjoint mask 合并
h 目标剪枝:    作为未来 h[U^mask][v] 的证据
```

任何剪枝都必须说明它不会破坏这三类用途。

## 2. 结合 v 的状态剪枝

### 2.1 基于 lower bound 的局部死亡

已有剪枝：

```text
dp[mask][v] + LB(v, U^mask) > best
```

该状态无法完成更优完整解，因此不需要进入当前 Dijkstra。

进一步的安全观察：

若一个状态 `(mask,v)` 被该式剪掉，它仍可能作为同根合并的 `t` 被其他 mask 使用，因为同根合并时完成剩余组的 root 还是 `v`，而 `dp[t][v]` 可能作为另一侧的一部分。也就是说：

```text
不能仅因 dp[mask][v]+LB(v,U^mask)>best 就删除 dp[mask][v]
```

但可以增加一个更强的“全用途死亡”条件。

对任意未来 disjoint `s`，若 `mask` 与 `s` 合并后仍可能有价值，则：

```text
dp[mask][v] + dp[s][v] + LB(v, U^(mask|s)) <= best
```

如果能为所有可能的 `s` 给出下界 `MinPartner(v, rem)`，则可安全删除：

```text
dp[mask][v] + MinPartner(v, U^mask) > best
```

一个保守的候选是：

```text
MinPartner(v, rem) = min_{非空 t subset rem} dp[t][v] + LB(v, rem^t)
```

这本身很贵，但可以按 root `v` 做动态维护：

```text
partner_lb[v][rem] = min over finite t subset rem of dp[t][v] + LB(v, rem^t)
```

该结构类似 subset DP / zeta min-plus 变体。它可能比直接存所有大侧状态更贵，但可作为诊断字段：

```text
root_partner_prunable++
```

优先级：中。理论安全，但实现和维护代价不低。

### 2.2 Root dominance：同一 mask 下的 v 支配

设同一个 `mask` 有两个 root：`u` 和 `v`。如果：

```text
dp[mask][u] + dist(u,v) <= dp[mask][v]
```

那么 `(mask,v)` 在图上扩展意义上被 `(mask,u)` 支配，因为从 `u` 走到 `v` 不比直接拥有 `(mask,v)` 更差。

但这并不自动支配同根合并用途。原因是 `(mask,v)` 可以和 `dp[t][v]` 直接合并，而 `(mask,u)` 只能和 `dp[t][u]` 合并。若 `dp[t][u]` 不存在或很差，删除 `(mask,v)` 可能影响合并。

安全版本需要同时满足对所有未来 disjoint `t`：

```text
dp[mask][u] + dist(u,v) + dp[t][v] >=? 
```

这不能直接替换为 `dp[t][u]`，除非已知：

```text
dp[t][u] <= dp[t][v] + dist(u,v)
```

该不等式在 Dijkstra 对 `t` 完全收敛后成立，但当前半集合算法并不对所有大侧 `t` 图上扩展。于是得到结论：

```text
同 mask 的 root dominance 可用于 Dijkstra seed 减少，但不能直接删除 dp cell。
```

可做安全弱化：

- 当前 mask 的 Dijkstra 入队前，若 `(mask,v)` 被某个 `(mask,u)` 通过 `dist(u,v)` 支配，则 `v` 不必作为初始 seed 入堆。
- 但仍保留 `dp[mask][v]` 供同根合并查询。

实现难点是不能做 APSP。可用当前 mask 的多源 Dijkstra 自然完成这个支配：较差 seed 即使入堆也会被更好路径覆盖。因此显式做 root dominance 的收益可能有限。

优先级：低到中。理论上可解释，但不一定比现有 Dijkstra 便宜。

### 2.3 以组距离签名压缩 v

对剩余组 `rem`，当前剪枝主要依赖：

```text
group_dist[a][v]
LB(v, rem)
vertex_group_mask[v]
```

若两个顶点在这些值上非常接近，可能表现相似。但精确算法不能用近似签名合并顶点，因为边结构和同根合并值 `dp[t][v]` 不同。

安全可用的版本是“等价类诊断”：

```text
signature(v) = (vertex_group_mask[v], rank_bucket(group_dist[0..g-1][v]))
```

统计每个 mask 的 active vertices 中，同一 signature 的数量。如果高度集中，可考虑后续做启发式 beam 或代表点上界，但不能作为精确剪枝。

优先级：诊断型，非精确剪枝。

### 2.4 必经区域 / 近最短路区域剪枝

对状态 `(mask,v)`，如果 `v` 离所有剩余组都远，`LB` 已能剪掉一部分。更强的 v 剪枝可以借鉴 shortest path A* 的“椭圆区域”：

```text
dp[mask][v] + lower_to_remaining(v) <= best
```

Test8 的 `LB` 是一个版本。可以继续加强 `lower_to_remaining`：

- `MST(rem)` 当前只在组间 metric closure 上算。
- 可加入当前 root 到 rem 的 1-tree 更强项。
- 可加入“组件到至少 r 个剩余组”的 top-r 距离下界。

候选下界：

```text
LB2(v,rem) = max(
    far(v,rem),
    mst(rem)/2 + near2(v,rem)/2,
    (mst(rem) + near1(v,rem)) / 2
)
```

第三项未必总强，需证明。更稳妥的是 general 1-tree / terminal MST lower bound：

```text
LB_tree(v, rem) = MST({v} union rem_group_metric) / 2 或 MST({v} union rem_group_metric) 的安全变体
```

若使用组距离作为 `v` 到组的边，MST 的一部分仍是安全下界，但取半还是取全需要谨慎证明。建议先作为诊断计算：

```text
lb2_pruned_would
```

优先级：中。可能减少 v 状态，但预处理/按点计算成本需要控制。

## 3. 转移顺序重排

### 3.1 从 popcount 分层到全局 label-setting

经典 Dijkstra-Steiner / DS* 的思路是把 `(v,mask)` 全部作为 label，用全局优先队列按：

```text
key = dp[mask][v] + lower_bound(v,U^mask)
```

选择下一个永久 label，而不是按 `popcount(mask)` 分层。

优点：

- 永远先处理最可能接近最优解的状态。
- 很多高 cost 状态可能永远不需要生成。
- 与 admissible heuristic / future cost 的理论相容。

风险：

- 当前半集合性质依赖按小侧 `|mask|<=H` 控制状态范围。
- 全局 label-setting 中，同根合并会不断生成不同 size 的 label，生命周期更复杂。
- `h` 目标剪枝目前是按 mask Dijkstra 设计的，可能需要重写。

安全路径：

1. 保持只允许 `|mask|<=H` 的图上扩展。
2. 全局堆只放小侧 label。
3. 同根合并只用于更新小侧 label或直接更新 `best`，不存死亡大侧。
4. 用 `best` 与 lower bound 统一剪枝。

这将变成一个新的 `Test10` 级别实验，不建议直接改 `Test9`。

优先级：高潜力，高改动。

### 3.2 延迟同根合并

当前 `Modify(mask,v)` 一永久化就立即枚举同根 disjoint `t`。但许多合并结果可能：

- 不是小侧，不能进堆。
- 不是未来需要的 complement。
- 不能改善 `best`。

可以改成延迟合并：

```text
状态永久化 -> 只登记到 root bucket
处理某个 mask 前 -> 只生成当前 mask 所需的 h[U^mask] 和小侧候选
```

也就是从 push-based merge 改为 pull-based merge。

当前 `h[U^mask][v]` 需要知道是否存在 complement 侧。可以在处理 mask 时按 root `v` 查询：

```text
best_complement[v] = min or max over t subset U^mask of dp[t][v]
```

注意现有 `h` 存的是 max confirmed component cost，用于目标判定，而不是普通 min。需要重新审视其语义。若能把 `h` 改成更直接的 complement-min：

```text
comp[v][rem] = min dp[rem_side][v]
```

则目标条件更自然：

```text
dp[mask][v] + comp[v][U^mask] <= best
```

但这会改变 Test5 原有逻辑，需要单独验证。优点是 pull 查询可以避免大量无用 `h[nxt]` 写入。

优先级：高，建议先做理论小样本模拟。

### 3.3 分离 best 更新与中间状态生成

当前同一个 `RelaxSameRoot` 同时做三件事：

```text
更新 dp[nxt][v]
更新 h[nxt][v]
若 nxt==U 更新 best
```

Test9 已经把 `nxt==U` 分离为 best-only。还可以继续分离：

- `best` 更新只需要完整 disjoint pair。
- 小侧 `dp` 更新只需要 `|nxt|<=H` 或 `|nxt|<=g-k` 的 live 中间态。
- `h` 更新只需要 complement 大小区间 `[g-H,g-k]`。

这说明同根合并可以拆成三个不同枚举器：

```text
EnumerateBestPairs(mask,v)
EnumerateLiveSmallDp(mask,v)
EnumerateFutureH(mask,v)
```

每个枚举器的合法 `t` size 区间不同。这样可以用按 size 分桶的 `root_masks_by_size[v][sz]` 直接跳过无关 size。

这是当前代码最容易继续推进的方向。

优先级：高，改动可控。

## 4. subset 结构与数据结构思想

### 4.1 root_masks 按 popcount 分桶

当前：

```text
root_masks[v] = 所有 finite mask
```

处理 `|mask|=k` 时，真正有用的 `t` 只落在少数 size：

```text
best:        |t| = g-k 且 t = U^mask
live dp:     |t| <= g-2k
future h:    g-H-k <= |t| <= g-k
```

因此可维护：

```text
root_masks_by_size[v][s]
```

同根合并时只扫相关 size 桶。正确性不变，只是枚举顺序过滤。

对 `g<=22`，每个 root 最多 23 个桶，成本低。该方案还能更好地支持生命周期释放。

优先级：很高。

### 4.2 disjoint 查询的 bitset 加速

对固定 root `v` 和当前 `mask`，需要找：

```text
t & mask == 0
```

如果 `root_masks[v]` 很长，逐个检查 `(t & mask)==0` 成本高。可为 root-local masks 建 bitset：

```text
has_mask[v][mask] = 1 if dp[mask][v] finite
```

但完整 bitset 是 `n*2^g`，又回到 dense 空间。

折中：

- 只对 hot root 建 compressed bitset。
- 只对出现过的 size 桶建 sorted vector。
- 对当前 `rem=U^mask` 枚举 submask，并查 root-local index。

Test8 已经在 `root_masks` scan 和 dense submask enum 之间取小者。下一步可把 dense enum 改成“按允许 size 枚举 submask”，减少死亡候选：

```text
for s in allowed_sizes:
    enumerate submasks of rem with popcount s
```

需要预处理 `submasks_by_mask_and_size` 吗？`g<=22` 下预处理全量可能大；可动态 Gosper 枚举 rem 的 size-s 子集。

优先级：高，尤其适合 `t_size_filter` 大的查询。

### 4.3 ZDD / Trie 存 root masks

`root_masks[v]` 是一个 mask 集合，天然可用 subset trie 或 Zero-suppressed Decision Diagram 表示，支持：

```text
列出所有 disjoint t
列出指定 size 范围内的 disjoint t
```

优点：

- 当 root 上 mask 集合具有共享前缀/稀疏性时，空间低。
- disjoint 查询可以沿 bit 位跳过冲突分支。

缺点：

- 实现复杂。
- 对 `g<=22`，普通 vector + size bucket + flat index 可能已经足够。

优先级：低到中，除非 `root_masks` 非常大且结构高度可压缩。

### 4.4 SOS / subset convolution 类结构

对每个 root `v`，同根合并本质是：

```text
dp_new[S] = min_{T subset S} dp[T][v] + dp[S-T][v]
```

这是 min-plus subset convolution。若对每个 root 全量做，复杂度和空间都不可接受。但可以局部用于 hot root：

- root 上 finite mask 很多。
- 同根合并占主瓶颈。
- `g` 不大，比如 10-16。

可做 hot-root 局部闭包：

```text
对某个 v，批量计算它的 mask 合并闭包
```

风险是会生成大量无用 mask。必须结合生命周期 size 范围，只算 live sizes。

优先级：中，适合实验，不适合第一版。

## 5. 更强但仍安全的状态支配

### 5.1 同根 subset 支配

若同一 root `v` 上：

```text
A subset B
dp[B][v] <= dp[A][v]
```

那么 `A` 在完成完整解方面被 `B` 支配，因为 `B` 已覆盖更多组且代价不高。

但是否能删除 `A` 要看用途：

- 作为最终 best 的一侧：可被 `B` 替代。
- 作为中间生成精确 `nxt=A|t`：替代为 `B|t` 会得到更大的 mask，可能跳过某些中间小侧状态。

因此安全删除条件更强：

```text
A 的所有未来用途都不需要精确生成 A|t 这个 mask
```

在当前调度中很难全局保证。可做弱版本：

- 若 `A` 已经不会进入图上扩展。
- 且 `A` 只可能用于 best 更新或 h/complement。
- 则可尝试用 superset dominance 删除。

优先级：中，需生命周期辅助。

### 5.2 根内 top-k / skyline

对每个 root `v`，维护 mask 的 skyline：

```text
不存在 B superset A 且 dp[B][v] <= dp[A][v]
```

不直接删除非 skyline，而是同根合并扫描时优先 skyline；非 skyline 只在需要精确小侧 mask 时使用。

这是一种排序/调度优化，不改变正确性。

优先级：中。

### 5.3 基于 upper bound 的 subset upper table

Steiner DP 文献常用“若 label cost 超过该子问题 upper bound，则丢弃”。对每个 mask 可以维护：

```text
UB[mask] = 当前已知覆盖 mask 的可行树上界
```

若：

```text
dp[mask][v] >= UB[mask]
```

则 `(mask,v)` 可能对该 mask 的最优子树无贡献。但它仍可能和其他 mask 在同 root 合并形成全局最优吗？

如果 `UB[mask]` 是任意 root 的覆盖 mask 上界，则对同样覆盖 mask 的更差 root，仍可能因 root 位置更适合连接其他组而有价值。因此不能只用 `UB[mask]` 删除。

更安全的是 root-aware upper：

```text
dp[mask][v] + LB(v, U^mask) > best
```

这已是当前做法。若要更强，需要更强 `LB`，而不是纯 `UB[mask]`。

优先级：低，除非配合更强 future cost。

## 6. 推荐后续推进顺序

综合正确性风险和实现成本，建议按以下顺序推进：

1. `root_masks_by_size`：把同根合并拆成 best / live-dp / future-h 三个枚举范围，先减少扫描，不改状态定义。
2. `h` complement-only：只保留 `[g-H,g-k]` 范围内未来会被查询的 `h`，当前 Test9 已证明方向可行，可进一步工程化。
3. 低常数 sparse dp/h：统一 cell 数组 + root/mask buckets + flat index，替换 dense 表。
4. hot-root size-limited submask enumeration：当 root bucket 很大时，按允许 size 枚举 `rem` 子集并用 root-local index 查。
5. pull-based delayed merge：大改调度，把无用 `h` 写入变成按需 complement 查询。
6. 全局 label-setting / DS* 风格：潜力最大，但会重写当前半集合流程，建议另开 Test10。

## 7. 下一轮 Test9 诊断建议

为了决定第 1 步收益，应新增这些计数：

```text
merge_best_candidates       nxt==U 的候选数
merge_live_dp_candidates    会写 live dp 的候选数
merge_future_h_candidates   会写 future h 的候选数
merge_dead_candidates       三类都不需要的候选数
root_bucket_scan_by_size[k] 各 size 桶扫描量
hot_root_count              root_masks[v] 超过阈值的 root 数
hot_root_scan_share         hot roots 贡献的 scan 占比
```

如果 `merge_dead_candidates / total_up_subset` 很高，优先做 size-bucket 枚举过滤。  
如果 `hot_root_scan_share` 很高，优先做 root-local index。  
如果 `future_h_candidates` 很少，优先把 `h` 改成 complement-only sparse 表。

