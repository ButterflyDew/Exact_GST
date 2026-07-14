# Test38：Local Seed-Cone Dominance

更新时间：2026-07-12。Test38 检查离线 row 在图闭包前能否删除局部被支配的 split seeds。定理与实现均正确，fast/ Toronto full 有稳定正信号；但它不减少 DBLP D2 必须定型的 rooted values，bounded gate 未在 V3 时间线附近完成，因此不进入当前 Test21。临时代码、统计字段、逐层输出和结果目录均已清理。

## 1. 支配定理

对一张 row 的闭包前 seed function `s(v)`，若边 `u-v` 满足：

```text
s(u) + w(u,v) <= s(v),
```

则从 `v` 出发的加权距离锥在任意目标 `x` 都不优于从 `u` 出发的锥：

```text
s(u) + dist(u,x)
<= s(u) + w(u,v) + dist(v,x)
<= s(v) + dist(v,x).
```

因此 `v` 不需要作为 Dijkstra 初始 source。沿严格下降边递归，最终只需 edge-local minima。零权等值边用 vertex id 定向，保证至少保留一个 source，不形成互删环。

Test21 的 `H(v)` 满足 edge consistency。若 `v` 通过 `s(v)+H(v)<=best`，支配它的 `u` 也通过：

```text
s(u) <= s(v)-w,
H(u) <= H(v)+w
=> s(u)+H(u) <= s(v)+H(v).
```

所以先做 future pruning 再做 local cone filtering 仍然安全。过滤只改变 heap source 集，不改变 seed 数组、最终 row value 或 branch 判定。

## 2. 无参数购买规则

无条件扫描 seed 邻边会让 seed 很少、图很密的实例退化。最终候选只在下式成立时购买 cone scan：

```text
sum(degree(v), v in seeds) <= |seeds| * ceil(log2(|seeds|+1)).
```

左侧是实际邻边比较数，右侧是把全部 seeds 逐个插入当前二叉 heap 的显式工作上界。规则不读取数据集名、`g`、层级、密度比例或 wall time。

## 3. 正确性

Release/O2：

```text
unconditional cone: g=2..12  300/300, seed 712791
unconditional cone: fixed g13  50/50, seed 712793
rent-or-buy cone:   g=2..12  100/100, seed 712795
```

均与 DPBF 在 `1e-6` 内一致。

## 4. Fast20 配对

同机快照：candidate `20260712_140431`，临时强制关闭 cone `20260712_140555`。提取数据后均删除。

```text
rent-or-buy cone   12.496s
cone disabled      12.702s
improvement         1.6%
```

购买只发生在 Toronto/ Toronto-new；DBLP、DBLP-new 与 MovieLens 的 `cone_checks=0`。逐库合计：

| dataset | cone | disabled | relation |
| --- | ---: | ---: | ---: |
| Toronto | `2.345s` | `2.373s` | `1.2%` faster |
| Toronto-new | `5.348s` | `5.553s` | `3.7%` faster |
| DBLP | `0.627s` | `0.595s` | cone 未购买，差异为运行波动 |
| DBLP-new | `0.797s` | `0.804s` | cone 未购买 |
| MovieLens | `3.379s` | `3.378s` | cone 未购买 |

Toronto 两库启用后，ordinary pops 约减少 `24%--29%`；所有 retained values 和最终权重不变。

## 5. Toronto Full g13

同机配对：

| metric | cone | disabled |
| --- | ---: | ---: |
| wall | `21.109s` | `21.862s` |
| ordinary time | `13.724s` | `14.297s` |
| ordinary pops | `7,559,414` | `10,140,319` |
| peak RSS | `102.9MiB` | `103.0MiB` |
| weight | `0.7048467020` | `0.7048467020` |

端到端改善 `3.4%`，ordinary pops 减少 `25.4%`。

## 6. DBLP g13 Gate

该结构正信号足以触发一次 bounded DBLP q1。运行没有产生最终 query/stats 行；在进程 CPU 约 `495.7s`、工作集约 `3.98GB` 时终止，已经接近 ReleaseV3 完整 `531.556s` 的时间线。没有最终权重，也没有可靠 D2 完成行，因此不能声称跨过 D2 或 full gate。

决定性限制是：cone filtering 只减少初始 heap sources 和部分 stale pops，仍必须输出同一批 surviving D2 rooted values。full DBLP 已知约有 `125.6M` pair states；Test38 不改变这一数量级。继续延长运行只会测常数，不满足当前“相对 V3 不退化”的目标。

## 7. 结论

- local seed-cone dominance 是可复用的精确离线-row 定理；
- rent-or-buy 规则能避免 fast 稠密图的扫描反噬；
- 但该机制不减少 row payload，只优化 heap source/pops；
- 它可作为未来真正减少 D2 values 的机制的辅助项，当前不单独进入 Test21；
- 不重复 full DBLP，不保留开关或 debug 字段。

