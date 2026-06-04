# task317：本窗口任务回顾记录

本文档记录本会话中已完成的主要开发任务与关键结果。

## 1) 代码风格与注释

- 将核心代码统一为：
  - 左大括号换行
  - 4 空格缩进
- 在核心逻辑处补充必要中文注释（DP、虚树构建、I/O 等）。

## 2) 编译报错排查与修复（MSVC 编码）

- 发现并修复 MSVC 对中文注释的编码误判问题。
- 在 `CMakeLists.txt` 增加 `/utf-8`（MSVC）编译选项，消除 C4819 引发的连锁语法假错误。

## 3) 运行文档

- 新增并多次更新 `RUN.md`，覆盖：
  - 编译命令
  - 参数说明
  - 各方法运行示例
  - 输出目录结构
  - 对比脚本用法

## 4) DPBF 参数与输出目录升级

- 为虚树选根增加策略参数（高度优先 / 子树优先）。
- 输出目录改为分层：
  - `result/<输出形式>/<图名>/<方法名>/...`

## 5) 新增 Half_DPBF 方法

- 新增：
  - `methods/Half_DPBF/half_dpbf_solver.h/.cpp`
- 实现小集合 DP + 枚举根覆盖全集流程。
- 增加统计文件 `half_dpbf_stats.txt`，并实现“每查询结束立即追加写入”。
- 后续补充了统计字段：`stage1_total`（一阶段小集合总数）。

## 6) 新增方法输出对比脚本

- 新增 `compare_method_output.py`：
  - 比较同图同输出模式下不同方法 `1.txt` 的边权结果
  - 支持误差阈值参数

## 7) 新增 Test1 方法

- 新增：
  - `methods/Test/test1.h/.cpp`
- 按测试需求实现 `Test1` 逻辑与统计输出：
  - 输出 `test1_stats.txt`
  - 含总有效状态与按集合大小分桶统计。

## 8) 新增全局可行性判断模块

- 新增：
  - `query_feasibility.h/.cpp`
- 提供 `IsQueryFeasible(graph, query)`，用于快速判断查询是否无解（基于连通分量覆盖所有组）。

## 9) 新增并迭代 Test2 方法

- 新增：
  - `methods/Test/test2.h/.cpp`
- 接入 `main.cpp` 与构建系统，新增 `gst_test2_main.exe`。
- 按反馈多轮修正关键逻辑，包括：
  - 去掉跨方法调用求初值
  - 使用全局可行性判断处理无解
  - 修正堆提前终止问题
  - 修正 `best` 与 `f[i][U]` 关系
  - 修正 `g` 的更新来源与时机
  - 堆支持减小键值并在状态改进时实时入堆/减键
- 现已可正常在 `example` 上得到非 `-1` 结果，并在大图上产生连续统计输出。

## 10) 主程序与构建入口扩展

- `main.cpp` 已支持按编译目标选择方法：
  - `DPBF` / `Half_DPBF` / `Test1` / `Test2`
- `CMakeLists.txt` 新增可执行文件：
  - `gst_dpbf_main`
  - `gst_half_dpbf_main`
  - `gst_test1_main`
  - `gst_test2_main`
  - `gst_main`（兼容旧入口）

