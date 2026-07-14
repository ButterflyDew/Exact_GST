# Paid-Attachment Half Profiles

更新时间：2026-07-13。本文定义一个真正去掉单一 root、同时保留内部 paid attachment 的 half state，并给出三块 exact completion 定理。独立穷举器已在随机正权小图上验证完备性和 Pareto dominance；当前仍缺少不枚举 connected subgraphs 的大图生成算法，因此它是 Test75 的理论基底，不是已完成 solver。

## 1. 状态定义

固定 permanent anchor group `a`，其余 groups 为 `K`，令 `h=floor(g/2)`。paid half state 不保存当前 root，而保存一棵连通子图 `T`：

```text
c(T)       已支付边成本
S          declared nonanchor groups, |S|<=h-1
phi_T(B)   min_{x in V(T)} D(B,x), B subset K-S, |B|<=h
```

`T` 必须命中 anchor 和 `S`，但允许经过尚未 declared 的 group terminals。后者不能自动并入 `S`：它们应表现为 `phi_T(B)=0` 的免费未来命中。seed `713661` 的第一个诊断反例正是 anchor path 经过未来 terminal；强制使用 actual-covered mask 会把 exact `7` 错成 `13`。

## 2. 三块完成定理

对任意 state `(T,S,phi)`，把剩余 groups 分为两个不交 blocks `B,C`：

```text
B union C = K-S
|B|<=h, |C|<=h.
```

候选值为：

```text
c(T) + phi_T(B) + phi_T(C).
```

每个候选都对应真实可行图：在 `T` 内分别选择达到 `phi_T(B)`、`phi_T(C)` 的 attachment vertices，并取两棵 rooted D witnesses 的并。边重叠只会让真实 union 更便宜，因此候选值不会低于 GST optimum。

反过来，取一棵最优树的 token centroid 和 Test21 已证明的 `A+D+D` 分解。令 `T` 为 anchor side 连同 centroid；其 declared groups 至多 `h-1`，另外两侧 group blocks 至多 `h`。两侧都可在 centroid 接入，故对应 profile completion 不大于该最优树。上下界合并得到：

```text
OPT = min_{T,S,B,C} c(T)+phi_T(B)+phi_T(C).
```

该公式不是 upper heuristic，而是 exact half-state completion。

## 3. Dominance

对相同 declared mask `S`，若：

```text
c(T1) <= c(T2)
phi_T1(B) <= phi_T2(B) for every valid future block B,
```

则 `T1` 在任意二块 completion 中都不差于 `T2`，可安全删除 `T2`。这比“较低 cost 支配”严格得多，也正面保留了 Test31--32 丢失的 paid internal attachments。

profile 维数只由 query groups 决定，不按图顶点建第二 boundary；最坏 Pareto 前沿仍可由既有 diamond 反链做成指数级，但指数落在参数 `g`，而不是显式 `n^2`。

## 4. 穷举验证

工具：`tools/paid_half_state_probe`。它对 singleton-group 正权随机图：

1. 穷举所有 connected edge subgraphs；
2. 为每棵 subtree 枚举 actual-hit mask 的所有合法 declared 子集；
3. 用 dense Dreyfus--Wagner 计算全部 `D(B,x)`；
4. 按上述 cost/profile dominance 维护 Pareto fronts；
5. 比较 profile completion 与完整 connected-subgraph optimum。

Release/O2 结果：

| seed / range | exact | declared candidates | Pareto states | ratio | max front |
| --- | ---: | ---: | ---: | ---: | ---: |
| `713671`, `n=5..8,g=3..7,m<=11` | `1000/1000` | `3,492,740` | `62,369` | `1.786%` | `66` |
| `713681`, `n=5..9,g=3..8,m<=13` | `200/200` | `8,451,893` | `19,620` | `0.232%` | `60` |
| `713691`, fixed `n=9,g=8,m=13` | `50/50` | `7,517,563` | `42,963` | `0.572%` | `117` |

更密图产生大量具有相同 vertex coverage、但边成本更高的 connected subgraphs，因此 dominance 压缩反而更强。fixed g8 的最大单 mask front 增至 `117`，说明不能假定常数代表数，但当前没有随候选总数同步爆炸。

## 5. 尚未解决的生成问题

穷举器证明的是状态和 completion，不是生产复杂度。直接枚举所有 connected subgraphs 显然不可行；而用当前 rooted A rows 只重建一棵 minimum-cost witness，又会重犯 Test32 的单 witness 反例。

下一步必须回答：

1. 能否从 pair predecessor paths 开始，按 cost/profile Pareto 直接生成 paid D2 half states；
2. edge extension 时 `phi(B) <- min(phi(B),D(B,new_vertex))` 是否能用共享 block factors 批量更新；
3. 同根 union 的 componentwise-min profile 是否能避免显式 `pair x root x consumer-mask`；
4. fast 与 full DBLP 的 pair-path Pareto fronts 是否比 `125.6M` D2 roots 小一个数量级。

第 4 项现已由下节的单-anchor projection 获得数量级正证据；但它还不是完整 block profile。完整 Test75 solver 仍须等待内部多点 profile 的生成接口，DBLP g13 query solver 也尚未运行。

## 6. Pair Root 的单 Anchor Profile

对 pair paid tree `T`，令单一 future profile 为到 permanent anchor group 的最小 attachment：

```text
phi_T(a) = min_{x in T} gd_a(x).
```

若最小值在 `x` 取得，则 rooted optimum `D({i,j},x)` 的成本不高于 `c(T)`，且其 witness 包含 `x`，所以点：

```text
(D({i,j},x), gd_a(x))
```

支配 `T`。因此单-anchor profile 的完整 Pareto family 可直接从 D2 roots 的二维 skyline 提取，不需要保存 predecessor witness。这是 exact projection，不是抽样。

工具 `tools/paid_pair_profile_probe` 在 exact incumbent 与现有 TSP lower 口径下得到：

| fast g12 | D2 settled roots | anchor skyline | ratio | max per pair |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `130,461` | `754` | `0.578%` | `31` |
| Toronto-new | `184,459` | `3,084` | `1.672%` | `173` |
| DBLP | `126,399` | `514` | `0.407%` | `14` |
| DBLP-new | `21,785` | `271` | `1.244%` | `8` |
| MovieLens | `19,206` | `234` | `1.218%` | `6` |

五库均超过一个数量级压缩，因此触发一次 full DBLP g13 **结构探针**，不运行完整 Test21：

```text
n / m                 2,497,782 / 12,786,329
nonanchor pairs       66
D2 settled roots      106,310,433
anchor skyline        821
skyline ratio         0.00077227%
max per pair          23
pair search           121.042s
including preprocess  139.710s
```

这说明 permanent anchor 确实能把 pair root 维投影为极小 paid-profile family；相对 Test49 的同一 `106.31M` roots，压缩不是容器常数。

## 7. 多坐标 Root Proxy 的边界

为检查单坐标正信号是否会被多维 profile 立即抹平，probe 还计算：

```text
(D({i,j},v), gd_k(v) for every remaining singleton k)
```

的 root-vector skyline。它要求所有 future distances 在同一个 root `v` 读取，比真实 paid tree 的 `min_{x in T}` 更受限，不是最终状态；但可作为多坐标压力口径：

| fast g12 | root-vector skyline | ratio | max per pair |
| --- | ---: | ---: | ---: |
| Toronto | `81,045` | `62.122%` | `1,609` |
| Toronto-new | `91,341` | `49.518%` | `1,793` |
| DBLP | `8,778` | `6.945%` | `183` |
| DBLP-new | `3,495` | `16.043%` | `118` |
| MovieLens | `822` | `4.280%` | `19` |

DBLP/MovieLens 仍有明显压缩，但 Toronto 两版回升到约一半 roots，故“给每个 root 加完整 singleton 向量”不能作为跨库状态。下一生成器必须让不同 future blocks 在 paid tree 的不同内部点取得 profile minima，并以 componentwise minimum 合并；不能退回单 root 向量。

当前更具体的 Test75 门槛变为：从每对至多几十个 anchor skyline seeds 出发，用 paid-path extension/union 或列生成补入真正被两块 completion 请求的 block coordinates，而不是预先展开全部 `2^(g-1)` profile 维。

## 8. 确定性内部骨架与 singleton completion

为区分“同根向量失败”与“内部 attachment 本身失败”，探针为每个 D2 root
恢复一棵确定性已付费骨架：两条 group-to-split 最短路、D2 predecessor 路径，
再加 root-to-anchor 最短路。对每个剩余 singleton，profile 取该组到骨架所有内部点
的最小距离。

五库 fast g12 的内部 witness skyline 为：

| dataset | witness skyline / D2 roots | ratio | 来自单-anchor skyline | 每对最大值 |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `32,260 / 130,461` | `24.728%` | `154` | `894` |
| Toronto-new | `26,473 / 184,459` | `14.352%` | `240` | `1,029` |
| DBLP | `5,547 / 126,399` | `4.388%` | `189` | `165` |
| DBLP-new | `2,825 / 21,785` | `12.968%` | `108` | `94` |
| MovieLens | `1,018 / 19,206` | `5.300%` | `179` | `27` |

内部路径明显优于第 7 节的同根向量，但 Toronto 仍远未达到数量级压缩；而且绝大多数
witness 并不属于单-anchor skyline。因此 `(D2(v),gd_anchor(v))` 只能完整回答一个
anchor attachment，不能直接代表多列 future profile。

在同一确定性骨架上逐个连接剩余 singleton，可得到始终合法的可行上界。fast g12：

| dataset | paid-singleton upper | exact | gap |
| --- | ---: | ---: | ---: |
| Toronto | `0.9705829000` | `0.9616227800` | `0.93177%` |
| Toronto-new | `3.8735458400` | `3.8735458400` | `0` |
| DBLP | `12.1663030000` | `12.1663030000` | `0` |
| DBLP-new | `10.3253438000` | `10.3147548000` | `0.10266%` |
| MovieLens | `0.0202240861` | `0.0202189774` | `0.025267%` |

这组 fast 结果足以触发一次 full DBLP g13 的结构验证。Release/O2 下只运行
`--upper-only`，不运行完整 query solver：

```text
paid_singleton_upper = 13.3957483032
exact                = 12.5936282853
gap                  = 6.3692527657%
settled D2 roots     = 106,310,433
pair_ms              = 230,871.077
total_ms             = 262,876.757
peak working set     = about 4.81 GiB
```

因此 singleton completion 不是大图终点：它对每个剩余组独立付费，重复计算了这些组
之间可共享的后缀。下一步必须计算 block profile `phi_T(B)`，并使用已证明 exact 的
`cost(T)+phi_T(B)+phi_T(C)`；不能继续微调 singleton 附着，也不能把上述结构时间写成
Test21 的端到端求解时间。

## 9. Pair-only 两块完成

进一步实验发现，anchor 不应预先并入 paid pair tree。保留 pair-only `T`，让 anchor
属于两个 completion blocks 之一，五库 fast g12 全部命中 exact；先付 anchor 的版本
则在 Toronto/Toronto-new/DBLP 分别留下 `2.330%/3.341%/0.08366%` gap。

更重要的是，只从单-anchor 二维 skyline 选 pair trees 时，五库 fast 仍全部 exact；
但小图穷举在 seed `713731` iteration `365` 得到 `exact=59 / skyline=60` 的反例。
扩大到 `3000` 例时 unrestricted rooted-pair family 为 `3000/3000`，anchor skyline 为
`2998/3000`。所以 skyline 只能是初始列，完备实现必须提供 partition-specific pricing。
完整数据与反例见 `archive/test75_pair_block_completion_20260713.md`。

full DBLP incidence-only 回溯进一步得到：`821` 棵初始树只有 `3,302` 个
tree-vertex incidences、`836` 个全局不同顶点，单树最多 `7` 点；总时 `166.382s`、
峰值约 `1.32GiB`。因此下一实现应计算 block rows 到这批 target vertices 的值，再按
短有序 incidence lists 做离线 min merge，而不是给每棵树复制 profile 向量。

后续 low-core 实验又把 high block rows 替换为 `q=ceil(h/2)` 阶以下的 rooted rows：
paid pair 依次吸收 balanced low blocks，达到 `g-2q` 个 declared groups 后以两个
`q` blocks 完成。真实 edge-union 穷举为 `3000/3000`，fixed g8 为 `100/100`；
DBLP-fast 仅用 D1--D3 即 exact，保留 `15,779` 个 core states、最大 front `45`。
但 Toronto/Toronto-new 分别仍差 `0.1982%/0.00689%`，front 达 `578/1,208`，所以
当前只能保留为 partition-pricing 基底。详细边界见
`archive/test75_pair_block_completion_20260713.md` 第 8 节。

最新的 plan lower 把 declared family 定价变成了可停止过程。fast g12 的 strong-certificate calls
在五库为 `1313/2253/929/370/4`，但前四库分散到 `43--55` 个 pair；因此 production
接口必须对同一 pair 的 requested columns 做批量 predecessor-order 传播。目标 g13 的
Toronto q1 中，`3+5+5` skyline upper 为 `0.7401396340`，只对该计划扫描全部 roots 即
得到 exact `0.7048467020`，额外定价 `1.062s`。这说明 anchor 的贡献是给出极小初始列
和高质量 plan，而不是保证 skyline 完备。

临时接入 Test21 后，该 upper 在 A4 末尾同样命中 exact，但 paid phase `6.097s`，后续
A states 只减少约 `0.5%`，Toronto g13 总时间 `20.788s -> 29.113s`。原因是 solver
仍需 D6/A5 证明没有更优解；component lower 又有 `28,150/36,036` plans 低于 exact。
因此代码已撤回。下一版必须先让 strong plan lower 与批量 root pricing 构成完整证书，
再从状态定义上删除 D6/A5；不能把 exact upper 命中误写成维度已经消除。

fixed g13 随后给出两个决定性反例：unrestricted `pair+6+5` 为 `160 -> 161`，
`q5/core3+5+5` 为 `106 -> 107`；后者允许 singleton 在所有 pair-tree points 接入仍失败。
因此此前 `3000/3000` 只适用于当时的 `g<=7` 范围，不能写成一般完备性。缺失来自
rooted-optimal pair witness 删除 paid geometry Pareto，而不是 skyline 或 attachment point。

多保留一个 declared group 的 `q5/core4+4+5` 修复这批反例：sequential singleton 与
single 2-block 两种生成在 g13 累计 `720/720`。新增范围包含额外 Steiner 顶点、每组
两个均衡候选顶点和更高环数；odd g7/g9/g11/g13 的 targeted 总计为 `13,520/13,520`，
其中 min/all attachment 对照 `1,410/1,410`。这些仍是证据，不是定理。Toronto g13 只
保留 `23,864` 个 core
states、max front `70`，strong family 也命中 exact；但 `5,476` plans 的 batch pricing
显式恢复 witness 仍需 `39.158s`。所以 core4 是新的理论候选，不是可集成 Test；不再扩
core3 的 attachment endpoints。

origin/added 两棵 pair forests 的 requested-profile 离线合并现已实现。若 `P` 在 `r`
生长，`A` 在 `x in argmin phi_P(A,r)` 接入，则

```text
cost(core)  = D(P,r) + phi_P(A,r)
phi_core(B) = min(phi_P(B,r), phi_A(B,x)).
```

每个 profile 只沿 singleton parent forests 和 pair predecessor forest 查询所需目标；
所有并列 `x` 用持久前驱链保留，再以 dense stamp 去重，全程无 Hash。五库 fast 对
`4,387,627` 个 attachment sets 与 `8,250` 个 strong plan prices 的显式核验均为零误差。
Toronto g13 同一 `5,476` plans 的定价由 `39.158s` 降为 `18.721s`，dense-oracle 总时间
约 `145.7s -> 111.0s`，best 仍为 `0.7048467020`、首次 exact rank 仍为 `1,552`。

已证明边界仅是“forest 因子化与显式 core4 定价等价”；core4 separator/decomposition
仍没有一般证明，且 strong-plan 诊断使用 supplied optimum 过滤候选。因此正式 Test21
不修改、D6/A5 不删除，也没有运行 full DBLP。

## 12. Rooted D4 与三标量 Witness Family 边界

core4 的更简洁候选是直接恢复已有 rooted-optimal D4 witness，再做 `4+5` paid-profile
completion。Toronto g13 同一 `5,476` strong plans 在 `8.023s` 内得到 exact，快于双
pair-forest 的 `18.721s`；fast Toronto-new/DBLP/DBLP-new 也 exact。但 fast MovieLens
g12 只到 `0.0202203614`，高于 exact `0.0202189774`。

为排除“只选一棵等价 witness”的实现缺陷，probe 对所有 tight D1--D4 derivations 传播：

```text
pB  = min_T phi_T(B)
pC  = min_T phi_T(C)
pBC = min_T (phi_T(B)+phi_T(C))
```

若 witness family 由左右 family union，则 `pBC` 是 `left.pBC`、`right.pBC`、
`left.pB+right.pC`、`right.pB+left.pC` 四者最小值；沿 tight edge 再与当前顶点的
两个 block values 做同样四项闭包。因此三个标量对“全部最优 D4 witnesses”是 exact
summary，不需要 profile Pareto 容器。MovieLens 唯一 unresolved strong plan 经过该
oracle 仍为 `0.0202203614`。再把四个 core groups 与两个 completion blocks 当作六个
macro terminals 做 64-state DP，也只到 `0.0202203613`。

所以缺口不是 predecessor tie，也不是 minimum D4 的合并顺序，而是成本更高、对未来
attachment 更好的 paid-core geometry。后续状态必须保存 cost/profile Pareto 或给出新的
exchange theorem；不能把 D4 最优 witness 重新包装成已消维算法。以上三个模式均只在
research probe 中，正式 Test21 未修改。

## 13. 文献与原创性边界

该 paid-profile state 是本仓库从 Test31--32 attachment 反例和 Test21 三块 completion 推导出的候选，尚未完成系统新颖性检索，不宣称论文原创性。它与 rank-based/treewidth representative sets 的区别仍见 `paid_attachment_representative_family.md`：这里的 profile boundary 由 query blocks 定义，当前没有 bounded-treewidth bag。

本轮窄检索到的 [Iwata--Shigemura separator pruning](https://ojs.aaai.org/index.php/AAAI/article/view/3965) 和 [Fuchs et al. separator-terminal DP](https://doi.org/10.1007/s00224-007-1324-4) 都不直接给出上述 profile 生成器；若后续形成论文主线，仍需单独完成系统文献检索。

[Ramirez Alfonsin--Tishchenko 的 quasi-binary tree edge separators](https://arxiv.org/abs/1209.3572)
研究删除若干树边后的平衡 component weight；它不保证 separator 由 rooted-optimal pair/D4
witness 构成，也不处理 paid attachment profile。因此只能作为邻近的树分隔背景，不能
作为当前 quartet exchange 的证明或原创性归属。
