# Test19：2026-07-10 降级与失败探针归档

这份文档保存 Test19 中已经降级、弱信号或负信号的探针结果。它们不属于默认主线；引用时要说明其状态。

## 1. One-tree LB：降级为内存模式候选

`tools/future_lb_probe` 显示 all-anchor 1-tree 是 admissible、能强于 current，但存在 consistency violation，因此不能直接作为普通 A* consistent heuristic。Test19 只把它接成默认关闭的诊断/剪枝开关：

```text
GST_TEST19_ONETREE_LB_DIAG=1
GST_TEST19_ONETREE_LB_PRUNE=1
GST_TEST19_ONETREE_LB_PRUNE=seed
GST_TEST19_ONETREE_LB_PRUNE=final
```

### 1.1 future LB probe

| probe | states | candidate | admissibility violations | consistency violations | stronger than current | avg safe gain | max safe gain |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| `seed=606063`, `g=2..7`, sampled | `7,680` | all-anchor 1-tree | `0` | `40` | `161` | `1.981` | `5.5` |
| `seed=606064`, `g=2..6`, exhaustive | `5,602` | all-anchor 1-tree | `0` | `4` | `207` | `1.940` | `8.0` |
| `seed=606065`, `g=5..8`, sampled | `7,680` | all-anchor 1-tree | `0` | `144` | `347` | `2.182` | `9.5` |

结论：one-tree 有理论价值，但需要 DS* / reopen 语义或只用于 admissible prune 点。

### 1.2 诊断结果

诊断只计算 one-tree 是否强于当前 LB，不实际剪枝。

| dataset/query | checks | stronger LB | stronger need | extra seed prune | extra final prune | avg gain | max gain | diag ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto `query.txt` q1 | `129` | `0` | `0` | `0` | `0` | `0` | `0` | 未形成收益 |
| `DBLP_data_bfs query_g9` q1 | `11,479` | `3,594` | `3,348` | `154` | `806` | `0.072545` | `0.475322` | `13.823` |
| `DBLP_data_bfs query_g12` q1 | `2,870,631` | `2,592,918` | `2,592,918` | `214,022` | `272,289` | `0.244828` | `0.732888` | `4,273.854` |

### 1.3 Kruskal prune A/B

以下均为 query 1，权重一致。表中 `prune` 为 seed+final 同时开启。

| dataset/query | default wall | prune wall | finite states | peak RSS | pq_pop | search ms | one-tree pruned |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs query_g12` | `4.957s` | `8.120s` | `1,802,274 -> 1,568,027` | `42.914 -> 37.742 MiB` | `8,977,474 -> 8,704,434` | `3115.910 -> 4596.857` | seed `213,695`, final `245,011` |
| `DBLP_data_new_bfs query_g12` | `4.159s` | `6.167s` | `1,364,093 -> 1,024,399` | `35.312 -> 28.090 MiB` | 未记录为关键项 | `2434.624 -> 3388.553` | seed `104,159`, final `331,005` |

结论：one-tree LB 是真实有效的状态/RSS 剪枝信号，尤其在 DBLP snapshots 上有限状态下降约 `13%--25%`；但当前逐点现算太贵，wall time 变慢，不能默认启用。

### 1.4 seed/final 模式拆分

| dataset/query | mode | wall | finite states | peak RSS | one-tree pruned | 备注 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `DBLP_data_bfs query_g12` | default | `4.957s` | `1,802,274` | `42.914 MiB` | `0` | 基线 |
| `DBLP_data_bfs query_g12` | seed-only | `6.534s` | `1,781,785` | `42.195 MiB` | seed `214,022` | 状态/RSS 收益小 |
| `DBLP_data_bfs query_g12` | final-only | `8.160s` | `1,568,027` | `37.707 MiB` | final `271,975` | RSS 收益主要来自这里 |
| `DBLP_data_new_bfs query_g12` | default | `4.159s` | `1,364,093` | `35.312 MiB` | `0` | 基线 |
| `DBLP_data_new_bfs query_g12` | seed-only | `5.253s` | `1,342,279` | `34.809 MiB` | seed `115,353` | 状态/RSS 收益小 |
| `DBLP_data_new_bfs query_g12` | final-only | `6.226s` | `1,024,399` | `28.043 MiB` | final `347,633` | RSS 收益主要来自这里 |

判断：final-only 更像“内存模式”，能保留主要 finite/RSS 收益，但仍有明显时间代价。

## 2. Two-edge 子下界：太弱

`max_current_anchor_two_edge_half` 只使用 doubled tour 中“任一 anchor 度至少为 2”的必要条件：对 root 或某个组 anchor，取到其它节点的两条最短 metric edge 之和的一半，再和 current 取 max。

| probe | states | candidate | admissibility violations | consistency violations | stronger than current | avg safe gain |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| `seed=606066`, `g=2..7`, sampled | `7,680` | anchor two-edge | `0` | `0` | `5` | `0.500` |
| `seed=606067`, `g=2..6`, exhaustive | `5,033` | anchor two-edge | `0` | `0` | `8` | `1.500` |
| `seed=606068`, `g=5..8`, sampled | `7,680` | anchor two-edge | `0` | `0` | `21` | `0.952` |

结论：它干净但太弱，不接入 Test19。

## 3. Group-aware separator：朴素 Voronoi 前沿为强负信号

`tools/group_separator_probe` 用组候选点做多源最短路 Voronoi 标号，把跨 Voronoi 边的端点作为候选边界，并按 `O(g)` 预算删除后统计剩余连通块触及的组数。

复现命令示例：

```text
build\tools\group_separator_probe\Release\gst_group_separator_probe.exe data DBLP query_g13.txt 1 1 64
```

注意：这是结构诊断，不是 `gst_test19_main` 的 full DBLP g13 求解长跑。

| dataset/query | boundary candidates | 小预算结果 | 全前沿结果 | 判断 |
| --- | ---: | --- | --- | --- |
| Toronto `query.txt` q1, `g=5` | `12,342`，`26.788% n` | 删到 `128g=640` 后仍有 `45,402` 点混合块触及 5 组 | 删完整前沿后无混合块，最大块 `414` 点 | 前沿不小 |
| snapshot `DBLP_data_bfs query_g12` q1 | `3,023`，`86.371% n` | 删到 `64g=768` 后仍有 `1,954` 点混合块触及 11 组 | 删到 `128g=1536` 后只剩 `8` 点 2 组混合块 | 需要删掉很大比例 |
| full DBLP `query_g10` q1 | `1,507,905`，`60.370% n` | 删到 `64g=640` 后仍有 `2,221,386` 点混合块触及 10 组 | 删完整前沿后无混合块，最大块 `1011` 点 | 强负信号 |
| full DBLP `query_g13` q1 | `1,517,943`，`60.772% n` | 删到 `64g=832` 后仍有 `2,220,388` 点混合块触及 13 组 | 删完整前沿后无混合块，最大块 `352` 点 | 强负信号 |

结论：如果 group-aware separator 要成为论文机制，不能是“删 Voronoi 前沿点后每块只含少数组”的朴素版本。该方向暂时降级为结构解释，不进入 solver。

## 4. Global-half Dijkstra-Steiner：理论成立，当前工程形态失败

`tools/global_half_probe` 独立实现了全局 `d+h` 标签队列，只生成
`|S|<=floor(g/2)` 的 rooted labels，并用同根两块索引检测三块完成。它不修改
Test19，也不使用数据集、组数、层级或运行进度特判。

正确性证据：

| seed / 范围 | instances | mismatches | settled / half universe |
| --- | ---: | ---: | ---: |
| `909007`, `g=2..10` | `1000` | `0` | `31.76%` |
| `909008`, 固定 `g=13` | `30` | `0` | `48.67%` |

Release/O2 fast snapshot q1：

| dataset/query | weight | settled ratio | pair ratio | merge checks | wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| `DBLP_data_bfs g9` | `9.8329110000` | `0.60%` | `1.32%` | `0.113M` | `0.129s` |
| `DBLP_data_bfs g12` | `12.1663030000` | `15.37%` | `60.19%` | `572.357M` | `51.814s` |
| `DBLP_data_new_bfs g12` | `10.3147548000` | `4.02%` | `11.64%` | `334.395M` | `20.341s` |

同两条 g12 查询上，Test19 当前实现约为 `3.675s/1.649s`。原型确实能少定型很多
labels，尤其是 `DBLP_data_new_bfs g12`，但同根 disjoint-label 扫描把收益完全吃掉；把
三块完成从逐补集枚举改成增量两块索引后，wall 仍从旧原型约 `44.8s/18.5s` 变为
`51.8s/20.3s`，说明补集枚举不是主瓶颈。

结论：global ordering 和 half-mask 状态空间的理论组合保留为有效信息，但当前
`unordered_map + 扫描同根已定型标签` 的实现不进入 Test19。只有找到不恢复
`O(n2^g)` 稠密表、也不引入经验阈值的增量 disjoint-join 结构后才重启。

## 5. 未完成的 DBLP g13 bounded run

2026-07-10 曾用 TSP ordering、关闭 save-need 启动一次有上限探针，并在
`elapsed=227.864s` 手动停止。最后进度为：

```text
k=2
masks=70
finite=107,075,418
inqueue=107,413,768
peak=3057.99 MiB
best=15.0174
```

该运行发生在硬编码 frontier 撤出前，但停止时 frontier 尚未触发。它没有产出最终权重，
不得作为正确性、最终时间或 g13 已跑通的证据；只说明单独依靠 TSP ordering 仍会在 full
DBLP g13 上快速膨胀。

## 6. Global-label 工程筛查：保留与撤回

第 4 节记录的是最初全扫描版本。后续诊断显示：

| query | legacy same-root scans | truly disjoint | half-union merges |
| --- | ---: | ---: | ---: |
| `DBLP_data_bfs g12` | `572.357M` | `70.740M` | `31.429M` |
| `DBLP_data_new_bfs g12` | `334.395M` | `27.915M` | `6.643M` |

因此保留了两种统一、精确的 disjoint 枚举：complement-submask lookup，以及受同一次
submask 工作预算约束的倒排位图。前者把 half-global 两条 g12 从约 `51.6/21.7s` 降到
`38.0/9.5s`；后者进一步降低候选发现操作，但合法 pair 输出仍是主要成本。

保留的 half-DP 支配规则：若 `|A union B|<=H`，只松弛合并 label，不写 two-block
completion index；任意使用 A/B/C 的答案可用代价不更大的 `D(A union B,v)` 替代 A+B。
two-block index 只保存超半 union，且 pair minimum 真正改善时才查第三块。

撤回项：

- 把 completion certificate 共址进主 label map：稠密 DBLP g12 产生 `960,488` 个
  pair-only entries，wall 从约 `19.0s` 退化到 `22.6s`。
- “最后一个 label 到达时枚举补集二分”：两条 g12 的理论预算为 `195.637M/42.435M`，
  高于当前超半 pair 数，不实现。
- 按 group size 自动选 anchor：DBLP-new g12 的 12 个 anchors 为 `2.92--4.01s`，
  同大小 groups 也有明显差异，不能形成统一规则。

## 7. Rooted-group anchored 原型

后续把固定一个 root terminal 的经典 Dijkstra-Steiner 语义直接适配到 root group：只为
其余 `g-1` 组建 mask，完整 mask 到达 root group 任一候选点即完成。它删除 half-DP
三块完成，理论状态/join 上界为 `2^(g-1)n / 3^(g-1)n`。

正确性：

| seed / mode | instances | mismatches |
| --- | ---: | ---: |
| `919005`, `g=2..10` | `5000` | `0` |
| `919008`, fixed g13 | `100` | `0` |

最终 Release/O2 20 条 fast suite 权重全部一致，总 wall `62.418s`。这是独立原型，快于
Test19 的 `ORDER=0,SAVE=0` 基线 `82.136s`，仍慢于当前默认 `34.731s`。最终每数据版本
wall 为 `2.167/16.637/11.761/3.708/28.145s`，顺序为 Toronto、Toronto-new、DBLP、
DBLP-new、MovieLens。

关键工程演化：root-local flat hash 取代节点式 `unordered_map`；在状态哈希前使用
`max(group_MST_half, 一个必达组距离)` 的 O(1) 安全守门。MovieLens g12 中该守门拒绝
`932.982M` 个候选，wall 从 `41.96s` 降到 `16.11s`，没有数据集分支。

## 8. Anchored full DBLP g13 bounded probe

2026-07-10 在上述机制形成后启动一次约两分钟的有界探针：

```text
gst_global_half_probe --anchored-dataset data\DBLP g13 1 1
```

外层 PowerShell 在约两分钟观察窗口后被终止，但没有杀掉实际 solver 子进程。稍后进程审计
发现 `gst_global_half_probe` 仍在运行，累计约 `404 CPU-s`，working set
`23,268,851,712 bytes`（约 `23.27GB`）；随后按 PID 强制终止并确认进程消失。

没有最终权重，也没有保留下可解释的 settled 进度。这是明确的 peak-memory 负信号：当前
独立 anchored 实现不能直接继续 full g13。下一次必须复用 Test19 预处理、先解决
open/label memory，并使用能显式持有和终止 solver PID 的 bounded runner。
