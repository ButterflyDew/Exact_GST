# Test101：Branch Exception Layout（已撤回）

## 1. 方法与理论动机

普通 D row 的 branch 集合是有限值集合的子集。q25 旧统计中，D2--D6 的 `values-branches` 只占各层 values 的约 `0.8%--5.8%`，因此 Test101 尝试不再总是保存 branch bitset，而保存有序的 nonbranch exception vertices；枚举 branch 时扫描有限值并跳过例外。

令一行有 `k` 个有限值、`b` 个 branch、`e=k-b` 个例外，`N=n+1`、`W=ceil(N/64)`。三种普通布局联合比较：

```text
sparse = 12k + min(8ceil(k/64), 4e)
dense  = 8N  + min(8W, 4e)
bitmap = 8k + 12W + min(8W, 4e)
```

每行严格取最小逻辑字节数，相等时保留原 bitset；没有数据集、`g`、层号或经验密度阈值。exception list 只利用框架 A 的 branch 语义，不是图/query 压缩。

## 2. 正确性

bitset 表示 branch 集合 `B`，exception 表示 `V_finite-B`；在枚举或成员查询时取补集即可恢复同一个 `B`。Release/O2 宽范围随机对拍 `200/200`，seed `716101`；固定 g13 `50/50`，seed `716102`。全部与 DPBF 在 `1e-6` 内一致。

## 3. 快速门结果

fast20 中有 `3,878` 张普通 row 选择 exception，共保存 `107,881` 个例外。空间收益很小而时间明显退化：

| 指标 | Test98 | Test101 | 变化 |
| --- | ---: | ---: | ---: |
| query sum | `7.418s` | `8.197s` | `+10.5%` |
| solver total | `7.333835s` | `8.121270s` | `+10.7%` |
| ordinary | `2.309556s` | `2.503001s` | `+8.4%` |
| anchored | `2.269729s` | `2.521770s` | `+11.1%` |
| D row payload | `21.093MiB` | `20.871MiB` | `-1.1%` |

exception list 减少了 branch metadata，却把原来的 word 扫描和 O(1) bit test 改成“枚举有限值并跳过例外”，在大量重复消费者上增加了分支与有序例外访问。fast 空间只少约 `0.222MiB`，不足以补偿热路径成本。

## 4. q25 的免长跑边界

利用旧 q25 每层的 rows、values、branches 和 payload，可以把所有普通行代入 `8k+12W+4e`，再按层使用 `sum(min(old,candidate))<=min(sum(old),sum(candidate))`。由此得到 D row payload 上界约 `11,591.4MiB`；与 Test98 对 A rows 的既有上界合并，总持久 row payload 约不超过 `18,312.2MiB`，比 Test98 的 `18,784.0MiB` 再低约 `471.8MiB`。这只是聚合上界，不是 q25 peak RSS 实测。

## 5. 结论

Test101 的集合等价和 q25 空间界有效，但 fast20 时间门失败，因此实现已撤回并恢复 Test98。没有运行 Toronto full、DBLP q5/q1 或 q25。后续若重新研究 branch metadata，必须同时给出不会增加消费者扫描的共享接口；不能按数据集或大图规模启用 exception list。
