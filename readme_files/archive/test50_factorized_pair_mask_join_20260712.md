# Test50：Factorized Pair-Mask Join

更新时间：2026-07-12。Test50 延续 Test49 的 arc-to-pair DAG，但不再把每个 gradient site 显式复制到 target masks。每个 closed accumulator 的 root/downhill arc 只保存一个 factor：

```text
(vertex, accumulator value, pair bitset)
```

target mask 枚举自身 pair submasks，读取对应 accumulator factors，并测试 pair bit。该表示保持 pair bits 与 consumer masks 因子化，不创建 `(target,vertex,value)` event。它通过扩大随机验证，但 fast 中大量 bit membership 失败，端到端仍退化，因此正式代码撤回。

## 1. 精确性

沿用 Test49 的 gradient 定理。对 target `T` 与 pair `R subset T`，令 accumulator `S=T-R`。factor 中若 `R` bit 为 1，则该 vertex 是 `closure(D(S)+D(R))` 或 `closure(A(S)+D(R))` 的必要 root/downhill source；bit 为 0 时该 pair forest 在该 arc 上没有 predecessor state。

ordinary recurrence 仍保留全部非-pair splits，包括 Test37 必需的 `3+3`；pair+pair 只选择一个确定方向避免重复。A recurrence 同理。该原型通过：

```text
seed        712991
iterations  500/500
n           8..16
g           6..12
```

## 2. 无参数购买

Test49 初版用 arc bytes 与所有 dense certificates 比较，误在稀疏 DBLP-new 启用。Test50 在 D2 完成后计算每张 pair certificate 的真实最小字节：

```text
sum_pair min(4(n+1), 8 * settled_pair)
```

只有 arc DAG 固定字节更小时才启用。fast20 中最终只启用 Toronto/Toronto-new 8 条；DBLP、DBLP-new 与 MovieLens 保持正式 recurrence。该选择只比较精确表示字节，没有数据集、密度比例、`g` 或 wall 特判。

## 3. Fast20

快照 `20260712_193347`：

```text
factorized Test50       14.080s
event-expanded Test49   14.941s
formal Test21           12.380s
```

聚合统计：

```text
edge scans          11,276,051
factor records       5,417,210
target bit probes  125,043,072
matched sites       27,076,750
ordinary wall            7.007s
anchored wall            3.979s
```

相比 Test49，factor records 避免了 29M target events 的高峰值，Toronto-new g12 peak 从 `109.7MiB` 降到 `63.4MiB`；但每个 target 对 factors 做 pair membership，约 `78%` probes 为未命中，时间仍比正式版慢 `13.7%`。

## 4. 接口否决

对一般 pair bitsets，当前接口只有两种直接消费方式：

1. 枚举 set bits 并按 target 展开，得到 Test49 的约 29M events 与高峰值；
2. 保持 factorized，由 targets 测试 bits，得到 Test50 的 125M probes。

两者都没有在 mask 维共享 min-plus 数值；仅换容器、排序、bit 扫描或事件去重不会改变这个乘积。下一候选必须直接计算一族 target masks 的 min-plus subset transform，或改变状态使中间 pair-extension targets 不再物化。Test50 不触发 full DBLP。

Test50 没有新增论文算法引用；本文不宣称系统文献检索后的原创性。
