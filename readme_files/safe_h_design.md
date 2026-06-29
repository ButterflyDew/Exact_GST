# 安全且尽量不回退的 h 设计

> 本文件保留前期推导与实验过程。当前正式采用的精简方案以
> `readme_files/test15_algorithm.md` 为准；未进入 Test15 的候选机制不再作为实现计划。

## 1. 双通道状态

后续实现必须区分：

```text
U[S,v]：算法已构造出的可行树代价，因此 U[S,v] >= D*(S,v)
E[S,v]：已证明精确的值，因此 E[S,v] = D*(S,v)
```

`U` 可以参与同根合并并继续产生可行上界，但只有 `E` 可以作为 h 的 witness：

```text
h(R,v)=max { E[T,v] | T subset R }.
```

这样由集合单调性：

```text
E[T,v]=D*(T,v)<=D*(R,v)
```

可立即得到 h 的安全性。

禁止再使用：

```text
target 弹出 => exact
```

因为 Dijkstra 只相对于当前种子集合定型，不能证明种子集合完整。

## 2. 精确状态的认证方式

### 2.1 singleton

组 a 的 rooted DP 为：

```text
E[{a},v]=dist(v,Group_a).
```

它已由现有多源 Dijkstra 精确计算。

### 2.2 独立下界夹逼

维护一个已证明安全的下界：

```text
L(S,v)=max(
    Far(S,v),
    MST/group-distance lower bound,
    max_{T proper subset S, E[T,v] exists} E[T,v]
).
```

若某个可行上界满足：

```text
U(S,v) <= L(S,v)+eps,
```

则由：

```text
L(S,v) <= D*(S,v) <= U(S,v)
```

可认证：

```text
U(S,v)=D*(S,v),
```

并将其加入 E。这个升级是单调、安全的，不依赖 target 弹出语义。

### 2.3 完整行认证

若某个 mask 的 h 查询非常频繁，但夹逼认证率低，可延迟执行该 mask 的完整 DP 行：

1. 枚举其全部合法子集二分；
2. 使用已认证的精确子行；
3. 执行不提前停止的 Dijkstra；
4. 将整行标记为 E。

每个 mask 最多完整认证一次，因此即使所有 mask 最终都触发，也只退化到 DPBF 的
理论主项，而不会超过项目复杂度上限。实际实现应按累计查询收益决定是否触发。

## 3. 精确双组 rooted DP

对两个组 a、b 和根 v：

```text
P[a,b,v] = min_x dist(v,x)+dist(x,Group_a)+dist(x,Group_b).
```

该值恰好等于 `D*({a,b},v)`。

证明：

- 对任意 x，取从 x 到 v、Group_a、Group_b 的三条最短路；其并是可行树，边并的
  代价不超过三条路径长度之和，所以 `D*<=P`。
- 在任意最优树中取连接根和两个命中组顶点的三路中位分叉点 x。三条分支边不重叠，
  距离不超过对应分支长度，因此 `P<=D*`。

所以二者相等。

计算 `P[a,b,*]` 时，把每个 x 作为初始源，源代价设为：

```text
dist(x,Group_a)+dist(x,Group_b),
```

再执行一次多源 Dijkstra 即可。

## 4. O(g) 组对骨架

全部 `C(g,2)` 对虽然仍被 `2^g` 主项覆盖，但在线查询所有组对需要 `O(g^2)`，不符合
项目要求的 `O(2^g g n)` 项。解决方法是只选 `O(g)` 个组对。

### 4.1 覆盖性质

选择一个组索引环：

```text
0-1-2-...-(g-1)-0
```

若 g 为奇数，环的最大独立集大小为 `floor(g/2)`。任何大小至少
`ceil(g/2)` 的补集 R 都包含一条环边。

若 g 为偶数，环有两个大小为 `g/2` 的交替独立集。分别在奇位置集合内部和偶位置
集合内部增加一条弦，即可破坏这两个最大独立集，使骨架独立数小于 `g/2`。

因此对所有 Test13/Test14 会处理的 `|S|<=floor(g/2)`：

```text
R=U-S
```

都至少包含一条骨架边。只需预计算约 `g+2` 个精确 pair DP，并在每次 h 查询扫描
这些边：

```text
h_pair(R,v)=max { P[a,b,v] | (a,b) 是骨架边且 a,b in R }.
```

单次查询为 `O(g)`，预处理空间为 `O(gn)`。

### 4.2 骨架的权重优化

覆盖性质只要求骨架独立数小于 `ceil(g/2)`。在此约束下，可以优先选择组间距离大的
边，使 pair witness 尽量强：

1. 用 `group_pair` 距离构造高权 Hamilton 环；
2. 偶数 g 时选择权重最大的两条合法奇/偶弦；
3. 或在 g<=22 时直接搜索一个边数受限、独立数合格的高权骨架。

这只影响剪枝强度，不影响正确性。

## 5. 动态大 mask witness

pair 骨架负责避免 h 在初期退化为 Far。随着算法运行，夹逼认证会产生更大的精确
状态 E。对当前补集 R，继续取：

```text
h(R,v)=max(h_pair(R,v), max certified E[T,v], T subset R).
```

在 Test14 的 mask-major 布局中，可按合法 `(current mask,T)` 精确枚举并用有序表
join，总计仍为 `O(3^g n)`。这里扫描的是认证表，不再扫描普通 finite/U 表。

## 6. 延迟完整认证

为每个未认证 mask T 统计：

```text
demand[T]      被补集 h 请求的次数
saved_scan[T]  若整行认证后预计可替代的工作量
cert_cost[T]   完整认证该行的估算成本
```

仅当：

```text
saved_scan[T] >= alpha * cert_cost[T]
```

时完整认证 T。这样额外计算由已经出现的热点需求触发，而不是预先重跑所有 DP。

首版建议先不启用完整行认证，只测试：

```text
singleton + 精确 pair 骨架 + 夹逼认证
```

若 target/relax 数相对旧 Test13 明显回升，再加入延迟认证。

## 7. 必须通过的验证

新的实现应保留随机对拍中的三项断言：

```text
所有 E[S,v] == 完整 DP 的 D*(S,v)
h(R,v) <= D*(R,v)
最终答案 == DPBF
```

性能验收同时比较：

```text
target / valid / relax / pq
h 命中率与平均 witness 大小
夹逼认证数量
pair witness 主导比例
完整行认证次数
wall time / DP time / peak RSS
```

目标不是仅仅恢复正确性，而是让安全 h 的 target 数接近旧 h，同时保留 Test14 的
稀疏空间和批量 join 优势。

## 8. 首版原型结果

已增加实验入口 `gst_test15_main`。它在 Test13 上实现：

```text
U/E 双通道语义
环加弦的 O(g) pair 骨架
LB + pair + certified subset 的夹逼认证
只有 certified 状态可进入 h
```

固定错误实例：

```text
DPBF              55
旧 Test13         56
关闭 h            55
安全 h 原型        55
```

历史白盒审计中，两个随机序列各 50000 个小实例均与 DPBF 一致，并额外检查：

```text
confirmed_overestimate=0
h_overestimate=0
最终答案一致
```

当前工程已删除旧白盒 fuzz 接口；常规回归改由 `gst_random_compare` 黑盒运行主程序与
DPBF 并比较最终答案。若需要重新审计 exact/h witness，应另建独立审计工具，不再把
调试指针暴露在 solver 正式接口中。

Toronto g10 初步性能：

```text
query 1:
旧 Test13 preprocess / DP    91 / 196 ms
安全 h preprocess / DP       436 / 250 ms

query 5:
旧 Test13 preprocess / DP    113 / 5442 ms
安全 h preprocess / DP       405 / 5898 ms
```

首版已经把 DP 回退控制在约 8%（query 5），但 pair 全量预处理给每条查询增加约
300ms，尚未满足“不回退”目标。下一步不应放弃安全认证，而应改成：

1. pair 行按需求延迟计算，容易查询不支付 pair 预处理；
2. 热查询达到阈值后才构造 pair 行；
3. 把认证表迁移到 Test14 的 mask-major 批量 join，消除 Test13 的 root bucket 扫描；
4. 用认证成功率选择真正有收益的骨架边，而不是无条件计算全部 `g+2` 条。

对“每条 pair 达到约 `2n` 次请求后单独生成”的简单延迟策略也进行了测试：

```text
query 1 wall: 706 ms -> 480 ms
query 5 wall: 6344 ms -> 6662 ms
```

它能避免容易查询的预处理，但困难查询因为 pair 证书到达过晚，前两层产生了更多
状态，反而恶化。因此该细粒度延迟策略不采用。

更合适的触发点是层边界：完成 `k=2` 后根据 active/target 膨胀程度决定是否一次性
构造骨架。Toronto query 1 的 `k=2 active` 约 16K，而 query 5 约 438K，区分度很高。
不过最终版本更应直接迁移到 Test14 的批量布局；Test14 已证明相同 DP 主体可显著
降低 join 成本，有空间吸收安全认证目前约 8% 的 DP 回退。

## 9. 夹逼认证统计

统计口径是每次安全模式 `Modify(S,v)` 的认证尝试；同一状态若重复触发 Modify 会重复
计数，因此它反映运行时工作量，而不是唯一状态数。

Toronto g10：

| query | 尝试 | 成功 | 成功率 | LB 主导 | pair 主导 | subset 主导 |
|---|---:|---:|---:|---:|---:|---:|
| q1 | 62,049 | 29,743 | 47.94% | 23,287 | 6,456 | 0 |
| q5 | 938,151 | 419,809 | 44.75% | 289,008 | 130,793 | 8 |
| q36 | 22,660 | 18,595 | 82.06% | 17,736 | 859 | 0 |

按 mask 大小的成功率：

| query | k=1 | k=2 | k=3 | k=4 | k=5 |
|---|---:|---:|---:|---:|---:|
| q1 | 98.35% | 46.35% | 22.53% | 17.07% | 12.29% |
| q5 | 99.44% | 39.36% | 13.20% | 4.56% | 1.37% |
| q36 | 99.94% | 63.25% | 39.42% | 25.71% | 18.32% |

结论：

1. 夹逼本身有效，不能简单删除；
2. 困难查询的大 mask 认证明显不足；
3. pair 证书有实际贡献，尤其 q5 占成功认证的 31.2%；
4. 当前“同根 certified 子集最大值”几乎从不成为最强证书，继续扩大该扫描不划算。

## 10. 更强且代价较小的精确性判断

### 10.1 同 mask 精确锚点的空间传播（优先）

rooted DP 关于根点满足 1-Lipschitz 性。对任意顶点 u、v：

```text
D*(S,u) <= D*(S,v)+dist(u,v)
```

因此若 `(S,u)` 已认证精确：

```text
D*(S,v) >= E[S,u]-dist(u,v).
```

对多个精确锚点取最大值仍是安全下界：

```text
anchorLB(S,v)=max_u { E[S,u]-dist(u,v) }.
```

它与当前只使用“同根更小 mask”的证书正交：统计显示 subset 证书几乎无贡献，而同
mask 已认证顶点很多。实现时不应额外为每个 mask 跑完整最短路；可在当前 Dijkstra
已经访问边 `(u,v,w)` 时同步执行：

```text
anchorLB[v]=max(anchorLB[v],anchorLB[u]-w)
anchorLB[u]=max(anchorLB[u],anchorLB[v]-w)
```

每条已扫描边只增加常数操作，不改变理论复杂度。若新认证点出现，将其值作为新的
anchor 注入；可在当前队列范围内形成认证级联。

### 10.2 利用实际覆盖组而非名义 mask

一棵为 S 构造的树可能沿途命中额外组。若为每个可行值维护实际覆盖掩码 C：

```text
C superset S
```

且该值与 `L(C,v)` 夹逼相等，则可直接认证更大的 `(C,v)`，它作为 h witness 比 S
更强。覆盖掩码可随 provenance 以 OR 传播：

```text
base:  color[v]
merge: coverA | coverB
edge:  cover | color[to]
```

稀疏 Test14 中每个状态增加一个整数即可。该方法尤其适合组较大、路径容易穿过其他
组的 Toronto 查询。

### 10.3 权重优化的 pair 骨架

当前环加弦只保证覆盖，不考虑证书强度。保持相同边数和预处理次数，可用
`group_pair` 距离选择高权 Hamilton 环与弦，使更远的组对优先成为精确 pair witness。
这不增加渐进代价，是 pair 方案最应先做的常数级增强。

### 10.4 热点 mask 的完整行认证

若某个 mask 的搜索本身已覆盖大量顶点，可补齐完整 merge seeds 并让 Dijkstra 跑到
结束，将整行认证为 exact。触发条件应使用：

```text
已扫描边数 / m
已弹出顶点数 / n
该 mask 作为未来 h witness 的预计次数
```

当当前搜索已经接近完整行成本时，补齐认证的边际开销很小，却能为后续大量 h 查询
提供大 mask witness。该方案比预先精确求所有 pair/triple 更符合困难查询的工作量。

### 10.5 暂不优先的方案

- 扩大 certified subset 扫描：现有统计中几乎不主导。
- 大量 exact triple：需要更多完整 Dijkstra，且单位超边覆盖大补集的效率低于 pair。
- 每次认证运行 `O(g^2)` 的更强 MST/1-tree：可能违反 `O(2^g g n)` 项，并增加热点
  常数；除非能预处理成 O(g) 查询，否则暂不采用。
