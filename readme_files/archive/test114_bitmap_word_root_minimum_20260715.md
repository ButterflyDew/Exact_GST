# Test114：Bitmap Completion 的整字根最小值证书

更新时间：2026-07-15。该探针由 Test107 的 DBLP g13 q25 长门触发：三级证书把最终 checks 从 `32.815B` 降到 `46.019M`，但仍有 `32.598B` physical scans，其中 `30.208B` 个候选最终由根级下界拒绝。Test114 尝试把拒绝时机提前到现有 64-bit bitmap word；实现正确但 fast20 没有局部时间收益，源码已恢复为 Test107，未再次运行 q25。

## 1. 方法与正确性

对当前 A row 的每个 64-bit 根块 `W`，在构造 root bitmap 时同时保存

`a_min(W) = min { A(v) | v in W and v is an A root }`。

对于 completion partition `(L,R)`，若 `a_min(W) + min D(L) + min D(R) > best`，则该 word 内任一共同根 `v` 都满足 `A(v) + D(L,v) + D(R,v) > best`，可在位交结果展开前整字跳过。`64` 来自已有 bitmap 的机器字宽，不是经验阈值；机制不改变 partition、状态语义或最优值。

## 2. 短门结果

Release/O2 黑盒对拍通过宽范围 seed `717001` 的 `100/100` 和固定 g13 seed `717002` 的 `50/50`。fast20 对照为 Test107 `20260715_095356`，Test114 连续三次为 `20260715_152352/152454/152528`。

| 指标 | Test107 | Test114 三次 |
| --- | ---: | ---: |
| completion scans | `13,111,383` | `12,285,877`，稳定 `-6.30%` |
| completion checks | `2,271,708` | `2,271,708`，相同 |
| completion time | `219.400ms` | `218.161/224.509/219.604ms` |
| solver wall | `6466.501ms` | `6685.625/7012.926/6729.550ms` |

新版本 completion 中位数为 `219.604ms`，与单次 Test107 控制的 `219.400ms` 无可辨别差异；额外的 word-min 数组初始化和逐 word 读取抵消了少展开的候选。q5、q32 与 Toronto g13 的当前 completion bitmap calls 都是 0，无法提供进一步短门证据。

## 3. 结论

Test114 的不等式正确，也确实批量删除了候选访问，但现有短门只证明 `6.30%` scans 改善，不能证明时间收益。唯一可能放大收益的现存样本是刚完成的两小时 q25；在没有更强结构信号时立即重跑违反长门纪律，因此不能凭预测保留机制。当前源码撤回到 Test107。若将来重访，必须先设计能离线估计“整字全部被根界拒绝”的覆盖率，或形成同时适用于有序 fallback 的区间证书，而不是再次直接启动 q25。
