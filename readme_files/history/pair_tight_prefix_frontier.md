# Pair Tight-Prefix Frontier

更新时间：2026-07-13。本文加强 Test49 的 pair gradient 定理：对一个给定 consumer，必要 frontier 不只是 parent-downhill endpoints，还可缩到 predecessor path 或整个 tight cone 上的 consumer 前缀最小值。定理有效，但 fast DBLP 的额外压缩很小；它保留为以后 exact pair interface 的组件，不进入当前 Test21。

## 1. 祖先支配定理

令 `D` 是一张 ordinary pair row，`A` 是任意 consumer rooted function。若 pair predecessor path 从 `u` 到 `v` tight：

```text
D(v) = D(u) + length(u -> v),
```

且 `A(u) <= A(v)`，则对任意输出根 `x`：

```text
A(u)+D(u)+dist(u,x)
<= A(u)+D(u)+length(u -> v)+dist(v,x)
=  A(u)+D(v)+dist(v,x)
<= A(v)+D(v)+dist(v,x).
```

所以 source `v` 的整个位移锥被祖先 `u` 支配。沿每棵 predecessor tree，只需保留 root 和满足

```text
A(v) < min A(u), u is a strict ancestor of v
```

的严格 prefix minima。Test49 的 parent 条件只比较一步，因此 prefix frontier 是其 exact 子集。

## 2. Tight-Cone 加强

证明并不要求 `u` 是选定 parent；只要存在一条 D-tight path 即可。对所有满足

```text
D(u) + w(u,v) = D(v)
```

的严格增距 arcs，按 `D` 从小到大传播最小 consumer 值，就得到 tight-cone frontier。零权等距 plateau 在 probe 中保守保留选定 parent path 的 prefix 结果，因此不会因处理顺序漏 source。

该操作保持完整 pair split-to-attachment 路径，不重犯 Test30 的 local-seed 错误。随机小图逐点比较

```text
closure(all pair sources)
closure(prefix/tight-cone sources)
```

为 `2000/2000`（seed `713451`），误差阈值 `1e-8`，证书错误为 0。

## 3. Fast g12 结构结果

consumer 取 Test21 permanent anchor 的 group-distance row，使用已知 exact incumbent；每条统计 55 个 nonanchor pairs：

| dataset | Test49 parent sites | tree-prefix sites | tight-cone sites | tight / parent |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `55,833` | `40,514` | `40,423` | `72.40%` |
| Toronto-new | `57,834` | `40,988` | `40,623` | `70.24%` |
| DBLP | `17,524` | `17,223` | `17,200` | `98.15%` |
| DBLP-new | `6,615` | `6,594` | `4,541` | `68.65%` |
| MovieLens | `510` | `510` | `508` | `99.61%` |

Toronto 的深 predecessor paths 能被 prefix minima 明显压缩；DBLP 的平均 predecessor depth 仅 `1.02`，tight cone 几乎没有额外祖先，因此决定性库没有数量级变化。扫描全部 tight edges 还会增加工作，所以没有触发 full DBLP。

## 4. 保留边界

该定理严格加强 Test49 的单 consumer second boundary，但没有处理 Test49/50 的首要失败：不同 A/D consumer masks 仍各自拥有不同的 prefix minima。若逐 consumer 枚举，`pair bits x consumer masks` 乘数仍在；若逐 target 测试，仍回到 membership probes。

因此它只能作为未来“同时共享 pair bits 与 consumer masks”机制里的 exact 局部算子。当前不再单独优化 parent/tight 扫描常数，也不以该结果运行 full。

本文没有采用新的论文算法；ancestor/tight-cone dominance 是由 Test49 gradient 证明直接推广得到的仓库内推导，尚未完成系统文献检索，不宣称论文级原创性。
