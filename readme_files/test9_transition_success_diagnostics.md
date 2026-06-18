# Test9 同根超集转移成功率诊断

本文回到 `Test9`，研究核心同根枚举：

```text
nxt = mask | t
cand = dp[mask][v] + dp[t][v]
```

目标是定量回答：

```text
哪些候选真正会成功更新 dp[nxt][v] 或 h[nxt][v]？
成功候选是否满足一些可提前判断的必要条件？
能否据此减少 up_subset / merge_scan / merge_dense？
```

## 新增统计

`test9_stats.txt` 增加以下字段。

总体成功率：

```text
dp_try              live dp 写入候选数
dp_succ             dp[nxt][v] 实际成功更新次数
dp_first            dp 首次 finite 次数
dp_improve          dp 已 finite 后被改善次数
dp_old              尝试时 dp[nxt][v] 已 finite 的次数
dp_ge_best_try      cand >= best 的 dp 尝试次数
dp_ge_best_succ     cand >= best 但仍成功写入 dp 的次数

h_try               future h 写入候选数
h_succ              h[nxt][v] 实际成功更新次数
h_first             h 首次有效次数
h_improve           h 已有效后被增大次数
h_old               尝试时 h[nxt][v] 已有效的次数
h_ge_best_try       h value >= best 的尝试次数
h_ge_best_succ      h value >= best 但仍成功写入 h 的次数
```

按 size 分布：

```text
src=<k> dp_try=... dp_succ=... h_try=... h_succ=...
t=<s>   dp_try=... dp_succ=...
nxt=<r> dp_try=... dp_succ=... h_try=... h_succ=...
```

## 样本

本轮使用 Release 运行：

```text
data_new/MovieLens/query_g8_uniform.txt   q1-q3
data_new/MovieLens/query_g10_uniform.txt  q1-q2
data_new/Toronto/query_g8_uniform.txt     q1-q2
data/Toronto/query_g10.txt                q1-q3
```

共 10 条查询，全部为 `g >= 8`。

## 主要观察

### 1. `t=0` 对 dp 更新完全无效

所有样本中：

```text
t=0 dp_succ=0
```

例子：

```text
MovieLens g8 q1:  t=0 dp_try=23027  dp_succ=0
MovieLens g10 q1: t=0 dp_try=80459  dp_succ=0
Toronto g8 q2:    t=0 dp_try=834344 dp_succ=0
Toronto g10 q2:   t=0 dp_try=416859 dp_succ=0
```

原因很直接：

```text
nxt = mask | 0 = mask
cand = dp[mask][v] + dp[0][v] = dp[mask][v]
```

`Modify(mask,v)` 进入前后 `dp[mask][v]` 已经是当前值，`t=0` 不可能改善 `dp[mask][v]`。

可转化为安全规则：

```text
live-dp 枚举器不应处理 t=0。
```

注意：`t=0` 对 `h` 不一定无效。当 `mask` 本身大小在未来 complement 查询范围内时，`h[mask][v]` 可能有用。因此只跳过 `dp` 部分，不直接跳过全部逻辑。

### 2. 偶数 g 下 `|mask|=H` 的 live-dp 更新无效

当 `g` 为偶数、`H=g/2` 时，处理 `|mask|=H`：

```text
g - 2|mask| = 0
```

live-dp 只可能枚举 `t=0`，而 `t=0` 的 dp 更新恒失败。因此：

```text
|mask|=H 时 live-dp 枚举器可完全关闭
```

样本：

```text
MovieLens g8 q1:  src=4 dp_try=364  dp_succ=0
MovieLens g10 q1: src=5 dp_try=803  dp_succ=0
Toronto g8 q2:    src=4 dp_try=140511 dp_succ=0
Toronto g10 q2:   src=5 dp_try=1351 dp_succ=0
```

这条规则对偶数 `g` 很明确。若 `g` 为奇数，`H=floor(g/2)`，则 `g-2H=1`，`|mask|=H` 仍可能通过 `|t|=1` 生成 live-dp 状态，不能直接关闭。

### 3. 大量成功 dp 更新其实 `cand >= best`

从最优值搜索角度，若：

```text
cand >= best
```

则该 partial state 不可能导出严格优于当前 `best` 的完整解，因为后续边权和同根合并代价非负。

本轮统计显示，许多成功写入的 `dp` 都满足 `cand >= best`：

```text
MovieLens g8 q1:
dp_succ=93532
dp_ge_best_succ=36551  (39.1%)

MovieLens g10 q1:
dp_succ=133755
dp_ge_best_succ=24862  (18.6%)

Toronto g8 q2:
dp_succ=8589850
dp_ge_best_succ=2549400  (29.7%)

Toronto g10 q2:
dp_succ=21347797
dp_ge_best_succ=14006851  (65.6%)

Toronto g10 q3:
dp_succ=16808088
dp_ge_best_succ=11313952  (67.3%)
```

可转化为安全规则：

```text
若 cand >= best，则不写 dp[nxt][v]。
```

如果只关心最优权重，跳过等于 `best` 的候选也是安全的，因为它不可能改善当前上界。若后续需要枚举所有最优解，则需要保留等号；当前项目只输出权重/一棵树，不需要保留所有等价最优候选。

这条规则不仅减少 `dp_seen`，还会减少后续 `active_vertices[nxt]`、`root_masks[v]`，从而级联减少未来的 `up_subset`。

### 4. `h` 候选几乎不会出现 `value >= best`

本轮数据中大多数查询：

```text
h_ge_best_try=0
h_ge_best_succ=0
```

说明 `h` 写入值 `dp[mask][v]` 通常已经经过当前目标/Dijkstra 条件筛过，不太可能超过 best。

因此，`h` 方向暂时没有类似 `cand >= best` 的明显剪枝空间。

### 5. 尝试时旧值已存在非常常见

典型样本：

```text
Toronto g10 q2:
dp_try=46502734
dp_old=41899651  (90.1%)

Toronto g10 q3:
dp_try=34600748
dp_old=30772042  (88.9%)

MovieLens g10 q1:
dp_try=510479
dp_old=446192  (87.4%)
```

这说明同一个 `(nxt,v)` 会被大量重复候选尝试改善。成功更新虽然不少，但大部分尝试只是和已有值比较后失败。

潜在方向：

```text
对同一个 root v、同一个 nxt，批量求最小 cand 后再写一次。
```

也就是把 `RelaxSameRoot` 从即时写入改成 root-local aggregation：

```text
best_candidate[nxt] = min(best_candidate[nxt], dp[mask][v] + dp[t][v])
```

对一个 `Modify(mask,v)` 内部，先聚合再写，可以减少对 `dp[nxt][v]` 的重复随机访问。但它不一定减少枚举数，只减少写表/比较次数。

### 6. 成功率按 source size 快速下降

示例：

```text
Toronto g10 q2:
src=1 dp_try=4673517  dp_succ=4546565  success=97.3%
src=2 dp_try=32968644 dp_succ=14319920 success=43.4%
src=3 dp_try=8632329  dp_succ=2413933  success=28.0%
src=4 dp_try=226893   dp_succ=67379    success=29.7%
src=5 dp_try=1351     dp_succ=0
```

```text
MovieLens g10 q1:
src=1 dp_try=142322 dp_succ=64287 success=45.2%
src=2 dp_try=225483 dp_succ=59527 success=26.4%
src=3 dp_try=116325 dp_succ=8898  success=7.6%
src=4 dp_try=25546  dp_succ=1043  success=4.1%
src=5 dp_try=803    dp_succ=0
```

必要条件层面可得：

```text
越接近 H 的 source mask，live-dp 成功率越低。
```

但不能仅按 source size 剪掉，因为 Toronto g10 的 `src=4` 仍有不少成功。更合理的做法是结合 `cand < best` 和 `t != 0`。

### 7. 目标大小为 1 的 dp 更新恒失败

所有样本中：

```text
nxt=1 dp_succ=0
```

这对应 `t=0` 且 `mask` 是 singleton，或者等价的自更新。它被 `t=0` 规则覆盖。

## 可以立即尝试的安全剪枝

### 规则 A：live-dp 跳过 `t=0`

当前：

```text
RelaxSameRoot(0)
```

会进入 dp 逻辑，但 dp 永远不成功。

建议：

```text
若 t==0:
    不执行 dp[nxt][v] 更新逻辑
    只在 h_live 时更新 h[mask][v]
```

预期收益：

```text
减少 dp_try = valid_total 级别的无效尝试
减少部分统计/比较开销
```

### 规则 B：live-dp 跳过 `cand >= best`

建议：

```text
if cand >= best:
    skip dp[nxt][v]
```

理由：

```text
后续所有扩展代价非负，不可能从 cand >= best 得到更优完整解。
```

预期收益在 Toronto g10 上很大，因为 `dp_ge_best_succ` 占 `dp_succ` 的 65% 以上。

需要注意：

- 如果要保留等价最优解的具体树，`cand == best` 可能有意义；当前只需要最优权重/一棵树，可以跳过。
- 对 `h` 不直接套用该规则。

### 规则 C：偶数 g 且 `|mask|=H` 时关闭 live-dp 枚举器

建议：

```text
if g % 2 == 0 and popcount(mask) == H:
    skip live-dp enumerator
```

仍保留：

```text
best check
future-h update
```

该规则由 `g-2H=0` 与 `t=0 dp_succ=0` 推出。

## 更进一步的候选方向

### Root-local aggregation

对一个 `Modify(mask,v)`，同一个 `nxt` 可能被多个 `t` 尝试更新。可以先聚合：

```text
local_best[nxt] = min cand
```

最后统一写：

```text
if local_best[nxt] < dp[nxt][v]:
    SetDp(nxt,v,local_best[nxt])
```

这不能减少枚举次数，但可能减少 dense 表随机访问和重复比较。

### 按 `cand < best` 提前过滤 root bucket

若 root bucket 按 `dp[t][v]` 排序，则对固定 `w=dp[mask][v]`：

```text
只需要 dp[t][v] < best - w
```

这可以真正减少枚举量，而不只是枚举后跳过。

可能结构：

```text
root_masks_by_size[v][sz] 按 value 升序
```

但动态更新会破坏排序。可以按层重建，或只对 hot root 重建。

## 当前结论

本轮最明确的必要条件是：

```text
dp 更新有价值必须满足：
1. t != 0
2. cand < best
3. 若 g 为偶数且 |mask|=H，则 live-dp 不可能成功
```

其中第 2 条最有潜力，因为它不只减少一次比较，而是会减少后续状态数，进而影响未来 `up_subset`。

下一步建议先在 `Test9` 或 `Test10` 上实现这三条规则，再复跑：

```text
MovieLens g8/g10
Toronto g8/g10
```

观察：

```text
dp_try
dp_succ
dp_seen
up_subset
dp_ms
```

## 第二轮：mask 与 t 关系诊断

在上一轮基础上，继续增加了以下统计：

```text
pair=<src_size>,<t_size> dp_try=... dp_succ=... h_try=... h_succ=...
h 的 t-size 成功率: t=<s> h_try=... h_succ=...
full_src=<k> try=... improve=...
dp_cur_le_t / dp_cur_gt_t
h_cur_le_t / h_cur_gt_t
```

新增样本：

```text
data_new/MovieLens/query_g10_uniform.txt  q1-q2
data_new/Toronto/query_g8_uniform.txt     q1-q2
data/Toronto/query_g10.txt                q1-q2
```

### 8. `src_size, t_size` 二维关系没有新的普适硬剪枝

从二维 `pair=src,t` 看，除了已知的：

```text
t=0 的 dp_succ=0
偶数 g 且 src=H 的 live-dp dp_succ=0
```

其他 pair 都可能在某些数据上成功。

例子：

```text
MovieLens g10 q1:
pair=4,1 dp_try=7202  dp_succ=490   h_succ=71
pair=4,2 dp_try=17058 dp_succ=553   h_succ=110

Toronto g10 q2:
pair=4,1 dp_try=63064  dp_succ=22005 h_succ=3086
pair=4,2 dp_try=152784 dp_succ=45374 h_succ=2333
```

同样是 `src=4,t=1/2`，MovieLens 成功率很低，Toronto 成功率仍不可忽略。因此不能简单按某个 pair 全局剪掉。

结论：

```text
二维 size 关系适合做优先级/排序，不适合直接做正确性剪枝。
```

### 9. h 的 t-size 模式与 dp 很不同

`h` 更新的成功率对 `t_size` 很敏感。

MovieLens g10 q1：

```text
t=1 h_try=7202   h_succ=71    success=0.99%
t=2 h_try=42627  h_succ=527   success=1.24%
t=3 h_try=93574  h_succ=8906  success=9.52%
t=4 h_try=108044 h_succ=13308 success=12.32%
t=7 h_try=920    h_succ=908   success=98.70%
t=8 h_try=201    h_succ=200   success=99.50%
```

Toronto g10 q2：

```text
t=1 h_try=63064    h_succ=3086    success=4.89%
t=2 h_try=2032746  h_succ=233643  success=11.49%
t=3 h_try=10626467 h_succ=2518234 success=23.70%
t=4 h_try=13178171 h_succ=3347734 success=25.40%
t=7 h_try=176234   h_succ=175445  success=99.55%
t=8 h_try=38896    h_succ=38720   success=99.55%
```

这说明：

```text
大 t_size 的 h 更新一旦被枚举，几乎总能成功；
小 t_size 的 h 更新成功率很低，但不是 0。
```

原因可能是 `h[nxt][v]` 维护的是 max side cost。大 `t` 形成的大侧 `nxt` 更接近未来 complement，且被写入次数少，首次写入比例高；小 `t` 产生的 `nxt` 更容易已有 h 值，因此失败较多。

可转化方向：

```text
h 枚举器应该优先处理大 t_size；
如果要限时/启发式，低 t_size h 候选可作为低优先级；
但精确算法不能直接跳过小 t_size h。
```

### 10. h 的 `t=0` 不可跳过

上一轮发现 `t=0` 对 dp 恒无效，但对 h 不是。

例子：

```text
Toronto g8 q2:
t=0 h_try=140511 h_succ=51899

Toronto g10 q2:
t=0 h_try=1351 h_succ=165

MovieLens g10 q2:
t=0 h_try=34 h_succ=2
```

这通常发生在 `src=H` 时：

```text
nxt = mask
h[mask][v] = max(h[mask][v], dp[mask][v])
```

该 `h[mask]` 未来可能作为某个同大小 complement 被查询。

因此可安全规则必须写成：

```text
live-dp 跳过 t=0；
h 不跳过 t=0。
```

### 11. `dp[mask][v] <= dp[t][v]` 不是成功必要条件

新增代价关系统计：

```text
dp_cur_le_t_try / dp_cur_le_t_succ
dp_cur_gt_t_try / dp_cur_gt_t_succ
h_cur_le_t_try / h_cur_le_t_succ
h_cur_gt_t_try / h_cur_gt_t_succ
```

MovieLens g10 q1：

```text
dp_cur_le_t: try=402316 succ=103640 success=25.76%
dp_cur_gt_t: try=108163 succ=30115  success=27.84%

h_cur_le_t: try=255723 succ=20085 success=7.85%
h_cur_gt_t: try=76267  succ=16340 success=21.42%
```

Toronto g10 q2：

```text
dp_cur_le_t: try=40731277 succ=18270994 success=44.86%
dp_cur_gt_t: try=5771457  succ=3076803  success=53.31%

h_cur_le_t: try=35710247 succ=8534675 success=23.90%
h_cur_gt_t: try=2417474  succ=962320  success=39.81%
```

结论：

```text
当前侧代价 <= 伙伴侧代价 不是 dp/h 成功的必要条件；
当前侧代价 > 伙伴侧代价 时，h 成功率反而更高。
```

这符合 `h` 的语义：`h[nxt][v]` 存的是 max-like 证据，当前侧代价越大越可能刷新 h。

因此不能用 `dp[mask][v] <= dp[t][v]` 或反向条件做精确剪枝。

### 12. full best 改善主要来自中间 src_size

`full_src=<k>` 统计显示，`nxt==U` 的 best 改善很少，且主要来自中间层。

例子：

MovieLens g10 q1：

```text
full_src=1 try=20   improve=0
full_src=2 try=830  improve=4
full_src=3 try=1099 improve=2
full_src=4 try=1055 improve=0
full_src=5 try=679  improve=0
```

Toronto g10 q2：

```text
full_src=1 try=3890   improve=0
full_src=2 try=126399 improve=0
full_src=3 try=84450  improve=7
full_src=4 try=9947   improve=6
full_src=5 try=1323   improve=0
```

这说明：

```text
best 检查本身不是大头；
best 改善更可能来自中间层，而不是 singleton 或 H 层。
```

但由于 `best` 改善会强烈影响后续剪枝，不能仅因改善率低就跳过 best check。

可以考虑：

```text
best check 保留，但放在非常低成本的 direct complement 查询中；
不要为了 best check 枚举大量非 complement t。
```

这与 Test10 的三类枚举器方向一致。

## 第二轮结论

新增观测没有发现比以下三条更强的普适硬剪枝：

```text
1. live-dp 跳过 t=0
2. live-dp 跳过 cand >= best
3. 偶数 g 且 |mask|=H 时关闭 live-dp
```

但补充了对 h 的重要认识：

```text
1. h 不能跳过 t=0
2. h 的大 t_size 成功率极高，小 t_size 成功率低但非零
3. h 更适合做按 t_size 的优先级/桶顺序优化，而不是硬剪枝
```

下一步若继续减少 `up_subset`，最有希望的不是继续找 size 维度硬条件，而是：

```text
1. 把 cand < best 前置到枚举器中；
2. root bucket 按 dp[t][v] 升序维护，使 dp[t][v] >= best - dp[mask][v] 后提前停止；
3. 对 h 按 t_size 从大到小处理，观察是否能更早形成目标剪枝所需 h。
```

## 第三轮：结合最终使用时机与答案树形态

前两轮主要看转移是否“局部更新成功”。这一轮换一个角度：

```text
即使某个 dp/h 更新局部成功，它最终是否真的可能参与更优答案？
```

这需要区分 `dp` 和 `h` 的最终用途。

## 13. dp[nxt][v] 的最终用途

一个被写入的 `dp[nxt][v]` 未来只有三种用途：

```text
1. 图上扩展：当 |nxt| <= H，未来作为当前 mask 跑 Dijkstra。
2. 同根合并：作为某个未来 mask 的 disjoint t。
3. 完整答案：作为 U^mask 的 complement 参与 best 更新。
```

因此，`dp` 更新不仅要问：

```text
cand < dp[nxt][v] ?
```

还应问：

```text
cand 是否仍可能导出优于 best 的完整解？
```

一个安全的更强必要条件是：

```text
cand + LB(v, U^nxt) < best
```

理由：

- `cand` 是当前连通块覆盖 `nxt`、根为 `v` 的代价。
- `LB(v,U^nxt)` 是从 `(nxt,v)` 完成剩余组的安全下界。
- 若二者已经不小于当前 `best`，则任何后续图上扩展、同根合并或 best 更新都不可能产生更优完整解。

这比上一轮的：

```text
cand < best
```

更强，且仍然安全。

注意：

```text
LB(v,U^nxt)
```

计算可能比简单 `cand < best` 贵。可以先用便宜版本：

```text
far(v,U^nxt) = max_{a in U^nxt} group_dist[a][v]
```

做第一层过滤，再决定是否算完整 `LB`。

建议新增统计：

```text
dp_try_cand_ge_best
dp_try_cand_plus_far_ge_best
dp_try_cand_plus_lb_ge_best
dp_succ_cand_plus_far_ge_best
dp_succ_cand_plus_lb_ge_best
```

若 `dp_succ_cand_plus_far_ge_best` 很高，则说明许多“局部成功”的 dp 状态从最终答案角度是死状态。

## 14. h[nxt][v] 的最终用途

`h[x][v]` 只在未来处理 `mask = U^x` 时使用：

```text
dp[mask][v] + h[x][v] <= best
```

也就是说，`h[x][v]` 的唯一用途是影响未来某个 complement mask 是否成为 Dijkstra 目标。

因此一个 h 更新有意义，需要满足至少存在未来可能的 `dp[U^x][v]`：

```text
dp[U^x][v] + h[x][v] <= best
```

当前更新时 `dp[U^x][v]` 可能还不存在，所以不能直接判死。但可以用下界替代：

```text
LowerDpAtRoot(v, U^x) + h[x][v] <= best
```

最简单的安全下界是：

```text
LowerDpAtRoot(v, R) >= 0
```

这只能推出：

```text
h[x][v] < best
```

本轮统计已经显示 `h_ge_best_try` 基本为 0，所以这条弱条件没什么价值。

更有意义的下界是：

```text
LowerDpAtRoot(v, R) >= LB(v, R)
```

但这里 `LB(v,R)` 是从 root `v` 连接到 R 的完成下界；如果 `dp[R][v]` 本身也是一棵覆盖 R 并根为 v 的连通树，则确实：

```text
dp[R][v] >= LB(v,R)
```

因此 h 的安全必要条件可写成：

```text
h_candidate + LB(v, U^x) < best
```

若不成立，未来 `dp[U^x][v] + h[x][v] <= best` 不可能成立，因为 `dp[U^x][v] >= LB(v,U^x)`。

这给了 h 一个此前没有的最终用途剪枝：

```text
更新 h[x][v] 前，若 value + LB(v,U^x) >= best，则该 h 更新没有目标筛选意义。
```

同样可以先用 `far(v,U^x)` 做廉价版本。

建议新增统计：

```text
h_try_value_plus_far_ge_best
h_try_value_plus_lb_ge_best
h_succ_value_plus_far_ge_best
h_succ_value_plus_lb_ge_best
```

这比单看 `h_ge_best_try` 更有意义。

## 15. 同根支配：superset dominance

考虑同一个 root `v` 上两个状态：

```text
A superset B
dp[A][v] <= dp[B][v]
```

直觉上，`B` 被 `A` 支配：`A` 覆盖更多组，代价还不高。

### 对最终 best 是安全支配

如果 `B` 用于完整答案：

```text
dp[B][v] + dp[U^B][v]
```

因为 `A superset B`，有：

```text
U^A subset U^B
```

在同一个 root 上，覆盖更少组的最优代价不应更大：

```text
dp[U^A][v] <= dp[U^B][v]
```

因此：

```text
dp[A][v] + dp[U^A][v] <= dp[B][v] + dp[U^B][v]
```

所以从“完整答案”角度，`B` 被 `A` 支配。

### 对中间 dp 不完全安全

如果 `B` 用于生成精确中间 mask：

```text
B | t = X
```

用 `A` 替代会得到：

```text
A | t superset X
```

这不再是同一个 `X`。未来可能恰好需要 `X` 作为小侧状态或 complement，因此不能直接删除 `B`。

结论：

```text
superset dominance 可用于 best-only 枚举器；
不能直接用于 live-dp 枚举器。
```

建议诊断：

```text
best_check_dominated_by_superset
best_improve_dominated_by_superset
```

实现方式：

对每个 root `v` 维护若干低代价 superset skyline。做 best check 时，如果存在：

```text
A superset mask
dp[A][v] <= dp[mask][v]
```

则当前 mask 作为 best 一侧可跳过。

## 16. 同根支配：subset dominance 对 h 的影响

`h[x][v]` 存的是 max-like 目标证据。若：

```text
A superset B
dp[A][v] <= dp[B][v]
```

对于未来目标筛选，`A` 覆盖更多组但代价不高，似乎也更有价值。但 `h[x][v]` 的索引是精确 `x`：

```text
处理 mask 时只查 h[U^mask][v]
```

若把 `B` 替换成 `A`，索引从 `B|t` 变成 `A|t`，对应未来查询的 complement 也变了。因此：

```text
不能用 superset dominance 直接删除 h 的精确索引更新。
```

但是可以用于 h 的排序：

```text
优先处理覆盖更多且代价不高的 side；
它们更可能形成大 t_size 的高成功 h 更新。
```

这与第二轮观察一致：大 `t_size` 的 h 成功率更高。

## 17. 答案树虚树形态：terminal centroid

把最终答案树压缩成虚树：

```text
叶子/关键点 = 选中的终端实点 + 分叉点
度为 2 的非终端点被压缩
```

对任意一棵覆盖 g 个终端组代表点的树，都存在一个 terminal centroid：

```text
删除该点或该边后，每个连通分量包含的终端数 <= g/2
```

若 centroid 是一个分叉点，并且答案不是一条链，那么在压缩虚树中它的度至少为 3。于是存在一种最优答案的“中心分解”：

```text
root = centroid
root 的每个孩子子树终端数 <= g/2
```

这正是半集合 DP 的结构来源。

更细地说：

- 若 centroid 是点：所有 incident component 的 terminal count 都 <= H。
- 若 centroid 是边：两侧 terminal count 都 <= H。
- 若答案虚树不是链，且选择点 centroid 为非终端分叉点，则 root degree >= 3。

注意边界情况：

- 终端点本身可以在原树中作为分叉点。
- 如果虚树是一条链，centroid 可能是一个终端点或一条边，不一定有 3 个孩子。
- 如果 centroid 落在边上，可以通过该边两侧的两个半树解释，而不是 3 分叉解释。

## 18. 虚树形态能给 DP 什么必要条件？

### 18.1 best 更新应集中在 centroid-compatible split

最终 best 可以由一个 centroid split 见证：

```text
S 和 U^S, 且 |S| <= H, |U^S| <= H
```

当 g 为偶数，这要求：

```text
|S| = H
```

当 g 为奇数，不可能两侧都 <= H，因为 `H=floor(g/2)`，此时点 centroid 通常有至少 3 个 child components，每个 child <=H，但任意二分会有一侧 >H。当前算法用小侧 + 大侧同根 h/best 处理这个问题。

因此：

```text
g 偶数时，best 改善理论上更可能在 |mask|=H 层出现；
g 奇数时，best 改善可能来自多个 child component 的组合，不一定是单个 H split。
```

但本轮统计显示 `full_src=H` 改善并不一定多，说明当前 `best` 已经被 greedy/root-star 提前压得较紧；真正改善可能在中间层先发生。

### 18.2 live-dp 中间 mask 若不是 child-union，最终可能无意义

在 centroid root `v` 下，最优答案的每个有意义状态应当是若干 child subtrees 的 union：

```text
mask = union of terminal sets of some centroid children
```

不是 child-union 的 mask 不会出现在这棵最优答案的 centroid 分解中。

问题是 child 分解未知，无法直接判断。但可以设计诊断：

```text
对最终成功 best 的 root v，记录参与 best 的 mask；
回溯或近似重建该 root 上成功 h/dp 的 child-union 层级；
统计大量成功 dp 更新是否集中在少数 root 和少数 mask family。
```

如果发现成功更新集中在少数 root 的少数 mask family，则可以考虑 root-local antichain/skyline。

### 18.3 根至少 3 个孩子的含义

若最终答案不是链，且 centroid 为分叉点，则每个 child component 的终端数 <=H，且 child 数 >=3。

这暗示：

```text
只由两个大块反复合并出来的中间状态，可能不符合分叉 centroid 形态；
更有价值的是多个相对小的 child components 在同一 root 汇合。
```

但不能直接剪二叉合并，因为 DP 的同根合并本身就是二叉化地构造多叉结构：

```text
((child1 | child2) | child3) ...
```

所以“root 至少 3 个孩子”不能转成简单的“跳过二合并”。

可用方式是：

```text
对同一 root，优先保留/扩展那些可由多个小 child-like mask 组成的状态；
对只有单一路径形态的状态，依赖 LB 和 cand<best 剪掉。
```

这更像启发式排序，不是硬剪枝。

## 19. 结合最终形态的新诊断建议

为了把虚树性质转成可操作数据，建议新增以下统计，而不是立即剪枝：

```text
best_improve_src_size
best_improve_rem_size
best_improve_root_count
best_improve_root_reuse

dp_success_by_root_rank
h_success_by_root_rank
root_success_concentration

dominated_best_try
dominated_best_improve

dp_success_cand_plus_far_ge_best
dp_success_cand_plus_lb_ge_best
h_success_value_plus_far_ge_best
h_success_value_plus_lb_ge_best
```

其中最值得优先实现的是：

```text
cand + far(v,U^nxt) >= best
cand + LB(v,U^nxt) >= best
h_value + far(v,U^x) >= best
h_value + LB(v,U^x) >= best
```

这些条件直接来自最终使用时机，比单纯 `src/t/nxt size` 更接近“最终答案是否可能需要该状态”。

## 第三轮结论

当前可以严格安全推进的方向：

```text
1. dp 更新前加 cand + LB(v,U^nxt) < best 过滤；
   可先用 cand + far(v,U^nxt) < best 作为便宜版本。

2. h 更新前加 h_value + LB(v,U^x) < best 过滤；
   同样先用 far 版本诊断。

3. best-only 枚举器可尝试 root-local superset dominance。
```

不能直接作为硬剪枝的方向：

```text
1. src_size/t_size 某个二维组合；
2. dp[mask][v] <= dp[t][v] 或反向；
3. h 的小 t_size；
4. 虚树 root 至少 3 个孩子。
```

虚树/centroid 性质非常重要，但它更适合指导：

```text
root-local 成功集中度统计
best 改善 root 的识别
child-union mask family 的后验分析
```

而不是马上给出简单硬剪枝。

## 第四轮：最终用途过滤实际验证

根据第三轮提出的两个方向，已在 `Test9` 中增加实际诊断：

```text
dp_far_dead_try / dp_far_dead_succ
dp_lb_dead_try / dp_lb_dead_succ
h_far_dead_try / h_far_dead_succ
h_lb_dead_try / h_lb_dead_succ
full_dom / full_dom_improve
```

运行样本：

```text
data_new/MovieLens/query_g10_uniform.txt q1
data_new/Toronto/query_g8_uniform.txt q1
data/Toronto/query_g10.txt q1
```

### 20. dp 的 `cand + far/LB >= best` 命中率极高

MovieLens g10 q1：

```text
dp_try=510479
dp_succ=133755

dp_far_dead_try=407257       79.8% of dp_try
dp_far_dead_succ=109815      82.1% of dp_succ

dp_lb_dead_try=407863        79.9% of dp_try
dp_lb_dead_succ=109925       82.2% of dp_succ
```

Toronto g8 q1：

```text
dp_try=1605536
dp_succ=859002

dp_far_dead_try=1423929      88.7% of dp_try
dp_far_dead_succ=769766      89.6% of dp_succ

dp_lb_dead_try=1429044       89.0% of dp_try
dp_lb_dead_succ=772251       89.9% of dp_succ
```

Toronto g10 q1：

```text
dp_try=4995027
dp_succ=2174112

dp_far_dead_try=4509620      90.3% of dp_try
dp_far_dead_succ=1970884     90.7% of dp_succ

dp_lb_dead_try=4509621       90.3% of dp_try
dp_lb_dead_succ=1970885      90.7% of dp_succ
```

这说明一个非常强的现象：

```text
大量局部成功写入的 dp 状态，从最终答案角度已经不可能改善 best。
```

尤其是 `far` 和完整 `LB` 的命中数几乎一样：

```text
dp_far_dead_* ~= dp_lb_dead_*
```

因此实际实现时不一定要算完整 `LB(v,U^nxt)`，先用便宜的 `far(v,U^nxt)` 就可能获得大部分收益。

安全剪枝建议：

```text
if cand + far(v, U^nxt) >= best:
    skip dp[nxt][v]
```

这是目前最有潜力的剪枝，因为它会减少：

```text
dp_seen
active_vertices[nxt]
root_masks[v]
未来 up_subset
```

### 21. h 的 `value + far/LB >= best` 命中率很低

MovieLens g10 q1：

```text
h_try=331990
h_succ=36425

h_far_dead_try=79
h_far_dead_succ=79
h_lb_dead_try=907
h_lb_dead_succ=211
```

Toronto g8 q1：

```text
h_try=1302367
h_succ=454520

h_far_dead_try=3
h_far_dead_succ=3
h_lb_dead_try=8817
h_lb_dead_succ=6009
```

Toronto g10 q1：

```text
h_try=4029319
h_succ=988276

h_far_dead_try=18
h_far_dead_succ=0
h_lb_dead_try=135
h_lb_dead_succ=77
```

结论：

```text
h 的最终用途过滤命中率太低，暂时不是主要优化方向。
```

这也解释了为什么前几轮看到 `h_ge_best_try` 基本为 0：能进入 h 更新的状态通常本身已经较小，`value + far/LB` 也很少超过 best。

因此不建议优先实现 h 的 `value+LB` 剪枝；它会增加计算成本，但过滤很少。

### 22. best-only superset dominance 命中存在，但改善几乎没有

MovieLens g10 q1：

```text
full_best_hits=3683
full_best_improve=6
full_dom=2235
full_dom_improve=1
```

Toronto g8 q1：

```text
full_best_hits=30401
full_best_improve=7
full_dom=1106
full_dom_improve=0
```

Toronto g10 q1：

```text
full_best_hits=25952
full_best_improve=15
full_dom=1630
full_dom_improve=0
```

解释：

- `full_dom` 有一定命中，说明 superset dominance 确实存在。
- 但 `full_dom_improve` 几乎为 0，说明被支配的 best 检查很少真正改善 `best`。
- best 检查总量本身相对 `up_subset` 也不是最大头。

结论：

```text
best-only superset dominance 理论上安全，但工程收益可能有限。
```

可以留作后续优化 best check 的小项，不应优先实现。

## 第四轮结论

本轮实际验证后，优先级应调整为：

```text
第一优先级：
    dp 更新前加 cand + far(v,U^nxt) < best

第二优先级：
    cand < best
    live-dp 跳过 t=0
    偶数 g 且 |mask|=H 关闭 live-dp

低优先级：
    h 的 value + far/LB 过滤
    best-only superset dominance
```

最值得马上做的实验版本：

```text
Test11 = Test10 + dp final-use far 剪枝
```

原因：

- `Test10` 已经拆分了三类枚举器，适合把 dp final-use 剪枝放在 live-dp 枚举器里。
- `far` 计算便宜，且实际命中几乎等同完整 `LB`。
- 该剪枝会减少未来状态数，不只是减少当前写表。


