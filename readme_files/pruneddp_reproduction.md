# PrunedDP++ baseline 复现说明

本文说明仓库对 SIGMOD 2016 论文 [Efficient and Progressive Group Steiner Tree Search](Efficient%20and%20Progressive%20Group%20Steiner%20Tree%20Search.pdf) 中 PrunedDP++ 的复现。论文给出了伪代码和复杂度声明，但没有公开本文可获得的源码，也没有规定状态容器、树 witness 的表示或优先队列更新细节。因此，仓库把三个会实质影响时间、空间或正确性的复现选择显式暴露为开关，并在输出中记录实际配置。

默认配置按“尽量贴近论文完整算法”的口径设置为：**Hash 状态、启用逐状态 MST 上界、启用 Algorithm 4 第 31 行的 pathmax 一致性传播**。其中第三项保留论文行为是为了可复现实验口径，并不表示仓库认可其正确性证明；需要正确性更稳健的对照时，应关闭 `lb2_pathmax`。

## 1. 论文中的 PrunedDP++ 主线

对查询组集合 `P`，状态 `(v,X)` 表示一棵以 `v` 为根并覆盖 `X subseteq P` 的树，其已付成本记为 `g(v,X)`。搜索包含两类转移：沿图边把根从 `v` 移到相邻顶点，以及在同一根合并两个不相交的已完成状态。PrunedDP 的 optimal-tree decomposition 和 conditional merging 只扩展 `g(v,X)<best/2` 的状态，并只接受合并成本不超过 `2best/3` 的候选。

PrunedDP++ 在此基础上使用 A* 优先级

```text
f(v,X) = g(v,X) + max(pi_1(v,X), pi_t1(v,X), pi_t2(v,X)).
```

`pi_1` 是到最远未覆盖组的距离。`pi_t1` 和 `pi_t2` 使用 Algorithm 3 预计算的组间最短访问路线 `W(i,j,R)`：前者在起止组对上取最小值，后者在起始组上取最大值。仓库现在严格按 Algorithm 3 的递推初始化 `W(i,i,{i})=0`，每次只向尚未访问的组扩展；旧实现额外插入了空 mask、单端 mask 和“不加入新组”的转移，与原文并不一致，现已删除。

## 2. 不确定性一：稀疏状态如何存储

Algorithm 4 只把 `Q` 写成优先队列、把 `D` 写成已完成状态集合，没有说明 `(mask,v)` 使用 Hash、树结构、分层数组还是完整 `2^k n` 表。论文同时声称 PrunedDP++ 生成的状态更少并使用更少内存；如果无条件开满 `2^k n`，剪掉状态并不会减少主状态表的空间，因此这种实现不能代表论文所强调的稀疏收益。

仓库提供两个语义相同的后端：

| 配置 | 实现 | 主状态空间 |
| --- | --- | --- |
| `hash`，默认 | `unordered_map<(mask,v), StateEntry>`，只在候选首次进入搜索时建立记录 | `O(s)`，`s` 为实际发现状态数 |
| `dense` | 连续 `StateEntry[2^k][n+1]`，用 `present` 区分未发现状态 | `O(2^k n)` |

搜索不遍历 Hash，所有合并仍枚举补集子 mask 并做一次键查找，所以两个后端的状态语义、候选集合和优先级完全一致。组距离 `O(kn)` 与 Algorithm 3 路线表 `O(2^k k^2)` 是论文明确需要的公共预处理，不属于 `(mask,v)` 主状态表。

## 3. 不确定性二：逐状态 MST 可行解

Algorithm 4 第 11--15 行要求每弹出一个状态 `(v,X)` 都执行以下操作：恢复该状态对应的树 `T(v,X)`；对每个未覆盖组恢复一条从 `v` 出发的最短路径；对这些边的并集求 MST；用 MST 权重更新 `best`。这不是简单地把若干距离相加，因为状态树和不同最短路径可能共享边，MST 还会删除并集中的环。

启用 `mst_upper` 时，仓库为每个被优先队列接受的候选建立不可变 witness DAG。边扩展节点记录一条原图边和父 witness，合并节点记录两个 witness。状态弹出后，代码恢复 witness 边与剩余组最短路径边，按 edge id 去重，再用 Kruskal 得到论文要求的真实 MST 上界。关闭 `mst_upper` 时，不构造 witness，也不执行逐状态可行解拼接；`best` 只由完整状态或互补状态合并更新。

论文声称 PrunedDP++ 的最坏复杂度不高于 PrunedDP，即 `O(3^k n + 2^k(n log n+m))`，但其 cost analysis 没有计入 Algorithm 4 第 11--15 行。若第 `i` 个弹出状态的 witness 与补全路径并集包含 `e_i` 条不同边、恢复过程访问 `w_i` 个 witness 节点，则严格实现额外需要

```text
sum_i O(w_i + e_i log e_i)
```

时间，并需要保存 witness DAG。该项不能一般性吸收到论文给出的 DP 上界中。因此文档与结果必须区分“严格 MST 复现时间”和“不执行 MST 的理论主搜索时间”，不能把前者直接标成满足论文原复杂度。

## 4. 不确定性三：`lb_2` 的强制一致性

论文先证明原始 `pi_t2` 是 admissible lower bound，随后明确承认它不一致。正文提出对每条转移传播父下界，Algorithm 4 第 31 行把新候选的总优先级改成

```text
candidate_f = max(raw_g_plus_h, parent_f).
```

这个 pathmax 值是**生成路径相关的候选属性**，不是只由 `(v,X)` 决定的状态函数。同一个 `(v,X)` 从两个父状态到达时可以得到两个不同的传播值；但 Algorithm 4 又只用 `(v,X)` 作为 `Q/D` 的键，第 35 行只在新 `lb` 更小时更新队列，状态进入 `D` 后永久关闭。于是，一个优先级较小但 `g` 较大的候选可能先关闭状态，之后 `g` 更小但传播优先级不更小的候选会被忽略。Lemma 7 把传播后的路径相关值当作统一的 `pi_t2(v,X)`，没有覆盖这一冲突，因而不能推出伪代码实现的状态级正确性。

仓库提供两种明确语义：

| 配置 | 行为 | 正确性口径 |
| --- | --- | --- |
| `lb2_pathmax=on`，默认 | 执行第 31 行 `max`；按论文 `Q/D` 语义永久关闭状态；队列中同键候选只在优先级严格减小时替换 | 严格复现论文，但保留上述正确性不确定性 |
| `lb2_pathmax=off` | 使用原始 admissible 下界；按更小 `g` 更新，并允许已关闭状态 reopen | 标准 inconsistent-A* 的安全实现，作为正确性参考 |

关闭 pathmax 不是关闭 `pi_t2`：`pi_t2` 仍参与 `max(pi_1,pi_t1,pi_t2)`，只是不再把父候选的优先级强行写入后继。reopen 可能使一个状态被展开多次，所以该安全模式也不能机械沿用论文“一状态只弹出一次”的最坏时间分析。

## 5. 实现结构

当前实现位于 `methods/PrunedDP/pruned_dp_solver.cpp`，公开配置位于 `pruned_dp_solver.h`：

```cpp
struct PrunedDpOptions {
    StateStorage state_storage = StateStorage::Hash;
    bool use_mst_upper_bound = true;
    bool enforce_lb2_pathmax = true;
};
```

一次查询依次执行：

1. 对每个组执行多源 Dijkstra，同时保存从任意顶点走向最近组终端的最短路径 witness；
2. 在组间度量上执行 Algorithm 3，得到 `W(i,j,R)` 和 `W(i,R)`；
3. 将每个组终端作为零成本单组状态加入 A* 队列；
4. 弹出状态，按开关更新 MST 上界，再执行互补合并、条件边扩展和条件子集合并；
5. Hash 模式只建立实际发现状态，dense 模式使用同一套更新与弹出逻辑；
6. 队列最小优先级不再小于可行上界时返回 `best`。

`pruneddp_stats.txt` 会记录 `state_storage`、`mst_upper`、`lb2_pathmax`、实际发现状态数、reopen 次数、MST 调用数、MST 改善次数和参与 MST 的边数。`weights.txt` 的 run header 也记录三个开关，防止不同复现口径被误当成同一次实验。

## 6. 构建与运行

Release/O2 构建：

```powershell
cmake --build build --config Release --target gst_pruned_dp_main
```

默认严格复现：

```powershell
.\build\Release\gst_pruned_dp_main.exe Toronto result g10 data 1 5
```

三个开关位于六个公共位置参数之后，可以独立组合：

```powershell
.\build\Release\gst_pruned_dp_main.exe Toronto result g10 data 1 5 `
  --state-storage=dense --mst-upper=off --lb2-pathmax=off
```

| 参数 | 可选值 | 默认值 |
| --- | --- | --- |
| `--state-storage` | `hash` / `dense` | `hash` |
| `--mst-upper` | `on` / `off` | `on` |
| `--lb2-pathmax` | `on` / `off` | `on` |

## 7. 当前验证

所有验证均使用 Release/O2，并以 `1e-6` 与 DPBF 比较：

| 配置 | 随机实例 | 结果 |
| --- | ---: | --- |
| 默认 Hash + MST + pathmax | `350` | 全部一致 |
| dense + MST + pathmax | `150` | 全部一致 |
| Hash + no-MST + pathmax | `150` | 全部一致 |
| Hash + MST + raw `lb_2` reopen | `400` | 全部一致 |
| dense + no-MST + raw `lb_2` reopen | `150` | 全部一致 |

MovieLens 默认查询 q1 另以默认配置和 DPBF 分别完整运行，两者答案均为 `0.0032922542`；PrunedDP++ 实际只发现 `2725` 个 `(mask,v)` 状态。这验证了零权边输入上实现可终止，也直观说明默认 Hash 不应预分配完整 `2^k n` 主状态表。随机通过不能消除论文 pathmax 的理论缺口，默认模式仍应在论文或实验表中标注为“strict-paper reproduction”，关闭 pathmax 的 reopen 模式才作为正确性参考。

## 8. 引用边界

PrunedDP、PrunedDP++、Algorithm 3 的组路线 DP、三类下界、逐状态 MST 可行解、optimal-tree decomposition 和 conditional merging 均来自上述 SIGMOD 2016 原文。Hash/dense 双后端、不可变 witness DAG、开关接口、运行配置记录，以及关闭 pathmax 后按更小 `g` reopen，是本仓库为消除复现歧义所做的工程与正确性处理，不应归因于原论文。
