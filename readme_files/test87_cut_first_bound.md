# Test87：Directed-Cut 优先的三级下界求值

更新时间：2026-07-15。Test87 已合入 Test80 研究分支，建立在 Test83 的按需 group-tour 与 Test84 的 A 顶层流式调度之上。它只调整三个既有合法下界的求值顺序，不改变 D/A 状态、离线有序连接、优先队列、完整化或 ReleaseV4 发行源码。

## 1. 问题与核心结论

Test83 首先计算 `max(farthest,directed-cut)`，幸存者再计算 group-tour。DBLP g13 q5 中 directed-cut 已经能拒绝绝大多数候选，但 Test83 仍会在每次首次下界访问时扫描全部剩余组以得到 farthest。对含约 250 万顶点的大图，这些分散的 group-distance 读取本身很重。

Test87 将求值拆成三个严格递增的阶段：

```text
C(v)                         directed-cut
max(C(v), F(v))              再加入 farthest
max(C(v), F(v), T(v))        最后加入 group-tour
```

**每一阶段都已经是合法下界。前一阶段能拒绝时立即结束；只有幸存者才进入后一阶段。** DBLP g13 q5 因此只为 D/A 的 `1.724%/1.054%` 首次下界访问计算 farthest，wall 从 Test84 的 `242.886s` 降到 `186.896s`，答案和状态计数保持不变。

## 2. 主线流程

对当前候选代价 `d`、顶点 `v`、剩余组集合 `R` 和可行上界 `best`，依次执行：

1. 计算并缓存 directed-cut 下界 `C(v,R)`。若 `d+C(v,R)>best`，立即拒绝。
2. 对仍可能改善上界的候选，扫描 `R` 的组距离，计算 `F(v,R)`，令 `H2=max(C,F)`。若 `d+H2>best`，立即拒绝。
3. 对第二次检查的幸存者，按 Test83 计算 group-tour `T(v,R)`，令 `H=max(C,F,T)`。若 `d+H>best`，拒绝；否则进入原来的行更新或图传播。
4. 同一 `(row,v)` 再次被访问时，从已经到达的缓存阶段继续，不重复计算已完成的分量。

这套流程同时用于普通 D 和锚定 A 的 seed 与 relax 候选。**步骤 1 是 Test87 唯一新增的提前终止点。** 通过步骤 1 的候选仍会恢复 Test83 的 `max(C,F)`；通过步骤 2 的候选仍会恢复旧 Test80 的完整 `max(C,F,T)`。

## 3. 缓存状态

每行使用 `heuristic_stamp[v]` 区分缓存世代，`heuristic[v]` 保存当前最大下界，单字节 `heuristic_state[v]` 只取四种阶段：

| 状态 | 含义 |
| --- | --- |
| `cut ready` | 只计算了 `C` |
| `farthest source` | 已计算 `C/F`，当前由 `F` 主导 |
| `dual source` | 已计算 `C/F`，当前由 `C` 主导 |
| `tour ready` | 已计算完整 `C/F/T` |

来源区分用于保持原下界主导者统计。新增 `d_h_far_evals_s*` 与 `a_h_far_evals_s*` 记录各层实际执行 farthest 扫描的次数；`d_h_evals_s*` 与 `a_h_evals_s*` 仍表示首次访问次数。没有 Hash，也没有数据集、`g`、层级、密度或时间特判。

## 4. 正确性

`C`、`F`、`T` 都是补全当前部分解所需额外代价的下界，因此

```text
C <= max(C,F) <= max(C,F,T).
```

若步骤 1 满足 `d+C>best`，更强的两个下界也必然满足拒绝条件，旧算法同样不会接受该候选。若步骤 1 不拒绝，Test87 计算与 Test83 相同的 `max(C,F)`；若步骤 2 仍不拒绝，又计算与旧 Test80 相同的完整最大值。于是所有最终接受的候选具有相同的下界值和优先队列键，所有被提前拒绝的候选也能被旧完整下界拒绝。

**结论：Test87 保持精确最优值，并保持 Test84 的普通/锚定 values、pops 与 completion checks。** Release/O2 随机验证包括 seed `715201` 的 `100/100`；Test88 完整撤回后又以 seed `715241` 完成宽范围 `50/50`、seed `715242` 完成固定 g13 `20/20`，均与 DPBF 在 `1e-6` 内一致。

## 5. 时空复杂度

设一行首次访问 `N_C` 个顶点，其中 `N_F` 个通过 cut 检查，`N_T` 个继续通过 farthest 检查。Test87 的下界工作为

```text
N_C * cost(C) + N_F * cost(F) + N_T * cost(T),
```

而 Test83 为 `N_C*(cost(C)+cost(F)) + N_T*cost(T)`。因为 `N_T<=N_F<=N_C`，Test87 不增加任何分量的调用次数；最坏情况下三者相等，渐进时间与 Test83 相同。缓存数组没有增加，渐进空间不变。

## 6. Release/O2 实测

Fast20 快照为 `result_snapshot/fast/20260715_010736`，基线 Test84 为 `20260714_235447`。20 条答案和普通/锚定 values、pops、completion checks 全部相同。wall 总和为 `8.381594s -> 8.368112s`，属于短套件波动范围；D 实际 farthest 比例为 `2,595,123/16,780,632=15.465%`，A 为 `2,696,363/8,296,619=32.500%`。

Toronto g13 q1 三次结果保存在 `result_snapshot/gates/20260715_test87_cut_first_toronto`：`8.322481s/8.332020s/8.345680s`，中位数 `8.332020s`，答案均为 `0.7048467020`。它与 Test83 历史三次中位数 `8.296s` 基本相当，说明小图中省去的读取不足以稳定超过计时波动。

DBLP g13 q5 的原始输出保存在 `result_snapshot/gates/20260715_test87_cut_first_dblp_q5`：

| 指标 | Test84 | Test87 | 变化 |
| --- | ---: | ---: | ---: |
| wall | `242.885516s` | `186.895733s` | `-23.05%` |
| ordinary D | `139.609703s` | `97.984235s` | `-29.82%` |
| anchored A | `57.499670s` | `35.212340s` | `-38.76%` |
| completion | `2.087352s` | `2.440190s` | 短阶段波动 |
| peak RSS | `2134.477MiB` | `2134.043MiB` | 基本相同 |
| ordinary values/pops | `35,981,208/7,599,016` | 相同 | `0` |
| anchored values/pops | `3,999,227/1,751,425` | 相同 | `0` |
| completion checks | `4,654,642` | 相同 | `0` |
| 最优值 | `14.5867185184` | `14.5867185184` | 相同 |

q5 的 D/A farthest 实际求值率分别只有 `6,007,855/348,509,194=1.724%` 和 `1,502,112/142,492,521=1.054%`。该图的 group-distance rows 共约 `247.7MiB`；每次 farthest 都从多个大数组读取一个顶点位置，因此避免的不是单纯几次 `max`，而是大量跨工作集内存读取。这解释了 fast/Toronto 基本持平而 DBLP 显著加速。

本轮没有运行五小时级 DBLP g13 q1。q5 已提供状态完全相同且 wall 下降 23% 的大图证据，尚不足以为一次求值顺序优化重复启动 q1。

## 7. 论文与发行边界

Test87 没有新增外部论文引用；directed-cut、farthest 和 group-tour 的来源仍见 Test80 方法文档。该机制是仓库内对已有合法下界的等价短路求值，不改图或查询，也不改变框架 A。ReleaseV4 保持冻结；若形成下一发行版，只需保留三级缓存和短路条件，研究计数字段可以删除。

后续 Test88 的补集预筛选与 Test89 的 cut 内同序前缀短路均已撤回：前者增加内存且跨套件不稳定，后者在 q5 只省约 9% potential 读取却增加 8.6% wall。两项否决不改变本文的 Test87 数据与结论。

当前 Test80 又在本文基线上保留 Test90 的流式 A0 completion；它改变后续顶层 D 的状态数，故新组合版数据见 `test90_streamed_a0_completion.md`，本文继续作为纯 cut-first 的冻结对照。
