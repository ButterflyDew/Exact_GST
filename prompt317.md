# GST 项目快速上手 Prompt（317）

你正在维护一个 C++17 项目：`GST_Framework`。  
目标问题是**组斯坦纳树（Group Steiner Tree）**：在无向带权图中找一棵最小权树，使其与每个查询组至少有一个点相交。

## 一、数据格式

`data` 目录下每个子目录表示一个图实例（如 `example`、`Toronto`、`LinkedMDB`），每个实例包含：

- `Graph.txt` / `graph.txt`
  - 第一行：`n m`
  - 接下来 `m` 行：`u v w`（无向边，`w` 为非负实数）
  - 点编号：`1..n`
- `Query.txt` / `query.txt`
  - 第一行：查询数量 `q`
  - 每个查询：
    - 一行：组数 `g`
    - 接下来 `g` 行：`s v1 v2 ... vs`
  - 点编号：`1..n`

## 二、输出要求（通用）

- 每个查询输出一行：`time_in_seconds weight`
- 无解输出 `-1`
- 查询结束时在终端打印时间与权值
- 结果目录结构统一为：
  - `result/<output_mode>/<graph_name>/<method_name>/...`

其中：

- `1.txt`：主结果（每查询一行）
- 若输出结构信息，还会有 `query_x_tree.txt` / `query_x_virtual.txt`
- 部分测试方法会有额外统计文件（同目录）

## 三、项目主要接口（文件级）

- 图与查询读取：
  - `graph_io.h/.cpp`
  - `query_io.h/.cpp`
- 输出管理：
  - `output_manager.h/.cpp`
- 浮点比较：
  - `float_compare.h`
- 答案树抽象：
  - `answer_tree.h/.cpp`
- 查询可行性（无解快速判断）：
  - `query_feasibility.h/.cpp`
- 方法实现（每种方法独立子目录）：
  - `methods/DPBF`
  - `methods/Half_DPBF`
  - `methods/Test`

## 四、运行方式（简版）

编译：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

运行（通用）：

```text
<exe> [graph_selector] [output_mode] [result_root] [debug_root] [root_policy]
```

常用可执行文件：

- `gst_dpbf_main.exe`
- `gst_half_dpbf_main.exe`
- `gst_test1_main.exe`
- `gst_test2_main.exe`
- `gst_main.exe`（兼容旧入口，等价 DPBF）

> 详细示例见 `RUN.md`。

