# Test102：布局无关的 Bitmap Branch 半连接（已撤回）

## 1. 动机

Test98 的 DBLP g13 q25 压力长门把 peak 从历史 Test80 的 `25541.363MiB` 降到 `18493.941MiB`，但 wall 从 `7619.938s` 增到 `8652.038s`。进一步检查 join stats 后发现，问题不只是一条 `rank/popcount` 指令较贵：历史 ordinary 的 direct/binary/linear work 合计约 `58.861B`，当前 Test98 全部落入 direct 后为 `65.229B`；anchored 同样从约 `47.237B` 增到 `54.037B`。物理布局改变了逻辑 join 计划。

Test102 因此尝试让 bitmap row 重新使用有序半连接。若 accumulator 有 occupancy bitmap，而 branch row 的 branch bits 按顶点域存储，则按 word 计算 `occupancy AND branch_bits`，只枚举公共顶点。已知命中 bit 一定存在，距离下标直接由该 word 的 rank 基值和低位 popcount 得到，不再重复执行完整成员测试。ordinary 与 anchored 共用同一算子；bitmap×bitmap 的普通相交也使用已知命中 bit 的直接下标。

该算子不改变状态、调度、上下界或返回集合，不使用 Hash，也没有数据集、`g`、层号、密度或时间开关。相对“扫描全部 branch 后逐点测试 accumulator”，word 半连接把候选枚举从 branch 集合收缩为真实交集；最坏渐进阶仍与 Test98 相同。

## 2. 正确性

Release/O2 黑盒 DPBF 对拍全部一致：

| 范围 | seed | 结果 |
| --- | ---: | ---: |
| `n=4..18, g=2..13` | `716201` | `100/100` |
| `n=13..18, g=13` | `716202` | `50/50` |

DBLP g13 q5 两次均返回 `14.5867185184`，D/A values、pops、join calls 与 Test98 完全一致。

## 3. Fast20 与 Toronto

五库 fast20 的两次独立串行结果均为正；Test98 对照为 `result_snapshot/fast/20260715_034137`。

| 指标 | Test98 | Test102 run 1 | Test102 run 2 |
| --- | ---: | ---: | ---: |
| query wall sum | `7.417830s` | `6.973272s` | `7.239309s` |
| solver total | `7.333835s` | `6.914071s` | `7.152371s` |
| ordinary | `2.309556s` | `1.974225s` | `2.037081s` |
| anchored | `2.269729s` | `2.066472s` | `2.087523s` |
| completion | `0.821343s` | `0.799928s` | `0.809238s` |
| ordinary direct work | `43,752,193` | `30,318,002` | `30,318,002` |
| anchored direct work | `39,920,373` | `27,103,726` | `27,103,726` |

direct work 分别稳定减少 `30.7%/32.1%`，而 D/A states 不变。Toronto g13 串行三次为 `6.960359s/7.043883s/7.035524s`，中位数 `7.035524s`，略快于 Test98 的 `7.101916s`；peak 仍约 `57.5MiB`。另有三次误并发运行，只用于确认答案，计时因共享资源干扰而作废，未进入表格。

## 4. DBLP g13 q5 失败门

| 指标 | Test98 | Test102 run 1 | Test102 run 2 |
| --- | ---: | ---: | ---: |
| query wall | `158.149758s` | `162.064230s` | `162.291284s` |
| solver total | `157.636263s` | `161.538403s` | `161.781719s` |
| ordinary | `81.353796s` | `84.043759s` | `84.565424s` |
| anchored | `29.702392s` | `31.388541s` | `31.035650s` |
| completion | `2.809005s` | `2.933165s` | `2.928641s` |
| peak RSS | `2130.301MiB` | `2130.676MiB` | `2131.301MiB` |

q5 的 ordinary/anchored direct work 在三版中分别完全相同：`65,291,245/50,247,939`。换言之，新半连接没有减少 q5 的记录工作；两次 wall 仍稳定回退约 `2.5%--2.6%`。可能原因是大型函数代码布局或未受益分支的微小累计开销，但无论原因是什么，都不能用 fast 正结果替代端到端门，也不能按 bitmap 数量或数据集增加启用开关。

## 5. 结论

Test102 证明“物理 row 布局不应强制逻辑 join 计划”是有效方向，并在 fast20 上把两类 direct work 降低约三成；但 DBLP q5 两次端到端失败，因此该内联实现已完全撤回，未重跑 q25/q1。后续 Test103 只保留 branch 半连接并移入独立 `noinline` 内核，q5 两次恢复为 `152.286s/151.027s`，支持“Test102 的失败主要来自未受益热路径的代码生成扰动，而不是半连接集合本身”这一判断；当前实现与完整证据见 `../test103_out_of_line_bitmap_semijoin.md`。
