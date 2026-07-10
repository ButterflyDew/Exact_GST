# Test19：当前算法与实现边界

Test19 是从合规 Test18 复制出的独立实验线。它保留 half-DP、稀疏行、exact reductions 和 Complete 主体，默认新增 exact group-tour TSP/2 ordering 与 persisted save-need。本文只描述当前代码，不记录实验时间线。

## 1. 代码入口

```text
methods/Test/test19.cpp                 查询预处理、half-DP、Complete 和调度
methods/Test/test19_future_bounds.*     group-tour TSP/2 预处理与查询
methods/Test/test19.h                   SolveResult 和 Test19Stats
main.cpp                                Test19 命令行调度与统计输出
CMakeLists.txt                          gst_test19_main 构建目标
```

`test19.cpp` 仍然偏长；后续拆分顺序见 `test19_maintenance.md`。结构重构不得与新算法混在同一轮验证中。

## 2. 主流程

1. 读取一个 group query，执行 Test18 继承的 exact query reductions。
2. 计算点到组距离 `gd`、组间距离 `gp` 和现有 safe lower bound 数据。
3. 构建 `MetricTspLowerBound`，并得到初始可行上界 `best`。
4. 只生成 `|S|<=floor(g/2)` 的 rooted DP rows。
5. 每个 mask 内执行图搜索，并用同根 disjoint rows 做 Steiner merge。
6. 用至多三块 half masks 在线拼接完整答案；final rows 按 need 保存和 compact。

half-DP 的三块存在性证明和公共符号统一放在 `test_series_overview.md`。

## 3. TSP/2 Future Bound

对 root `v` 和未覆盖组集合 `R`，预计算 group-only Hamiltonian path：

```text
path[R][a][b] = 覆盖 R、从组 a 到组 b 的最短 Hamiltonian path
TSP(v,R)      = min gd[a][v] + path[R][a][b] + gd[b][v]
h_tsp(v,R)    = TSP(v,R) / 2
```

任意连接 `v` 与 `R` 中各组的 future tree 翻倍后是一条 closed walk。沿 walk 为每组保留一个实际命中点，再把相邻段替换为 `gd/gp` 的最短边，代价只会下降，所以 `h_tsp` 不超过 future tree 代价。组集合之间的 `gp` 不需要单独满足三角不等式。

root 沿边 `uv` 移动时，tour 的两条 root incident edges 各自至多变化 `w(u,v)`；除以二后有：

```text
h_tsp(u,R) <= w(u,v) + h_tsp(v,R)
```

因此它可进入普通 A* ordering。对 subset 扩展还需要 splice 条件：

```text
h(v,I) <= h(w,J) + OPT(v,w,I-J)
```

将 `(w,J)` 的 group tour 与连接 `v,w,I-J` 的桥接树双倍后在实际点 `w` 拼接，即得该不等式。`future_lb_probe` 使用 DPBF 精确桥接代价做了 `14,080` 次随机检查，TSP/2 为 0 违规。

该构造可追溯到 `Dijkstra meets Steiner` 的 TSP/2 future bound 与 Hamiltonian-path 预计算；Test19 的工作是把它适配到 group-set 与 half-DP 保存语义，不把下界本身声称为原创。

## 4. 默认接入点

| point | behavior |
| --- | --- |
| seed / pop / relax | 使用 `d + max(current_lb, h_tsp)` 排序和安全跳过 |
| final save | 把同一下界写入 row `need` |
| later lookup / compact | 复用持久 need，减少无效状态触达 |

相关开关：

```text
GST_TEST19_TSP_LB_ORDER=0    关闭默认 ordering
GST_TEST19_TSP_LB_SAVE=0     关闭默认 persisted save-need
GST_TEST19_TSP_LB_DIAG=1     只统计潜在收益
GST_TEST19_PROGRESS=1        打印进度
```

两个关闭变量同时设置才是未启用 TSP/2 的 Test18-family 基线路径。`GST_TEST19_ASTAR_ORDER=1` 是旧 current-LB ordering 实验开关；one-tree 开关也仍可用于归档探针复现，但都不属于默认机制。

## 5. 复杂度与合规边界

TSP helper 的预处理为 `O(g^3 2^g)` 时间和 `O(g^2 2^g)` 空间；单次 `TourHalf(v,R)` 查询扫描 endpoint pairs，为 `O(|R|^2)`。整体仍保持目标口径：

```text
O(3^g n + 2^g((g + log n)n + m))
```

当前代码不包含固定数据集、固定组数、固定层级、运行时间点或 row-size 特判。硬编码 k=3/k=4 frontier 已从 Test18/Test19 撤出。普通图压缩若 baseline 同样可用，也不进入本方法主线。

## 6. 验证状态

当前默认、baseline、order-only 和 save-only 均已通过随机小图 DPBF 对拍；五个 fast 数据版本共 20 条查询权重一致。完整可引用数字只放在 `test19_effect_report.md`，失败和降级机制只放在 archive。
