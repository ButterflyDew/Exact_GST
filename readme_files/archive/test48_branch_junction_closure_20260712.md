# Test48：Anchor Branch-Junction Closure

更新时间：2026-07-12。Test48 沿 Test44--47 的 paid anchor backbone 继续扩展共享几何，但不再增加 block 扫描层数。它从所有 triple attachment 的精确最优根提取候选分叉点，把这些根到 anchor backbone 的确定性最短路并成一棵父树，再在压缩父树上做 subset DP，使不同根共享的路径前缀只付费一次。该机制在 fast20 上产生本轮最好结果，并显著压低 full 内存；但 full DBLP g13 仍未在 ReleaseV3 的 query 时间门槛内完成，因此正式代码、统计字段与工具最终撤回。

> 后续状态：该合法上界已作为公共连续数组 helper 进入纯 A 的 Test80；Test80 不设 V3 门禁，并在 `666.509s / 2161.9MiB` 完成 full。本文的“撤回/未完成”只描述 Test48 当时的独立 gate。

## 1. 为什么只扫描 Triple Roots

固定 root-star 根到 permanent anchor group 的一条最短路 `P`。对 nonanchor block `S`，旧 paid-backbone attachment 为：

```text
E_P(S) = min_x [d_P(x) + sum(i in S) gd_i(x)].
```

singleton 只给出一条接入路径，pair 仍可退化为路径；triple 是第一种能在根 `x` 处见到真实度 3 junction 的 block。因此 Test48 只为全部 `|S|=3` 扫描精确 argmin root，并加入 `P` 的顶点：

```text
C = V(P) union {argmin E_P(S) : |S|=3}.
```

这不是固定 `g`、数据集、密度或时间特判。全图扫描量从 singleton/pair/triple 的

```text
[C(k,1)+C(k,2)+C(k,3)] n
```

降为 `C(k,3)n`。full DBLP g13 从 `744,339,036` scans 降到 `549,512,040`；结构探针中的 scan wall 从 `10.35s` 降到约 `7.09--7.62s`，而最终 junction upper 不变。

## 2. 父树共享与压缩树 DP

从 `P` 做一次 multi-source Dijkstra，并固定每个顶点到 `P` 的 parent。所有候选根的 parent paths 的并形成一棵以 `P` 为 super-root 的森林；`P` 已付费，所以其多个根在 super-root 处以零代价连通。

只保留以下压缩树顶点：

- triple candidate roots；
- `P` 顶点；
- 至少有两个 active children 的分叉结点。

连续 degree-2 非候选路径压成一条带总长度的树边。在压缩树顶点 `x`，任意 group subset `S` 可由同根 star 服务，代价为 `sum(i in S) gd_i(x)`。对子树 `T_x` 定义：

```text
F_x(S) = 在 T_x 内服务恰好 S，并把所有启用设施连到 x 的最小代价。
```

合并 child `y` 时，若 `y` 的子树服务非空 mask，边 `(x,y)` 只支付一次；否则不支付：

```text
F'_x(S) = min(R subset S)
          F_x(S-R) + F_y(R) + [R != empty] length(x,y).
```

这正面表示多个候选根共享的 parent-path prefix，而不是事后把独立 block costs 相加。复杂度为：

```text
O(m log n + C(k,3)n + |C'| 3^k)
space O(n + |C'| 2^k),  k=g-1
```

其中 `C'` 是压缩树顶点集，full DBLP 只有 25 个。全部结构使用连续 arrays、bit masks 与离线顺序，没有 Hash。

每个 DP 方案都对应真实可行树：支付 `P`，支付所有服务非空子树的压缩树边，再从设施根沿最短路连接其负责 groups。因此该值是合法上界。探针还保存 convolution choices，恢复所有 `(junction, group mask)`，按 edge id 合并真实路径；五库和 full 均未进一步降低标量值。

## 3. 独立结构结果

五库 fast g12：

| dataset | triple-root closure | junction tree | junction path union |
| --- | ---: | ---: | ---: |
| Toronto | `1.012770` | `0.971477` | `0.971477` |
| Toronto-new | `4.055991` | `4.003287` | `4.003287` |
| DBLP | `12.226144` | `12.226144` | `12.226144` |
| DBLP-new | `11.003405` | `11.003405` | `11.003405` |
| MovieLens | `0.0202656` | `0.0202273` | `0.0202273` |

full DBLP g13 q1：

```text
root-star upper              17.427423110
paid path + triple closure   13.161911611
junction tree upper          13.019988893
junction path union          13.019988893
triple scans                 549,512,040
candidates / compressed      25 / 25
tree convolutions            13,286,025
tree DP                      about 0.05s
```

full 的改善来自候选根在同一 parent tree 上的祖先嵌套；即使没有新增 LCA junction，也能避免重复支付公共前缀。

## 4. 集成 Test21

临时集成版在 ordinary D2 前运行 junction upper，并通过随机 DPBF 对拍：

```text
seed        712931
iterations  300/300
n           4..14
g           2..13
```

fast20 快照 `20260712_180048`：

```text
total query time       9.758s
formal Test21          12.380s
previous best Test45   10.112s
junction improvements  13/20
```

与正式 `20260712_050625` 比较：

```text
D2 retained values     791,583 -> 396,968  (0.501x)
D2 pops              1,488,758 -> 668,492  (0.449x)
ordinary wall           5.963s -> 3.607s    (0.605x)
junction extra wall     about 0.273s total over 20 queries
```

这证明收益来自 incumbent 对 ordinary D2 的真实剪枝，而不是单纯计时波动。

## 5. Full DBLP 硬门槛

第一次 bounded 脚本误把 DBLP 图加载计入 V3 的 query 门槛，而且 `Start-Process` 因 `Path/PATH` 重复键没有启动 solver；该空跑不构成实验结果。修复后在单一 `PATH` 子环境启动，并从本次 `weights.txt` header 出现时开始计 query wall。

唯一有效 gate：

```text
query wall at stop       535.507s
V3 query time            531.556s
sampled peak             2087.0MiB
final weight line        absent
```

因此 Test48 没有证明相对 V3 不退化。虽然 peak 远低于正式 Test21 的约 `4.97GB`，也低于 Test44--47 多数轨迹，但内存改善不能替代时间门槛。集成 helper、统计字段、CLI 输出、探针和 bounded 脚本全部撤回。

## 6. 结论与下一边界

Test48 首次证明在 paid-anchor A 框架中，多个 attachment roots 的公共路径前缀可以由一棵小型 mask-DP skeleton 直接共享；这是比扩大 block arity 更强的结构结论。但它仍只提供 incumbent，ordinary D2 接口没有改变。即使 fast D2 减半、full peak 约降到 2GiB，full 时间仍未跨线。

下一主线不能再表述为“继续把上界降一点”。它必须让共享 skeleton 直接承担原来 D2 的连接信息，或构造一个不逐 root 物化 pair rows 的 exact implicit interface。继续加入更多 seed block sizes、更多 facility roots、更多 parent trees或更深 witness union，都没有新的数量级依据。

Test48 没有直接采用新的论文算法；本文只使用项目已有的 Dreyfus-Wagner subset recurrence、Dijkstra shortest paths 和 paid-anchor 构造背景。压缩父树上的 subset facility DP 是本轮候选设计，尚未完成系统文献检索，因此不宣称论文级原创性。
