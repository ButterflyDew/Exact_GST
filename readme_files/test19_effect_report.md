# Test19：当前效果基线（Release/O2）

更新时间：2026-07-10。本报告只保留当前默认实现可复现、可引用的数据；撤回机制、负探针和未完成长跑统一放在 `readme_files/archive/test19_probe_archive_20260710.md`。

## 1. 结论摘要

Test19 默认启用 exact group-tour TSP/2 的两个接入点：

```text
ordering          seed / pop / relax 使用 d + TSP/2
persisted need    final row 保存时持久化同一下界，供后续 lookup / compact 使用
```

在五个 fast 数据版本、`g=9..12` 共 20 条查询上，默认版与关闭两个接入点的基线权重全部一致；总 wall 从 `82.136s` 降到 `34.731s`（`-57.72%`），finite states 从 `9,014,442` 降到 `5,553,850`（`-38.39%`）。Toronto 两版略慢，本文不使用数据集特判掩盖该边界。

Test19 默认 half-DP 路径自身没有运行 full DBLP g13 q1。2026-07-10，独立的 dual-anchored global-label 原型已经在 `1427.625s / 9.015GiB` 精确完成同一查询；该成绩与 Test19 本页基线分开记录，见 `dual_anchored_global_labels.md`。

## 2. 构建与模式

关键计时均来自显式 Release/O2 构建：

```text
cmake -S . -B build
cmake --build build --config Release --target gst_test19_main
cmake --build build --config Release --target gst_random_compare
```

模式开关：

| mode | environment |
| --- | --- |
| 当前默认 | 不设置变量，`ORDER=1, SAVE=1` |
| order-only | `GST_TEST19_TSP_LB_SAVE=0` |
| save-only | `GST_TEST19_TSP_LB_ORDER=0` |
| 未启用 TSP/2 的基线 | 两个变量都设为 `0` |
| 只统计潜在收益 | `GST_TEST19_TSP_LB_DIAG=1` |

## 3. 正确性

撤出硬编码 k=3/k=4 frontier 后的最终回归：

| path | random compare against DPBF |
| --- | --- |
| 默认 order+save | `ALL_OK seed=707201 iterations=120` |
| baseline，`ORDER=0, SAVE=0` | `ALL_OK seed=707202 iterations=40` |
| order-only | `ALL_OK seed=707203 iterations=40` |
| save-only | `ALL_OK seed=707193 iterations=100` |

Toronto `query.txt` q1 默认路径权重为 `0.2582152999`，并确认 `tsp_lb_order_enabled=1`、`tsp_lb_save_enabled=1`。所有随机和 smoke 临时目录均已清理。

`future_lb_probe` 对 admissibility、edge consistency 和跨 subset splice 条件均未发现 TSP/2 违规；公式和证明见 `test19_algorithm.md`，探针统计见 `test19_research_directions.md`。

## 4. Fast Snapshot A/B

数据位于 `data_snapshot/generated_fast`。每个版本使用 `g=9..12` 各一条查询；结果目录：

```text
baseline      result_snapshot/fast/20260710_114210  (ORDER=0, SAVE=0)
order-only    result_snapshot/fast/20260710_114358  (ORDER=1, SAVE=0)
save-only     result_snapshot/fast/20260710_114613  (ORDER=0, SAVE=1)
default       result_snapshot/fast/20260710_114826  (ORDER=1, SAVE=1)
```

20 条查询在四种模式下权重全部一致。wall 和 finite 为每个数据版本四条查询的合计，peak RSS 为四条中的最大值。

| dataset version | baseline wall | default wall | wall change | finite drop | baseline -> default peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: |
| `Toronto_data` | `1.461s` | `1.540s` | `+5.35%` | `0.12%` | `13.211 -> 18.902 MiB` |
| `Toronto_data_new` | `6.063s` | `6.225s` | `+2.67%` | `0.66%` | `37.074 -> 42.832 MiB` |
| `DBLP_data_bfs` | `6.517s` | `4.842s` | `-25.70%` | `39.13%` | `43.055 -> 39.637 MiB` |
| `DBLP_data_new_bfs` | `5.823s` | `2.519s` | `-56.74%` | `66.37%` | `35.746 -> 25.352 MiB` |
| `MovieLens_data_bfs` | `62.271s` | `19.605s` | `-68.52%` | `64.39%` | `163.855 -> 158.410 MiB` |
| **total** | **`82.136s`** | **`34.731s`** | **`-57.72%`** | **`38.39%`** | - |

Toronto 上 TSP/2 几乎不增强，却仍支付预处理和查询成本；这是当前统一默认策略的已知负边界。DBLP 和 MovieLens 的状态、时间收益足以覆盖它，不增加数据集或规模阈值分支。

## 5. 两个接入点的贡献

| mode | total wall | 主要作用 |
| --- | ---: | --- |
| baseline | `82.136s` | 对照 |
| save-only | `71.205s` | 加强已保存 row 的后续 need |
| order-only | `38.206s` | 提前减少图搜索和入队 |
| order+save | `34.731s` | ordering 后继续减少 compact / lookup 工作 |

order+save 相对 order-only 再降 `9.10%`。组合中 `tsp_lb_save_pruned=0`，但持久 need 仍额外 compact `128,238` 个 live states，并增加 `37,851` 次 lookup skip，因此两个接入点都保留为默认。

## 6. 当前边界

- exact TSP/2 预处理约为 `O(g^3 2^g)` 时间、`O(g^2 2^g)` 空间；逐状态查询约为 `O(|R|^2)`。
- 本页现有结果只说明 Test19 fast baseline，不把独立原型数字记到 Test19 名下。
- dual 接入前的旧 rooted-group global-label 原型曾为 `62.418s`，且一次未完成 full 子进程达到约 `23.27GB`；这是归档负结果。当前 dual-anchored + generated-star + compact 实现 fast 为 `13.742s`，full 已完成，见独立主文档。
- one-tree、two-edge、separator 和旧 bounded g13 输出只作归档证据，不属于当前效果基线。
