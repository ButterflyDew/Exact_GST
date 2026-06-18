# Test9 空间诊断说明

`Test9` 是 `Test8` 的副本，用于研究把当前 `O(n 2^g)` 的 dense `dp/h` 表改成“只保留需求状态”的可行性。它不改变求解逻辑，仍使用 dense 表求解，只额外输出空间相关诊断字段。

## 目标问题

`Test8` 当前同时维护：

```text
dp[mask][v]
h[mask][v]
```

二者都是 `2^g * (n+1)` 的 dense 表。`active_vertices[mask]` 已经减少了扫描时间，但没有减少底层表空间。

`Test9` 用于回答：

```text
实际 finite 的 (mask, v) 有多少？
哪些 size 层占空间？
若只保留当前窗口 size in [k, g-k]，理论峰值是多少？
大侧 mask 是否只是同根合并产物，是否值得换成稀疏结构？
```

## 新增可执行文件

```text
gst_test9_main.exe
```

运行示例：

```powershell
.\build\Release\gst_test9_main.exe example weight result_test9 debug child_first
.\build\Release\gst_test9_main.exe DBLP weight result_test9 debug child_first g10 data 1 3
.\build\Release\gst_test9_main.exe DBLP weight result_test9 debug child_first g4_uniform data_new 1 5
```

其中最后两个参数是可选查询范围：

```text
query_begin query_limit
```

例如 `8 1` 表示只跑原查询文件中的第 8 条。

## 输出字段

`test9_stats.txt` 继承 `Test8` 的字段，并新增以下空间诊断。

Dense 表规模：

```text
dense_one       一个 dense 表的 cell 数，约为 (2^g) * (n+1)
dense_dp_h      dp + h 两张 dense 表的 cell 数
```

实际 finite 状态：

```text
dp_seen         dp[mask][v] 首次 finite 的 cell 数，不含隐式 dp[0][v]
h_seen          h[mask][v] 首次有效的 cell 数
dp_sparse_zero  dp_seen + n，用于估计把 dp[0][v]=0 作为特殊层后的稀疏 dp 总 cell
sparse_dp_h_zero dp_sparse_zero + h_seen
```

大侧状态：

```text
dp_large        popcount(mask) > floor(g/2) 的 dp finite cell
h_large         popcount(mask) > floor(g/2) 的 h effective cell
dead_nxt_skip   因 |nxt| > g-k 而不再持久化 dp[nxt][v] 的次数
dead_h_skip     因 h[nxt][v] 未来不会被 h[U^mask] 查询而不再持久化的次数
t_size_filter   在同根合并中，按 |t| > g-2k 可提前过滤的候选次数
full_best_hits  nxt == U 时只更新 best、不再写 dp[U]/h[U] 的次数
full_best_improve 上述 best-only 分支实际改善 best 的次数
dp_skip_high    因高层死亡规则跳过的 dp 存储次数
h_skip_low      因 |nxt| < g-H 跳过的 h 存储次数
h_skip_high     因 |nxt| > g-k 跳过的 h 存储次数
h_skip_full     因 nxt == U 跳过的 h 存储次数
```

Mask 覆盖：

```text
dp_masks        有至少一个 finite dp cell 的 mask 数
h_masks         有至少一个有效 h cell 的 mask 数
root_mask_entries root_masks[v] 的总条目数
max_root_masks  单个根 v 上最多出现多少 mask
```

窗口保留模拟：

```text
peak_window_layer  当前模拟下 dp+h 保留峰值出现在哪一层 k
peak_window_dp     峰值层保留的 dp cell
peak_window_h      峰值层保留的 h cell
peak_window_dp_h   峰值层 dp+h cell
peak_lifecycle_layer 理论生命周期集合的峰值层
peak_lifecycle_dp    理论生命周期集合中保留的 dp cell
peak_lifecycle_h     理论生命周期集合中保留的 h cell
peak_lifecycle_dp_h  理论生命周期集合中保留的 dp+h cell
```

按 size 分布：

```text
size=<s> dp=<...> h=<...> dpm=<...> hm=<...>
```

其中 `dp/h` 是该 size 的 cell 数，`dpm/hm` 是该 size 下出现过的 mask 数。

按层窗口：

```text
win_k=<k> keep_dp=<...> keep_h=<...> drop_low_dp=<...> drop_high_dp=<...>
```

这里窗口定义为：

```text
k <= popcount(mask) <= g-k
```

它对应用户提出的“处理 size=k 时，size<k 和 size>g-k 的状态可能不需要保留”的观察。注意：这是诊断模拟，不是正确性证明。当前 `Test8` 的同根合并仍会在处理较大 mask 时读取较小 disjoint mask，因此真正删除低层状态需要重新设计合并调度或补充事件索引。

理论生命周期集合：

```text
life_k=<k> keep_dp=<...> keep_h=<...> drop_dp=<...> drop_h=<...>
```

其中 `dp` 的保留集合按用途取并集：

```text
[k,H]        // 未来图上扩展
[0,g-2k]     // 作为未来同根合并的小侧 counterpart
[g-H,g-k]    // 作为未来 h/complement 查询相关的大侧
```

`h` 只需要保留未来会以 `h[U^mask]` 查询到的大小：

```text
[g-H,g-k]
```

这比简单窗口 `[k,g-k]` 更保守，但更贴近当前 `Modify` 调度下的真实依赖。

## Test9 当前安全过滤

`Test9` 现在不只是诊断，也实现了两个理论上安全的存储过滤：

1. `nxt == U` 时只更新 `best`，不再持久化 `dp[U][v]` 或 `h[U][v]`。
2. 当前处理 `|mask|=k` 时，若 `nxt != U` 且 `|nxt| > g-k`，则不再持久化 `dp[nxt][v]`；若 `|nxt|` 不在 `[g-H,g-k]`，则不再持久化 `h[nxt][v]`。

这两条不改变图上扩展顺序，也不改变 `best` 的候选来源。它们只避免写入未来不会入堆、不会作为 disjoint counterpart、也不会被 `h[U^mask]` 查询到的状态。

## 已做冒烟

`example`：

```text
query=1 best=8
dense_dp_h=384
sparse_dp_h_zero=111
dead_nxt_skip=18
dead_h_skip=65
peak_lifecycle_dp_h=100
```

`data_new/DBLP/query_g4_uniform.txt` 前 2 条：

```text
query=1 best=4.7633664017
dense_dp_h=79929056
dp_seen=11540
h_seen=6054
sparse_dp_h_zero=2515376
dead_nxt_skip=29
dead_h_skip=6128
peak_lifecycle_dp_h=17594
life_k=2 keep_dp=3949 keep_h=3949 drop_dp=7591 drop_h=2105
```

第 2 条无解，可行性判断直接返回：

```text
query=2 best=-1
```

## 初步结论

1. Dense `dp/h` 的主要浪费非常明显。即使 `g=4`，`data_new/DBLP` 因为 `n` 很大，`dense_dp_h` 已接近 8000 万 cell；过滤后的实际 finite `dp+h` 约 1.76 万 cell。
2. `h` 的生命周期比 `dp` 窄很多。安全过滤后，`data_new/DBLP g4_uniform` 第 1 条的 `h_seen` 从原诊断版约 12016 降到 6054。
3. 仅把 `dp/h` 改成稀疏 `(mask,v)->value` 存储，就可能大幅降低空间，但会影响随机访问和同根合并速度。
4. 用户提出的窗口思想很有潜力，但不能直接在当前 `Test8` 调度中删除 `size<k` 状态，因为较大 mask 的同根合并仍可能需要读取较小 disjoint mask。
5. 若要真正实现窗口删除，需要把同根合并从“当前 mask 查所有 root_masks[v]”改成更明确的生命周期模型，或者保留一个压缩的历史索引用于回答较大 mask 对小侧状态的需求。

## 低常数稀疏存储设计

后续若要把 `dp/h` 从 dense 表改成稀疏表，建议不要直接用 `std::unordered_map<pair<int,int>, double>`。更合适的是按当前访问模式设计专用结构。

### 推荐结构：统一 cell 数组 + 双向 bucket + flat index

核心数据：

```text
cells        连续数组，存 {mask, v, value}
mask_cells   每个 mask 下的 cell id 列表，用于 active_vertices[mask] / Dijkstra seed
root_masks   每个 v 下 finite 的 mask 列表，用于同根合并
root_cells   每个 v 下对应的 cell id 列表，可与 root_masks 对齐
index        自定义 flat hash，key=(mask,v)，value=cell id
```

`SetDp(mask,v,value)`：

```text
id = index.find(mask,v)
if found:
    cells[id].value = min(cells[id].value, value)
else:
    id = cells.push(...)
    index.insert(mask,v,id)
    mask_cells[mask].push_back(id)
    root_masks[v].push_back(mask)
    root_cells[v].push_back(id)
```

`GetDp(mask,v)`：

```text
id = index.find(mask,v)
return found ? cells[id].value : INF
```

这样保留了 `Test8` 的两个关键局部性：

- 按 `mask` 遍历 active 顶点。
- 按 `v` 遍历 finite mask 做同根合并。

### 自定义 flat hash

因为 `g <= 22`，`mask` 可放进低成本整数 key。若 `n < 2^31`，可用：

```text
key = (uint64(mask) << vertex_bits) | uint64(v)
```

也可以用：

```text
key = uint64(mask) * (n + 1) + v
```

实现用 power-of-two 容量开放寻址：

```text
keys[]    uint64
values[]  int cell_id
used[]    uint8，或以特殊 key 表示空桶
```

哈希函数可以使用 splitmix64 finalizer，避免低位冲突。该结构没有节点分配，内存大致是：

```text
load_factor 0.65 时，index 约 16/0.65 ~= 25 bytes per cell
cell 本体约 16 bytes
bucket id 约 8 bytes
```

如果 cell 数远小于 `n2^g`，仍会显著小于 dense `double` 表。

### Root-local index

同根合并最常见的操作是“已知 v，查某个 t 是否 finite”。因此可把 index 局部化：

```text
root_masks[v]   vector<int>
root_values[v]  vector<double>
root_index[v]   mask -> local offset
```

当 `root_masks[v].size()` 很小时线性扫；超过阈值后为该 root 建局部 flat map。优点是 key 只需要 `mask`，常数比全局 `(mask,v)` 更低。缺点是 Dijkstra 的 `GetDp(mask,to)` 需要从 `to` 的 root-local bucket 查。

### Sorted vector + rebuild

层结束后，如果某些 bucket 进入只读阶段，可以对 `root_masks[v]` 排序并用二分查询。它的内存最小，但动态插入成本高。适合配合“按层重建 root 索引”，不适合作为第一版动态稀疏存储。

## 建议下一步

优先分三步决策。

第一步：继续用 `Test9` 采样。

推荐样本：

```powershell
.\build\Release\gst_test9_main.exe Toronto weight result_test9 debug child_first g10 data 1 3
.\build\Release\gst_test9_main.exe LinkedMDB weight result_test9 debug child_first g10 data 8 1
.\build\Release\gst_test9_main.exe DBLP weight result_test9 debug child_first g4_uniform data_new 1 3
.\build\Release\gst_test9_main.exe MovieLens weight result_test9 debug child_first g4_uniform data_new 1 3
```

若 `g10` 因 dense 表内存过大无法运行，本身就是支持空间优化的证据；先从 `g4/g5/g6` 的 `data_new` 观察 `sparse_dp_h_zero / dense_dp_h` 与 `peak_window_dp_h / dense_dp_h`。

第二步：做稀疏 `dp/h` 原型。

建议先不做窗口删除，只替换存储：

```text
dp[mask] -> vector<pair<v,value>> + per-mask unordered_map/index
h[mask]  -> sparse map keyed by v
```

保留 `active_vertices[mask]` 和 `root_masks[v]`。这样风险最小，能直接验证空间下降和访问开销。

第三步：再研究窗口生命周期。

若 `win_k` 的 `peak_window_dp_h` 明显低于全部 sparse cell，再考虑：

```text
按 size 层释放已不参与未来图扩展的 mask
把同根合并改为事件式：新状态出现时只和仍在窗口内的必要状态合并
为小侧历史状态建立只读压缩索引，避免保留完整 dp/h dense 表
```

这一步需要单独证明正确性，尤其是“小 size 状态何时可释放”。

