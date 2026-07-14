# Test28：D2 在线 Pair-Matching 上界

更新时间：2026-07-12。Test28 是已经撤出源码和构建入口的短期原型。当前 Test21 仍见 `../test21_anchor_half.md`。

## 1. 动机

Test26 将 full DBLP g13 的首要瓶颈定位到 ordinary `D2`。Test27 的单个 pair 加 remaining singleton star 没有改善 fast DBLP 的 `best`，因此 Test28 尝试同时使用多个已完成 pair。

固定 root-star 根 `r`。每完成一张 `D({i,j},*)` row，就读取 `D({i,j},r)`，并在 nonanchor groups 上重新计算标量 matching DP：

```text
M(empty) = 0
M(S) = min(
    gd[i][r] + M(S-{i}),
    D({i,j},r) + M(S-{i,j}) for j in S-{i}
)

candidate = gd[anchor][r] + M(all nonanchor groups)
```

每个项都是若干同根可行树的并，因此 `candidate` 是合法完整上界。算法只使用 Test21 已完成的有序 D rows 和一个 `2^(g-1)` 标量数组，不使用 Hash、数据集特判、固定 `g`、运行时间或密度阈值。在线重算使前面的 pair rows 一旦足够好，就能在 D2 尚未结束时收紧 `best`。

## 2. 正确性回归

Release/O2 下与 DPBF 比较：

```text
g=2..12    500/500, seed 712521
fixed g13    50/50, seed 712523
```

## 3. Fast DBLP 门槛

两条查询均完成并得到与正式 Test21 相同的权重和状态数：

| dataset, g12 q1 | matching checks | updates | best before -> after | D2 values / pops | Test28 wall | Test21 wall |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| DBLP | `506,935` | `0` | `12.166303 -> 12.166303` | `2,226 / 3,089` | `0.352s` | `0.304s` |
| DBLP-new | `506,935` | `0` | `11.006435 -> 11.006435` | `10,615 / 13,495` | `0.401s` | `0.385s` |

DBLP-new 的后续三块机制仍把最终结果降到 `10.3147548`，但 pair matching 在 D2 内没有贡献。wall 只用于确认没有净正收益；决定性否决证据是两库 `updates=0` 且 D2 payload 完全不变。

## 4. 结论

固定单根上的 pair/singleton forest 太受 root-star 几何约束，即使允许多个 pair，也没有比已有 root-star 上界更早得到好解。它不能缓解 full DBLP 的 `125.6M` pair-root 状态问题，因此不触发 full DBLP g13 长跑。

Test28 的源码宏、CLI、构建目标和统计字段均已删除。后续 D2 机制若仍组合多个 pair，必须允许共享的内部主干或在传播前减少候选 roots；继续强化固定根的独立块 partition 不再作为主线。
