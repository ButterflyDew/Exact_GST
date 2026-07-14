# Test80：Anchor-Junction Progressive Ordered Rows

更新时间：2026-07-13。Test80 是框架 A 的带研究统计综合实现，也是 ReleaseV4
的算法来源。它从 Test21 的
anchor-aware 离线有序 rows 出发，合并 Test48 的 paid-anchor branch-junction
可行上界，以及 Test57/58 的 D2-work delayed progressive residual packing。
求解全过程仍是 `D/A ordered rows + A+D+D completion`，不调用 ReleaseV3，
不切换到 global-label 框架 B，也不使用 Hash、数据集特判或 wall-time 阈值。

## 1. 结论

| 项目 | Test80 | 对照 | 结论 |
| --- | ---: | ---: | --- |
| 随机 DPBF | `200/200 + 100/100` | `1e-6` | 全部一致 |
| 固定 g13 DPBF | `50/50` | `1e-6` | 全部一致 |
| small35 | `2.488s` | Test21 `2.252s` | 低 `g` 慢 `10.5%` |
| fast20 | `9.099s` | Test21 `12.380s` | 快 `26.5%` |
| Toronto full g13 | `8.722s / 58.0MiB` | Test21 `20.788s / 102.9MiB` | 快 `2.38x`，峰值小 `1.77x` |
| DBLP full g13 q1 | `666.509s / 2161.9MiB` | V3 `531.556s / 3870.7MiB` | A 慢 `25.4%`，峰值低 `44.2%` |

DBLP 最终权重为 `12.5936282853`。这是 A 系列第一次不依赖 B、不中途门禁并
真正完成 full DBLP g13 q1。随后已经验证的单一路径被清理为 ReleaseV4；本文件
继续负责研究选择、阶段计数和 Test80 原始数据，发行实现与当前实测见
`release_v4.md`。ReleaseV4 在同一 DBLP query 上得到相同权重，正式结果为
`544.379s / 2158.6MiB`；不能把本文件的研究统计开销当成发行版时间。

## 2. 为什么选择这组机制

本次不是把所有通过随机对拍的机制相加，而是按端到端证据选择：

1. 保留 Test21 的 pivot-oriented `D`、唯一 anchor 主干 `A`、不可拆分 branch bits
   和 `A+D+D` 完整化。这是无 Hash 的框架 A 完备基底。
2. 加入 Test48 branch-junction。它在 fast 把 D2 values/pops 约减半，并在 full
   把初始 incumbent 从 `17.3608` 降到 `13.01999`。
3. 加入 Test57/58 progressive packing。同步 residual 势在五库都不增加 D2
   states，且用 D2 实际工作决定是否购买，不依赖经验参数。
4. 不加入 Test39/70 one-third endpoint。Test70 fast 为 `9.161s`，但 Toronto
   full `9.64--9.69s` 慢于 Test58 `9.201s`；它只小幅减少高层 payload，不改变 D2。
5. 不加入 local seed-cone、第二 dual、recursive anchor 等正确但整体退化或只省
   常数的机制。对应证据保留在 `archive/`。
6. Test79 只把 junction 接入 V3/B，不能回答“A 是否可独立完成”。它已撤出活跃
   CLI，结果仅作为 `archive/test79_v3_junction_control_20260713.md` 的 B 对照。

## 3. 完整执行流程

```text
group distances + root-star root
             |
             v
farthest permanent anchor + directed-cut dual
             |
             v
root-to-anchor paid path P
             |
             v
triple attachment roots -> compressed parent tree subset DP
             |
             +----> legal branch-junction incumbent
             |
             v
offline ordinary D rows (sorted arrays + branch bits)
             |
             +----> measured D2 work reaches g(2m+n)
             |                |
             |                v
             |       progressive residual packing
             |       strengthens the same A lower bound
             v
offline anchored A rows
             |
             v
A + D + D exact completion
```

`D` 和 `A` 始终按 mask 大小离线生成。junction 只更新 `best`，packing 只更新
admissible potential；二者都不改变状态语义，不把 row 转成 global labels。

## 4. A 的状态与完备递推

设 permanent anchor 为 `a`，其余组集合为 `K`，`h=floor(g/2)`：

```text
D(S,v) = 覆盖 S subset K、根为 v 的最小普通树，1 <= |S| <= h
A(S,v) = 覆盖 a union S、根为 v 的最小 anchor 树，0 <= |S| <= h-1
```

`D` 固定 mask 最低位为 pivot。含 pivot 的一侧为累计侧，另一侧只枚举根处
不可继续拆分的 branch value；随后做一次带一致 future 的图闭包。若一个 `D`
value 可在根处分成两块，它可以递归拆到不可拆分分支再逐个接入，因此不会丢解。

`A` 每次只合并一个较小 `A` row 与一个不可拆分 `D` branch，再做图闭包。
最优树的 token-centroid 可把它分成唯一含 anchor 的一侧和至多两个普通侧，容量
分别不超过 `h-1,h,h`，所以最终的 `A+D+D` 扫描完备。

非 singleton row 只有两种表示：

```text
sparse: sorted vertices[] + distances[]
dense:  distances[1..n]
```

表示只比较真实字节数；branch 资格用与 row payload 对齐的 bitset。所有交集在
顺序归并和二分查找之间按显式操作数选择，没有 `unordered_map` 或密度阈值。

## 5. Branch-Junction 上界

从 root-star 根 `r` 到 anchor group 恢复一条确定性最短路 `P`，其成本只支付一次。
从 `P` 做 multi-source Dijkstra，得到每个顶点到 `P` 的距离和确定 parent。
对每个 nonanchor triple `S` 扫描：

```text
x_S = argmin_x d_P(x) + sum(i in S) gd_i(x)
```

把全部 `x_S` 到 `P` 的 parent paths 合并，只保留候选根、`P` 顶点和分叉点，
连续 degree-2 路径压成一条带长度的树边。在压缩树结点 `x` 上定义：

```text
F_x(M) = 在 x 的子树内服务 nonanchor mask M，且把启用部分连到 x 的最小成本
```

合并 child `y`：

```text
F'_x(M) = min(R subset M)
          F_x(M-R) + F_y(R) + [R != empty] length(x,y)
```

结点自身可用同根 star 服务任意子集。每个 DP 方案都能恢复为 `P`、parent-tree
edges 和 group shortest paths 的真实并，因此只产生合法 upper，不参与 lower 证明。

复杂度为：

```text
O(m log n + C(g-1,3)n + |C'|3^(g-1)) time
O(n + |C'|2^(g-1)) space
```

full DBLP 的 `P` 只有 4 个顶点，候选/压缩树均为 25 个结点；triple scans 为
`549,512,040`，subset convolutions 为 `13,286,025`，用时 `3.708s`。

## 6. Delayed Progressive Packing

正式 directed-cut dual 构造后暂时保留 directed residual capacity `r_e`。对每个组
`i`，在同一 residual graph 上计算到组的 directed distance，并在 paid path 上截断：

```text
q_i(v) = min(d_i^r(v), max(p in P) d_i^r(p))
```

`q_i` 在本组 terminals 为零。令 `a_ei=max(0,q_i(u)-q_i(v))`。对 active groups
同步增加 scale，步长取所有 residual arc capacity 与 group 剩余上限允许的最小值：

```text
delta = min( min_e r_e / sum(active i) a_ei,
             min(active i) (1-scale_i) )
```

arc 饱和时冻结在该 arc 上有正梯度的 groups，其余 groups 继续；最多 `g` 轮。
最终满足每条 arc 上 `sum_i scale_i*a_ei <= r_e`，所以
`potential_i + scale_i*q_i` 仍是合法 directed-cut potential。

packing 不是无条件预处理。D2 实际累计以下事件：全顶点 pair seed checks、heap
push/pop、settled adjacency scans。只有达到一次 packing 的静态工作上界时才购买：

```text
pair_work >= g * (2m+n)
```

该判据来自两组 directed arcs 和一组 vertex rows，不读取数据集、固定 `g`、层号、
密度或运行时间。若 D2 在此前结束，residual 直接释放。

## 7. 正确性边界

1. `D` pivot/branch recurrence 与完整同根 `D+D` 等价。
2. `A+D` 保留最优树中唯一含 anchor 的主干。
3. token-centroid 保证 `A+D+D` completion 覆盖任意最优树。
4. junction 只合并真实路径，故是 feasible upper。
5. progressive packing 逐 arc 不超过剩余 capacity，故是 admissible lower。
6. 所有剪枝仍只有 `partial + lower > best`；增强 lower 和降低 best 都不会删掉最优状态。

Release/O2 黑盒证据：

```text
seed 713901  g=2..13    200/200
seed 713903  fixed g13   50/50
seed 713911  final code 100/100
```

## 8. Release/O2 实测

### 8.1 Fast20

快照：`result_snapshot/fast/20260713_170142`。

| dataset | Test80 | Test21 | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `1.499s` | `2.375s` | `0.631x` |
| Toronto-new | `2.918s` | `5.529s` | `0.528x` |
| DBLP | `0.643s` | `0.595s` | `1.080x` |
| DBLP-new | `0.578s` | `0.785s` | `0.736x` |
| MovieLens | `3.461s` | `3.095s` | `1.118x` |
| **total** | **`9.099s`** | **`12.380s`** | **`0.735x`** |

20 条权重均与既有精确结果一致。DBLP 与 MovieLens 有局部反向，不能用总和掩盖。
当前结果略低于已撤回 Test70 `9.161s`，且不含其 one-third endpoint recurrence。

### 8.2 Small35 与 Toronto Full

small 快照：`result_snapshot/small/20260713_170239`，总计 `2.488s`。额外 junction
与 residual 保留使其比 Test21 慢 `10.5%`；这是当前明确保留的低 `g` 放宽项。

Toronto full：`result/Toronto/Test80/query_g13`。

```text
weight              0.7048467020
wall                   8.721789s
peak                  58.035MiB
junction upper         0.8334348456
ordinary values        2,709,269
anchored values          549,984
```

### 8.3 DBLP Full

结果：`result/DBLP/Test80/query_g13`。

```text
weight                 12.5936282853
wall                      666.509237s
peak                     2161.949MiB
junction before            17.3608143368
junction upper             13.0199888930
pair work                 530,400,720
packing budget            364,915,720
packing rounds                      2
packing scales min/avg/max 0.066667 / 0.138462 / 1.000000
ordinary values            44,972,384
anchored values            28,074,810
```

时间分解：

| component | time |
| --- | ---: |
| group distances | `18.113s` |
| initial dual | `36.490s` |
| branch-junction | `3.708s` |
| ordinary D，含 packing | `290.443s` |
| progressive packing | `27.285s`（已包含在 D） |
| anchored A，含 completion | `317.109s` |

D2 只保留 `7,756,381` values；旧 A 结构探针约为 `125.6M` pair states。这个下降
解释了为何本次能完成，但高层 A 仍使时间慢于 V3 `25.4%`。空间从 V3 的
`3870.7MiB` 降到 `2161.9MiB`，证明 ordered A 的内存优势真实存在。

q1 之后的跨询问剖析按截止时间完成 19 条新 DBLP g13 询问，详见 `test80_dblp_g13_cross_query.md`。新样本表明 q1 相对容易：其 wall 与 peak 只有新询问中位数的 `32.6%/36.8%`。跨询问时 D 在 16/19 条中比 A 更耗时，D3/D4 主导行存储，而 A5 与完整化仍是重要时间热点；因此不能再从 q1 单例推断“主要瓶颈总在 A”。这些是带统计 Test80 的研究数据，不替代 ReleaseV4 的发行时间。

## 9. 代码位置

| 文件 | 责任 |
| --- | --- |
| `methods/Test/test80_anchor_progressive.cpp` | D/A ordered rows、工作购买、完整化 |
| `methods/Test/test80_anchor_progressive.h` | Test80 统计与入口 |
| `methods/Common/anchor_junction_upper.cpp` | paid path、triple roots、压缩树 DP |
| `methods/Common/dual_cut_potential.h` | directed-cut dual 与 progressive packing |
| `main.cpp` | `Test80` CLI 和结果统计 |

## 10. 论文关系

- [Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302)：rooted subset DP 的经典来源；Test80 的 pivot/branch ordered-row 组织是仓库适配。
- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：Dijkstra-Steiner label setting 与一致 future 的理论背景；Test80 在离线 row closure 中使用 admissible future。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 路线来源；GST group potential 与 residual progressive packing 是仓库组合。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：统一 GST baseline；Test80 不把普通图/query 压缩计作自身贡献。

branch-junction parent-tree facility DP、D2-work delayed purchase 与同步 progressive
packing 是本仓库候选组合。本文只陈述实现、证明和实验，不在系统文献检索完成前
声称论文级原创性。
