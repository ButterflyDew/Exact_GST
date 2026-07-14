# Test21：Anchor 主干与不可拆分分支基

更新时间：2026-07-13。本文只描述当前 `methods/Test/test21_anchor_half.*`。Test21 是 A 框架下的研究实现，不切换到 ReleaseV3 的 global-label 方案 B，不使用全局 Hash，也不压缩普通图或 query。

> 状态更新：Test21 现已冻结为纯 A 基线。后续 Test80 在独立源码中重新接入 Test48 junction 与 Test58 progressive packing，并完成 full DBLP；本文后文“已撤回/未完成”的表述只描述 Test21 冻结时状态，当前事实见 `test80_anchor_progressive.md`。

## 1. 当前结论

Test21 把任意可行树看成一条包含 permanent anchor 的主干，以及沿主干接入的 ordinary 分支。普通侧 `D` 和 anchor 侧 `A` 仍是离线、按 mask 大小生成的有序 rows；新版本只让根处不可继续分解的 `D` 值作为分支接入，并把三/四块可行上界提前到对应 rows 刚可用的时刻。

| 项目 | 当前结果 |
| --- | --- |
| 正确性 | 随机 `g=2..12` 为 `500/500`；固定 `g=13` 为 `50/50`，均与 DPBF 在 `1e-6` 内一致 |
| fast20 | `12.380s`；旧 Test21 `13.438s`；ReleaseV3 `12.509s` |
| small35 | `2.252s`；旧 Test21 `2.476s`；ReleaseV3 `1.905s` |
| Toronto full g13 | `20.788s / 102.9MiB`；旧 Test21 `25.723s / 131.6MiB` |
| DBLP full g13 | 最新 gate 仍未在 V3 的 `531.556s` 时间线内完成，目标尚未达成 |

所以当前可以主张“跨库平均和 Toronto full 明显改善”，不能主张“DBLP 已不退化”。所有 DBLP gate 都被终止，没有最终权重，临时目录不作为成绩保留。

## 2. 主流程

```text
group distances + root-star + dual/TSP lower
                    |
                    v
      选择离 root-star 根最远的 permanent anchor
                    |
                    v
     ordinary D rows：pivot 主干 + 不可拆分分支
                    |
          q = ceil((g-1)/4) 层
                    |
    root-star 根上的四块标量 DP + 最多四次 witness lifting
                    |
          t = ceil((g-1)/3) 层
                    |
      每张 D_t row 完成时在线结算对应三块分区
                    |
          最佳三块 witness 最多三次 lifting
                    |
                    v
        anchored A rows：A + 不可拆分 D 分支
                    |
                    v
             A + D + D 完整化答案
```

`q`、`t` 都由分块完备性推出。源码没有固定数据集、固定 `g`、密度、运行时间或完成进度特判。

## 3. 状态定义

设查询组集合为 `U`，permanent anchor 为 `a`，并令：

```text
K = U - {a}
h = floor(g / 2)
```

只对 `K` 编码 mask：

```text
D(S,v) = 覆盖 nonanchor 组 S、根为 v 的最小树权重
          1 <= |S| <= h

A(S,v) = 覆盖 anchor a 与 nonanchor 组 S、根为 v 的最小树权重
          0 <= |S| <= h-1
```

`D` 使用同根不交集合并和图最短路闭包。`A` 只允许 `A(X,v)+D(Y,v)`，不做 `A+A`：含 anchor 的分支唯一，两个 A 相加会重复计算 anchor 侧结构，完备分解也不需要它。

状态族总数仍满足 Pascal 恒等式：

```text
sum(i=1..h) C(g-1,i) + sum(i=0..h-1) C(g-1,i)
= sum(i=1..h) C(g,i)
```

因此 Test21 没有声称 worst-case mask 数下降。消掉的是 ordinary DP 中的 anchor bit；实际收益来自 A/D 方向、分支基和更早的合法上界共同删除 payload。

## 4. 不可拆分 D 分支基

### 4.1 定义

生成 `D(S,v)` 时，先得到所有同根 split seed：

```text
split(S,v) = min D(X,v) + D(S-X,v)
```

随后做图闭包。若最终值满足 `D(S,v) = split(S,v)`，它在根 `v` 可继续拆分；若最终值严格小于所有 split seed，它必由边传播得到，记为根不可拆分分支。

源码不复制第二份 `(vertex,value)` row，只给原 row 的每个值保存一位 `branch_bits`。位图既能顺序检查，也能枚举 set bits；线性归并和二分查找按显式操作数择优，没有经验密度阈值。

### 4.2 为什么 A 只需不可拆分分支

若候选 `A(X,v)+D(Y,v)` 中 `D(Y,v)` 可拆成 `D(P,v)+D(Q,v)`，则：

```text
A(X union P,v) <= A(X,v) + D(P,v)
```

再接 `D(Q,v)` 可得到不更差的同一候选。对子块递归分解，mask 大小严格下降，最终只剩 singleton 或根不可拆分 D 值。因此从 A 主干接入所有可拆 D 整块是重复工作。

### 4.3 ordinary D 的 pivot recurrence

对目标 mask `S` 固定最低位 group 为 pivot。含 pivot 的一侧是累计主干，另一侧只取不可拆分分支：

```text
D(S-B,v) + branch_D(B,v) -> D(S,v)
pivot in S-B
```

任意根树都可从含 pivot 的根分支开始，再把其它根分支逐一接入；可拆分的根分支继续递归展开。因此该定向 recurrence 与完整 `D+D` recurrence 等价，但不会反复把已经可同根拆开的 D 值当作新分支。

## 5. 上界何时进入 DP

所有上界都来自真实 D/A trees 的同根并，只能降低 `best`，不参与 lower bound。

### 5.1 root-quarter 标量 witness

令 `q=ceil((g-1)/4)`。D_q 完成后，在已经确定的 root-star 根 `r` 上读取标量 `D(S,r)`，先做两块 pair DP，再拼互补 halves：

```text
P(M,r) = min D(X,r) + D(M-X,r), |X|,|M-X| <= q

candidate = gd[anchor][r] + P(M,r) + P(K-M,r)
```

该步骤只做小 mask 标量运算。Toronto full 中为 `21,649` 次 pair checks、`462` 次 complement checks，共 `15.7ms`，把 `best` 从 `1.1498267817` 降到 `1.0094809942`。

保存最佳四块 partition 后，最多选择四次“anchor 与其中一块先做图闭包”，再与其它三块相接。Toronto full 中它继续把 `best` 降到 `0.9726035549`。若标量 candidate 没改善现有 best，lifting 自然不运行。

### 5.2 在线三块结算

令 `t=ceil((g-1)/3)`。每个 `K=X union Y union Z` 至少有一块大小为 `t`；同层 rows 按 mask 递增生成。因此一个分区恰好在它最后一张所需 D_t row 完成时首次可用。

Test21 立即计算：

```text
gd[anchor][v] + D(X,v) + D(Y,v) + D(Z,v)
```

而不是等整层结束后再扫描全部分区。每个无序分区仍只结算一次，后续 D_t rows 已经能使用更小的 `best`。Toronto full 的 D4 retained values 从旧版 `4.333M` 降到 `1.684M`。

整层结束后保留最佳三块 witness，并最多做三次 anchor-side lifting。该阶段把 Toronto full 的 `best` 从 `0.9726035549` 降到 common-root 的 `0.8470137289`，再降到 `0.7876599892`。

## 6. A rows 与最终完整化

基础 row 为：

```text
A(empty,v) = gd[anchor][v]
```

之后按 `|S|=1..h-1` 离线生成 `A(S)`：

1. 枚举 `A(X)+branch_D(Y)`。
2. 用有序 row 交集产生 seeds。
3. 在当前 row 内运行带 admissible lower 的 priority queue 图闭包。
4. 对当前 A roots 在线结算剩余 `D(B)+D(C)`。
5. 用更新后的 best 过滤并保存有序 sparse/dense row。

最终完整化来自 token-centroid：最优树在某个根可拆成唯一含 anchor 的 A 块和至多两个 ordinary D 块，且块大小分别不超过 `h-1,h,h` 个 nonanchor groups。

## 7. 正确性要点

1. **D pivot 完备。** 含 pivot 的根分支作为累计侧，其它根分支递归拆到不可拆分基后逐一接入，可复现完整同根 recurrence。
2. **A+D 完备。** 根处分支时恰有一个分支含 anchor；其余分支均可作为 ordinary 分支接入 A 主干。
3. **A+D+D 完备。** token-centroid 保证存在一个 A 块和至多两个 D 块，容量不超过 half 边界。
4. **上界安全。** quarter、三块 common-root 和 witness lifting 都只并真实可行树。
5. **剪枝安全。** row 只在 `partial + admissible lower > best` 时删除；lower 取 farthest、TSP/2 与 directed-cut dual 的最大值。

随机对拍不能代替以上证明，但用于捕捉实现错误。当前 Release/O2 证据：

```text
seed 712491: g=2..12, 500/500
seed 712493: fixed g=13, 50/50
```

## 8. 表示与复杂度

非 singleton row 只有两种精确表示：

```text
sparse: sorted vertices[] + distances[]
dense:  distances[1..n]
```

只比较真实字节数选择表示。分支资格是与 row payload 对齐的 packed bitset；没有 `unordered_map`、全局 label id 或 mask-vertex Hash。

令 `P` 为 D/A mask 总数，`L` 为实际保留 payload：

- group distances：`O(g(m+n log n))`；
- TSP endpoint lower：`O(2^g g^3)` 时间、`O(2^g g^2)` 空间；
- subset joins：保守受 `O(3^(g-1))` 控制；
- 每张实际 row 至多一次图闭包，保守为 `O(P(m+n log n))`；
- rows 为 worst-case `O(Pn)`，实际有序 payload 为 `O(L)`；
- branch bits 为 `O(L)` bits；quarter 标量 DP 为 `O(3^(g-1))` 小 mask 运算。

priority queue 按二叉堆复杂度核算。本文不把稀疏实测写成更低的 worst-case 指数阶。

## 9. Release/O2 实测

### 9.1 fast20

当前 Test21：`result_snapshot/fast/20260712_050625`。旧 Test21：`20260712_012305`。ReleaseV3：`20260711_230000`。

撤回 Test44--47 后又以同一 Release/O2 源码完成 `100/100` 随机 DPBF 对拍（seed `712921`），并得到独立 fast20 快照 `20260712_173406`、总时间 `12.743s`。该次运行用于确认撤回完整性；它不替换下表同一实现的 `12.380s` 基准。

| dataset | 当前 Test21 | 旧 Test21 | ReleaseV3 | 当前最大 peak |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `2.375s` | `2.313s` | `1.493s` | `18.2MiB` |
| Toronto-new | `5.529s` | `6.050s` | `4.704s` | `37.9MiB` |
| DBLP | `0.595s` | `0.626s` | `0.581s` | `14.1MiB` |
| DBLP-new | `0.785s` | `0.862s` | `0.484s` | `11.9MiB` |
| MovieLens | `3.095s` | `3.587s` | `5.247s` | `183.8MiB` |
| **合计** | **`12.380s`** | **`13.438s`** | **`12.509s`** | - |

20 条权重与配对结果均在 `1e-6` 内一致。Toronto-fast 小幅反向，另外四库相对旧 Test21 正向；不能用总和掩盖单库结果。

### 9.2 small35

当前快照：`result_snapshot/small/20260712_050707`。

```text
current Test21  2.252s
old Test21      2.476s
ReleaseV3       1.905s
```

branch bitset 与 quarter 标量 DP 没有造成低 g 预处理失控，但 small 总体仍慢于 V3 `18.2%`。

### 9.3 Toronto full g13

正式结果位于 `result/Toronto/Test21/query_g13`：

```text
weight       0.7048467020
wall         20.788s
peak         102.9MiB
old Test21   25.723s / 131.6MiB
ReleaseV3     4.163s / 125.8MiB
```

相对旧 Test21，时间改善 `19.2%`，峰值改善 `21.8%`；相对 V3，时间仍明显落后。

| D layer | retained values | branch values | time |
| ---: | ---: | ---: | ---: |
| 1 | `0.553M` | `0.553M` | `<0.1ms` |
| 2 | `1.120M` | `0.976M` | `1.06s` |
| 3 | `2.616M` | `2.130M` | `2.02s` |
| 4 | `1.684M` | `1.273M` | `2.95s` |
| 5 | `0.378M` | `0.243M` | `3.26s` |
| 6 | `0.224M` | `0.132M` | `4.67s` |

### 9.4 DBLP full 边界

branch basis、在线 D_t 和 root-quarter 组合形成后才重新触发 DBLP gate。最新运行在 V3 的 `531.556s` 时间线之后仍未完成，采样 RSS 约 `4.97GB`；运行被终止，没有最终权重。此前不同中间版也出现同样结论，详细过程见 archive。

随后只加日志、不改算法的 Test26 将瓶颈定位到 ordinary D2：公共预处理与隐式 D1 在 `53.8s` 完成；直接运行约三分钟后 D2 仍没有完成，RSS 约 `2.12GiB`，此时 A 阶段尚未开始。该数量级与历史 full pair probe 的 `125.6M` settled states 相符。

后续 Test27--29 依次检查了 single-pair generated star、固定 root-star 根的 multi-pair matching，以及全部 fast D2 roots 上的 pair forest。DBLP 与 DBLP-new 均为 0 次 best 更新，D2 payload 完全不变；这些入口已全部撤出，也没有触发 full DBLP 长跑。

Test30 尝试只让 A+D 读取 ordinary D 的 local split seeds，从语义上删除 propagated branch；扩大随机图后出现 `exact=35 / candidate=36`，fast Toronto g12 也丢失精确值。该反例说明 split root 与 attachment root 之间的连接路径是必要信息，不能由单边界 A 状态统一延后吸收。

Test31 又尝试把 A 扩展到完整 nonanchor lattice、只接入完整 D1/D2 pendant cherries。独立小图在关闭所有相关剪枝后仍为 `exact=74 / candidate=75`：单根 A 状态无法免费改根到已经属于树的内部 attachment，因而会重复支付已有主干边。高阶 D block 不能仅靠连续 pair cherries 替代。

Test32 进一步重建每个 D1/D2 的确定 witness，并把合并成本免费播种到 witness 内所有顶点；同一反例仍失败。A 自身图传播产生的“更贵但包含更多已付费 attachment”的树会在旧 root 被更低标量 cost 删除，说明 cost 与 paid attachment set 是 Pareto 维度，单棵 witness 不能修复。

Test33 的 first-half global warm start 在 full DBLP g13 上用 `64.764s` 和 `5.47M` created labels 只把 `best` 从 `15.0174` 降到 `14.8912`；fast 收益也不跨库。该 B 上界 oracle 已撤出，不能用更深经验停止时刻包装成主线。

Test34 证明 root-star 单根 dual 的 objective 不是自由根 GST 下界，不能直接与 incumbent 相等停止。Test35 随后用 permanent anchor 的全部 terminals 构造合法 super-root dual；其 objective 虽是自由根下界，但 future 过度松弛。DBLP-fast g12 即使复用正式单根 dual 的精确 `best`，ordinary values 仍从 `50,267` 增到 `1,173,010`、wall 从约 `0.304s` 增到 `2.225s`。该替换已撤回，不触发 full DBLP。

Test36 将全部 D2 pair 看成共享 source profile 的向量波。五库所有 pair-root 值均精确，但不同 pair 的 source offset 产生异构 priority：DBLP-fast wave/reference 为 `1753/78ms`，MovieLens 为 `32051/708ms`，queue peak 分别达 `1.26M/17.42M`。Test37 则逐 rooted state 否决了只接 singleton/pair branch 的 recurrence：seed `712761` 第 11 个实例出现 `D(mask=63,root=6)=44/45`。二者均已撤出。

Test38 证明相邻 split seed 满足 `s(u)+w<=s(v)` 时可删除 `v` 的整张加权距离锥，并用 `sum degree <= S log S` 的操作数购买规则避免稠密图反噬。同机 fast20 改善 `1.6%`，Toronto full pops 减少 `25.4%`、wall 改善 `3.4%`；但 bounded DBLP 在约 `495.7 CPU-s / 3.98GB` 时仍无最终结果，且 surviving D2 values 数量级不变。该辅助优化已撤出，见 `archive/test38_local_seed_cone_20260712.md`。

Test39 进一步证明 `q=ceil((g-1)/3)` 以内的 exact closed D components 足以沿一条 backbone 构造 half-sized ordinary/anchored endpoint states，最终仍以 `A+P+P` 完成。它通过 `500/500` 与固定 g13 `50/50`，但 fast20 为 `12.695s`、Toronto full 为 `21.403s`；full P6 values 只比 D6 少 `4.3%`，D2 完全不变。因此状态解释归档、代码撤回，不触发 DBLP，见 `archive/test39_one_third_backbone_20260712.md`。

Test40 在 exact incumbent 下先生成 A1 roots，再按 `best-A1-future` 为每个 D2 pair 建立首批 consumer thresholds。full DBLP 的 target union 平均已占 `59.1% n`；min/median/max 三对即使保留正式 future pruning，仍需 settle 完整 D2 row 的 `99.95%/99.85%/99.73%`，wall 反而慢 `1.03x--1.33x`。因此 consumer-driven D2 已撤回。正向信息是 exact best 下这三对正式 rows 只剩 `126k/64k/12k` states，下一主线应在 D2 前产生更强的 anchor-aware 可行上界，见 `archive/test40_anchor_consumer_d2_20260712.md`。

Test41 随后在 directed-cut dual 的零残量有向子图上运行离线 half rows 与二/三块 completion。fast Toronto-new/DBLP-new 的 upper 从 `5.71717/11.00643` 降到 `5.00459/10.99742`，但 full DBLP 用 `2.16s / 1.33M` row values 后仍停在 dual primal `17.360814`，没有改善。零残量拓扑过窄，机制已撤回，见 `archive/test41_zero_residual_half_upper_20260712.md`。

Test42 再把 ReleaseV3 的 ordinary-graph greedy 按 `g(m+n log n)` 实际 row 工作购买。fast20 从 `12.380s` 降到 `12.182s`，12/20 条触发、10/20 条改善 incumbent；但 full DBLP 在 `558.7 CPU-s / 4.31GB sampled peak` 时仍无最终结果，已超过 V3 完整时间线。该共享 upper 工程不足以解决 D2，统计与代码均撤回，见 `archive/test42_delayed_greedy_20260712.md`。

Test43 用 `q=ceil((g-1)/3)` 的 singleton-only anchored caterpillar rows，在 D2 前尝试三个 C blocks 同根 completion。fast Toronto-new/DBLP-new 的逐 row star upper 降到 `4.46074/10.82456`，但三 C completion 五库均为 0 次更新；full 即使预先给出合法 greedy `15.0174`，在 `234.8 CPU-s / 2.33GB sampled peak` 时仍未完成 size 1。显式 early A1/C1 也形成全图传播障碍，代码已撤回，见 `archive/test43_anchor_three_upper_20260712.md`。

Test44--47 随后固定 root-star 到 anchor 的已付费最短路 `P`，把每个 block 的任意-root attachment 压成 `E(S)=min_x[d_P(x)+sum gd_i(x)]`，再做 partition 与 witness lifting。该方向使 fast20 最低达到 `10.112s`，full early upper 最低达到 `13.161912`，sampled peak 降到约 `2.18GB`；但四个集成版 full gate 均未在 V3 的 `531.556s` 内完成，最好轨迹也在约 `543.1 CPU-s` 后仍无最终结果。因此正式代码撤回；有效的下一边界是扩展共享 skeleton 的几何覆盖，而不是增加更多 scalar block sizes。详见 `archive/test44_47_paid_backbone_upper_20260712.md`。

Test48 用所有 triple attachment 的精确 argmin roots 构造到 `P` 的确定性最短路父树，并在压缩树上做 subset facility DP，使多个 roots 的公共路径前缀只付费一次。它把 full upper 继续降到 `13.019989`；集成版随机 `300/300`，fast20 达到 `9.758s`，D2 values/pops 相对正式版降到 `0.50x/0.45x`。但按 `weights.txt` header 后的正确 query 口径，full 在 `535.507s / 2087MiB sampled peak` 仍无最终权重，超过 V3 `531.556s`。路径 witness union 没有继续改善，因此代码撤回；详见 `archive/test48_branch_junction_closure_20260712.md`。

Test49 进一步利用 pair predecessor forest：对任意 closed consumer `A`，若 forest edge `p->v` 上 `A(p)<=A(v)`，则 `v` 的合并 source cone 被 `p` 支配。full exact-best 的 66 个 nonanchor pairs 有 `106.31M` states，但 permanent-anchor gradient sites 只有 `8.95M`（`8.416%`）；共享 `(directed arc,pair bits)` DAG 仅 `449MB` 且枚举/回溯 0 error。可惜逐 pair、只批 A、同时批 ordinary+A 三个集成版 fast20 分别为 `16.834/13.245/14.941s`：把 sites 显式分发给 consumer masks 后重新产生 `28.95M` events 和高峰值。代码撤回；下一步必须在 mask 维也做符号共享，详见 `archive/test49_pair_gradient_arc_dag_20260712.md`。

Test50 不再展开 events，而为每个 accumulator 下降 arc 保存 `(vertex,value,pair-bitset)` factor，targets 再测试对应 pair bit；同时以 arc bytes 与所有 pair rows 的真实最小 certificate bytes 做无参数购买。随机 `500/500` 正确，fast peak 较 Test49 回落，但 `5.42M` factors 产生 `125.0M` bit probes、只有 `27.1M` 命中，fast20 仍为 `14.080s`。因此 event expansion 与 target membership 两种接口都撤回，详见 `archive/test50_factorized_pair_mask_join_20260712.md`。

Test51 为 Test48 的每个 triple 同时加入路径付费 `argmin(q+d_P)` 和共享前缀免费 `argmin(q)` 两个 exact marginal roots。它在 fast Toronto/DBLP-new 上降低 upper，但 full DBLP 把 candidates/tree 从 `24/20` 扩到 `84/86` 后仍为 `13.019988893`，没有超过 Test48。故同一 parent tree 的 facility 扩张已撤回，详见 `archive/test51_marginal_junction_endpoints_20260712.md`。

Test52--53 又从 Test48 facility assignment 恢复 attachment-token centroid，用它构造第二个合法 dual orientation。full D2 的 max-dual states 降约一成，但 replay wall 反而增加，且第二 dual 另需约 39 秒；按 attachment 深度把信息压进一次 group order 的 fast20 为 `11.565s`。因此 primal skeleton 对 dual 的局部增益成立，但现有两种消费接口都不回本，代码与公共 API 已撤回，详见 `archive/test52_53_primal_skeleton_dual_feedback_20260712.md`。

撤回后正式源码已重新通过 Release/O2 编译和随机 `100/100`（seed `713131`）；Test51--53 的 helper、probe、统计字段、公共 dual API 与生成二进制均不在当前实现中。

Test54 又在一次 residual construction 中令每个 group moat 覆盖整条 paid anchor path。机制随机 `300/300` 精确，Toronto-new D2 近乎减半；但 DBLP/DBLP-new values 分别膨胀到正式版 `18.38x/4.40x`，fast20 `15.347s`。顺序式 cover-all charge 已撤回，详见 `archive/test54_anchor_path_covering_dual_20260712.md`。

撤回后正式源码已通过 Release/O2 编译与随机 `100/100`（seed `713151`），不保留 covering-root API、路径恢复或调试字段。

Test55 把 Test54 的最远路径 cap 改成 root-preserving 路径均值。随机 `300/300` 精确，fast20 `13.198s`；DBLP/DBLP-new D2 values 仍为正式版 `16.31x/2.48x`。因此 sequential scalar-cap 路线整体撤回，不再尝试 path median、插值或其它聚合，详见 `archive/test55_barycentric_anchor_path_dual_20260712.md`。

撤回后正式源码已通过 Release/O2 编译与随机 `100/100`（seed `713171`），不保留 barycentric-root API、路径恢复或实验字段。

Test56--58 转为 group-order-independent 的同步 residual packing，用实际 D2 工作购买，并与 Test48 junction upper 组合；progressive filling 版达到 fast20 `9.390s`、Toronto full `9.201s/58.0MiB`。full DBLP peak 降到 `2094MiB`，但 header 后 `531.671s` 仍无最终 weight，因此不满足 V3 门槛，全部撤回。详见 `archive/test56_58_balanced_residual_packing_20260712.md`。

撤回后的正式 Test21 已通过 Release/O2 编译与随机 `100/100`（seed `713221`），不保留 junction helper、balanced residual API、D2-work 购买逻辑或实验统计字段；本轮不重复运行 full DBLP。

Test59 进一步把 ordinary split 定向为“较小侧作为根不可拆分 branch，等分时由 pivot 消歧”。任意可拆根树确实总存在大小至多半集的不可拆分根分支；随机 `500/500`（seed `713231`）通过。但 fast20 为 `13.016s`，ordinary/anchored/completion payload 与正式版均只相差 `0.02%` 以内，说明该定理没有删除 rooted 状态，只重排了等价 split。代码已撤回；正式版随机回归为 `100/100`（seed `713241`）。详见 `archive/test59_balanced_irreducible_branch_20260712.md`。

Test60 随后把最高 `D_h=C(M_h)` 的 closure 通过 `min C(f)+g=min f+C(g)` 转到 anchor side。nonanchor 数为 `2h-1` 时可删除全部 top-D 传播，fast g12 改善 `3.8%`；但 DBLP g13 所属的 `2h` 情形需要互补 A-half generators。Toronto full 中 D6 values/pops 减少约 `45%`，A merge probes 却从 `43.40M` 增至 `73.42M`，wall 从 `20.788s` 退到 `25.121s`。代码已撤回，不触发 DBLP；正式版随机回归为 `100/100`（seed `713261`）。详见 `archive/test60_top_closure_transposition_20260712.md`。

Test61 再利用 `min C(Q_N)+D_C=min Q_N+D_C`，把非 canonical A-half row 改成由 closed-D roots 驱动的 `A_X+branch-D_Y+D_C` 三函数 scalar completion。随机 `500/500`（seed `713271`）精确；Toronto 相对 Test60 回收约 `1.28s`，但 canonical A-half 仍把 merge probes 推到 `60.34M`，总时间 `23.838s`，未越过正式 `20.788s`。代码撤回且不运行 DBLP；正式版随机回归为 `100/100`（seed `713281`）。详见 `archive/test61_three_function_scalar_completion_20260712.md`。

Test62 最后用 anchor-aware target A* 直接求 `min C(Q_C)+M_N`，彻底取消 A-half rows。随机 `500/500`（seed `713291`）精确；Toronto target 仅弹出 `22.5k` states，但 top generator 和 point-query 成本仍使总时间为 `23.786s`。因此 Test60--62 的显式 row、三函数 scalar 与 direct target 三种 top closure 接口均撤回，不运行 DBLP；正式版随机回归为 `100/100`（seed `713301`）。详见 `archive/test62_canonical_half_target_astar_20260712.md`。

随后对 pair-mask symbolic 路线完成文献边界核验：经典 fast subset convolution 在 min-sum semiring 上的 exact 版本依赖有界整数范围 `M`，不适用于当前实权与 `1e-6` 口径；近年的 strongly polynomial 结果是近似算法。full DBLP 的 `162,166` 条 directed zero-residual arcs 即使全部用于 contraction，也只能让 `2,497,782` 个顶点最多减少 `6.49%`。因此 generic Möbius/FFT 与 zero-SCC quotient 都不是 D2 数量级机制，详见 `history/d2_symbolic_convolution_boundaries.md`。

Test63 检查 `gd_i+gd_j` 是否本身已为 1-Lipschitz，从而让某些 D2 rows 无需 closure。五库 fast g12 共 `275` 个 nonanchor pairs 全部有大量严格 violation，零张 row 可删除；最少 violation 数仍为 `2528`。临时 probe 已撤回，不触发 full，详见 `archive/test63_pair_lipschitz_certificate_20260712.md`。

Test64 按三端点 lower 估计每个候选 anchor 会删除的 incident D2 工作。full DBLP 的结构评分准确选中事后最优组 6，但只把 exact-best pair settled 从 farthest 组 12 的 `106.31M` 降到 `104.75M`；集成 fast20 虽为 `11.454s`，DBLP/DBLP-new/MovieLens 分别退化约 `26.0%/9.1%/13.2%`。单独换 anchor 只是重分配同一状态族，已撤回，见 `archive/test64_pair_work_anchor_20260712.md`。

Test65 将 anchor 分解递归应用到 ordinary lattice：用 secondary anchor 把 `D` 精确写成 `D0+B`，先生成不含 secondary 的全部 `D0`，再生成含它的 `B`。Pascal 状态数与 recurrence 完备性均保持，随机 `500/500` 通过；但有效 upper 被推迟，fast20 为 `13.755s`。由 `ceil((g-2)/4)` 推出的双-anchor四块结算在 `16/20` 条上零更新，修正版仍为 `13.617s`。该等价状态排序已撤回，见 `archive/test65_recursive_anchor_order_20260713.md`。

Test66 随后恢复一棵同时连接两个最远组、按 edge id 只付费一次的 backbone，其余 groups 以最多四个 singleton/pair/triple attachments 接入。该 upper 在 `18/20` 条改善初值，并使 DBLP-new/Toronto-new ordinary payload 降约 `8.4%/8.6%`；但额外图搜索使单独版 fast20 为 `12.867s`，与 recursive ordering 组合仍为 `12.889s`。group-metric cycle/2 全局证书 `0/20` 命中。代码全部撤回，不触发 full，见 `archive/test66_dual_anchor_backbone_20260713.md`。

Test67 把 Test48 的 attachment candidate tree 迭代到“所有至多 quarter-sized block 的最优接入根都已在树上”的无参数 fixed point，再在压缩树上按选择付费。fast g12 均在 `2--4` 轮、`4--62` 个压缩点稳定；Toronto/DBLP-new upper 仅小幅改善，DBLP g12 完全不动，五条 g12 仍距 exact 约 `0.04%--6.60%`。随机 upper 合法性 `500/500`（seed `713421`）；源码撤回且不触发 full，见 `archive/test67_skeleton_fixed_point_20260713.md`。

随后把 Test49 的 parent-gradient 支配推广到任意 predecessor ancestor：pair frontier 只需 consumer 在 root-to-vertex 路径上的严格 prefix minima；再沿全部严格增距 tight arcs 传播可得到更小的 tight cone。随机闭包逐点等价 `2000/2000`（seed `713451`）。fast Toronto/Toronto-new/DBLP-new 的 sites 降到 Test49 parent sites 的 `72.40%/70.24%/68.65%`，但 DBLP/MovieLens 仍为 `98.15%/99.61%`，不触发 full。定理保留在 `history/pair_tight_prefix_frontier.md`；它没有消除 consumer-mask 乘数。

Test69 再把 pair state 按相同 `(split root,attachment)` 合成 rank-1 path profile；固定 profile 的所有 pair-mask joins 可精确写成两次 singleton transform。fast DBLP profiles 只有 states 的 `7.52%`，因此触发一次 full 结构测量；full `106.31M` states 仍有 `53.17M` profiles（`50.02%`），平均只共享 2 个 pairs。profile 级 `O(k2^k)` transform 不可接受，代码撤回且不运行 full Test21，见 `archive/test69_pair_path_profiles_20260713.md`。

Test70 将 Test39 one-third endpoint 接到 Test58 的 junction incumbent 与 delayed progressive packing。随机最大 `500/500` 精确，fast20 达到 A 线新低 `9.161s`，Toronto full 为 `9.64--9.69s / 58.1MiB`；但有效 DBLP gate 在 `531.831s / 2160.4MiB` 强制停止时仍无 weight，严格慢于已完成的 V3 `531.556s`。组合代码与公共 API 全撤回，见 `archive/test70_one_third_progressive_junction_20260713.md`。

Test71 把 endpoint closed-component 阈值进一步压到 `q=ceil(h/2)`，并禁止高层 P 发布 branch；seed `713581` iteration `428` 给出 `DPBF=20 / Test71=21` 的完备性反例。禁用 packing 后反例不变，junction upper 也未参与剪枝，故错误明确来自 P/A 接口。Test72 保留 exact D 语义，只把大侧作为 accumulator、小侧作为 branch，累计随机 `3000/3000` 正确；但每个无序 partition 仍访问一次，fast20 为 `12.843s`，慢于正式 `12.380s`。两版均撤回且不运行 full，见 `archive/test71_72_quarter_endpoint_20260713.md`。

Test73 保持 ordinary D 完全离线，只把 A 改成 goal-directed global consumer，并用 root 倒排的 irreducible D values 做宏转移。随机 `100/100` 精确；Toronto-fast g12 settled A 仅降 `6.7%`，created labels 增至 `220,259`，wall 从正式 `1.791s` 退到 `6.288s`。这补齐了 Test22/23 未覆盖的最后一个调度象限，也确认只移动 A 调度不能解决 D2；源码与入口已撤回，见 `archive/test73_offline_d_global_a_20260713.md`。

Test74 再把 ReleaseV3 切换前已经生成的 exact nonanchor half rows 作为 global 宏种子。随机 `300/300` 精确，但 fast20 的 settled labels 与 V3 在每条查询上完全相同（汇总均为 `1,939,448`），created 反而增加 `15,592`，wall `12.509s -> 13.176s`。因此 A half values 只是 B recurrence 可重建的路径缩写，不能靠 seed 注入形成共享；候选撤回且不运行 full，见 `archive/test74_half_seed_global_20260713.md`。

Test75 后续没有把 paid profile 直接塞回 A rows，而改为低维 plan 定价。fast g12 的
strong lower 可在 `4--2253` 次 plan pricing 后闭合；Toronto g13 的 skyline
`3-core+5+5` plan 经全 root 定价得到 exact `0.7048467020`。但临时 Test77 在 A4 后
执行该 upper 需要 `6.097s`，且因缺少 plan-level 完备证书仍要继续 D6/A5，最终
`29.113s`，慢于正式 `20.788s`。临时代码与统计已撤回，正式版随机 `100/100`
（seed `713931`）；没有运行 DBLP。详见
`archive/test75_pair_block_completion_20260713.md` 第 9 节。

随后 fixed g13 穷举推翻了 core3 完备性：pair-only 为 `160 -> 161`，
`q5/core3+5+5` 为 `106 -> 107`，且放开全部 singleton attachment points 仍不修复。
多声明一组的 `q5/core4+4+5` 在两种生成顺序的 g13 证据累计 `720/720`，扩展 odd-g、
Steiner 顶点、多终端组和高环数后 targeted 总计 `13,520/13,520`。Toronto g13 也以
`23,864` core states 得到 exact。双 pair-forest requested-profile 因子化已把同一
`5,476` strong plans 的显式定价从 `39.158s` 降到 `18.721s`；五库 fast 的 attachment
sets 与逐 plan prices 对照均为零误差。但这只证明两种定价实现等价，尚未证明 core4
family 完备，且诊断使用 supplied optimum 过滤 plans。正式 Test21 未修改、D6/A5 未
删除、未运行 full DBLP；新理论问题见 Test75 记录第 10 节。

直接使用 rooted-optimal D4 witness 可把 Toronto 同一计划族再降到 `8.023s`，但不能
成为正式替代：fast MovieLens 的 deterministic D4、全部 tight D4 witnesses 三标量
闭包和六标签 macro DP 都停在约 `0.02022036`，高于 exact `0.0202189774`。这证明需要
保留非最优 paid-core 的 cost/profile Pareto geometry，而不是只恢复更多等价最短 witness。

Test75 的理论基底改为 paid-attachment half profile：状态保存整棵 anchor-paid subtree 的成本，以及每个未来 block 到该树任意内部点的最小 attachment cost；不再保存单一 root。`paid-half + D + D` 已证明为 exact completion。穷举随机累计 `1250/1250` 精确；fixed `n=9,g=8,m=13` 将 `7.52M` declared subtrees 压为 `42,963` 个 Pareto states（`0.572%`），最大单 mask front `117`。这是当前保留的正方向，但大图生成和 D2 消除尚未证明，见 `history/paid_attachment_half_profiles.md`。

随后 pair-profile probe 证明单-anchor projection 可直接由 `(D2(v),gd_anchor(v))` skyline 完整表示。五库 fast g12 只保留 roots 的 `0.407%--1.672%`；full DBLP g13 把 `106,310,433` 个 D2 roots 压到 `821` 个 skyline states、每对最多 `23`，结构探针总计 `139.710s`。但把所有 remaining singleton distances 强制放在同一 root 后，Toronto 两版 frontier 回升到 `49.5%--62.1%`；因此下一步必须生成可在 paid tree 不同内部点取最小值的多-block profile，不能用 root vector 冒充。详见 `history/paid_attachment_half_profiles.md`。

确定性内部骨架进一步确认了这个边界：fast 五库的多-singleton witness skyline 为 D2 roots 的 `4.388%--24.728%`，且绝大多数状态不在单-anchor skyline 中。逐个把剩余 singleton 接到骨架上虽在 fast 五库得到三条 exact、其余 gap 小于 `1%` 的上界，但 full DBLP g13 为 `13.3957483032`，距 exact `6.3693%`；结构探针耗时 `262.877s`、峰值约 `4.81GiB`。所以该方向只保留为 paid-tree 候选生成器，completion 必须从 singleton 求和升级为两个 half block 的 `phi_T(B)+phi_T(C)`。

升级后的 pair-only 两块 completion 在五库 fast g12 全部 exact；相反，先把 anchor path 付费会在 Toronto/Toronto-new/DBLP 留下 `2.330%/3.341%/0.08366%` gap。unrestricted rooted-pair family 的新增穷举为 `3000/3000`，但只保留 permanent-anchor skyline 为 `2998/3000`，已有 `exact=59 / skyline=60` 反例。故 anchor 真正的作用是提供极小初始列与定价方向，不是把 skyline 伪装成完备替代；详见 `archive/test75_pair_block_completion_20260713.md`。

full DBLP g13 对这批初始列做 incidence-only 回溯后，`821` 棵 trees 总计只有 `3,302` 个 tree-vertex incidences、`836` 个 unique target vertices，单树最多 `7` 点；结构探针 `166.382s / 约1.32GiB`。因此后续 A 不再维护 `anchor mask x root` rows，而尝试只生成 `(block,target vertex)`，再按短有序 tree incidences 离线归并。

Test64--67 的共同结论是：anchor 确实决定哪些 pair rows 被延后，递归 `D0+B` 也有干净的精确语义；但换 anchor、增加 anchor 数、重排 masks 或迭代同一 conditional parent tree，都不会给出覆盖最优 paid skeleton family 的 exact 表示。正式源码已恢复并通过 Release/O2 随机 `100/100`（seed `713391`）。

因此当前大实例门槛仍然失败。paid backbone 与 branch-junction closure 已分别证明 block-root 标量化和共享路径前缀有效，但二者都仍把 skeleton 只当作 incumbent oracle，未删除 ordinary D2。下一步必须让共享 skeleton 直接承担 pair 连接信息，或从完备性中删除/隐式表示 D2；不再扩更多 scalar block sizes、anchor 数、facility roots、split orientation 或上界 witness。一般图上同一 `(S,v)` 可有 `2^r` 个 cost/attachment Pareto 不可比 partial trees，故显式小 representative family 没有通用保证；证明见 `history/paid_attachment_representative_family.md`。显式 `n^2` 边界、固定数量 witness、独立 A blocks 求和、更多 parent trees、consumer target wave或只更换同一批 row 的扫描方式都不能解决首要瓶颈。

## 10. 文献与原创性边界

- [Dreyfus-Wagner](https://doi.org/10.1002/net.3230010302)：subset Steiner DP 与同根合并的经典来源。
- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：future cost、consistency 与 label-setting 理论背景。
- [PrunedDP](https://doi.org/10.1145/2882903.2915217)：GST baseline；普通图/query 压缩不计作 Test21 收益。
- [Wong dual ascent](https://doi.org/10.1007/BF02612335)：directed-cut dual-ascent 来源。
- [Fuchs et al.](https://doi.org/10.1007/s00224-007-1324-4)：猜测 separator terminals、按单点重叠拼小组件的已知路线；Test21 不采用其 `n^q` separator 枚举。
- [Rank-based representative sets](https://arxiv.org/abs/1305.7448)：有界 tree-decomposition bag 上的 connectivity 压缩；Test21 没有对应的有界边界，不能直接据此压缩全图 paid attachments。

当前仓库内的新研究候选是：permanent-anchor A 主干、root-irreducible D branch basis、pivot 定向 recurrence、row 偏序驱动的在线三块结算，以及 root-star 标量 quarter partition 与有限 witness lifting 的组合。系统文献检索尚未完成，文档不宣称论文级原创性。
