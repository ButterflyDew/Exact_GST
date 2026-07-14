# Test33：First-Half Global Warm Start 探针

更新时间：2026-07-12。Test33 只修改 `tools/global_half_probe` 的临时停止条件，没有建立生产 solver。探针模式和字段已在取证后删除。

## 1. 候选机制

farthest-anchor global labels 仍使用 root-star、greedy、directed-cut dual 与原 lower bounds，但不跑到精确结束。当首次由一个至少覆盖 `floor(g/2)` 个 nonanchor groups 的 settled label 严格改善 incumbent 时立即停止，把该可行 `best` 交回 Test21 的 A/ordered-row 主算法。

`half` 来自方法本身的状态边界，不是数据、时间、固定 rows 或密度参数。global 只作为上界 oracle，不承担最终正确性。

## 2. Fast g12 q1

| dataset | initial -> half-update best | settled | search | wall | 判断 |
| --- | ---: | ---: | ---: | ---: | --- |
| DBLP | `12.166303 -> 12.166303` | `7,655` | `0.126s` | `0.228s` | dual 已精确，无 update |
| DBLP-new | `11.006435 -> 10.363816` | `3,123` | `0.050s` | `0.110s` | 接近最终 `10.314755` |
| Toronto | `1.031404 -> 1.026149` | `83,284` | `0.281s` | `0.323s` | 改善过弱 |
| Toronto-new | `4.194458 -> 4.166745` | `97,276` | `0.232s` | `0.286s` | 改善过弱 |

DBLP-new 是正信号，但另外三库不能证明 oracle 成本可由后续 row 节省覆盖。

## 3. Full DBLP g13 q1 bounded 结果

使用 ReleaseV3 已验证的 farthest anchor 第 12 组。探针在第一个 size-6 update 自动停止：

```text
initial best          15.0174017721
stop best             14.8911998998
wall                  64.764s
bounds                18.411s
upper envelope        42.087s（包含 dual）
dual                  37.700s
global search          4.101s
settled labels        32,568
created/open labels   5,471,527 / 5,131,254
stop label size        6
```

这不是 full solver 结果，没有最终权重。历史 ReleaseV2 在 `262,144` 和 `1,048,576` settled 时曾达到 `13.8933` 与 `12.6821`；first-half 门槛明显停得更早，但得到的 `0.1262` 改善不足以证明能抵消 `64.8s` 预热及巨大 open frontier。

## 4. 结论

first-half global warm start 不接入 Test21，也不继续跑 full global。它在 DBLP-new fast 上有用，但收益不跨库；full DBLP 的结构门槛输出也过弱。继续选择更深 global 停止时刻会重新引入缺乏理论依据的经验参数，违反 `agent.md` 第六条。

fast DBLP 的 directed-cut dual objective、dual primal 与最终值恰好均为 `12.166303`，但后续 Test34 随机反例证明 `Objective()` 不是自由根 GST 的全局停止下界，不能据此提前返回。详见 `test34_dual_objective_certificate_20260712.md`。
