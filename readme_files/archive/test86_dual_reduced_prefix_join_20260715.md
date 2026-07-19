# Test86：对偶约化代价前缀连接

更新时间：2026-07-15。Test86 研究能否把 directed-cut 下界真正移入 Test80 的离线有序行连接，而不是像 Test82 那样只做整行标量拒绝。代数前缀有效，但顶点交集查询抵消了候选减少；三轮 fast20 工作量探针后未实现生产 solver，全部诊断字段和候选快照均已撤回，未运行 Toronto 或 DBLP。

## 1. 可加的约化代价

对覆盖组集 `S` 的普通状态定义

```text
rho_S(v) = D(S,v) - phi_S(v),
```

其中 `phi` 是 Test80 的 directed-cut potential。对不交的 `X,Y`，完整组势满足 `phi_X+phi_Y+phi_remaining=phi_all`，所以 D+D 候选的对偶完整下界为

```text
rho_X(v) + rho_Y(v) + phi_all(v).
```

A+D 完全相同，只需在 A 侧减去 anchor 与其 nonanchor mask 的势。若 accumulator 行 `X` 保存

```text
c_X = min_v (rho_X(v) + phi_all(v)),
```

并把 branch 行按 `rho_Y` 递增排列，则所有满足 `rho_Y>best-c_X` 的后缀都可在同根相交前安全跳过。该截止位置由当前可行上界直接决定，没有 block size、经验比例、数据集或固定 `g` 特判。

## 2. 朴素第二索引

第一轮探针不改变执行，只统计每次真实连接在上述截止位置前有多少 branch 值。fast20 汇总如下：

| 指标 | D+D | A+D |
| --- | ---: | ---: |
| branch 值 | `166,292,939` | `116,111,732` |
| 前缀 eligible | `44,047,954` | `47,481,474` |
| eligible 比例 | `26.488%` | `40.893%` |
| 逐点二分工作 / 当前自适应连接 | `4.5345x` | `5.6329x` |

前缀本身确实删除了多数 branch 访问，但 eligible 顶点不按 vertex id 排序。为每个 eligible 顶点在 accumulator 有序行中做二分，比当前双指针/直接扫描更贵。仅保存一个 32-bit 排列，fast20 各独立查询的投影总和也需 `9.382MiB`。

## 3. O(1) accumulator 子情形

若 accumulator 是 singleton 或 dense row，eligible 顶点可以 O(1) 查询。第二轮探针显示该子情形只覆盖 D/A branch 访问的 `1.792%/2.476%`，且其中仍有 `91.228%/92.231%` 通过前缀。只为这些连接建立索引仍投影为 `8.091MiB`，没有实际收益。

## 4. 按顶点顺序报告的最终门槛

最后考虑更强的二维接口：在 branch 行的 vertex-order `rho` 序列上建立 Cartesian-min 或等价 range-report 结构，只按 vertex 顺序输出 `rho<=threshold` 的点。这样可在“逐点二分”和“输出 eligible 后顺序扫描 accumulator 前缀”之间取较小者。

探针在不计索引构造、树遍历和缓存成本的理想条件下得到：

| 指标 | D+D | A+D |
| --- | ---: | ---: |
| 理想 report 工作 / 当前连接 | `0.9310x` | `1.1006x` |

D 侧理论上最多只剩 `6.9%` 的连接工作下降，A 侧在零额外成本假设下已经更差。真实 Cartesian tree 还需子树结构或 succinct rank/select 支持，并产生额外随机访问。因此该方向没有达到实现门槛。

## 5. 结论

Test82 的整行证书不足，Test86 又证明把它细化为 exact reduced-cost prefix 仍受二维约束：**按下界顺序找到候选，与按顶点顺序完成同根交集，是两个不兼容的排序维度。**在当前行定义上增加第二索引只能用空间和随机查询换回已经由线性归并完成的工作，不能减少 D3/D4 或 A4/A5 状态本身。

后续不再实现 `rho` 排列、Cartesian interval tree、固定 block minima 或 sparse accumulator Hash。若继续让下界进入物化前，必须改变状态/消费者接口，使同一个主序同时服务下界与连接；仅给现有行增加二维索引不是论文主线。

Test86 没有新增外部论文引用。directed-cut potential 背景仍见 Test80 文档；上述约化恒等式和三种接口比较是仓库内推导，尚不宣称论文级原创性。
