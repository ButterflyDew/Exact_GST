# Test100：Bitmap-Branch Semijoin（已撤回）

## 1. 方法

Test98 的 bitmap row 同时有有限顶点 occupancy 和普通 D branch bits。Test100 在 accumulator 为 bitmap、branch row 为 dense/bitmap 时，按 word 计算 `occupancy AND branch_bits`，只对交集顶点读取两侧距离；旧实现会枚举 branch bits 的全部置位，再逐点检查 accumulator membership。新算子不增加空间、不使用 Hash，也不改变状态或连接结果。

局部工作由扫描 `W` 个 branch words、访问 `b` 个 branch 顶点并做 `b` 次 membership，变为扫描 `W` 个 word 和访问 `c<=b` 个交集顶点。该变化只作用于物理连接接口，不改变 solver 的渐进上界。

## 2. 验证

Release/O2 宽范围随机小图与 DPBF 对拍 `100/100`，seed `716001`，误差不超过 `1e-6`。fast20 两次结果如下：

| 指标 | Test98 | Test100 run 1 | Test100 run 2 |
| --- | ---: | ---: | ---: |
| query sum | `7.418s` | `7.395s` | `7.425s` |
| solver total | `7.333835s` | `7.311056s` | `7.354673s` |
| ordinary | `2.309556s` | `2.083599s` | `2.111049s` |
| anchored | `2.269729s` | `2.209425s` | `2.248173s` |

ordinary 局部时间下降，但端到端两次的中位数与 Test98 基本相同。数据集方向也不稳定：Test98 fast 中 Toronto/Toronto-new 分别有 `838/1522` 张 D bitmap rows，因而出现明显局部收益；DBLP、DBLP-new、MovieLens 分别只有 `0/35/0` 张，未得到稳定改善。按 bitmap 出现与否解释结果是诊断，不允许转化为数据集开关。

## 3. 结论

Test100 只是已知位图布局上的局部常数优化，没有形成跨结构端到端收益，也没有改变主要状态或理论复杂度。源码已恢复 Test98；未运行 Toronto full、DBLP q5/q1 或 q25。后续不应继续围绕同一 word loop 微调，研究应回到能减少状态、候选或 completion partitions 的结构机制。
