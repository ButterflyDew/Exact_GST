# Test41：Zero-Residual Half/三块可行上界

更新时间：2026-07-12。Test41 尝试在 ordinary D2 前复用 directed-cut dual 的零残量有向子图，用 A 风格的离线 half rows 与二/三块同根 completion 计算更强可行上界。机制正确且很轻，但 full DBLP 上完全没有改善 dual primal，已撤回；临时 common accessor、工具、构建入口和二进制均已删除，正式 Test21 未改变。

## 1. 状态与正确性

dual ascent 结束后，从 root-star 根到每个 query group 至少存在一条零残量有向路径。令 `Z` 为这些零残量 arcs 构成的有向子图，定义：

```text
R(S,v) = 在 Z 中从 v 出发到达 S 中各组的最小有向树成本。
```

singleton 由 group terminals 在反向零弧上做 Dijkstra；高阶 row 使用同根不交 split，再沿反向零弧闭包。只保存 `|S|<=floor(g/2)`，最后在同一 root 拼二或三块。每个 row witness 都是原图真实边的并，块成本求和只可能重复计边，因此 completion 一定是合法 GST 上界。该子图来自本方法的 dual residual，不是普通图/query 压缩。

首版反向闭包曾把 `predecessor -> current` arc 错查为同端点索引，导致三块 probes 为 0；该探针错误在正式计时前修正。下表只引用修正后结果。

## 2. Fast g12

所有运行使用 Release/O2、query 1：

| dataset | zero arcs | dual primal | zero-half upper | row values | rows wall |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | `2,104` | `1.120571270` | `1.120571270` | `21,065` | `17.6ms` |
| Toronto-new | `3,511` | `5.717172880` | `5.004591620` | `95,557` | `66.6ms` |
| DBLP | `964` | `12.166303000` | `12.166303000` | `4,539` | `18.2ms` |
| DBLP-new | `949` | `11.006434800` | `10.997419800` | `108,933` | `47.8ms` |
| MovieLens | `29` | `0.020218977` | `0.020218977` | `2,720` | `21.0ms` |

两库出现合法更新，且五库都没有上界反向；这足以触发一次 full 结构 probe，但不是 full solver。

## 3. Full DBLP g13 Gate

```text
n                  2,497,782
m                  12,786,329
zero arcs          162,166
root-star          17.427423110
dual primal        17.360814337
zero-half upper    17.360814337
row values         1,331,721
pushes/pops        1,955,225 / 1,955,225
triple partitions  183,183
triple probes      272,097
rows wall          2.164s
total probe wall   57.591s
```

rows 数量和 wall 都很小，但 restricted optimum 没有优于 dual greedy。与 ReleaseV3 在普通图上通过 work-triggered greedy 得到的 `15.0174` 相比，零残量拓扑过窄；增加更多 residual arcs 才可能改善，而那会重新引入 corridor 选择与状态膨胀。

## 4. 结论

1. dual zero-subgraph 上的 half/三块 DP 是合法、方法专属的 upper oracle，但对关键 full 查询无效。
2. 不把 `residual<=slack` 等 corridor 阈值直接启用；它虽可由 dual gap 派生，仍需先证明工作量边界，不能靠 full 数据调参。
3. 当前更直接的缺口是 Test21 尚未复用 ReleaseV3 已验证的、按实际工作购买的普通图 greedy。下一实验先用同一无参数 rent-or-buy 事件消除 incumbent 差距，再判断 D2 是否仍超出 V3。

Test41 只复用仓库已有 Wong dual-ascent residual；论文归属仍见主文档，没有新增引用。
