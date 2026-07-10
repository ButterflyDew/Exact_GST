# Dual-Anchored Global Labels：DBLP g13 精确突破

更新时间：2026-07-10。本文是当前 global-label 研究线的唯一主说明，记录问题层面的状态空间变化、精确性证明、复杂度、Release/O2 验证和 full DBLP g13 q1 完整结果。旧 anchored 失败版本仍保存在 `archive/test19_probe_archive_20260710.md`，不得与本文当前实现混用。

实现与 runner：

```text
tools/global_half_probe/global_half_probe.cpp
tools/global_half_probe/run_bounded.ps1
tools/dual_cut_probe/dual_cut_potential.h
```

## 1. 结论

当前独立原型已经精确跑通未经普通图/query 压缩的 full DBLP g13 q1：

```text
best                 12.5936282853
solver wall          1427.6247319s
runner wall          1459.076s
peak RSS             9231.500MiB (9.015GiB)
settled labels       57,198,969
all possible labels  10,228,417,290
settled ratio        0.55921622%
```

对照上一条合规 Test18 完整成绩 `20193.624s / 25185.023MiB`，当前 solver wall 快 `14.15x`，峰值 RSS 降至 `36.65%`。Test18 使用了普通图缩减，当前方法没有；本文不把 baseline 也能使用的图/query 压缩计作方法收益。

这不是 ReleaseV1 的替换版。它解决的是大 `g` 的状态洪峰；small `g=4..6` 上 dual 预处理会反噬 PrunedDP，详见第 7 节。

## 2. 核心变化

### 2.1 用任意必达组消掉一个 subset 维度

固定任意组 `A0`。每棵可行 GST 都至少包含一个 `x in A0`；固定该 `x` 后，只需连接其余 `g-1` 个组。算法不为 anchor bit 建 label，而在全图同时搜索：

```text
D(S,v),  empty != S subseteq U without A0
goal = min { D(U without A0, x) : x in A0 }
```

因此 label mask 数从 `2^g-1` 降为 `2^(g-1)-1`，join 上界从 `3^g n` 降为 `3^(g-1)n`。anchor 选第一组只是确定性规范，不是经验参数；换任意组都保持精确。

### 2.2 全局 label-setting 取代逐层稠密 row

每个 `(S,v)` 是一个 Dijkstra-Steiner label，按 `D(S,v)+h(v,U-S)` 进入同一个 `std::priority_queue`。状态只从 singleton、图边松弛和同根不交 mask join 生成；没有先完整物化 pair/triple rows，也没有固定 `k=3/k=4`、数据集、`g`、row density 或运行时刻分支。

同根 join 在两种等价枚举中选择理论工作上界较小者：补集 submask 枚举，或已定型 mask 的倒排位图。选择式是

```text
bitmap_words * |S| <= 2^(remaining bits) - 1
```

它比较两种完整枚举的实际操作上界，不是调参阈值；输出的每个不交 pair 仍计入 `O(3^(g-1)n)`。

### 2.3 GST dual-cut 势函数

从 root-star 最优根运行一次确定性的 farthest-first Wong-style directed-cut dual ascent。对每个组得到势 `p_a(v)`，查询

```text
h_dual(v,R) = sum_{a in R} p_a(v)
```

顺序处理共享残量弧容量，所以任一有向弧上所有组势差之和不超过边费；于是它同时满足 edge consistency 和 disjoint-subset splice。当前 key 取以下安全下界的最大值：

```text
cheap group-distance / group-MST bound
exact group-metric TSP/2 bound
directed-cut dual potential
```

零残量图恢复的 primal 只作为完整可行上界。dual 的详细构造与证明见 `dual_cut_potential.md`。

### 2.4 生成即用的 root-star 完成上界

每个新可行 label 已经表示一棵以 `v` 为根、覆盖 `S` 的树。把 `v` 到每个剩余组的独立最短路并入，得到合法完整解：

```text
UB(S,v) = D_candidate(S,v) + sum_{a notin S} gd[a][v]
```

路径或边重复只会使这个和高估 union 成本，不会产生过小上界。`CheapLowerBound` 本来就要扫描剩余组，因此同步累加该上界不增加渐近复杂度。full g13 中它在搜索前段连续降低 `best`，使 open frontier 从增长转为回落；这是本轮能完成的关键。

### 2.5 无损紧凑 frontier

root-local flat hash 的 slot 直接存 `(cost, lower, mask, settled)`；heap node 只存 `(key,root,mask)`，弹出后从 hash 读取精确 cost。所有数值仍为 `double`，没有 float 量化、load-factor 特判或近似状态淘汰。

在同一 900 秒目标探针上，该表示把 peak RSS 从约 `10.96GiB` 降到 `8.75GiB`；最终 full peak 为 `9.015GiB`。

## 3. 精确性

1. **状态可行。** singleton 为零成本组内点；边松弛给可行树加路径边；同根 join 合并两棵可行树。因此 tentative 和 settled cost 都是可行上界。
2. **状态完备。** Dreyfus-Wagner recurrence 的边传播与不交 subset join 都被枚举；固定 anchor 后，对每个 `x in A0` 的最优 rooted tree 仍在该 recurrence 中。
3. **下界安全。** group-distance、TSP/2 和 dual potential 均 admissible；取最大值仍 admissible。各项满足图边 consistency 与 subset splice，因此全局 label-setting 不会永久定型错误值。
4. **上界安全。** greedy、dual primal 和逐 label root-star completion 都显式构造可行 GST，只能降低合法 `best`。
5. **终止精确。** 当 heap 最小 `cost+lower >= best` 时，任何未定型或未生成完成路径都不可能优于 `best`；此时返回值即最优值。

所有剪枝只使用上述 lower/upper 关系。进度环境变量和 bounded timeout 不进入求解决策。

## 4. 复杂度

按 `agent.md`，实现使用 `std::priority_queue`，理论核算按斐波那契堆口径。设原图 `n` 点、`m` 边：

```text
labels / edge propagation  O(2^(g-1) n + 2^(g-1) m)
all disjoint joins         O(3^(g-1) n)
group distances + dual     O(g(m + n log n))
metric TSP/2 bounds        O(g^3 2^g) time and O(g^2 2^g) space
group-MST subset bounds    O(g^2 2^g) time and O(2^g) space
```

这些项均包含在项目目标

```text
O(3^g n + 2^g((g + log n)n + m))
```

之内。label hash 是 `O(2^(g-1)n)`，同根倒排索引是 `O(g 2^(g-1)n)` bit/word 口径。当前 binary heap 使用 lazy stale nodes，其理论峰值由成功松弛数界定，而不只是 label 数；实测 full peak open 为 `61.729M`。本轮改进是把实际 frontier 变稀疏并降低每个存活项常数，不声称 polynomial space。

## 5. 正确性验证

当前 Release/O2 二进制：

| check | result |
| --- | --- |
| random `g=2..10`, seed `710971` | `1000/1000`, `mismatches=0` |
| random fixed `g=13`, seed `710973` | `50/50`, `mismatches=0` |
| Toronto existing DPBF last run, q `1/40/80/120/160` | 5/5 exact to printed precision |
| fast five dataset versions, `g=9..12` | 20/20, max abs diff `1e-10` |
| small five dataset versions, `g=2..8` | 35/35, max abs diff `1e-10` |

Toronto 比较读取 `result/Toronto/DPBF/default/weights.txt` 的最后一次 160-query run，符合追加结果的比较规则。

## 6. Fast 结果

`data_snapshot/generated_fast`，每个数据版本运行 q1 的 `g=9..12`。表中是当前 solver 内部 wall 合计；ReleaseV1 使用同一 Release/O2 批次的 `total_ms`。

| dataset version | current | ReleaseV1 | current speedup | settled labels |
| --- | ---: | ---: | ---: | ---: |
| `Toronto_data` | `1.742s` | `6.792s` | `3.90x` | `531,687` |
| `Toronto_data_new` | `6.339s` | `11.176s` | `1.76x` | `1,910,121` |
| `DBLP_data_bfs` | `0.676s` | `2.828s` | `4.19x` | `31,811` |
| `DBLP_data_new_bfs` | `0.651s` | `2.160s` | `3.32x` | `58,681` |
| `MovieLens_data_bfs` | `4.333s` | `11.755s` | `2.71x` | `11,927` |
| **total** | **`13.742s`** | **`34.712s`** | **`2.53x`** | **`2,544,227`** |

旧 anchored 原型为 `62.418s`；dual、生成式完成上界和紧凑 frontier 组合后快 `4.54x`。PrunedDP 完成的 19 条内部时间为 `279.869s`，MovieLens g12 在 100 秒内未完成；把 timeout 的 100 秒计为下界时，当前相对 PrunedDP 为 `>27.64x`。

## 7. Small 边界

small suite 不隐藏重预处理反噬。表中 current 包含 dual，PrunedDP 为冻结 O2 `total_ms`；每行汇总五个数据版本。

| g | current | dual prep | PrunedDP | speedup | slower individual cases |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | `0.0897s` | `0.0513s` | `0.1476s` | `1.65x` | 0 |
| 3 | `0.1572s` | `0.0814s` | `0.1687s` | `1.07x` | 0 |
| 4 | `0.2982s` | `0.1560s` | `0.1826s` | `0.61x` | 3 |
| 5 | `0.4187s` | `0.2211s` | `0.2314s` | `0.55x` | 2 |
| 6 | `0.5228s` | `0.2563s` | `0.3484s` | `0.67x` | 1 |
| 7 | `0.6414s` | `0.3384s` | `2.8646s` | `4.47x` | 0 |
| 8 | `0.7773s` | `0.4065s` | `7.5134s` | `9.67x` | 0 |

35 条合计 `2.905s` 对 `11.457s`，平均快 `3.94x`，但有 6 条慢于 PrunedDP，最差是 MovieLens g5 的约 `0.48x`。因此当前原型不满足 ReleaseV1 的 small 守门，不进入发行默认路径；也不增加 `g>=7` 或数据集特判来粉饰结果。后续若生产化，应使用由算法工作量推导的统一 gate，或保留 ReleaseV1 fallback，并重新完成全套 A/B。

## 8. Full DBLP g13 q1

命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\global_half_probe\run_bounded.ps1 `
  -GraphFolder data\DBLP -QuerySelector g13 -QueryIndex 1 `
  -AnchorGroup 1 -Seconds 1800
```

运行对象：`n=2,497,782`、`m=12,786,329`、`g=13`、anchor group 1 含 `202` 点。runner 正常退出：

```text
bounded_timeout=0
bounded_elapsed_sec=1459.076
bounded_peak_rss_mb=9231.500
```

主要计数：

| metric | value |
| --- | ---: |
| solver wall | `1427.6247319s` |
| dual build | `35.0138947s` |
| dual objective / primal | `10.9396208724 / 17.3608143368` |
| root-star / initial upper | `17.4274231102 / 15.0174017721` |
| final best | `12.5936282853` |
| settled / pushes / pops | `57,198,969 / 96,453,922 / 74,218,889` |
| bound pruned | `1,864,520,222` |
| edge relaxations | `3,769,227,416` |
| disjoint pairs | `581,264,810` |
| generated-star checks / updates | `1,782,481,490 / 15` |
| settled-star checks / updates | `57,198,969 / 2` |
| peak open / stop open | `61,729,007 / 17,453,930` |

按 mask size 的终态：

| k | settled | created | open at stop | peak open |
| ---: | ---: | ---: | ---: | ---: |
| 1 | `3,800,339` | `4,865,976` | `1,065,637` | `3,708,995` |
| 2 | `8,571,683` | `11,150,487` | `2,578,804` | `8,941,604` |
| 3 | `14,775,088` | `19,050,238` | `4,275,150` | `15,825,272` |
| 4 | `15,915,143` | `20,762,320` | `4,847,177` | `17,852,345` |
| 5 | `9,327,059` | `12,394,827` | `3,067,768` | `10,578,930` |
| 6 | `3,410,181` | `4,661,940` | `1,251,759` | `3,782,192` |
| 7 | `1,005,702` | `1,293,403` | `287,701` | `934,572` |
| 8 | `296,852` | `358,725` | `61,873` | `227,541` |
| 9 | `81,152` | `95,094` | `13,942` | `63,527` |
| 10 | `14,593` | `18,493` | `3,900` | `14,268` |
| 11 | `1,177` | `1,396` | `219` | `1,363` |
| 12 | `0` | `0` | `0` | `0` |

终止时不需要生成 size 12 label：`best` 已等于最优值，heap 最小 lower-bounded key 已不能改善它。open labels 保留在内存但不影响精确终止。

## 9. 保留与撤回

当前保留：anchor 状态变换、global label-setting、dual potential、TSP/2、生成式 root-star 上界、无损紧凑 frontier、理论工作量选择的 join 枚举。

已经探测并撤回：第二 anchor-group dual、group-entry threshold dual、path-union completion，以及旧 anchored entry wrapper。它们或无更新、或收益不跨数据、或增加预处理；均不在当前 hot path。历史数字只进 archive，不恢复固定 `g`、固定层、状态密度、时间点或数据集特判。

## 10. 文献关系

- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：Dijkstra-Steiner labels、consistent lower bound 与 subset splice 接口。
- [DS*](https://arxiv.org/abs/2011.04593)：一般 admissible lower bounds 下的精确启发式 Steiner 搜索。
- [Wong, A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph](https://doi.org/10.1007/BF02612335)：directed-cut dual ascent 路线。
- [Improved Algorithms for the Steiner Problem in Networks](https://www.sciencedirect.com/science/article/pii/S0166218X0000319X)：dual ascent、reduced costs 与 exact Steiner solver。
- [PrunedDP](https://ronghuali.github.io/paper/sigmod2016gst.pdf)：项目 baseline 与 group-metric lower bounds。
