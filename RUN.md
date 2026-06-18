# GST Framework 运行方法

本文档说明如何在项目根目录编译并运行程序。

## 1. 环境要求

- CMake >= 3.16
- 支持 C++17 的编译器（Windows 可用 Visual Studio 2022）
- 在项目根目录执行命令：`GST_Framework`

## 2. 编译

在 PowerShell 中执行：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

如果你使用的是单配置生成器（如 Ninja），第二条命令可写为：

```powershell
cmake --build build
```

## 3. 运行参数

程序入口：`main.cpp`  
可执行文件：

- `gst_dpbf_main.exe`（方法 1：DPBF）
- `gst_half_dpbf_main.exe`（方法 2：Half_DPBF）
- `gst_pruned_dp_main.exe`（baseline：PrunedDP）
- `gst_test1_main.exe`（测试方法：Test1）
- `gst_test2_main.exe`（测试方法：Test2）
- `gst_test3_main.exe` ... `gst_test12_main.exe`（测试方法：Test3-Test12）
- `gst_main.exe`（兼容旧命名，等价于 DPBF）

参数格式：

```text
<exe> [graph_selector] [output_mode] [result_root] [debug_root] [root_policy] [query_selector] [data_root] [query_begin] [query_limit]
```

- `graph_selector`：选择跑哪个图  
  - 为空：默认优先选 `data/example`，若无则选排序后第一个子目录
  - 数字：按 `data_root` 子目录排序后的 1-based 下标选择（如 `1`、`2`）
  - 字符串：按目录名选择（如 `graph_1`）
  - 也可直接写图目录路径，例如 `data_new/Toronto`
- `output_mode`：输出形式（默认 `weight`）
  - `weight`：只输出边权
  - `tree` 或 `concrete`：输出具体树结构
  - `virtual`：输出虚树结构
- `result_root`：结果根目录（默认 `result`）
- `debug_root`：调试根目录（默认 `debug`）
- `root_policy`：虚树选根优先级（默认 `child_first`）
  - `child_first`：先比较“根最大孩子子树叶子数”，再比较树高
  - `height_first`：先比较树高，再比较“根最大孩子子树叶子数”
- `query_selector`：选择查询文件（默认 `query.txt` 或 `Query.txt`）
  - 可写完整文件名：`query_g10.txt`
  - 可省略 `.txt`：`query_g10`
  - 可省略 `query_` 前缀：`g10`
  - 新数据查询同样支持简写，例如 `g10_uniform` 会解析到 `query_g10_uniform.txt`
  - 文件位于 `<data_root>/<图名>/` 下，例如 `data/DBLP/query_g10.txt`
- `data_root`：数据根目录（默认 `data`）
  - 旧数据：不传该参数，默认读取 `data`
  - 新数据：传 `data_new`
- `query_begin`：可选，1-based 起始查询编号（默认 `1`）
- `query_limit`：可选，最多运行多少条查询（默认 `-1`，表示从 `query_begin` 跑到文件末尾）

## 4. 运行示例

### 4.1 DPBF 默认运行（只输出边权）

```powershell
.\build\Release\gst_dpbf_main.exe
```

### 4.2 Half_DPBF 运行（参数与 DPBF 相同）

```powershell
.\build\Release\gst_half_dpbf_main.exe Toronto virtual result debug height_first
```

### 4.3 兼容旧入口（仍为 DPBF）

```powershell
.\build\Release\gst_main.exe example weight
```

### 4.4 Test1 运行（测试方法）

```powershell
.\build\Release\gst_test1_main.exe Toronto weight result debug
```

### 4.5 Test2 运行（测试方法）

```powershell
.\build\Release\gst_test2_main.exe Toronto weight result debug
```

### 4.6 指定更大组数查询文件

例如运行 `data/DBLP/query_g10.txt`：

```powershell
.\build\Release\gst_test8_main.exe DBLP weight result debug child_first g10
```

结果写入 `result/weight/DBLP/Test8/query_g10/weights.txt`（统计为同目录下 `test8_stats.txt`）。

如果方法不使用 `root_policy`，仍需保留第 6 个参数占位，通常写 `child_first` 即可。

### 4.7 运行 `data_new` 新数据

`data_new` 与旧 `data` 使用相同图和查询格式，只是查询文件名包含 `uniform` / `nonuniform` 后缀。可以通过第 7 个可选参数指定数据根目录：

```powershell
.\build\Release\gst_test8_main.exe Toronto weight result debug child_first g10_uniform data_new
.\build\Release\gst_pruned_dp_main.exe DBLP weight result debug child_first g12_nonuniform data_new
```

只测试部分查询时，例如从第 8 条开始只跑 1 条：

```powershell
.\build\Release\gst_test9_main.exe Toronto weight result_test9 debug child_first g10 data 8 1
```

### 4.8 Test9 空间诊断运行

`Test9` 是 `Test8` 的空间诊断副本，用于研究 `O(n2^g)` 表空间是否可以改成“只保留需求状态”。它输出 `test9_stats.txt`，包含 dense 表规模、实际 finite 状态、`h` 有效项、按层窗口保留峰值等字段。

```powershell
.\build\Release\gst_test9_main.exe example weight result_test9 debug child_first
.\build\Release\gst_test9_main.exe DBLP weight result_test9 debug child_first g10 data 1 3
.\build\Release\gst_test9_main.exe DBLP weight result_test9 debug child_first g4_uniform data_new 1 5
```

`Test10` 使用 dense `dp/h`，只部署三类同根枚举器，不做 sparse `dp/h`：

```powershell
.\build\Release\gst_test10_main.exe MovieLens weight result_test10 debug child_first g8_uniform data_new 1 1
```

`Test11` 从 `Test10` 派生，额外在 live-dp 枚举器中加入 `cand>=best` 与 `cand+far(v,U^nxt)>=best` 剪枝：

```powershell
.\build\Release\gst_test11_main.exe MovieLens weight result_test11 debug child_first g8_uniform data_new 1 1
```

`Test12` 从 `Test11` 派生，用于研究 Dijkstra 前缀节点入堆：非目标前缀节点只有 `dp+LB<=best` 时才入堆，并输出 target 传播统计。

```powershell
.\build\Release\gst_test12_main.exe MovieLens weight result_test12 debug child_first g8_uniform data_new 1 1
```

也可以把图目录直接写在第一个参数中：

```powershell
.\build\Release\gst_test8_main.exe data_new/Toronto weight result debug child_first g10_uniform
```

结果仍写入 `result/weight/<图名>/<方法名>/<查询子目录>/`，例如 `result/weight/Toronto/Test8/query_g10_uniform/weights.txt`。

## 5. 输出说明

结果按**查询文件**分子目录，不同 `query_g**` 互不覆盖：

- 默认 `query.txt` → 子目录 `default`
- `query_g10.txt`（或参数 `g10`）→ 子目录 `query_g10`

目录结构示例：

```text
<result_root>/<输出形式>/<图名>/<方法名>/<查询子目录>/
  weights.txt          # 主结果（原 1.txt）
  test8_stats.txt      # Test8 等方法的诊断统计（若有）
  query_<编号>_tree.txt / query_<编号>_virtual.txt  # tree/virtual 模式
```

- 主结果文件：`.../<查询子目录>/weights.txt`
  - 每个查询一行：`耗时(秒) 边权和`
  - 无解时边权和为 `-1`
  - 每个查询结束后立即追加写入，程序中途终止时已完成查询仍会保留
  - 文件不会在新运行开始时清空；若文件已有内容，会先空两行并写入本次运行信息（含 `run_subdir`、`query_file`），再追加结果
- 终端输出：每个查询结束后打印用时与边权；结束时打印 `weights.txt` 与统计文件路径
- 当 `output_mode` 为 `tree` 或 `virtual` 且有解时，树结构写在同一 `<查询子目录>/` 下
- Half_DPBF / Test1–Test12 的统计文件（每个查询结束立即追加一行）：
  - `.../<查询子目录>/<方法名小写>_stats.txt`（如 `test8_stats.txt`、`half_dpbf_stats.txt`）
  - 每行含：`valid_total`，以及 `k=g/4+1..g/2` 的 `valid/total`
- Test2 统计示例：`.../Test2/query_g10/test2_stats.txt`
  - 每行含：`valid_total`，以及 `k=1..g/2` 的 `valid/total`
- 各 Test 方法的统计文件同样不会在新运行开始时清空；每次运行开始会追加运行信息作为分隔。

## 6. 方法输出比较脚本

新增脚本：`compare_method_output.py`  
用于比较同图、同输出形式、同查询子目录下两个方法 `weights.txt` 的边权列是否一致（允许误差）。

示例：

```powershell
python .\compare_method_output.py --graph Toronto --mode virtual --method-a DPBF --method-b Half_DPBF --base-dir result --tol 1e-6
```

返回码：

- `0`：一致
- `1`：文件缺失或行数不一致
- `2`：有查询超过误差阈值

## 7. 数据文件要求

每个图目录（`data` 或 `data_new` 下的一个子目录）需要包含：

- `Graph.txt`（或 `graph.txt`）
- `Query.txt`（或 `query.txt`）
- 可选的更多查询文件，例如 `query_g10.txt`、`query_g11.txt`
- `data_new` 当前使用 `query_g4_uniform.txt`、`query_g4_nonuniform.txt` 这类命名，运行时可用 `g4_uniform`、`g4_nonuniform` 简写

程序会自动兼容大小写文件名。

## 8. 常见问题

- 如果构建目录有权限/锁文件问题，可删除 `build` 后重新配置：

```powershell
Remove-Item -Recurse -Force .\build
cmake -S . -B build
cmake --build build --config Release
```

