# Test98：带 Rank 的 Bitmap 有序行

更新时间：2026-07-15。Test98 已合入 Test80 研究版；ReleaseV4 保持冻结。该机制不改变框架 A 的状态、递推、上下界或调度，只为已经生成的 D/A 有序行增加第三种物理表示，并由每行的精确字节数自动选择。它不使用数据集名、固定 `g`、层号、密度阈值或运行时刻特判，也不引入 Hash。

## 1. 动机与结论

Test80 原先把一行保存为 sparse 或 dense。sparse 行只保存有限顶点，空间小，但按顶点访问需要二分；dense 行可以直接下标访问，却必须为所有顶点保存一个 `double`。实际低层行经常处在两者之间：有限值已经多到二分访问昂贵，却仍不足以承担完整 dense 数组。

Test98 增加 **ranked bitmap row**：用一个位图表示哪些顶点有有限值，有限距离仍按顶点顺序紧凑存放，再用每个 64-bit word 之前的累计置位数把顶点编号映射到距离下标。它同时保留 Test80 需要的顺序枚举、按点访问、branch 枚举和有序相交接口。

Release/O2 结果表明该表示值得保留：fast20 solver time 从 Test90 的 `8.261585s` 降到 `7.333835s`，DBLP g13 q5 wall 从 `183.914s` 降到 `158.150s`，最优值及 D/A values、pops 均不变。没有启动约五小时的 DBLP g13 q1。

## 2. 三种物理表示

令 `N=n+1` 为顶点下标域大小，`W=ceil(N/64)` 为全域位图的 word 数，`k` 为当前行的有限值数。普通 D 行还需要标记其中哪些有限值是不可拆分 branch；锚定 A 行不需要这组 branch bits。

| 行类型 | sparse | dense | ranked bitmap |
| --- | ---: | ---: | ---: |
| 普通 D 行 | `12k + 8ceil(k/64)` | `8N + 8W` | `8k + 20W` |
| 锚定 A 行 | `12k` | `8N` | `8k + 12W` |

普通 sparse 的 `12k` 来自 `k` 个 32-bit 顶点和 `k` 个 64-bit 距离，附加的位图按**稀疏下标**标记 branch。普通 dense 保存 `N` 个距离，并按**顶点下标**保存 branch bits。普通 ranked bitmap 保存 `k` 个距离、`W` 个 occupancy words、`W` 个 32-bit rank 前缀和 `W` 个按顶点标记的 branch words。A 行没有 branch bits，因此相应公式少一组位图。

每行完成后直接比较上述三个逻辑字节数，严格选择最小者；相等时保留原 sparse 表示。**这不是通过实验拟合出的 density threshold，而是当前行三种等价表示的精确空间比较。**统计字段 `d_row_bytes_s*` 和 `a_row_bytes_s*` 继续记录容器实际 capacity，对应的布局计数新增 `d_bitmap_rows_s*` 与 `a_bitmap_rows_s*`。

## 3. Ranked Bitmap 如何工作

### 3.1 构造

Test80 在一行搜索结束后已经把有限顶点排序并去重。若该行选择 bitmap，构造过程按此顺序完成三件事：

1. 对每个有限顶点 `v` 设置 `vertex_bits[v/64]` 的对应 bit，并把 `D(S,v)` 或 `A(S,v)` 依次写入紧凑的 `distances`。
2. 从左到右扫描位图，令 `rank_before_word[j]` 等于 word `j` 之前的总置位数。
3. 对普通 D 行，把 branch 顶点写入同样按顶点编号索引的 `branch_bits`。

由于距离数组和位图中的置位都遵循顶点升序，第 `i` 个置位与 `distances[i]` 一一对应。

### 3.2 按顶点读取

查询顶点 `v` 时，先检查它的 occupancy bit。若 bit 为零，该行在 `v` 上没有有限值；否则其紧凑下标为

```text
rank_before_word[v / 64]
+ popcount(vertex_bits[v / 64] 中位于 v 之前的 bits)。
```

因此一次读取只需常数次 word 访问和一次 `popcount`，不需要 sparse 行的二分查找，也不需要为缺失顶点保存 `INF`。

### 3.3 顺序枚举与 branch 枚举

顺序枚举逐 word 取出最低置位，同时顺序读取 `distances`。输出顶点仍严格递增，所以 Test80 原有的离线有序接口不变。普通 bitmap 行的 branch bits 使用顶点域；枚举 branch 时同样逐 word 取置位，并用 rank 取得对应距离。

## 4. 与有序归并的结合

Test98 没有把连接改成通用查表。它根据两侧布局选择仍然有序的执行方式：

- **bitmap + bitmap：** 对 occupancy words 做按位与，只访问共同置位的顶点；两侧距离分别由 rank 取得。
- **bitmap + sparse：** 枚举 sparse 一侧的有序顶点，在 bitmap 中做常数时间成员测试和 rank 读取。
- **bitmap + dense：** 枚举 bitmap 的有限顶点，直接下标读取 dense 距离。
- **没有 bitmap 的组合：** 完全沿用原 sparse/dense 的直接扫描、二分或双指针归并。

D+D、A+D 和三行 completion 都通过这些统一的有序访问接口读取行。普通 branch join 继续只枚举合法 branch；A0 流式 completion 及普通 A completion 也保持原分块和触发时机。completion 已经从驱动行流式得到的距离会直接传给访问函数，避免对同一 bitmap 顶点再做一次 rank 查询。

## 5. 正确性与复杂度

**表示等价性。** 对任意一行，sparse、dense 和 ranked bitmap 保存完全相同的有限顶点集合及其距离；rank 公式只是把顶点编号恢复为同一有序距离数组的下标。branch bitmap 也标记同一 branch 集合。因此布局选择不改变任何状态值、剪枝条件、队列键、生产者依赖或 completion 分块。

**时间。** bitmap 构造为 `O(k+W)`，按点读取为 word-RAM 模型下的 `O(1)`，顺序枚举为 `O(k+W)`，两个 bitmap 的交集为 `O(W+c)`，其中 `c` 是共同有限顶点数。只有当 bitmap 字节数严格小于 sparse 时才选择它；由第 2 节公式可得此时 `W=O(k)`，所以构造和完整枚举不会比保存该行引入更高的渐进阶。整个 solver 的最坏时间上界不变。

**空间。** 每行使用三种等价表示中的最小逻辑字节数，因此持久行 payload 不会高于旧 sparse/dense 二选一。rank 目录本身为每个 64-bit word 一个 32-bit 计数；它不是分层 succinct directory，目标是以简单可审查的实现换取常数时间成员读取。整个 solver 的渐进空间上界同样不变。

## 6. Release/O2 验证

### 6.1 正确性

- 宽范围随机小图与 DPBF 对拍 `200/200`，seed `715801`，误差不超过 `1e-6`。
- 固定 g13 随机小图对拍 `50/50`，seed `715803`，全部一致。
- DBLP g13 q5 最优值保持 `14.5867185184`；D/A values、pops 及 completion checks 与 Test90 完全一致。
- DBLP g13 q25 最优值保持历史 Test80 的 `12.0889822532`；D2--D5 与全部 A 层的 values/pops 一致，D6 和 A0 completion 的减少来自 Test90 的流式上界反馈。

### 6.2 Fast20

最终快照为 `result_snapshot/fast/20260715_034137`。下表使用逐询问 wall 的数据集内求和；Test90 对照为 `20260715_015858`。

| 数据集版本 | Test90 | Test98 | 比例 |
| --- | ---: | ---: | ---: |
| Toronto | `1.495s` | `1.341s` | `0.897x` |
| Toronto-new | `3.008s` | `2.872s` | `0.955x` |
| DBLP | `0.318s` | `0.272s` | `0.857x` |
| DBLP-new | `0.372s` | `0.346s` | `0.931x` |
| MovieLens | `3.142s` | `2.587s` | `0.823x` |
| **合计** | **`8.335s`** | **`7.418s`** | **`0.890x`** |

排除 runner 开销后，stats 中的 solver time 为 `8.261585s -> 7.333835s`，改善 `11.2%`。

### 6.3 Toronto g13

Test98 的最终记录为 `7.101916s / 57.582MiB / 0.7048467020`，solver 内部 `total_ms=7086.938`；Test90 三次 `total_ms` 为约 `8.048--8.079s`。D2 的 `65/66` 行选择 bitmap，D3 为 `8/220`；A1 为 `12/12`，A2 为 `5/66`，更高层均保持 sparse。

| 层 | Test90 row bytes | Test98 row bytes |
| --- | ---: | ---: |
| D2 | `4,136,492` | `3,678,244` |
| D3 | `6,910,964` | `6,906,720` |
| A1 | `553,584` | `472,736` |
| A2 | `1,406,244` | `1,401,508` |

### 6.4 DBLP g13 q5

最终记录位于 `result_snapshot/gates/20260715_test98_ranked_bitmap_dblp_q5`。

| 指标 | Test90 | Test98 | 变化 |
| --- | ---: | ---: | ---: |
| query wall | `183.914s` | `158.150s` | `-14.0%` |
| solver total | `183.329s` | `157.636s` | `-14.0%` |
| ordinary | `96.390s` | `81.354s` | `-15.6%` |
| anchored | `34.729s` | `29.702s` | `-14.5%` |
| completion | `2.141s` | `2.809s` | `+31.2%` |
| peak RSS | `2134.734MiB` | `2130.301MiB` | 基本不变 |

该询问只有 D2 的 `5/66` 行选择 bitmap，但这几行会被后续大量连接重复读取。D2 payload 从 `43,560,712B` 降到 `40,100,112B`；状态数完全不变，时间收益来自更便宜的重复成员读取。completion 单项变慢，但只占总时间的小部分，未抵消 D/A 主阶段收益。

## 7. DBLP g13 q25 压力长门

最终记录位于 `result_snapshot/gates/20260715_test98_ranked_bitmap_dblp_q25`。该次运行使用当时的 Test80 主线，即 Test83、Test84、Test87、Test90 与 Test98 的组合；历史对照是 2026-07-14 跨询问剖析时、尚未合入这些机制的 Test80。因此本节能回答“Test83--98 相对历史版本的实际时空结果”，但**不是 Test90 与 Test98 的单变量对照**。

| 指标 | 历史 Test80 | 当次 Test80/Test98 | 变化 |
| --- | ---: | ---: | ---: |
| 最优值 | `12.0889822532` | `12.0889822532` | 一致 |
| query wall | `7619.938s` | `8652.038s` | `+13.5%` |
| solver total | `7618.003s` | `8650.484s` | `+13.6%` |
| ordinary | `3340.812s` | `3738.620s` | `+11.9%` |
| anchored | `4228.012s` | `4859.751s` | `+14.9%` |
| completion | `1060.844s` | `1211.875s` | `+14.2%` |
| query peak RSS | `25541.363MiB` | `18493.941MiB` | `-27.6%` |

空间结果是明确的：当前主线把 q25 峰值降低 `7047.422MiB`，约 `6.88GiB`。时间结果同样必须如实保留：q25 比历史版本慢 `13.5%`，因此**当前包含 Test98 的主线不是跨查询普遍加速**；Test98 在 q5 的受控对照上有正时间收益，而当前主线在大量行都选择 bitmap 的 q25 上呈现明显的空间换时间。

### 7.1 行布局与 payload

当前 q25 没有任何 D/A row 选择 dense。下表中的 payload 是各层 stats 记录的 row bytes；A4 由 Test84 流式消费，因此各层 payload 之和是累计构造量，不等于同一时刻全部驻留。

| 层 | bitmap / sparse rows | 历史 payload | 当前 payload |
| --- | ---: | ---: | ---: |
| D2 | `66 / 0` | `1267.7MiB` | `1074.6MiB` |
| D3 | `220 / 0` | `3928.9MiB` | `3088.5MiB` |
| D4 | `465 / 30` | `5522.1MiB` | `4110.8MiB` |
| D5 | `365 / 427` | `2487.9MiB` | `2035.1MiB` |
| D6 | `193 / 731` | `1547.4MiB` | `1185.5MiB` |
| A1 | `12 / 0` | `228.7MiB` | `191.0MiB` |
| A2 | `66 / 0` | `1135.7MiB` | `858.2MiB` |
| A3 | `220 / 0` | `2997.7MiB` | `2175.6MiB` |
| A4 | `482 / 13` | `4864.8MiB` | `3494.3MiB` |

D2--D5 和 A1--A4 的状态数与历史版本一致，这些同状态层的累计 row payload 实际减少约 `5405.4MiB`，可直接归因于三布局选择。D6 payload 另降 `361.8MiB`，但其中同时包含 Test90 将 D6 values 从 `133,817,411` 降到 `117,235,722` 的状态收益，不能全部归给 Test98。全部层的累计 row bytes 从 `23980.8MiB` 降到 `18213.6MiB`；当前 A4 的流式生命周期和容器外开销解释了它与进程 peak RSS 不能直接相加比较。

### 7.2 工作量与时间边界

当前版本的 ordinary values、pops 分别减少 `1.19%/1.14%`，completion scans/checks 分别减少 `1.68%/1.70%`；全部 A values/pops 不变。尽管逻辑工作没有增加，ordinary、anchored 和 completion 仍同时变慢。结合 D2、D3、A1--A3 全部选择 bitmap，这一结果**高度提示** rank/popcount 读取和 bitmap 交叉访问存在可观累计代价，但下段所述版本差异使它不能被当作严格的单变量归因。

join stats 给出更具体的边界：历史 ordinary 的 direct/binary/linear work 合计约 `58.861B`，Test98 direct work 为 `65.229B`；anchored 相应为 `47.237B -> 54.037B`。内联版 Test102 虽在 fast20 降低约三成 direct work，却因 DBLP q5 两次回退而撤回；后续 Test103 把同一 branch 半连接隔离为独立 `noinline` 内核，fast20 保持 `30.7%/32.1%` 的 work 降幅，q5 两次恢复到 `152.286s/151.027s`，现已合入 Test80。完整说明见 `test103_out_of_line_bitmap_semijoin.md`。

由于没有再花两小时运行一个“仅 Test90、无 Test98”的 q25，对 `+13.5%` wall 不能作严格的单机制因果分解。受控 q5 对照仍证明 Test98 在部分结构上更快；本次 q25 则否决“精确字节最小布局也会自动最小化时间”的推断。后续不能据此增加数据集、`g`、层号或密度开关。Test103 已解除 bitmap 与 branch join 的部分耦合，Test104 又为 bitmap-compatible completion 恢复三路 word 相交；Test105–107 随后减少了 completion 的实际 partitions 后工作。后续组合版 q25 已实测，但它同时包含三项机制，只能作为当前版本对本节 Test98 的组合对照。

### 7.3 长跑前的空间预测

实际长跑之前，旧 q25 聚合统计已给出每层 row 数、有限值数和旧 payload。把一层的所有行都按 bitmap 公式计算，再利用

```text
sum(min(old_row, bitmap_row)) <= min(sum(old_row), sum(bitmap_row))
```

可以在不知道层内各行密度分布的情况下，得到逐行选择后的**确定性 payload 上界**。`N=2,497,783`、`W=39,028` 时，预测旧累计 row payload `23980.8MiB` 可降到不超过 `18784.0MiB`，至少减少 `5196.8MiB`。本次同状态层实测减少 `5405.4MiB`，确认了该预测的方向和保守性；预测本来就不等于进程 peak RSS，也没有预测 wall。

## 8. 文献与原创性边界

位向量上的 `rank` 是经典静态数据结构原语，可追溯到 Guy Jacobson 的 *Space-efficient Static Trees and Graphs*（FOCS 1989，[DOI](https://doi.org/10.1109/SFCS.1989.63533)，[DBLP](https://dblp.org/rec/conf/focs/Jacobson89)）。Test98 不把“位图加 rank”本身作为新的 succinct data structure。

本仓库中的方法适配是：在框架 A 的 D/A 有序行内统一组织 occupancy、按序距离、branch bits 与每 word rank；让 D+D、A+D 和 completion 在 sparse/dense/bitmap 三种布局间保持同一无 Hash 接口；并以每行精确字节最小化取代经验密度阈值。其论文价值应与框架 A 的状态设计和有序求值整体讨论，不能单独宣称 rank 原语原创。

## 9. 当前边界

Test98 三布局继续保留在 `methods/Test/test80_anchor_progressive.cpp`，其上已叠加 Test103 的独立 branch 半连接、Test104 的 completion 位图相交和 Test107 的三级 completion 下界。当前组合版 q25 为 `8121.863s/18498.1MiB`，相对本节 Test98 的 `8652.038s/18493.9MiB` 时间下降而空间不变；两版 D/A 状态逐项相同。ReleaseV4 未修改，DBLP g13 q1 未随这些研究机制重跑。Test98 的 q25 证明布局在重查询上有显著空间价值，也暴露了消费接口耦合；当前后续入口依次见 `test103_out_of_line_bitmap_semijoin.md`、`test104_completion_bitmap_intersection.md` 与 `test105_completion_partition_minimum.md`。
