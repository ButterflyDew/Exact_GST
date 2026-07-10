# Test18 research log：状态削减与 cover-aware 上界更新

Test18 是当前继续探索大组数的主线版本。它继承 Test17 的 Complete 优化，并进一步减少实际保存和扫描的 `(mask,v)` 状态数。

Test18 仍遵守 Test16 后的核心安全原则：

```text
不恢复非 exact h
不把 dp 上界当作下界
只使用安全下界或合法完整上界影响搜索
```

## 1. 与 Test17 的差异

Test18 当前主线主要保留这些机制：

1. 虚拟 singleton；
2. 全根 `full_lb` corridor；
3. `need[S,v]` stale 状态识别与 compact；
4. 稠密行自适应轻量表示，pair 行是默认 light 的特例；
5. cover-aware Complete，提前更新合法完整上界；
6. 补侧 Complete row 物化，把同一个 `rem` 的普通 Complete 补侧 split 结果复用到多个 popped root；
7. pair 层完成后的同根 pair/single 分区上界，尝试在进入 k=3 前降低 `best`；
8. 正确 DP 顺序下的保存必要条件，包括最大层 future-use、奇数最大层 forced-complete 下界和 future split 保存下界；
9. 查询级标准 Steiner 度缩图，删除非 query terminal 叶子/无 terminal 分量，并压缩非 query terminal 度 2 链。
10. 查询级 Voronoi exact torso：删除无 query terminal 且 portal 数 `<=1` 的单色内域枝叶组件；对 portal 数 `==2` 的无终端组件，用组件内两门户最短路桥边 exact 替换；对 portal 数 `==3` 的无终端组件，用 pair 边 + 三终端 hub gadget 保留二端/三端连接代价；对收益为正且局部 Steiner table 可行的 `portal==4` 组件，用 6 条 pair 边、4 个 triple hub 和 1 个 quad hub exact 替换。

已撤出但保留证据的探针包括：`g=13, k=3` 固定 rows 三分块同根上界，以及用数据集、组数、层数或表示阈值外条件硬触发的非 dense 高阶行轻量表示。它们都能提供瓶颈定位信息，但触发条件不具备通用判据，不作为当前主线机制；当前非 dense 高阶行保持普通 sparse。

当前实现还保留一个无参数初始上界增强：先从 root-star 最优根运行 greedy，再把这次 greedy 命中的组顶点作为候选根各运行一次 greedy。所有结果都只是合法完整上界，取最小值更新 `best`。

此外，Test18 不再预处理半掩码 Far 表；Far 改为当前 mask 内的 lazy cache。

2026-07-09 新增补侧 Complete row 物化：对固定 `rem=U-S`，用已保存 half-DP row 懒构造 `complete_row[v]=min_x dp[x][v]+dp[rem-x][v]`。触发采用直接查询与物化 row 的 rent/buy 成本比较，不含数据集、`g`、层数或时间点特判。随机对拍 `seed=303033` 80 组、Toronto query 1 和 40k g12 三组 A/B 已通过；详细结果集中维护在 `test18_effect_report.md` 7.2。

## 2. 虚拟 singleton

Test16/17 会为每个 singleton `{a}` 保存一条长度为 `n` 的状态行。但：

```text
D*({a},v)=gd[a][v]
```

这个值已由组距离预处理给出，不依赖后续 DP。因此 Test18 不再持久保存 singleton 行。

接口语义改为：

```text
Available({a}) = true
Lookup({a},v) = gd[a][v]
```

同根合并遇到 singleton 时，枚举另一侧稀疏行并直接读取 `gd[a][v]`。这是等价改写，减少了持久状态数和早期 pull 扫描量。

## 2.5 tree-aware 多起点 greedy 上界

初始上界对 pair 层尤其重要。Test18 先计算 root-star 最优根 `r`，并从 `r` 运行一次组件增长 greedy。该 greedy 过程中会依次命中若干实际组顶点；这些点来自查询和图结构本身，不需要设置候选数量参数。

早期实现只把已经命中的组顶点作为下一轮 Dijkstra 的源。这是合法上界，但不是严格意义上的“从当前树扩展”：新增路径上的中间点没有成为后续连接点，可能高估后续增量。当前实现改为 tree-aware greedy：

1. 维护当前已构造树的顶点集合 `T`；
2. 每一轮以 `T` 中所有点作为多源 Dijkstra 起点；
3. 命中任意未覆盖组顶点 `u` 后，沿 Dijkstra parent 回溯，把从 `u` 到 `T` 的整条路径加入 `T`；
4. 路径上顺便经过的组也立即计入 covered。

这仍然只构造合法完整树并降低上界。由于最多扩展 `g` 轮，时间仍是 `O(g(m+n log n))` 级别，低于主 DP 账本。

当前实现把这些命中点去重后作为额外起点，再各运行一次 greedy：

```text
best = min(best, Greedy(r), min_{x in hit_vertices} Greedy(x))
```

正确性直接来自每次 greedy 都构造一棵合法完整树；该步骤只降低上界，不参与下界或剪枝证明。

实测 Toronto g12 query 1 中，tree-aware 后的多起点 greedy 把初始上界从旧版 `0.9875179880` 进一步降到 `0.8983317489`，pair 保存状态从 `705,941` 降到 `497,448`，finite states 从约 `4.206M` 降到约 `3.441M`，peak RSS 从约 `116 MiB` 降到约 `99 MiB`。DBLP g15 query 1 上初始 best 只从 `19.4822` 降到 `19.3812`，前 8 个 pair mask 的 finite 从 `17,804,184` 小幅降到 `17,802,796`。因此它是明确的无参数正优化，但不能单独解决 DBLP g15。

## 3. full_lb corridor

预先计算每个 root 的完整下界：

```text
full_lb[v] = LowerBound(v,U)
```

若：

```text
full_lb[v] > best
```

则任何包含 root `v` 的完整同根解都不可能优于当前 `best`。Test18 在 `TrySet`、`Lookup`、堆弹出和边松弛中跳过这类 root。

这是安全的，因为任意包含 `v` 的完整可行树都可视作以 `v` 为根的完整 rooted 解，其代价至少为 `D*(U,v)`，而：

```text
full_lb[v] <= D*(U,v)
```

## 4. need 与 compact

对每个保存的非 singleton 状态记录：

```text
need[S,v] = dp[S][v] + LowerBound(v,U-S)
```

如果之后 `best` 下降到：

```text
need[S,v] > best
```

那么该状态作为当前侧与任意补侧可行树拼接，都不可能得到更优完整解。由于 `best` 单调不增，它未来也不会重新变得有用。

Test18 对 stale 状态做两层处理：

- join 和 lookup 时即时跳过；
- 当 `best` 下降后，在层切换处线性 compact，把失效项从稀疏行中删除。

compact 的总扫描量受已保存状态数控制，不改变理论复杂度。

## 4.5 稠密行的自适应轻量表示

DBLP g15 的 pair 层几乎是全图稠密的：前 8 个 pair mask 已保存 `17,802,796` 个状态，平均每个 pair 约 `2.23M`，而图有 `2.50M` 个点。此时继续用通用稀疏行：

```text
v + d + cover + need
```

空间常数很差。Test18 先对 `|S|=2` 的行做专门表示：

```text
light = true
只保存 dp[S][v] 的精确距离
不持久保存 cover / need
```

如果当前行满足：

```text
saved_count * (sizeof(int)+sizeof(double)) > n * sizeof(double)
```

则转为 dense-light 行；否则保存为 `v,d` 两个稀疏数组。这个判定只是表示成本比较，不是调参。

进一步地，Test18 把这个策略推广到任意 `|S|>2` 的稠密行，但更保守：高阶行只有在上述 dense 成本判定成立时才转为 `light+dense`；否则一律保留原来的 `v,d,cover,need`。这样做的含义是：

- 中等稀疏行继续保留 `cover/need`，不损失 stale 跳过和 cover-aware 上界更新；
- 足够稠密的高阶行保存精确 `dp[S][v]`，避免在 DBLP 这类大图上为每个状态持久保存 `cover/need`；
- 触发条件等价于 `3*saved_count > 2*n`，来自 `v+d` 稀疏距离表与 dense 距离表的空间比较。

dense-light 行当前使用 full dense `double[n]`。曾进一步测试 packed dense：

```text
packed dense = bitset[n] + prefix-per-word + dense_values[saved_count]
```

该表示仍按点保存完全相同的 double 距离，只是用 bitset 表示哪些点有限，并用 prefix+popcount 支持随机 `Lookup(S,v)`。它是正确的纯布局替换，但 DBLP snapshot g9 中只让 `pull_scan` 小幅下降，wall time 与 peak RSS 没有收益，因此不保留。

正确性理由：

- light 行保存的 `dp[S][v]` 仍是精确值，后续同根合并读取的数值不变；
- 不保存 `need` 只会少跳过 stale 状态，可能增加工作量，但不会产生错误答案；
- 不保存 actual-cover 时，后续 cover 只使用保守的 `S | color[v]`。这可能错过一些 early upper update，但不会构造非法上界，也不会影响 DP 精确性。

复杂度上，dense 行的 join 是按点扫描，仍计入 `O(3^g n)`；空间上，DBLP g15 的 105 个 pair 行若全 full dense，约为 `105*n*sizeof(double)≈2.1GB`，明显小于保存 `v,d,cover,need` 的通用稀疏表示。packed dense 进一步处理“超过 dense 阈值但远未接近全图”的行：例如有限点约为 `0.7n` 时，`bitset+values` 比 `double[n]` 少约 25% 空间，并且 dense join 也少扫无穷点；但实测不是正优化，因此不进入主线。

### 4.5.1 best 下降后的 light/dense-light compact

best 下降后，light / dense-light 行也可以用同一个必要条件做 compact：

```text
dp[S][v] + LowerBound(v,U-S) <= best
```

不满足该条件的 `(S,v)` 不可能再作为当前侧拼出更优完整解。普通行复用持久 `need`；light 行不存 `need`，只在 compact 点现场重算；dense-light 行顺着 dense 数组扫描，把失效 root 置为 `INF`。单独测试这条 compact 时没有引入新下界，只补回了轻量表示原本漏掉的 stale 删除；加入 4.5.2 后，当前主线会在 light/dense-light compact 点复用同一套保存下界。

fast snapshot A/B（`data_snapshot/generated_fast`，5 个 dataset version，`g=9..12` 各 1 条；当前版 `result_snapshot/fast/20260708_121239` vs 临时关闭 light/dense-light compact `result_snapshot/fast/20260708_121613`；20 条权重全部一致）：

| dataset version | baseline live | current live | live 下降 | light removed | dense removed | pull_hits 下降 | tryset_calls 下降 | wall 变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Toronto_data` | `901,189` | `883,504` | `1.96%` | `17,685` | `0` | `2.05%` | `1.97%` | `-0.83%` |
| `Toronto_data_new` | `4,038,026` | `3,940,978` | `2.40%` | `97,048` | `67,709` | `5.44%` | `5.46%` | `+0.47%` |
| `DBLP_data_bfs` | `2,575,905` | `2,573,944` | `0.08%` | `1,961` | `1,875` | `0.11%` | `0.11%` | `-2.92%` |
| `DBLP_data_new_bfs` | `3,277,644` | `2,700,555` | `17.61%` | `577,089` | `575,917` | `25.16%` | `25.19%` | `-1.44%` |
| `MovieLens_data_bfs` | `2,220,040` | `2,219,998` | `0.00%` | `42` | `0` | `0.00%` | `0.00%` | `+1.02%` |
| **total** | `13,012,804` | `12,318,979` | `5.33%` | `693,825` | `645,501` | `10.28%` | `10.31%` | `+0.26%` |

结论：fast 小图上它不是普遍 wall-time 加速项，总时间基本持平；但它稳定降低后续 join/lookup 实际触达，尤其在 `DBLP_data_new_bfs` 上把 live `(mask,v)` 减少 `17.61%`，`pull_hits/tryset_calls` 减少约 `25%`。40k DBLP snapshot g9 query 1-5 的此前 compact 记录则更强：`finite_states=10.718141M`，`compact_removed=4.426633M`，`live_states=6.291508M`，按状态数加权 compact 比例 `41.30%`。

### 4.5.2 正确 DP 顺序保存必要条件（达到 fast 平均 20%）

随后沿“正确 DP 顺序下的未来可用性”继续测试三类保存必要条件：

- `|S|=H` 的最大层行不能再参与 join；若当前 mask 顺序下没有任何后续同层 Complete 会读取它，则当前行自己的 Complete 结束后整行不保存。
- 奇数 `g` 的最大层行若仍有未来读者，则未来补侧只能是一个 H-mask 加一个 singleton，因此保存 root `v` 前使用 `dp[S][v] + min_b(LowerBound(v,R-{b}) + gd[b][v]) <= best`。
- `|S|<H` 的行在自己的 Complete 后若未来还要发挥作用，则算法至少还要引入两个非空补侧块；因此保存 root `v` 前使用 `dp[S][v] + max(LowerBound(v,R), far_R(v)+near_R(v)) <= best`。这个 future split 下界只在保存和 compact 点使用，不进入 Dijkstra relax。

fast snapshot A/B（当前组合版 `result_snapshot/fast/20260708_133624` vs 临时关闭 light/dense-light compact 且无 order prune 的基线 `result_snapshot/fast/20260708_121613`；20 条权重全部一致）：

| dataset version | baseline live | current live | live 下降 | finite 下降 | split states | order states | forced-LB states | pull_hits 下降 | tryset_calls 下降 | wall 变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Toronto_data` | `901,189` | `492,302` | `45.37%` | `41.06%` | `358,814` | `58,868` | `10,936` | `38.37%` | `38.70%` | `+10.63%` |
| `Toronto_data_new` | `4,038,026` | `2,632,514` | `34.81%` | `30.01%` | `783,357` | `430,418` | `109,926` | `27.41%` | `27.57%` | `-4.20%` |
| `DBLP_data_bfs` | `2,575,905` | `2,329,185` | `9.58%` | `9.49%` | `12` | `214,752` | `29,995` | `0.11%` | `0.11%` | `-2.10%` |
| `DBLP_data_new_bfs` | `3,277,644` | `2,312,666` | `29.44%` | `11.47%` | `224,139` | `145,442` | `42,603` | `27.87%` | `27.88%` | `-1.83%` |
| `MovieLens_data_bfs` | `2,220,040` | `1,516,825` | `31.68%` | `31.67%` | `301,618` | `358,821` | `42,768` | `20.02%` | `20.02%` | `+3.59%` |
| **total** | `13,012,804` | `9,283,492` | `28.66%` | `22.48%` | `1,667,940` | `1,208,301` | `236,228` | `21.93%` | `21.94%` | `+1.64%` |

结论：加入 future split 后，fast 总 live 降幅从上一版 `16.43%` 提升到 `28.66%`，达到平均 `20%` 目标；相对上一版 `result_snapshot/fast/20260708_124821` 的 live `10.874450M`，当前又额外下降 `14.63%`。该收益不是所有数据集均匀分布：`Toronto_data`、`Toronto_data_new`、`DBLP_data_new_bfs`、`MovieLens_data_bfs` 都超过 `20%`，但 `DBLP_data_bfs` 仍只有 `9.58%`，future split 只额外删了 12 个状态。还测试过“按 last-use 主动 clear 已保存行”的 resident-state 版本：最终 live 会变成 0，但这是算法结束释放造成的统计假象；peak resident 对只做最大层保存条件的组合版没有改善，wall time 还从 `96.812s` 变慢到约 `98.520s`，因此不保留。

## 5. cover-aware Complete

普通 Complete 使用名义 mask：

```text
rem = U ^ S
```

但当前弹出的树可能实际覆盖更多组，记录在：

```text
cover[v]
```

于是可以用更小的剩余集合：

```text
cover_rem = U ^ (cover[v] & U)
```

若 `cover_rem` 能由两个当前可查的小块 `X,Y` 补完：

```text
X union Y = cover_rem
X intersect Y = empty
|X|, |Y| <= H
```

则：

```text
dist[S][v] + dp[X][v] + dp[Y][v]
```

是一棵合法完整同根树的代价，可以立即更新 `best`。

这个机制只降低合法上界，不直接删除状态。它的收益来自更早得到较小 `best`，继而让原有安全门控自然变强。

## 5.5 pair/single 同根分区上界

DBLP g15 的核心困难之一是：pair 层本身不会更新 `best`，于是进入 k=3 时仍使用较松的初始上界。Test18 在所有 pair 行处理完成、进入 k=3 之前，额外做一次合法上界构造。

对候选根 `v`，使用已经保存的精确 pair rooted DP：

```text
dp[{a,b}][v]
```

以及虚拟 singleton：

```text
gd[a][v]
```

在组集合上做一个小 DP：

```text
F[0] = 0
F[M] = min(
    gd[a][v] + F[M-{a}],
    dp[{a,b}][v] + F[M-{a,b}]
)
```

其中 `a` 取 `M` 的最低位，`b` 枚举 `M-{a}` 中的组。`F[U]` 表示把所有组分成若干 singleton/pair 组件后，同根粘在 `v` 上的总代价。这些组件都是合法树，取它们的并仍是一棵覆盖所有组的合法树；总代价用求和可能重复计算共享边，因此是安全上界。

候选根集合不使用调参：

- root-star 最优根；
- tree-aware greedy 命中的组顶点；
- 最小组中的所有终端。

复杂度为：

```text
O(r g 2^g)
```

其中 `r` 是上述候选根数。它不乘 `n`，只在 k=2 全部完成后运行一次；在 DBLP g15 中相对 105 个 dense pair Dijkstra 的代价很小。

自然的推广是：在所有 k=3 行完成后，再允许同根分区 DP 使用 singleton/pair/triple 三类块。早期 Toronto g10/g12 和 DBLP snapshot g9 探针中，它没有更新 best 或额外耗时大于收益，因此当时没有作为普遍默认策略保留。后续 full DBLP g13 探针显示，若在 k=3 内按 rows `96/160/224/256` 增量触发，三分块上界能连续降低 `best` 并减少 k=3 后段 dense 行；但固定 rows 触发没有理论依据，违背 `agent.md` 第六条，已从当前代码撤出。完整实测表统一维护在 `test18_effect_report.md`。

## 6. Far/LowerBound 的当前 mask 在线计算

早期 Test18 为 `Far(v,R)` 预处理两张半掩码表，查询为 O(1)，但空间是：

```text
O(n(2^floor(g/2)+2^ceil(g/2)))
```

这对大图和 `g=15` 以上的实验不够划算。当前实现改为每个 mask 在线展开补集：

```text
R = U - S
rem_bits = groups in R
```

并使用当前 mask 内的 stamp lazy cache：

```text
far_cache[v] = max_{a in R} gd[a][v]
lb_cache[v]  = LowerBound(v,R)
```

每个 mask 开始时只把 `R=U-S` 的组位展开成一个很短的 `rem_bits` 数组，并把 `MST(R)/2` 提前算成当前 mask 的常数。之后同一个 `(mask,v)` 的 `Far` 或 `LowerBound` 查询最多各扫描一次 `rem_bits`。因此最坏时间为 `O(n g 2^g)`，但实际只为被同根合并、入堆或边松弛触达的点付费；空间从半掩码大表降为 `O(n)`。

这里没有把 `Far` 与 `LowerBound` 强行合并成同一个 cache miss。实测中大量候选会被 `Far` 直接剪掉，而完整 `LowerBound` 还需要维护最近两组距离；合并后虽然减少了部分重复扫描，但会给 Far-pruned 状态支付额外常数。Toronto g10 query 1 的 A/B 中，合并版约 `0.228--0.235s`，分离 lazy 版约 `0.212--0.229s`，因此当前保留分离 cache。

进一步测试过完全删除 `far_cache/far_seen`，令每次 `Far(v,R)` 都直接扫描当前 mask 的 `rem_bits`。这个版本虽然少了一张 `double[n] + int[n]` 工作区，但会让 `TrySet`、seed、pop 和 relax 中重复触达的同一 `(mask,v)` 反复扫描补集。Toronto g10 query 1 中缓存版约 `0.224s`，direct 版约 `0.229s`；DBLP snapshot large / `DBLP_data_bfs` g9 query 1 中缓存版约 `4.11s`，direct 版约 `4.53s`，状态数完全一致。考虑到 full DBLP g15 的主要空间来自 dense pair/k3 行，Far 工作区的 `O(n)` 空间不是瓶颈，因此 direct-Far 不是正优化，不保留。

曾经用于观察的 `diam(rem)` 统计已经移除。实验中它几乎没有产生额外剪枝，却需要额外预处理和热路径判断，不符合收益大于代价的原则。

同理，早期用于 h 研究的 exact/certify 诊断也已经从 Test18 热路径移除。当前 Test18 不使用 h，也不依赖 exact witness；继续在每个弹出状态上额外计算 `LowerBound(v,S)` 只会增加运行时间。后续瘦身阶段已把 Test18 中长期为 0 的 h/exact/certify 兼容统计字段删除，只在 Test16/17 中保留历史输出。

另一个已测试但未保留的上界增强是：取 greedy 命中的代表点，对这些点做 metric MST/KMB 上界。该上界本身正确，但实测不是有效收益：Toronto g12 query 1 中 `greedy_upper=1.0171679214`，KMB 上界约 `1.0235355597`，没有改善；DBLP g15 query 1 进入 DP 时 `best` 仍为 `19.4822`，pair 层开放程度不变，额外预处理反而降低限时内处理进度。因此该方案不进入 Test18 主线。

还测试过 pair 层同根匹配上界：对 greedy 自然产生的少量候选根，利用已处理 pair rooted DP 与 singleton rooted DP，贪心选择不相交 pair 来替代两个 singleton，从而得到一个合法同根完整上界。该方案正确且无参数，但实测无效：Toronto g12 query 1 中得到的最好候选约 `1.1109907532`，弱于 tree-aware greedy 的 `0.8983317489`；DBLP g15 query 1 前 8 个 pair mask 中最好候选约 `20.3498`，弱于当前 `19.3812`，没有更新 best。因此删除，不进入主线。

还测试过把第一次 tree-aware greedy 生成树中的分叉点作为额外 greedy 起点。该想法同样无参数，候选数也受 `g` 控制；但 Toronto g12 query 1 与 DBLP g15 query 1 都没有进一步降低 `multi_greedy_upper`，状态数完全不变，只增加上界阶段工作量。因此不保留。

还测试过 root-star 最优根的 shortest-path union 上界：从该根跑一次单源最短路，取到每个组最近点的最短路径并按无向边去重求和。该上界合法，但 Toronto g12 query 1 中 `path_union_upper≈1.2510`，仍弱于 greedy/multi-greedy；DBLP g15 query 1 进入 DP 时 `best` 仍为 `19.4822`，因此不保留。

还测试过从 root-star 最优根跑一次单源最短路，并把每个组离该根最近的真实终端点作为额外 greedy 起点。该方案同样无参数且只产生合法上界，但 Toronto g12 query 1 和 DBLP g15 query 1 都没有改善 `multi_greedy_upper`；DBLP pair 层仍以 `best=19.4822` 进入，限时进度略慢，因此不保留。

还测试过 warm-start 调度：先按 root-star 距离把组分成三块，处理这三块闭包，使若干大 mask 能更早尝试拼出完整上界，再回到普通 size-order。这只是改变拓扑顺序，不改变正确性。但 Toronto g12 query 1 中它没有提前得到更好 best，反而因为过早处理高阶 mask 让 finite states 从约 `4.21M` 增至约 `4.96M`；DBLP g15 query 1 也未降低 `best=19.4822`，pair 层进度变慢。因此不保留。

还测试过 group-MST-edge seeded greedy：取组间 metric MST 的每条边作为一条真实组间路径种子，再从路径端点做 greedy 扩展。该方案同样只产生合法上界，并且无参数；但 Toronto g12 query 1 中没有优于 multi-greedy，DBLP g15 query 1 仍以 `best=19.4822` 进入 pair 层，额外 greedy 运行只带来时间开销。因此不保留。

pair 层还测试过局部支配初始源过滤：若 `gd[a][u]+gd[b][u]+w(u,v)<gd[a][v]+gd[b][v]`，则不把 `v` 作为 pair Dijkstra 的初始源。该条件保持 pair DP 值不变，并且在 Toronto g12 query 1 上把 pair 层 active seed 从约 `547K` 降到 `186K`，时间从约 `17.04s` 降到 `15.89s`。但 DBLP g15 上它只减少 seed，不减少最终 `finite/inqueue`；每个 pair 仍保存约 `4.45M` 状态，额外邻边扫描让 18 个 pair 的限时进度从约 `56s` 变慢到约 `66s`。由于当前目标是跑动 DBLP g15，该过滤不保留。

pair 层实际覆盖 `cover` 也做过诊断。Toronto g12 query 1 中，pair 保存状态里 `cover` 大于 2 的约 `182,735 / 705,941 ≈ 25.9%`，看起来有升格或压缩潜力。但 DBLP g15 query 1 前 8 个 pair 中，`cover` 大于 2 的只有 `61,683 / 17,804,184 ≈ 0.35%`，额外覆盖组数也几乎是一比一。因此基于 pair actual-cover 的升格/压缩不能解决 DBLP g15 的 pair finite 爆炸；当前只保留统计字段，不作为算法条件。

还测试过只保存 pair Dijkstra 中“未被图扩展改善”的源状态，试图把由其它 root 扩展得到的 pair 状态视为 dominated。这个想法不正确：随机对拍 `seed=777001 iteration=38` 找到反例，DPBF 最优为 `25`，剪后结果为 `26`。这说明 pair 的图扩展状态虽然看似可由更靠近 pair 核心的 root 支配，但在半 DP 的后续同根构造中仍可能是必要的。该剪枝已删除。

还测试过只在预处理阶段用一次 `root+groups` metric MST 增强 `full_lb[v]`。该下界安全，并且不进入热路径；但实测收益几乎为零。Toronto g12 query 1 中 `full_aug_pruned=0`；DBLP g15 query 1 中只额外剪掉 227 个完整 root，前 8 个 pair mask 的 finite 仍为 `17,802,796`，同时进入 k=2 前多约 1 秒预处理。因此不保留。

还测试过更强的 `root+groups` metric MST 下界：在超节点 `{v}∪R` 上计算 MST，root 到组边为 `gd[a][v]`，组间边为 `gp[a][b]`。该下界安全，也确实能在 DBLP g15 pair 层多剪少量状态；例如直接 `O(g^2)` Prim 版把前 6 个 pair 的 finite 从约 `13.35M` 降到约 `12.30M`，但时间从约 `18.9s` 增至约 `26.7s`。随后又测试了“先对当前 `R` 建一次组间 MST 树，再对 `{root}+R` 做小 Kruskal”的较低常数版本：前 8 个 pair 的 finite 从约 `17.80M` 降到约 `16.43M`，但时间从约 `25.3s` 增至约 `27.8s`。两者都是剪得动但收益小于代价，因此不保留。

还测试过一个更便宜的三点 MST 下界：对当前 root `v` 与补集 `R` 中距离最远的两个组 `a,b`，加入

```text
MST({v,a,b}) = min(gd[a][v]+gd[b][v],
                   gd[a][v]+gp[a][b],
                   gd[b][v]+gp[a][b])
```

这是安全下界，且能在扫描 `rem_bits` 时顺手维护。但实测仍不是正优化：随机对拍 `seed=112233` 的 80 组通过；Toronto g10 query 1 中状态略减但时间从约 `0.225s` 增到约 `0.244s`；DBLP snapshot large / DBLP_data_bfs g9 query 1 中 finite 几乎不变，时间从约 `5.46s` 增到约 `5.75s`。因此删除，不进入主线。

还测试过 parent-based terminal greedy：组 Dijkstra 时顺手保存 `gd_parent`，再从最小组的每个终端作为起点，用 `gd + parent` 廉价构造合法 greedy 上界。该方案无参数且正确，但未改善关键上界：随机对拍 `seed=121212` 的 80 组通过；Toronto g12 query 1 中 `terminal_greedy_upper=multi_greedy_upper=0.8983317489`，时间略慢；DBLP snapshot large / DBLP_data_bfs g9 query 1 也无改善；full DBLP g15 query 1 进入 k=2 时 best 仍为 `19.3812`。同时 `gd_parent` 需要额外 `O(gn)` 个 int，full DBLP g15 前 2 个 pair 的 RSS 约从旧探针 `1.39GB` 增至 `1.53GB`。因此删除，不保留。

## 7. 正确性

Test18 继承 Test16/17 的三块分解和安全门控证明。

虚拟 singleton 是等价改写，因为 singleton rooted DP 恒等于预处理的 `gd`。

`full_lb` 使用完整解安全下界，只排除不可能作为更优完整解 root 的点。

`need` 使用当前侧可行上界加补侧安全下界。若 `need[S,v]>best`，则该状态无法与任何补侧组成更优解；`best` 单调不增，所以删除后不会在未来重新需要。

cover-aware Complete 只构造合法完整解并更新上界。它不把候选值当作下界，也不新增剪枝规则，因此不会破坏精确性。

pair/single 同根分区上界只在已保存的精确 pair rooted DP 与 singleton `gd` 上构造合法同根树。求和可能高估真实并集代价，不会低估，因此只能安全降低 `best`。

Far/LB 的 lazy cache 只是把同一个安全下界的计算方式从预处理表或全量清空改成当前 mask 在线计算，不改变任何判定语义。

## 8. 复杂度

Test18 仍满足：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

原因：

- 虚拟 singleton 减少存储和扫描；
- `full_lb` 是 `O(gn)` 级别的下界预计算；
- `need` 检查和 compact 总量受已保存稀疏状态数控制；
- 稠密行自适应表示只改变存储和交集枚举方式，dense join 仍是按点扫描，计入 `O(3^g n)`；
- Far/LB 的当前 mask 在线计算最坏为 `O(n g 2^g)`，被 `O(2^g g n)` 口径覆盖；stamp cache 避免每个 mask 全量清空，不新增渐进代价；
- cover-aware Complete 枚举的是补集二分，仍由三进制归属计数控制。
- pair/single 同根分区上界只对 `r` 个候选根运行一次，复杂度 `O(r g 2^g)`，不乘 `n`，在理论上被主项覆盖。

空间上，Test18 删除 singleton 持久行；非 light 的 `|S|>2` 行额外保存一个 `need` double；`|S|=2` 的 pair 行默认轻量保存精确距离，任意足够稠密的行会转成 dense-light 距离表，避免在 DBLP 这类近全图行上为每个状态支付 `cover/need` 常数。

## 9. 统计字段

Test18 在 Test17 基础上增加：

```text
global_root_alive / global_root_pruned
tryset_calls / tryset_keep
tryset_pruned_full / tryset_pruned_ge_best / tryset_pruned_far / tryset_pruned_lb
stale_need_skips / lookup_need_skips
compact_calls / compact_removed / compact_ms
compact_light_removed / compact_dense_removed
order_pruned_rows / order_pruned_states / order_lb_pruned_states / order_split_pruned_states
live_states
best_updates / first_best_update_size / last_best_update_size
root_star_upper / greedy_upper / multi_greedy_upper / multi_greedy_roots
pair_partition_roots / pair_partition_updates / pair_partition_upper / pair_partition_ms
best_after_k*
early_cover_extra / early_cover_pair_ready / early_cover_pair_better
early_cover_best_updates / early_cover_best_candidate
pair_saved_cover_extra / pair_saved_cover_extra_groups / pair_saved_cover*
pair_dense_rows / pair_dense_states
dense_rows / dense_states / dense_rows_k* / dense_states_k*
pair_saved_slack_count / pair_saved_slack_rel_avg / pair_saved_slack_rel_max / pair_saved_slack_rel*
```

建议重点看：

1. `tryset_keep / tryset_calls`：前置门控是否真正减少写入。
2. `stale_need_skips + compact_removed`：`best` 下降后旧状态是否大量失效。
3. `root_star_upper / greedy_upper / multi_greedy_upper`：初始上界是否已经足够强。
4. `pair_partition_*`：pair 层完成后是否在进入 k=3 前降低 `best`。
5. `early_cover_best_updates` 和 `best_after_k*`：cover-aware Complete 是否把好上界前移。
6. `pair_saved_cover*`：pair 层保存状态是否实际覆盖更多组，是否值得研究升格。
7. `pair_dense_rows / pair_dense_states`：pair 行是否触发 dense 表示，以及 dense 表示覆盖多少状态。
8. `dense_rows* / dense_states*`：所有 dense-light 行的规模，尤其看是否在 k=3 以后触发。
9. `pair_saved_slack_rel*`：pair 层保存状态距离当前 `best` 门槛还有多远。桶编号含义为：
   - `0`: `(best-need)/best <= 1%`
   - `1`: `<= 5%`
   - `2`: `<= 10%`
   - `3`: `<= 25%`
   - `4`: `<= 50%`
   - `5`: `> 50%`
10. `finite_states`、`active_seed`、`pull_scan`：状态总量是否真的下降。

2026-07-08 还补齐了 normal Complete 对 actual cover 的使用：当 `k*3>=g` 后，Complete 不再总是按名义 `rem=U-S` 补全集，而是先用当前树实际覆盖得到 `cover_rem=U-cover`。这只构造合法完整上界，不剪状态。Toronto query 1 权重一致且状态不变；DBLP snapshot g9 query 1 中 normal Complete 额外出现约 `1394` 次补集缩小，但 `finite_states=1.784423M`、`live_states=0.350298M` 不变，因此它是语义补齐和后续 g13 观察点，不是已证实的大突破。

## 10. 已验证结果

基础 Test18 增量验证：

```text
随机小图黑盒对拍：
  seed=271828   100 组通过
  seed=424242    30 组通过

Toronto query_g10 前 5 条：
  Test17：6.327104s
  Test18：4.909424s
  speedup：22.41%
  bad=0
  max_diff=0

状态统计：
  finite_states：5.27M -> 2.97M
  active_seed：11.73M -> 2.07M
  pull_scan：339.43M -> 83.22M
  pq_push/pop：约 4.73M，基本不变
```

cover-aware Complete 接入后的验证：

```text
随机小图对拍：
  seed=314159  100 组通过
  seed=271828   60 组通过（g 最高到 10）

Toronto g12 query 1：
  weight 不变，为 0.8599958231
  finite_states：5.48M -> 4.47M
  active_seed：4.09M -> 3.29M
  peak RSS：约 150 MiB -> 128 MiB
  best_after_k3：1.0171679214 -> 0.8912215862

Toronto g15 query 1（限时到 k=5/k=6 入口）：
  旧版进入 k=5：best=1.32334，finite≈24.09M，active≈19.49M
  新版进入 k=5：best=1.13477，finite≈18.25M，active≈14.28M
  新版进入 k=6：best=1.10241，finite≈27.88M，active≈21.52M
```

Far/LB 在线计算的实现微调：

```text
随机小图对拍：
  seed=112358  50 组通过

Toronto g10 query 1：
  weight 不变，为 0.4475349050
  wall_ms：约 220.3 -> 211.7

DBLP g15 query 1（90s 限时）：
  前 8 个 pair mask 后 finite=17,804,184，与旧观察一致
  说明该改动只降低计算/清空开销，不改变状态集合
```

Far/LB cache 合并尝试（未保留）：

```text
思路：
  同一个 (mask,v) 第一次查询时同时算出 Far 与 LowerBound。

结论：
  不划算。Toronto g10 query 1 中，合并版约 0.228--0.235s，
  当前分离 lazy 版约 0.212--0.229s。

原因：
  Far-pruned 状态很多，而完整 LowerBound 的最近两组维护没有产生足够额外剪枝。
  因此当前 Test18 保留分离 Far/LB lazy cache。
```

tree-aware greedy 上界：

```text
随机小图对拍：
  seed=13579  60 组通过

Toronto g12 query 1：
  weight 不变，为 0.8599958231
  greedy_upper=0.9117467825
  multi_greedy_upper=0.8983317489
  pair 保存状态：705,941 -> 497,448
  finite_states：4.206M -> 3.441M
  peak RSS：约 116 MiB -> 99 MiB

DBLP g15 query 1（90s 限时）：
  进入 k=2 的 best：19.4822 -> 19.3812
  前 8 个 pair mask 后 finite：17,804,184 -> 17,802,796
```

稠密行自适应轻量表示：

```text
随机小图对拍：
  seed=424200  80 组通过
  seed=97531   80 组通过
  seed=97532   80 组通过

Toronto g12 query 1：
  weight 不变，为 0.8599958231
  pair_dense_rows=0
  dense_rows=0
  peak RSS：约 98.7 MiB -> 94.1 MiB
  finite_states：3.441M -> 3.494M
  说明 Toronto 的 pair 行仍适合 sparse-light；丢失 pair actual-cover 会让少量 early update 延后。

DBLP g15 query 1（90s 限时，前 8 个 pair mask）：
  pair_dense_rows=8
  dense_rows=8
  finite_states=17,802,796，与 tree-aware 版相同
  elapsed 到第 8 个 pair mask：约 26.08s -> 25.97s
  说明 DBLP 的 pair 行确实进入 dense 表示；该改动主要降低空间常数，不直接减少状态数。

DBLP snapshot large / DBLP_data_bfs g9 query 1（40k 点）：
  通用 dense-by-cost 版本：
    weight=11.8766830000
    dense_rows=89，其中 dense_rows_k2=36, dense_rows_k3=53
    dense_states=3.367888M
    wall_ms≈5456, peak RSS≈85.3 MiB
  临时关闭 k>2 dense、只保留 pair dense：
    weight=11.8766830000
    dense_rows=36
    wall_ms≈5529, peak RSS≈114.6 MiB
  说明高阶 dense-light 对稠密 k=3 行是明确空间正优化，时间没有可见回退。

DBLP full g15 query 1（短探针，前 12 个 pair mask）：
  进入 k=2 时 best=19.3812
  第 12 个 pair 后 finite=26.705702M
  pair_dense_rows=12, dense_rows=12
  与旧 pair-dense 观察一致，说明高阶 dense-light 不影响 k=2 前段行为。
```

packed dense 布局尝试（未保留）：

```text
思路：
  对 dense-light 行使用 bitset + prefix + dense_values，只枚举有限点。

验证：
  随机小图对拍 seed=101010 80 组通过
  Toronto 默认 query 1：weight=0.2582152999，与 DPBF 一致
  DBLP snapshot large / DBLP_data_bfs g9 query 1：
    packed_dense_rows=13
    packed_dense_states=501,376
    pull_scan：约 27.35M -> 26.88M
    wall_ms：约 4.24s -> 4.49s
    peak RSS：约 74.2 MiB -> 75.1 MiB

结论：
  布局正确，但收益小于 popcount/rank 与额外结构开销，因此撤回。
```

pair/single 同根分区上界：

```text
随机小图对拍：
  seed=565656  80 组通过

Toronto g12 query 1：
  pair_partition_roots=134
  pair_partition_updates=0
  pair_partition_upper=0.8983317489
  pair_partition_ms≈120
  说明 Toronto 上已有 multi-greedy 足够强，该步骤没有副作用但也没有进一步收益。

DBLP snapshot large / DBLP_data_bfs g9 query 1（40k 点）：
  pair_partition_roots=10
  pair_partition_updates=1
  pair_partition_upper=12.5874170000
  finite_states：3.515430M -> 1.807961M
  wall_ms：约 5456 -> 4188
  peak RSS：约 85.3 MiB -> 73.9 MiB

DBLP full g15 query 1（k=3 初段探针）：
  k=2 pair 层完成约 340.7s
  进入 k=3 前 best：19.3812 -> 17.7702
  k=3 初段：
    elapsed≈347.1s，finite≈238.11M，dense_rows=107
    elapsed≈366.7s，finite≈251.33M，dense_rows=113
    elapsed≈386.0s，finite≈264.56M，dense_rows=119
  对比高阶 dense-light 但无 pair_partition 的探针，早期 k=3 finite 只小幅下降，但 `lb_prune` 明显增加；这是目前 full DBLP g15 上第一条能显著降低 k=3 前 best 的机制。
```

pair saved slack 诊断：

```text
随机小图对拍：
  seed=246813  60 组通过
  seed=97531   30 组通过

Toronto g12 query 1：
  pair_saved_slack_count=705,941
  avg_rel_slack≈17.66%
  <= 1% : 23,419   (3.32%)
  <= 5% : 115,817  (16.41%)
  <=10% : 230,217  (32.61%)
  <=25% : 518,368  (73.43%)
  <=50% : 698,957  (99.01%)

DBLP g15 query 1（90s 限时，前 8 个 pair mask）：
  pair_saved_slack_count=17,804,184
  avg_rel_slack≈35.74%
  <= 1% : 2,845      (0.016%)
  <= 5% : 19,850     (0.11%)
  <=10% : 70,334     (0.39%)
  <=25% : 1,236,231  (6.94%)
  <=50% : 17,658,127 (99.18%)
```

解释：若只把 `LowerBound` 增强一个相对 `best` 的小量，最多只能剪掉 slack 不超过这个量的保存状态。DBLP g15 上，哪怕有一个几乎免费的新下界能稳定提升当前 `best` 的 10%，也只能覆盖前 8 个 pair 状态的约 0.4%；提升 25% 也只覆盖约 6.9%。因此 DBLP 的 pair 爆炸不是“当前 LB 稍弱一点”的问题，而是 pair 层进入时 `best` 过松，或者缺少更本质的结构性必要条件。

Test18 瘦身后验证：

```text
随机小图对拍：
  seed=202020  80 组通过

Toronto 默认 query 1：
  Test18=0.2582152999
  DPBF  =0.2582152999

DBLP snapshot large / DBLP_data_bfs g9 query 1：
  weight=11.8766830000
  finite_states=1.807961M
  dense_rows=36
  wall_ms≈4365
  peak_rss≈74.9 MiB

瘦身内容：
  删除 Test18 中长期为 0 的 h/exact/certify/valid 兼容字段；
  撤回 packed dense 代码，只保留失败实验记录；
  Test18 输出改为 total_k / active_k / inq_k / merge_k 桶。
```

DBLP g15 仍未跑通：

```text
DBLP g15 query 1（pair dense 后，120s 限时诊断）：
  前 12 个 pair mask 用时约 38.4s
  finite≈26.71M
  pair_dense_rows=12
  前 12 个 pair 基本都是约 3.1-3.3s/个，没有发现单个早期 pair 异常慢

DBLP g15 query 1（pair dense 后，长探针）：
  k=2 pair 层完成约 352.8s
  完成 105 个 pair 后：
    finite≈233.71M
    pair_dense_rows=105
    best=19.3812，pair 层没有 early update
  进入 k=3 后仍快速膨胀：
    elapsed≈359.6s，finite≈238.15M
    elapsed≈407.4s，finite≈269.26M
    elapsed≈461.9s，finite≈304.82M
  说明 pair dense 解决的是 pair 行空间常数，不解决高阶状态规模；k=3 的 singleton+dense-pair 扫描仍会把状态继续推高。

DBLP g15 query 1（通用 dense-by-cost 后，k=3 初段探针）：
  k=2 pair 层完成约 356.5s
  完成 105 个 pair 后：
    finite≈233.71M
    pair_dense_rows=105
    dense_rows=105
    dense_states≈233.71M
    best=19.3812，pair 层没有 early update
    rss≈3333 MiB
  进入 k=3 后，高阶行立即触发 dense-light：
    elapsed≈363.3s，finite≈238.15M，dense_rows=107，rss≈3389 MiB
    elapsed≈383.7s，finite≈251.48M，dense_rows=113，rss≈3507 MiB
    elapsed≈403.8s，finite≈264.81M，dense_rows=119，rss≈3618 MiB
  这说明高阶 dense-light 把每个稠密行的空间压到一张 `double[n]`，但没有改变 k=3 的状态数量斜率；每 2 个 k=3 mask 仍大约增加 4.44M finite。

DBLP g15 query 1（加入 pair/single 同根分区上界后，k=3 初段探针）：
  k=2 pair 层完成约 340.7s
  进入 k=3 前 best=17.7702
  进入 k=3 后：
    elapsed≈347.1s，finite≈238.11M，dense_rows=107
    elapsed≈366.7s，finite≈251.33M，dense_rows=113
    elapsed≈386.0s，finite≈264.56M，dense_rows=119
  该上界显著降低了 best，但早期 k=3 finite 只小幅低于无该上界时的 `264.81M`。说明 DBLP g15 的 k=3 稠密性非常强，best 需要进一步降低，或需要额外的结构性保存条件。

DBLP full g15 query 1（历史 120s 限时，tree-aware 前）：
  仍停留在 k=2 pair 层
  处理到第 22 个 pair 时：
    finite≈48.97M
    active_seed≈48.14M
    best=19.4822
    full_prune≈5.93M
    far_prune≈0.106M
    seed_block_lb=0

DBLP full g15 query 1：
  约 5 分钟未产出第一条结果，停止。

DBLP 5k synthetic g15 进度观察：
  k=3 finite≈0.525M
  k=4 finite≈2.8M
  k=5 finite≈9.6M
  k=6 约 126s 时 finite≈23.97M
  k=6 约 222s 时 finite≈26.84M
  full_prune=0
```

结论：Test18 对中等组数是明确正优化；tree-aware greedy、cover-aware Complete、稠密行自适应表示和 pair/single 同根分区上界都值得保留。pair/single 分区是目前第一条能在 full DBLP g15 上把 k=3 前 best 从 `19.3812` 降到 `17.7702` 的机制；但最新探针也说明，单靠这一步仍不足以让早期 k=3 finite 发生数量级下降。若所有 k=3 行都接近全图稠密，仅 pair+k3 的 dense 距离表就约为 `(105+455)*n*sizeof(double)`，已经是十 GiB 级；继续到 k=4 会再次放大。因此要真正跑动 DBLP g15，下一步应沿着这个方向继续寻找更强的合法上界，或找到能减少 singleton+dense-pair 扫描/保存范围的结构性必要条件。当前 `Far/LowerBound/full_lb` 都不足以产生数量级剪枝；slack 诊断也说明低成本、小幅度增强 LB 很难解决问题。

## 10.1 2026-07-09 未保留：逐层同根分区上界

理论动机：固定 rows 探针不能保留，但“同一 root 上把若干已 ready rooted row 粘成完整树”本身是合法上界。于是短暂测试过一个更干净的逐层版本：每完成一层 `k`，用所有 `|B|<=k` 的 ready row 和 singleton，在每个候选 root 上做一次 partition DP，得到“全部组被若干 `<=k` 分块覆盖”的同根上界。它不剪状态、不改变 DP 值，也没有固定 rows、数据集名或时间点触发；复杂度口径为每层 `O(r*3^g)`，仍不乘图边数。

正确性守门：

```text
seed=808081, 80 组随机小图：ALL_OK
seed=818283, 40 组固定 g13 小图：ALL_OK
Toronto query 1：0.2582152999
DBLP snapshot g9 query 1：11.8766830000
```

效果：

- DBLP snapshot g9 query 1：`layer_partition_triggers=2`，`updates=0`，`layer_partition_ms=1.737`；
- `DBLP_data_new_bfs` g12 query 1：`layer_partition_triggers=4`，`updates=0`，`layer_partition_ms=69.613`，wall `36.406s`，与已有 frontier k=3/k=4 版 `35.967s` 同量级但没有新增上界；
- `DBLP_data_bfs` g12 query 1：`layer_partition_triggers=4`，`layer_partition_updates=1` 只出现在 k=3，k=5/k=6 均无更新；最终 `best_after_k4=17.1766480000` 不变，`layer_partition_ms=69.814`，wall `106.251s`，不优于已有 frontier k=3/k=4 版 `105.866s`。

判断：这条路比固定 rows 探针干净，但在两个 g12 DBLP 快照的高层 `k=5/k=6` 没有提供新上界，也没有状态收益。当前代码已撤回该探针，只保留这条负结果，避免后续重复实现。

## 11. 专题报告索引

2026-07-08 之后，本轮“各数据集实际效果”和“full DBLP g13 query 1 结构探针”不继续混写在本研究日志的长段落里，统一维护到 `test18_effect_report.md`。

该报告当前包含：

- fast snapshot 20 条 A/B 的分数据集统计；
- DBLP snapshot g9 补充探针；
- full DBLP g13 query 1 的 two-portal 完整实跑与 three-portal 结构探针对比：上一完整 two-portal 代码包含标准度缩图删/压 `747777` 点、Voronoi / two-portal 阶段总缩掉 `181591` 点、pair 层 finite 降到 `121.365M`、最终权重 `12.5936282853`；three-portal exact torso 探针在此基础上把 Voronoi 内部点提高到 `220206`、pair finite 降到 `119.455M`。探针最终同样得到权重 `12.5936282853`，但它来自撤回前二进制，不能作为当前源码完整成绩；
- 2026-07-09 当前源码 `portal==4` 收益门控版的 full DBLP g13 query 1 中止探针：不含固定 rows、`g==13` 或非 dense 高阶 light 特判；k=5 masks `1316` 到达 `best=12.9949`，masks `1625` 手动停止，无最终 weights 行，只作为撤回特判后的关键路径证据；
- 上一合规主线在 full DBLP g13 query 1 上的 leaf-reduction 实跑，包含跨过 k=4、进入 k=5、以及 exact current-row cover 在 k=4 后段把 best 降到 `13.1290` 的记录；
- full DBLP g13 query 1 的 pair 层、pair/single 分区、已撤出的 k=3 增量三分块 rows `96/160/224/256` 探针实测；
- 已撤出的高阶轻量表示组合探针，包含跨过完整 k=4、进入 k=5、以及 k=5 时间瓶颈的实时日志摘录；
- capacity-aware split、全终端候选根、hub pair/single 上界、k=4 quad 同根分区上界、actual-cover-aware 同根合并等已尝试但不保留的方向。

## 12. 图结构诊断：Voronoi / multiway-cut 代理

2026-07-08 新增 `tools/structure_probe`，只做诊断，不改 Test18 语义。它对每个组做多源 Dijkstra，用最近组给顶点染色，并把跨颜色边的端点当作 multiway-cut 代理边界。

full DBLP g13 query 1 的结果：

- `alive_roots_by_root_star=2,228,369 / 2,497,782 = 89.21%`；
- `voronoi_boundary_vertices=1,517,941 = 60.77%`；
- `boundary_edges=6,139,713 = 48.02%`；
- `non_boundary_components=523,529`；
- 最大非边界单色组件只有 `352` 点；
- `component_portals_total=705,300`，平均 portal 数 `1.35`；
- portal 分布为 `p0=139,149`、`p1=223,834`、`p2=92,310`、`p3_4=52,157`、`p5_8=13,200`、`p9_16=2,191`、`pgt16=688`；
- 原图上单独应用 Voronoi leaf 条件时，无 query terminal 且 portal 数 `<=1` 的可删枝叶顶点为 `638,697`，占全图 `25.57%`；three-portal exact torso 探针的实际前段归因为 `747,777 + 220,206` 个内部点，其中 two-portal 收缩贡献 `79,519`，three-portal 收缩贡献 `38,615` 并添加 `12,135` 个 hub；
- `metric_torso_vertices_est=1,520,103 = 60.86%`，`metric_torso_component_edges_est=1,422,520`。

40k DBLP snapshot g9 query 1 的结果：

- `alive_roots_by_root_star=40,000 / 40,000 = 100.00%`；
- `voronoi_boundary_vertices=36,184 = 90.46%`；
- `boundary_edges=415,639 = 63.23%`；
- 最大非边界单色组件 `106` 点；
- `metric_torso_vertices_est=36,203 = 90.51%`，可删枝叶顶点 `962 = 2.41%`。

判断：DBLP 不像有小 multiway cut，可以先排除“小 separator 直接参数化 DP”这个幻想；full DBLP g13 的非边界内域极碎且 portal 数很小，这解释了当前两级 exact 图缩减为什么有结构收益。当前已实现的保守入口包括标准 Steiner 非终端叶删/度 2 链压缩、无 query terminal 且 portal 数 `<=1` 的 Voronoi leaf deletion、portal 数 `==2` 的最短路桥边 exact 替换、portal 数 `==3` 的三终端 hub gadget，以及收益为正且局部表可行的 `portal==4` pair/triple/quad gadget；Toronto query 1 与 DBLP snapshot g9 query 1 权重保持一致。后续不再把普通 `portal>=5` local Steiner table / mimicking torso 当作优先实现方向，因为它属于图/询问本身的通用压缩，baseline 也可使用；只有能证明 Test18 特有或原创价值时才重新打开。
