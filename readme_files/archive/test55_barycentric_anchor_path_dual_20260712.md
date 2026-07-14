# Test55：Barycentric Anchor-Path Dual

更新时间：2026-07-12。Test55 是 Test54 cover-all cap 的结构性修正：每个 group 不再增长到 paid anchor path 上最远点，而取路径 residual 距离的均值，并至少保持原 root-star cap。该规则对路径顶点对称、无经验参数，且仍只构造一个 dual。随机精确性通过，但 DBLP 两版 D2 继续数量级反向，fast20 `13.198s`，故实现与 API 已撤回。

## 1. Root-Preserving Barycentric Cap

对当前 residual distance `d_i`、paid path `P=(p_0,...,p_l)`，其中 `p_0` 是 root-star root：

```text
average_i = (1 / |P|) sum(p in P) d_i(p)
tau_i     = max(d_i(p_0), average_i)
phi_i(v)  = min(d_i(v), tau_i).
```

`max` 只保证原 root 仍被 moat 到达，使 zero-residual primal recovery 保持可用；新增 charge 由路径均值确定，不含可调系数。与 Test54 相同，每步只扣当前 residual capacity，因此 future potential 保持合法。随机 DPBF 对拍为 `300/300`（seed `713161`，`g=2..13`）。

## 2. Fast20

Release/O2 快照 `20260712_204121`：

```text
Test55 barycentric cap  13.198s
Test54 covering cap     15.347s
formal Test21           12.380s
```

相对正式 Test21 的五库聚合 D2：

| dataset | values ratio | pops ratio |
| --- | ---: | ---: |
| Toronto | `1.02x` | `1.00x` |
| Toronto-new | `0.68x` | `0.63x` |
| DBLP | `16.31x` | `17.77x` |
| DBLP-new | `2.48x` | `3.21x` |
| MovieLens | `0.82x` | `1.03x` |

均值确实缓和 Test54 的部分过度 charge，并保留 Toronto-new/MovieLens 的正信号；但 DBLP 的灾难性退化几乎没有改变。因此问题不是 path max 这个聚合函数过激，而是 sequential scalar-cap 模型本身。

## 3. 结论

Test54--55 共同否决：在 sequential directed-cut ascent 中，仅把每组停止高度从单根距离替换为 paid-path 的某个 scalar aggregate，无法稳定编码 A 主干。新增 residual capacity 总由早处理 groups 优先占用，后处理 groups 的 mask future 会跨库反向。

下一机制必须改变 charge 的并发语义，例如多个 active groups 同时竞争 residual arc，或构造与 group order 无关的共享 packing；不能继续尝试 path min/max/mean/median、经验插值或更多 scalar cap。Test55 不新增论文引用。
