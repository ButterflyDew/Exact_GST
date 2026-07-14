# Test43：Anchor Caterpillar 三块可行上界

更新时间：2026-07-12。Test43 尝试完全绕开 ordinary D2：只用 singleton branches 生成低阶 anchored caterpillar rows，再以三个 anchor-containing blocks 同根拼出完整可行树。fast 两库的逐 row star completion 有改善，但三块本身五库均无更新；full DBLP 即使先给出合法 greedy upper，也无法在 bounded gate 内完成 size 1。工具、逐层日志、构建入口和二进制均已删除，正式 Test21 未改变。

## 1. 状态与三块上界

固定 permanent anchor `a`，令 `k=g-1`，`q=ceil(k/3)`。定义受限状态：

```text
C(empty,v) = gd[a][v]
C(S,v) = closure(min_{i in S} C(S-{i},v) + gd[i][v]), |S|<=q.
```

每个 `C(S,v)` 都是一棵包含 anchor、`S` 和 root `v` 的真实连通树，只允许依次接入 singleton branch。它不声称等于完整 A state；Test37 的 bounded-branch 反例不影响其作为可行上界 witness。

把 nonanchor groups 分成至多三个大小不超过 `q` 的 blocks，在同一 root 求和：

```text
C(S1,v) + C(S2,v) + C(S3,v).
```

三棵树都包含 `v`，所以 union 连通并覆盖全部 groups；不同 anchor 路径可能重复计费，但不会产生非法偏小值。rows 仍是 mask-major、按 vertex 排序的离线列表，不使用 Hash。`q` 由三块覆盖公式决定，不是固定 `g`/层或数据参数。

每张 C row 定型时还检查：

```text
C(S,v) + sum_{i notin S} gd[i][v],
```

即一个 anchored caterpillar block 加 remaining root-star paths，同样是合法上界。

## 2. Fast g12 Gate

所有运行使用 Release/O2、query 1、正式 root-star/farthest anchor、TSP/2 与 directed-cut potential：

| dataset | initial | C-star / three-block | exact | row values | rows wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `1.120571270` | `1.120571270` | `0.961622780` | `267,016` | `309ms` |
| Toronto-new | `5.717172880` | `4.460740380` | `3.873545840` | `940,685` | `948ms` |
| DBLP | `12.166303000` | `12.166303000` | `12.166303000` | `19,830` | `128ms` |
| DBLP-new | `11.006434800` | `10.824557800` | `10.314754800` | `114,880` | `244ms` |
| MovieLens | `0.020218977` | `0.020218977` | `0.0202189774` | `906` | `71ms` |

Toronto-new 与 DBLP-new 分别出现 19/3 次 row-star 更新；五库最终 `three_best==star_best`，即三个 C blocks 没有一次额外更新。已知 exact 值检查均在 `1e-6` 容差内满足 `upper>=OPT`。

## 3. Full DBLP g13 Gates

探针输出每个 size 完成行。两次 bounded run 在终止前均没有出现 `size=1`，因此没有 upper、row values 或最终 query 行：

| starting upper | sampled CPU | working set | sampled peak | size 1 |
| --- | ---: | ---: | ---: | --- |
| dual primal `17.360814337` | `258.3s` | `2,215,632,896B` | `2,302,799,872B` | 未完成 |
| legal V3 greedy `15.0174017721` | `234.8s` | `2,277,462,016B` | `2,328,821,760B` | 未完成 |

第二次通过 CLI 注入已知合法 greedy tree cost，只用于组合因果诊断；数值从未硬编码进 solver。即使已有该上界，12 张 C1/A1 closure 仍在 D2 前形成重型状态搜索。继续等待无法满足 V3 `531.556s` 总时间门槛。

## 4. 结论

1. singleton-only C recurrence 产生的 witness 合法，fast 两库也确实能提前改善 incumbent；但 full 的 C1 自身已经不可购买。
2. 三个都包含 anchor 的 blocks 重复支付 anchor trunk，fast 没有任何 completion 更新。下一结构若仍做多块，必须**共享一次有证书的 anchor backbone**，不能把三棵独立 A 树简单相加。
3. Test40 与 Test43 共同证明：先显式生成 A1 roots，再消费/完成的接口存在与 D2 类似的全图传播障碍。下一候选应让 anchor backbone 成为隐式公共对象，或直接在该 backbone 上消掉 root 维度。

Test43 没有引入新的论文机制，因此没有新增论文引用。
