# Test13：在线 `h` 的半集合 DP

> **正确性警告（2026-06-25）**：当前 h 方案已找到最终答案错误反例。target 弹出
> 不能保证该状态相对于完整 DP 转移集合精确，偏大的状态可能进入 confirmed，随后
> 使 h 超过真实补集代价。固定反例、事件链与对拍器见
> `readme_files/test13_h_correctness_audit.md`。本文件原有 h 正确性论证已失效。

Test13 从 Test11 派生，保留半集合 DP、组距离下界和初始上界，删除稠密
`h[mask][v]` 及 `future-h` 推送过程。

## 1. 状态

```text
dp[S][v] = 覆盖组集合 S、以 v 为同根连接点的最小代价
```

只定义并存储 `|S|<=H=floor(g/2)` 的状态。较大的集合状态完全不存在，
也不会由 live 合并产生；需要完整解时临时拼接两个小侧状态。

## 2. 为什么只需求解到 `g/2`

在一棵最优答案树中，为每个组固定一个实际命中的代表点。如果同一顶点命中
多个组，就在该顶点放置相应数量的组权重，总权重为 `g`。

树的带权重心性质保证存在顶点 `r`，删除 `r` 后每个连通分量中的组权重都不
超过 `g/2`。若当前点存在权重大于 `g/2` 的分量，就沿该唯一重分量移动；
重侧权重严格下降，最终必然到达不存在重侧的顶点。

把删除 `r` 后的每个分量重新连上 `r`，得到若干棵同根子树，每棵覆盖至多
`H=floor(g/2)` 个组。它们在根 `r` 处合并后恢复原最优树。因此至少存在一个
根，使最优解可完全由大小不超过 `g/2` 的精确小侧状态组成。

所以只需对小侧状态执行图上最短路，全集答案通过同根合并得到。这是半集合
DP 的完整性依据，而不是经验截断。

### 大状态二分引理

令 `H=floor(g/2)`，当前正在处理 `|S|=k<=H`，`R=U^S`。假设旧算法中的
某个 `dp[R][v]` 仅由同根不交合并产生，把其合并推导树展开为互不相交的叶子
集合 `A_1,...,A_p`。当前层之前能作为叶子的状态大小均不超过 `k`，所以：

```text
1 <= |A_i| <= k
sum_i |A_i| = |R| = g-k.
```

存在一批叶子，其大小和 `q` 满足：

```text
|R|-H <= q <= H.
```

证明：按任意顺序累加叶子，直到首次达到下界 `|R|-H`。累加前小于该下界，
本次增加量至多为 `k`。

- 若 `g=2H`，则下界为 `H-k`，首次达到后 `q<H-k+k=H`。
- 若 `g=2H+1`，则下界为 `H+1-k`，首次达到后 `q<H+1-k+k=H+1`；
  因为大小为整数，所以 `q<=H`。

其余叶子大小为 `|R|-q<=H`。因此任何旧的纯合并大状态都可按叶子重新分成
两个大小不超过 `H` 的同根部分。

这里所有 `A_i` 都严格属于补集 `R=U^S`，当前状态 `S` 不在这些分块中。
例如 `H=5`、全集分块大小为 `4,4,2` 时，若当前 `|S|=4`，则补集大小只有
`|R|=6`；另一个 `4` 和 `2` 正好构成补集的二分。因而 `4,4,2` 不是反例，
其中一个 `4` 必须由当前 `S` 一侧贡献。

更一般地，待证明的始终是 `R` 能否二分，而不是整个 `U` 的全部分块能否任取
两块完成二分。上面的区间累加论证严格使用
`sum_i |A_i|=|R|=g-k`，从而给出所需的两个不超过 `H` 的补集部分。

### 最终答案只需三个小侧状态

对最优答案树取带权重心 `r`。删除 `r` 后每个分支覆盖的组数不超过 `H`。
反复把任意两个总大小不超过 `H` 的分支集合合并；若最终仍有至少四块，则
任意两块之和都大于 `H`，四块总和严格大于 `2H+1>=g`，矛盾。因此所有分支
可以合并成至多三个集合 `A,B,C`，且三者大小均不超过 `H`。

在算法的同层顺序中选择 `A,B,C` 中最后完成的一个，设为 `S`。另外两块在
`S` 弹出时已经可用；在线枚举 `X subset U^S` 会枚举到另外两块的划分。因此

```text
dp[S][r] + dp[X][r] + dp[(U^S)^X][r]
```

不超过该最优树的代价，同时它本身又对应一个可行完整解，所以必然等于最优
值。这给出完全删除大状态后的全局正确性。

还需说明 `S` 不会被 target 剪枝漏掉。若此时 `best` 已等于最优值，则无需
再发现该拼接；否则 `h` 与 `LB` 都是不超过真实补集代价的安全下界，所以

```text
dp[S][r] + h <= OPT < best
dp[S][r] + LB <= OPT < best.
```

因此 `(r,S)` 必然成为 target，并最终以精确距离弹出，在线拼接随即得到
`OPT`。

## 3. `h` 的在线定义

对固定根 `v`，GST 的精确同根状态满足集合单调性：

```text
A subset B  =>  dp[A][v] <= dp[B][v]
```

原因是覆盖 `B` 的任意可行连通子图也覆盖 `A`，而 `dp[A][v]` 是较弱约束下
的最优值。

处理当前状态 `S`，令 `k=|S|`、`X=U^S`。本层实际使用：

```text
h_S[X][v] =
max { exact_dp[T][v] | T subset X, 1 <= |T| <= k, T is confirmed }.
```

这里的大小上限来自当前 `dp` 的 mask 大小 `k`，不是 `h` 的参数 `X` 的
大小。算法按 popcount 分层处理；进入第 `k` 层时，更大的 mask 尚未求解，
不能作为可信证据。当前层中已经提前处理并 confirmed 的 `k` 大小状态也可
作为安全下界证据。

每个候选 `T subset X`，故由集合单调性：

```text
exact_dp[T][v] <= exact_dp[X][v].
```

因此这些值的最大值仍是完成 `X` 的合法下界。若

```text
dp[S][v] + h[U^S][v] >= best,
```

则在同根 `v` 处完成剩余组的方案不可能优于 `best`。边扩展和同根合并代价
均非负，后续代价也不会重新变小，因此该状态不必为了寻找更优权重而成为
target。实现保留 `<=best` 的等号 target，只会放宽搜索。

关键前提是 `h` 只能使用已经确认精确的 `dp`。同根合并临时生成的值可能
偏大；若把这种上界当成下界，会抬高 `h` 并触发错误剪枝。

Test13 用压缩 bitset 记录 `(mask,v)` 是否已经确认：

- 组候选点上的 singleton `dp=0` 是精确状态；
- 小侧 target 从 Dijkstra 弹出时已经定型，标记为 confirmed；
- 只由同根合并写入、尚未定型的状态不标记。

处理当前小侧 `S` 时只查询一次：

```text
OnlineH(S,v)
```

实现扫描同根 confirmed bucket，只接受：

```text
|T| <= |S|
confirmed[T][v] = true
T subset U^S
```

然后取最大 `dp[T][v]`。若没有可信有效子状态，返回 Test11 的哨兵 `-1`，
即放弃本次 `h` 收紧，而不是使用不可信值。

任意 confirmed `T subset U^S`
本身就是完成补集代价的合法下界，不依赖是否已经构造出另一侧。这样 `h`
只依赖可信小状态，不再需要任何大状态。

## 4. 同根合并

Test13 为每个 `(root,size)` 维护两个连续 bucket：

```text
finite_bucket[root][size]
confirmed_bucket[root][size]
```

live 合并只扫描允许大小的 finite bucket，并要求：

```text
T & S = 0
|T| <= H - |S|
```

随后用 `dp[S][v]+dp[T][v]` 更新 `dp[S|T][v]`。所有根、所有不交有序 mask
对的总数由三进制归属计数限制为 `O(3^g n)`。

在线 `h` 同样只扫描 `size<=|S|` 的 confirmed bucket，再检查与 `S` 不交。
bucket 只跳过不存在状态，不改变有效转移集合。

## 5. 在线补集拼接

每个非 stale 堆顶状态 `(v,S,w)` 在线计算：

```text
comp(v,U^S) =
min dp[X][v] + dp[(U^S)^X][v]
```

其中两侧大小都不超过 `H`。实现扫描同根 finite bucket 中的 `X`，再 O(1)
检查另一侧。该值只用于一次：

```text
best = min(best, w + comp(v,U^S)).
```

拼接得到的是合法完整连通子图，因此只会安全降低上界。重心三分证明保证最优
答案对应的三块中，最后弹出的那块一定能在线枚举到另外两块。

最内层保留两个 O(1) 剪枝：

```text
cand >= best
cand + far(v,U^(S|T)) >= best
```

Test13 把组位分成两半。对每个顶点和每个半掩码，预存该半掩码中距离最远
的组编号。一次 `far` 查询分别查左右半掩码，再比较两个组距离即可：

```text
预处理时间/空间 O(n(2^floor(g/2) + 2^ceil(g/2)))
单次查询 O(1)
```

因此 `far` 不会给 `3^g` 内层额外乘上 `g`。

## 6. Dijkstra

对每个 `|S|<=H`：

1. 扫描 `active[S]`。
2. 计算一次 `OnlineH(S,v)`；函数内部扫描同根、大小不超过 `|S|` 的
   confirmed bucket，同时按需缓存 `LB(v,U^S)`。
3. 满足 `dp+h<=best` 且 `dp+LB<=best` 的状态成为 target。
4. `dp<mx_target` 的非 target 作为最短路前缀入堆。
5. 使用 `std::priority_queue`；`LB` 只作入队、出队和松弛门控。
6. target 定型出堆时标记 confirmed，并执行同根合并。

## 7. 空间变化

Test11 的主要稠密空间是：

```text
dp : 8 * 2^g * (n+1) bytes
h  : 8 * 2^g * (n+1) bytes
```

Test13 保留：

```text
M = sum_{k=0}^H C(g,k)
dp        : 8 * M * (n+1) bytes
confirmed :     M * (n+1) bits
```

辅助空间为 `active`、finite/confirmed root-size bucket、`group_dist`、
两个 `uint8` 半掩码 far 表、MST cache 和单个 mask 的临时数组。在线化本身
将两张稠密 double 表降为一张；confirmed 位图只有同规模 double 表的
`1/64`。实际峰值仍应通过统一的 working-set/RSS 统计验证。

## 8. 统计字段

```text
merge_enum       finite bucket 扫描次数
live_dp_checks   通过 size/finite/cand 检查的转移数
h_calls          在线 h 查询次数
h_checks         confirmed bucket 扫描次数
h_hits           通过 size 与 confirmed 检查的有效子状态数
complement_calls 堆顶在线拼接次数
complement_enum  在线扫描的小侧候选数
complement_hits  找到可行补集拼接的次数
active_seed      所有 mask 扫描的 active 状态数
lb_calls         实际计算 LB 的次数
pq_push/pq_pop
relax_try/relax_ok
prep_ms/group_dist_ms/greedy_ms/dp_ms
```
