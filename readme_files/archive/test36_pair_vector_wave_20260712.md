# Test36：多 Pair 向量波

更新时间：2026-07-12。Test36 尝试让一个图传播事件同时更新全部 ordinary D2 pair rows，以减少 full DBLP 的 `125.6M` 独立 pair-root states。随机与 fast 证明候选精确，但多目标优先级造成严重检查与队列膨胀；源码、CMake 目标和构建产物均已删除，正式 Test21 未修改。

## 1. 向量状态

对 source `s` 和当前 vertex `v`，一条波携带 `dist(s,v)`，同时表示每个 group pair `{i,j}` 的候选：

```text
c_ij(s,v) = gd[i][s] + gd[j][s] + dist(s,v).
```

若该 source 在 `v` 对所有 active pairs 都不改善当前值，则沿任意后续路径 `P` 仍不会改善：对每个 pair，已有某个 source 在 `v` 不劣，沿同一 `P` 延伸后仍不劣。因此可删除该 pair bit；只有实际改善的 bits 随边传播。

独立 Python 原型在 `1000/1000` 个随机图上与逐 pair multi-source Dijkstra 完全一致。C++/Release/O2 探针随后对 fast g12 的全部 `66*3500=231,000` pair-root 值逐项核对，五库均为 `errors=0`。

## 2. 为什么单一 Priority Key 不够

同一个 source 对不同 pair 有不同 offset：

```text
offset_ij(s) = gd[i][s] + gd[j][s].
```

探针使用 `min_ij offset_ij(s)+dist(s,v)` 作为 source wave 的单一 key。它保证同一 source 的较短路径先出队，但高-offset pairs 会随低-offset pair 过早传播，造成大量稍后被覆盖的 active bits。若改成每个 pair 自己的 `offset_ij+dist`，label-setting 次序恢复，但也退化为 66 个独立 Dijkstra；一般实数 offset 没有可证明的小分组数。

## 3. Fast g12

下表的 reference 是同一进程内 66 次标准 multi-source Dijkstra，不含 Test21 其它阶段。wave/reference 均计算未剪枝的完整 pair functions，因此比较口径一致。

| dataset | wave / reference | accepted / pair states | checks / pair states | wave peak queue |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `265.25 / 70.23ms` | `1.177x` | `21.62x` | `46,963` |
| Toronto-new | `417.74 / 70.09ms` | `2.098x` | `34.22x` | `50,091` |
| DBLP | `1752.94 / 78.47ms` | `0.286x` | `146.20x` | `1,259,316` |
| DBLP-new | `361.39 / 66.39ms` | `0.220x` | `38.33x` | `225,232` |
| MovieLens | `32051.09 / 708.15ms` | `0.090x` | `2710.52x` | `17,420,824` |

DBLP 和 MovieLens 的 accepted events 确实显著少于 pair states，说明跨 pair source sharing 是真实现象；但 active-pair checks 和 lazy queue 完全吞掉收益。MovieLens 即使 accepted ratio 只有 `9.0%`，wall 仍慢约 `45x`。

## 4. 结论

- “在 vertex 对所有 pairs 不改善即可停止该波”是安全支配规则；
- 一个 source profile 共享多个 pair values 可以减少 accepted events，但不能共享它们彼此不同的 label-setting priority；
- 不增加 `(source,vertex)` 记忆时会反复传播；显式记忆则是 `n^2` 状态；
- pair-specific queue 恢复正确顺序，却回到独立 pair searches；
- fast 已给出跨库数量级负结果，不触发 full DBLP，不保留实现或开关。

