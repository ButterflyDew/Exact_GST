# Test99：Completion Root Epoch 探针（已撤回）

## 1. 目标与方法

Test80 的 `CompleteRoots` 会针对同一张 A row 枚举多个 `A+D+D` partitions。旧实现对每个 partition 都从头用双指针把有序驱动行与同一 `roots` 列表相交；若一张 A row 有 `P` 个 partitions、`k` 个 roots，根侧指针在最坏情况下会重复推进 `O(Pk)` 次。

Test99 复用已有 `heuristic_stamp[n+1]`，为每张 A row 分配一个负 epoch，将 `k` 个 roots 标记一次；每次访问驱动顶点时直接检查 epoch。它不增加数组、不使用 Hash，也不改变 partition、驱动行、状态、剪枝或答案。根成员判定的最坏复杂度由 `O(Pk)` 降为一次 `O(k)` 标记和每个驱动顶点 `O(1)` 检查。

## 2. 正确性

宽范围随机小图与 DPBF 对拍 `100/100`，seed `715901`；固定 g13 为 `50/50`，seed `715902`。所有答案误差不超过 `1e-6`。DBLP g13 q5 两次均得到 `14.5867185184`，D/A values、pops、completion scans 和 checks 与 Test98 完全一致。

## 3. Release/O2 结果

fast20 中，completion 从 Test98 的 `821.343ms` 降到 `538.619ms`，solver time 从 `7.333835s` 降到 `7.289658s`，外层 query sum 从 `7.418s` 降到 `7.366s`。Toronto g13 三次 query wall 为 `6.790s/7.245s/6.726s`，中位数 `6.790s`；completion 为 `1.398s/1.549s/1.410s`，低于 Test98 的 `1.857s`。

DBLP g13 q5 未通过端到端门：

| 指标 | Test98 | Test99 run 1 | Test99 run 2 |
| --- | ---: | ---: | ---: |
| query wall | `158.150s` | `161.587s` | `162.903s` |
| solver total | `157.636s` | `161.058s` | `162.387s` |
| ordinary | `81.354s` | `83.895s` | `85.298s` |
| anchored | `29.702s` | `30.739s` | `30.903s` |
| completion | `2.809s` | `2.243s` | `2.271s` |
| peak RSS | `2130.301MiB` | `2129.926MiB` | `2130.520MiB` |

Test99 稳定减少了 completion 局部时间，但两次 q5 端到端都比 Test98 慢。ordinary 算法没有被改动，其计时波动说明局部收益不足以越过大图执行噪声与代码布局影响；不能只挑 completion 或 Toronto 的正结果宣称整体改善。

## 4. 结论

eager root epoch 已从源码撤回，Test80 恢复 Test98。不能为了保留它按数据集、固定 `g`、层号或 wall time 设置开关。该探针保留一个有效理论信息：同一 A row 的重复 root merge 可以共享，但下一实现必须以无经验参数的 rent-or-buy 或直接 bitmap semijoin 控制构造成本，并重新通过 DBLP q5 端到端门。没有运行 DBLP q1 或 q25。
