# Test42：D-Layer Rent-or-Buy Greedy

更新时间：2026-07-12。Test42 把 ReleaseV3 已验证的普通图 tree-growing greedy 接入 Test21，但不无条件预处理：ordinary rows 的实际 seed/merge/heap 工作足以支付一次 greedy 最坏构造时才购买。fast 总时间有小幅正收益，full DBLP 仍超过 V3 时间线，故代码、统计字段与结果目录全部撤回，正式 Test21 未改变。

## 1. 购买规则

令 `heap_cost=ceil(log2 n)`，使用与 ReleaseV3 相同的无参数成本：

```text
greedy_buy_work = g * (m + n * heap_cost).
```

每张 ordinary row 计入 seed 扫描、同根 merge probes，以及 `(pushes+pops)*heap_cost`。第一次累计工作达到购买成本时，从 root-star 根运行一次确定性 greedy；所得树是真实可行上界。规则不读取数据集、固定 `g`、层号、row density 或 wall time，符合 `agent.md` 第六条。

本探针只让新 `best` 作用于后续 rows，没有增加复杂 compact；这样先隔离 upper 的因果收益。

## 2. 正确性与 Fast Gate

随机 DPBF 对拍：`100/100`，seed `712841`，`g=2..13`。Release/O2 fast 快照曾位于 `result_snapshot/fast/20260712_152310`，提取证据后已删除。

```text
fast20 candidate  12.182s
formal Test21     12.380s
greedy activated  12/20
greedy improved   10/20
greedy total      11.41ms
```

| dataset | candidate | formal Test21 |
| --- | ---: | ---: |
| Toronto | `1.973s` | `2.375s` |
| Toronto-new | `5.292s` | `5.529s` |
| DBLP | `0.664s` | `0.595s` |
| DBLP-new | `0.787s` | `0.785s` |
| MovieLens | `3.466s` | `3.095s` |

MovieLens 与 DBLP-fast 没有购买 greedy；其反向来自热循环新增 work 计数和运行波动，说明正式集成还需消掉计数常数。但总和改善 `1.6%`，且 full incumbent 差距是已知关键变量，因此仍满足一次 full gate。

## 3. Full DBLP g13 Gate

直接运行 q1，以 ReleaseV3 的 `531.556s` 为硬门槛。进程在没有最终 query/stats/weight 行时终止：

```text
sampled CPU             558.7s
working set             4,290,899,968 bytes
sampled peak            4,308,881,408 bytes
final weight            none
```

它已经超过 V3 完整求解时间，不能声称完成，也不能用 fast 总和正收益保留。由于最终 stats 未写出，本页不虚构 greedy activation mask 或 D2 完成时刻；同一 greedy 源码与 V3 一致，但这不能替代当前 run 的直接日志。

## 4. 结论

1. Test21 在 D2 前缺少 V3 的普通图 greedy 确实是 incumbent 差距之一，但补上它仍不足以让 full DBLP 不退化。
2. work-triggered greedy 是通用上界工程，不是新的 A 状态机制；在 full gate 失败后继续优化计数/compact 只会回到细枝末节，不作为主线。
3. Test40 的 exact-best 样本和 Test42 的失败共同说明：需要的不是把 `17.36` 小幅降到 V3 greedy 水平，而是由 anchor-aware half/三块结构在 D2 前得到接近最优的 incumbent，或从状态定义中消掉 D2。

Test42 复用 ReleaseV3 已记录的 greedy，没有新增论文引用。
