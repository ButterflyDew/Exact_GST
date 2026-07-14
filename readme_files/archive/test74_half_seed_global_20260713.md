# Test74：Half-Row Macro Seeds for Global Search

更新时间：2026-07-13。Test74 检查 ReleaseV3 切换时是否浪费了已支付的 ordered half rows：在 global label search 的 singleton seeds 之后，把切换前仍保留的 exact nonanchor half-row values 作为宏种子注入，再立即释放 row storage。候选随机精确，但 fast20 的 settled frontier 与 ReleaseV3 逐条完全相同，只增加 created labels 和时间，因此源码、CLI、CMake 与 runner 已撤回。

## 1. 机制

ReleaseV3 在 measured row work 足以购买 dual/global 时切换。正式版释放所有 rows，global 从 singleton terminals 开始。Test74 在相同切换事件中：

1. 选择与正式版相同的 farthest permanent anchor；
2. 只读取不含 anchor group、且仍标记为 ready 的 exact rows；
3. 对每个 `(mask,root,cost)` 调用正式 global `Relax`；
4. 由同一 admissible lower 和 incumbent 决定是否创建 label；
5. 注入结束立即 `swap` 释放 rows，再运行原 global recurrence。

宏种子都是已存在的真实 partial trees，不改变答案条件。没有新增 DP 层、Hash、数据集判断、固定 `g`、层级或 wall-time 特判。

## 2. 正确性

加入额外合法初始 labels 只为原 label graph 提供已知路径的缩写，不删除 singleton 路径或任何 merge/edge transition。global 仍在最小 open key 不小于 incumbent 时停止，所以答案与正式 ReleaseV3 相同。

Release/O2 随机对拍：

```text
seed          713641
iterations    300/300
n             4..14
g             2..10
tolerance     1e-6
```

## 3. Fast20

候选快照：`result_snapshot/fast/20260713_050314`。基线为 ReleaseV3 `result_snapshot/fast/20260711_230000`。

| metric | ReleaseV3 | Test74 |
| --- | ---: | ---: |
| query wall total | `12.509s` | `13.176s` |
| stats total | `12.454s` | `13.119s` |
| global settled | `1,939,448` | `1,939,448` |
| global created | `2,710,638` | `2,726,230` |

每条查询的 settled labels 均严格相同，不只是汇总相同。宏种子扫描后有些直接创建较大 mask labels，但这些 labels 要么本来就会由 singleton recurrence 创建并 settle，要么成为额外 open/stale 项；它们没有让任何正式 label 越过停止边界。候选总时间慢 `5.3%`，created 多 `15,592`。

## 4. 结论

ReleaseV3 切换前的 half rows 对 global frontier 没有额外信息：它们只是原 recurrence 的已知路径缩写，而不是更强 lower、不同状态语义或 paid-attachment certificate。A 的贡献不能通过“把 row values 塞回 B heap”获得；下一机制必须让 half state 携带 global labels 无法由 singleton 成本重建的结构信息，或改变 completion/state dominance。

本轮没有采用新的论文算法，也没有新增论文引用。由于 fast settled frontier 完全不变，不运行 Toronto full 或 DBLP g13。
