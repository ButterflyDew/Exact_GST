# ReleaseV6 当前研究状态

更新时间：2026-07-18。本文只维护当前方法、已通过证据、投稿缺口和后续纪律。ReleaseV6 之前的完整状态页已原样移至 `history/research_status_pre_release_v6_20260718.md`；实验时间线见 `test_series_overview.md`，撤回路径见 `archive/README.md`。

## 1. 当前结论

ReleaseV6 是当前发行版。它由 Test152/Test157 的冻结 M5 独立清理得到，包含：

1. 永久可选组锚点下的普通 `D` 与锚定 `A` 状态组织；
2. 只正向计算低层 `A`，以 closure-aware `F/H` 反向计算高层锚定格；
3. 按根转置生成高层补集终端；
4. ordinary 实际工作量驱动的 progressive dual；
5. 三块、junction facility、anchor-tree facility、tour/cut lower bound 和 residual packing 等合法界增强。

ReleaseV6 不调用 Test、ReleaseV5 或其他旧 Release，不含研究开关、调试输出、quarter upper、数据集名、固定 `g`、query、density、wall-time 或观测进度特判。方法与实现分别见 `release_v6_method_cn.md` 和 `release_v6.md`。

## 2. 方法评审

当前采用**完整算法贡献**尺度，不要求每个局部操作都是首次提出。逐组件排重已经确认以下构件有先例：

- Iwata--Shigemura 的平衡三分解与半状态 MITM；
- Goodman、Li--Eisner、Gildea 的一般 inside/outside；
- subset packing/convolution 的不相交事件；
- Dijkstra-Steiner/DS* 的 rooted future cost；
- Wong 路线的 directed-cut dual 与 reduced costs；
- DPBF、PrunedDP++、GPU4GST 的 rooted grow/merge 基础。

ReleaseV6 的论文方法贡献落在这些构件形成的完整 exact GST 求值组织：**永久组锚定定向平衡分解，ordinary `D` 和低层 `A` 在 inside 方向求值，高层锚定格在图闭包外侧以 `F/H` 求值，再以按根补集终端连接两侧。** 最近的完整算法没有采用这套联合执行结构；从通用原语到可运行算法还需要组平衡分解、`F/H` 闭包语义、第一条跨切分边和终端双射证明。

逐组件排重见 `test152_novelty_audit.md`，整算法评审见 `test152_integrated_method_review.md`，总门槛见 `release_v6_review_gate.md`。允许主张新的 exact GST 求值组织；不允许主张首次发明 outside、首次半状态、首次固定 terminal、改善最坏 `3^g` 指数底数或稳定降低实际空间。

## 3. 已通过证据

### 3.1 正确性与复杂度

- Test157 独立核验 group-balanced decomposition、strict branch、显式高层 `A` 对 `F/H`、每个跨切分边和 transposed terminal。
- 生产路径累计通过 1000 个 DPBF 随机实例、三类 row validator、3000 个 packing 与 2000 个 progressive dual 随机实例。
- ReleaseV6 新增通过 seed `160001` 的 DPBF 随机小图 `500/500`。
- ReleaseV6 与冻结 M5 在 Twitch `g=10` q1--q20、Musae `g=13` q1--q5 共 `25/25` 权重一致。
- `test152_full_complexity_audit.md` 已逐项求和 core 与全部辅助模块；无隐藏超过 `O(3^g)` 的 mask 工作。

### 3.2 方法效果

| 对照 | 冻结结果 | 结论 |
| --- | --- | --- |
| 公平 M0 -> 永久锚定 M1 | Twitch/Github `g=12` 两轮合计 `3.187x/2.615x` | 锚定状态组织具有强独立效果；低 `g` 收益可较小 |
| 完整高层 `A` M1 -> `F/H+transpose` M3 | Twitch/Github `g=12` 两轮合计 `1.098x/1.202x` | 联合机制通过时间效果门 |
| M1 -> 仅 `F/H` M2 | 跨库方向不稳 | 不单独宣传 adjoint 加速 |
| strict `Br` -> all branches | 两轮 240 对 `1.018x` | 只作规范化辅助，不是论文核心 |
| M4 eager dual -> M5 progressive dual | 三库 `g=4..10` 共 420 条：`212.569s -> 207.016s` | 冻结 M5 为单一路线，不按 `g` 回退 |

冻结 M5 在 Musae/Twitch/Github `g=11..16` 选定 180 条中完成 176 条；唯一真实超时为 Github g16 q2，q3--q5 未启动。该面板证明可运行性，不把未完成基线或完成子集包装成无删失平均。

### 3.3 发行抽取

Release/O2 构建成功。Twitch `g=10` q20 中 M5/V6 为 `28.475907/27.924329s`，Musae `g=13` q5 为 `12.956268/11.903326s`；权重全部一致。RSS 差异很小且方向混合，所以只用于证明抽取无结构性回退，不作为新算法效果或稳定空间收益。

## 4. 尚缺的投稿实验

1. Toronto、DBLP、DBpedia、LinkedMDB、MovieLens 的冻结多询问面板；不能用 q1 代表一个 `<dataset,g>`。
2. 合适 Linux/CUDA 机器上的 GPU4GST 作者代码同机对照。当前 2 GiB GT 730 不能替代作者面向 A6000 的平台。
3. 最终八库实验的完整分布、完成率、timeout 删失、query peak RSS 与复现 manifest。
4. 论文正文中的相关工作与 claim 必须沿当前逐组件／整算法两层边界书写。

这些是实验章节缺口，不再触发无休止的方法改写。全量实验可以在 ReleaseV6 方法冻结后补齐。

## 5. 后续研究纪律

1. ReleaseV6 保持冻结。新机制先在新的 Test 编号中实现、证明和消融，不能直接修改发行版。
2. 若评审暴露正确性、复杂度、工程特判或真实性能问题，先在 Test152/Test157 脉络解决，再重新冻结。
3. 不按数据集、固定 `g`、query id、层号、density、wall time 或观测进度切换算法。
4. 不采用 baseline 同样可做的普通图／查询压缩作为本方法贡献，除非给出本方法特有或原创的接口。
5. q1 只作 smoke 和历史兼容，不作总体性能证据；多询问结果至少报告总时间、分位数、方差或变异系数和逐询问分布。
6. 除非出现方法结构突破或正式发行候选，不运行约五小时的全 DBLP g13；ReleaseV6 已冻结，也不为补一个漂亮单点启动该长跑。
7. 所有时间使用 Release/O2；所有结果读取最后一次 run；临时目录与残留 solver 进程在验证后清理。

## 6. 当前文档入口

- `release_v6_method_cn.md`：论文方法章节式完整说明。
- `release_v6.md`：发行源码、构建、验证与复现。
- `release_v6_review_gate.md`：方法、实现和投稿实验门。
- `test152_integrated_method_review.md`：完整算法贡献评审。
- `test152_novelty_audit.md`：逐组件和逐定理排重。
- `test152_core_correctness.md`：源码对齐正确性证明。
- `test152_full_complexity_audit.md`：实现级复杂度求和。
- `test157_same_source_ablation.md`：M0--M5 同源消融。
- `test157_frozen_panel.md`：冻结查询与高 `g` 可运行性。
- `test158_root_irreducible_branch_ablation.md`：strict branch 单变量结果。
- `gpu4gst_datasets.md`：八数据集转换与查询生成协议。
- `pruneddp_reproduction.md`：PrunedDP++ 三项复现歧义与开关。
