# Test18：合规主线快照

这份文档只写当前源码和继续工作的入口。旧长文和失败路线统一放在 `readme_files/archive/`。

## 1. 阅读顺序

```text
readme_files/test18_algorithm.md                  Test18 合规快照和安全边界
readme_files/test18_effect_report.md              Test18 可引用效果和 O2 后计时
readme_files/archive/test18_failed_attempts_20260710.md  撤回/失败路线归档
readme_files/test19_algorithm.md                  后续实验入口，Test18 的独立副本
readme_files/test19_maintenance.md                Test19 代码维护和拆分计划
```

## 2. 当前一句话

Test18 仍是 exact half-DP：只保存小 mask 的 rooted row，用安全下界剪掉不可能改进 `best` 的状态，并只用真实可行完整树更新 `best`。它现在作为可复现的合规快照维护，不继续承接新探针。当前代码不含固定 rows、固定组数/层级、数据集名、时间点或非 dense 高阶行特判。

## 3. 代码入口

```text
methods/Test/test18.cpp     主要实现，保持为历史快照，不继续加新实验
methods/Test/test18.h       参数、统计和公开入口
main.cpp                    Test18 输出字段
tools/structure_probe       图结构诊断
tools/future_lb_probe       用 exact future oracle 检查候选下界
```

常用开关：

```text
GST_TEST18_PROGRESS=1       打印长跑进度
GST_TEST18_ASTAR_ORDER=1    可选 A* ordering 探针，默认关闭
```

## 4. 保留机制

| 机制 | 作用 | 安全性边界 |
| --- | --- | --- |
| exact query reductions | 标准 Steiner 度缩图和 Voronoi exact torso，当前覆盖 `portal<=4` | 必须是精确等价变换；普通图/询问压缩不进入主线 |
| virtual singleton / group distance | singleton 不持久化 row，按 `gd[a][v]` 查询 | `gd` 是精确最短路 |
| full-root lower-bound corridor | 用当前安全 `LowerBound(v,R)` 做保存/compact 剪枝 | 只能剪 `dp + LB >= best` 的状态 |
| stale need compact | best 下降后清理已不可能参与更优完整解的状态 | 不修改 remaining state 的 DP 值 |
| light / dense-light row | pair 行和成本判定足够稠密的行少存常数项 | dense-light 只由表示成本触发，非 dense 高阶行保持普通 sparse |
| DP-order save prune | 在正确层序下用最大层保存必要条件减少存储 | 不使用未来未完成 row 作为证据 |
| future split save LB | 用已证明安全的未来拆分下界加强保存判断 | 不能恢复 capacity-aware 之类反例下界 |
| cover-aware Complete | 只在 `cover_rem==rem` 时直接构造完整上界 | 只是上界更新，不作为剪枝下界 |
| complement Complete row cache | 对固定补侧懒物化 `complete_row[v]` | 只重排同一批已保存 half-DP row 的查询 |
| pair/single root partition | pair 层后构造同根完整上界 | 每个分块都是合法 rooted row 或 singleton |
| A* ordering | 用一致 `LowerBound` 改变单层图搜索堆键 | 默认关闭，结果混合，不作为主线加速结论 |

## 5. 不要恢复的边界

- 不恢复固定 rows 三分块、固定组数/层级、dense-light 阈值外触发、数据集名或时间点特判。
- 不恢复非 dense 高阶行轻量表示。当前只有 pair light 和成本判定 dense-light。
- 不把 capacity-aware split、far+near 负例、root+groups MST、local pair dominance 等失败下界塞回剪枝。
- 不做 baseline 也能使用的普通图/询问压缩，除非能证明是 Test18 特有或原创。
- 不因为 A* ordering 的混合短测结果单独触发 full DBLP g13 query 1。

## 6. 验证状态

当前必须继续维持这些验证口径：

- 随机小图对拍 DPBF。近期关键种子包括 `303033`、`404041`、`555661`、`555662`、`707071`、`717273`。
- Toronto / DBLP snapshot 的 query 1 用已有结果和最后一次 append 输出比较。
- CMake 已显式保证 Release / RelWithDebInfo 使用 O2；关键计时以 `test18_effect_report.md` 的 O2 后表为准。
- 运行后清理 `.tmp_random_compare*`、`result_tmp*` 和只含 header 的无效结果目录。

## 7. 下一步

Test19 副本已经建立，后续 DS* / A* / one-tree LB 等新实验优先进入 Test19。Test18 只接受修正文档、修复构建或修复确定 bug 这类维护性变更。硬编码只处理 k=3/k=4 的 frontier 同根分区已经从当前源码撤出。

full DBLP g13 query 1 只在出现突破性理论机制、重要结构输出或强短探针证据后再跑。
