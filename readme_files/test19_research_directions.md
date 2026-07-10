# ReleaseV2 之后的研究路线

更新时间：2026-07-10。ReleaseV2 已完成单路径发行重写；本文只维护仍未解决的问题、已排除方向和下一次进入代码的门槛。当前算法见 `release_v2.md`，full 原始计数见 `release_v2_evidence.md`。

## 1. 当前状态

| component | role | evidence |
| --- | --- | --- |
| ReleaseV1 | small `g` 稳健发行版 | small35 无单条反噬；fast 时间/solver 增量空间平均优于 PrunedDP 一个数量级 |
| ReleaseV2 | 当前大 `g` 发行版 | final fast20 `12.806s`；precursor full DBLP g13 `1427.625s / 9.015GiB` |
| Test19 | half-DP 研究对照 | exact reductions + group TSP/2；不包装 ReleaseV2 |
| distance/dual probes | ReleaseV2 前置证据 | 说明 row recurrence 瓶颈与 directed-cut 势函数来源 |

旧文档中的“full 未完成”只描述各自冻结的 row solver 或 dual 接入前原型，不代表当前发行状态。

## 2. 已解决的主瓶颈

ReleaseV2 通过三个统一结构改变了 full 行为：

1. 任意必达组作为 anchor，把 mask universe 降为其余 `g-1` 组；
2. 全局 Dijkstra-Steiner labels 不再逐层物化所有 pair/triple rows；
3. directed-cut future potential 与 generated root-star witness 同时加强 lower/upper 两端。

full 只定型 label universe 的 `0.5592%`，open frontier 在 `61.729M` 达峰后下降，并由 `min key >= best` 精确停止。这里没有普通图/query 压缩或固定 `g`/层级特判。

## 3. 尚未解决的问题

1. **统一的小 g 成本判据。** ReleaseV2 small 总体快于 PrunedDP `3.58x`，但 g3--g6 汇总退化。不能加入 `g>=7` 或数据集分支；若设计自动 portfolio，gate 必须由可证明的预处理/recurrence 工作量给出，并把 gate 成本计入总时间。
2. **frontier 空间。** full peak 已从旧 anchored 的 `23.27GB` 降到 `9.015GiB`，但 label hash、lazy heap 与 disjoint index 仍重复保存 mask/key。只考虑无损表示，不降低 double 精度、不按 density 特判。
3. **anchor 的输入结构选择。** 任意组都精确，ReleaseV2 确定性取第一组。可以研究无需试跑多个 solver 的理论选择规则，但必须跨数据验证，并报告选择开销。
4. **lower-bound 查询的形式化核算。** TSP endpoint 扫描与 dual subset sum 是参数多项式开销；继续优化时要保持项目的显式 `O(3^g n + 2^g((g+log n)n+m))` 口径，而不是只写 `O*`。

## 4. 已排除方向

| direction | decisive evidence | record |
| --- | --- | --- |
| old anchored entry wrapper | full 约 `23.27GB` 且未完成 | `archive/test19_probe_archive_20260710.md` |
| replacement cap / exact-pair future | DBLP pair states 删除 `0` | `archive/group_replacement_future_bound_probe_20260710.md` |
| representative degree / second-order path | DBLP g12 删除 `0`，预处理重 | 同上 |
| pair predecessor production row | full pair bytes 缩小 `3.226x`，但 fast 慢 `38.5%` | `archive/test20_pair_forest_prototype_20260710.md` |
| TSP endpoint witness row | bytes 缩小 `1.54x--1.69x`，但 fast 慢 `32.4%` | `archive/tsp_witness_row_probe_20260710.md` |
| second anchor dual / path-union completion | 收益不跨数据或 full early trajectory 不变 | `release_v2_evidence.md` |

这些机制不以开关形式留在 ReleaseV2，也不因单个数据集的局部正信号恢复。

## 5. 下一步门槛

1. 先给统一证明或结构探针，再改发行代码。
2. 不使用数据集名、固定 `g`、固定层、状态密度、运行时间或完成进度特判。
3. 不把 baseline 同样可做的图/query 压缩计作方法收益。
4. 随机小图与 DPBF 在 `1e-6` 内一致，并比较 Toronto 现有最后一次 DPBF run。
5. small 必须逐 `g` 报告重预处理负例；fast 必须跨五个数据版本报告时间与 solver 增量空间。
6. 只有改变状态/空间数量级或给出重要新结构信息时，才重复 full DBLP g13。
7. 使用 Release/O2，运行后清理临时目录、空结果与残留进程。

## 6. 论文关系

- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：提供 Dijkstra-Steiner label-setting、future cost 和 consistency/splice 理论；ReleaseV2 将 terminal subsets 适配为 GST group subsets。
- [DS*](https://arxiv.org/abs/2011.04593)：说明一般 admissible lower bounds 的精确搜索背景；ReleaseV2 没有实现 DS* reopen，而使用更强的 consistent/splice 路径。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：提供 directed-cut dual-ascent 原始路线；ReleaseV2 的组汇点 potentials 与 subset splice 是本仓库适配。
- [Improved Algorithms for the Steiner Problem in Networks](https://www.sciencedirect.com/science/article/pii/S0166218X0000319X)：说明 dual ascent/reduced costs 在 exact Steiner solver 中的历史位置；ReleaseV2 不复制该文的完整 branch-and-cut 框架。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：当前 GST 主 baseline；ReleaseV2 不把 baseline 可共享的普通压缩算作自身贡献。
