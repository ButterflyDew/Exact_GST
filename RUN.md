# Build And Run

当前主要入口：

```text
ReleaseV1   small g 稳健、half-DP 单路径发行版
ReleaseV2   dual-anchored global-label 大 g 发行版
ReleaseV3   ordered half rows 与 farthest-goal global 的统一发行版
ReleaseV4   anchor-aware ordered rows 的当前纯 A 发行版
Test80      ReleaseV4 的带研究统计来源版本
```

先读 `readme_files/test_series_overview.md`。当前纯 A 发行入口为 `gst_release_v4_main`；算法方法见 `readme_files/release_v4_method_cn.md`，发行实现、验证和性能见 `readme_files/release_v4.md`。

`gst_release_v3_main` 保留为框架 B 的冻结发行对照。`gst_test80_main` 保留研究计数和阶段日志，用于解释 ReleaseV4 的来源；ReleaseV4 不调用两者。

## Build

计时只使用 Release/O2；CMake 已显式设置 MSVC `/O2 /Ob2` 或其他编译器 `-O2`：

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_release_v4_main gst_release_v3_main
```

常用目标：

```text
gst_dpbf_main             正确性 oracle
gst_pruned_dp_main        主 baseline
gst_release_v1_main       ReleaseV1
gst_release_v2_main       ReleaseV2
gst_release_v3_main       ReleaseV3
gst_release_v4_main       ReleaseV4
gst_test16_main ... gst_test19_main
gst_test21_main / gst_test80_main
gst_random_compare        黑盒随机对拍
gst_snapshot_prepare      snapshot 数据生成
```

## Solver CLI

```text
<exe> [graph_selector] [result_root] [query_selector] [data_root] [query_begin] [query_limit]
```

```text
graph_selector  图名、data_root 下的 1-based 编号或图目录路径
result_root     结果根目录，默认 result
query_selector  query.txt，或 g13 表示 query_g13.txt
data_root       数据根目录，默认 data
query_begin     1-based 起始编号，默认 1
query_limit     运行条数，默认 -1 表示到文件末尾
```

示例：

```powershell
.\build\Release\gst_release_v2_main.exe Toronto result query data 1 1
.\build\Release\gst_release_v1_main.exe Toronto result g10 data 1 1
.\build\Release\gst_pruned_dp_main.exe DBLP result g10 data 1 1
.\build\Release\gst_release_v3_main.exe Toronto result g13 data 1 1
.\build\Release\gst_release_v4_main.exe DBLP result g13 data 1 1
```

## Output

```text
<result_root>/<graph>/<method>/<query_subdir>/weights.txt
<result_root>/<graph>/<method>/<query_subdir>/<method_lower>_stats.txt
```

`weights.txt` 每个非 header 行为：

```text
time_seconds best_weight
```

文件按 run header 追加。比较结果时必须定位最后一个 header，并读取该次 run 的行；不能默认读取文件开头。无解权重为 `-1`。

## Random Compare

```powershell
.\build\Release\gst_random_compare.exe `
  .\build\Release\gst_release_v4_main.exe `
  .\build\Release\gst_dpbf_main.exe `
  ReleaseV4 714003 100 4 14 2 13 `
  .tmp_random_compare_release_v4 0
```

完整参数：

```text
gst_random_compare <method_exe> <dpbf_exe> <method_name>
                   [seed] [iterations] [min_n] [max_n] [min_g] [max_g]
                   [work_root] [keep=0]
```

工具为每个实例分别运行目标方法和 DPBF，以 `1e-6` 比较最后一次权重。`keep=0` 删除 case 子目录；运行结束仍需删除空 work root。

## Snapshot

```powershell
python tools\snapshot_benchmark\snapshot.py `
  --method ReleaseV4 --suite small --no-prepare

python tools\snapshot_benchmark\snapshot.py `
  --method ReleaseV4 --suite fast --no-prepare
```

suite 与结果格式见 `readme_files/snapshot_benchmark.md`。

## Run Discipline

1. 正确性以 DPBF、`1e-6` 和 Toronto 现有最后一次结果为准。
2. benchmark 使用 Release/O2，并在文档中标出旧非 O2 数字。
3. 运行后清理 `.tmp_random_compare*`、`result_tmp*`、空结果目录和残留 solver 进程。
4. full DBLP g13 q1 的 ReleaseV3、Test80 与 ReleaseV4 都已有完整精确证据；纯文档或格式调整不重复长跑。
5. 不使用数据集、固定 `g`、层级、密度或运行时刻特判，也不把 baseline 可用的普通压缩记作方法贡献。
