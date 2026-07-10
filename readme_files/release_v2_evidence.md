# ReleaseV2 实验附录：Full DBLP g13 与状态轨迹

更新时间：2026-07-10。本文只保存形成 ReleaseV2 的 precursor 原始证据和撤回项；算法、证明、复杂度、最终 small/fast 结果与论文归属统一见 `release_v2.md`。

## 1. Full Run

命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\global_half_probe\run_bounded.ps1 `
  -GraphFolder data\DBLP -QuerySelector g13 -QueryIndex 1 `
  -AnchorGroup 1 -Seconds 1800
```

环境：Release/O2；原图，不做普通图/query 压缩。

```text
n                      2,497,782
m                      12,786,329
g                      13
anchor group size      202
bounded_timeout        0
runner wall            1459.076s
solver wall            1427.6247319s
peak RSS               9231.500MiB
best                   12.5936282853
```

## 2. Bounds 与搜索总量

```text
dual objective              10.9396208724
dual primal                 17.3608143368
dual build                  35.0138947s
root-star upper             17.4274231102
initial upper               15.0174017721
settled labels              57,198,969
label universe              10,228,417,290
settled ratio               0.55921622%
pushes / pops               96,453,922 / 74,218,889
bound pruned                1,864,520,222
edge relaxations            3,769,227,416
disjoint pairs              581,264,810
generated-star updates      15
settled-star updates        2
peak open / stop open       61,729,007 / 17,453,930
```

剪枝与 join 枚举分解：

```text
prehash bound pruned        152,374,394
cheap bound pruned          615,939,855
expensive future queries    1,166,541,635
open future reuses          26,118,260
legacy scan work estimate   8,177,268,846
submask queries / probes    2,009,619 / 88,311,571
bitmap queries              55,189,350
bitmap word operations      488,218,259
best updates                17
```

`best` 的关键轨迹：

```text
settled 65,536       14.906159...
settled 262,144      13.893288655...
settled 524,288      13.838512054...
settled 1,048,576    12.6821107071
settled 33,554,432   12.6799005233
final                12.5936282853
```

生成式 root-star 在搜索早期给出关键上界；open frontier 在约 `61.7M` 达峰后回落，最终由 `min key >= best` 精确停止。

## 3. 按 Mask Size

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

不生成 size-12 label 不是缺失：终止时完整可行值已等于最优值，剩余 heap key 的 lower bound 均不能改善它。

## 4. 表示层 A/B

无损紧凑 frontier 做了两件事：hash slot 直接存 `(cost,lower,mask,settled)`，heap node 删除重复 cost。相同 900 秒目标探针的 peak RSS 从约 `10.96GiB` 降到 `8.75GiB`；完整 run peak 为 `9.015GiB`。

旧 dual 接入前 anchored entry wrapper 曾在约 `404 CPU-s` 达到 `23.27GB` 且无最终结果。不能把这个旧负结果继续写成 ReleaseV2 当前状态。

最终 ReleaseV2 fast20 的 label 汇总保留如下，便于以后区分“时间抖动”和“搜索轨迹变化”：

| dataset version | settled | created | max peak open |
| --- | ---: | ---: | ---: |
| Toronto | `531,687` | `611,886` | `228,466` |
| Toronto-new | `1,910,121` | `2,037,777` | `370,901` |
| DBLP | `31,811` | `31,811` | `10,725` |
| DBLP-new | `58,681` | `227,415` | `88,123` |
| MovieLens | `11,927` | `11,927` | `2,672` |
| **total** | **`2,544,227`** | **`2,920,816`** | - |

## 5. 已撤回项

| candidate | evidence | status |
| --- | --- | --- |
| second anchor-group dual | Toronto/DBLP 有小幅状态收益，MovieLens 预处理退化 | removed |
| group-entry threshold dual | g12 无状态变化 | removed |
| path-union completion | correctness 通过，但 full early trajectory 无变化 | removed |
| old separate Entry wrapper | 额外复制 label/heap cost，RSS 明显更高 | replaced |

这些机制不在 `release_v2.cpp`，也没有以开关形式保留。

## 6. Correctness Record

precursor：

```text
seed 710971, random g2..10, 1000 instances, mismatches=0
seed 710973, fixed g13, 50 instances, mismatches=0
Toronto existing DPBF q1/40/80/120/160, all exact
fast20, max abs diff 1e-10
small35, max abs diff 1e-10
```

最终 ReleaseV2 的独立回归与正式 snapshot 目录见 `release_v2.md`。

## 7. 论文说明

本附录不新增算法归属声明。Dijkstra-Steiner、TSP/2、DS*、Wong dual ascent 与 PrunedDP 的来源和 ReleaseV2 的实际适配统一列在 `release_v2.md` 第 11 节，避免在实验日志中重复或产生不同口径。
