# Test40：A1 Consumer-Driven D2 探针

更新时间：2026-07-12。Test40 检查能否先由 anchored `A1` rows 给出需要读取的 roots，再只计算这些 roots 上的 ordinary `D2`。结论是否定的：full DBLP 的目标集合本身较密；加入所有合法 consumer threshold 与正式 future pruning 后，三对代表 pair 仍需 settle 完整 D2 row 的 `99.7%--99.9%`。临时工具、构建入口和二进制均已删除，正式 Test21 未改变。

## 1. 问题与最有利口径

固定 permanent anchor `a`。在 known exact incumbent 下先计算：

```text
A1_x(v) = closure(gd[a][v] + gd[x][v]).
```

对 pair `{i,j}`，首批消费者是所有 `x` 不属于 `{i,j}` 的 `A1_x + D2_{i,j}`。探针故意直接读入已知精确答案作为 `best`，因此比正式 Test21 在 D2 前拥有更强剪枝；若该口径仍不稀疏，真实在线调度不会更好。

首先只统计 A1 settled roots 的 pair-wise union。所有操作使用 Release/O2、正式 farthest anchor、TSP/2、directed-cut potential 和 `1e-6` 比较；没有 Hash、数据集分支或 solver 开关。

## 2. Fast g12 目标密度

| dataset | A1 settled sum | pair target union avg | ratio of `n` |
| --- | ---: | ---: | ---: |
| Toronto | `9,032` | `1,252` | `35.78%` |
| Toronto-new | `19,001` | `2,410` | `68.84%` |
| DBLP | `1,726` | `377` | `10.77%` |
| DBLP-new | `1,170` | `216` | `6.18%` |
| MovieLens | `84` | `13` | `0.37%` |

fast DBLP 的强正信号满足运行一次 full 结构探针的门槛；这不是 full solver 长跑，也不生成结果快照。

## 3. Full DBLP g13 目标密度

full q1 使用已知精确值 `12.5936282853`：

```text
n                         2,497,782
anchor                    group 12
A1 settled sum            7,157,183
pair target union avg     1,475,534
pair target ratio avg     59.07%
pair target ratio range   45.16%--61.80%
```

输出 roots 已不具数量级稀疏性，但“输出少”仍不等于“搜索少”，因此继续做严格停止实验。

## 4. Exact Consumer Threshold

对每个 target root 保存所有 A1 消费者允许的最大 D2 成本：

```text
T_ij(v) = max_x(best - A1_x(v) - H(U-{a,i,j,x},v)).
```

若 `D2_ij(v)>T_ij(v)`，该 root 不可能通过首批 `A1+D2` 改善 incumbent。目标先用合法 lower bound 过滤；D2 multi-source Dijkstra 按真实距离定型，并在 frontier 超过各自 threshold 时证明目标无用。同时保留正式 D2 的 `H(U-{i,j},v)` 剪枝。比较侧使用完全相同的 seeds、exact `best` 和 future bound，只按正式 A* key 跑完整 row。

按 raw target union 排序后抽取 min/median/max 三对：

| pair | raw targets | threshold targets | consumer/full settled | consumer/full pops | consumer/full wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| groups 2,13 | `1,127,908` | `516,487` | `126,081/126,146` (`99.95%`) | `146,816/156,951` | `235.1/176.3ms` |
| groups 5,9 | `1,530,490` | `232,878` | `63,612/63,705` (`99.85%`) | `75,886/81,401` | `135.7/105.5ms` |
| groups 3,10 | `1,543,727` | `7,785` | `11,602/11,633` (`99.73%`) | `13,881/15,218` | `29.0/28.2ms` |

consumer stop 只少 `6.4%--8.8%` pushes，却因 raw-distance 定型弱于正式 future ordering而慢 `1.03x--1.33x`。即使第三对 threshold targets 只有 7,785 个，也几乎必须定型完整 row。

## 5. 结论与保留信息

1. A1 首批消费者不足以使 full DBLP D2 搜索提前停止；继续做 pause/resume、target wave 或 higher-A 循环只会扩大 target 集。
2. 结果否决 `consumer-driven targets` 作为显式 D2 barrier 的出路；不修改正式 Test21，也不触发 full solver。
3. exact incumbent 下三对正式 D2 row 只剩 `126k/64k/12k` settled，远低于真实早期 D2 的约 `1.90M` pair-average。这是本次最重要的正信息：首要杠杆不是 D2 输出接口，而是 **在 D2 前得到更强的 anchor-aware 可行上界**。
4. 下一候选必须从 A 的可行树语义产生 incumbent，并保持离线有序列表；不能调用 B warm start、按时刻停止，或加入数据集/`g`/固定层特判。

Test40 未使用新的论文机制，因此没有新增论文引用。
