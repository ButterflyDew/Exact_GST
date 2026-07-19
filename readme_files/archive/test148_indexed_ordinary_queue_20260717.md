# Test148：ordinary decrease-key 队列诊断（已撤回）

更新日期：2026-07-17。Test148 曾在 Test146 上临时把 ordinary D 的 `std::priority_queue` lazy duplicates 替换为数组位置索引的 binary heap。每个顶点在堆内最多保留一个当前条目，距离改善时原地 decrease-key，因此正式 pop 不再出现 stale entry。

该实现只改变图闭包的队列数据结构，不改变 D/A/H 状态语义。随机小图与 DPBF 的 Release/O2 对拍通过 `200/200`（seed `148001`）。Toronto 冻结五问的完整答案也逐项一致。

## 诊断结果

| 面板 | 原实现 | Test148 | 观察 |
| --- | ---: | ---: | --- |
| DBLP g15 q20 D2 | `678.464s/3319.988MiB` | `502.171s/3254.297MiB` | pops `450.539M -> 202.265M` |
| DBLP g15 q8 D3 | `716.484s/3588.332MiB` | `657.954s/3591.695MiB` | D3 closure `425.731s -> 387.482s` |
| LinkedMDB g15 q40 D3 | `465.448s/3670MiB` | `412.777s/3676.617MiB` | D3 closure `345.981s -> 306.060s` |
| DBpedia g15 q34 D2 | `294.608s/4522.043MiB` | `294.478s/4497.465MiB` | 基本中性 |
| Toronto 冻结五问完整 | `142.504s`（Test146） | `136.791s` | 五问答案精确，收益约 `4%` |
| MovieLens 冻结五问 D3 | `96.570s`（Test145） | `93.191s` | ordinary 只少约 `0.95s` |

DBLP q20 的大幅改善说明该极端询问存在显著 lazy-duplicate 常数成本；DBpedia 中性与 MovieLens 的小绝对收益说明这不是统一瓶颈。Test148 还会让按实际 queue work 购买的 packing/tree 调度自然改变触发边界，因此个别运行的状态数可能有极小变化，但所有候选仍由合法上界控制，答案保持精确。

## 撤回原因

1. decrease-key heap 是 baseline 同样可以采用的通用数据结构替换，不是 permanent-anchor A 独有或原创的优化。
2. `agent.md` 要求复杂度核对时按 Fibonacci-heap 理论界理解 `priority_queue`，具体实现继续使用 `priority_queue`；Test148 不应成为活动实现。
3. 当前研究目标是减少 method-specific ordinary states、merge seeds 或传播，而不是把通用工程收益包装成论文贡献。

因此 Test148 的源码分支、CMake 目标与 profiler 方法入口已经撤出。有效快照保留在 `result_snapshot/g15_profile/runs/20260717_test148_d2`、`20260717_test148_d3`、`20260717_test148_d2_paired` 与 `20260717_test148_full`，仅用于量化通用队列成本。本文没有引入论文引用。
