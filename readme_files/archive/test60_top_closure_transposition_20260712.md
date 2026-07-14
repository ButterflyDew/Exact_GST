# Test60：Top-Layer Closure Transposition

更新时间：2026-07-12。Test60 利用 metric closure 的全局最小值交换恒等式，把最高 ordinary half row 的图闭包移到 anchor side，目标是删除只供最终完成读取的 `D_h` rooted states。机制随机 `500/500` 精确，并在 `k=2h-1` 的 fast g12 上减少总时间；但 DBLP g13 同属的 `k=2h` 情形需要额外 A-half generators，Toronto full 从 `20.788s` 退化到 `25.121s`，因此全部代码与统计字段撤回，不触发 full DBLP。

## 1. 闭包交换恒等式

对图度量闭包

```text
C(f)(v) = min_x f(x) + dist(x,v)
```

有：

```text
min_v C(f)(v) + g(v) = min_x f(x) + C(g)(x).
```

若 `A=C(A)` 已经是闭包 row，而 top ordinary row 写成 `D_h=C(M_h)`，其中 `M_h` 是同根 split generator，则：

```text
min_v A(v) + D_h(v) = min_v A(v) + M_h(v).
```

因此在最终全局 `min` 的一侧，`D_h` 可以只保留 generator，不运行图传播。这正是 `test21_high_row_generators.md` 中“generator 只对最后一个全局 min 侧安全”的具体 half-state 实现；它没有错误地交换 row-level closure。

## 2. 奇偶边界

令 nonanchor 数为 `k`，`h=ceil(k/2)`。

### 2.1 `k=2h-1`

任意 top mask `S` 的补集大小为 `h-1`。现有闭包 A row `A(U-S)` 已可直接与 `M_h(S)` 做离线有序交集，所以全部 `D_h` closure 都可删除。

### 2.2 `k=2h`

top masks 两两互补且都为 `h`。现有 A 只生成到 `h-1`，所以必须补 A-half rows。为避免两侧同时构造 `D_h`，每个互补 pair 只按 mask 顺序闭包一侧 `C`，另一侧 `N` 保留 generator，并完成：

```text
A_minus(N) + D(C)
A(C)       + M(N).
```

`A_minus` 只缺 `anchor + D_h` 这一种整块 seed；缺失候选可在互补方向由完整 `A(C)+M(N)` 重现。规则由互补 half 结构推出，不依赖数据集、`g` 值、密度或 wall time。

## 3. 正确性

实现仅改变 top layer：lower D、dual、branch basis、三/四块 upper 和有序 row 表示均不变。随机 DPBF 对拍：

```text
seed          713251
iterations    500/500
n             4..14
g             2..13
tolerance     1e-6
build         Release/O2
```

## 4. Fast20

候选快照：`result_snapshot/fast/20260712_223047`。

| g | formal total | Test60 total | change | top closure rule |
| ---: | ---: | ---: | ---: | --- |
| 9 | `0.830s` | `0.947s` | `+14.0%` | half D closures + A-half |
| 10 | `1.619s` | `1.692s` | `+4.5%` | no D_h closures |
| 11 | `2.576s` | `2.860s` | `+11.0%` | half D closures + A-half |
| 12 | `7.279s` | `6.999s` | `-3.8%` | no D_h closures |
| **total** | **`12.304s` stats** | **`12.495s` stats** | **`+1.6%`** | |

snapshot wall/query summary 的对应总数为正式 `12.380s`、Test60 `12.576s`。`k=2h-1` 的全删除有正信号，但 `k=2h` 新增 A-half 的成本跨库占优。

## 5. Toronto Full g13

DBLP g13 与 Toronto g13 都是 `k=12=2h`，因此 Toronto 是必要的前置门：

| metric | formal Test21 | Test60 |
| --- | ---: | ---: |
| weight | `0.7048467020` | `0.7048467020` |
| wall | `20.787524s` | `25.120947s` |
| peak RSS | `102.949MiB` | `105.293MiB` |
| D6 values | `223,812` | `124,010` |
| D6 pops | `312,399` | `172,447` |
| anchored merge probes | `43,398,633` | `73,418,298` |
| ordinary phase | `13.979s` | `14.963s` |
| anchored phase | `6.433s` | `9.744s` |

top D6 rooted payload确实减少约 `45%`，但 split generator 本身仍须全部构造，D6 phase 没有变快；额外 A6 masks 又增加约 `30.0M` merge probes。故该接口在与 DBLP 相同的结构类上明确退化，不运行 DBLP。

## 6. 结论

closure transposition 是有效的状态恒等式，并证明 `k=2h-1` 时最高 D closure 可以完全删除；但 `k=2h` 的 anchor 方向不对称不能免费消失。显式补齐 A-half rows 会把节省转成更大的 subset-merge 成本。

下一候选若继续使用该恒等式，必须把互补 A-half generator 也因子化，不能逐 mask 构造 A6；或者直接设计三函数的最终 completion operator。只按奇偶保留快的一支虽然理论合法，但不会改善 DBLP g13，不能作为当前主目标的完成。

撤回后正式 Test21 已重新通过 Release/O2 编译与随机 DPBF 对拍 `100/100`（seed `713261`）；top generator、A-half、统计字段和完成接口均未保留。

Test60 没有新增论文引用。metric closure 与 rooted subset recurrence 的背景仍来自 Dreyfus--Wagner；本轮贡献仅是仓库内的 half-layer 转置与互补方向分析，不宣称论文级原创性。
