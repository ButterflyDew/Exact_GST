# Test70：One-Third Endpoint + Progressive Junction

更新时间：2026-07-13。Test70 将两个此前分别精确、但单独不足的 A 框架机制组合：Test39 的 one-third backbone endpoint states，以及 Test58 的 branch-junction incumbent + delayed progressive residual packing。组合版在 fast20 得到 A 研究线新低 `9.161s`，Toronto full 为 `9.690s / 58.0MiB`；但 DBLP g13 在 V3 的 `531.556s` 门禁内仍无最终 weight，因此源码、公共 API、CLI、runner 与无效中间快照全部撤回。

> 后续状态：Test80 选择不含 one-third endpoint 的 Test58 型组合，fast20 `9.099s`、Toronto full `8.722s`，并在 `666.509s` 完成 DBLP。本文继续作为“为何不选 Test70 状态框架”的证据。

## 1. 状态组合

令 `k=g-1`、`q=ceil(k/3)`、`h=floor(g/2)`：

```text
D(S,v)  exact closed component, |S|<=q
P(S,v)  ordinary backbone endpoint, q<|S|<=h
A(S,v)  anchor backbone endpoint, |S|<=h-1
```

高 P/A rows 只接入 `|B|<=q` 的 closed D branch；P rows 不再发布 branch bits，最终仍由 `A+P+P` completion 完成。Test39 的 heavy-path 证明保证该递推 exact，不是 restricted upper。

在 D2 前构造一次 Test48 junction upper：扫描全部 nonanchor triples 的 paid-backbone attachment argmin roots，把 parent paths 并成压缩树，并由 subset facility DP 只为实际选择的分支付费。

正式 directed-cut dual 暂时保留 residual。D2 的真实工作事件累计到

```text
budget = g * (2m+n)
```

且仍有 pair rows 未处理时，才购买 anchor-path residual packing。各组同步增长；arc 饱和时冻结使用该 arc 的 groups，其余继续，直到全部冻结。该规则不读取数据集、固定 `g`、层、density 或 wall time。

## 2. 实现核验

初版将 residual 按输入 `edge.u/edge.v` 写入、却按顶点编号方向读取；端点非升序时两个 directed capacities 被交换。该版虽然 upper/exact solver 仍安全，但 packing scales 无效，快照 `20260713_023017` 已删除，不作为结果。

修正为统一 `min(u,v)->max(u,v)` 后，fast Toronto g12 scales 精确复现 Test58：

```text
rounds    2
min/avg/max scale  0.094814 / 0.170246 / 1.0
```

扩大随机与 DPBF：

```text
500/500  seed 713511  n=8..18, g=6..13
500/500  seed 713521  junction combined
300/300  seed 713531  deterministic anchor path
```

均在 `1e-6` 内一致。最终 branch-junction fast g12 upper 也逐库复现 Test48 的 `0.97147734 / 4.00328729 / 12.226144 / 11.0034048 / 0.0202272822`。

## 3. Fast20

有效快照：`result_snapshot/fast/20260713_023805`。

```text
Test70                    9.161s
historical Test58         9.390s
historical Test48         9.758s
formal Test21            12.380s
```

| dataset | Test70 | formal Test21 | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `1.492s` | `2.366s` | `0.631x` |
| Toronto-new | `3.038s` | `5.511s` | `0.551x` |
| DBLP | `0.620s` | `0.591s` | `1.049x` |
| DBLP-new | `0.550s` | `0.780s` | `0.705x` |
| MovieLens | `3.462s` | `3.057s` | `1.132x` |

Toronto g12 ordinary values 从正式 `817,941` 降到 `377,121`，D6 从 `110,307` 降到 `33,727`；Toronto-new ordinary 从 `2,453,121` 降到 `849,480`，D6 从 `514,393` 降到 `147,887`。P6 branch count 均为 0。

收益主要来自 junction incumbent 与 packing；one-third endpoint 让总时间比 Test58 再低约 `0.23s`，但 DBLP/MovieLens 因 junction 图搜索分别小幅退化。

## 4. Toronto Full

最终确定性 path 版：

```text
weight                 0.7048467020
wall                   9.641--9.690s
peak                   58.0--58.1MiB
ordinary values        2,700,162
D5 / D6                  347,048 / 194,678
P5 / P6 branch bits            0 / 0
anchored values          549,316
```

正式 Test21 为 `20.788s / 102.9MiB`，所以组合仍有强收益；但历史 Test58 是 `9.201s / 58.0MiB`。one-third endpoint 删除高层 branch payload，却没有在 Toronto 上回收其 endpoint recurrence 成本。

本次 packing 为一轮 uniform `0.060976`；历史 Test58 记录为三轮 `0.0610/0.0872/0.3293`。tight DFS 与 deterministic Dijkstra predecessor 两种最短 anchor path 都得到同一一轮结果；没有继续为单库挑选等长 path witness。

## 5. DBLP g13 硬门禁

有效 runner 先把重复的 `Path/PATH` 环境键规范成单一 `Path`，隐藏启动并持有真实 PID；从本次 `weights.txt` header 出现后计 query wall。第一次 Path 错误空跑目录已删除，不计实验。

有效证据：`result_snapshot/gates/20260713_0250_test70_anchor_third`。

```text
query wall at forced stop   531.831s
V3 complete query time      531.556s
sampled peak                2160.4MiB
final weight lines                0
```

约 `477 CPU-s` 前的 RSS/CPU 轨迹仍与 Test58 接近；one-third 高层状态没有让 solver 在余下时间完成。V3 在更短时间已有精确 weight，因此 Test70 严格失败，不能因 fast/Toronto 正收益保留。

## 6. 结论

Test70 证明 junction incumbent、progressive packing 与 one-third endpoint 可以正确组合，并把 fast 推到新低；但 DBLP 门禁再次说明瓶颈不只是 P5/P6 branch publication。下一机制必须减少 packing 购买后仍存在的 rooted high-state/consumer 工作，或改写 completion interface，不能继续叠加同类 potential、upper 或 endpoint 限制。

Test70 没有直接采用新的论文算法。one-third separator 的文献边界沿用 Test39；directed-cut residual 背景沿用 Wong；progressive packing、实际工作购买和 paid-junction 组合是仓库候选，不宣称系统检索后的论文原创性。
