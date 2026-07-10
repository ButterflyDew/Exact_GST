# Test19 后续研究路线

更新时间：2026-07-10。本文只维护当前状态、未解决问题和进入生产代码的门槛。发行版见 `release_v1.md`；本轮大 `g` 突破见 `dual_anchored_global_labels.md`；旧失败与撤回机制见 `archive/`。

## 1. 当前状态

| component | status | evidence |
| --- | --- | --- |
| ReleaseV1 | 冻结的可审查发行版 | fast 总时间与 solver 增量空间平均优于 PrunedDP 一个数量级；small 35 条无反噬 |
| Test19 | 现有 half-DP 实验基线 | exact reductions、group-tour TSP/2；默认路径自身未跑 full g13 |
| dual-cut distance solver | 已完成阶段使命 | full 900 秒显著削减 pair 层但停在 k3；数据保留在 `dual_cut_potential.md` |
| dual-anchored global labels | 当前大 `g` 主成果，独立原型 | fast20 `13.742s`；full DBLP g13 q1 在 `1427.625s / 9.015GiB` 得到精确值 |
| Test20 | 不存在生产版本 | 旧 pair certificate 原型因 fast 时间退化撤出 |

full DBLP g13 q1 的目标已经由独立 global-label 原型完成。旧文档中的“未完成”只描述各自冻结的 distance solver 或 dual 接入前 anchored 版本，不再代表仓库当前研究进度。

## 2. 本轮突破的含义

突破不是新的普通图压缩，也不是某个 `g` 或层级特判，而是三个问题结构同时生效：

1. 任意必达组作为 anchor，把完整 subset 空间变为其余 `g-1` 组的 rooted recurrence；
2. 全局 Dijkstra-Steiner label-setting 只生成有希望的 `(S,v)`，不逐层物化整个 pair/triple row；
3. directed-cut dual 与生成式 root-star completion 一边证明未来代价，一边尽早给出合法完整上界。

full run 只定型理论 label universe 的 `0.5592%`，open frontier 在峰值 `61.729M` 后下降，并在不生成 size-12 labels 的情况下由 `min key >= best` 精确终止。详细证明和计数统一放在 `dual_anchored_global_labels.md`。

## 3. 尚未解决的问题

1. **生产级小 g gate。** 当前 dual-anchored 原型在 small g4--g6 有 6 条慢于 PrunedDP。不能加 `g>=7` 或数据集分支；需要从可计算的 recurrence/dual 工作量推导统一 gate，或让 ReleaseV1 作为公平 fallback。
2. **生产接口与统计瘦身。** 原型仍同时保留 half、anchored 和 dual-anchored 实验入口及大量诊断字段。若进入新发行版，应重写为单路径可 review 实现，不直接复制探针代码。
3. **frontier 空间。** full peak 已从旧 anchored 的 `23.27GB` 降到 `9.015GiB`，但 root-local hash、heap 和 disjoint index 仍有重复 mask/key。只能做无损表示改进，不能牺牲 double 精度或恢复密度特判。
4. **anchor 的理论选择。** 任意组都精确；当前统一取第一组。可以研究由输入结构推导、无需试跑多个 solver 的 anchor 上界，但必须跨数据验证，且选择成本计入总时间。

## 4. 已排除方向

| direction | decisive evidence | archive/current doc |
| --- | --- | --- |
| old anchored entry wrapper | full 约 `23.27GB` 且未完成 | `archive/test19_probe_archive_20260710.md` |
| replacement cap / exact-pair future | DBLP pair states 删除 `0` | `archive/group_replacement_future_bound_probe_20260710.md` |
| representative degree / second-order path | DBLP g12 删除 `0`；预处理重 | 同上 |
| pair predecessor production row | full pair bytes 缩小 `3.226x`，但 fast 慢 `38.5%` | `archive/test20_pair_forest_prototype_20260710.md` |
| TSP endpoint witness row | bytes 缩小 `1.54x--1.69x`，但 fast 慢 `32.4%` | `archive/tsp_witness_row_probe_20260710.md` |
| second anchor dual / path-union completion | 收益不跨数据或 full 轨迹不变 | `dual_anchored_global_labels.md` |

这些方向不保留在 Release/Test19/global-label hot path，也不因单个数据集的局部正信号重新启用。

## 5. 下一步门槛

1. 先证明机制或输出结构探针，不先堆生产 solver 开关。
2. 不使用数据集名、固定 `g`、固定层、状态密度、运行时间或完成进度特判。
3. 不把 baseline 同样可做的图/query 压缩计作本方法收益。
4. 随机小图与 DPBF 在 `1e-6` 内一致，并比较 Toronto 现有 DPBF 最后一次结果。
5. small 必须明确报告重预处理负例；生产候选需有统一工作量 gate 或公平 fallback。
6. fast 必须给出跨数据时间、权重和独立于公共图的 solver 空间口径。
7. full DBLP g13 已完成；除非有改变状态/空间数量级的重要新机制，不重复启动该长跑。
8. 所有关键构建使用 Release/O2，运行后审计进程并清理临时目录。

## 6. 文献入口

- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：rooted labels、consistent lower bound 与 replacement pruning。
- [DS*](https://arxiv.org/abs/2011.04593)：一般 admissible lower bound 的精确搜索边界。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual ascent。
- [Improved Algorithms for the Steiner Problem in Networks](https://www.sciencedirect.com/science/article/pii/S0166218X0000319X)：exact Steiner 中的 dual/reduced-cost 路线。
- [PrunedDP](https://ronghuali.github.io/paper/sigmod2016gst.pdf)：当前 baseline。
