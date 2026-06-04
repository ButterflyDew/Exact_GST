# GST Framework 项目总览（面向下游 LLM）

本文是一份单文件项目上下文说明，目标是让后续 LLM 只阅读本文件，就能理解该仓库要解决的问题、工程结构、运行方式、输出约定，以及当前主要算法线索。

## 1. 项目目标

本项目研究无向非负边权图上的组斯坦纳树（Group Steiner Tree, GST）精确求解。

给定：

- 一个无向图 `G=(V,E)`，每条边有非负权重。
- 一个查询 `Q`，包含 `g` 个组：`group_0, group_1, ..., group_{g-1}`。
- 每个组是一批候选顶点，解只需要从每个组中至少选中一个顶点。

目标：

```text
找到一棵连通子树，使其至少包含每个组中的一个候选顶点，并使边权和最小。
```

若不存在某个连通分量同时覆盖所有组，则该查询无解，输出权重 `-1`。

本项目侧重精确算法及其剪枝实验，不是近似算法框架。当前主线实验版本是 `Test8`，核心约束通常按 `g <= 22` 设计；基础 `DPBF` / `Half_DPBF` 仍保留较朴素的 bitmask DP 结构。

## 2. 核心状态与基本思想

多数算法都围绕以下状态展开：

```text
dp[mask][v] = 覆盖 mask 中所有组，并以 v 作为当前连接点/根的最小连通子树代价
```

其中 `mask` 是组集合的 bitmask，`v` 是图上的顶点。

基本转移有两类：

```text
同根合并: dp[a | b][v] = min(dp[a | b][v], dp[a][v] + dp[b][v])
图上扩展: dp[mask][to] = min(dp[mask][to], dp[mask][v] + w(v,to))
```

`DPBF` 会显式处理接近全部 mask。后续 `Half_DPBF`、`Test5` 到 `Test8` 利用“半集合”性质：完整解可在某个根处由两侧合并而来，其中至少一侧覆盖的组数不超过 `g/2`。因此实验版本主要显式扩展 `|mask| <= floor(g/2)` 的状态，再通过同根合并更新全集 `U` 的候选答案。

## 3. 目录结构

关键文件和目录：

```text
GST_Framework/
  CMakeLists.txt
  RUN.md
  main.cpp
  graph_io.h/.cpp
  query_io.h/.cpp
  query_feasibility.h/.cpp
  answer_tree.h/.cpp
  output_manager.h/.cpp
  output_naming.h/.cpp
  compare_method_output.py
  data/
    <GraphName>/
      graph.txt 或 Graph.txt
      query.txt 或 Query.txt
      query_g10.txt 等可选查询文件
  data_new/
    <GraphName>/
      graph.txt
      query_g4_uniform.txt / query_g4_nonuniform.txt 等新查询文件
  methods/
    DPBF/
      dpbf_solver.h/.cpp
    Half_DPBF/
      half_dpbf_solver.h/.cpp
    Test/
      test1.h/.cpp ... test8.h/.cpp
  readme_files/
    test6_algorithm.md
    test7_algorithm.md
    test8_algorithm.md
    project_overview_for_llm.md
```

各模块职责：

- `main.cpp`：统一入口，解析运行参数，按编译目标选择算法，逐查询运行并写结果。
- `graph_io.*`：选择图目录，读取 `graph.txt` / `Graph.txt`。
- `query_io.*`：解析 `query.txt`、`query_g10.txt` 等查询文件。
- `query_feasibility.*`：基于连通分量快速判断查询是否可行。
- `answer_tree.*`：定义权重输出、具体树输出、虚树输出。
- `output_manager.*`：负责结果、统计和树文件的追加写入。
- `output_naming.*`：把查询文件映射到输出子目录，并定义 `weights.txt` 与统计文件名。
- `methods/*`：各算法实现。
- `readme_files/*_algorithm.md`：Test6/Test7/Test8 的细节说明和正确性解释。

## 4. 数据格式

### 4.1 图文件

图文件位于 `data/<图名>/graph.txt` 或 `Graph.txt`。

格式：

```text
n m
u_1 v_1 w_1
u_2 v_2 w_2
...
u_m v_m w_m
```

约定：

- 顶点编号为 `1..n`。
- 图是无向图，读入后会建立双向邻接表。
- 边权 `w` 必须非负。

### 4.2 查询文件

查询文件位于同一图目录下，默认是 `query.txt` 或 `Query.txt`，也可用 `query_g10.txt`、`query_g11.txt` 等。

格式：

```text
q
g
s_1 v_1 v_2 ... v_s1
s_2 ...
...
s_g ...
```

其中：

- `q` 是查询条数。
- 每个查询先给出组数 `g`。
- 随后 `g` 行分别给出一个组：先是组大小 `s`，再列出该组候选顶点。

`query_selector` 支持多种写法：

```text
g10
query_g10
query_g10.txt
g10_uniform
query_g10_uniform
query_g10_uniform.txt
```

前三种会解析到 `data/<图名>/query_g10.txt`，后三种会解析到 `data/<图名>/query_g10_uniform.txt`。
若运行参数指定 `data_root=data_new`，则从 `data_new/<图名>/` 下按同样规则解析。

## 5. 构建与运行

环境要求：

- CMake >= 3.16
- C++17 编译器
- Windows 上可使用 Visual Studio 2022；`CMakeLists.txt` 已在 MSVC 下添加 `/utf-8`，避免中文注释编码问题。

构建：

```powershell
cmake -S . -B build
cmake --build build --config Release
```

通用参数：

```text
<exe> [graph_selector] [output_mode] [result_root] [debug_root] [root_policy] [query_selector] [data_root]
```

参数含义：

- `graph_selector`：图名、排序后的 1-based 下标、直接图目录路径，或空值。空值优先选择 `<data_root>/example`。
- `output_mode`：`weight`、`tree` / `concrete`、`virtual`。
- `result_root`：结果根目录，默认 `result`。
- `debug_root`：调试根目录，默认 `debug`。
- `root_policy`：虚树选根策略，`child_first` 或 `height_first`。
- `query_selector`：查询文件选择器，默认 `query.txt` / `Query.txt`；`g10_uniform` 会解析到 `query_g10_uniform.txt`。
- `data_root`：数据根目录，默认 `data`；新数据传 `data_new`。

主要可执行文件：

```text
gst_dpbf_main.exe       -> DPBF
gst_half_dpbf_main.exe  -> Half_DPBF
gst_test1_main.exe      -> Test1
...
gst_test8_main.exe      -> Test8
gst_main.exe            -> 兼容旧命名，等价于 DPBF
```

示例：

```powershell
.\build\Release\gst_test8_main.exe Toronto weight result debug child_first
.\build\Release\gst_test8_main.exe DBLP weight result debug child_first g10
.\build\Release\gst_test8_main.exe Toronto weight result debug child_first g10_uniform data_new
.\build\Release\gst_half_dpbf_main.exe Toronto virtual result debug height_first
```

## 6. 输出约定

结果按查询文件分子目录，避免不同 `query_g**` 互相覆盖。

映射规则：

```text
query.txt / Query.txt -> default
query_g10.txt         -> query_g10
query_g11.txt         -> query_g11
query_g10_uniform.txt -> query_g10_uniform
```

目录结构：

```text
<result_root>/<输出形式>/<图名>/<方法名>/<查询子目录>/
  weights.txt
  <method>_stats.txt
  query_<编号>_tree.txt
  query_<编号>_virtual.txt
```

例子：

```text
result/weight/Toronto/Test8/default/weights.txt
result/weight/Toronto/Test8/query_g10/weights.txt
result/weight/Toronto/Test8/query_g10/test8_stats.txt
```

`weights.txt` 每个查询一行：

```text
耗时秒数 最优边权
```

如果无解，第二列为 `-1`。

输出文件采用追加模式：

- 每个查询结束后立刻追加一行，长跑中断时已完成查询不会丢失。
- 新运行不会清空旧文件。
- 如果文件已有内容，会先空两行，再写入 `# Run started=...` 运行头信息。
- 运行头包含 `graph`、`method`、`mode`、`query_file`、`run_subdir`、`query_count`、`result_dir`。

若 `output_mode` 为 `tree` 或 `virtual`，且查询有解，会额外写：

```text
query_<查询编号>_tree.txt
query_<查询编号>_virtual.txt
```

统计文件名由方法名决定，例如：

```text
half_dpbf_stats.txt
test1_stats.txt
test8_stats.txt
```

`compare_method_output.py` 用于比较两个方法在同一图、同一输出模式、同一查询子目录下的 `weights.txt` 权重列是否一致：

```powershell
python .\compare_method_output.py --graph Toronto --mode weight --method-a Test7 --method-b Test8 --query-run query_g10
```

## 7. 输出模式与答案树

`OutputMode` 定义在 `methods/DPBF/dpbf_solver.h`：

```text
weight       -> 只输出最优权重
tree         -> 输出具体树边集
concrete     -> tree 的别名
virtual      -> 输出压缩后的虚树
```

具体树 `ConcreteAnswerTree` 包含：

- `total_weight`
- `tree_edges`
- `selected_vertex_per_group`

虚树 `VirtualTreeAnswer` 会从具体树压缩得到，只保留关键点：

- 度不等于 2 的点
- 被某组选择的终端点

虚树选根有两种策略：

```text
child_first  -> 先最小化根的最大孩子子树叶子数，再最小化树高
height_first -> 先最小化树高，再最小化根的最大孩子子树叶子数
```

## 8. 算法版本概览

### 8.1 DPBF

文件：`methods/DPBF/dpbf_solver.cpp`

这是基础 Dreyfus-Wagner / DPBF 风格实现。

流程：

1. 对每个单组 `1<<gi`，把该组候选点设为 `dp=0`。
2. 对固定 mask 在图上跑 Dijkstra，把状态扩展到所有点。
3. 对非单点 mask 枚举子集，同根合并。
4. 再跑 Dijkstra。
5. 从 `dp[full_mask][v]` 中取最小值。

优点是结构清楚、可恢复具体树。缺点是 mask 数量随 `2^g` 增长，很快变慢。

### 8.2 Half_DPBF

文件：`methods/Half_DPBF/half_dpbf_solver.cpp`

核心思路：

- 只计算 `|mask| <= ceil(g/2)` 的小集合 DP。
- 对每个根 `v`，收集该根上 finite 的小集合状态。
- 在固定根上再做集合覆盖式 DP，把多个小集合拼成全集。

统计字段：

```text
stage1_total
ltV
ltV2
ltV4
ltV8
```

它是一次早期半集合尝试，主要用于和基础 DPBF 比较。

### 8.3 Test1 到 Test4

这些是实验迭代版本，主要用于验证状态数统计、可行性判断、堆更新、早停条件等设计。它们仍接入统一入口，输出 `testX_stats.txt`，但当前主要研究线已转向 Test5 到 Test8。

如果后续 LLM 只需要继续当前算法主线，优先阅读：

```text
methods/Test/test5.cpp
methods/Test/test6.cpp
methods/Test/test7.cpp
methods/Test/test8.cpp
readme_files/test6_algorithm.md
readme_files/test7_algorithm.md
readme_files/test8_algorithm.md
```

### 8.4 Test5：半集合 DP 与 `h` 目标剪枝

文件：`methods/Test/test5.cpp`

约束：

```text
g <= 22
H = floor(g/2)
U = (1 << g) - 1
```

Test5 的基本思想：

- 只显式图上扩展 `|mask| <= H` 的状态。
- `Modify(mask,v,w)` 表示状态 `(mask,v)` 被确认。
- 在同根 `v` 上枚举 `t subset (U ^ mask)`，尝试更新 `dp[mask|t][v]`。
- 当 `mask|t == U` 时更新当前上界 `best`。

Test5 还维护：

```text
h[x][v] = 同根合并中观察到的、可作为 x 一侧组成部分的最大已确认子状态代价
```

处理某个 `mask` 的图上 Dijkstra 时，如果：

```text
dp[mask][v] + h[U ^ mask][v] <= best
```

则 `(mask,v)` 是当前必须触达的目标状态。为了保证这些目标的最短路前缀可到达，代码还会把 `dp[mask][v] < mx_used` 的非目标点加入堆。

统计字段主要包括：

```text
valid_total
up_subset
inqueue
按 k 的 valid / total
```

### 8.5 Test6：组距离下界 `LB`

文件：`methods/Test/test6.cpp`

详细说明：`readme_files/test6_algorithm.md`

Test6 在 Test5 基础上增加每组一次多源 Dijkstra：

```text
group_dist[a][v] = dist(v, group_a)
group_pair[a][b] = min_{y in group_b} group_dist[a][y]
```

这不是 APSP，而是 `g` 次 SSSP。

对状态 `(mask,v)`，剩余组 `rem = U ^ mask`，定义：

```text
far(v,rem) = max_{a in rem} group_dist[a][v]
near2(v,rem) = rem 中距离 v 最近两个组的距离和
mst(rem) = 以 group_pair 为边权的 rem 组间 MST 代价
LB(v,rem) = max(far(v,rem), mst(rem)/2 + near2(v,rem)/2)
```

剪枝条件：

```text
dp[mask][v] + LB(v, U ^ mask) > best
```

若成立，状态不可能扩展出优于当前 `best` 的完整解，可跳过入队、出队或边松弛。`LB` 是安全下界，且具备一致性，因此可作为 Dijkstra 的 A* 式 key：

```text
key = dp + LB
```

### 8.6 Test7：root-star 初始上界与稀疏同根合并

文件：`methods/Test/test7.cpp`

详细说明：`readme_files/test7_algorithm.md`

Test7 保留 Test6 的 `LB`，并新增：

```text
root_star = min_v sum_a group_dist[a][v]
```

固定 `v` 时，把 `v` 到每个组最近候选点的最短路并起来，是一个合法可行解。因此 `root_star` 是安全上界，可以在 DP 开始前初始化 `best`。

同根合并方面，Test7 为每个根维护：

```text
root_masks[v] = 当前 dp[*][v] finite 的 mask 列表
```

`Modify(mask,v)` 不再稠密枚举所有 `t subset (U^mask)`，而是只扫描同根上已 finite 的候选 mask，并检查 `t & mask == 0`。这会跳过大量 `dp[t][v] = inf` 的无效转移。

新增统计：

```text
initial_upper
dense_would
up_subset
lb_seed
lb_relax
```

### 8.7 Test8：当前主线实验版本

文件：`methods/Test/test8.cpp`

详细说明：`readme_files/test8_algorithm.md`

Test8 仍保持按 `popcount(mask)` 的外层 DP 顺序，没有把不同大小的状态混入同一个全局堆。

相对 Test7，Test8 主要新增或强化三点。

第一，组件增长上界：

```text
从 root-star 的 best_root 出发
covered = vertex_group_mask[best_root]
while covered != U:
    从当前 selected 组件多源 Dijkstra
    找到最近的未覆盖组节点 x
    加入到组件
```

每轮连接当前组件到一个未覆盖组，因此始终得到合法可行解。该上界通常比 root-star 更紧，能显著增强 `dp + LB > best` 剪枝。

第二，active vertices：

```text
active_vertices[mask] = 当前 dp[mask][v] finite 的所有 v
```

处理某个 `mask` 时只扫描 active 顶点，不再每个 mask 都全图扫描 `1..n`。`LB` 也按需计算。

第三，混合同根合并：

```text
若 root_masks[v].size() 较小:
    扫 root_masks[v]
否则:
    枚举 t subset (U^mask)
```

两种方式枚举到的有效集合相同，都是：

```text
{ t | (t & mask)==0 且 dp[t][v] finite }
```

区别只是访问无效候选的数量。

Test8 统计字段较多，常用诊断含义：

```text
valid_total      被确认并调用 Modify 的状态总数
up_subset        同根合并有效/候选枚举次数
inqueue          Dijkstra 出队有效状态数
merge_scan       扫 root_masks 的检查次数
merge_dense      枚举互补子集的检查次数
active_seed      实际扫描 active 顶点数
full_seed        若全图扫描会访问的顶点数
lb_calls         实际计算 LB 次数
pq_push/pq_pop   堆操作次数
relax_try/ok     边松弛尝试与成功次数
masks            处理过的 mask 数
max_active       单个 mask 最大 active 顶点数
star_ub          root-star 初始上界
greedy_ub        组件增长上界
greedy_pop       组件增长上界阶段堆弹出次数
prep_ms          预处理耗时
dp_ms            DP 主过程耗时
```

## 9. 当前实验观察与瓶颈

已有实验中，慢查询的主要瓶颈不是单一剪枝弱，而是中等大小 `mask` 的状态爆炸：

```text
valid_total
inqueue
up_subset / merge_scan / merge_dense
```

在大图上，全图扫描也会放大成本。Test8 的 `active_vertices` 能显著减少“每个 mask 扫所有顶点”的浪费，但慢查询仍可能受同根合并和中等层状态数量支配。

`greedy_ub` 通常比 `star_ub` 更紧，对部分数据集很有效，因为它能更早降低 `best`，从而增强 `dp + LB > best` 的过滤。

诊断时优先看：

```text
weights.txt       每条查询耗时与权重
test8_stats.txt   每条查询的状态数、合并次数、堆操作、上界和耗时拆分
```

如果某条查询很慢，先比较：

```text
valid_total
up_subset
merge_scan / merge_dense
active_seed / full_seed
lb_calls
prep_ms / dp_ms
star_ub / greedy_ub
按 k 的 valid / total / active / inq / merge
```

## 10. 正确性边界与不变量

重要不变量：

- 图边权必须非负，Dijkstra 依赖该条件。
- `dp[mask][v]` 始终表示覆盖 `mask` 并连通到 `v` 的最小已知代价。
- 单组候选点初始化为 `0`，因为选中该点本身不消耗边权。
- 同根合并只在同一个 `v` 上相加，避免把两个不连通组件错误合并。
- `LB` 必须是完成剩余组所需额外代价的安全下界。
- 初始上界必须来自合法可行解，不能低于最优值。
- Test5-Test8 只显式扩展半侧状态，全集答案通过同根合并维护。
- Test8 仍按 `popcount(mask)` 分层处理，不是全局跨层 label-setting。

如果修改剪枝，必须回答两个问题：

```text
1. 是否可能剪掉某个能形成优于 best 的状态？
2. 是否破坏了 Dijkstra 的非负权/一致 key 前提？
```

如果修改同根合并，必须保证枚举的有效集合仍等价于：

```text
{ t | (t & mask)==0 且 dp[t][v] finite }
```

如果修改输出，必须保留逐查询追加写入的性质，避免长跑中断丢失已完成结果。

## 11. 常见开发任务入口

想改算法：

```text
methods/Test/test8.cpp
methods/Test/test8.h
readme_files/test8_algorithm.md
```

想改运行参数或方法选择：

```text
main.cpp
CMakeLists.txt
RUN.md
```

想改图/查询解析：

```text
graph_io.cpp
query_io.cpp
```

想改输出目录或文件名：

```text
output_manager.cpp
output_naming.cpp
RUN.md
compare_method_output.py
```

想改树结构输出：

```text
answer_tree.cpp
answer_tree.h
methods/DPBF/dpbf_solver.cpp
```

想比较方法结果：

```text
compare_method_output.py
```

## 12. 建议给下游 LLM 的阅读顺序

如果只允许读一个文件，读本文。

如果可以继续读少量文件，建议顺序：

```text
1. RUN.md
2. readme_files/test8_algorithm.md
3. methods/Test/test8.h
4. methods/Test/test8.cpp
5. main.cpp
```

如果任务是证明或调整下界，继续读：

```text
readme_files/test6_algorithm.md
readme_files/test7_algorithm.md
readme_files/test8_algorithm.md
```

如果任务是排查结果是否一致，继续读：

```text
compare_method_output.py
result/<mode>/<graph>/<method>/<query_subdir>/weights.txt
result/<mode>/<graph>/<method>/<query_subdir>/<method>_stats.txt
```

## 13. 当前命名约定速查

常用符号：

```text
g      查询组数
U      全集 mask，(1 << g) - 1
H      floor(g / 2)
mask   当前覆盖的组集合
rem    U ^ mask，剩余组集合
v      当前根/连接点
best   当前已知合法完整解上界
LB     剩余组完成代价下界
```

输出术语：

```text
weights.txt      主结果文件
*_stats.txt      诊断统计文件
default          默认 query.txt 的输出子目录
query_g10        query_g10.txt 的输出子目录
```

算法术语：

```text
root-star 上界      min_v sum_a dist(v, group_a)
greedy 上界         从当前连通组件逐轮连接最近未覆盖组
active_vertices     某 mask 上 dp finite 的顶点列表
root_masks          某根 v 上 dp finite 的 mask 列表
same-root merge     同根合并 dp[a][v] + dp[b][v]
```

