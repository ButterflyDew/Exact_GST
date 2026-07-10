# Test18 handoff：当前主线、正确性与后续入口

> 归档状态说明：本文冻结的是撤回前历史口径。正文中把 frontier k=3/k=4 写作“当前候选”的位置已经过期；该机制后来因硬编码层级不符合 `agent.md` 第六条，从 Test18/Test19 源码和统计字段完整撤出。当前状态以 `readme_files/test18_algorithm.md` 为准。

这份文件给无记忆 agent 快速接手 Test18。当前实测效果和 DBLP g13 结构探针保存在 `readme_files/test18_effect_report.md`；完整历史实验记录保存在 `readme_files/test18_research_log.md`；本文件前半部分先说明当前实现、正确性边界、验证状态与下一步优先级，后半部分保留较详细的原始说明。

## 0. 当前状态一句话

Test18 是当前大组数组斯坦纳树实验主线：它保留精确 half-DP 框架，只用安全下界剪枝、只用合法完整解更新上界；当前真正有效的增量是 tree-aware greedy、虚拟 singleton、full-root lower-bound corridor、stale `need` compact、dense-light 行表示、best 下降后的 light/dense-light 行 compact、正确 DP 顺序下的最大层保存条件与 future split 保存下界、cover-aware Complete、补侧 Complete row 物化、k=2 后的 pair/single 同根分区上界、事件触发的 frontier k=3/k=4 同根分区上界候选，以及查询级 exact graph reductions。

DBLP g15 仍未跑通。现有 best 已能在 pair 层完成后从 `19.3812` 降到 `17.7702`，但 k=3 行仍快速变稠密；2026-07-09 新增的补侧 Complete row 物化在 40k g12 快照上显著降低 k=4/k=5/k=6 Complete 查询成本，下一步应先用它获取新的 DBLP g13 证据，再继续寻找更本质的状态保存必要条件，而不是继续堆小幅 LB。

## 1. 当前代码入口

- 方法入口：`methods/Test/test18.cpp`
- 统计结构：`methods/Test/test18.h`
- 主程序输出：`main.cpp` 的 `Test18` 分支
- 快照/随机验证工具：
  - `tools/random_compare`
  - `tools/snapshot_benchmark`

核心数据结构 `State` 有三类行：

1. sparse：保存 `v,d,cover,need`；
2. sparse-light：保存 `v,d`，当前只用于 pair 行这种由 DP 结构决定不持久化 cover/need 的非 dense 轻量行；
3. dense-light：保存精确 `dp[S][v]`，当前使用 full dense `double[n]`。

packed dense（`bitset + prefix + values`）已经实现并 A/B 过。它是正确的纯布局替换，但 DBLP snapshot g9 中只让 `pull_scan` 小幅下降，wall time 与 peak RSS 反而略差，因此已从代码撤回，只保留实验记录。

## 2. 必须遵守的安全原则

1. 不恢复非 exact 的 h 剪枝。
2. 不把 `dp` 上界当作下界。
3. 任何会剪状态的条件必须来自安全下界。
4. 任何会降低 `best` 的机制必须显式构造合法完整解。
5. 布局优化只能改变访问方式，不能改变保存的精确 `dp[S][v]` 映射。

## 3. 当前保留机制速览

| 机制 | 作用 | 正确性理由 | DBLP g15 观察 |
|---|---|---|---|
| 虚拟 singleton | 不保存 `{a}` 行，直接用 `gd[a][v]` | singleton rooted DP 恒等于组到点最短路 | 减少早期存储和 pull 扫描 |
| tree-aware greedy | 降低初始 `best` | 只构造合法完整树 | `19.4822 -> 19.3812`，但 pair 状态几乎不降 |
| `full_lb[v]` | 排除不可能作为完整 root 的点 | 完整 rooted 解下界 | 对 DBLP pair 爆炸帮助有限 |
| `need[S,v]` + compact | 删除已不可能参与更优完整解的稀疏状态 | `dp[S,v]+LB(v,U-S)>best` 后随 best 单调不再有效 | 中等图有效，大图有限 |
| dense-light | 达到成本阈值的稠密行少存 `cover/need` | 仍保存精确距离；丢 cover 只少做 upper update | pair 与稠密高阶行空间常数明显下降 |
| light/dense-light compact | best 下降后对轻量行现场重算 `dp+LB`，删除失效 root；dense 行将失效 root 置为 `INF` | 使用同一个安全 `LB(v,U-S)`，只删除失效状态，不改变 DP 值 | 单独在 fast snapshot A/B 中让 live `(mask,v)` 下降 `5.33%`，`pull_hits/tryset_calls` 下降约 `10.3%` |
| max-layer order prune | `|S|=H` 行若按当前 mask 顺序不会再被后续同层 Complete 读取，则当前行 Complete 后整行不保存 | 最大层不能再参与 join；没有未来 Complete 读者时，保存行对后续 DP 无影响 | fast snapshot 额外跳过 `1.208M` 个保存状态 |
| odd max-layer forced LB | 奇数 `g` 的最大层行未来只能由一个同层 H-mask 加一个 singleton 补齐，保存前用 `min_b LB(v,R-b)+gd[b][v]` | 枚举的是后续 Complete 唯一合法块形，且每块都是安全下界 | fast snapshot 额外剪掉 `0.236M` 个保存状态 |
| future split save LB | `|S|<H` 行在保存后未来至少还要加入两个非空补侧块，保存/compact 使用 `max(LB(v,R), far_R(v)+near_R(v))` | 当前行自己的 Complete 已执行；之后任意 future join/Complete 的算法代价都至少分别触达补集最远组和最近组 | fast snapshot 额外剪掉 `1.668M` 个保存状态，总 live `(mask,v)` 下降 `28.66%` |
| cover-aware Complete | 用 actual cover 更早拼合法完整解 | 只更新上界，不剪状态 | Toronto 有效，DBLP pair 层 cover 额外很少 |
| 补侧 Complete row 物化 | 对固定 `rem=U-S`，把 `min_x dp[x][v]+dp[rem-x][v]` 懒物化成一条补侧 row，后续普通 `Complete(u)` O(1) 查询 | 只重排同一批已保存 rooted row 的补侧上界计算；触发采用 rent/buy 成本比较，无数据集或层数特判 | 40k g12 三个快照权重一致；显式 Release/O2 后重跑，wall time 分别约 `176.50s -> 103.11s`、`49.92s -> 38.14s`、`135.75s -> 64.34s` |
| pair/single root partition | k=2 后用 singleton/pair 分区构造完整同根上界 | 求和可能高估，不会低估 | full DBLP g15 进入 k=3 前 `19.3812 -> 17.7702` |
| frontier k=3/k=4 root partition | 每条 k=3 或 k=4 row 完成后，用该新 row 加补侧已 ready 且大小不超过该 row 的分块构造完整同根上界 | 触发来自 row ready 事件；每个分块都是精确 rooted row 或虚拟 singleton，求和只会高估 | 40k g12 均权重一致，k=3/k=4 可前移 best；O2 后 wall 暂为混合结果，作为 g13 短探针候选 |
| Standard Steiner degree reduction | 查询级删除非 terminal 叶子/无 terminal 分量，并压缩非 terminal 度 2 链 | 正权重 Steiner 树不需要非终端死枝；度 2 Steiner 链可用等长边替换 | full DBLP g13 当前代码先删/压 `747,777` 点 |
| Voronoi exact torso | 查询级处理无 query terminal 的单色内域组件：`portal<=1` 删除，`portal==2` 用两门户最短路桥边替换，`portal==3` 用 pair 边 + 三终端 hub gadget 替换，收益为正且局部表可行时 `portal==4` 用 6 条 pair 边 + 4 个 triple hub + 1 个 quad hub 替换 | 每一步都保留对应 portal 子集的局部最优连接代价；四门户额外要求删点数大于新增 hub 数，且 quad hub 不让任何二/三门户子集变便宜 | DBLP snapshot g9 在度缩图后 Voronoi 阶段缩掉 `1,270` 个内部点，四门户额外处理 `9` 个组件 / `92` 点，最终图 `37,384` 点 |

## 4. 已证伪或不保留的方向

这些方向不要无意识重做；通用历史数据见 `test18_research_log.md`，g13 专项失败实验见 `test18_effect_report.md`。

- 通用 k=3 singleton/pair/triple root partition 的旧启用方式：Toronto g12 额外约 `428ms` 且不更新 best，因此旧版不保留。2026-07-09 又短暂测试过只在完整 k=3 layer-end 触发的无 rows 版本：随机对拍 `seed=202020` 80 组通过，Toronto query 1、DBLP snapshot g9 query 1 权重正确；40k DBLP g9 没有新增更新，fast g9 五数据集里唯一出现的 `layer_partition_updates=1` 与旧主线已有 `best_after_k3=8.734936` 相同，没有带来状态收益，因此该 layer-end 版本已撤回。
- g13 k=3 固定 rows 增量 singleton/pair/triple root partition：rows `96/160/224/256` 探针能把 `best` 从 `14.7395` 降到 `13.3535`，但固定 rows 触发没有理论依据，违背 `agent.md` 第六条，已从当前代码撤出；只在效果报告中保留观察。当前代码中新的 `frontier_partition_*` 不是这一路径：它只在具体 k=3/k=4 row ready 的 DP 事件上检查包含该新 row 的分区。
- 非 dense 高阶行轻量表示探针：曾用硬触发来观察高阶 `cover/need` 常数瓶颈；该触发来自数据集、组数、层级和表示阈值外的组合特判，已从代码和当前机制文档撤出。非 dense 高阶行必须保持普通 sparse。
- g13 k=4 singleton/pair/triple/quad root partition：正确，但 full DBLP g13 rows `16/32/64` 都没有继续降低 `best=13.3535`，因此撤回。
- 删除 `far_cache/far_seen`：省 `O(n)` 工作区但 DBLP snapshot g9 从约 `4.11s` 变慢到 `4.53s`。
- 合并 Far/LB cache：Far-pruned 候选太多，合并会为它们多付完整 LB 常数。
- root+groups MST LB：能剪少量状态，但时间收益为负。
- 三点 MST LB：安全但状态几乎不降，时间变慢。
- pair 层局部支配源过滤：Toronto 有收益，DBLP 不减少最终 finite，且更慢。
- pair actual-cover 升格/压缩：DBLP pair 额外 cover 太少。
- actual-cover-aware 同根合并：理论上安全，但当前只对非 light 行有持久 cover；DBLP snapshot g9 中 `714,348` 次检查只产生 `459` 次命中和 `1` 次实际候选改善，wall time 变慢，因此不保留。
- pair 图扩展 dominated skip：随机对拍已找到反例，不正确。
- capacity-far 多块容量下界直接进 Dijkstra 剪枝：随机对拍 `seed=202020` 第 33 例反例。原因是该下界不是 1-Lipschitz，根沿边移动一次时多个块距离之和可能下降超过边长，不能在当前 mask 的图搜索前/中剪状态。
- compact-only 容量顺序统计下界：只放在 light/dense-light compact 中也不安全；随机对拍 `seed=424244` 第 6 例反例，DPBF 为 `33`，该版本为 `35`。未来 join 后的图搜索会共享移动路径，不能把多个补侧块从当前 root 出发的距离简单相加当作必要下界。
- row lifetime 主动 clear：最终 live 变 0 是算法结束释放造成的统计假象；peak resident 没有改善，wall time 反而慢于只做保存剪枝的组合版。
- 多种额外 greedy/KMB/root-path 上界：合法但不改善关键 best。

## 5. 当前验证状态

完整 A/B 表、DBLP snapshot g9 补充探针、full DBLP g13 query 1 结构探针，以及本轮未保留的 g13 方向，统一维护在 `test18_effect_report.md`。本节只保留算法文档需要知道的摘要。

最近已验证的稳定主线：

- 随机小图对拍：`seed=202020`、`seed=515151`、`seed=111222`，补侧 row cache 接入后的 `seed=303033` 和显式 O2 后 `seed=404041` 80 组通过；frontier k=3/k=4 接入后 `seed=707071` 80 组与固定 g13 小图 `seed=717273` 40 组通过；
- Toronto 默认 query 1：Test18 `0.2582152999`，已有 DPBF `0.2582152999`；
- DBLP snapshot large / `DBLP_data_bfs` g9 query 1：权重 `11.8766830000`，缩减 `40000/657322 -> 37384/652317`，`degree_reduce_removed_vertices=1510`，Voronoi 阶段删除内部点 `1270`，其中二门户 `478` 点、三门户 `349` 点并添加 `119` 个 hub、四门户 `92` 点并添加 `45` 个 hub；`finite_states=1.700718M`，`live_states=0.349174M`；
- 补侧 Complete row 物化 A/B：40k g12 query 1 上 `DBLP_data_bfs` 权重 `17.1766480000`，wall `176.505s -> 103.110s`；`DBLP_data_new_bfs` 权重 `8.8385900000`，wall `49.917s -> 38.144s`；`Toronto_data_new` 权重 `14.9921859260`，wall `135.753s -> 64.341s`；这些关键时间已在显式 Release/O2 守门写入 CMake 后重跑，历史探针时间未在本轮重跑，只作为显式 O2 守门前历史记录；
- frontier k=3/k=4 同根分区上界：40k g12 query 1 三组均权重一致，k=3/k=4 都可前移 best；wall 对 row-cache O2 表为 `103.110s -> 105.866s`、`38.144s -> 35.967s`、`64.341s -> 66.259s`，暂未证明端到端稳定提速。DBLP g13 query 1 短探针手动停止于约 `893s`，无最终 weights 行；该短探针停在 k=3，但已无固定 rows 地在 implied triple rows `60/137/212` 分别达到旧固定 rows 探针在 `96/160/224` 才达到的 `14.3326/13.9751/13.8176`；
- fast snapshot 20 条 A/B：live `(mask,v)` 总体下降 `28.66%`，已达到多数据集平均 `20%` 目标；其中 `DBLP_data_bfs` 仍只有 `9.58%`；
- full DBLP g13 query 1：上一完整 two-portal exact torso 主线仍是当前合规完整基准，最终权重 `12.5936282853`，耗时 `20193.624s`，peak RSS `25185.023 MiB`。three-portal exact torso 的撤回前二进制探针显示，Voronoi 阶段删除内部点从 `181591` 增至 `220206`，其中三门户 `38615` 点、添加 `12135` 个 hub，最终 DP 图顶点数为 `1,541,934`；pair 层 finite 从 `121.365M` 降到 `119.455M`。该探针最终同样得到权重 `12.5936282853`，但高层性能数字不能写作当前源码成绩；当前源码已重建，并通过 `seed=202020` 80 组、`seed=424244` 40 组随机对拍、Toronto query 1、DBLP snapshot g9 query 1 验证。2026-07-09 当前源码 `portal==4` 收益门控版做过一次中止探针，不含任何 `g==13`、固定 rows 或非 dense 高阶 light 特判；该 run 在 k=5 masks `1316` 到达 `best=12.9949`，masks `1625` 停止，只有 partial 证据，不能写作完整成绩；
- Test18 输出已删除长期为 0 的 `h_*`、`certify_*`、`confirmed_states`、`valid_total/valid_k*`，以及撤回探针对应的旧统计字段。

当前瘦身后仍需在每次修改后优先运行：

构建口径：`CMakeLists.txt` 显式保证 Release/RelWithDebInfo 使用 O2；MSVC Release 为 `/O2 /Ob2`。计时对比默认使用下面的 Release 构建。

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target gst_test18_main gst_dpbf_main gst_random_compare
build\Release\gst_random_compare.exe build\Release\gst_test18_main.exe build\Release\gst_dpbf_main.exe Test18 202020 80 4 10 2 8 .tmp_random_compare_test18 0
build\Release\gst_test18_main.exe Toronto result_tmp_test18_toronto query.txt data 1 1
build\Release\gst_test18_main.exe DBLP_data_bfs result_tmp_test18_dblp_g9 g9 data_snapshot\generated_large 1 1
build\Release\gst_test18_main.exe DBLP_data_new_bfs result_tmp_test18_cache_g12 g12 data_snapshot\generated_large 1 1
```

长跑若需要 stderr 进度日志，显式设置 `GST_TEST18_PROGRESS=1`；代码不再按 `g`、图规模或数据集形状自动开启进度输出。

packed dense 的 A/B 结论：

- 随机对拍 `seed=101010` 80 组通过；
- Toronto 默认 query 1 权重正确；
- DBLP snapshot g9：`packed_dense_rows=13`，`packed_dense_states=501,376`，`pull_scan` 从旧主线约 `27.35M` 降到 `26.88M`，但 wall time 约 `4.24s -> 4.49s`，peak RSS 也没有下降；
- 因此撤回 packed dense，不进入主线。

## 6. 下一步建议

优先级从高到低：

1. 补侧 Complete row 物化已经在三个 40k g12 快照上显示大幅收益；frontier k=3/k=4 上界已经在 DBLP g13 短探针中无固定 rows 地提前复现旧三分块探针的 k=3 best 里程碑。下一步如果继续长一点，也应保持带进度上限，观察 k=4/k=5 的 `finite`、`dense_rows`、`complement_ms` 和 `complement_cache_*` 是否同步改善。
2. full DBLP g13 query 1 不再作为常规验证项；只有更长短探针确认 k=4/k=5 关键路径明显改善、需要最终权重确认，或必须获取不可替代的关键里程碑时才长跑。常规验证优先用随机对拍、Toronto、DBLP snapshot g9/g12 和结构探针。
3. 继续寻找“减少 dense 行保存范围”的必要条件，而不是只换 dense 存储布局；补侧 row cache 解决的是重复 Complete 查询，不解决状态数本身。
4. 新的理论入口是 DS* / zero-star Steiner 视角：把每个 group 接一个零边超级终端后，GST 可视为普通 Steiner Tree；这解释了为什么 future-cost 可以进入 half-DP，也说明非一致下界不能直接塞进当前 Dijkstra relax。2026-07-10 新增 `tools/future_lb_probe`，用 DPBF exact future oracle 筛查 `h(v,R)`，并检查边一致性 `h(u,R)<=w(u,v)+h(v,R)`；三组随机/穷举小图中当前 `LowerBound(v,R)` 可采纳性和一致性均零违规，平均达到 exact future optimum 的 `0.91--0.98`，而负对照 `far+near` 大量违规。`rooted_metric_mst_half` 虽然可采纳但已有 consistency 违规，说明“可采纳”与“能进普通 Dijkstra relax”必须分开处理。
5. 基于上一条一致性证据，当前代码提供可选 A* ordering 实验路径：设置 `GST_TEST18_ASTAR_ORDER=1` 后，单层图搜索用 `d+LowerBound(v,R)` 入堆，并在 relax 时用同一个 LB 做安全跳过。随机对拍默认/开启各 80 组通过；DBLP snapshot g9 与 `DBLP_data_new_bfs` g12 权重一致，`pq_pop` 和 `relax_ok` 大幅下降，wall 分别约 `4.290s -> 3.971s`、`33.979s -> 32.261s`。但补充 A/B 后结论是混合的：`DBLP_data_bfs` g12 权重一致且 heap 事件下降但 wall 基本不动，`Toronto_data_new` g12 权重一致但 wall 约 `62.049s -> 64.131s` 变慢。因此它暂是有理论依据的候选路径，还不是默认主线成绩；不要单独为它触发 full DBLP g13 q1。
6. 图/询问本身的进一步压缩不再作为优先方向；文献已有、baseline 也能使用的压缩只在能证明为 Test18 特有或具有原创性时再考虑。当前代码已经接入的标准度缩图、`portal<=1/2/3` exact torso 与收益门控 `portal==4` exact torso 保持为既有主线；不要继续沿普通 `portal>=5` mimicking/torso 压缩做工程扩展。
7. 不建议继续小幅增强 LB：pair slack 诊断显示 DBLP pair 状态离 best 门槛普遍很远，小幅 LB 很难产生数量级状态下降；若只是一个更强但未证明可采纳/一致的公式，先放诊断，不进剪枝。

---

# Test18 full algorithm notes：状态削减与 cover-aware 上界更新

Test18 是当前继续探索大组数的主线版本。它继承 Test17 的 Complete 优化，并进一步减少实际保存和扫描的 `(mask,v)` 状态数。

Test18 仍遵守 Test16 后的核心安全原则：

```text
不恢复非 exact h
不把 dp 上界当作下界
只使用安全下界或合法完整上界影响搜索
```

## 1. 与 Test17 的差异

Test18 主要增加十一个机制：

1. 虚拟 singleton；
2. 全根 `full_lb` corridor；
3. `need[S,v]` stale 状态识别与 compact；
4. 稠密行自适应轻量表示，pair 行是默认 light 的特例；
5. cover-aware Complete，提前更新合法完整上界；
6. 补侧 Complete row 懒物化，把同一个 `rem` 的补侧 split 结果复用到多个 popped root；
7. pair 层完成后的同根 pair/single 分区上界，尝试在进入 k=3 前降低 `best`；
8. 事件触发的 frontier k=3/k=4 同根分区上界，尝试在对应 row ready 时继续降低 `best`；
9. 正确 DP 顺序下的最大层保存条件、奇数最大层 forced-complete 保存下界与 future split 保存下界；
10. 查询级标准 Steiner 度缩图：删除非 query terminal 叶子/无 terminal 分量，压缩非 query terminal 度 2 链；
11. 查询级 Voronoi exact torso：在 DP 前处理无 query terminal 的单色内域组件；`portal<=1` 删除，`portal==2` 替换为组件内最短路桥边，`portal==3` 替换为保留二端与三端连接代价的 hub gadget，`portal==4` 在删点数大于新增 hub 数且局部表可行时替换为 pair/triple/quad gadget。

当前实现还保留一个无参数初始上界增强：先从 root-star 最优根运行 greedy，再把这次 greedy 命中的组顶点作为候选根各运行一次 greedy。所有结果都只是合法完整上界，取最小值更新 `best`。

此外，Test18 不再预处理半掩码 Far 表；Far 改为当前 mask 内的 lazy cache。

## 1.4 Standard Steiner degree reduction

Test18 在每条 query 进入组距离预处理前，先把所有 query group 候选点标记为 protected terminal。随后只对非 protected 顶点做三种等价缩图：

- 非 terminal 度 `0/1` 的叶子迭代删除；
- 删除整个不含 query terminal 的连通分量；
- 非 terminal 度 `2` 的 Steiner 链压缩为一条等长边。

正确性来自正权重 Steiner 树的基本结构。非 terminal 叶子若出现在连接所有组的子图中，只能作为死枝，删去后不会破坏任何 terminal 间连通性且不会增大代价；不含 query terminal 的连通分量不可能贡献覆盖；非 terminal 度 2 链若被最优树使用，只承担两个端点之间的一段路径，用等长边替换保持所有 terminal 连接代价。若某条链形成自环或只回到同一 core，正权重下也不会改善连接，当前实现直接丢弃自环。

这一步在 `ComputeGroupDistances` 之前执行，因此后续 `gd`、greedy、Voronoi exact torso 和 half-DP 都只看缩减图。输出中 `degree_reduce_removed_vertices/edges` 记录总缩减量，`degree_reduce_leaf_vertices`、`degree_reduce_dead_vertices`、`degree_reduce_contracted_vertices` 分别记录三类来源，`degree_reduce_ms` 记录耗时。

## 1.5 Voronoi exact torso

Test18 在每条 query 进入 half-DP 前，先对每个组做多源 Dijkstra，得到 `gd[group][v]`，再按最近组给顶点做 Voronoi 归属。跨归属边的端点是边界点；去掉这些边界点后，每个剩余连通块只属于一个最近组。

当前处理满足这些条件的块：

- 块内没有当前 query 的 terminal；
- 块到边界的不同 portal 数为 `0/1/2/3/4`。

`portal<=1` 的正确性来自正权重连通子图的枝叶性质：这样的块若没有 terminal，且最多只通过一个 portal 连接外部，那么任何覆盖所有组的最优连接子图都不需要进入它。进入后只能从同一个 portal 返回，非负边权不会让这种往返变得更优。

`portal==2` 时，块内没有 terminal；任何使用该块的最优子图只可能承担两个 portal 之间的一段连接。因此用块内两 portal 的最短路桥边替换，不会增加任意可行解的代价，也不会产生比原图更便宜的二端连接。

`portal==3` 时，外部只会关心该块连通了三个 portal 中的哪个子集。大小为 `0/1` 的子集代价为 0；大小为 `2` 的子集用组件内两端最短路精确保留；大小为 `3` 的子集代价为：

```text
min_x dist(x,p0) + dist(x,p1) + dist(x,p2)
```

其中距离都限制在该组件和三个 portal 诱导的局部图内，并排除原本就留在外部的 portal-portal 直接边。三终端 Steiner 树一定有一个分叉点 `x`，反过来从最优 `x` 到三个 portal 的最短路并起来也是合法连接，因此上式正好等于局部三端 Steiner 代价。实现中额外加入三条 pair 边和一个 hub：hub 的三条边权相加等于三端代价，任意两条 hub 边之和不小于对应 pair 最短路，所以它不会制造更便宜的二端连接。

`portal==4` 时，Test18 先在“组件 + 四个 portal”的局部图上跑 4-terminal Dreyfus-Wagner，得到所有 portal 子集的精确局部 Steiner cost。只有同时满足两个条件才替换：第一，组件内部点数 `>5`，因为替换会新增 4 个 triple hub 和 1 个 quad hub，若删点数不大于新增 hub 数就不是图规模收益；第二，存在 quad hub 权重 `w0..w3`，满足四条边权和等于四门户局部最优代价，且任意二/三门户子集通过 quad hub 不低于对应局部表代价。通过后加入 6 条 pair 边、4 个 triple hub 和 1 个 quad hub；若任一条件失败，组件原样保留。

四门户替换的正确性仍是 exact graph reduction：pair 与 triple gadget 精确保留所有二/三门户子集代价；quad hub 对四门户子集给出精确代价，并由不等式保证不会让任何真子集变便宜。若新图中混用多个真子集 gadget，它们对应的是原局部图中若干合法连接子图的并，不能比局部 Steiner 最优表更便宜。因此替换只改变局部表示，不改变外部可见的最优连接代价。

这一步是 exact graph reduction，不是状态剪枝超参数。它不会降低 `best`，也不使用数据集、组数或 DP 层数特判；只减少后续 DP 看到的 `n,m`。输出中 `original_n/original_m` 记录原图规模，`n/m` 记录两轮缩图后的规模，`leaf_reduce_removed_*` 记录第二步处理掉的内部顶点数；`leaf_reduce_two_portal_*`、`leaf_reduce_three_portal_*` 和 `leaf_reduce_four_portal_*` 分别记录二门户、三门户和四门户贡献。

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

dense-light 行当前使用 full dense `double[n]`。它牺牲了少量无穷点扫描，但随机 `Lookup(S,v)` 是直接数组访问，常数很小。

复杂度上，dense 行的 join 是按点扫描，仍计入 `O(3^g n)`；空间上，DBLP g15 的 105 个 pair 行若全 dense，约为 `105*n*sizeof(double)≈2.1GB`，明显小于保存 `v,d,cover,need` 的通用稀疏表示。曾测试过 packed dense（`bitset+values`）进一步压缩超过 dense 阈值但未接近全图的行；该表示正确，但在 DBLP snapshot g9 上时间和 peak RSS 都没有收益，因此不保留。

### 4.5.1 已撤回且不得恢复：非 dense 高阶行轻量表示探针

full DBLP g13 的 k=4 以后主要增长来自高阶 sparse 行，因此曾测试过一个专用探针：在缺少通用成本判据的情况下，强行让部分非 dense 高阶行只保留 `v,d`，不持久化 `cover,need`，用来判断高阶行常数是否已经成为瓶颈。

该探针确认了一个有用事实：高阶行的 `cover/need` 常数确实会影响 DBLP g13 的 k=4/k=5 瓶颈。但它的触发条件不是由通用成本模型或安全下界推出，违反 `agent.md` 第六条。当前代码已删除该分支；`z.light` 只来自 `k==2` 的 DP 结构或 `dense_by_cost` 的通用表示成本判定，非 dense 高阶行恢复普通 sparse，继续保存 `cover,need`。完整数据只保留在 `test18_effect_report.md` 的撤出探针章节，不能写作当前主线能力。

full DBLP g13 组合探针中，这条表示与已撤出的三分块上界叠加后，让当时那条探针从 k=4 初段推进到完整跨过 k=4 并进入 k=5。可保留的信息只是瓶颈判断：k=4 后段 `early_cover` 将 `best` 从 `13.3535` 降到 `13.1290`，k=5 初段继续到 `13.1267`；但该组合探针在 k=5 初段手动中断，不能写作当前主线成绩。three-portal exact torso 的 g13 长跑探针只保留结构和瓶颈信息；当前源码撤回该特判后的中止探针已确认仍能在 k=5 masks `1316` 到达 `best=12.9949`，但缺少最终权重和完整 wall time。

### 4.5.2 light/dense-light compact

当 `best` 在层切换处下降时，Test18 现在也会 compact light 行。基础 stale 条件是：

```text
dp[S][v] + LowerBound(v,U-S) > best
```

则 `(S,v)` 不可能再作为当前侧参与任何更优完整解。等价地，仍值得保留的状态必须满足必要条件 `dp[S][v] + LowerBound(v,U-S) <= best`。普通 sparse 行直接使用持久 `need`；sparse-light / dense-light 行不存 `need`，compact 时现场重算保存阶段同一套安全下界：基础 `LowerBound(v,U-S)` 加上 4.6 的正确 DP 顺序下界。dense-light 行只把失效 root 置为 `INF`，继续保持 dense 表示；曾尝试在密度下降后转回 sparse-light，但 Windows 进程 RSS 峰值反而上升，因此当前不做表示转换。

正确性与代价：

- light 行保存的 `dp[S][v]` 仍是精确值，后续同根合并读取的数值不变；
- 不保存 `need` 只会少跳过 stale 状态，可能增加工作量，但不会产生错误答案；
- 不保存 actual-cover 时，后续 cover 只使用保守的 `S | color[v]`。这可能错过一些 early upper update，但不会构造非法上界，也不会影响 DP 精确性；
- compact 使用的仍是保存阶段已经证明安全的必要条件，只在 best 下降后重算，不改变任何 surviving `dp` 值；
- 实际代价轻量：普通行复用 `need`，light 行只在 best 下降后的 compact 点重算，dense-light 行顺着已有 dense 数组线性扫描一次。

## 4.6 正确 DP 顺序保存必要条件

Test18 现在利用当前 size-order / mask-order 做三类保存必要条件：

1. `|S|=H` 的行处理完自己的 Dijkstra 与 Complete 后，已经不可能再通过 join 生成更大半掩码。若按照同层 mask 顺序，后续也不存在一个合法 Complete 会读取 `S` 作为补侧，则整行不保存。该判断只依赖 mask 顺序，预计算为 `needed_after_row[S]`。
2. 当 `g` 为奇数、`|S|=H`、`R=U-S` 有 `H+1` 个组时，未来能读取 `S` 的完整拼接只能是“一个未来 H-mask `R-{b}` 加一个 singleton `{b}`”。因此保存 root `v` 前使用必要条件：

```text
dp[S][v] + min_b( LowerBound(v,R-{b}) + gd[b][v] ) <= best
```

其中只枚举确实在当前 `S` 之后处理的未来 H-mask。这个下界不用进 Dijkstra，不需要满足 1-Lipschitz；它只在最终保存点删除不可能被后续 Complete 改善 best 的 root。
3. 对 `|S|<H` 的行，当前行自己的 Complete 已经执行完；若保存的 `(S,v)` 以后还能产生更优解，那么从此刻开始，算法未来至少还要引入两个非空补侧块。原因是未来 join 只能生成大小不超过 `H` 的半掩码，而 `R=U-S` 不可能一次被某个半掩码吃完；若 `(S,v)` 直接作为未来 Complete 的补侧被读取，另两个 Complete 块也同样非空。因此未来额外代价至少分别触达 `R` 中离 `v` 最远和最近的两个组。保存前使用：

```text
dp[S][v] + max(
    LowerBound(v,R),
    max_{a in R} gd[a][v] + min_{a in R} gd[a][v]
) <= best
```

这个 future split 下界只在保存点和 light/dense-light compact 点生效；它不进入当前 mask 的 Dijkstra relax，所以不要求 1-Lipschitz。

正确性与代价：

- 最大层顺序剪枝发生在当前行自己的 Complete 之后；被整行跳过的状态没有任何后续 join 或 Complete 读者；
- 奇数最大层 forced LB 枚举的是后续 Complete 的唯一合法块形；每一块都用安全下界，因此只会低估未来所需代价，不会误删可能更优的状态；
- future split 下界只使用当前 root 到补集各组的最短路距离；任意未来算法路径都要为至少两个非空补侧块分别付费，所以 `far+near` 是该路径额外代价的安全下界；
- 这些判断都只在保存/compact 阶段生效，不进入 Dijkstra relax，也不要求新下界满足 1-Lipschitz；
- fast snapshot 中这些条件与 light/dense-light compact 叠加后 live `(mask,v)` 下降 `28.66%`，达到多数据集平均 `20%` 目标；其中 `DBLP_data_bfs` 仍只有 `9.58%`，需要继续关注。

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

当前实现有两处使用 actual cover：

- 当 `k*3<g`、名义 mask 还太小不能做普通 Complete 时，如果 `cover_rem` 已经小到能由两个可查半块补完，就提前尝试合法完整上界；
- 当 `k*3>=g`、普通 Complete 已经可用时，也用 `cover_rem` 替代名义 `rem`。若 `cover_rem==rem`，复用预计算的补集二分；若实际覆盖缩小了补集，则现场枚举更小的 `cover_rem` 二分。

后者是 2026-07-08 补齐的结构性修正：它不新增任何剪枝，只避免在上界构造中重复补已经由当前树顺路覆盖的组。Toronto query 1 与 DBLP snapshot g9 query 1 权重均保持一致；DBLP g9 中 `complement_cover_smaller=2672`，其中旧 early-cover 贡献 `1278`，normal Complete 额外触发约 `1394` 次，但 `finite_states/live_states` 尚未下降。

## 5.2 补侧 Complete row 物化

当 `k*3>=g` 且普通 Complete 可用时，固定当前 mask `S` 的名义补集：

```text
rem = U ^ S
```

原始实现会在每个 popped root `u` 上重复枚举：

```text
other(u) = min_{X union Y = rem, X intersect Y = empty}
           dp[X][u] + dp[Y][u]
```

这批 `X,Y` 全部来自已经保存的 half-DP row 或 singleton `gd`，与当前 Dijkstra 的传播无关；对同一个 `rem`，不同 popped root 只是读取同一批 rooted row 在不同 `v` 上的值。因此当前实现把普通 `cover_rem==rem` 的补侧结果懒物化为一条临时 row：

```text
complete_row[v] = min_X dp[X][v] + dp[rem-X][v]
```

物化后，后续 `Complete(u)` 只需要读取 `complete_row[u]`。若 actual cover 让 `cover_rem` 小于名义 `rem`，仍沿用原来的现场枚举；这条 cache 只服务普通补集，不混合 cover-aware 的更小补集语义。

触发规则是无参数的 rent/buy 成本比较：

- `rent` 是继续直接 Complete 一次时，对所有 split 做两次 `Lookup` 的估计成本；
- `buy` 是把当前 `rem` 的补侧 row 扫描/合并出来的估计成本，singleton/dense/sparse/sparse-sparse 都复用当前行表示的扫描模型；
- 当已经支付的直接查询成本加上下一次 `rent` 不低于 `buy` 时，构建一次 `complete_row`。

正确性上，它只是改变同一批已保存 rooted row 的求值顺序，不引入新的下界，也不剪状态。构造出的 `complete_row[v]` 仍是若干合法 rooted 组件同根粘合的上界；若 `best` 在 row 构建后继续下降，缓存中的值可能变得不再有剪枝意义，但只有在 `dist[S][u]+complete_row[u] < best` 时才会更新合法完整上界，因此不会破坏精确性。

实测 40k g12 query 1 三个快照权重均与回撤版一致，wall time 明显下降；完整表见 `test18_effect_report.md` 的 7.2 节。

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

把分区块大小扩到 3，即允许 singleton/pair/triple 三类块，上界本身仍是安全的：它只使用已 ready 的精确 rooted row 和虚拟 singleton，所有块在同一 root 上粘合；求和可能重复计边，因此只会高估合法完整树代价。

已撤出的是旧的两种启用方式：只在 k=3 layer-end 统一触发，以及在 `g=13` 探针中按 rows `96/160/224/256` 增量触发。前者在快照上没有额外状态收益，后者属于没有充分理论依据的固定超参数，违反 `agent.md` 第六条。

当前保留的是事件触发版本：每条 k=3 或 k=4 row `T` 完成并保存后，只枚举“包含这条新 row”的新分区，即 `T + partition(U-T)`；补侧分区允许 singleton、pair，以及已 ready 且大小不超过 `|T|` 的 triple/quad。触发点来自 DP row ready 事件，不依赖数据集、组数、固定 rows 或时间点。它只更新 `best`，不剪状态；若 `best` 下降，则复用既有 compact 逻辑让后续保存条件自然变强。40k g12 上它能前移 k=3/k=4 best，但 wall 仍是混合结果；详细数据见 `test18_effect_report.md`。

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

补侧 Complete row 物化只把固定 `rem` 的补侧 split 从“每个 popped root 现场枚举”改为“必要时先合成一条 rooted row 再查询”。它读取的仍然是同一批 singleton `gd` 和已保存的精确 half-DP row；`complete_row[v]` 是这些合法 rooted 组件在同一 root `v` 上粘合得到的上界。它不作为下界、不删除状态，只可能更快发现同一个或更早可见的合法完整上界。

pair/single 同根分区上界只在已保存的精确 pair rooted row 与 singleton `gd` 上构造合法同根树。每个块本身都是合法 rooted tree，所有块同根连通。求和可能高估真实并集代价，不会低估，因此只能安全降低 `best`。

frontier k=3/k=4 同根分区上界同样只构造合法完整树：新 ready 的 row `T` 和补侧每个 ready 分块都以同一 root 粘合，分块本身来自精确 rooted row 或 singleton `gd`。把这些分块代价相加可能重复计边，因此得到的是可行完整解上界；它不作为下界，也不直接删除状态。

Far/LB 的 lazy cache 只是把同一个安全下界的计算方式从预处理表或全量清空改成当前 mask 在线计算，不改变任何判定语义。

## 8. 复杂度

Test18 仍满足：

```text
O(3^g n + 2^g((g + log n)n + m)).
```

原因：

- 虚拟 singleton 减少存储和扫描；
- `full_lb` 是 `O(gn)` 级别的下界预计算；
- 标准 Steiner 度缩图和 Voronoi leaf reduction 都是图规模预处理；前者线性扫描/链追踪，后者复用组距离并在线性扫描图后重建缩减图，不改变主 DP 的渐进阶；
- `need` 检查和 compact 总量受已保存稀疏状态数控制；
- 稠密行自适应表示只改变存储和交集枚举方式，dense join 仍是按点扫描，计入 `O(3^g n)`；
- Far/LB 的当前 mask 在线计算最坏为 `O(n g 2^g)`，被 `O(2^g g n)` 口径覆盖；stamp cache 避免每个 mask 全量清空，不新增渐进代价；
- cover-aware Complete 枚举的是补集二分，仍由三进制归属计数控制；
- 补侧 Complete row 物化对同一个 `rem` 最多额外构建一条临时 row；构建时对补侧 split 的总扫描仍是当前 row 表示下的 join/scan 工作，摊回同一组三进制归属，不超过原 `O(3^g n)` 口径；临时空间是一条 `O(n)` double row；
- pair/single 同根分区上界只对 `r` 个候选根运行一次，复杂度 `O(r g 2^g)`，不乘 `n`，在理论上被主项覆盖。
- frontier k=3/k=4 同根分区上界只在已保存的 k=3/k=4 row ready 事件上运行，单次只枚举 `U-T` 的子掩码和候选根；总量为 `O(r poly(g) 2^g)`，不乘图边数，被 `O(3^g n)` 主项覆盖。

空间上，Test18 删除 singleton 持久行；非 light 的 `|S|>2` 行额外保存一个 `need` double；`|S|=2` 的 pair 行默认轻量保存精确距离，任意足够稠密的行会转成 dense-light 距离表，避免在 DBLP 这类近全图行上为每个状态支付 `cover/need` 常数。未通过表示成本判定的高阶行保持普通 sparse，不再按 `g`、固定层数或数据集特判。

## 9. 统计字段

Test18 在 Test17 基础上增加：

```text
global_root_alive / global_root_pruned
tryset_calls / tryset_keep
tryset_pruned_full / tryset_pruned_ge_best / tryset_pruned_far / tryset_pruned_lb
astar_order_enabled / astar_relax_lb_pruned
stale_need_skips / lookup_need_skips
compact_calls / compact_removed / compact_ms
compact_light_removed / compact_dense_removed
original_n / original_m
degree_reduce_removed_vertices / degree_reduce_removed_edges
degree_reduce_leaf_vertices / degree_reduce_dead_vertices / degree_reduce_contracted_vertices / degree_reduce_ms
leaf_reduce_removed_vertices / leaf_reduce_removed_edges / leaf_reduce_removed_components / leaf_reduce_ms
leaf_reduce_two_portal_components / leaf_reduce_two_portal_vertices / leaf_reduce_two_portal_edges_added
leaf_reduce_three_portal_components / leaf_reduce_three_portal_vertices / leaf_reduce_three_portal_hubs_added / leaf_reduce_three_portal_edges_added
leaf_reduce_four_portal_components / leaf_reduce_four_portal_vertices / leaf_reduce_four_portal_hubs_added / leaf_reduce_four_portal_edges_added
order_pruned_rows / order_pruned_states / order_lb_pruned_states / order_split_pruned_states
live_states
best_updates / first_best_update_size / last_best_update_size
root_star_upper / greedy_upper / multi_greedy_upper / multi_greedy_roots
pair_partition_roots / pair_partition_updates / pair_partition_upper / pair_partition_ms
frontier_partition_triggers / frontier_partition_roots / frontier_partition_candidates
frontier_partition_updates / frontier_partition_upper / frontier_partition_ms
best_after_k*
early_cover_extra / early_cover_pair_ready / early_cover_pair_better
early_cover_best_updates / early_cover_best_candidate
pair_saved_cover_extra / pair_saved_cover_extra_groups / pair_saved_cover*
pair_dense_rows / pair_dense_states
dense_rows / dense_states / dense_rows_k* / dense_states_k*
pull_pairs_k* / pull_scan_k* / pull_hits_k*
pull_seed_scan_k* / pull_singleton_scan_k* / pull_dense_dense_scan_k* / pull_dense_sparse_scan_k* / pull_sparse_sparse_scan_k*
search_seed_try_k* / search_seed_push_k* / search_pq_pop_k* / search_relax_try_k* / search_relax_ok_k*
complement_calls_k* / complement_scan_k* / complement_hits_k*
complement_cache_builds / complement_cache_queries
complement_cache_scan / complement_cache_hits / complement_cache_need_skips
complement_cache_rent_cost / complement_cache_buy_cost
complement_cache_builds_k* / complement_cache_queries_k*
complement_cache_scan_k* / complement_cache_hits_k*
pull_ms_k* / search_ms_k* / complement_ms_k*
complement_cache_ms / complement_cache_ms_k*
pair_saved_slack_count / pair_saved_slack_rel_avg / pair_saved_slack_rel_max / pair_saved_slack_rel*
```

建议重点看：

1. `tryset_keep / tryset_calls`：前置门控是否真正减少写入。
2. `stale_need_skips + compact_removed`：`best` 下降后旧状态是否大量失效。
3. `root_star_upper / greedy_upper / multi_greedy_upper`：初始上界是否已经足够强。
4. `pair_partition_*`：pair 层完成后是否在进入 k=3 前降低 `best`。
5. `frontier_partition_*`：事件触发的 k=3/k=4 同根分区上界是否在对应行完成时降低 `best`；`triggers_k3/triggers_k4` 是完成的 frontier row 数，`candidates` 是补侧分区可行的候选 root 次数。
6. `astar_order_enabled / astar_relax_lb_pruned` 搭配 `pq_pop / relax_ok / search_ms`：A* ordering 只影响单层图搜索顺序和 relax push，不应期待 `finite/live` 改变。
7. `early_cover_best_updates` 和 `best_after_k*`：cover-aware Complete 是否把好上界前移。
8. `pair_saved_cover*`：pair 层保存状态是否实际覆盖更多组，是否值得研究升格。
9. `pair_dense_rows / pair_dense_states`：pair 行是否触发 dense 表示，以及 dense 表示覆盖多少状态。
10. `dense_rows* / dense_states*`：所有 dense-light 行的规模，尤其看是否在 k=3 以后触发。
11. `pair_saved_slack_rel*`：pair 层保存状态距离当前 `best` 门槛还有多远。桶编号含义为：
   - `0`: `(best-need)/best <= 1%`
   - `1`: `<= 5%`
   - `2`: `<= 10%`
   - `3`: `<= 25%`
   - `4`: `<= 50%`
   - `5`: `> 50%`
12. `finite_states`、`active_seed`、`pull_scan`：状态总量是否真的下降。
13. `pull_*_k* / search_*_k* / complement_*_k*`：按层拆分 join、Dijkstra search 与 Complete 补侧扫描成本；`pull_seed` 是 k=2 的 pair seed，`pull_singleton` 是 singleton+row，`pull_dense_dense / pull_dense_sparse / pull_sparse_sparse` 区分 join 的行表示组合。
14. `complement_cache_*`：补侧 Complete row 物化是否触发、构建扫描量、缓存查询量和 rent/buy 成本；`complement_cache_ms` 是 `complement_ms` 的子集，不要与 `complement_ms` 相加。
15. `order_pruned_states / order_lb_pruned_states / order_split_pruned_states`：正确 DP 顺序下，最大层行无未来读者、奇数最大层 forced-complete 下界、以及 `|S|<H` future split 下界额外删除的保存状态。
15. `live_states=finite_states-compact_removed`：compact 后仍留在状态行中、会参与后续 join / lookup 的 live `(mask,v)` 数量。
16. `original_n/original_m`、`degree_reduce_*`、`leaf_reduce_removed_*`、`leaf_reduce_two_portal_*`、`leaf_reduce_three_portal_*` 与 `leaf_reduce_four_portal_*`：两轮 exact query graph reduction 是否真正缩小了当前 query 的图，并区分二门户桥边、三门户 hub 和四门户 pair/triple/quad gadget 的贡献。

## 10. 已验证结果

本节保留较早的原始验证摘录，方便追溯 Test18 的演进；当前最新 A/B 表和 DBLP g13 结构探针统一维护在 `test18_effect_report.md`。

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

结论：Test18 对中等组数是明确正优化；tree-aware greedy、cover-aware Complete、补侧 Complete row 物化、稠密行自适应表示和 pair/single 同根分区上界都值得保留。pair/single 分区是目前第一条能在 full DBLP g15 上把 k=3 前 best 从 `19.3812` 降到 `17.7702` 的机制；补侧 row cache 则是 2026-07-09 后第一条在 40k g12 快照上把 k=5/k=6 Complete 热点直接打下来的 Test18 内生机制。它不减少状态数本身，若所有 k=3 行都接近全图稠密，仅 pair+k3 的 dense 距离表仍是十 GiB 级；因此后续仍需要更强合法上界或结构性保存必要条件。当前 `Far/LowerBound/full_lb` 都不足以产生数量级剪枝；slack 诊断也说明低成本、小幅度增强 LB 很难解决问题。
