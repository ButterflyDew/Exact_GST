# Test139：角色定向的同根 Word 证书

更新时间：2026-07-16。Test139 沿 Test107/Test121 的 permanent-anchor A 主线，研究能否修复 Test126 将三项独立取最小值造成的同根相关性丢失。候选在现有 ranked-bitmap 半连接中，为 accumulator 与 branch 保存角色不同的逐 word profile，并在展开共同根之前拒绝整个 64-bit word。下界严格强于 Test126，且 fast20 确实跳过数百万个根；但 Toronto g13 q1--q5 的总 wall 和 q5 peak 均回退，因此实现、CMake 目标、二进制和临时结果已撤回，没有运行 DBLP。

## 1. 角色定向的联合下界

设 `K` 是全部原始组，`pi_S(v)` 是 Test107 directed-cut dual 在根 `v` 上对组集 `S` 的可加势。当前 root-irreducible recurrence 已固定连接角色：左行 `L` 是 accumulator，右行 `R` 只提供 closure 真正改善过的 branch roots。对现有 bitmap word `w` 定义：

```text
r_L(w) = min_{v in support(L) intersect w} [D(L,v) - pi_L(v)]
c_R(w) = min_{v in branch(R) intersect w} [D(R,v) + pi_(K-R)(v)]
```

对任意共同根 `v`，因为 `L` 与 `R` 不交且 `K-R` 可分成 `L` 与 `K-(L union R)`，有：

```text
[D(L,v) - pi_L(v)] + [D(R,v) + pi_(K-R)(v)]
= D(L,v) + D(R,v) + pi_(K-(L union R))(v).
```

右式正是该 seed 在当前目标 mask 下的 directed-cut 完整下界。因此若 `r_L(w)+c_R(w)>best`，word 中所有共同 branch roots 都可以安全跳过。A+D 完全相同，只需把 accumulator 的已覆盖集合写成 `anchor union L`。实现对两个 minimum 各减去全局 `fp::kEps`，只为抵消浮点消元次序差异，不是算法超参数。

该界严格支配 Test126 的三项独立下界。Test126 使用 `min r_L + min r_R + min pi_K`；本轮 branch profile 保留 `r_R(v)+pi_K(v)` 在同一个根上取最小值，所以 `c_R >= min r_R + min pi_K`。与此同时，本轮只在现有 bitmap accumulator 与 dense/bitmap branch 的半连接上启用，不增加 Hash、第二排序索引或新的图/query 压缩。

## 2. 无参数的惰性物化

第一版在 D row 完成后立即建立两种 profile。fast20 虽将 D/A direct work 分别降低约 `13.0%/3.9%`，但未命中 bitmap join 的行也支付了构建和驻留成本。最终版改为由依赖关系驱动的惰性物化：某行第一次作为 bitmap accumulator 时才建立 `r`，第一次作为 dense/bitmap branch 时才建立 `c`；每种角色最多构建一次并由后续消费者复用。

这不是按数据集、固定 `g`、层号、密度或 wall time 设置开关。D2 不会消费 D2 row，首次兼容消费者只能出现在 progressive packing 完成后的更高层，因此惰性构建也自然避免了混用 packing 前后两套 dual。所有组势的总和只建立一张共享 `O(n)` 数组，row profile 的空间与该行实际被消费的 bitmap words 成正比。

## 3. 正确性

全部测试使用 Release/O2，候选与 DPBF 以 `1e-6` 比较：

```text
seed 717801  g=2..8      100/100  eager prototype
seed 717802  fixed g15    50/50  eager prototype
seed 717803  g=2..8      100/100  lazy final
seed 717804  fixed g15    30/30  lazy final
```

所有实例均通过。fast20 与 Toronto g13 q1--q5 的最终权重、普通 D values/pops、锚定 A values/pops也逐项一致；候选只在调用逐根 `Set` 之前批量执行同一个 directed-cut 拒绝，没有改变状态定义、闭包、停止条件或 exact completion。

## 4. 固定短面板

fast20 覆盖五个固定数据版本的 g9--g12 首询问。惰性最终版的结果如下：

| 指标 | Test121 | Test139 | 变化 |
| --- | ---: | ---: | ---: |
| 20 条 wall 合计 | `8.219492s` | `8.087036s` | `-1.61%` |
| 逐询问 wall 变化中位数 | - | - | `-0.45%` |
| ordinary direct work | `29,591,330` | `25,750,138` | `-12.98%` |
| anchored direct work | `26,832,205` | `25,773,652` | `-3.95%` |
| 跳过共同根 | - | D `3,841,192`，A `1,058,553` | - |

跨库状态完全相同。候选的累计行 profile 比基线多约 `1.39MiB`；未命中路径的实例不再建立行级 profile。该结果说明新界显著强于 Test126，但 fast20 的端到端收益仍很小。

Toronto g13 q1--q5 在同一进程内逐询问配对：

| query | Test121 wall | Test139 wall | wall 变化 | D direct work | A direct work |
| --- | ---: | ---: | ---: | ---: | ---: |
| q1 | `6.898049s` | `6.887370s` | `-0.15%` | `-1.17%` | `-0.65%` |
| q2 | `3.394971s` | `3.349576s` | `-1.34%` | `0` | `0` |
| q3 | `9.096387s` | `9.043903s` | `-0.58%` | `-2.20%` | `-0.81%` |
| q4 | `2.520839s` | `2.534969s` | `+0.56%` | `0` | `0` |
| q5 | `25.511847s` | `26.298103s` | `+3.08%` | `-4.57%` | `-2.88%` |
| 合计 | `47.422094s` | `48.113920s` | `+1.46%` | `-3.64%` | `-2.12%` |

q5 的 D/A row payload 分别增加 `8.514/1.005MiB`，query peak 从 `146.441MiB` 增到 `157.047MiB`。额外读取与 profile 构建超过了被省去的逐根下界调用。单独的 Toronto q5 D3 截止探针还显示 D1--D3 的证书 words、拒绝 words 和跳过 roots 全为零；该路径从 D4 才开始出现。因此 DBLP D3 panel 对 Test139 没有判别力，而 Toronto 完整面板又不足以触发 D4/full 长门，本轮没有运行任何 DBLP 查询。

## 5. 结论

Test139 保留两条有效信息：第一，按连接角色保存 `reduced accumulator + complete branch` 可以严格保留一侧势与根的相关性，并显著强于三项独立 minimum；第二，profile 应按真实消费者惰性物化，而不是为所有行预处理。被否决的是固定机器 word 粒度的持久 profile：它只减少同一状态空间中的部分逐根调用，不减少 D/A values、pops、row 数或高层维度，空间和构建成本仍随被消费行数增长。

后续若继续做批量下界，必须让一个联合证书被更大范围的 row pairs 共享、一次跳过渐近上更大的有序区间，或者直接改变高层状态维度；不再通过缩小 word、增加更多逐 word 标量、经验 rent-or-buy 阈值或数据集开关延续本实现。

本轮没有新增外部论文引用。上述角色定向消元由本项目的 directed-cut 势可加性和 Test107 的固定 accumulator/branch recurrence 推导；进入论文前仍需单独检索 reduced-cost join pruning 与 learned/zone-map 类物理剪枝的相近工作，当前不宣称新颖性。
