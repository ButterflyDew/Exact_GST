# Test104：A+D+D Completion 的共享位图相交

更新时间：2026-07-15。Test104 已合入 Test80 研究版，ReleaseV4 保持冻结。它不改变框架 A 的状态、递推、partition、上下界或调度，只让 `A+D+D` completion 在现有 ranked-bitmap 行上恢复顺序、输出敏感的共同根枚举。

## 1. 问题与结论

Test98 为 D/A 行加入 ranked bitmap 后降低了重查询空间，但旧 completion 仍先枚举一张驱动行，再逐顶点查询另外两侧。DBLP g13 q25 的 Test98 长跑记录了 `37.155B` 次 completion scans、`32.815B` 次有效 checks 和 `1211.875s` completion time；逐顶点 rank lookup 已成为物理布局与消费方式之间的冲突。

Test104 对一个可严格识别的子情形改写共同根相交：**左、右多组 D row 的有限根集合都已有顶点域 bitset 时，为当前 A row 的有序根集合按需构造同域 bitset，再按 64-bit word 求三者交集，只枚举交集中的根。** 单组 D 侧直接读取预计算组距离，不需要额外 occupancy；不可达值仍为无穷，空侧取零。只要任一多组侧不是 bitmap，算法完整回退到原来的有序列表、二分或逐顶点读取路径。

保留版本通过当前环境下的 fast20、Toronto、DBLP q5 和 DBLP q32 门槛。fast20 中 completion 从 `870.788ms` 降到 `472.844ms`，总 solver 从 `6.906246s` 降到 `6.884491s`。DBLP q5/q32 的新内核调用数均为零，因此两者只能证明未命中路径没有回退，不能被写成 Test104 的算法收益。Test104 阶段没有单独重跑 q25，但其已保存布局可以严格证明至少 `41,265` 个 completion partitions 会命中新内核；后续当前组合版 q25 实际命中 `83,048` 个 partition。

## 2. 主线流程

对一张已经闭包的锚定行 `A(S,·)`，令 `R_S` 为本行仍可能改善 incumbent 的有序根集合，`U` 为尚未覆盖的非锚点组。原 completion 枚举每个无序划分 `U=L∪R`，并计算

```text
min_v A(S,v) + D(L,v) + D(R,v).
```

Test104 在每个划分上依次执行以下步骤。

1. **检查物理表示。** 空侧和单组侧天然可直接读取；多组侧必须是 Test98 已经选择的 ranked bitmap。至少有一个多组 bitmap 且其余多组侧也都是 bitmap 时，进入新内核。
2. **按需构造 A-root bitmap。** 一张 A row 第一次遇到兼容划分时，才把 `R_S` 写入一个可复用的顶点域 bitset；同一 A row 的后续兼容划分共享它。没有兼容划分时不构造。
3. **按 word 求共同根。** 每个 word 计算 `B_A & B_L & B_R`；空侧或单组侧视为全域，不参与按位与。算法按最低置位顺序枚举结果，因此根顺序仍为递增。
4. **恢复紧凑距离下标。** 对每个命中 bit，利用 `rank_before_word + popcount(lower_bits)` 直接取得 bitmap 行的紧凑距离；单组侧按顶点读取组距离。
5. **执行原更新。** 用完全相同的 `A(S,v)+D(L,v)+D(R,v)` 更新 `best`。不兼容划分继续执行 Test103 的原 completion。

新路径由行已经具备的等价表示决定，不读取数据集名、固定 `g`、层号、运行时间或经验密度。bitmap 行本身仍由 Test98 的三种逻辑字节数严格比较产生。

## 3. 正确性

**共同根等价。** `B_A` 的置位恰好是 `R_S`，D bitmap 的置位恰好是该行的有限根。三者按位与的置位集合因此正好等于旧路径中“被驱动行枚举且通过另外两侧成员检查”的根集合。单组侧的组距离已在所有图顶点上定义，空侧代价为零，所以省略其 occupancy 不删除可行根。

**距离等价。** ranked bitmap 的 `rank_before_word` 记录当前 word 以前的置位数，word 内较低 bit 的 popcount 给出局部偏移。两者之和是该根在紧凑 `distances` 数组中的唯一位置，因此新内核读取的两个 D 值与旧 `BitmapValue` 完全相同。

**求解器等价。** 新旧路径都按顶点递增顺序检查同一个根集合，并执行同一个浮点加法和 `min`。不兼容划分没有改变。当前保留源码最终重建后完成宽范围 `100/100`（seed `716406`）和固定 g13 `50/50`（seed `716407`）的 Release/O2 黑盒 DPBF 对拍，误差均不超过 `1e-6`。

## 4. 时空复杂度

设顶点域 word 数为 `W=ceil((n+1)/64)`，当前 A 根数为 `k`，某个兼容划分的三路共同根数为 `c`。旧路径需要枚举最小驱动域，并对其余侧做成员读取；新路径对每张 A row 支付一次 `O(W+k)` 的 root bitmap 构造，随后每个兼容划分支付 `O(W+c)`。最坏情况下两者仍为线性顶点域工作，completion 的 mask/partition 上界不变；在有限根很多而交集可由 word 并行筛出的情况下，新路径删除逐顶点成员查询。

额外持久工作空间只有一个复用的 `W`-word root bitmap，即 `O(n/word_size)`。DBLP 的 `n=2,497,782` 对应 `39,028` words、`312,224B=0.298MiB`。D/A row payload、priority queue 和状态数均不改变。统计分别报告命中划分数、word scans 和最终根 checks，避免把 word 相交误记为零成本。

## 5. Release/O2 证据

### 5.1 Fast20

对照为 Test103 快照 `result_snapshot/fast/20260715_073117`，Test104 为 `result_snapshot/fast/20260715_080653`。

| 指标 | Test103 | Test104 |
| --- | ---: | ---: |
| query wall sum | `6.967003s` | `6.952451s` |
| solver total | `6.906246s` | `6.884491s` |
| completion | `870.788ms` | `472.844ms` |
| completion driver/root visits | `32,733,889` | `22,685,338` |
| bitmap word scans | `0` | `3,427,380` |
| completion checks | `17,576,380` | `17,576,380` |
| bitmap-compatible partitions | `0` | `62,316` |
| A rows that built root bitmap | `0` | `2,840` |

答案和全部 D/A values、pops、completion checks 保持不变。局部 completion 时间下降 `45.7%`；solver 总时间只下降 `0.3%`，因此该算子不是状态级突破。

### 5.2 未命中门槛

当前环境下重编译的 Test103 控制结果保存在 `result_snapshot/gates/20260715_test103_current_rebuild_controls`。Test104 的 q5、q32 与 Toronto 结果分别保存在同日 `test104_completion_bitmap_intersection_*` gate 目录。

| 门槛 | Test103 control | Test104 | Test104 calls | 结论 |
| --- | ---: | ---: | ---: | --- |
| Toronto g13 | `7.884339s` | 三次中位数 `7.790190s` | `0` | 未命中，无稳定回退 |
| DBLP g13 q5 | `170.556322s` | `156.676919s` | `0` | 状态与 checks 逐项相同，只作无回退证据 |
| DBLP g13 q32 | `475.287922s` | `468.353096s` | `0` | 状态与 checks 逐项相同，只作无回退证据 |

这些时间差不能归因于三路位图相交，因为新内核没有调用。历史 Test103 的更快 q5/Toronto 数字也仍保留，但跨时段波动不能代替本表的当前重编译单变量控制。

### 5.3 q25 的免长跑覆盖下界

最后一次 q25 使用 Test98：D3 为 `220 bitmap / 0 sparse`，D4 为 `465 bitmap / 30 sparse`。仅用这些行数就能证明 Test104 会覆盖大量真实 completion partitions。

- A4 留下 8 个组，所有 `D4+D4` 无序划分共有 `C(12,4)·C(8,4)/2=17,325` 个。任一 sparse D4 最多参与 `C(8,4)=70` 个划分，所以涉及 sparse 行的划分至多 `30·70=2,100` 个；因此至少 `15,225` 个划分两侧都是 bitmap。
- A5 留下 7 个组，所有 `D3+D4` 划分共有 `C(12,5)·C(7,3)=27,720` 个。D3 全是 bitmap，而 30 张 sparse D4 最多排除 `30·C(8,3)=1,680` 个；因此至少 `26,040` 个划分兼容。

两层合计至少 `41,265` 次真实调用。DBLP 每次扫描 `39,028` 个 words，对应至少 `1.610B` 次 word 相交；输出根 checks 仍必须执行，不能从该下界推导端到端加速比例。遵循长跑纪律，本轮没有为这个物理算子重跑 q25 或 q1。

## 6. 论文边界与下一步

word-level bitset intersection 和 rank lookup 都是标准数据结构原语，Test104 不把它们本身声明为原创，也没有新增外部论文引用。可讨论的仓库贡献是：在永久锚点的 `A+D+D` completion 中，把 Test98 的物理行选择、A-root 生命周期和输出敏感共同根枚举统一为一个无 Hash 接口。

Test104 只减少共同根扫描，不减少 `A+D+D` 的 partitions 或有效 checks。后续 Test105–107 已沿本文提出的理论主线形成分块、根和分量三级无参数下界，在 fast20、DBLP q5、q32 与 Toronto 上实际删除 partitions 后的 scans/checks，见 `test105_completion_partition_minimum.md`。Test104 本文继续只负责物理共同根接口，不用后续结果改写其历史单变量结论。
