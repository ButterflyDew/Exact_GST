# 运行方法

当前仓库只保留一个研究中 Test 方法：`Test16`。历史 Test11--Test15 的代码、结果和单独文档已经合并/删除；研究脉络见 `readme_files/test16_algorithm.md`。

## 编译

```powershell
cmake -S . -B build
cmake --build build --config Release
```

只编译单个入口：

```powershell
cmake --build build --config Release --target gst_test16_main
```

## 可执行文件

```text
gst_dpbf_main.exe
gst_half_dpbf_main.exe
gst_pruned_dp_main.exe
gst_test16_main.exe
gst_random_compare.exe
gst_snapshot_prepare.exe
```

## 参数

```text
<exe> [graph_selector] [result_root] [query_selector] [data_root] [query_begin] [query_limit]
```

```text
graph_selector  图名、data_root 下的 1-based 编号，或图目录路径；默认 example
result_root     结果根目录，默认 result
query_selector  查询文件；默认 query.txt / Query.txt；g10 代表 query_g10.txt
data_root       数据根目录，默认 data；新数据传 data_new
query_begin     1-based 起始查询编号，默认 1
query_limit     运行条数，默认 -1 表示跑到文件末尾
```

所有方法只输出权重，不支持 tree / virtual。

## 示例

```powershell
.\build\Release\gst_test16_main.exe Toronto result g10 data 1 3
.\build\Release\gst_pruned_dp_main.exe DBLP result g10_uniform data_new 1 1
python tools\snapshot_benchmark\snapshot.py --method Test16 --suite fast --build
```

## 输出

```text
<result_root>/<graph>/<method>/<query_subdir>/weights.txt
<result_root>/<graph>/<method>/<query_subdir>/<method_lower>_stats.txt
```

所有 stats 行都会追加：

```text
wall_ms rss_before_mb rss_after_mb peak_rss_mb
```

其中 `rss_*` 是实际进程常驻内存，`peak_rss_mb` 是进程峰值常驻内存。

`weights.txt` 每行：

```text
time_seconds best_weight
```

无解时 `best_weight=-1`。

查询子目录：

```text
query.txt              -> default
query_g10.txt          -> query_g10
query_g10_uniform.txt  -> query_g10_uniform
```

## 随机对拍

随机对拍工具是独立 tools 子工程，不链接任何方法实现，而是生成临时数据并调用主程序 exe：

```powershell
.\build\Release\gst_random_compare.exe `
  .\build\Release\gst_test16_main.exe `
  .\build\Release\gst_dpbf_main.exe `
  Test16 271828 100 4 10 2 8
```

参数：

```text
gst_random_compare <method_exe> <dpbf_exe> <method_name>
                   [seed] [iterations] [min_n] [max_n] [min_g] [max_g]
                   [work_root=.random_compare_tmp] [keep=0]
```

工具每轮生成一个连通小图和一个查询，分别运行目标方法与 DPBF，然后按 `1e-6` 比较两个 `weights.txt` 的最优值。失败时会打印可复现的图和查询。
