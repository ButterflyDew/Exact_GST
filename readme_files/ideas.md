# 未实现但较有价值的想法

## 1. Root-batched 同根合并

当前同一层中大量状态反复扫描同一个 root 的 bucket。

想法：

```text
states_by_root[v] = 当前 size 层中 root 为 v 的所有 mask
```

对每个 root：

```text
一次扫描 root_masks_by_size[v]
批量服务多个 mask
```

适合 DBLP uniform，因为它的 `bucket_scan/up_subset` 爆炸。

风险：要保证同层内 h 的产生不影响尚未处理 mask 的目标筛选，可能需要先只批处理 live-dp，h 仍即时。

## 2. 同层 best-first 调度

仍按 `popcount(mask)` 分层，但层内不按 mask 编号顺序，而按：

```text
dp[mask][v] + far(v,U^mask)
```

或：

```text
dp[mask][v] + LB(v,U^mask)
```

优先处理更可能改善 `best` 的状态。若早更新 `best`，后续同层状态会被更多剪枝挡掉。

与 PrunedDP 不同：不跨层做全局 label-setting，仍保持半集合 DP 结构。

## 3. Root bucket 按 value 排序

live-dp 枚举中：

```text
cand = dp[mask][v] + dp[t][v]
```

若 root bucket 按 `dp[t][v]` 升序，则对固定 `(mask,v)`：

```text
dp[t][v] >= best - dp[mask][v]
```

后面可直接停止。

这是把 `cand < best` 从“枚举后过滤”变成“枚举前停止”。

风险：bucket 动态更新，排序维护成本高。可按层重建或只对 hot root 重建。

## 4. 更强初始上界

半集合 DP 对 `best` 很敏感。DBLP uniform 慢的一个原因是早期上界不够紧。

候选：

```text
top-K root-star roots 做 greedy
从每组代表点附近启动 greedy
基于 group_pair MST 构造可行树
```

只影响上界，不影响正确性。

## 5. DBLP 大图预处理局部化

DBLP 的 `n` 很大，`group_dist_ms` 是固定成本。

可研究：

```text
按 best 半径截断 group_dist Dijkstra
只计算可能进入 DP 的 root 区域
在同一 query 文件内复用重复组的 group_dist
```

风险：截断必须只用于剪枝或上界，不能破坏安全下界。

## 6. h 语义重构

当前缺省 `h=-1` 会放宽目标筛选。剪掉某些 dp 后，可能导致 h 证据缺失，Dijkstra 变宽。

可研究：

```text
h-only ghost: 被剪掉 dp 时，不写 dp/root_masks，但保留必要 h 信息
```

或更彻底：

```text
用 complement availability / cover 语义替代 h
```

风险：cover 查询容易变贵，必须按 root 缓存或批处理。

