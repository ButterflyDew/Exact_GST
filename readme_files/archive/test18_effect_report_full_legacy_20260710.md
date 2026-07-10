# Test18 效果报告：当前主线、DBLP g13 结构实测与撤出探针

> 归档状态说明：本文冻结的是撤回前历史口径。正文中把 frontier k=3/k=4 写作“当前候选”的位置已经过期；该机制后来因硬编码层级不符合 `agent.md` 第六条，从 Test18/Test19 源码和统计字段完整撤出。当前状态以 `readme_files/test18_effect_report.md` 为准。

本文是 Test18 的独立效果报告，只维护 A/B 统计、数据集实测、DBLP g13 结构探针和撤出探针结论。算法定义、正确性和复杂度见 `test18_algorithm.md`；更细的探索过程和失败路线归档见 `test18_research_log.md`。

计时口径：`CMakeLists.txt` 已显式保证 Release/RelWithDebInfo 使用 O2；关键 A/B 时间已在这个 O2 守门写入后重跑。full DBLP g13 长跑和早期结构探针没有在本轮重跑，统一视为“显式 O2 守门前历史记录”；检查到当时 Release 二进制已有 `/O2 /Ob2`，但这些历史时间不作为当前源码的关键 A/B。

注意：两个 g13 专用探针已从当前实现撤出。一类是 k=3 增量三分块同根上界，问题在于用固定 rows 触发；另一类是非 dense 高阶行轻量表示，问题在于用数据集、组数、层级和表示阈值外的组合条件硬触发。二者都缺少可发表的普适判据，违背 `agent.md` 第六条。本文只保留它们的观测数据，用来定位瓶颈和说明潜在收益，不把它们写作当前主线成绩。

## 1. 结论摘要

- fast snapshot 20 条查询权重全部与基线一致，live `(mask,v)` 从 `13.012804M` 降到 `9.283492M`，平均下降 `28.66%`。
- 40k DBLP snapshot / `DBLP_data_bfs` g9 query 1 与 DPBF 权重一致；标准 Steiner 度缩图 + Voronoi exact torso 后 `n=37384`、`m=652317`、`finite_states=1.700718M`、`live_states=0.349174M`，compact 对中等快照仍有效。
- exact query graph reduction 已分成两步接入 Test18 主线：先删非 query terminal 叶子/无 terminal 分量并压缩度 2 Steiner 链，再做 Voronoi 内域 exact torso；后者对 `portal<=1` 直接删除，对 `portal==2` 用组件内两门户最短路桥边替换，对 `portal==3` 用 pair 边 + 三终端 hub gadget 保留全部二端/三端连接代价，对 `portal==4` 只在删点数大于新增 5 个 hub 且局部 Steiner table 可由 pair/triple/quad gadget 精确保留时替换。Toronto query 1 先删/压 `11922` 点，再处理 `379` 个 Voronoi 内部点，其中二门户 `160` 点、三门户 `164` 点并添加 `54` 个 hub、四门户 `7` 点并添加 `5` 个 hub；DBLP snapshot g9 query 1 先删/压 `1510` 点，再处理 `1270` 个 Voronoi 内部点，其中二门户 `478` 点、三门户 `349` 点并添加 `119` 个 hub、四门户 `92` 点并添加 `45` 个 hub；权重均保持一致。
- full DBLP g13 query 1 的上一完整 two-portal 主线仍是当前合规完整基准：最终权重 `12.5936282853`，耗时 `20193.624s`，peak RSS `25185.023 MiB`。three-portal exact torso 的 g13 长跑探针得到同一权重，且显示 Voronoi 阶段删除内部点从 `181591` 增至 `220206`、pair 层 finite 从 `121.365M` 降到 `119.455M`、最终 finite 少 `17.73M`、peak RSS 低约 `245 MiB`；但该长跑使用的是撤回非 dense 高阶 light 之前的二进制，不能写作当前源码成绩。当前源码已重建，并通过随机对拍、Toronto query 1、DBLP snapshot g9 query 1 验证。
- 2026-07-09 当前源码 `portal==4` 收益门控版做过一次 full DBLP g13 query 1 中止探针：没有恢复任何 `g==13`、固定 rows、层数或非 dense 高阶 light 特判；它在 k=5 masks `1316` 得到 `best=12.9949`，到 masks `1625` 手动停止时 `finite=1,578,232,938`、`peak=19888.4 MiB`。该 run 没有最终 weights 行，不能作为完整成绩；只说明撤回第六条不合规特判后，当前源码仍能到达关键上界路径。中止后只含 header 的无效结果目录已清理。
- 补侧 Complete row 物化是 2026-07-09 新增的 Test18 内生机制：对固定 `rem=U-S`，用已保存 half-DP row 懒构造 `complete_row[v]=min_x dp[x][v]+dp[rem-x][v]`，后续普通 `Complete(u)` 直接查 row。它不是图/询问压缩，也没有 `g`、数据集、层数或时间点特判；rent/buy 触发只比较直接查询与物化 row 的表示成本。40k g12 三个快照权重均与回撤版一致；在显式 Release/O2 守门后重跑，wall time 分别约 `176.505s -> 103.110s`、`49.917s -> 38.144s`、`135.753s -> 64.341s`。
- frontier 同根分区上界是对已撤出固定 rows 三分块/四分块探针的干净替代候选：每完成一条 k=3 或 k=4 row，只用这条新 row 加补侧已 ready 的不超过同样大小的分块，在同一候选 root 上构造完整树上界。触发点来自 DP row ready 事件，没有数据集、固定 rows、层数阈值或时刻特判。随机对拍 `seed=707071` 80 组和固定 g13 小图 `seed=717273` 40 组通过；40k g12 三个快照均权重一致，k=3/k=4 都可前移 best，但端到端 wall 目前是混合结果：`103.110s -> 105.866s`、`38.144s -> 35.967s`、`64.341s -> 66.259s`。因此它是新的 g13 有上限探针候选，不写作已证实普适 wall-time 突破。
- 已撤出的 g13 k=3 三分块探针在 rows `96/160/224/256` 连续更新 `best`，说明如果能无参数地更早拿到强合法上界，k=3 后段状态能明显减少。
- 已撤出的高阶轻量表示探针曾在组合运行中跨过完整 k=4 并进入 k=5；这说明高阶 `cover/need` 常数和 k=5 join/search 是真实瓶颈，但该实现不再作为当前代码成绩。
- g13 query 1 的探针说明 three-portal exact torso 有明确结构收益；当前源码还新增了收益门控的 four-portal exact torso，并在 Toronto query 1、DBLP snapshot g9 query 1 和随机对拍中验证通过。full DBLP g13 query 1 不再作为常规验证项；只有出现突破性机制、需要确认最终权重，或要取得不可替代的关键输出时才重新长跑。
- 2026-07-09 理论筛选后进一步收窄方向：普通 `portal>=5` mimicking/torso 属于图/询问压缩，baseline 也可使用，除非能证明 Test18 特有或原创，否则不继续作为优先路线；无 rows 的 k=3 layer-end 同根三块分区也已短测并撤回，因为它没有比旧主线带来新的 `best_after_k3` 或状态收益。

## 2. 当前保留的主线机制

当前保留五类机制。

第一类是状态保存和 compact 的安全门控：

- best 下降后的 light / dense-light compact；
- 最大层 `|S|=H` 无未来读者时整行不保存；
- 奇数 `g` 最大层 forced-complete 保存下界；
- `|S|<H` 的 future split 保存下界 `max(LB(v,R), far_R(v)+near_R(v))`。

这些条件只在保存点或 compact 点生效，不进入当前 mask 的 Dijkstra relax，因此不要求新增下界满足 1-Lipschitz。最终仍是精确 DP：只删除不可能参与更优完整解的保存状态，只用合法完整树更新 `best`。

第二类是合法完整上界：

- 初始 tree-aware greedy；
- cover-aware Complete；
- k=2 完成后，使用 singleton/pair 同根分区上界。

同根分区上界只降低 `best`，不直接删除状态；收益来自更早变小的 `best` 让已有安全门控自然变强。

第三类是无数据集特判的表示降常数：

- pair 行默认按 sparse-light 保存精确 `v,d`；
- 任意行只在 `3*saved_count > 2*n` 的表示成本判定成立时转为 dense-light；
- 未通过表示成本判定的高阶行恢复普通 sparse，继续持久化 `cover,need`。

这样当前实现只保留由表示成本或 DP 结构推出的轻量行；除 pair 行外，非 dense 行无论 `g`、层数或数据集如何，都不再切换成 light 存储。

第四类是补侧 Complete row 物化：

- 对固定 `rem=U-S`，把普通 Complete 中反复枚举的 `min_x dp[x][v]+dp[rem-x][v]` 懒物化为临时 row；
- 只有当直接查询累计成本达到构建 row 的估计成本时才触发，没有数据集/组数/层级特判；
- actual cover 使 `cover_rem<rem` 时仍走原始现场枚举，因此该 row cache 不混淆 cover-aware Complete 的更小补集。

第五类是 exact query graph reduction：

- 先执行标准 Steiner 非终端缩图：删除非 query terminal 叶子、删除不含 query terminal 的连通分量，并把非 query terminal 的度 2 Steiner 链压缩成一条等长边；
- 再按组距离做 Voronoi 归属，处理无 query terminal 的单色内域组件：portal 数 `<=1` 时删除，portal 数 `==2` 时用组件内两门户最短路桥边替换，portal 数 `==3` 时用 pair 边 + 三终端 hub gadget 替换，portal 数 `==4` 时只在组件删点数大于新增 5 个 hub 且局部 Steiner table 可行时替换；
- 缩减前后分别输出 `original_n/original_m` 与 `n/m`；第一步用 `degree_reduce_*` 记录删除/压缩量，第二步用 `leaf_reduce_removed_*` 记录总缩减量，并用 `leaf_reduce_two_portal_*`、`leaf_reduce_three_portal_*`、`leaf_reduce_four_portal_*` 分别记录二门户、三门户和四门户替换。

这些规则都来自正权重 Steiner 树和二门户割的 exact 图等价，不是状态剪枝超参数；它们只减少后续 DP 的图规模，不改变最优解。

## 3. fast snapshot 多数据集效果

数据来源：`data_snapshot/generated_fast`，5 个 dataset version，每个 version 跑 `g=9..12` 各 1 条，共 20 条。当前组合版为 `result_snapshot/fast/20260708_133624`，基线为临时关闭 light/dense-light compact 且无 order prune 的 `result_snapshot/fast/20260708_121613`。20 条输出权重全部一致。

总体效果：

- live `(mask,v)`：`13.012804M -> 9.283492M`，下降 `28.66%`；
- finite states：下降 `22.48%`；
- `pull_hits`：下降 `21.93%`；
- `tryset_calls`：下降 `21.94%`；
- 总 query 时间：`99.926s -> 98.287s`，快 `1.64%`。

分数据集明细：

| dataset version | queries | baseline live | current live | live 下降 | finite 下降 | split states | order states | forced-LB states | pull_hits 下降 | tryset_calls 下降 | wall 变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `Toronto_data` | 4 | `901,189` | `492,302` | `45.37%` | `41.06%` | `358,814` | `58,868` | `10,936` | `38.37%` | `38.70%` | `+10.63%` |
| `Toronto_data_new` | 4 | `4,038,026` | `2,632,514` | `34.81%` | `30.01%` | `783,357` | `430,418` | `109,926` | `27.41%` | `27.57%` | `-4.20%` |
| `DBLP_data_bfs` | 4 | `2,575,905` | `2,329,185` | `9.58%` | `9.49%` | `12` | `214,752` | `29,995` | `0.11%` | `0.11%` | `-2.10%` |
| `DBLP_data_new_bfs` | 4 | `3,277,644` | `2,312,666` | `29.44%` | `11.47%` | `224,139` | `145,442` | `42,603` | `27.87%` | `27.88%` | `-1.83%` |
| `MovieLens_data_bfs` | 4 | `2,220,040` | `1,516,825` | `31.68%` | `31.67%` | `301,618` | `358,821` | `42,768` | `20.02%` | `20.02%` | `+3.59%` |
| **total** | 20 | `13,012,804` | `9,283,492` | `28.66%` | `22.48%` | `1,667,940` | `1,208,301` | `236,228` | `21.93%` | `21.94%` | `+1.64%` |

解读：

- 当前组合版已达到“多数据集平均 live states 下降 20%”的阶段性目标；
- `Toronto_data`、`Toronto_data_new`、`DBLP_data_new_bfs`、`MovieLens_data_bfs` 都超过 `20%`；
- `DBLP_data_bfs` 仍只有 `9.58%`，而且 future split 只额外删了 12 个状态，说明这组数据需要更结构性的条件；
- 这些数据来自 3500 点 fast snapshot，不能直接外推到 full DBLP。

## 4. Exact query graph reduction 实测

当前 Test18 的 query 级缩图有两步。第一步是标准 Steiner 非终端缩图：非 query terminal 叶子不可能出现在最优连接子图中；不含 query terminal 的连通分量对当前查询无用；非 query terminal 的度 2 Steiner 链可以用一条等长边替代。第二步是 Voronoi exact torso：单色非边界组件内没有当前 query terminal 时，按 portal 数做 exact 替换或删除；当前覆盖 `<=1/2/3`，并对收益为正且局部表可行的 `4` 门户组件启用 pair/triple/quad gadget。

这两步都只用当前 query 的 terminal 集合和正权重 Steiner 树性质，不按数据集、组数或运行阶段触发。

### 4.1 Toronto query 1

命令：

```powershell
build\Release\gst_test18_main.exe Toronto result_tmp_test18_p4_guard_toronto query.txt data 1 1
```

结果：

```text
best=0.2582152999
original_n/m=46073/68353
reduced_n/m=33831/56039
degree_reduce_removed_vertices=11922
degree_reduce_removed_edges=11970
degree_reduce_leaf_vertices=5505
degree_reduce_dead_vertices=0
degree_reduce_contracted_vertices=6417
degree_reduce_ms=21.250
leaf_reduce_removed_vertices=379
leaf_reduce_removed_edges=344
leaf_reduce_removed_components=122
leaf_reduce_two_portal_components=51
leaf_reduce_two_portal_vertices=160
leaf_reduce_two_portal_edges_added=51
leaf_reduce_three_portal_components=54
leaf_reduce_three_portal_vertices=164
leaf_reduce_three_portal_hubs_added=54
leaf_reduce_three_portal_edges_added=324
leaf_reduce_four_portal_components=1
leaf_reduce_four_portal_vertices=7
leaf_reduce_four_portal_hubs_added=5
leaf_reduce_four_portal_edges_added=22
leaf_reduce_ms=23.913
finite_states=40
live_states=40
wall_ms=97.306
peak_rss_mb=25.691
```

权重保持为已有正确值 `0.2582152999`。相对旧的 Voronoi-only 记录 `44803/67053`，标准度缩图先把大量非终端叶子和链压掉；四门户收益门控只额外处理 `1` 个组件 / `7` 个内部点。这类小查询状态本来很少，缩图没有带来 wall-time 收益，主要证明规则不会破坏结果。

### 4.2 DBLP snapshot g9 query 1

在 40k 点 DBLP snapshot large / `DBLP_data_bfs` g9 query 1 上，degree+Voronoi 两步缩图后权重仍为 `11.8766830000`，与 DPBF 一致：

```text
original_n/m=40000/657322
reduced_n/m=37384/652317
degree_reduce_removed_vertices=1510
degree_reduce_removed_edges=1639
degree_reduce_leaf_vertices=337
degree_reduce_dead_vertices=0
degree_reduce_contracted_vertices=1173
degree_reduce_ms=108.596
leaf_reduce_removed_vertices=1270
leaf_reduce_removed_edges=3366
leaf_reduce_removed_components=402
leaf_reduce_two_portal_components=182
leaf_reduce_two_portal_vertices=478
leaf_reduce_two_portal_edges_added=182
leaf_reduce_three_portal_components=119
leaf_reduce_three_portal_vertices=349
leaf_reduce_three_portal_hubs_added=119
leaf_reduce_three_portal_edges_added=714
leaf_reduce_four_portal_components=9
leaf_reduce_four_portal_vertices=92
leaf_reduce_four_portal_hubs_added=45
leaf_reduce_four_portal_edges_added=198
leaf_reduce_ms=100.681
finite_states=1.700718M
live_states=0.349174M
dense_rows=36
wall_ms=4325.699
peak_rss_mb=136.980
```

相对 two-portal 版同一 query 的 `finite_states=1.709344M`，三门户和四门户 exact torso 合计少约 `8.6k` finite states；相对三门户但无四门户收益门控的当前源码记录 `finite_states=1.702211M`，四门户再少约 `1.5k` finite states，并把最终图从 `37,431/652,849` 降到 `37,384/652,317`。`live_states` 仍为 `0.349174M`，变化很小。曾测试过不带收益门控的四门户替换；它虽然正确，但在 g9 上删 `251` 个内部点却新增 `385` 个 hub，使最终图变大，因此已拒绝。当前 `component_size > 5` 是表示收益门控：四门户 gadget 固定新增 5 个 hub，只有删掉的内部点更多时才进入主线。

补齐 normal Complete 的 actual-cover 版本后，`complement_cover_smaller=2672`，其中 normal Complete 相比旧 early-cover 额外缩小补集约 `1394` 次。这说明“当前树顺路覆盖额外组”这个性质确实存在，但在 g9 snapshot 上还不足以变成状态级收益。

此前只观察 light/dense-light compact 的 query 1-5 汇总为：`finite_states=10.718141M`，`compact_removed=4.426633M`，`live_states=6.291508M`，按状态数加权 compact 比例 `41.30%`。这说明 compact 对 40k snapshot g9 很有效，但它主要降低后续 join / lookup 触达，不等同于解决 full DBLP 的高阶 dense row。

## 5. full DBLP g13 query 1

数据：`data\DBLP\query_g13.txt` 第 1 条，`g=13`，总 group vertices 为 `2487`。组大小为：

```text
202, 164, 238, 167, 212, 197, 193, 188, 196, 179, 183, 202, 166
```

full DBLP 图规模为 `2,497,782` 点、`12,786,329` 边。

### 5.1 上一完整 run：degree + Voronoi two-portal exact torso 已跑通 g13 query 1

命令：

```powershell
build\Release\gst_test18_main.exe DBLP result_tmp_test18_two_portal_full_g13 g13 data 1 1
```

注：进度日志由 `GST_TEST18_PROGRESS=1` 显式开启；不再用 `g`、图规模或数据集形状自动判断是否输出长跑日志。

该 run 使用 three-portal 接入前的合规 Test18 代码：标准 Steiner 度缩图、Voronoi leaf / two-portal torso、cover-aware Complete、pair/single 同根分区、正确 DP 顺序保存条件全部启用；不包含固定 rows 三分块上界，也不包含已撤出的非 dense 高阶行轻量表示特判。run 已得到最终 weights 行；下表为实时进度日志和最终 stats 中的稳定里程碑。

| 阶段 | elapsed | best | finite states | dense_rows | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| preprocess done | `30.090s` | `15.0174` | - | - | - | `degree_reduce_ms=5755.63`，`degree_removed_vertices=747777`，`leaf_removed_vertices=181591`，其中二门户 `79519` 点、桥边 `29456` 条 |
| k=2 完成，78 个 pair mask | `192.978s` | `15.0174` | `121,364,979` | `78` | - | 相对上一完整 portal `<=1` 主线少 `5.75M` finite |
| pair/single 分区后进入 k=3 | `201.078s` | `14.7395` | `121,364,979` | `78` | - | 当前保留的合法上界正常触发 |
| k=3 完成，进入 k=4 | `885.888s` | `14.7395` | `550,354,284` | - | - | 相对上一完整 portal `<=1` 主线少约 `22.88M` finite |
| k=4 masks `889` | `2121.120s` | `13.1290` | `1,262,730,807` | - | - | 同点比上一完整 portal `<=1` 主线少约 `44.61M` finite |
| enter k=5 | `2574.770s` | `13.1290` | `1,393,763,583` | - | - | 当前主线已完整跨过 k=4；同点少约 `46.06M` finite |
| k=5 masks `1600` | `7179.880s` | `12.9949` | `1,588,804,438` | `894` | `19990.2 MiB peak` | 旧 portal `<=1` 同点为 `7556.640s / 1.636567B / 20632.1 MiB peak` |
| k=5 masks `1800` | `9477.180s` | `12.9949` | `1,655,073,329` | `897` | `21478.5 MiB peak` | 旧 portal `<=1` 同点为 `9945.570s / 1.703303B / 22157.1 MiB peak` |
| k=5 masks `2338` | `16078.700s` | `12.8052` | `1,809,347,214` | `899` | `24987.2 MiB peak` | k=5 后段由当前主线再次得到更强合法上界；不依赖撤回特判 |
| enter k=6 | `16460.000s` | `12.8052` | `1,815,213,034` | `899` | `25115.7 MiB peak` | 当前主线完整跨过 k=5 |
| k=6 masks `2544` | `16991.300s` | `12.5944` | `1,816,373,690` | `899` | `25139.2 MiB peak` | 最大层也产生新的合法上界；内存基本未继续上涨 |
| k=6 masks `3000` | `18053.400s` | `12.5944` | `1,818,288,087` | `899` | `25171.3 MiB peak` | best 暂未继续下降；最大层 finite 增长远慢于 k=5 |
| final | `20193.624s` | `12.5936282853` | `1,819,204,977` | `899` | `882.5 / 25185.0 MiB` | 完整跑通 query 1；`live_states=1,186,737,682`，`stale_need_skips=1,295,089,242` |

结论：

- 该 two-portal run 在 full DBLP g13 query 1 上先通过标准度缩图删/压 `747,777` 点，再通过 Voronoi leaf / two-portal torso 总计缩掉 `181,591` 点，DP 图顶点数从 `2,497,782` 降到 `1,568,414`；
- pair 层 finite 从上一完整 portal `<=1` 主线 `127,116,733` 降到 `121,364,979`，下降约 `4.52%`；相对早期无缩图基线 `165,917,671`，下降约 `26.85%`；
- 新增缩图不会直接降低 `best`，但 pair/single 同根分区仍在进入 k=3 前把 `best` 从 `15.0174` 降到 `14.7395`；
- 该 two-portal run 已完整跑通 DBLP g13 query 1；在 k=5 masks `1316` 将 best 降到 `12.9949`，masks `2338` 继续降到 `12.8052`，k=6 masks `2544` 再降到 `12.5944`，最终到 `12.5936282853`；two-portal exact torso 的收益主要表现为图规模、finite states、wall time 和峰值 RSS 同时下降；
- g13 query 1 已完整跑通；若继续扩展到更多 DBLP g13 query 或更大组数，k=5/k=6 join-search 的单行成本和高阶 sparse 行常数仍是当前瓶颈。

### 5.2 g13 探针：degree + Voronoi three-portal exact torso

当前源码在 two-portal torso 之上新增 `portal==3` exact hub gadget。下面这次 full DBLP g13 query 1 长跑保留为结构和瓶颈探针：它能说明 three-portal exact torso 对图规模、pair 层和最终状态规模的影响，但它使用的是撤回非 dense 高阶 light 之前的二进制；撤回后的当前源码尚未重新做 full g13 长跑。因此本节不把 `20333.217s` 写作当前主线成绩。

命令：

```powershell
$env:GST_TEST18_PROGRESS='1'
build\Release\gst_test18_main.exe DBLP result_tmp_test18_three_full_g13_final g13 data 1 1
```

关键日志与最终 stats（探针，不作为当前源码完整成绩）：

| 阶段 | elapsed | best | finite states | dense_rows | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| preprocess done | `28.713s` | `15.0174` | - | - | - | `degree_removed_vertices=747777`，`leaf_removed_vertices=220206`，其中二门户 `79519` 点、三门户 `38615` 点、三门户 hub `12135` 个 |
| k=2 完成，78 个 pair mask | `182.196s` | `15.0174` | `119,454,597` | `78` | `3096.41 MiB peak` | 相对 two-portal 完整 run 的 `121,364,979` 少约 `1.91M` finite |
| pair/single 分区后进入 k=3 | `189.972s` | `14.7395` | `119,454,597` | `78` | - | 上界更新行为与 two-portal run 一致 |
| k=3 完成，进入 k=4 | `841.761s` | `14.7395` | `542,517,085` | - | - | 相对 two-portal 完整 run 的 `550,354,284` 少约 `7.84M` finite |
| k=4 masks `889` | `2021.990s` | `13.1290` | `1,246,888,758` | `865` | `12795.8 MiB peak` | 同点比 two-portal 少约 `15.84M` finite，早期仍更快 |
| enter k=5 | `2460.140s` | `13.1290` | `1,377,298,654` | - | - | 相对 two-portal 少约 `16.46M` finite，快约 `115s` |
| k=5 masks `1600` | `7045.170s` | `12.9949` | `1,571,633,211` | `895` | `19757.8 MiB peak` | 相对 two-portal 少约 `17.17M` finite，peak 低约 `232 MiB` |
| k=5 masks `1800` | `9328.020s` | `12.9949` | `1,637,713,596` | - | `21241.5 MiB peak` | 相对 two-portal 少约 `17.36M` finite，快约 `149s` |
| k=5 masks `2338` | `16153.000s` | `12.8052` | `1,791,626,785` | `900` | `24746.9 MiB peak` | finite 继续少约 `17.72M`，但耗时已比 two-portal 同点慢约 `74s` |
| enter k=6 | `16556.200s` | `12.8052` | `1,797,484,574` | - | - | 相对 two-portal 少约 `17.73M` finite，慢约 `96s` |
| k=6 masks `2544` | `17101.900s` | `12.5944` | `1,798,644,209` | - | `24898.4 MiB peak` | 与 two-portal 同样在最大层得到更强合法上界 |
| k=6 masks `3000` | `18209.400s` | `12.5944` | `1,800,557,375` | `900` | `24925.2 MiB peak` | 后段增长缓慢，但单 mask / join-search 变重 |
| final | `20333.217s` | `12.5936282853` | `1,801,473,879` | `900` | `882.094 / 24940.082 MiB` | `live_states=1,180,035,239`，`stale_need_skips=1,288,362,792` |

结论：

- 三门户 torso 在 full g13 上把 Voronoi 阶段处理的内部点从 two-portal 的 `181,591` 增至 `220,206`；由于新增 `12,135` 个 hub，最终 DP 图顶点数为 `1,541,934`，相对 two-portal 的 `1,568,414` 净少约 `26,480`；
- pair 层 finite 从 `121.365M` 进一步降到 `119.455M`。这一段只依赖 exact graph reduction 和 pair 行，仍是当前源码可保留的结构性信息；
- 探针最终 finite 从 two-portal 的 `1,819,204,977` 降到 `1,801,473,879`，peak RSS 从 `25185.023 MiB` 降到 `24940.082 MiB`，但这些高层性能数字来自撤回前二进制，只能作为瓶颈定位参考；
- 探针 wall time 从 `20193.624s` 增至 `20333.217s`，慢约 `139.6s`。即使不考虑二进制差异，也不能把 three-portal 写成端到端加速；更稳妥的结论是它降低图规模和状态量，但 k=6 后段的单 mask / join-search 代价会回吐早期时间收益。

### 5.3 当前源码中止探针：degree + Voronoi p4 guarded，不含撤回特判

这次 run 使用 2026-07-09 当前源码：标准 Steiner 度缩图、`portal<=1/2/3` exact torso、收益门控的 `portal==4` pair/triple/quad gadget、pair/single 同根上界和现有安全保存条件全部启用。保存行逻辑没有按数据集、组数、固定 rows 或层数触发的高阶 light 特判；非 dense 高阶行保持普通 sparse，只有 `k==2` 的结构 light 和 `3*saved_count > 2*n` 的 dense-light 成本判定。

命令：

```powershell
$env:GST_TEST18_PROGRESS='1'
build\Release\gst_test18_main.exe DBLP result g13 data 1 1
```

该 run 的目的只是确认当前源码撤回不合规特判后是否仍能到达关键 g13 上界路径；在用户提醒 full g13 q1 长跑成本过高后，于 k=5 masks `1625` 手动停止。它没有最终 weights 行，不能写作完整性能成绩。停止时 `result\DBLP\Test18\query_g13` 只含 header，已清理，避免污染正式结果目录。

关键日志：

| 阶段 | elapsed | best | finite states | dense_rows | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| preprocess done | `27.684s` | `15.0174` | - | - | - | `degree_removed_vertices=747777`，`leaf_removed_vertices=225679`；相对 three-portal 探针的 `220206`，p4 guarded 额外缩掉 `5473` 个 Voronoi 内部点 |
| k=2 完成，78 个 pair mask | `175.839s` | `15.0174` | `119,317,630` | `78` | - | 相对 three-portal 探针少约 `0.137M` finite |
| pair/single 分区后进入 k=3 | `183.452s` | `14.7395` | `119,317,630` | `78` | - | 合法同根上界仍正常触发 |
| k=3 完成，进入 k=4 | `808.023s` | `14.7395` | `542,062,434` | - | - | 相对 three-portal 探针少约 `0.455M` finite |
| k=4 masks `889` | - | `13.1290` | `1,246,102,085` | - | - | 同点相对 three-portal 探针少约 `0.787M` finite |
| enter k=5 | `2362.270s` | `13.1290` | `1,376,487,221` | - | - | 当前源码已跨过 k=4 |
| k=5 masks `1142` | `2766.620s` | `13.1267` | `1,395,330,278` | `877` | `15960.5 MiB peak` | k=5 初段自然出现小幅合法上界更新 |
| k=5 masks `1256` | `3756.280s` | `13.1226` | `1,445,337,392` | `882` | `17035.5 MiB peak` | dense-light 增长来自统一成本阈值 |
| k=5 masks `1316` | `4373.210s` | `12.9949` | `1,479,283,932` | `890` | `17691.2 MiB peak` | 到达旧完整 run 的关键强上界路径 |
| k=5 masks `1600` | `6814.470s` | `12.9949` | `1,570,795,606` | `895` | `19711.7 MiB peak` | 与旧 three-portal 探针同点 best 一致，但仍不是完整 run |
| stopped at k=5 masks `1625` | `7126.620s` | `12.9949` | `1,578,232,938` | `895` | `19888.4 MiB peak` | 用户提醒后停止；没有最终 weights / stats 行 |

结论：

- 这次当前源码探针保留的有效信息是：撤回固定 rows 三分块上界和非 dense 高阶 light 特判后，full DBLP g13 query 1 仍能在 k=5 中段到达 `best=12.9949`；
- `portal==4` 收益门控的结构收益是可见但不巨大：preprocess 相对 three-portal 探针额外缩掉 `5473` 个 Voronoi 内部点，pair 层少约 `0.137M` finite，k=4 masks `889` 少约 `0.787M` finite；
- 该 run 不提供最终权重和完整 wall time，不应与 two-portal 完整 run 或撤回前 three-portal 完整探针混写；
- 后续不再把 full DBLP g13 query 1 当作常规回归。只有先在随机对拍、小数据集、snapshot 或结构探针上拿到明确突破，再用 full g13 q1 验证最终权重或关键里程碑。

### 5.4 上一合规主线：仅 Voronoi leaf reduction 后跨 k=4/k=5

命令：

```powershell
build\Release\gst_test18_main.exe DBLP result_tmp_test18_leaf_reduce_dblp_g13 g13 data 1 1
```

该 run 使用新增标准度缩图之前的合规 Test18 主线：包含 Voronoi leaf reduction、cover-aware Complete、pair/single 同根分区、正确 DP 顺序保存条件；不包含固定 rows 三分块上界，也不包含已撤出的高阶轻量表示特判。run 在进入 k=5 后手动停止，因此没有最终 weights 行；下表为实时进度日志。它的作用是证明撤出两类 g13 特判后，合法主线仍可跨过 k=4；但它不是当前 degree+Voronoi 代码的完整结果。

| 阶段 | elapsed | best | finite states | dense_rows | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| preprocess done | `27.292s` | `15.0174` | - | - | - | `group_dist_ms=18137.6`，`greedy_ms=3396.1` |
| k=2 完成，78 个 pair mask | `222.586s` | `15.0174` | `142,322,584` | `78` | `3135.04 / 3147.38 MiB` | leaf reduction 后 pair 状态少于旧基线 |
| pair/single 分区后进入 k=3 | `232.420s` | `14.7395` | `142,322,584` | `78` | - | 当前保留的合法上界 |
| k=3 完成，进入 k=4 | `1056.310s` | `14.7395` | `634,175,441` | `364` | `7189.82 / 7205.02 MiB` | 比旧无 leaf 的 `715.40M` 少约 `81.22M` finite |
| k=4 masks `436` | `1262.270s` | `14.7395` | `743,332,942` | `436` | `8203.57 / 8203.57 MiB` | 同状态量下 RSS 明显低于已撤出组合探针 |
| k=4 masks `860` | `2473.890s` | `14.3121` | `1,402,085,407` | `859` | `14226.5 / 14226.5 MiB` | current-row cover 首次更新 best |
| k=4 masks `864` | `2485.330s` | `14.0441` | `1,406,710,802` | `859` | `14339.3 / 14339.3 MiB` | 第 2 次 best 更新 |
| k=4 masks `889` | `2551.440s` | `13.1290` | `1,430,116,033` | `859` | `14863.9 / 14874.3 MiB` | exact cover 把 best 推到旧撤出探针同量级 |
| enter k=5 | `3089.300s` | `13.1290` | `1,566,731,569` | `865` | - | 完整跨过 k=4 |
| k=5 masks `1083` | `3123.950s` | `13.1277` | `1,567,888,412` | `865` | `17917.0 / 17921.4 MiB` | 普通 Complete 小幅更新；随后手动停止 |

结论：

- Voronoi leaf reduction 在 full DBLP g13 上把 k=3 末尾 finite 从旧基线约 `715.40M` 降到 `634.18M`，也把 pair 层 finite 从 `165.92M` 降到 `142.32M`；
- 但单靠缩图不能解决 best 缺口：k=4 中段以前 best 仍停在 `14.7395`，状态数继续涨到十亿级；
- 该合规历史 run 在 k=4 后段依靠 exact current-row cover 连续把 best 降到 `13.1290`，这是真正可保留的有效信息；
- 该合规历史 run 已能完整跨过 k=4 并进入 k=5；k=5 初段内存约 `17.9 GiB`，单 mask 耗时从数秒到十几秒。当前 degree+Voronoi run 已确认同段 finite/RSS 更低，下一步比较重点转为 k=5 后续 mask 的耗时构成。

### 5.5 早期基线：pair 层与 k=3 瓶颈

无三分块增量上界的早期探针：

| 阶段 | elapsed | best | finite states | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | --- |
| preprocess done | `22.571s` | `15.0174` | - | - | `group_dist_ms≈19102.1`，`greedy_ms≈3378.5` |
| k=2 完成，78 个 pair mask | `256.031s` | `15.0174` | `165,917,671` | `2822.65 / 2825.18 MiB` | `pair_dense_rows=78` |
| pair/single 同根分区后进入 k=3 | `267.504s` | `14.7395` | `165,917,671` | - | 合法上界有效 |
| k=3 处理到累计 `102` masks | `345.322s` | `14.7395` | `211,392,344` | `3269.88 / 3282.59 MiB` | 被手动中断 |

结论：g13 能完整跑完 pair 层，但进入 k=3 后仍快速增加有限状态和 RSS；瓶颈是高阶行接近 dense，而不是 pair 层能否完成。

### 5.6 已撤出的三分块同根上界探针

该探针使用已 ready 的 singleton/pair/triple rooted row，在同一 root 上做集合分区 DP。每个分块都是精确已知的合法 rooted tree，总代价求和可能重复计边，因此上界本身是安全的。

撤出原因：探针在 `g=13` 的 k=3 中按已处理 triple rows `96/160/224/256` 增量触发，并在 layer-end 兜底。固定 rows 没有充分理论依据，是面向数据的工程性超参数。当前代码不再启用这一路径，也不再输出 `triple_partition_*` 统计。

最新探针命令：

```powershell
build\Release\gst_test18_main.exe DBLP result_tmp_test18_dblp_g13_triple_256 g13 data 1 1
```

关键触发点：

| 触发点 | elapsed | best 变化 | 分区耗时 | 下一条进度样本 | finite states | dense_rows | RSS / peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| triple rows `96` | `581.210s` | `14.7395 -> 14.3326` | `64.552ms` | cumulative masks `174` | `351,495,565` | `174` | `4643.24 / 4655.09 MiB` |
| triple rows `160` | `807.096s` | `14.3326 -> 13.9751` | `63.797ms` | cumulative masks `238` | `471,188,901` | `238` | `5856.14 / 5856.14 MiB` |
| triple rows `224` | `1036.420s` | `13.9751 -> 13.8176` | `65.749ms` | cumulative masks `302` | `582,545,844` | `300` | `7118.45 / 7118.45 MiB` |
| triple rows `256` | `1173.360s` | `13.8176 -> 13.3535` | `71.139ms` | cumulative masks `334` | `635,871,829` | `317` | `7994.81 / 8011.73 MiB` |
| layer-end rows `286` | `1303.160s` | `13.3535 -> 13.3535` | `78.130ms` | enter k=4 | `680,000,972` | `318` | `8985.03 / 8985.03 MiB` |

保留下来的有效信息：

- 只在 k=3 layer-end 做三分块时，k=3 完成前仍是 `best=14.7395`、`finite_states=715,400,289`、`dense_rows=364`、RSS 约 `8268.88 MiB`；随后才用约 `51s` 把 best 降到 `13.3535`。
- 增量触发后，k=3 完成时已是 `best=13.3535`、`finite_states=680,000,972`、`dense_rows=318`。相对 layer-end-only，少保存约 `35.40M` 个 finite states 和 `46` 个 dense rows。
- 新增 rows `256` 触发点本身是有效的：相对只触发到 rows `224` 的探针，k=3 末尾从 `686,231,521` finite states / `336` dense rows 降到 `680,000,972` / `318`。
- k=4 初段探针到 cumulative masks `384` 时，`finite_states=695,763,926`、`dense_rows=318`、RSS / peak RSS 约 `9333.23 / 9337.82 MiB`，随后手动中断。也就是说该改动减少了 k=3 后段状态和 dense 行，但还没有跑通 full g13。
- 这个方向的正确结论不是“保留固定 rows 触发”，而是“需要寻找无参数、可解释的更早合法上界”。

### 5.7 已撤回且不得恢复：非 dense 高阶行轻量表示探针

想法：k=4 初段显示 `dense_rows` 长时间停在 k=3 末尾的 `318` 附近，继续增长的主要是高阶 sparse 行。对这些行，`d` 是精确 DP 值；`cover` 只服务于上界更新，`need` 只服务于 stale skip。因此当时探针曾强行让部分非 dense 高阶行只保存 `v,d`，用来判断高阶行常数是否是瓶颈。

撤出理由：这个表示本身不改变 DP 值，但触发范围不是由通用成本模型或理论下界推出，而是面向某个数据集、组数和层级阶段的特判。因此当前代码已删除该分支；当前保存行逻辑只有 `k==2` 的结构 light 和 `dense_by_cost` 的通用 dense-light，非 dense 高阶行恢复普通 sparse 并持久化 `cover,need`。文档也不再把它列入当前机制，只在这里保留瓶颈定位数据。

验证：

```powershell
cmake --build build --config Release --target gst_test18_main gst_dpbf_main gst_random_compare
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 202020 80 4 10 2 8 .tmp_random_compare_test18_a 0
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 424244 40 13 18 13 13 .tmp_random_compare_test18_b 0
```

两组随机对拍均为 `ALL_OK`。DBLP 探针叠加了 5.6 中已撤出的三分块探针，并在 k=5 初段手动中断；当时的临时结果目录已清理。因此下表只保留实时进度日志摘录，作为高阶行常数和 k=5 瓶颈定位证据，不作为当前代码独立成绩。

| 阶段 | elapsed | best | finite states | dense_rows | RSS / peak RSS | 备注 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| k=4 masks `384` | `1349.950s` | `13.3535` | `695,763,926` | `318` | `9152.54 / 9166.09 MiB` | 同一状态数下 RSS 低于上一轮约 `9333 MiB` |
| k=4 masks `436` | `1477.300s` | `13.3535` | `743,350,005` | `318` | `9697.98 / 9709.17 MiB` | quad 探针同点约 `10423.9 MiB` |
| k=4 masks `890` | `2634.750s` | `13.1290` | `1,174,456,672` | `329` | `14616.8 / 14627.8 MiB` | `early_cover` 首次继续改善 best |
| enter k=5 | `3207.530s` | `13.1290` | `1,313,498,293` | `329` | - | 完整跨过 k=4 |
| k=5 masks `1083` | `3265.000s` | `13.1277` | `1,314,666,443` | `329` | `16219.3 / 16225.2 MiB` | 普通 Complete 小幅更新 best |
| k=5 masks `1142` | `4015.980s` | `13.1267` | `1,333,040,577` | `329` | `16426.4 / 16432.2 MiB` | 手动中断 |

结论：

- 组合探针把 DBLP g13 从“k=4 初段探针”推进到“完整跨过 k=4 并进入 k=5”，只能说明高阶行 `cover/need` 常数确实值得关注，不能证明这条特判可以保留；
- 当时的硬编码启用条件不可保留；后续如果要做高阶 light 表示，必须换成无数据集特判的成本模型或理论条件；
- 它没有改变状态数增长本身，k=4 末尾仍达到约 `1.313B` finite states，峰值 RSS 约 `16.2 GiB`；
- k=4 后段 `early_cover` 能把 `best` 从 `13.3535` 降到 `13.1290`，这说明继续保留 exact current-row cover 仍有价值；
- k=5 初段内存基本横住，但每个 mask 需要十几秒甚至更久，下一步瓶颈应转向 k=5 的 join/search 成本，而不是继续只降存储常数。

## 6. 已尝试但不保留的 g13 方向

### 6.1 capacity-aware future split 保存下界

想法：当 `g=13`、`H=6` 且补集很大时，未来两块各自最多容纳 `H` 个组，于是把 `far+near` 增强为容量感知的两块最远距离下界。

结果：

- 随机对拍 `seed=616161`、`seed=717171`，各 80 组通过；
- DBLP g13 前 18 个 pair mask：finite 从原版 `38,206,609` 降到 `37,193,165`，只降约 `2.7%`；
- elapsed 到前 18 个 pair mask：约 `59.57s -> 66.39s`，明显变慢。

结论：作为保存点下界是可行方向，但在 full DBLP g13 上剪得太少、常数变大，因此不保留。

随后还测试过一个更窄的 compact-only 版本：普通保存点仍用 `far+near`，只在 best 下降后的 light/dense-light compact 中使用容量顺序统计量，试图专门清理 k=4/k=5 前的 live 行。这个版本不安全，随机对拍 `seed=424244` 在第 6 例给出反例：DPBF 最优 `33`，compact-only capacity 版得到 `35`。原因是未来 join 之后还会发生图搜索，若把多个补侧块从当前 root 出发的距离直接相加，会重复计算后续共享的移动路径，因此不能作为保存状态的必要下界。

### 6.2 pair/single 分区候选根扩到所有终端

想法：原版只用最小组的终端作为同根分区候选根；g13 上尝试把所有 query group 终端都作为候选根，提高找到更好完整上界的概率。

结果：

- 随机对拍 `seed=818181`、`seed=919191`，各 80 组通过；
- 该探针叠加了 capacity-aware split，因此不把状态数作公平对比；
- 只看上界，进入 k=3 时 best 仍为 `14.7395`，没有优于原版 pair/single 分区；
- 候选根扩展带来约数十秒额外开销。

结论：正确但收益为零，当前不保留。

### 6.3 hub pair/single 上界

想法：同根分区要求所有 singleton / pair 组件在同一个 root 拼接，限制较强；hub 版改为选择少量 hub，从 hub 到 pair rooted tree root 再连接，构造合法但可能更灵活的完整上界。

正确性：每个 pair 组件使用已保存的精确 pair rooted tree，再加 hub 到该 root 的最短路；singleton 使用 hub 到该组终端的最短路。所有组件通过 hub 连通，求和可能重复计边，因此只是高估合法树代价，可以安全更新 `best`。

结果：

- 随机对拍 `seed=123123`、`seed=321321`，各 60 组通过；
- DBLP g13 pair 层完成后，原版 best 已为 `14.7395`；
- hub 版进入 k=3 时 elapsed 约 `293.786s`，best 仍为 `14.7395`；
- 继续到 k=3 masks=116，finite states 为 `238,002,718`、RSS 约 `3539.84 MiB`，best 仍未改善。

结论：上界构造正确，但没有降低 g13 的关键 best，还增加 pair 后的额外 Dijkstra / 扫 pair 行代价，因此已从热路径撤回，只保留为失败实验记录。

### 6.4 k=4 quad 同根分区上界

想法：既然 k=3 的 singleton/pair/triple 同根分区能把 layer-end 上界前移，那么 k=4 中自然可以再允许 singleton/pair/triple/quad 四类块，尝试继续降低 `best`，从而压住 k=4 sparse 行的增长。

正确性：quad 版本仍只使用已 ready 的精确 rooted row 与 singleton `gd`，并把所有分块同根粘合。各分块求和可能重复计边，因此不会低估合法完整树代价，只能安全降低 `best`。

结果：

- 随机对拍 `seed=202020` 80 组、`seed=424244` 40 组通过；
- full DBLP g13 query 1 复现了 k=3 rows `96/160/224/256` 的 best 更新，并以 `best=13.3535`、`finite_states=680,000,972`、`dense_rows=318` 进入 k=4；
- k=4 quad rows `16`：`best=13.3535 -> 13.3535`，未更新，耗时约 `138.594ms`；
- k=4 quad rows `32`：未更新，耗时约 `134.209ms`；
- k=4 quad rows `64`：未更新，耗时约 `142.321ms`；
- 手动中断时到 cumulative masks `436`，`finite_states=743,350,005`、`dense_rows=318`、RSS / peak RSS 约 `10423.9 / 10424.3 MiB`，early-cover 最好候选约 `18.1564`，仍远弱于当前 `13.3535`。

结论：quad 上界构造正确，但在 k=4 早段没有继续降低 g13 的关键 best，只增加额外上界计算。因此已从热路径撤回，暂不保留。

### 6.5 actual-cover-aware 同根合并

想法：普通同根合并按名义 mask `A+B=S` 拉取两个 rooted row；但若 `A` 侧状态的 actual cover 已经覆盖了 `B` 中的一部分组，则理论上只需要补 `S-cover(A)`。这只增加合法候选，不剪状态，本身是安全的结构性增强。

探针实现只对非 light 行使用持久 `cover`，不对 sparse-light / dense-light 行猜 cover。随机对拍 `seed=202020` 80 组和 `seed=424244` 40 组均为 `ALL_OK`。

小图实测说明它命中太少：

- Toronto query 1：`cover_pull_checks=0`，没有触发；
- DBLP snapshot large / `DBLP_data_bfs` g9 query 1：`cover_pull_checks=714,348`，`cover_pull_hits=459`，真正改善当前候选只有 `cover_pull_updates=1`；权重仍为 `11.8766830000`，但 wall time 为 `4.388s`，慢于当前主线文档中的约 `4.079s` 级别。

结论：这条路的理论形式是对的，但在当前表示下太窄。该探针运行在当时仍含已撤出的非 dense 高阶行轻量表示版本上；当前撤回后，非 dense 高阶行已恢复持久化 `cover/need`，但已有 Toronto / DBLP g9 结果显示 actual-cover-aware 命中很少、收益窄。因此该实现不进入主线，只保留为负结果。

## 7. 下一步判断

DBLP g13 的证据现在更清楚：two-portal 主线能减少小图 live states，也能把 full DBLP g13 pair 层从上一完整 portal `<=1` 主线的 `127.117M` 进一步压到 `121.365M`，把进入 k=5 的 finite 从 `1.439822B` 压到 `1.393764B`，并在 k=5 masks `2338` 推进到 `12.8052`，k=6 masks `2544` 再推进到 `12.5944`，最终权重 `12.5936282853`。three-portal exact torso 的 g13 探针又把 pair 层压到 `119.455M`，说明 `portal==3` exact torso 是有结构收益的；但该完整长跑来自撤回前二进制，不能当作当前源码性能成绩。当前源码已进一步接入收益门控的 `portal==4` exact torso，并在 Toronto query 1、DBLP snapshot g9 query 1 与随机对拍中通过；2026-07-09 中止探针又确认当前源码在不恢复任何第六条不合规特判时仍能在 k=5 masks `1316` 到达 `best=12.9949`。已撤出的三分块探针说明，更早的强合法上界能把 k=3 后段从 dense 洪峰里削掉一部分，但固定 rows 触发不能保留；已撤出的非 dense 高阶行轻量表示探针说明高阶行常数和 k=5/k=6 join-search 是真实瓶颈，但当时的硬编码启用方式不能保留。下一步不应继续堆低收益局部 LB，更值得投入的是：

- full DBLP g13 query 1 只在有突破性新机制、需要最终权重确认，或必须获取不可替代的关键里程碑时再长跑；常规验证优先使用随机对拍、Toronto、DBLP snapshot g9 和结构探针；
- 针对 k=5 的 join/search 成本做分解诊断，尤其是哪些 split 与哪些已保存行贡献了主要扫描；
- 继续寻找能在进入 k=3 前或 k=3/k=4 更早阶段显著压低 `best` 的合法完整上界；
- 针对 singleton + dense-pair / 高阶 sparse 行扫描的结构性保存条件；
- 不再把普通 `portal>=5` local torso / mimicking 压缩作为优先实现方向；这类图/询问压缩 baseline 也可使用，除非后续能提出 Test18 特有或原创的利用方式。

### 7.1 k=5/k=6 join/search 成本分解诊断

2026-07-09 在不改变算法语义的前提下，新增了分层诊断字段：`pull_*_k*`、`search_*_k*`、`complement_*_k*` 以及对应的 `*_ms_k*`。它们只统计 Test18 内部 DP 行 join、Dijkstra search 和 Complete 补侧扫描成本，不参与任何剪枝或上界判定。

验证：

```powershell
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 202020 80 4 10 2 8 .tmp_random_compare_join_diag 0
build\Release\gst_test18_main.exe Toronto result_tmp_join_diag_toronto query.txt data 1 1
build\Release\gst_test18_main.exe DBLP_data_bfs result_tmp_join_diag_large_dblp_g9 g9 data_snapshot\generated_large 1 1
```

结果：随机对拍 `ALL_OK`；Toronto query 1 权重 `0.2582152999`；40k DBLP snapshot g9 query 1 权重 `11.8766830000`。

fast g12 query 1 诊断摘录：

| dataset | k | pull_ms | search_ms | complement_ms | pull_scan | dense-dense | dense-sparse | singleton | sparse-sparse | complement_scan | pq_pop |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs` | 5 | `390.3` | `3611.7` | `2942.9` | `19.83M` | `45.1%` | `22.6%` | `25.2%` | `7.1%` | `23.41M` | `2.83M` |
| `DBLP_data_bfs` | 6 | `610.5` | `1770.0` | `1155.5` | `35.81M` | `36.0%` | `32.9%` | `11.0%` | `20.1%` | `9.94M` | `3.03M` |
| `DBLP_data_new_bfs` | 5 | `436.2` | `1620.5` | `1284.8` | `23.86M` | `82.1%` | `0.6%` | `17.2%` | `0.0%` | `13.22M` | `1.72M` |
| `DBLP_data_new_bfs` | 6 | `665.5` | `725.6` | `438.5` | `39.02M` | `70.8%` | `23.9%` | `5.3%` | `0.0%` | `5.17M` | `1.58M` |
| `Toronto_data_new` | 5 | `261.3` | `3209.9` | `2978.4` | `14.39M` | `24.3%` | `33.0%` | `21.6%` | `21.1%` | `22.41M` | `0.99M` |
| `Toronto_data_new` | 6 | `410.8` | `1502.4` | `1344.0` | `30.05M` | `5.0%` | `35.0%` | `8.8%` | `51.2%` | `11.85M` | `0.75M` |

判断：

- k=5/k=6 的主要时间不只是 join；`search_ms` 中的 Complete 补侧扫描占比很高，尤其 k=5。
- dense-dense join 在 DBLP 类 snapshot 上是重要 pull 来源，但 Toronto 类仍有大量 sparse-sparse / dense-sparse，因此单纯继续换 dense 布局不是普适突破。
- 已实现并保留的 Test18 内部机制是“补侧 Complete row 物化”：对当前 `rem=U-S`，用已保存 half-DP 行在同一 root 上构造 `complete_row[v]=min_x dp[x][v]+dp[rem-x][v]`，再让每个普通 `Complete(u)` O(1) 查询，而不是重复枚举 split。这个机制只利用 Test18 已保存的 rooted DP 行，不是图/询问压缩；正确性上它仍只构造合法完整上界，不能作为剪枝下界。

### 7.2 补侧 Complete row 物化 A/B

这次 A/B 使用同一份 2026-07-09 当前源码，差别只在是否启用补侧 Complete row 物化。回撤版是直接枚举补侧 split；物化版在普通 `cover_rem==rem` 路径上使用 rent/buy 规则懒构建 `complete_row`。三组 40k g12 query 1 的最终权重均一致。

构建口径：`CMakeLists.txt` 现在显式保证 Release/RelWithDebInfo 使用 O2；MSVC Release 为 `/O2 /Ob2`。本轮检查旧 `build/CMakeCache.txt` 和 `gst_test18_main.tlog`，历史 Release 计时本身已经带 `/O2 /Ob2`；下表是 O2 守门写入 CMake 后重新跑的关键 A/B。更早章节中的历史探针时间未在本轮重跑，只作为显式 O2 守门前历史记录保留。

验证命令摘录：

```powershell
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 303033 80 4 10 2 8 .tmp_random_compare_complete_cache_final 0
build\Release\gst_test18_main.exe Toronto result_tmp_complete_cache_final_toronto query.txt data 1 1
build\Release\gst_test18_main.exe DBLP_data_new_bfs result_tmp_complete_cache_final_fast_new_g12 g12 data_snapshot\generated_large 1 1
```

正确性守门：

- 随机对拍 `seed=303033` 和显式 O2 后的 `seed=404041`，均为 80 组 `ALL_OK`；
- Toronto query 1：Test18 `0.2582152999`，与已有 DPBF `0.2582152999` 一致；
- 40k g12 A/B 的权重均一致。

核心 A/B：

| dataset | weight | 直接 Complete wall | row cache wall | speedup | 直接 complement_ms | row cache complement_ms | cache builds | cache queries | cache_ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs` g12 q1 | `17.1766480000` | `176.505s` | `103.110s` | `1.71x` | `102.210s` | `31.563s` | `1202` | `10.328M` | `8.359s` |
| `DBLP_data_new_bfs` g12 q1 | `8.8385900000` | `49.917s` | `38.144s` | `1.31x` | `22.837s` | `11.571s` | `1010` | `1.290M` | `2.452s` |
| `Toronto_data_new` g12 q1 | `14.9921859260` | `135.753s` | `64.341s` | `2.11x` | `93.013s` | `24.062s` | `1311` | `14.447M` | `4.700s` |

注：`complement_cache_ms` 是 `complement_ms` 的子集，不应相加。直接版为回撤补侧 row cache 后的同源码对照 run；启用版为当前保留实现。

解释：

- 这不是减少状态数的机制，而是把同一 `rem` 上重复发生的 Complete 补侧 split 查询改成一次 row 构建加多次查询；
- 对 `DBLP_data_new_bfs`，`complement_scan` 从直接版 `81.59M` 降到 `30.00M`，`lookup_need_skips` 从 `7.04M` 降到 `1.80M`，说明热点确实来自重复 Lookup；
- 对 `Toronto_data_new`，cache queries 达到 `14.447M`，因此即使构建 `586.74M` 次 row-entry 扫描也能换来端到端约 `2.11x`；
- `DBLP_data_bfs` 这种稠密形态仍有 `8.359s` cache 构建开销，但总 wall 仍从 `176.505s` 降到 `103.110s`，说明 rent/buy 触发没有只偏向某个数据形态。

后续判断：这条机制已经足够作为新的 g13 短探针理由，但仍不应立刻无脑跑完整 DBLP g13 q1。先跑带进度的短探针，看 k=5/k=6 的 `complement_ms`、`complement_cache_*` 和 `best` 里程碑是否相对当前中止探针明显改善；只有改善明确或需要最终确认时再做 full run。

## 8. Frontier 同根分区上界

这是 2026-07-09 新增的事件触发候选机制，用来替代已撤出的固定 rows 三分块/四分块探针。它仍然只更新 `best`，不作为下界、不直接剪状态。

机制：

- 当一条 k=3 或 k=4 row `B` 完成并保存后，立刻把 `B` 作为新可用分块；
- 对同一候选 root，求补侧 `U-B` 在 singleton/pair/已 ready triple/已 ready quad 分块下的最小同根分区代价；k=3 新 row 只允许补侧分块大小不超过 3，k=4 新 row 只允许补侧分块大小不超过 4；
- 用 `dp[B][root] + partition_cost[U-B][root]` 构造完整覆盖所有组的候选树；
- 每个分块都是已 ready 的精确 rooted row 或虚拟 singleton；把这些分块同根粘合一定给出合法完整连通子图，求和可能重复计边，因此只会高估，不会低估；
- 触发条件是 DP 事件“新 k=3/k=4 row ready”，不是 `g==13`、固定 rows、数据集名、时间点或高阶行表示特判；
- 2026-07-09 后续维护把分区 DP 从扫描全 `2^g` 改为只扫描补侧 `U-B` 的子掩码；这是等价枚举，不改变机制语义。

这与 5.6 中撤出的探针不同：撤出探针按 rows `96/160/224/256` 增量触发；本机制只在一条新 row 产生时枚举“包含该新 row 的新分区”，因此没有固定进度超参数。

验证：

```powershell
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 707071 80 4 10 2 8 .tmp_random_compare_frontier4_a 0
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 717273 40 13 18 13 13 .tmp_random_compare_frontier4_b 0
build\Release\gst_test18_main.exe Toronto result_tmp_frontier4_toronto query.txt data 1 1
build\Release\gst_test18_main.exe DBLP_data_bfs result_tmp_frontier4_dblp_g9 g9 data_snapshot\generated_large 1 1
```

结果：两组随机对拍均 `ALL_OK`；Toronto query 1 权重 `0.2582152999`；DBLP 40k g9 query 1 权重 `11.8766830000`。

g9/g12 统计：

| dataset | weight | k3/k4 triggers | k3/k4 updates | best after k2 | best after k3 | best after k4 | final best layer | frontier ms | wall vs row-cache O2 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs` g9 q1 | `11.8766830000` | `84 / 70` | `2 / 0` | `14.4864060000` | `11.8777710000` | `11.8766830000` | k4 `11.8766830000` | `4.561ms` | no direct O2 table |
| `DBLP_data_bfs` g12 q1 | `17.1766480000` | `220 / 495` | `2 / 2` | `19.5634940000` | `17.4668070000` | `17.1766480000` | k4 `17.1766480000` | `369.145ms` | `103.110s -> 105.866s` |
| `DBLP_data_new_bfs` g12 q1 | `8.8385900000` | `220 / 495` | `2 / 1` | `9.7733920000` | `9.7316730000` | `8.8385900000` | k4 `8.8385900000` | `415.546ms` | `38.144s -> 35.967s` |
| `Toronto_data_new` g12 q1 | `14.9921859260` | `220 / 495` | `5 / 0` | `16.9052096880` | `15.7317096960` | `15.3033445250` | k5 `14.9921859260` | `256.870ms` | `64.341s -> 66.259s` |

DBLP g13 query 1 短探针：

```powershell
$env:GST_TEST18_PROGRESS='1'
build\Release\gst_test18_main.exe DBLP result_tmp_frontier_partition_g13_probe g13 data 1 1
```

该 run 在 2026-07-09 手动中断于约 `893s`，没有最终 weights 行；只保留实时进度日志里的 k=3 里程碑。预处理阶段先删/压 `747777` 点，Voronoi 阶段处理 `225679` 个内部点，其中 three-portal `38615` 点并添加 `12135` 个 hub；进入 k=3 时为 `elapsed=209.492s`、`best=14.7395`、`finite=119.318M`。该短探针停止在 k=3，尚未测试 k=4 frontier 对 full DBLP g13 的影响。

| milestone | elapsed | best | cumulative masks | implied triple rows | finite | dense rows | peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| first frontier update seen | `246.958s` | `14.6873` | `89` | `11` | `135.512M` | `89` | `3239.38 MiB` |
| early strong update | `283.218s` | `14.5001` | `99` | `21` | `150.206M` | `99` | `3356.88 MiB` |
| old rows-96 value reached | `394.823s` | `14.3326` | `138` | `60` | `207.182M` | `138` | `3791.09 MiB` |
| intermediate update | `529.671s` | `14.2883` | `186` | `108` | `277.619M` | `186` | `4355.09 MiB` |
| near old rows-160 value | `629.075s` | `14.0609` | `210` | `132` | `312.686M` | `210` | `4661.12 MiB` |
| old rows-160 value reached | `662.211s` | `13.9751` | `215` | `137` | `319.692M` | `215` | `4695.84 MiB` |
| old rows-224 value reached | `875.745s` | `13.8176` | `290` | `212` | `425.118M` | `290` | `5577.09 MiB` |
| manual stop | `893.461s` | `13.8176` | `297` | `219` | `434.705M` | `297` | `5659.34 MiB` |

对照 5.6 中撤出的固定 rows 探针：旧探针在 triple rows `96/160/224` 才分别达到 `14.3326/13.9751/13.8176`；事件触发版分别在 implied triple rows `60/137/212` 达到同样 best。这里不把 finite/wall 与旧探针做严格 A/B，因为当前源码还叠加了后续 O2、补侧 row cache 和当前 exact graph reduction 维护；但“无固定 rows 也能更早拿到相同 k=3 上界里程碑”已经成立。

判断：

- 它确实把强上界前移到 k=3；在 `DBLP_data_new_bfs` g12 中，k=4 frontier 又把最终 best 前移到 k=4，并把 finite 从前一版约 `8.347M` 降到 `7.475M`；
- 40k g12 的 wall-time 还不是稳定正收益，但 DBLP g13 短探针显示 k=3 frontier 无固定 rows 地提前复现旧三分块探针的关键 best 里程碑；
- 该机制的 overhead 在这些快照中只有几十到一百毫秒量级，不是瓶颈；
- 下一步可以考虑更长但仍带进度上限的 DBLP g13 探针，观察 k=4 frontier 是否也能改变 full DBLP g13 的 `best`、`finite`、`dense_rows` 和后续 `complement_cache_*`；完整 DBLP g13 q1 仍不作为常规验证项。

## 9. Voronoi / multiway-cut 结构诊断

文献动机：[Dijkstra meets Steiner](https://arxiv.org/abs/1406.0492) 的 future-cost 思路支持当前的 goal-oriented 保存下界；DS* 进一步把可采纳 lower bound 与 Dijkstra-Steiner 结合起来，说明“下界如何进入图搜索”本身需要严格条件。Voronoi / multiway-cut 诊断只用于理解 DBLP 的结构，不再自动推出“继续做图压缩”的实现计划；文献已有、baseline 也可用的压缩必须先证明 Test18 特有或原创价值。因此这里的工具只输出诊断，不改算法语义：

```powershell
build\tools\structure_probe\Release\gst_structure_probe.exe data DBLP g13 1
build\tools\structure_probe\Release\gst_structure_probe.exe data_snapshot\generated_large DBLP_data_bfs g9 1
```

诊断口径：

- 对每个组做多源 Dijkstra，得到 `gd[group][v]`；
- 用最近组给每个顶点做 Voronoi 归属；
- 跨归属边的端点组成一个 multiway-cut 代理边界；
- 同时用 Test18 的 `LowerBound(v,U)` 统计在 root-star 上界下仍可能作为完整 root 的点。

full DBLP g13 query 1：

```text
root_star_upper=17.427423
alive_roots_by_root_star=2,228,369 / 2,497,782 = 89.21%
voronoi_boundary_vertices=1,517,941 = 60.77%
boundary_edges=6,139,713 = 48.02%
tie_vertices=361,558
non_boundary_components=523,529
largest_non_boundary_component=352
component_portals_total=705,300
avg_portals=1.35
component_portal_bins: p0=139,149, p1=223,834, p2=92,310, p3_4=52,157, p5_8=13,200, p9_16=2,191, pgt16=688
removable_leaf_vertices=638,697 = 25.57%   # 原图上单独应用 Voronoi leaf 条件的结构探针估计
metric_torso_vertices_est=1,520,103 = 60.86%
metric_torso_component_edges_est=1,422,520
no_terminal_portal_components_p4=15,073
no_terminal_portal_vertices_p4=33,054
quad_hub_feasible_components=14,781 = 98.06% of p4 components
quad_hub_feasible_vertices=31,085 = 94.04% of p4 vertices
```

40k DBLP snapshot g9 query 1：

```text
alive_roots_by_root_star=40,000 / 40,000 = 100.00%
voronoi_boundary_vertices=36,184 = 90.46%
boundary_edges=415,639 = 63.23%
non_boundary_components=1,956
largest_non_boundary_component=106
component_portals_total=6,053
avg_portals=3.09
removable_leaf_vertices=962 = 2.41%
metric_torso_vertices_est=36,203 = 90.51%
metric_torso_component_edges_est=22,244
no_terminal_portal_components_p4=212
no_terminal_portal_vertices_p4=402
quad_hub_feasible_components=210 = 99.06% of p4 components
quad_hub_feasible_vertices=387 = 96.27% of p4 vertices
```

结论：

- “小 multiway cut / 小 separator”不符合 DBLP 数据；边界本身太大，不能期待用小边界参数直接跑通 g13。
- `full_lb` corridor 也解释了为什么当前下界难以压缩 root 空间：在 full DBLP g13 上，root-star 上界下仍有 `89.21%` 的点存活；在 40k g9 snapshot 上是 `100%`。
- 但去掉边界后，剩余单色内域高度碎片化，full DBLP g13 最大内域组件只有 `352` 点，且 portal 平均数只有 `1.35`。这提示更合适的结构路线不是 separator DP，而是把“只属于一个最近组的内域组件”压缩成边界到该组的 attachment 信息，减少 dense row 扫描的有效图规模。
- 保守的 exact 化入口已经接入 Test18，且现在分成两步：先做标准 Steiner 非终端叶删/度 2 链压缩，再做 Voronoi exact torso。结构探针中 `638,697` 是原图上单独应用 `portal<=1` Voronoi 条件的估计；three-portal g13 探针前段归因为 `747,777` 个标准度缩图顶点加 `220,206` 个后续 Voronoi 内部点，其中二门户 exact 桥边替换贡献 `79,519` 点，三门户 exact hub gadget 贡献 `38,615` 点并添加 `12,135` 个 hub。当前源码又接入了收益门控的四门户 exact torso；在 DBLP snapshot g9 上额外处理 `9` 个组件 / `92` 点，净少 `47` 个图顶点。
- p4 结构探针显示，quad hub 可行性本身不是瓶颈：full DBLP g13 原图 p4 无终端组件中 `98.06%` 可行，覆盖 `94.04%` 的 p4 内部点；g9 snapshot 中 `99.06%` 可行，覆盖 `96.27%` 的 p4 内部点。真正需要门控的是表示收益：很多 p4 组件很小，不加 `component_size > 5` 会因为新增 hub 多于删除内部点而把图变大。
- 更进一步的普通 local torso / mimicking 压缩暂不作为主线：即便能保持 portal 子集连接代价，它仍属于图/询问本身的通用压缩，baseline 也可以使用。后续只有在能给出 Test18 特有的利用方式，或提出不属于已有通用压缩套路的原创机制时，才重新打开这个方向。

## 10. DS* / zero-star Steiner 视角

文献线索：

- [Dijkstra meets Steiner](https://arxiv.org/abs/1406.0492) 给出了 Dijkstra-Steiner 的目标导向 future-cost 版本。对 Test18 的直接启发是：`dp[S][v] + h(v,U-S) > best` 这类条件本质上是 goal-oriented DP，而不是普通局部剪枝。
- [DS*](https://arxiv.org/abs/2011.04593) 进一步把 A* 思路接入 Dijkstra-Steiner，允许使用可采纳 lower bound 来引导搜索；这说明“下界能否进入图搜索”是算法语义问题，不只是把一个公式放进 `TrySet`。
- 对组斯坦纳树，可以把每个 group 加一个新的超级终端 `t_i`，并用零边连接到该 group 的所有原顶点。要求连通所有 `t_i` 后，就得到等价的普通 Steiner Tree 实例。Test18 当前的 `gd[group][v]` 和虚拟 singleton 可以理解为没有显式建出这些零边/超级终端的 Dijkstra-Steiner 实现。

这条视角给出的边界很重要：

- 当前热路径中，`Far/Lb` 被用于 `TrySet`、seed、pop 和 relax 前的剪枝；这要求下界随 root 沿一条边移动时下降不超过边长，也就是实际需要 1-Lipschitz / consistent 风格。否则从 `u` 放松到 `v` 时，`d(u)+w+h(v)` 可能突然低于 `best`，而 `d(u)+h(u)` 已经被提前剪掉。
- 之前的 capacity-aware split 正是反例：它看起来比 `far+near` 强，但既不能作为当前 root 的真实必要下界，也不满足当前 Dijkstra relax 所需的一致性；随机对拍已经给出错误结果。因此不能再把“强一些的公式”直接塞进图搜索。
- 保存/compact 阶段的下界门槛更宽：状态 `dp[S][v]` 已经算完，只判断未来是否还可能组成更优完整解；这里不要求新下界满足 1-Lipschitz，只要求它是从 `(v,U-S)` 出发补完剩余组的真实可采纳下界。这也是当前 future split 只放在保存/compact 点的原因。

候选突破不应是另一个小 LB，而是两步走：

1. 先把 Test18 的单层搜索改造成 DS* 语义的可选实验路径，或者至少做一个不改变结果的 DS* readiness 诊断。目标是允许“可采纳但不一致”的强下界参与搜索顺序，而不是继续要求所有下界都能安全放进普通 Dijkstra relax。
2. 再寻找真正强的可采纳 `h(v,R)`。优先方向不是图/询问压缩，而是 zero-star Steiner 实例上的 cut/dual lower bound：例如 rooted cut relaxation 或 dual moat/packing 形式。它们若成立，baseline 当然也可在理论上使用，但 Test18 的优势在于 `h(v,R)` 可直接嵌入 half-DP 的状态保存、Complete 上界和 DS* 搜索次序；这需要写成算法性质，而不是数据集特判。

进入代码前的验收门槛：

- 任意新 `h(v,R)` 先在随机小图上用 exact future oracle 验证可采纳：把 root `v` 当作额外 singleton group，精确求连通 `{v}` 与 `R` 中每个 group 的最优 GST，必须满足 `h(v,R) <= opt_future(v,R)+1e-6`。
- 若只放保存/compact，随机对拍和 Toronto/DBLP snapshot g9 必须通过；若进入图搜索，必须先给出 DS* 或 consistent 证明，不能再复用普通 Dijkstra 的剪枝位置。
- full DBLP g13 q1 仍不作为常规验证。只有在 g12 / 有上限 g13 短探针里看到 `finite_states`、`dense_rows` 或 `search_ms` 的关键改善，才值得长跑确认最终 weights。

当前结论：DS* 是比继续堆 `LowerBound` 常数更可能带来数量级变化的理论入口，但还没有可直接启用的新下界。下一步应先做 exact future oracle + 候选下界诊断，而不是马上改热路径。

### 10.1 exact future oracle 初筛

2026-07-10 新增 `tools/future_lb_probe`，只做小图诊断，不改 Test18 语义、不写 result 目录。它对随机小图采样 `(root v, remaining mask R)`，把 `v` 当作一个额外 singleton group，调用现有 DPBF 精确求解 future GST：

```text
opt_future(v,R) = OPT connected tree covering {v} and one vertex from every group in R
```

然后比较若干候选下界是否满足 `h(v,R) <= opt_future(v,R)+1e-6`，并额外检查边一致性：

```text
h(u,R) <= w(u,v) + h(v,R)
```

前者决定候选能否作为 future lower bound；后者决定候选能否直接放进当前普通 Dijkstra relax。可采纳但不一致的候选只能走保存/compact，或等待 DS* 语义接入。

当前保留的候选包括：

- `current_lb`：Test18 热路径里的 `LowerBound(v,R)`；
- `rooted_metric_mst_half`：zero-star metric closure 上 root+groups 的 MST / 2；
- `group_mst_half`：只看 group metric MST / 2；
- `far_plus_near_negative_control`：故意放入的负对照，用来确认 oracle 能抓住不安全公式。

构建与运行：

```powershell
cmake --build build --config Release --target gst_future_lb_probe
build\tools\future_lb_probe\Release\gst_future_lb_probe.exe 909091 40 4 9 2 6 32
build\tools\future_lb_probe\Release\gst_future_lb_probe.exe 919293 20 4 8 2 6 0
build\tools\future_lb_probe\Release\gst_future_lb_probe.exe 949596 50 6 11 5 7 48
```

结果摘要：

| run | states | positive-ratio states | `current_lb` admissibility / consistency violations | `current_lb` avg ratio | `current_lb` avg gap | negative-control admissibility / consistency violations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| seed `909091`, sampled | `1,280` | `1,072` | `0 / 0` | `0.978449` | `0.570429` | `546 / 2628` |
| seed `919293`, exhaustive small | `2,849` | `2,514` | `0 / 0` | `0.922509` | `2.819014` | `909 / 1364` |
| seed `949596`, sampled g5--g7 | `2,400` | `2,248` | `0 / 0` | `0.909994` | `2.388123` | `802 / 14582` |

附带观察：

- `rooted_metric_mst_half` 和 `group_mst_half` 都没有可采纳性违规，但明显弱于当前 `LowerBound`；三组平均 ratio 分别约为 `0.268--0.298` 和 `0.090--0.147`。其中 `rooted_metric_mst_half` 在 g5--g7 采样里出现 `4` 次 consistency 违规，说明“可采纳”不等于“可以直接进当前 Dijkstra relax”。
- `far_plus_near_negative_control` 在三组里分别有 `546/909/802` 次可采纳性违规，并有 `2628/1364/14582` 次 consistency 违规，最大超出 oracle `44/59/48`，说明 capacity/far+near 这类公式若没有额外语义约束，确实不能作为普通 future 下界，更不能直接进入 Dijkstra relax。
- `current_lb` 在这些小图 future oracle 上已经相当接近精确 future optimum，平均 ratio 约 `0.91--0.98`。因此继续在同一类 metric MST / nearest-distance 下界里做小增强，预期很难带来数量级状态下降；更合理的下一步是寻找不同性质的可采纳下界，或先实现 DS* readiness 诊断，确认非一致但可采纳的下界如何进入搜索。

### 10.2 A* ordering 短探针

既然 `future_lb_probe` 里 `current_lb` 同时满足可采纳和 edge consistency，可以把单层图搜索的堆键从 `d` 改成 `d+Lb(v)`，并在 relax 时顺手用同一个 `Lb` 做安全剪枝：

```text
push key = d[v] + LowerBound(v,R)
if nd + LowerBound(to,R) > best: skip relax push
```

当前代码用环境变量开启：

```powershell
$env:GST_TEST18_ASTAR_ORDER='1'
```

默认路径仍是原来的 Dijkstra 排队；A* ordering 只是实验路径。它不改变 DP 状态语义，不使用数据集、组数、层数或时刻特判；理论上依赖的是 10.1 里验证过的 `current_lb` consistency。复杂度口径不变：仍是同一批 `(mask,v)` 的 priority_queue 图搜索，只是堆键换成一致启发式，且 `Lb` 仍按当前 mask 懒缓存。

正确性守门：

```powershell
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 555661 80 4 10 2 8 .tmp_random_compare_astar_default 0
$env:GST_TEST18_ASTAR_ORDER='1'
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 555662 80 4 10 2 8 .tmp_random_compare_astar_on 0
```

两组随机对拍均 `ALL_OK`。Toronto query 1 权重一致：默认 `0.2582152999`，A* ordering `0.2582152999`；wall `0.0929s -> 0.0963s`，没有实际收益。

DBLP/Toronto snapshot 短 A/B：

| dataset | weight | wall | `pq_pop` | `relax_ok` | `astar_relax_lb_pruned` | `search_ms` | `complement_ms` | `finite/live` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs` g9 default | `11.8766830000` | `4.290s` | `8.037M` | `7.001M` | `0` | `2552.414` | n/a | `1.697M / 0.349M` |
| `DBLP_data_bfs` g9 A* | `11.8766830000` | `3.971s` | `3.779M` | `2.743M` | `22.390M` | `2228.644` | n/a | `1.697M / 0.349M` |
| `DBLP_data_new_bfs` g12 default | `8.8385900000` | `33.979s` | `42.021M` | `38.191M` | `0` | `24558.279` | `9994.274` | `7.475M / 5.244M` |
| `DBLP_data_new_bfs` g12 A* | `8.8385900000` | `32.261s` | `18.254M` | `14.424M` | `102.272M` | `22901.356` | `10030.463` | `7.475M / 5.244M` |
| `DBLP_data_bfs` g12 default | `17.1766480000` | `102.721s` | `115.591M` | `101.383M` | `0` | `69889.802` | `30713.216` | `29.315M / 26.946M` |
| `DBLP_data_bfs` g12 A* | `17.1766480000` | `102.658s` | `59.499M` | `45.291M` | `386.958M` | `69777.738` | `31032.974` | `29.315M / 26.946M` |
| `Toronto_data_new` g12 default | `14.9921859260` | `62.049s` | `66.059M` | `36.214M` | `0` | `40650.237` | `22917.988` | `29.168M / 25.834M` |
| `Toronto_data_new` g12 A* | `14.9921859260` | `64.131s` | `68.875M` | `39.030M` | `39` | `42299.746` | `23691.817` | `29.168M / 25.834M` |

判断：

- 这是搜索顺序/relax push 收益，不是状态保存收益；`finite/live` 不变。
- `DBLP_data_new_bfs` g12 和 `DBLP_data_bfs` g9 上，A* ordering 明显减少 `pq_pop` 和成功 relax，wall 有 `5--7%` 收益。
- `DBLP_data_bfs` g12 上，heap 事件也明显下降，但 `search_ms` 和 wall 基本不动，说明稠密行扫描/Complete 热点已经盖过了 heap-pop 收益。
- `Toronto_data_new` g12 反向：`astar_relax_lb_pruned` 只有 `39`，反而增加 `pq_pop/relax_ok/search_ms`，wall 变慢约 `3.4%`。
- 因此当前判断是：A* ordering 是有理论依据的 DS* / consistent-LB 实验入口，但还不是默认主线优化。它可以作为后续 DBLP g13 有上限短探针的候选开关之一，但不应仅凭这些混合 snapshot 结果默认启用，更不值得单独触发 full DBLP g13 q1。
