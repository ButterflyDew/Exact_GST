# Test21 Anchor-Half 实验归档（2026-07-11）

本文保存 Test21 从 dense recurrence 原型到当前 ordered-row 实现的试错数据。这里只用于追踪选择过程；当前算法、证明与可引用结果见 `../test21_anchor_half.md`。

所有计时均为 Release/O2。除标明的 snapshot 外，随机对拍使用小随机图和 DPBF，不是 DBLP 长跑。

## 1. Dense recurrence 原型

最初对每个 `D/A` mask 保存完整 `n` 个值，用于隔离状态定义和完备性：

| 检查 | 结果 |
| --- | --- |
| random `g=2..10`, seed `711911` | `500/500` |
| fixed `g=13`, seed `711913` | `60/60` |
| Toronto g8 q1 | `0.168s / 8.7MiB`；ReleaseV3 `0.047s / 6.3MiB` |
| Toronto fast g12 q1 | `3.135s / 72.1MiB`；ReleaseV3 `1.325s / 51.7MiB` |

Toronto g12 中 ordinary `1.562s`、anchored `1.087s`、completion `0.470s`。结论：recurrence 正确，但 dense 物化不适合作为实现。

## 2. 已撤回的 Hash frontier

曾把 `A` 改为跨 mask 的 anchor-rooted Hash label frontier。所有版本权重正确，但性能不稳定，且偏离 A 原本的离线有序列表框架。

| 版本 | Toronto fast g12 | peak | 结论 |
| --- | ---: | ---: | --- |
| sparse A Hash frontier | `10.538s` | `137MiB` | 约 `34.5M` merge、`79.8M` completion，否决 |
| 加 dual / root-star | `9.273s` | `133.8MiB` | 仍远慢 |
| completion cache + farthest anchor | `4.512s` | `94.4MiB` | 明显改善但仍重 |
| ordinary D 也稀疏 | `4.720s` | `73.7MiB` | 空间降、时间退化 |
| root-local flat Hash | `4.506s` | `70.7MiB` | Hash 常数仍高 |
| tentative completion | `4.862s` | - | 正确但更慢，立即撤回 |
| cached lower ordering | `5.099s` | - | 正确但更慢，立即撤回 |

对应正确性种子依次为：

```text
711921 / 711923
711931 / 711933
711941 / 711943
711951 / 711953
711961 / 711963
711971 / 711973
711981 / 711983
```

每组前者覆盖 `g=2..10`，后者固定 `g=13`。这些实现均已从源码删除，不留开关。

## 3. 回到离线有序 rows

撤回 Hash 后，`D` 和 `A` 都改为 mask-major、size-ordered rows；sparse row 按顶点编号排序，merge 使用 dense 扫描或双指针线性归并。

| 阶段 | 正确性 | Toronto fast g12 | peak |
| --- | --- | ---: | ---: |
| ordered D/A 基线 | `1000/1000` seed `711991`; g13 `100/100` seed `711993` | `2.423s` | `27.3MiB` |
| ordinary D 加 farthest/one-label lower | `300/300` seed `712001`; g13 `30/30` seed `712003` | `1.772s` | `19.5MiB` |
| 空 A row 跳过 completion | `500/500` seed `712011`; g13 `50/50` seed `712013` | `1.661s` | `19.4MiB` |

最终 fast20 为 `14.405s`，配对 ReleaseV3 为 `12.509s`；MovieLens 四例 Test21 合计 `3.423s`，ReleaseV3 为 `5.247s`。当前结果和逐库表见主文档。

## 4. 2026-07-12 三块 join 消融

full Toronto g13 使用原图 `n=46073,m=68353`；ReleaseV3 配对结果为 `4.163s / 125.8MiB`。

| 机制 | 正确性 | fast Toronto g12 | full Toronto g13 | 结论 |
| --- | --- | ---: | ---: | --- |
| balanced `min max(D(B),D(C))` lower | 随机通过但缺乏合法 future-cost 证明 | `1.705s` | `42.426s` | 状态显著下降但 pair `max` 抵消；随后因共享主干语义撤回 |
| full A absorption | `300/300`; g13 `30/30` | `2.716s` | `72.444s` | `1.94B` A merge probes，撤回 |
| anchor-first bridge | `300/300`; g13 `30/30` | `2.163s` | `55.478s` | low A 在失去及时 best 后膨胀，撤回 |
| projected exact D lower | **失败** | - | - | seed `712051` iteration 52：exact `39`、got `40`，立即撤回 |
| per-root lazy completion | `500/500`; g13 `50/50` | `4.634s` | `61.207s` | `133.6M` split checks 仍产生大量二分访问，撤回 |
| anchor-filtered three-way join | `500/500` seed `712081`; g13 `50/50` seed `712083` | `1.77s` | `29.805s` | 当前保留 |

projected D 反例说明：即使 `D(P,v)` 是精确 rooted row，也不能直接作为任意 anchored partial tree 的增量 future cost；partial tree 可能已经提供共享主干。该错误机制未留源码或开关。

三路 join 把 full completion 从约 `15.4s` 降到 `5.88s`，fast20 从上一 ordered 版本的 `14.405s` 降到 `13.569s`。但 full D5/D6 仍耗时约 `12.9s`，因此后续方向转到 `../test21_high_row_generators.md`。

## 5. 四块上界与 witness lifting

令 `t=ceil((g-1)/3)`。在 D_t 完成后枚举 `gd(anchor)+D(X)+D(Y)+D(Z)`，并保存最佳三块 witness。只对 witness 的三种 anchor-side 选择运行临时闭包，是当前保留机制。

| 版本 | full Toronto g13 | peak | 结论 |
| --- | ---: | ---: | --- |
| 三路 join 基线 | `29.805s` | `238.5MiB` | D5/D6 retained 约 `10.0M` |
| 每层重复 common-root upper | `27.205s` | - | upper 改善但重复扫描约增 `1.98s`，撤回 |
| 全部 A4 提前闭包 | `25.708s` | `131.7MiB` | 上界强但 fast Toronto 退化，撤回 |
| 最佳三块 witness 最多三次 lifting | `25.723s` | `131.6MiB` | 当时保留；现已被在线结算版替代 |
| 再吸收第二块 | 无上界改善 | - | 仅产生 20 个 settled values，撤回 |

保留版在 Toronto 把 common-root upper 从 `1.1498267817` 降到 `0.8470137289`，witness lifting 再降到 `0.7876599892`；D5/D6 retained values 降到 `0.378M/0.224M`。

## 6. Balanced split 反例

候选规则要求每次 `D+D` 或 `A+D` 合并的两侧都至少包含总 token 的三分之一，希望把 D6 的 31 个无序 split 降为 10 个 3+3 split。该规则不含经验参数，但 rooted tree 的回根主干可能与一侧子树共享，centroid 分解不能直接替代 rooted recurrence。

| 受限位置 | 反例 |
| --- | --- |
| D 与 A 同时受限 | seed `712133`, iteration 2：`39 -> 40` |
| 仅 D 受限 | seed `712135`, iteration 47：`34 -> 38` |
| 仅 A 受限 | seed `712133`, iteration 18：`46 -> 48` |

首轮 `g=2..10` 500 例曾全部通过，但固定 g13 很快否决；因此不能用窄随机通过替代理论完备性。限制已完全撤回。

## 7. Root-major ordered convolution

探针把传统“按目标 mask 两两归并 rows”与“整层按 root 读取一次 payload，再做局部 disjoint subset convolution”比较。Toronto g13 的结构信号为：

| layer | 传统游标步数 | 每层输入 payload | 同根合法 pair |
| ---: | ---: | ---: | ---: |
| D5 | `180.005M` | `8.621M` | `117.861M` |
| D6 | `380.750M` | `8.999M` | `200.741M` |

原型只在 `size>ceil((g-1)/3)` 时启用，即四块完整上界已经存在后；没有固定 g、数据集或运行时阈值。每个 root 在扫描 active rows 与预计算 disjoint masks 之间选择操作数严格较小者，全程仍是 ordered rows、无 Hash、同一批 exact splits。

正确性通过 `g=2..10` `300/300`（seed `712151`）与固定 g13 `50/50`（seed `712153`）。full Toronto 的 D6 从 `5.37s` 降到 `4.79s`，但 D5 基本持平，整体仅为 `25.642s / 137.3MiB`；fast20 从当时主版 `13.438s` 退到 `13.885s`，五个数据版本都没有稳定正收益。说明合法同根 pair 本身已占主要成本，单纯转置访问顺序不足以突破。实现与 probe 字段均已撤回。

文献核对同时确认：[Fuchs et al.](https://doi.org/10.1007/s00224-007-1324-4) 已通过猜测额外 separator terminals、拆成小组件并按单点重叠拼回，将经典 `3^k` 改进到任意 `c>2` 的 `O*(c^k)`，代价是 `n^q` separator 枚举。Test21 后续若使用开放主干/多边界状态，必须说明与该路线的区别，且不能在 DBLP 上直接引入 `n^2` 状态。

bounded DBLP g13 只在四块 upper/witness 保留版上运行：CPU `584s` 时仍未完成，采样 RSS/peak 约 `3.36/3.39GiB`，随后主动终止。它已超过 V3 完整 `531.556s`，因此未对 root-major 负版本重复长跑。

## 8. 保留与排除

保留：

- `D/A` 两族离线有序 rows；
- `A+D` 单 anchor-side recurrence；
- farthest permanent anchor；
- TSP/2、one-label/farthest 与 directed-cut dual；
- row 内 priority queue 和 row 后 anchor-filtered 三路 completion。

排除：

- 跨 mask Hash label map；
- root-local flat Hash；
- balanced-only D/A splits；
- root-major layer convolution；
- A 结束后重新启动 global B；
- 固定 `g`、数据集、层级、density、wall time 或完成比例特判；
- baseline 同样可做的图/query 压缩。
