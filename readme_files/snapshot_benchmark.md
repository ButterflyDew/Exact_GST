# Snapshot benchmark

快照测试用于避免优化只贴合单个数据集。一次命令会对同一个方法跑完
`data_snapshot/snapshot_plan.json` 中配置的 5 个 dataset version，并按 suite 选择图规模与查询数量。

默认 fast 测试的组数是 `g in {9,10,11,12}`；small 使用 `g=2..8` 检查重预处理反噬。runner 也支持在 suite 中显式配置 `groups`，修改计划时必须把新旧 suite 视为不同实验口径。

## Dataset version

当前固定覆盖 5 个版本：

- `Toronto_data`：来自 `data/Toronto`。
- `Toronto_data_new`：来自 `data_new/Toronto`。
- `DBLP_data_bfs`：来自 `data/DBLP` 的随机 BFS 诱导子图。
- `DBLP_data_new_bfs`：来自 `data_new/DBLP` 的随机 BFS 诱导子图。
- `MovieLens_data_bfs`：来自 `data/MovieLens` 的随机 BFS 诱导子图。

`data` 和 `data_new` 中同名图视为两个独立版本。查询会优先从原查询投影到导出子图；
若投影后不足指定条数，则用固定 seed 补足合成查询。每个生成目录下的 `snapshot_meta.txt`
会记录来源、seed、原图规模和实际导出点数。

## Suite 规模

| suite | 生成目录 | 图规模 | 每个 g 准备查询数 | 每个 g 实际执行数 | 总执行查询数 | 目标 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `small` | `data_snapshot/generated_small` | 5 个版本均为 3500 点 BFS 子图，`g=2..8` | 8 | 1 | 35 | 小组数预处理与退化检查，目标约 60s |
| `fast` | `data_snapshot/generated_fast` | 5 个版本均为 3500 点 BFS 子图 | 8 | 1 | 20 | 快速诊断，目标约 100s，可小幅超时 |
| `normal` | `data_snapshot/generated_normal` | Toronto 两版完整图；DBLP/MovieLens 三版为 20000 点 BFS 子图 | 40 | 5 | 100 | 常规回归，目标约 1 小时 |
| `large` | `data_snapshot/generated_large` | Toronto 两版完整图；DBLP/MovieLens 三版为 40000 点 BFS 子图 | 40 | 全部 | 800 | 大量测试，目标约 12 小时 |

说明：

- `fast` 使用独立小图，不复用 normal/large 的图；它的职责是快速发现明显退化。
- `normal` 和 `large` 对大图使用更大的固定 BFS 子图，避免只在 Toronto 或很小子图上过拟合。
- timeout 是单个 `<dataset, g>` 命令的保护阈值；总墙钟时间会在 summary 中输出。

## 命令

从仓库根目录执行：

```powershell
python tools/snapshot_benchmark/snapshot.py --method Test16 --suite fast --build
```

常用选项：

```text
--method Test16        方法名：DPBF / Half_DPBF / PrunedDP / ReleaseV1 / ReleaseV2 / ReleaseV3 / ReleaseV4 / Test16 / Test17 / Test18 / Test19 / Test21 / Test80
--suite fast           small / fast / normal / large
--build                先 configure/build 目标方法与 snapshot prepare 工具
--prepare-only         只生成当前 suite 对应的 snapshot 数据
--no-prepare           复用当前 suite 已有 snapshot 数据，直接跑测试
--force-prepare        删除并重建当前 suite 的 snapshot 数据
--plan <file>          使用另一份 snapshot plan
```

结果写入：

```text
result_snapshot/<suite>/<timestamp>/snapshot_summary.csv
result_snapshot/<suite>/<timestamp>/<dataset>/<method>/query_g*/weights.txt
result_snapshot/<suite>/<timestamp>/<dataset>/<method>/query_g*/<method>_stats.txt
```

`snapshot_summary.csv` 会汇总每个 `<dataset, g>` 的退出码、完成查询数、查询时间、最后权重和
峰值常驻内存。
