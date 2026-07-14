# Test29：Variable-Root Pair Forest 复核

更新时间：2026-07-12。Test29 是已经撤出源码和构建入口的短期质量探针。它不是新主线；早期 Test18 已对少量候选根测试过同类 pair matching，本次只在当前 Test21 的全部 fast D2 rows 上做更彻底复核。

## 1. 机制

D2 层完成后，对每个图点 `v` 从 root-star 值开始：

```text
gd[anchor][v] + sum(gd[i][v], i in nonanchor groups)
```

若 `D({i,j},v)` 已保留，则 pair 相对两个 singleton 的节省为：

```text
gd[i][v] + gd[j][v] - D({i,j},v).
```

按节省从大到小确定性选择不相交 pairs，未匹配组仍使用 singleton。结果是若干同根可行树的并，因此只会产生合法上界。实现按顶点顺序同步扫描有序 D2 rows，复杂度 `O(n g^2)`，无 Hash、阈值或数据特判。

## 2. 正确性与 fast 结果

Release/O2 与 DPBF：

```text
g=2..12    300/300, seed 712531
fixed g13    30/30, seed 712533
```

| dataset, g12 q1 | visited D2 values | updates | best before -> after | probe time | final wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| DBLP | `2,226` | `0` | `12.166303 -> 12.166303` | `0.916ms` | `0.309s` |
| DBLP-new | `10,615` | `0` | `11.006435 -> 11.006435` | `1.228ms` | `0.391s` |

状态数与正式 Test21 完全相同。探针本身很轻但没有任何上界改善，说明否决来自候选质量，不是实现常数。

## 3. 与既有工作的关系

`test18_research_log_full_legacy_20260710.md` 第 329 行附近已经记录：早期版本在 greedy 候选根上使用 pair matching，Toronto 和 DBLP 都弱于 tree-aware greedy。Test29 把根集合扩展到当前 D2 rows 出现的全部根，仍得到相同结论。

因此固定根 Test28 与任意根 Test29 共同排除“独立同根 pair forest”作为 D2 突破口。后续若组合 pair，必须表达 pair 之间共享的内部主干；继续增加匹配精度、pair 数量或候选根数量不再作为研究主线。full DBLP 未启动。
