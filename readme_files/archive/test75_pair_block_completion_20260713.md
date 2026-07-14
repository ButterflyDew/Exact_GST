# Test75：Paid Pair 与两块完成

更新时间：2026-07-13。本记录研究 `paid pair tree + half block + half block`，目标是让 permanent anchor 负责选择极小的初始 paid-tree 列，而不把 anchor path 预先焊入每棵树。所有结果均为 Release/O2。

## 1. 两种状态顺序

对连接 groups `i,j` 的 paid tree `T`，定义：

```text
phi_T(B) = min_{x in T} D(B,x).
```

比较两种 completion：

```text
pair-only:    c(T) + phi_T(B) + phi_T(C),  B union C = K - {i,j}
anchor-first: c(T+a) + phi_(T+a)(B) + phi_(T+a)(C)
```

两者都是合法上界，但第二种先固定 anchor path，会限制后续两个块的共享拓扑。anchor 更适合作为 pair 列的选择/定价方向，而不是每棵 paid tree 的强制组成部分。

## 2. Fast g12 结果

独立工具 `tools/paid_block_completion_probe` 先构造 dense exact half rows，再恢复确定性 D2 witness 的内部路径。该工具只用于 fast 结构验证，不进入正式 Test21。

| dataset | pair-only | anchor skyline only | anchor-first | exact |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `0.9616227800` | `0.9616227800` | `0.9840280200` | `0.9616227800` |
| Toronto-new | `3.8735458400` | `3.8735458400` | `4.0029630800` | `3.8735458400` |
| DBLP | `12.1663030000` | `12.1663030000` | `12.1764810000` | `12.1663030000` |
| DBLP-new | `10.3147548000` | `10.3147548000` | `10.3147548000` | `10.3147548000` |
| MovieLens | `0.0202189773` | `0.0202189773` | `0.0202189774` | `0.0202189774` |

五库 pair-only 均命中 exact；而 anchor-first 在 Toronto 两版分别差 `2.330%` 与 `3.341%`，DBLP 差 `0.08366%`。因此后续状态顺序固定为“先 paid pair，anchor 留在 completion 一侧”。

DBLP-fast 的参考探针构造 `60,060` 个按 partition 请求的 profile columns、`138,027,708` 个 root-profile values；dense half oracle 自身有 `156,502,500` 次 seed probes。它证明候选语义，不是可接受的生产实现。

## 3. 小图穷举

`paid_half_state_probe` 额外枚举所有 connected edge subgraphs，只允许满足某个 root 上 `cost(T)=D({i,j},root)` 的 paid pair witness，再与两个 half blocks 完成：

| seed / range | unrestricted pair-block | anchor-skyline pair-block |
| --- | ---: | ---: |
| `713701`, `1000`, `n=5..8,g=3..7,m<=11` | `1000/1000` | 未单列 |
| `713711`, `300`, `n=5..9,g=3..8,m<=13` | `300/300` | 未单列 |
| `713721`, fixed `n=9,g=8,m=13`, `100` | `100/100` | 未单列 |
| `713731`, `3000`, `n=5..8,g=3..7,m<=11` | `3000/3000` | `2998/3000` |

unrestricted rooted-pair family 尚未发现反例。anchor skyline 不是完备族；第一条反例为：

```text
seed=713731 iteration=365 n=7 g=7
exact=59, anchor-skyline completion=60, anchor group=2 (1-based)
terminals: 1 6 3 4 2 5 0
edges: (1,0,15) (2,1,5) (3,0,19) (4,3,16) (5,4,3)
       (6,0,6) (2,4,16) (3,2,19) (2,0,16) (5,6,14) (6,1,16)
```

因此不能把 full DBLP 的 `821` 个 anchor skyline states 直接宣称为 exact 替代。它们是初始列；当 upper/lower 未闭合时，必须有不依赖数据集、`g` 或时间阈值的 partition-specific pricing 补列。

## 4. 下一生产接口

不应物化 dense `D(B,v)`。对每个 half block `B`，生产接口只回答候选 paid trees 的集合目标：

```text
distance(B, T) = min_{x in T} D(B,x).
```

为测量目标集合本身，full DBLP g13 又运行一次 incidence-only 结构探针；它不构造
`g^2 n` profiles，也不运行完整 query：

```text
anchor skyline trees   821
tree-vertex incidences 3,302
unique target vertices 836
max vertices per tree  7
settled D2 roots        106,310,433
pair search             145.840s
including preprocess    166.382s
peak working set        about 1.32 GiB
```

相对图的 `2,497,782` 个顶点，初始列最终只请求 `836` 个 target vertices；每棵树取
profile minimum 又只需扫描至多 `7` 个有序 incidence。这个规模支持 sparse target
oracle，但不改变 skyline 缺少 exact pricing 的事实。

这允许从 Test21 的离线有序列表或目标导向 DP 直接结算少量 tree targets，避免 Hash，也不做 baseline 可共享的图/query 压缩。当前尚需完成：

1. 证明 unrestricted rooted-pair completion 的完备性，或找到反例后扩大 declared paid mask；
2. 实现 sparse multi-tree target oracle，不创建 `mask x all vertices` dense rows；
3. 用 dual/lower bound 驱动 exact pricing，补足 anchor skyline 的极少失配；
4. 通过 fast20 与 Toronto full 后，才运行一次 DBLP g13 完整门。

## 5. 旁挂旧 A rows 的否决

曾将 skyline pair trees 直接接到正式 Test21：ordinary rows 完成后在 tree vertices 读取
`D(B,x)`，每生成一条 anchored A row 就读取 `A(S,x)` 并结算互补块。实现使用 unique
target lookup 加短 incidence lists，不用 Hash，也没有任何经验门控。随机 DPBF 对拍
`500/500`（seed `713741`）通过。

但它没有删除旧状态：

| fast g12 | formal wall | attached wall | A values | A merges | completion checks | updates | profile time |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Toronto | `1.791s` | `1.909s` | `199,016`（完全相同） | `8,105,710`（完全相同） | `11,237,827`（完全相同） | `3` | `75ms` |
| Toronto-new | `3.609s` | `4.039s` | `558,880`（完全相同） | `21,242,755`（完全相同） | `35,449,843`（完全相同） | `0` | `289ms` |

因此附加 upper 即使偶尔更新，也没有改变 A payload，净时间严格退化。集成代码与统计
字段已撤回；正式 Test21 再通过随机 `100/100`（seed `713751`），Toronto-fast g12
复跑 `1.816s / 18.2MiB`。下一版必须让 tree targets 直接成为 A 的输出接口，从生产上
删除 `A(mask,all roots)`，不能在旧 A 完成后再扫一遍。

## 6. 结论边界

这是当前正向主线，但还不是 Test21 solver。五库 exact 和 `3000/3000` 穷举支持 pair-block 状态；`2998/3000` 同时严格否定了“anchor skyline 本身完备”。本轮没有运行 full DBLP query solver。

## 7. Paid Seeds Global A 的否决

还测试了真正删除 singleton-only labels 的 paid-A global recurrence：只在 skyline pair tree
的每个内部点播种 `(pair mask,cost)`，随后允许图传播、singleton attachment 和两个
不交 paid labels 合并。该状态不重新播种普通 D2，fast DBLP g12 也得到 exact；但：

```text
tree-incidence seeds  2,047
created labels        1,483,423
settled labels        1,193,065
heap pushes           2,009,855
global phase          18.039s
formal Test21 query   about 0.33s
```

因此 root 维虽在种子处被压掉，却在逐边传播时重新乘上 consumer masks；这是状态规模
失败，不是 heap 或查找常数。实验模式已从 pair-profile 工具撤回。后续 block oracle
必须直接输出 tree profile scalars，不能把 paid trees 重新展开为 `(mask,root)` labels。

## 8. 低阶 Paid Core Growth

为彻底删除 high rooted rows，令：

```text
h = floor(g/2)
q = ceil(h/2)
core_size = g - 2q
```

新生成规则只构造 exact `D(L,v), |L|<=q`。从 rooted paid pair 开始，在树内实现
`phi_T(L)` 的点接入低阶 block，按 edge union 只支付尚未购买的边；paid core 达到
`core_size` 后，再用两个不超过 `q` 的 blocks 完成。增长使用 balanced canonical
顺序：g12 为 `pair+2+2`，g13 为 `pair+3+2`。

connected-subgraph 穷举结果：

| seed / range | low-core existence | rooted-pair generated | anchor skyline generated |
| --- | ---: | ---: | ---: |
| `713881`, `3000`, `n=5..8,g=5..7,m<=11` | `3000/3000` | `3000/3000` | `2988/3000` |
| `713891`, fixed `n=9,g=8,m=13`, `100` | `100/100` | `100/100` | `100/100` |

这里的 generated 不是事后挑 core：它枚举 rooted-optimal block witnesses，逐次做真实
edge union，并且只允许在当前 profile minimum 点附着。unrestricted rooted-pair 仍未见
反例；anchor skyline 继续只适合作为初始列。

fast dense-oracle 原型 `tools/paid_core_growth_probe` 不生成 D4--D6。紧凑 low-profile
表示把每 state 从 `2^g` doubles 降为仅 `|L|<=q` 的列。代表结果：

| fast g12 | completion | exact gap | pair seeds | generated | core states | max front | total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| DBLP | `12.1663030000` | `0` | `518` | `242,022` | `15,779` | `45` | `1.770s` |
| Toronto | `0.9635285700` | `0.1982%` | `731` | `975,866` | `221,848` | `578` | `11.958s` |
| Toronto-new | `3.8738126700` | `0.00689%` | `3,048` | `2,023,298` | `293,598` | `1,208` | `35.930s` |

DBLP 给出明确正信号，但 Toronto 两版未通过跨库 exact/规模门。把所有 D2 roots 预先
按完整 low profiles 定价，在 Toronto 到 `6.73GiB / 127 CPU-s` 仍未完成，已停止；
因此 pricing 必须按 partition 单列生成。只对当前获胜 core/partition 扫描 `210,000`
个 roots 又完全不改善 Toronto，说明缺口来自另一个 core/partition，而非同计划漏 root。

另一个捷径也已否决：强制 paid pair 包含 permanent anchor，随后只使用 ordinary D，
随机仅 `865/1000` exact（seed `713901`）。生产版仍需要 nonanchor paid pair 与少量
anchored low blocks，不能把 anchor side 全删。

Toronto 的 exact pair+half winner 为 pair groups `5,8`（1-based）、root `2529`，
两个 5-group masks 为 `1102/2849`。在全部 `18,480` 个 pair/half partitions 中：

- 用 root-star 上 exact high rows 排序，winner rank 为 `162`；
- 只用生产可用的 low-row top split seeds 排序，winner rank 退到 `804`；
- 对当前 low-core incumbent 的 core/partition 扫描 `210,000` 个 pair-root plans，值仍
  严格等于 `0.9635285700`，没有接近 exact。

所以缺口确实来自另一 partition，不是当前计划少一个 root。low-row score 可以排序，
但不能作为 exact 停止证书；full 上也不能无界依次定价数百个 high partitions。

把每个 partition 的真实 priced value 按 low-row score 排序后，首个 exact plan 的位置为：

```text
Toronto      804 / 18,480
Toronto-new 3293 / 18,480
```

因此“按 low score 逐列精确定价直到命中”在 full 上没有可接受的调用上界。

还测量了 q 的结构权衡（Toronto-fast g12）：

| q | core form | result | generated/core | time | conclusion |
| ---: | --- | ---: | ---: | ---: | --- |
| `2` | `pair+2+2+2`, then `2+2` | `0.9619292900` | `6.84M / 218,016` | `61.35s` | 更近但爆炸 |
| `3` | `pair+2+2`, then `3+3` | `0.9635285700` | `0.976M / 221,848` | `11.96s` | 当前理论基底 |
| `4` | `pair+2`, then `4+4` | `0.9709136300` | `42,968 / 20,112` | `1.835s` | 小但质量差 |
| `5` | `pair`, then `5+5` | exact | `0 / 731` | `2.015s` | 退化到 pair-half |

q5 在 DBLP/DBLP-new 也 exact，但 Toronto-new 的 anchor-skyline q5 为
`3.8958238100`，差 `0.5751%`。允许所有 roots 后恢复 exact，却保留 `77,606`
个 pair states、max front `1,730`，耗时 `27.681s`。所以提高 q 只把缺失的 pricing
从 core 转回 paid-pair front，没有解决维度消除。

本节没有触发 full DBLP。当前保留的是 unrestricted low-core 生成定理候选与 DBLP
正向规模；skyline pricing 和 Toronto frontier 尚未解决，不能接入正式 Test21。

## 9. 强下界定价与 g13 低维 Core 门禁

对 g12 的 `pair+5+5` plans，新增合法 lower：

```text
max(OPT(P)+OPT(B)+OPT(C),
    OPT(P union B)+OPT(C),
    OPT(P union C)+OPT(B),
    0.5*(OPT(P union B)+OPT(P union C)+OPT(B)+OPT(C)))
```

其中 size-7 union 的 root-free optimum 只由现有 size-5 rows 做一次 top split，不构造
high closure。按该 lower 排序并精确定价，直到下一 lower 不小于 incumbent，fast g12 q1
得到：

| dataset | certificate calls | distinct pairs | pair-specific columns |
| --- | ---: | ---: | ---: |
| Toronto | `1,313` | `54` | `2,626` |
| Toronto-new | `2,253` | `53` | `4,506` |
| DBLP | `929` | `43` | `1,858` |
| DBLP-new | `370` | `55` | `740` |
| MovieLens | `4` | `4` | `8` |

这证明 lower 可以给 declared pair family 的停止条件，但该 family 的全局完备性仍须
另证；调用还分散到大多数 pair，不允许“每 plan 重扫整棵 pair forest”。Toronto
进一步按 `D(P,r)+OPT(B)+OPT(C)` 截断各 plan 的 root 前缀后，
profile/completion 访问从 `6,355,464/3,177,732` 降为
`1,107,852/553,926`；仍需按 pair 批量传播列，而不是独立 point pricing。

目标 g13 使用 `q=5`，状态为 `3-group paid core + 5 + 5`。Toronto g13 q1 的已存 exact
为 `0.7048467020`：

```text
anchor-skyline core completion   0.7401396340
same-root three-block            0.7694927244
same-plan all-root pricing       0.7048467020
all-root candidates              138,219
pricing after low rows           1.062s
component plans below exact      28,150 / 36,036
```

因此 paid core 的 root 定价确实能把 A4/D5 信息转成 exact upper；缺口不是计划本身，而是
anchor skyline 漏 root。但 component lower 仍留下绝大多数 plans，尚不能据此删除 D6/A5。
一次临时 Test77 集成在 A4 后得到 `0.7453572351 -> 0.7401396340 -> 0.7048467020`，
paid phase 为 `6.097s`，只把后续 anchored values 从 `555,975` 降到 `553,165`；总时间
由正式 Test21 的 `20.788s` 退到 `29.113s`。集成代码、目标和统计字段已撤回，正式
Test21 重新随机对拍 `100/100`（seed `713931`）。没有运行 full DBLP g13。

当前结论不是继续优化这 `6.097s` 的常数，而是先证明/实现完整的低维定价证书：只有当
strong lower 能在 A4/D5 后闭合，才允许真正不生成 D6/A5。same-root 三块仍只是互补
upper；Toronto g12 已有 `0.9688685500 > 0.9616227800`，不能单独启用为 exact 路径。

## 10. g13 反例与四组 Core

扩大到 fixed `n=g=13,m=13` connected-subgraph 穷举后，先前的小图正信号不能继续
外推。seed `713951` 的第 9 个实例给出 unrestricted rooted-pair `pair+6+5` 反例：

```text
exact            160
pair completion  161
terminals        4 11 5 2 1 7 9 10 12 0 8 3 6
edges            (1,0,11) (2,0,19) (3,2,16) (4,0,18)
                 (5,3,8) (6,5,17) (7,6,17) (8,7,11)
                 (9,8,13) (10,7,18) (11,3,6) (12,10,17)
                 (11,10,7)
```

第 10 个实例进一步否定 `q=5, core3+5+5`：min-profile attachment 与允许 singleton
在 pair tree 任意内部点接入都只能得到 `107`，exact 为 `106`。反例为：

```text
terminals  2 7 10 8 4 11 6 9 12 1 5 3 0
edges      (1,0,17) (2,1,13) (3,0,6) (4,2,1)
           (5,4,9) (6,2,20) (7,1,10) (8,0,1)
           (9,4,2) (10,1,12) (11,6,9) (12,6,7)
           (12,11,8)
```

这说明缺失不是 attachment endpoint，而是“每个 root 只保留 rooted-optimal pair tree”
删除了成本/几何 Pareto。q5/core3 即使 strong pricing 在 Toronto 命中 exact，也不能
作为删除 D6/A5 的完备证书。

相邻的 `q=5, core4+4+5` 修复了上述反例。两种生成顺序分别为
`pair+singleton+singleton` 与 `pair+2-block`；g13 合并多组 seeds 为 `720/720`
exact。目前仍只是穷举证据，不称为定理。Toronto g13 dense oracle 的 block 版本为：

```text
generated / core states       48,083 / 23,864
max front                     70
skyline completion            0.7401396340
strong plans below exact      5,476 / 90,090
pair/core root bounds         1.495M / 47.213M
after attachment lower        6.631M core candidates
completion probes             22.158M
strong-family best            0.7048467020
first exact strong rank       1,552
batch pricing                 39.158s
```

因此 core4 恢复了当前 family 的完备性信号和 Toronto exact，但上述 `39.158s` 的显式
witness 恢复仍未过性能门。显式 factorized `(pair,block)` columns 在 fast Toronto g12
已有 `10,003` 列、每 pair 最多 `214`，不能直接物化成 full dense profiles。更大的
`q4/core5+4+4` 虽在同一 10/10
穷举通过，Toronto 却有 `40,569` 个 component plans 低于 exact，当前获胜 plan 全 root
定价也只到 `0.7388999387`，路线更弱。

### 10.1 双 pair-forest 的 requested-profile 因子化

令 origin paid pair 为 `P`、root 为 `r`，added pair 为 `A`，并令 `x` 是
`phi_P(A,r)` 的任一最小点。core 的付费成本与任意 completion block `B` 的 profile 为：

```text
base(P,A,r,x)       = D(P,r) + phi_P(A,r)
phi_(P union A_x)(B)= min(phi_P(B,r), phi_A(B,x))
```

右侧四个量只需在 requested vertices 上查询。实现分别沿两棵 singleton parent forests
与 paid-pair predecessor forest 反向传播，并用持久前驱链保存所有并列 `x`；两条
singleton paths 相交时以 dense stamp 去重。整个接口是数组与有序列表操作，不使用 Hash，
也不物化 `columns x n` 的 dense profiles。

五库 fast g12 对显式 witness 做了两层等价核验：`4,387,627` 个 attachment-root sets
全部一致，`8,250` 个 strong plan prices 也全部一致。各库的 records 与显式 core
candidates 逐项相等。Toronto g13 Release/O2 结果为：

```text
factorized records             6,631,397
tied targets / tied records    1,079,699 / 3,451,844
completion probes              22,157,960
attachment / completion        6.043s / 12.678s
factorized pricing             18.721s
explicit witness pricing       39.158s
best / first exact rank        0.7048467020 / 1,552
dense-oracle total             111.035s  (旧显式约 145.7s)
```

这证明的是“因子化实现与当前 core4 family 的显式定价等价”，没有证明 core4 family
必然包含最优树。定价器还使用 supplied optimum 筛选 strong plans，因此不能作为端到端
solver 或 stopping certificate。本轮仍未运行 full DBLP；在 core4 decomposition 定理
完成前只保留为 upper/certificate probe，不能删除正式 Test21 的 D6/A5。

### 10.2 D4 最优 Witness 为什么仍不够

修正 `high-core4` 的退出条件后，targeted odd-g 搜索又加入 extra Steiner vertices、
均衡双终端 groups 与高环数；g7/g9/g11/g13 共 `13,520/13,520`，其中 min/all
attachment 对照 `1,410/1,410`。这加强了 quartet 候选的结构信号，但仍不是证明。

更简洁的 rooted-D4 paid core 在 Toronto g13 对同一 `5,476` plans 只需 `8.023s`，best
和 first exact rank 仍为 `0.7048467020 / 1,552`。但 fast MovieLens g12 给出边界：

```text
deterministic rooted-D4        0.0202203614
all tight D4 witnesses        0.0202203614
six-label macro DP            0.0202203613
exact                         0.0202189774
```

“all tight D4 witnesses”不是抽样：对 witness family 精确维护 `min phi_B`、`min phi_C`
与 `min(phi_B+phi_C)` 三个标量，union/edge extension 都由四项 min 闭合。它仍有 gap，
说明缺的是非最优 paid-core 的 cost/attachment Pareto geometry。六标签 DP 也未修复，
说明仅把 minimum rows 换一种括号顺序没有消维。三条代码保留作 semantic probes，均不
进入正式 Test21，也不触发 full DBLP。
