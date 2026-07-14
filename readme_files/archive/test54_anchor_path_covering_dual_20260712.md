# Test54：Anchor-Path Covering Dual

更新时间：2026-07-12。Test54 尝试在一次 directed-cut residual construction 内编码 permanent-anchor 主干，避免 Test52 的第二次 dual 与双 subset-sum 成本。对每个 group 的 residual moat，不在到达单个 root 时停止，而增长到覆盖 root-star 到 anchor 的整条确定性最短路。机制精确但性能严重分化：Toronto-new D2 近乎减半，DBLP 两版却膨胀 `4x--21x`；fast20 为 `15.347s`，故公共 API、路径恢复、统计字段和代码全部撤回。

## 1. 定义

令 `P` 为 root-star root 到 permanent anchor 最近 terminal 的确定性最短路。处理 group `i` 时，在当前 directed residual graph 上计算距离 `d_i(v)`，并取：

```text
tau_i = max(p in P) d_i(p)
phi_i(v) = min(d_i(v), tau_i).
```

随后像普通 sequential dual ascent 一样，按 `phi_i` 的有向梯度扣减 residual arcs。每个 group 仍只产生一张 potential row，future query 仍为：

```text
h(v,R) = sum(i in R) phi_i(v).
```

因此没有第二 dual、没有 mask 特判，也没有额外运行时阈值。`tau_i` 覆盖整条 paid anchor path，而普通 root dual 只增长到 root-star 一点；这正面对应 A 主干几何。

每步只从尚未使用的 residual capacity 扣费，所以 potential 仍满足 edge consistency 与 cut packing。Test21 以该 potential 运行，通过随机 DPBF 对拍 `300/300`（seed `713141`，`g=2..13`）。

## 2. Fast20

Release/O2 快照 `20260712_203335`：

```text
Test54 covering dual   15.347s
formal Test21         12.380s
Test48 candidate       9.758s
```

相对正式 Test21 的五库聚合 D2 变化：

| dataset | values ratio | pops ratio | dual wall |
| --- | ---: | ---: | ---: |
| Toronto | `1.30x` | `1.41x` | `35.8 -> 43.5ms` |
| Toronto-new | `0.51x` | `0.46x` | `40.3 -> 51.8ms` |
| DBLP | `18.38x` | `21.19x` | `38.6 -> 94.8ms` |
| DBLP-new | `4.40x` | `5.30x` | `47.3 -> 75.7ms` |
| MovieLens | `1.05x` | `1.31x` | `1803.2 -> 2565.6ms` |

Toronto-new 的强正信号说明 anchor path cap 的确改变了 residual charge，而非实现空转；但 DBLP 两版的数量级反向直接违反跨库与大图目标，因此不触发 full DBLP。

## 3. 原因与边界

cover-all cap 对早处理 groups 增长得更远。虽然每次 charge 合法，但它会提前消耗大量 arc capacity，使后续 groups 的 residual distances 变小；对包含早期 groups 的 masks 势可能更强，对主要由后期 groups 组成的 masks 则显著更弱。不同数据集的 group order 与 anchor path 几何不同，于是产生不可接受的方向翻转。

这否决了“只把单根 cap 改成整条主干最大 cap”的共享 residual 方案。下一次若仍研究共同 residual，必须让多个 groups 对主干的 charge 同时平衡，不能由 sequential order 让早期 moat 独占新增容量。Test54 没有新增论文引用；directed-cut dual 的来源仍为 Wong。
