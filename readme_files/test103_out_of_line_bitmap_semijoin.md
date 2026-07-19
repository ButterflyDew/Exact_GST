# Test103：独立 Bitmap Branch 半连接内核

更新时间：2026-07-15。Test103 已合入 Test80 研究版；ReleaseV4 保持冻结。它不改变框架 A 的状态、递推、上下界、调度或 row 布局，而是解除 Test98 中“物理 bitmap 布局固定选择 direct lookup”的耦合。整个机制无 Hash，不使用数据集名、固定 `g`、层号、密度阈值、运行时刻或经验参数。

## 1. 问题与结论

Test98 的 DBLP g13 q25 长跑把 peak 从历史 Test80 的 `25541.363MiB` 降到 `18493.941MiB`，但 wall 从 `7619.938s` 增到 `8652.038s`。状态工作没有增加，join work 却出现反常膨胀：历史 ordinary 的 direct/binary/linear work 合计约 `58.861B`，Test98 的 direct work 为 `65.229B`；anchored 相应为 `47.237B -> 54.037B`。原因是 sparse row 原来会在 direct、binary 与 linear merge 中选择，而 bitmap accumulator 直接进入逐 branch 的成员查询路径。

Test103 对一个可证明的子情形恢复有序半连接：当 accumulator 是 ranked bitmap，且 branch row 是 dense 或 ranked bitmap 时，两者都已有按顶点域组织的 bitset。算法按 word 计算

```text
accumulator.vertex_bits[word] AND branch.branch_bits[word]
```

并且只枚举交集中的顶点。五库 fast20 上 ordinary/anchored direct work 分别稳定减少 `30.7%/32.1%`，两次 solver 为 `6.961516s/6.906246s`，均优于 Test98 的 `7.333835s`。DBLP g13 q5 两次为 `152.286s/151.027s`，答案与全部状态计数保持不变；Test103 阶段没有单独重跑 q25/q1。

## 2. 算法流程

### 2.1 触发条件

普通 D 连接和锚定 A+D 连接共用同一个内核。只有同时满足以下语义条件时进入新内核：

1. accumulator row 的物理布局是 ranked bitmap，因此有限顶点集合已有 `vertex_bits`；
2. branch row 是 dense 或 ranked bitmap，因此不可拆分 branch 集合已有按顶点编号索引的 `branch_bits`。

这不是经验开关。条件只说明两个等价 row 表示已经提供可直接相交的精确集合；其余布局继续走 Test98 原有 direct/binary/linear 路径。

### 2.2 Word 半连接

对每个 64-bit word，内核先取 occupancy 与 branch bits 的按位与。随后反复取最低置位，得到一个同时属于 accumulator 有限域和 branch 集合的顶点 `v`。因此旧路径中“枚举 branch，再询问 accumulator 是否包含 `v`”被改写为先做集合半连接、再读取命中值。

对 accumulator，命中 bit 在紧凑距离数组中的下标是

```text
rank_before_word[word]
+ popcount(vertex_bits[word] 中位于 v 之前的 bits)
```

若 branch row 也是 bitmap，用同一公式取得 branch 距离；若它是 dense，直接读取 `distances[v]`。内核把 `(v, accumulator_value, branch_value)` 交回原 D+D 或 A+D 消费者，后续 seed、下界、堆传播和状态发布完全不变。

### 2.3 为什么是独立内核

Test102 曾把同一半连接和额外的 bitmap×bitmap 读取逻辑以内联 lambda 放入很长的 `Solve` 函数。它在 fast20 上同样减少约三成 direct work，但 DBLP q5 的记录 work 没有减少，wall 仍两次回退到 `162.064s/162.291s`，高度提示大型内联实例扰动了未受益的热路径。

Test103 只保留 branch 半连接，并把实现放在独立 `noinline` 模板函数中。`Solve` 中只留下两处短分派；命中时回调仍在模板实例内专门化，未命中时原二进制热路径不被大段新循环展开。该隔离是代码生成边界，不是按数据选择算法。完整失败前身见 `archive/test102_layout_independent_bitmap_semijoin_20260715.md`。

## 3. 正确性

**集合等价。** 旧路径枚举 `branch_bits` 中每个顶点 `v`，仅当 `v` 同时存在于 accumulator 时调用消费者。新路径枚举 `vertex_bits AND branch_bits` 的置位；两者得到完全相同的顶点集合。

**值等价。** 每个交集 bit 已知在对应 occupancy 中存在。per-word rank 公式恢复它在紧凑距离数组中的唯一有序下标；dense branch 直接按同一顶点下标读取。因此交给消费者的两侧距离与 Test98 一致。

**求解器等价。** 消费者调用顺序仍按顶点递增，Dijkstra seed、下界过滤、队列键和发布 row 均不改变。Release/O2 黑盒 DPBF 对拍为宽范围 `100/100`（seed `716301`）和固定 g13 `50/50`（seed `716302`），误差均不超过 `1e-6`。

## 4. 复杂度

令 `W` 为顶点域 bitmap word 数，`b` 为 branch 数，`c` 为 accumulator 有限域与 branch 集合的交集大小。旧 direct 路径扫描 branch bitset 并做 `b` 次 accumulator 成员查询，可写为 `O(W+b)`；新路径扫描 word 交集并只恢复 `c` 组距离，为 `O(W+c)`，且 `c<=b`。两者最坏渐进阶相同，但新算子输出敏感地删除不在 accumulator 中的 branch 候选，并把随机成员测试改为顺序 word 相交。额外工作空间为 `O(1)`，持久 row payload 与 Test98 完全相同。

## 5. Release/O2 结果

### 5.1 Fast20

Test98 对照为 `result_snapshot/fast/20260715_034137`；Test103 两次为 `20260715_072330` 和 `20260715_073117`，以后者作为当前快照。

| 指标 | Test98 | Test103 run 1 | Test103 run 2 |
| --- | ---: | ---: | ---: |
| query wall sum | `7.417830s` | `7.038716s` | `6.967003s` |
| solver total | `7.333835s` | `6.961516s` | `6.906246s` |
| ordinary | `2.309556s` | `1.997116s` | `1.948501s` |
| anchored | `2.269729s` | `2.168034s` | `2.127914s` |
| ordinary direct work | `43,752,193` | `30,318,002` | `30,318,002` |
| anchored direct work | `39,920,373` | `27,103,726` | `27,103,726` |

D/A values、pops 和最终权重全部不变。两次 solver 分别改善 `5.1%/5.8%`，direct work 的减少完全稳定。

### 5.2 Toronto g13

串行结果位于 `result_snapshot/gates/20260715_test103_noinline_bitmap_semijoin_toronto/final`：`7.060508s / 57.512MiB / 0.7048467020`。它与 Test98 的 `7.101916s/57.582MiB` 基本持平略好，说明独立函数调用没有抵消半连接收益。

### 5.3 DBLP g13 q5

两次结果位于 `result_snapshot/gates/20260715_test103_noinline_bitmap_semijoin_dblp_q5`。

| 指标 | Test98 | Test103 run 1 | Test103 run 2 |
| --- | ---: | ---: | ---: |
| query wall | `158.149758s` | `152.286192s` | `151.026957s` |
| solver total | `157.636263s` | `151.813663s` | `150.544939s` |
| ordinary | `81.353796s` | `79.213464s` | `78.614509s` |
| anchored | `29.702392s` | `29.246122s` | `28.909070s` |
| completion | `2.809005s` | `2.933122s` | `2.905585s` |
| peak RSS | `2130.301MiB` | `2130.414MiB` | `2130.605MiB` |

q5 的 best、D/A states、completion scans/checks 和 direct work 与 Test98 逐字段一致，说明新内核没有减少该询问的记录工作。因此两次较低 wall 只证明 Test103 **没有破坏大图热路径**，不能归因于半连接减少了 q5 的算法工作。

## 6. q25 与论文边界

最后一次 q25 长跑使用 Test98，不包含 Test103。q25 的大量 bitmap rows 和膨胀的 direct work 是 Test103 的动机，但在没有新长跑前，不能声称 `8652.038s` 已被改善。后续只有出现更高层算法突破或发行节点时才重跑 q25；不为单个算子再启动 q1。

Test103 没有引入新的外部论文机制。位向量 rank 的文献边界沿用 Test98 对 Jacobson FOCS 1989 的说明；word-level set intersection 与 semijoin 是标准执行原语，本仓库不宣称其本身原创。可讨论的贡献是框架 A 中**布局选择、branch 语义与离线有序 join planner 的统一**，以及用输出敏感半连接消除物理布局导致的候选膨胀。

## 7. 当前状态

Test103 的 branch 半连接继续保留在 `methods/Test/test80_anchor_progressive.cpp`。后续 Test104 已把相同的“先求精确集合交、再读取距离”原则推进到 bitmap-compatible 的 `A+D+D` completion，并用独立内核避免扩大未命中热路径，详见 `test104_completion_bitmap_intersection.md`。再后续的 Test105–107 已用分块、根和分量三级下界实际减少 completion partitions 后的 scans/checks，见 `test105_completion_partition_minimum.md`。当前组合版已经运行 q25：相对 Test98，Test103 使 D/A direct work 分别减少 `16.25%/19.35%`，全部状态保持不变；由于同次还包含 Test104/107，端到端差异不作为 Test103 单变量结论。ReleaseV4 未修改，DBLP g13 q1 未重跑。
