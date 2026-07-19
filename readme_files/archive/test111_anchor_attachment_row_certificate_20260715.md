# Test111：Anchor Attachment 行证书（已撤回）

更新时间：2026-07-15。Test111 检查能否把 permanent anchor 的几何信息压成普通 D 行的常数个标量，从而在共同根枚举前整块跳过一个 pivot/branch split。证书通过 Release/O2 正确性门禁，但没有减少状态，fast20 的昂贵有序连接几乎不命中，最终源码、统计字段与临时快照全部撤回；当前 Test80 仍为 Test107。

## 1. 证书

对普通行 `D(X,v)` 保存

```text
mu(X) = min_v D(X,v)
alpha(X) = min_v (D(X,v) + dist(v, anchor-group)).
```

branch 侧只在 root-irreducible roots 上计算对应的 `mu_branch` 和 `alpha_branch`。普通 split `D(X,v)+D_branch(Y,v)` 的 future 始终还包含 anchor 组，因此现有 `H(v)` 至少为 `dist(v,anchor-group)`。于是每个实际候选都满足

```text
D(X,v) + D_branch(Y,v) + H(v)
  >= max(alpha(X) + mu_branch(Y),
         mu(X) + alpha_branch(Y)).
```

右侧严格大于 incumbent 时，可以在读取两个有序行之前跳过整个 split。该结论只使用 permanent anchor、现有 D/branch 语义和合法 future，不含数据集、固定 `g`、层号、密度或时间特判，也不引入 Hash。每张行只增加常数个标量，最坏渐进复杂度不变。

## 2. 正确性门禁

Release/O2 与 DPBF 在 `1e-6` 内全部一致：

```text
seed 716941, n=4..10, g=2..8: 100/100
seed 716942, n=14..18, g=13:  50/50
```

fast20 的 20 条答案和 ordinary values 均与 Test107 相同；总 ordinary values 仍为 `2,893,969`。

## 3. Fast20 否决

Test107 对照为 `result_snapshot/fast/20260715_095356`，Test111 临时快照原为 `20260715_103940`：

| 指标 | Test107 | Test111 |
| --- | ---: | ---: |
| solver total | `6400.056ms` | `6958.764ms` |
| ordinary | `2139.984ms` | `2230.339ms` |
| ordinary values | `2,893,969` | `2,893,969` |
| D join work | `33,800,869` | `33,466,066` |
| certificate calls | `0` | `166,745` |
| certificate rejects | `0` | `36,438` (`21.85%`) |

表面的 split 拒绝率不低，但 join work 只下降 `0.99%`。原因是拒绝集中在本来就很短的稀疏行；真正昂贵的 Toronto 路径几乎不命中。例如 Toronto g12 只拒绝 `207/24,057=0.86%`，Toronto-new g12 也只有 `2,225/24,057=9.25%`。维护 `alpha` 需要在每张已保存行上再读取 anchor distance，成本高于省下的连接工作，ordinary 增加 `4.22%`，solver total 增加 `8.73%`。因此没有运行 Toronto g13 或任何真实 DBLP 门禁。

## 4. 结论

Test82 已说明 directed-cut 约化代价的行标量不能减少状态；Test108--110 又说明逐共同根证书只省少量读取。Test111 进一步排除了更便宜的静态 anchor-distance 行证书：**把 attachment geometry 压成单个最小值，会丢失“哪个根同时接近 accumulator、branch 与 anchor”的相关性。** 下一机制若仍希望在有序连接前批量拒绝，必须保留可共享的根区间、排序前缀或受控的 attachment profile；继续增加常数个全行最小值没有理论杠杆。

Test111 没有新增外部论文引用。
