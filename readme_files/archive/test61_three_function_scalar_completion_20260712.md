# Test61：Three-Function Scalar Half Completion

更新时间：2026-07-12。Test61 修正 Test60 在 `k=2h` 时显式生成两侧 A-half rows 的成本：每个互补 half pair 仍只闭包一侧 ordinary `D_h`，但非 canonical anchor 方向不再构造 A-half row，而直接用 closed-D roots 驱动 `A_X + branch-D_Y + D_h` 三函数标量交集。机制随机 `500/500` 精确，并把 Toronto full 的 Test60 退化回收约 1.28 秒；但仍为 `23.838s`，慢于正式 `20.788s`，故代码撤回，不运行 DBLP。

## 1. 第二个闭包交换

Test60 已有：

```text
D_h = C(M_h)
min C(f) + g = min f + C(g).
```

令非 canonical anchor-half row 为 `A_N=C(Q_N)`，canonical ordinary half `D_C` 已闭包，则：

```text
min_v A_N(v) + D_C(v)
= min_v C(Q_N)(v) + D_C(v)
= min_v Q_N(v) + D_C(v).
```

而 A generator 可展开为：

```text
Q_N(v) = min_{X union Y=N} A_X(v) + branch_D_Y(v).
```

所以该方向只需最终三路 scalar completion，不需要生成或传播 `A_N`。

`Y=N` 的例外是 `anchor + D_N` 整块 seed；它可由互补 canonical 方向 `A_C + M_N` 重现，与 Test60 的方向完备性证明相同。

## 2. 离线实现

对每个互补 pair，mask 较小侧 `C` 保留 closed `D_C` 与完整 `A_C`，另一侧 `N` 只保留 split generator `M_N`。最终执行：

```text
A_C + M_N
min over proper Y subset N:
    A_(N-Y) + branch_D_Y + D_C.
```

第二式以 `D_C` 的有序 sparse roots 为 driver，在 A row 与 branch-D row 中二分查找；没有 Hash、target events 或经验阈值。所有规则由互补 half orientation 推出。

## 3. 正确性与 Fast20

随机 DPBF：

```text
seed          713271
iterations    500/500
n             4..14
g             2..13
tolerance     1e-6
build         Release/O2
```

候选快照：`result_snapshot/fast/20260712_224545`。

| metric | formal | Test60 | Test61 |
| --- | ---: | ---: | ---: |
| query summary | `12.380s` | `12.576s` | `12.616s` |
| stats total | `12.304s` | `12.497s` | `12.536s` |
| anchored merge probes | `40.41M` | `51.58M` | `48.18M` |
| direct three-way probes | `0` | `0` | `3.57M` |
| top completion | `0` | `4ms` | `213ms` |

显式 A-half merges 减少，但二分三路查询补回了相近工作；fast20 仍慢于正式约 `1.9%`。

## 4. Toronto Full g13

| metric | formal Test21 | Test60 | Test61 |
| --- | ---: | ---: | ---: |
| weight | `0.7048467020` | `0.7048467020` | `0.7048467020` |
| wall | `20.787524s` | `25.120947s` | `23.838044s` |
| peak RSS | `102.949MiB` | `105.293MiB` | `105.090MiB` |
| D6 values | `223,812` | `124,010` | `124,010` |
| D6 pops | `312,399` | `172,447` | `172,447` |
| anchored masks | `1,586` | `2,510` | `2,048` |
| anchored merge probes | `43.40M` | `73.42M` | `60.34M` |
| direct three-way probes | `0` | `0` | `7.65M` |
| anchored phase | `6.433s` | `9.744s` | `8.606s` |
| top scalar completion | `0` | `0.001s` | `0.812s` |

Test61 证明非 canonical A-half 可以完全消掉，但 canonical `A_C=C(Q_C)` 仍新增约 `16.9M` merges；同时三路 lookup 需 `7.65M` probes。两者超过少做一半 D6 propagation 的收益。

## 5. 结论

当前 top-half 代数已经收敛到单一剩余项：

```text
min C(Q_C) + M_N.
```

非 canonical `min C(Q_N)+D_C` 已被精确压成三块 scalar completion。下一候选若沿此方向，必须因子化 canonical `Q_C` 或构造直接求 `min C(Q_C)+M_N` 的 target operator；继续优化三路 lookup、调整 canonical mask 顺序或只保留 `k=2h-1` 快支都不能解决 DBLP g13 主目标。

撤回后正式 Test21 已通过规范化 PATH 的 Release/O2 编译和随机 DPBF 对拍 `100/100`（seed `713281`）；三函数 helper、top rows、统计字段与完成接口均未保留。

Test61 没有新增论文引用。metric closure 与 rooted subset recurrence 背景仍来自 Dreyfus--Wagner；三函数 scalar half completion 是本轮仓库候选，尚不宣称论文级原创性。
