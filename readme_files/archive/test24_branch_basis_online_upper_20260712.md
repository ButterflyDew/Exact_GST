# Test24：分支基与在线上界演化记录

更新时间：2026-07-12。Test24 是已经合入 Test21 并删除独立源码入口的结构原型。当前算法、证明和可引用结果见 `../test21_anchor_half.md`；本文只保存演化数据和 DBLP 否决边界。

## 1. 根不可拆分 branch basis

若 `D(S,v)` 等于某个同根 split `D(X,v)+D(S-X,v)`，则它作为 A 主干的新分支是冗余的：先接 X、再接 S-X 可得到不更差候选。最终只需 singleton 和严格优于所有同根 splits 的 edge-propagated values。

第一版给 branch values 重复保存 `(vertex,value)`，Toronto full 为 `24.326s / 211.3MiB`。随后改为与原 row 对齐的一位资格位，峰值回到 `133.5MiB` 左右。

## 2. pivot ordinary recurrence

每个 D mask 固定最低位为 pivot；含 pivot 的一侧作为累计主干，另一侧只用 branch basis。随机 `g=2..12` 和固定 g13 对拍均通过。

| 版本 | fast20 | Toronto full g13 | peak |
| --- | ---: | ---: | ---: |
| 只限制 A 接枝 | `13.372s` | `24.669s` | `133.5MiB` |
| D pivot + A branch basis | `12.853s` | `22.716s` | `133.5MiB` |

branch basis 减少 merge 候选，但不减少 Dijkstra settled labels；单独使用仍未跨过 DBLP 门槛。

## 3. 在线 D_t 三块结算

旧实现等 `t=ceil((g-1)/3)` 整层完成后再枚举三块 common-root 上界。新实现利用 row 偏序：一个三块分区在最后一张所需 D_t row 完成时恰好首次可用，因此立即结算且仍只结算一次。

| 结果 | pivot branch | 加在线三块 |
| --- | ---: | ---: |
| fast20 | `12.853s` | `12.570s` |
| Toronto full | `22.716s / 133.5MiB` | `20.680s / 120.6MiB` |
| Toronto D4 values | `4.333M` 旧基线 | `3.212M` |

这是第一次同时降低 merge 工作、实际图 labels 和峰值的机制。

## 4. quarter 上界路线

### 4.1 全图 pair envelopes

在 `q=ceil((g-1)/4)` 层，把四个 D blocks 两两压成 pair envelopes，再拼互补 halves。Toronto full 中它把 best 从 `1.1498267817` 降到 `0.9767276789`，D4 values 降到 `1.715M`、峰值降到 `103.3MiB`；但代价是 `83.60M` row probes 和 `1.55s`，净 wall 退到 `22.039s`。Toronto-fast 花 `151ms` 却完全没有改善 best。全图版本已撤回。

四块 witness 再做最多四次 anchor-side lifting，可把 full best 降到 `0.9392565627`，但 wall 仍为 `21.932s`，不能覆盖 envelope 成本。

### 4.2 root-star 标量 partition

最终只在已经确定的 root-star 根读取 D 标量，做四块 subset DP；若 candidate 改善 best，再运行同样的四次 lifting。

Toronto full 中：

```text
scalar checks       21,649 pair + 462 complement
scalar time         15.7ms（包含 lifting 后总 quarter 阶段）
best                1.1498267817 -> 1.0094809942 -> 0.9726035549
D4 values           1.684M
peak                102.9MiB
```

该版本合入 Test21。它不是按数据集选择 root；root-star 根是既有可行上界的确定根。

## 5. DBLP gate

只在以下结构信号出现后运行 gate：

1. branch basis 在 random/fast/Toronto full 正确并正向；
2. 在线 D_t 明确减少 full labels；
3. root-quarter 用极小标量成本进一步减少 full labels。

三轮候选都没有在 ReleaseV3 的 `531.556s` 时间线内完成 DBLP g13 q1，均被终止且没有最终权重。最后的 root-quarter 组合被终止时采样 RSS 约 `4.97GB`。一次中间版 DBLP g12 诊断在十分钟内也未完成，说明大图问题并非只来自 g13 最后一层。

这些运行目录均为无效临时结果，清理后不保留。结论是：上界链已经能显著减少 Toronto labels，但 DBLP 仍需要在图传播前减少 rooted states；继续增加五/六块上界或重排同一批 joins 不再是主线。

## 6. 正确性种子

演化中使用过的主要最终检查：

```text
712431 / 712433  pivot branch basis
712451 / 712453  online D_t
712471 / 712473  quarter witness lifting
712491 / 712493  合入后的正式 Test21
```

前者覆盖随机 `g=2..12`，后者覆盖固定 `g=13`；正式 Test21 分别为 `500/500` 和 `50/50`。
