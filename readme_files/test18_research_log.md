# Test18 research log：状态削减与 cover-aware 上界更新

Test18 是当前继续探索大组数的主线版本。它继承 Test17 的 Complete 优化，并进一步减少实际保存和扫描的 `(mask,v)` 状态数。

Test18 仍遵守 Test16 后的核心安全原则：

```text
不恢复非 exact h
不把 dp 上界当作下界
只使用安全下界或合法完整上界影响搜索
```

## 1. 与 Test17 的差异

Test18 主要增加六个机制：

1. 虚拟 singleton；
2. 全根 `full_lb` corridor；
3. `need[S,v]` stale 状态识别与 compact；
4. 稠密行自适应轻量表示，pair 行是默认 light 的特例；
5. cover-aware Complete，提前更新合法完整上界；
6. pair 层完成后的同根 pair/single 分区上界，尝试在进入 k=3 前降低 `best`。

当前实现还保留一个无参数初始上界增强：先从 root-star 最优根运行 greedy，再把这次 greedy 命中的组顶点作为候选根各运行一次 greedy。所有结果都只是合法完整上界，取最小值更新 `best`。

此外，Test18 不再预处理半掩码 Far 表；Far 改为当前 mask 内的 lazy cache。

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

进一步地，Test18 把这个策略推广到任意 `|S|>2` 的稠密行，但更保守：高阶行只有在上述 dense 成本判定成立时才转为 `light+dense`；否则仍保留原来的 `v,d,cover,need`。这样做的含义是：

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

自然的推广是：在所有 k=3 行完成后，再允许同根分区 DP 使用 singleton/pair/triple 三类块。该上界同样安全，复杂度为 `O(r g^2 2^g)`，由于 `r<=n` 且多项式因子被 `3^g` 吸收，理论上仍不超过主复杂度。但是实测不是正优化：Toronto g10 query 1 中没有更新 best，额外约 `17ms`；Toronto g12 query 1 中没有更新 best，额外约 `428ms`；DBLP snapshot large / `DBLP_data_bfs` g9 query 1 中也没有更新 best，额外约 `1.6ms`。因此当前只保留 k=2 后的 pair/single 分区；k=3 分区作为“形式正确但代价大于收益”的失败实验记录，不进入主线。

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
  高阶 dense-light 正式版：
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

DBLP g15 query 1（高阶 dense-light 后，k=3 初段探针）：
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
