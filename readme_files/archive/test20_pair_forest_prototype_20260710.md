# Test20 Pair-Forest 生产原型归档

本文记录 2026-07-10 从 ReleaseV1 复制并接入 pair predecessor certificate 的一次生产原型。它通过正确性与 small 守门，但 fast 端到端时间明显退化，因此已从 `methods/Test`、主 CMake、main dispatch 和 snapshot method 列表撤出。当前仓库只保留 `tools/pair_forest_probe` 与 `pair_forest_certificate.md` 中可复核的表示层结论。

## 1. 实现边界

原型保持 ReleaseV1 的单一路径，只改 pair rows：

- pair Dijkstra relax 时保存 `edge_id+1`，直接 seed 保存 code 0；
- dense certificate 为 `int32[n+1]`，sparse certificate 为排序的 `(vertex,parent)`；
- pair+singleton、pair+pair、pair+normal row 均在线性解码后 join；
- Complete 一旦补侧包含 certificate，就先物化完整 complement row，避免未解码随机 lookup；
- 两个 `O(n)` decode workspaces 延迟到第一次实际使用时分配；
- ReleaseV1 未修改，原型无环境开关、数据集分支、固定 g/层级特判。

## 2. 正确性

| check | result |
| --- | --- |
| DPBF random `g=2..10` | `ALL_OK seed=710601 iterations=300` |
| DPBF fixed `g=13` | `ALL_OK seed=710613 iterations=30` |
| small suite | 35/35 与 PrunedDP 权重一致 |
| fast suite | 20/20 与 ReleaseV1 权重一致 |

误差阈值为 `1e-6`。

## 3. Small 守门

最终有效结果：

```text
Test20     result_snapshot/small/20260710_160100
ReleaseV1  result_snapshot/small/20260710_142123
PrunedDP   result_snapshot/small/20260710_142204
```

| g | Test20 | ReleaseV1 | PrunedDP | Test20 / Release | Pruned / Test20 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | `0.0539s` | `0.0486s` | `0.1746s` | `1.11x` | `3.24x` |
| 3 | `0.0726s` | `0.0631s` | `0.1951s` | `1.15x` | `2.69x` |
| 4 | `0.0916s` | `0.0986s` | `0.2085s` | `0.93x` | `2.28x` |
| 5 | `0.1319s` | `0.1247s` | `0.2592s` | `1.06x` | `1.96x` |
| 6 | `0.1804s` | `0.1798s` | `0.3784s` | `1.00x` | `2.10x` |
| 7 | `0.4023s` | `0.3693s` | `2.9016s` | `1.09x` | `7.21x` |
| 8 | `0.9479s` | `0.8835s` | `7.5610s` | `1.07x` | `7.98x` |

总时间 `1.881s`，ReleaseV1 `1.768s`，PrunedDP `11.678s`。35 条中没有一条慢于 PrunedDP。最初 eager 分配两个 workspaces 的 `20260710_155914` 已被延迟分配版本取代，不作为最终结果。

## 4. Fast 决定性负结果

最终结果：`result_snapshot/fast/20260710_160208`。20 条权重全部一致。

| dataset | Test20 | ReleaseV1 | ratio | peak RSS Test20 / Release | decode ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `10.812s` | `6.800s` | `1.59x` | `52.79 / 55.04 MiB` | `5,974.8` |
| Toronto-new | `14.999s` | `11.000s` | `1.36x` | `97.23 / 99.67 MiB` | `6,958.5` |
| DBLP | `4.634s` | `2.820s` | `1.64x` | `32.45 / 34.10 MiB` | `2,031.1` |
| DBLP-new | `5.125s` | `2.126s` | `2.41x` | `31.96 / 33.92 MiB` | `3,017.3` |
| MovieLens | `11.814s` | `11.461s` | `1.03x` | `153.16 / 153.44 MiB` | `299.3` |
| **total** | **`47.384s`** | **`34.207s`** | **`1.385x`** |  | **`18,281ms`** |

四个 g 值合计的 decode calls 每个数据版本约 `56k--72k`。certificate 在 3500 点 fast 图上只节省少量 MiB，而重复线性解码累计约 `18.28s`，超过总退化 `13.18s`。这说明“单次 row 解码仅占 pair search 6%”不能外推到完整 recurrence：同一 pair row 会被许多高层 split 和 Complete 重复使用。

当前 mask recurrence 还能精确算出 g13 的读取次数。对固定 pair `P`：

```text
build uses    = sum_{j=1..4} C(11,j) = 561
Complete uses = C(11,6)             = 462
total uses    = 1023
all pair decodes = 78 * 1023 = 79,794
```

这里 k5 的 complement 虽可形式化为 `pair+size6`，但 size6 row 尚未生成，所以不计；k6 的 `pair+size5` 全部可用。full pair probe 把每个 pair 恰好解码一次，合计 `14.289s`。在状态集合不变的原型语义下，重复 `1023` 次仅 decode 就约为 `14,618s`，即 `4.06h`，尚未计算高层 join、Dijkstra 和其它预处理。这是组合依赖计数，不是数据/时间阈值，也进一步说明不应启动 full solver。

相对 PrunedDP fast 下界 `>380.375s`，该原型只有 `>8.03x`，也失去 ReleaseV1 已达到的时间一个数量级目标。

## 5. 撤回结论

1. predecessor certificate 是 exact、显著省 pair-row 持久空间的有效信息；full DBLP g13 单层的 `3.226x` 压缩仍成立。
2. “每次 recurrence 使用都线性解码”的生产形态不可接受，fast 已足以否定，不运行 full solver。
3. 不能用 `g==13`、数据集或内存阈值只在目标查询启用；这会违反 `agent.md` 第六条。
4. 若继续该方向，必须先给出统一的 row schedule、succinct random-access certificate，或证明受控 cache 的选择规则；在此之前不重建 Test20。
