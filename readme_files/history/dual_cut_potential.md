# GST Dual-Cut Potential：共享容量下界与零残量上界（组件历史）

更新时间：2026-07-10。本文说明 directed-cut 势函数组件：把 Group Steiner Tree 的组约束写成有向组汇点 cut relaxation，从一个压缩的 dual-ascent 解中提取可加、edge-consistent、满足 subset splice 的 future potential，并从零残量子图恢复可行上界。

> 状态更新：本文第 9 节冻结的是 distance-only solver 的有界结果，该 solver 自身没有完成 full g13。势函数随后形成 ReleaseV2，并进一步进入 ordered-half/global 的 ReleaseV3；当前主结果见 `../release_v3.md`，ReleaseV2 原始逐层计数见 `../release_v2_evidence.md`。

实现位于：

```text
methods/Common/dual_cut_potential.h
tools/dual_cut_probe/dual_cut_probe.cpp
tools/distance_epoch_solver_probe/distance_solver.cpp
```

本文的旧 distance solver 没有进入 Test19。当前共享实现位于 `methods/Common/dual_cut_potential.h`，由 ReleaseV3 与研究探针复用；旧 row solver、ReleaseV2 私有实现和 ReleaseV3 实测仍按文档归属分开维护。

## 1. 有向组汇点模型

把每条无向边 `{u,v}` 替换为两条费用均为 `w(u,v)` 的有向弧。对每个组 `A_a` 增加只有入弧的汇点 `t_a`：

```text
x -> t_a, cost 0, for every x in A_a
```

固定根 `r` 后，任何包含 `r` 的 GST 都可朝外定向，并经某个组候选点进入 `t_a`；反过来，连接全部 `t_a` 的有向树去掉汇点后就是合法 GST。只有入弧这一点很重要：不同候选点不能通过 `t_a` 免费互相切换。

对每个不含根、但含某个组汇点的集合 `W`，directed-cut relaxation 要求至少一条弧进入 `W`。它的 dual 给每个此类 cut 一个非负权重，且穿过任一弧的 dual 权重总和不能超过该弧费用。

## 2. 压缩的 Wong dual ascent

显式维护指数多个 cuts 没有必要。设当前有向残量费用为 `c'`，依次处理组；顺序按原图 `gd[a][r]` 从大到小，平局按组编号。这是确定性的 farthest-first dual ascent，不含数据集或组数阈值。

处理组 `a` 时，令：

```text
d_a(v) = v 到 A_a 的残量有向最短距离
L_a    = d_a(r)
p_a(v) = min(d_a(v), L_a)
```

然后对每条弧 `u->v` 更新：

```text
c'(u,v) <- c'(u,v) - max(0, p_a(u)-p_a(v))
```

最短路三角不等式保证减后残量非负。`p_a` 等价于同时增长嵌套 cuts

```text
W_tau = {v | d_a(v) <= tau}, 0 <= tau <= L_a
```

的压缩表示。所有组共用同一份残量，所以它们不会在共享主干上重复消费边费用；落在不同分支的 cuts 则可以分别计费。这是它相对 group metric TSP/2 的核心差异。

### 增量残量最短路

原图的全部 `gd[a][v]` 已经由 ReleaseV1 预处理。残量弧只会降权，因此：

- 第一个组直接复用对应 `gd`，不重跑 Dijkstra；
- 后续组从原始 `gd` 出发，先扫描累计降权后违反 Bellman 不等式的弧；
- 只把受影响顶点压入堆，再沿反向弧传播距离下降。

若没有局部违反，旧距离已经满足新图全部 Bellman 不等式，因而仍是精确最短距离；若有违反，非负权下降的堆传播会得到精确新距离。实现直接复用 `graph.adj`，不复制 `2m` incoming 邻接。

## 3. Future potential

对任意剩余组集 `R`，定义：

```text
h_D(v,R) = sum_{a in R} p_a(v)
h(v,R)   = max(h_TSP/2(v,R), h_D(v,R))
```

### Admissibility

保留 owner 属于 `R`、且不含根 `v` 的 dual cuts。连接 `v` 与 `R` 中全部组的任意树必须跨过每个保留 cut；共享弧容量约束保证全部 charge 不超过树费用。因此 `h_D(v,R)` 是 rooted GST 的下界。

### Edge consistency

对图边 `uv`，`h_D(u,R)-h_D(v,R)` 的正项恰来自包含 `v`、不含 `u` 的 cuts，也就是进入这些 cuts 的弧 `u->v` 所承受的 dual charge。故：

```text
h_D(u,R) <= w(u,v) + h_D(v,R)
```

反向同理。

### Subset splice

对 `R' subset R` 和任意 `u,v`，需要满足 Dijkstra-meets-Steiner 的强条件：

```text
h_D(u,R) <= h_D(v,R') + OPT({u,v} union (R-R'))
```

对 `R-R'` 的 owner cuts，连接桥必须从 `u` 到达对应组；对 `R'`，只保留使 `p_a(u)-p_a(v)` 为正、即把 `u` 与 `v` 分开的 cuts。桥树必须跨过这两类 cuts，而弧容量仍限制总 charge，所以上式成立。

两个满足 splice 的下界取 `max` 后仍满足 splice：分别应用不等式，再对右侧取同一个最大值即可。因此 dual-cut 可以与已有 TSP/2 安全组合，也适用于组重叠、partial tree 顺路命中其他组的情形。

## 4. 零残量 primal

每个组处理结束时，至少有一条从 `r` 到该组候选点的零残量有向路径；后续 residual 只下降，这条路径不会消失。实现只在零残量弧子图上运行确定性 grow-greedy，把当前树连接到最近的未覆盖组，得到合法 GST 上界。

fast g12 中：

- DBLP 直接得到最终最优 `12.166303`；
- MovieLens 直接得到最终最优 `0.0202189774`；
- DBLP-new 把初始上界从 `11.6953` 降到 `11.0064`。

该 primal 只使用 dual 已产生的零残量结构，不做图/query 压缩。

## 5. 无组数特判的工作量门控

无条件构建 dual 会在 small 的 `g=4..6` 上慢于 PrunedDP。当前实现不使用 `g>=7` 一类阈值，而比较两个由算法直接给出的工作量：

```text
W_DP = n * sum_{k=2..H} C(g,k) * (2^(k-1)-1)
W_D  = 2g * (m + n*ceil(log2 n))
```

`W_DP` 是 half-DP 在 dense 情形下所有无序 proper splits 的基本 join 次数；`W_D` 的两份 `g(m+n log n)` 分别对应 dual residual shortest paths 和零残量 primal。仅当 `W_DP>=W_D` 才构建 dual。

这是静态 recurrence 成本比较，不读取数据集名、固定 `g`、固定层、状态密度、墙钟时间或运行进度。它与 ReleaseV1 complement cache 的 rent-or-buy 原则相同，但发生在查询开始前。

## 6. 证明与正确性验证

全部构建使用 Release/O2。

| check | result |
| --- | --- |
| dual 穷举随机，`g<=7` | `ALL_OK seed=710881 iterations=300` |
| rooted admissibility | `94,456` checks，`0` violations |
| edge consistency | `166,596` checks，`0` violations |
| exact subset splice | `9,970,118` checks，`0` violations |
| 当前 solver 随机，`g=2..10` | `ALL_OK seed=710883 iterations=1000` |
| 当前 solver 固定 `g=13` | `ALL_OK seed=710887 iterations=100` |
| small 35 条 | 35/35 与 ReleaseV1 权重一致 |
| fast 20 条 | 20/20 与 ReleaseV1 权重一致 |

splice 检查对每个随机图枚举全部 `R' subset R`、全部 `from/to`，bridge 真值由额外 singleton 组的 exact rooted DP 得到，不是抽样普通最短路。

## 7. Small：预处理不反噬

5 个数据版本各一条，按 `g` 汇总：

| g | dual enabled queries | ReleaseV1 | current solver | current / Release | PrunedDP speedup |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 0/5 | `0.0337s` | `0.0328s` | `0.974x` | `5.32x` |
| 3 | 0/5 | `0.0518s` | `0.0499s` | `0.964x` | `3.91x` |
| 4 | 0/5 | `0.0868s` | `0.0867s` | `0.999x` | `2.41x` |
| 5 | 0/5 | `0.1119s` | `0.1107s` | `0.990x` | `2.34x` |
| 6 | 0/5 | `0.1659s` | `0.1669s` | `1.006x` | `2.27x` |
| 7 | 0/5 | `0.3624s` | `0.3700s` | `1.021x` | `7.84x` |
| 8 | 4/5 | `0.8842s` | `0.8806s` | `0.996x` | `8.59x` |

门控前 `g=4..6` 曾只有 PrunedDP 的 `0.59x--0.86x`；门控后恢复到 `2.27x--2.41x`，因此旧负结果与当前结论不能混写。

## 8. Fast20

同一进程内逐查询先跑冻结 ReleaseV1，再跑当前 solver：

| dataset version | enabled | ReleaseV1 | current solver | ratio | dual prep |
| --- | ---: | ---: | ---: | ---: | ---: |
| Toronto | 4/4 | `7.013s` | `4.526s` | `0.645x` | `0.036s` |
| Toronto-new | 4/4 | `11.421s` | `5.799s` | `0.508x` | `0.042s` |
| DBLP | 4/4 | `2.872s` | `0.496s` | `0.173x` | `0.040s` |
| DBLP-new | 4/4 | `2.115s` | `0.700s` | `0.331x` | `0.045s` |
| MovieLens | 1/4 | `11.964s` | `7.572s` | `0.633x` | `0.610s` |
| **total** | **17/20** | **`35.385s`** | **`19.093s`** | **`0.540x`** | **`0.773s`** |

总时间降低约 `46.0%`。DBLP 四条 fast 查询累计只生成 `46,991` 个 distance-row states；DBLP-new 为 `319,348`。MovieLens 的保守门控只在 g12 启用 dual，牺牲一部分全开收益以保护 g9/g10 的重图预处理。

g12 代表 peak RSS 为：Toronto `26.77MiB`、Toronto-new `38.74MiB`、DBLP `13.94MiB`、DBLP-new `11.77MiB`、MovieLens `~183.0MiB`。MovieLens 的公共图约 `147MiB`，dual residual 造成临时峰值；其持久 distance row payload 只有 `115,884` bytes。

## 9. Full DBLP g13

### 9.1 Pair-only 结构探针

root-dual、hybrid-only，逐 pair row 释放：

```text
n / m                    2,497,782 / 12,786,329
root                     24,492
initial best             15.0174017721
TSP/2 full-root bound    5.2513360579
dual full-root bound     10.9396208724
zero-residual upper      17.3608143368
group distances          16.174s
dual build               28.158s
hybrid pair search       154.022s
hybrid pair settled      147,906,508
hybrid pair pushes       233,467,341
dual > TSP vertices      87.796%
```

旧 distance-only pair 层在同一初始上界下有 `168,924,473` 个 live states；hybrid 在 pair partition 前减少约 `12.44%`。它没有复现 fast DBLP g12 的 97% 删除，不能把 snapshot 比例外推到 full。

### 9.2 900 秒完整 solver

运行由 bounded 脚本持有实际 PID 并在 900 秒回收；没有最终权重：

```text
pair-partition roots       165
best                       15.0174 -> 14.7395
enter k=3                  294.336s
live states                132,304,946
allocated row payload      1,522,359,800 bytes
sampled peak at 871s       >=5.57GiB (5703MiB)
```

对照上一版 distance-only + pair partition：

```text
enter k=3       404.684s -> 294.336s   (1.375x, -27.27%)
live states     167,476,094 -> 132,304,946 (-21.00%)
```

本轮 distance solver 在 900 秒仍未完成 k3。这个冻结结论只说明：dual-cut 能显著改变 pair 层，但逐层 row recurrence 的瓶颈转移到 triple rows；它不能作为“该 solver 已跑通 DBLP g13”的证据。

### 9.3 后续 global-label 完整结果

dual potential 随后接入 anchored global label-setting，而不是继续堆 triple-row 局部优化。配合生成式 root-star 完成上界和无损紧凑 frontier，full DBLP g13 q1 得到：

```text
best          12.5936282853
solver wall   1427.6247319s
peak RSS      9231.500MiB
settled       57,198,969 / 10,228,417,290 possible labels
```

该结果证明 dual 的价值不只在旧 pair 层减常数；与不物化整层 row 的 recurrence 结合后，ReleaseV2 实际只定型 `0.5592%` 的理论 labels。当前 ordered-half/global 组合见 `../release_v3.md`。

### 9.4 Objective-only 边界

`gst_dual_cut_probe --objective-only` 只运行 group distances、TSP 表和一次 root dual，不重放 pair rows。2026-07-12 的 Release/O2 结果为：

| instance | initial feasible best | root-dual objective | group distance | dual build |
| --- | ---: | ---: | ---: | ---: |
| Toronto g13 q1 | `0.8058327102` | `0.5252425477` | `0.105s` | `0.162s` |
| DBLP g13 q1 | `15.0174017721` | `10.9396208724` | `16.430s` | `34.480s` |

这里的 objective 是“连接指定 root 与剩余组”的 rooted dual 下界，不是 unrooted GST 的全局证书；指定 root 未必属于最优 GST，因此不能用 `best==objective` 直接终止 Test21。它的用途是衡量 state potential 和重放 pair 前的 dual 强度。若要全局证书，必须使用所有可行 anchor roots 的合法 super-root dual；第 10 节已经说明该版本的额外成本与弱收益。

## 10. 已停止的扩展

测试过把根改为最小必经组的全部候选点，形成 anchor-group super-root dual，再与 root dual 取 max。它在 fast g12 只额外删除少量 pair states，却几乎把 dual 预处理翻倍；MovieLens 多约 `0.64s`。该扩展不进入 solver、不运行 full，只保留 `gst_dual_cut_probe --anchor` 复现入口。

后续 Test35 又使用 Test21 的 farthest permanent anchor，直接以 group-rooted dual **替换**单根 dual，从而排除“只是第二次预处理太贵”的解释。即使另用单根 dual 提供完全相同的 primal `best`，DBLP-fast g12 的 ordinary values 仍由 `50,267` 增到 `1,173,010`；super-root potential 本身过松。详见 `../archive/test35_anchor_group_dual_20260712.md`。

## 11. Distance-Solver 阶段结论

对冻结的 distance solver，dual-cut 之后不应继续优先堆 metric 小下界。当时识别出的 triple-row 问题是：

1. 用 dual reduced costs 分析 triple active roots，而不是保存全部传播状态；
2. 推导按 source row 聚合消费者的 subset-lattice pebbling schedule；
3. 保持 `O(3^g n + 2^g((g+log n)n+m))` 口径，不把 pair Dijkstra 重放上千次；
4. 在 fast/40k snapshot 出现数量级状态下降前，不再启动该 distance solver 的 full g13。

ReleaseV3 已把 small 工作量 gate、ordered rows 和 global labels 收敛为单路径；剩余 frontier 空间与低 `g` 边界见 `../research_status.md`。

## 12. 复现

```powershell
cmake --build build --config Release --target gst_dual_cut_probe gst_distance_epoch_solver_probe
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe --self-check 710881 300 7 10
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe data_snapshot/generated_fast DBLP_data_bfs g12 1
.\build\tools\distance_epoch_solver_probe\Release\gst_distance_epoch_solver_probe.exe --self-check 710883 1000 2 10 10
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\distance_epoch_solver_probe\run_bounded.ps1 -Seconds 900
```

## 13. 文献关系

- [Dijkstra meets Steiner](https://arxiv.org/abs/1406.0492)：feasible lower bound、edge consistency 与 subset splice 的理论接口。
- [DS*](https://arxiv.org/abs/2011.04593)：允许一般 admissible/LP lower bounds 的 label-setting 扩展。
- [Wong, A Dual Ascent Approach for Steiner Tree Problems on a Directed Graph](https://doi.org/10.1007/BF02612335)：directed-cut dual ascent 的原始路线。
- [Improved Algorithms for the Steiner Problem in Networks](https://www.sciencedirect.com/science/article/pii/S0166218X0000319X)：dual ascent、reduced costs 与 exact Steiner solver。
- [SCIP-Jack](https://eprints.lancs.ac.uk/id/eprint/127117/1/gamrath17.pdf)：dual ascent 在 exact Steiner 与 Group Steiner 变体中的工程位置。
- [PrunedDP](https://ronghuali.github.io/paper/sigmod2016gst.pdf)：当前项目 baseline 与 group-distance/TSP-family 下界来源。
