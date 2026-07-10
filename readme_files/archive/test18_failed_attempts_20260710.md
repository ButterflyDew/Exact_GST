# Test18：2026-07-10 失败和撤回路线归档

这份文档只归档撤回、失败或不能作为当前主线的路线。目的不是证明它们无价值，而是避免下一轮无意识恢复不合规触发条件。

## 1. 第六条相关撤回项

以下内容不得恢复到当前主线：

- 固定 rows 三分块同根上界：例如在 g13 k=3 rows `96/160/224/256` 触发。它能给出瓶颈信息，但固定进度阈值没有理论依据。
- frontier k=3/k=4 同根分区：row-ready 事件本身合理，但只对大小 3/4 启用仍是无统一判据的层级超参数；当前源码和统计字段均已撤出。
- `g==13 && |S|>=4` 这类组数和层级特判。
- “行没有达到 dense-light 阈值时另走轻量表示”这类 dense-light 成本模型之外的非 dense 高阶行特判。
- 任何依赖数据集名、查询编号、运行时刻、手工中止位置的启用逻辑。

保留的有效信息：早拿到强合法上界确实能降低后续 dense 洪峰；但启用条件必须来自完整理论判据或统一成本模型，不能只因为某个 row-ready 层在一次探针中有效。

## 2. 下界失败项

| 路线 | 结论 |
| --- | --- |
| capacity-aware future split | 不安全。随机对拍出现 DPBF `33`、实验版 `35` 的反例 |
| compact-only capacity LB | 即使只放在 compact，也会因为未来路径共享而错误删除状态 |
| far+near | `future_lb_probe` 中出现 admissibility 和 consistency violation |
| root+groups MST / three-point MST | 要么太弱，要么不能满足进入 relax 的一致性要求 |
| local pair dominance | 命中少，不能解释 g13 主要瓶颈 |
| actual-cover-aware root partition | 理论形式正确，但当前表示下命中窄，收益不足 |

规则：新下界先进入 `tools/future_lb_probe` 做 exact future oracle 筛查，再考虑进入 solver。

## 3. 上界失败或混合项

| 路线 | 结论 |
| --- | --- |
| k=3 layer-end 三分块 | 正确，但没有比旧主线带来新的 `best_after_k3` 或状态收益 |
| hub pair/single 上界 | 正确，但不降低关键 best，还增加额外扫描和 Dijkstra 成本 |
| k=4 quad 同根分区 | 正确，但 g13 早段没有继续降低关键 best |
| pair/single 候选根扩到所有终端 | 候选变多，收益不足 |
| frontier k=3/k=4 | 上界正确，但硬编码层级不合规且 wall 混合；已撤出，探针信息留档 |

## 4. 图/询问压缩边界

保留：当前 exact query reductions，包括标准 Steiner 度缩图和 Voronoi exact torso。它们必须是精确等价变换。

暂不继续：普通 mimicking network、local torso、baseline 也能同样使用的图/询问压缩。除非能证明是 Test18 half-DP 特有，或形成原创理论贡献，否则不作为当前优先路线。

## 5. A* / DS* 状态

A* ordering 当前是可选实验路径。它有 consistent `current_lb` 的 probe 支撑，但 snapshot wall 结果混合：

- DBLP g9 和一个 DBLP g12 snapshot 上减少 `pq_pop` 且 wall 有收益。
- 另一个 DBLP g12 基本持平。
- Toronto g12 变慢。

因此它适合继续短探针，不适合作为默认主线，也不触发 full DBLP g13 长跑。

## 6. 旧证据位置

完整旧长文和实时日志在：

```text
readme_files/archive/test18_algorithm_full_legacy_20260710.md
readme_files/archive/test18_effect_report_full_legacy_20260710.md
readme_files/archive/test18_research_log_full_legacy_20260710.md
```

这些文件只用于查证历史输出，不作为当前读者入口。
