# Test64：Pair-Work Anchor Elimination

更新时间：2026-07-12。Test64 检查 A 框架是否应按预计删除的 D2 工作选择 permanent anchor，而不是沿用 ReleaseV3 的 farthest-goal anchor。该规则有合法状态上界、无经验参数，并在 full DBLP 上准确选中事后最优 anchor；但 fast20 的收益集中在 Toronto，两版 DBLP 与 MovieLens 均退化，因此 solver、统计字段和探针模式全部撤回。

## 1. 状态上界

对 pair groups `i,j`、root `v`，任意连接三者的树至少支付：

```text
L_pair(i,j,v) = max(
    gd_i(v), gd_j(v), d(i,j),
    (gd_i(v)+gd_j(v)+d(i,j))/2)
```

最后一项来自树边加倍后的三端点巡回下界。对 remaining groups，再取：

```text
L_future = max(
    farthest remaining group,
    (minimum Hamilton path on remaining groups
     + two nearest remaining groups from v)/2)
```

若 `L_pair+L_future>best`，则该 `(pair,v)` 不可能成为 Test21 的 settled D2 state。对每张 pair 计数仍可能存活的顶点，并选择 incident pair 上界总和最大、即保留 D2 上界最小的 anchor；完全并列时用 farthest-goal 消歧。

minimum group path 只做一次 `O(2^g g^2)` DP。每个顶点预取 group distance 的前三大和前四小值，因此全部 pair/anchor 评分为 `O(g^2 n)` 时间、`O(g^2+2^g g)` 空间，不运行 pair Dijkstra，不读取数据集名、固定 `g`、时间或密度。

## 2. 结构探针

历史 `pair_forest_probe` 临时为全部 pair 输出 seed、三端点上界和逐 anchor 汇总。所有运行使用 Release/O2 与已知精确 best，只用于判断可达空间，不作为 solver 输入。

fast20 中，使用完整 endpoint TSP future 的评分在 20 条里有 14 条直接选到事后最少 settled 的 anchor，另 4 条距最优不超过 `2.5%`；明显误差只出现在绝对 D2 很小的 g9。较便宜的可分离 future 在主要 g12 查询仍保留方向，例如：

```text
DBLP g12       farthest 126,399 -> selected 108,544
MovieLens g12  farthest  19,206 -> selected  15,423
Toronto g12    farthest 130,461 -> selected 129,485
```

full DBLP g13 q1 的 78 张 pair 探针耗时约 `316.1s`，无 closure/decode error：

```text
all-pair settled             125,637,681
farthest anchor group 12     106,310,433 kept
state-bound group 6          104,751,323 kept
settled reduction              1,559,110  (1.47%)
search                       231.672s -> 228.470s
```

中强度 `O(g^2 n)` 评分在 `best=12.5936282853`、Test48 upper `13.019988893` 和正式 dual primal `17.360814` 三种时间线下都选择 group 6。评分探针连同 group distances 的总时间约 `24.5--25.0s`；它没有依赖精确 optimum 才改变选择。

## 3. Solver A/B

临时 Test21 集成通过 Release/O2 随机 `300/300`（seed `713311`）。fast20 快照为 `result_snapshot/fast/20260712_235632`，正式基线为 `20260712_050625`：

| dataset | formal | Test64 | ratio |
| --- | ---: | ---: | ---: |
| DBLP | `0.591s` | `0.744s` | `1.260x` |
| DBLP-new | `0.780s` | `0.851s` | `1.091x` |
| MovieLens | `3.057s` | `3.461s` | `1.132x` |
| Toronto | `2.366s` | `2.113s` | `0.893x` |
| Toronto-new | `5.511s` | `4.216s` | `0.765x` |
| **total** | **`12.304s`** | **`11.386s`** | **`0.925x`** |

snapshot runner 的 query wall 总和为 `11.454s`；表内使用 solver `total_ms`，两种口径结论一致。20 条 anchor-bound 扫描共 `136.2ms`，不是主要退化来源。anchor 改变了后续 mask family、upper 和 A rows，D2 上界最优不等于端到端最优；DBLP g12 的实际 retained D2 甚至从 `2,226` 增到 `3,696`，说明精确-best 结构探针不能代替当前 incumbent 下的完整路径。

## 4. 结论

1. permanent anchor 确实决定哪一整束 incident pair rows 被延后，这是 A 的真实维度消除效应。
2. 单独优化被删除的 D2 上界不能稳定优化完整 A；它只是重新分配同一个单-anchor状态族。
3. 不能用 fast20 总和掩盖三库退化，也不应为这个混合结果触发 Toronto/full DBLP solver。
4. 下一步应递归应用 anchor 分解：把 ordinary D 精确拆成不含 secondary anchor 的 `D0` 与包含它的 `B`，先完成较小 half lattice，再延后整类 `B` states。该方向改变状态生成顺序和首批 D2 规模，而不只是换一个 anchor。

