# Test22：A/D 补集双向搜索实验归档

更新时间：2026-07-12。Test22 是一次已经撤回的结构原型；当前 Test21 仍见 `../test21_anchor_half.md`。本页保留状态定义、正确性和否决数据，不保留源码开关。

## 1. 核心设想

令 `K` 为 `g-1` 个 nonanchor groups，`h=floor(g/2)`。Test21 的两族状态为：

```text
D(S,v): 覆盖 S，1 <= |S| <= h
A(S,v): 覆盖 anchor 与 S，0 <= |S| <= h-1
```

当 g 为奇数时，它们与 nonanchor subset lattice 的非空 masks 恰好一一对应：

```text
低半 mask S       <-> D(S)
高半 mask K\S     <-> A(S)
```

因此原型不再先完整生成 D rows、再完整生成 A rows，而是同时运行两个 label-setting 波前：

```text
D singleton terminals -> D+D
anchor terminals       -> A+D
                         |
                         v
                   A + D + D completion
```

状态只使用 Test21 已证明的 `D+D`、`A+D` 和 centroid 三块完成。全局 heap 按 admissible lower 排序；当最小 key 不小于当前可行 `best` 时精确停止。

## 2. 无 Hash 实现

每个实际触达的 root 保存稳定 label IDs，并维护：

- 按编码 mask 排序的 `(code,id)` 小表，用二分查找；
- D/A 分离的 settled disjoint bitmap；
- 当累计二分比较租金不小于 direct directory 的槽数时，为该 root 一次性购买直接索引；
- bitmap word work 与补集 submask work 取严格较小者。

所有选择都来自实际理论操作数，没有数据集、固定 g、层级、density 或 wall-time 阈值，也没有 Hash。

Toronto fast g12 的实现演进为：

| 版本 | wall | 说明 |
| --- | ---: | --- |
| root-local 线性 Find | `79.525s` | `238.3M` merge checks，否决 |
| ordered binary index | `11.195s` | 状态不变 |
| disjoint bitmap + 在线 completion | `7.586s` | completion 不再单独枚举全补集 |
| D/A split index + direct rent/buy | `5.594s` | 无 Hash direct directory |
| pre-index lower + bitmap/submask choice | `4.006s` | 最终原型 |

在线四块 common-root upper 没有提前改变该查询的 `best`，只增加 completion checks，已在最终原型前撤回。group-MST attachment 加入完整 lower 也没有删除任何 label，但其 cheap 部分作为 pre-index rejection 有效。

## 3. 正确性

最终执行路径通过：

```text
random g=2..10  100/100, seed 712281
fixed g=13       30/30, seed 712283
```

更早的状态原型还通过 `g=2..8` `200/200`（seed `712201`）、`g=2..10` `300/300`（seed `712203`）和固定 g13 `30/30`（seed `712205`）。所有比较均为 Release/O2、相对 DPBF `1e-6`。

## 4. Fast20 否决

Test22 快照：`result_snapshot/fast/20260712_025232`，运行后已删除；配对 Test21/V3 快照分别为 `20260712_012305`、`20260711_230000`。

| dataset | Test22 | Test21 | ReleaseV3 |
| --- | ---: | ---: | ---: |
| Toronto | `4.328s` | `2.313s` | `1.493s` |
| Toronto-new | `11.738s` | `6.050s` | `4.704s` |
| DBLP | `1.040s` | `0.626s` | `0.581s` |
| DBLP-new | `1.035s` | `0.862s` | `0.484s` |
| MovieLens | `3.575s` | `3.587s` | `5.247s` |
| **total** | **`21.715s`** | **`13.438s`** | **`12.509s`** |

Test22 只在 MovieLens 基本持平，另外四库均退化。更重要的是，状态本身也系统性多于 V3：

| dataset | Test22/V3 settled | Test22/V3 created |
| --- | ---: | ---: |
| Toronto | `1.440x` | `1.244x` |
| Toronto-new | `1.516x` | `1.235x` |
| DBLP | `2.107x` | `2.135x` |
| DBLP-new | `2.661x` | `1.521x` |
| MovieLens | `1.068x` | `1.068x` |

这证明退化不只是 ordered directory 常数。B 的单向 farthest-goal orientation 在四库中产生更小 frontier；同时运行 D 与 reverse-A 波前会处理更多有效状态。

## 5. 结论

- A/D 补集双射和在线三块相遇是正确的状态组织，但不是当前性能突破。
- 无 Hash ordered/direct rent-buy 已把单例加速约 `19.8x`，仍不能抵消额外 frontier。
- 不运行 Toronto full 或 DBLP full；fast 与状态计数已经否决。
- 后续不能只把 B 的全局调度换皮成双向 A。真正的新状态必须在进入图传播前减少需要表示的 rooted partial trees，而不是同时展开两种 orientation。

