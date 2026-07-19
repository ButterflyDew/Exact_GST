# Test90：在顶层 D 中流式执行 A0 Completion

更新时间：2026-07-15。Test90 已合入 Test80 研究分支，建立在 Test83、Test84 与 Test87 之上。它不改变 ordinary D、anchored A 或 completion 的状态语义，只把原本在全部 D rows 结束后统一执行的 `A(empty)+D+D` 扫描，移动到每张最高层 D row 刚完成的时刻。ReleaseV4 保持冻结。

## 1. 核心观察

固定 permanent anchor 后有 `k=g-1` 个 nonanchor groups，ordinary rows 只生成到 `h=floor(g/2)`。A0 completion 把全部 nonanchor groups 分成两侧 `L,R`，并要求 `|L|,|R|<=h`：

```text
gd_anchor(v) + D(L,v) + D(R,v).
```

这个 partition 的大小不是任意的：

- 若 `g=2h+1`，则 `k=2h`，只能有 `|L|=|R|=h`；
- 若 `g=2h`，则 `k=2h-1`，两侧大小只能是 `h` 与 `h-1`。

因此，**每个 A0 completion partition 都包含一张最高层 `D_h` row。** 当该 `D_h(S)` 完成时，另一侧要么属于已经完成的 `D_(h-1)` 层，要么是同层补集 `D_h(N-S)`；后一种情况只在两张补集 row 都就绪时处理。无需等待整个 D_h 层结束。

## 2. 主线流程

Test90 保持原来的层序和整数 `mask` 递增顺序。每张 `D_h(S)` 保存后执行：

1. 令 `T=N-S`。偶数 `g` 时 `|T|=h-1`，它已经就绪；奇数 `g` 时仅在 `T<S` 时处理，表示补集 pair 的第二张 row 刚就绪。
2. 用现有 `ForEachTripleValue` 对 `D(S)`、`D(T)` 与隐式 anchor group-distance row 做按顶点编号的离线有序相交。
3. 每个共同根产生一棵真实可行树，用其代价立即降低 `best`。
4. 后续 D_h rows 继续使用更强的 `best` 做原有合法下界剪枝。
5. 顶层 D 完成后，所有 A0 partitions 已恰好处理一次；进入 A 阶段时不再重复原 `CompleteRoots(0, all_vertices)`，然后照常构造 A1 及以上状态。

没有 Hash、临时状态族、图压缩或查询压缩。`g<4` 时没有非 singleton 顶层 row，继续走原 A0 completion。

## 3. 正确性

对于偶数 `g`，每个 partition 有唯一的 h-side，所以在该 row 完成时恰好访问一次。对于奇数 `g`，两个 sides 都是 h-row；条件 `T<S` 只选择数值顺序中后完成的一侧，因此每个无序补集 pair 也恰好访问一次。候选集合与原 A0 completion 完全相同。

每次更新都来自 `gd_anchor+D+D` 三棵真实树在共同根的并，所以 `best` 始终是可行上界。提前得到更小的上界只会让后续 D 状态在 `partial+admissible_lower>best` 时更早被拒绝，不会删除最优解；等于 `best` 的状态仍由严格大于条件保留。

相交时把两个 masks 按整数大小排序，浮点加法顺序与原 completion 的 `anchor+left+right` 一致。**结论：Test90 与 Test87 返回相同精确答案，但允许后半段 D_h 的 values、pops 和扫描量下降。**

Release/O2 验证为宽范围 seed `715401` 的 `200/200`，撤回 Test91 后又以 seed `715421` 完成 fixed g13 `50/50`，累计 `250/250` 均与 DPBF 在 `1e-6` 内一致。

## 4. 复杂度

原 A0 completion 枚举同一组补集 partitions，并做同样的有序 row 相交。Test90 只改变执行时刻，因此最坏时间复杂度和 partition 数不变；原 A0 扫描被删除，不会重复支付。若提前上界不改善，额外开销只有顶层 row 完成时的一次补集判断。

Test90 不保存新 row，只增加常数个计数器，渐进空间不变。由于后续 D_h rows 可能更早被剪枝，实际 row payload 和 queue 工作可以下降；最坏渐进空间仍与 Test87 相同。

## 5. Release/O2 实测

### 5.1 Fast20

Test87 基线快照为 `result_snapshot/fast/20260715_010736`，Test90 为 `20260715_015858`。20 条答案一致；只有 `3/20` 条在 D_h 内提前改善 `best`。

| 指标 | Test87 | Test90 | 变化 |
| --- | ---: | ---: | ---: |
| wall 总和 | `8.368112s` | `8.334301s` | `-0.40%` |
| solver time | `8.286833s` | `8.261585s` | `-0.30%` |
| D_h values | `251,257` | `246,741` | `-1.80%` |
| D_h pops | `393,760` | `387,557` | `-1.58%` |
| anchored values/pops | `1,267,139/1,970,841` | 相同 | `0` |
| streamed partitions/checks | `0/0` | `3,745/82,797` | 执行原 A0 候选 |
| streamed time | `0` | `4.466ms` | 很小 |

### 5.2 Toronto g13 q1

三次输出保存在 `result_snapshot/gates/20260715_test90_streamed_a0_toronto`。wall 为 `8.092504s/8.066321s/8.076165s`，中位 `8.076165s`；Test87 三次中位数为 `8.332020s`，下降约 `3.07%`。答案均为 `0.7048467020`。

流式 A0 在 D6 中把 `best` 从 `0.7876599892` 降到 `0.7709288737`。以 Test90 中位运行与 Test87 中位运行比较，D6 values 为 `203,785 -> 198,478`，pops 为 `282,701 -> 275,078`，D6 时间为 `1.270310s -> 1.183611s`。A values/pops 严格保持 `493,596/672,677`；流式相交约 `2.2ms`。

### 5.3 DBLP g13 q5

原始输出保存在 `result_snapshot/gates/20260715_test90_streamed_a0_dblp_q5`：

| 指标 | Test87 | Test90 | 变化 |
| --- | ---: | ---: | ---: |
| wall | `186.895733s` | `183.913915s` | `-1.60%` |
| ordinary D | `97.984235s` | `96.389534s` | `-1.63%` |
| D6 values | `79,107` | `51,127` | `-35.37%` |
| D6 pops | `99,802` | `61,889` | `-37.99%` |
| D6 time | `6.931118s` | `6.045126s` | `-12.78%` |
| anchored values/pops | `3,999,227/1,751,425` | 相同 | `0` |
| completion time | `2.440190s` | `2.140716s` | 原 A0 全图扫描被删除 |
| peak RSS | `2134.043MiB` | `2134.734MiB` | 基本相同 |
| 最优值 | `14.5867185184` | `14.5867185184` | 相同 |

q5 在 462 个 partitions 上只执行 `2,282` 次共同根检查、耗时 `0.678ms`，并以 4 次更新把顶层开始时的 `15.1140873783` 降到 `14.8708439399`。收益来自随后 D6 的合法状态减少，不是省略 completion 候选。

本轮没有运行五小时级 DBLP g13 q1。Test90 已有跨库 fast、Toronto 和 DBLP q5 的一致正证据，但尚不能据此推断 q25 等重询问的收益比例。

## 6. 论文与发行边界

Test90 没有新增外部论文引用。它直接利用 Test80 的平衡 `A+D+D` completion 大小关系和有序 row 表示，是框架 A 内部的依赖调度改进。后续 Test91 的补集成对 mask 重排因跨库退化已撤回；Test90 保留原整数 mask 顺序。ReleaseV4 暂不更新。
