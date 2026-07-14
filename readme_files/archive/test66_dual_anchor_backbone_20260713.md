# Test66：Dual-Anchor Paid Backbone

更新时间：2026-07-13。Test66 在 Test65 的递归 anchor 边界上构造一棵同时连接两个最远组的 paid backbone，再把其它 groups 作为至多四个 singleton/pair/triple blocks 接入。该上界合法、无参数，并在 18/20 条 fast 查询上改善初始 incumbent；但额外图搜索与独立 block attachment 仍不回本，单独版和 nested 组合版都慢于正式 Test21，因此全部代码与统计字段撤回。

## 1. 构造

固定 root-star 根 `r`，令 primary/secondary anchors 为从 `r` 看最远的两个 groups。沿 group-distance tight arcs 恢复 `r` 到两个 anchor groups 的确定性最短路，按 edge id 取并得到 backbone `P`，其边只支付一次。

从 `P` 做一次 multi-source Dijkstra，得到 `d_P(x)`。对其余 `k=g-2` 个 groups 的 block `S`，`1<=|S|<=3`，定义：

```text
E_P(S) = min_x [d_P(x) + sum(i in S) gd_i(x)].
```

每个 `E_P(S)` 对应一棵真实 attachment tree。由于 `4*3>=k` 对当前 `g<=13` 的目标成立，更一般地本候选只在四块确实覆盖时使用四轮 subset DP；候选为：

```text
cost(P) + min partition of K into at most four blocks E_P(S).
```

实现使用连续 vectors、edge bitset 和离线 mask DP，没有 Hash、图/query 压缩或数据特判。时间为一次图搜索加 `O(n sum(s=1..3) C(k,s)+3^k)`，空间为 `O(n+m+2^k)`。

## 2. 正确性

1. tight-arc DFS 恢复的每条 anchor path 都是最短路；按 edge id 取并后 `cost(P)` 不重复计公共边。
2. 对 block `S`，从 argmin `x` 到 `P` 的最短路与 `x` 到各组的最短路之并是一棵合法连接子图，成本不超过公式和。
3. 各 block attachment 与 `P` 取并仍是合法 GST；独立 block 之间即使重叠也只会在标量和中重复付费，不会低估。
4. 因此该机制只提供可行上界，不能错误剪枝。Release/O2 随机 `300/300`（seed `713361`）通过。

## 3. Fast20

单独集成快照 `20260713_002545`：

```text
formal Test21       12.380s
dual-anchor upper   12.867s
```

逐库 solver 口径：

| dataset | time ratio | ordinary values ratio | upper improvements |
| --- | ---: | ---: | ---: |
| DBLP | `1.021x` | `0.958x` | `3/4` |
| DBLP-new | `0.951x` | `0.916x` | `4/4` |
| MovieLens | `1.170x` | `1.000x` | `4/4` |
| Toronto | `1.027x` | `0.991x` | `4/4` |
| Toronto-new | `0.987x` | `0.914x` | `4/4` |

fast backbone 通常只有 `2--85` 条边，block scans 为每条 `220.5k--612.5k`。稀疏四库每条 upper 约 `1--3ms`，MovieLens 的额外 multi-source Dijkstra 每条约 `14--15ms`；其 incumbent 已经很强，payload 不变，所以整库明显退化。

## 4. 两个修正均失败

### 4.1 Global group-TSP certificate

候选从已有 Held-Karp path table 计算 group-metric 最短 cycle/2。任意 GST 加倍后给出访问各组代表点的闭游，因此该值是合法自由根 lower；若等于 incumbent 可跳过 backbone。fast20 为 `0/20` 命中，MovieLens lower 仍比 best 低约 `3%--18%`。带诊断版快照 `20260713_003028` 为 `12.700s`，改善主要是运行波动，不能视为门控成功；字段撤回。

### 4.2 与 Recursive Ordering 组合

把 Test65 的 `D0` 后 `B` 顺序放在 dual-anchor upper 之后，通过随机 `100/100`（seed `713381`），fast20 快照 `20260713_003249` 为 `12.889s`。共享上界不足以抵消 quarter/三块完成被推迟，组合仍慢于 formal，也慢于非 nested 版。

## 5. 结论

1. 同时支付两个 anchor paths 可以让 upper 与 ordinary payload跨库下降，但独立 `E_P(S)` 仍没有共享不同 attachment roots 的公共前缀。
2. Test48 的 branch-junction closure 比增加第二 anchor 更关键；下一结构若回到 backbone，必须让 skeleton 直接承担 D2/attachment 信息，而不是继续扩 anchor 数或 scalar blocks。
3. TSP certificate 是共享 lower，不是 A 的原创贡献，且本批零命中，不保留。
4. 不运行 Toronto full 或 DBLP full；正式 Test21 恢复 farthest single-anchor、按 size ordinary rows。

