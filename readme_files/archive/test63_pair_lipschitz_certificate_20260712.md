# Test63：Pair Split 1-Lipschitz Certificate

更新时间：2026-07-12。Test63 检查能否在运行 D2 shortest-path closure 前整张删除已经 metric-closed 的 pair split rows。证书只需 group distances 与一次图边扫描；五库 fast g12 的所有 `275/275` nonanchor pairs 均失败，因此临时 probe 模式与二进制撤回，不触发 full DBLP。

## 1. Exact 删除条件

对 pair `{i,j}` 定义 split function：

```text
f_ij(v) = gd_i(v) + gd_j(v).
```

ordinary pair row 是 metric closure：

```text
D_ij(v) = C(f_ij)(v) = min_x f_ij(x) + dist(x,v).
```

若对每条无向边 `{u,v}` 都有：

```text
|f_ij(u)-f_ij(v)| <= w(u,v),
```

则 `f_ij` 已是 1-Lipschitz，故 `C(f_ij)=f_ij`。整张 D2 row 没有严格优于 split seed 的 root-irreducible values，不会作为 branch 被 D/A recurrence 读取，可以完全删除且不损失精确性。

对一条边，令 group gradient 为 `delta_i=gd_i(u)-gd_i(v)`，则只需检查：

```text
|delta_i + delta_j| <= w.
```

一次边扫描可同时标记全部 pairs；没有 Dijkstra、Hash、数据集阈值或经验参数。

## 2. Fast 五库结果

所有运行使用 Release/O2、g12 query 1、与 Test21 相同的 root-star/farthest anchor，容差使用仓库统一 `1e-9` 比较：

| dataset | nonanchor pairs | certified closed | min violation edges | max violation edges |
| --- | ---: | ---: | ---: | ---: |
| Toronto | 55 | 0 | 2,679 | 3,402 |
| Toronto-new | 55 | 0 | 2,528 | 4,305 |
| DBLP | 55 | 0 | 6,223 | 18,519 |
| DBLP-new | 55 | 0 | 4,747 | 10,314 |
| MovieLens | 55 | 0 | 145,113 | 147,746 |

不是少数浮点边界导致证书失败：每个 pair 都有数千到十余万条严格 violation edges。因而没有一张 D2 row 能以该条件删除，也没有必要运行 full 版本。

## 3. 结论

`gd_i+gd_j` 是 2-Lipschitz，但实际数据上从未退化成 1-Lipschitz。D2 closure 的作用不是少数 pair 的例外修正，而是所有 pairs 的普遍必要步骤。

后续不得继续尝试按整张 pair row 的 global Lipschitz certificate 删除 D2。仍可能存在局部 cone/source dominance，但 Test38 已证明它只减少 heap 工作、不减少 retained output 数量；下一机制必须跨 pair/root/consumer 共享状态或改变状态定义。

Test63 没有新增论文引用。metric closure 条件是直接图度量推论；临时 `--lipschitz` 模式、统计输出和生成二进制均不属于正式实现。
