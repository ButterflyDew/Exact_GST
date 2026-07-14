# GST Framework 文档入口

本文只负责导航和事实优先级，不重复算法证明或实验表。

## 1. 从这里开始

| 需要了解 | 唯一入口 |
| --- | --- |
| 构建、CLI、输出格式 | `../RUN.md` |
| 当前纯 A ReleaseV4 方法章节 | `release_v4_method_cn.md` |
| ReleaseV4 发行实现与性能 | `release_v4.md` |
| ReleaseV3 框架 B 冻结对照 | `release_v3.md` |
| Test80 研究选择与阶段数据 | `test80_anchor_progressive.md` |
| Test80 DBLP g13 跨询问剖析 | `test80_dblp_g13_cross_query.md` |
| 当前 anchor-aware half 研究 | `test21_anchor_half.md` |
| Test21 高 D generator 否决边界 | `test21_high_row_generators.md` |
| A 状态 paid-attachment 规模边界 | `history/paid_attachment_representative_family.md` |
| 显式 D2 输出门槛 | `history/explicit_d2_output_barrier.md` |
| 当前未解决问题与实验门槛 | `research_status.md` |
| snapshot benchmark | `snapshot_benchmark.md` |

## 2. 发行版

| version | 定位 | 状态 |
| --- | --- | --- |
| ReleaseV4 | permanent-anchor ordered D/A rows | 当前纯 A 发行入口；由 Test80 清理得到，不调用 B |
| ReleaseV3 | ordered half rows -> farthest-goal global | 冻结的框架 B 发行对照；full DBLP g13 已完成 |
| ReleaseV2 | fixed-anchor global labels | 历史大 `g` 发行版；保留独立源码与 full 证据 |
| ReleaseV1 | half-DP | 历史 small `g` 发行版；保留独立源码与结果 |

ReleaseV1--V4 都有独立源码，不通过 Test 开关启用。ReleaseV4 复用公共
junction/dual helper，但不调用 Test80 或其他 Release；Test80 继续保留研究统计。

## 3. 文档分层

```text
readme_files/
  release_v4.md                 当前纯 A 发行算法、实现与结果
  release_v3.md                 冻结的框架 B 发行对照
  release_v3_implementation.md  ReleaseV3 源码导读
  test80_anchor_progressive.md  ReleaseV4 来源与阶段统计
  test80_dblp_g13_cross_query.md Test80 跨询问瓶颈与后续依据
  research_status.md            当前开放问题
  release_v1.md / release_v2.md 历史发行版
  history/                      已冻结但仍有效的研究与证明
  archive/                      失败尝试、撤回路径、原始长日志
```

事实优先级：ReleaseV4 文档 > 当前 Test/Release 文档 > `history/` > `archive/`。
旧文档中的“当前”“未完成”只描述对应冻结 solver，不得覆盖 ReleaseV4 的当前事实。

## 4. 当前结论

- 当前 ReleaseV4 是纯 A：small35 `2.148s`，fast20 `8.303s`，Toronto full
  `7.857s / 58.0MiB / 0.7048467020`，DBLP full
  `544.379s / 2158.6MiB / 12.5936282853`；每条 snapshot 权重均与 Test80 一致。
- Test80 的 full DBLP g13 q1 为 `666.509s / 2161.9MiB / 12.5936282853`；它是
  ReleaseV4 的算法来源和阶段统计对照，发行清理使 full wall 降低 `18.3%`。
- Test80 又按截止时间完成 19 条新 DBLP g13 询问；wall/peak 中位数为
  `2042.8s/5874.1MiB`，D3/D4 主导空间，A5 与完整化主导后段时间。q1 相对容易，
  后续研究依据见 `test80_dblp_g13_cross_query.md`。
- ReleaseV3 的 full DBLP g13 为 `531.556s / 3870.7MiB`，继续作为框架 B 时间对照。
- ReleaseV4 不使用数据集名、固定 `g`、row density 或运行时刻特判，也不使用
  baseline 可共享的普通图/query 压缩；D2 packing 购买由可证明的工作量比较决定。
- 当前 Test21 研究版为 anchor 主干 + branch basis + 在线分块上界；fast20 `12.380s`、Toronto full `20.788s / 102.9MiB`，但 DBLP g13 仍未跨过 V3 时间门槛。
- anchor-group super-root dual 的 objective 合法但 future 过松；Test35 fast20 `27.792s`，已撤回。一般显式 paid-attachment family 存在 `2^r` 反链，下一机制必须提供隐式共享或新的受控边界。
- D2 多 pair 向量波虽减少 accepted profiles，却因异构 priority 在 fast 慢 `3.8x--45x`；固定 singleton/pair branch recurrence 又有 rooted 反例。两条入口均已清理，证据见 archive Test36--37。
- local seed-cone dominance 在 fast/Toronto full 有 `1.6%/3.4%` 正收益，但不减少 DBLP D2 payload，bounded gate 仍未完成；作为辅助定理归档，不进入当前 Test21。
- one-third backbone states 精确且不增加 mask 数，但只减少 full Toronto 高层约 `4%`、不动 D2；Test39 已撤回，不触发 DBLP。
- A1 consumer-driven D2 在 full DBLP 三对上仍 settle 完整 row 的 `99.7%--99.9%`，Test40 已撤回；exact-best 样本表明下一杠杆是 D2 前的 anchor-aware incumbent。
- dual zero-residual half/三块 upper 很轻且 fast 两库有更新，但 full DBLP 仍停在 `17.360814`；Test41 已撤回。
- work-triggered ordinary greedy 把 Test42 fast20 降到 `12.182s`，但 full 在 `558.7 CPU-s` 仍未完成；不满足大实例门槛，已撤回。
- singleton-only anchor C rows 在 fast 两库改善 upper，但三块无更新且 full C1 在 `234.8 CPU-s` 仍未完成；Test43 已撤回。
- paid anchor backbone 把 fast20 最低降到 `10.112s`、full upper 降到 `13.1619`、peak 约减半，但 full 仍在 `543--555 CPU-s` 无最终结果；Test44--47 因 V3 硬门槛撤回。
- branch-junction closure 只扫描 triple argmin roots，再在其到 anchor backbone 的压缩父树上做 subset DP，使公共路径前缀只付费一次。Test48 fast20 为 `9.758s`、D2 values/pops 降到 `0.50x/0.45x`、full upper 为 `13.01999`，但有效 full gate 在 `535.507s query / 2087MiB peak` 仍无最终权重；已撤回。
- pair gradient 定理把 full exact-best 下单个 anchor consumer 的 D2 sites 压到 `8.416%`，并可用 `449MB` arc-to-pair bit DAG 精确枚举；但逐 consumer/target 展开后 fast20 为 `16.834/13.245/14.941s`，均退化。Test49 已撤回，下一步必须同时共享 pair bits 与 consumer masks。
- factorized arc records 避免 target events，但 Test50 fast20 仍为 `14.080s`：`5.42M` factors 触发 `125.0M` membership probes、仅 `27.1M` 命中。event 展开与 target 探测两种接口均已否决。
- Test51 同时保留 triple 在接入路径“首个使用者付费”和“共享前缀已付费”两种边际状态下的精确 junction。fast Toronto/DBLP-new upper 有改善，但 full DBLP 把候选从 24 扩到 84、压缩树从 20 扩到 86 后仍严格等于 Test48 的 `13.019988893`；同一 parent tree 上继续扩 facility roots 已否决。
- Test52--53 让 Test48 primal skeleton 反向决定 dual：attachment-centroid 与 root-star 两个 potentials 在 full 各自约 42%/47% positions 更强，max 使 D2 settled/pushes 降 `9.6%/11.1%`；但 replay wall `40.31s -> 43.62s`，还需第二次 dual `39.12s`。按 attachment 深度重排一次 dual 的 fast20 又退到 `11.565s`，全线撤回。
- Test54 在一次 residual construction 中把每个 group moat 增长到覆盖整条 paid anchor path。Toronto-new D2 降到 `0.51x/0.46x`，但 DBLP/DBLP-new values 膨胀到 `18.38x/4.40x`，fast20 `15.347s`；sequential cover-all charge 的跨库方向翻转已否决。
- Test55 改用 root-preserving path-average cap，fast20 回升到 `13.198s`，但 DBLP/DBLP-new D2 values 仍为 `16.31x/2.48x`。Test54--55 共同否决 sequential ascent 中所有“每组一个 paid-path scalar cap”的继续微调。
- Test56--58 的 order-independent residual packing、D2-work 购买与 Test48 junction 后来按纯 A 边界重建为 Test80。历史 gate 在 `531.671s` 无 weight；当前不设 B 门禁的 full 于 `666.509s / 2161.9MiB` 完成，二者口径不能混写。
- Test59 证明每个可拆 `D(S,v)` 都有大小至多 `|S|/2` 的根不可拆分分支，并据此把 split 定向到较小侧。随机 `500/500` 精确，但 fast20 `13.016s`，ordinary/anchored/completion payload 与正式版均在 `0.02%` 内；只改 split orientation 不改变状态族，已撤回。
- Test60 用 `min C(f)+g=min f+C(g)` 把最高 `D_h` closure 转到已闭包 anchor side。`k=2h-1` 的 fast g12 改善 `3.8%`，但 DBLP g13 同类的 `k=2h` 必须补 A-half generators；Toronto full 虽让 D6 values/pops 减约 `45%`，wall 仍从 `20.788s` 退到 `25.121s`，已撤回且不触发 DBLP。
- Test61 再用 `min C(Q_N)+D_C=min Q_N+D_C` 消掉非 canonical A-half rows，以 closed-D roots 驱动三函数 scalar completion。Toronto 相对 Test60 回收 `1.28s`，但 canonical A-half 仍增加约 `16.9M` merges，最终 `23.838s/105.1MiB`，继续慢于正式版；已撤回。
- Test62 用 target A* 直接计算最后的 `min C(Q_C)+M_N`，彻底删除 A-half rows。Toronto 仅需 `22.5k` target pops，但 top generator 与 `1.05M` point probes 仍使 wall 为 `23.786s`；Test60--62 三种 top closure 接口全部否决，研究重心返回低层状态族。
- Test63 检查 `gd_i+gd_j` 是否已为 1-Lipschitz，从而整张删除无需 closure 的 D2。五库 fast g12 共 `275/275` pairs 全部失败，每对至少有 `2,528` 条严格 violation edges；该 exact certificate 零命中，probe 已撤回。
- Test64 的 pair-work anchor 在 full 结构上准确选中最少 D2 的组，但 fast 的 DBLP/DBLP-new/MovieLens 分别退化约 `26.0%/9.1%/13.2%`；单独换 anchor 只是重分配状态族，已撤回。
- Test65 的 recursive `D0+B` 状态排序由 Pascal 恒等式保持精确，随机 `500/500`，但 fast20 为 `13.755s`；双-anchor四块 upper 修正后仍为 `13.617s`。没有共享几何时，递归 anchor 只推迟有效上界。
- Test66 的双-anchor paid backbone 在 `18/20` 条改善初值，并减少 DBLP-new/Toronto-new ordinary payload；单独/与 nested 组合的 fast20 仍为 `12.867/12.889s`，group-TSP 全局证书 `0/20` 命中。增加 anchor 数不能替代 junction 前缀共享。
- Test67 将 attachment candidate tree 无参数迭代到 fixed point；fast g12 在 `2--4` 轮、`4--62` 个压缩点稳定，但 DBLP upper 不动，跨库仍有至多 `6.60%` exact gap。继续扩同一 conditional skeleton 已否决，未触发 full。
- pair ancestor/tight-cone dominance 将 Test49 frontier 精确缩为 consumer prefix minima；三库可再降约 `28%--31%`，但 DBLP/MovieLens 仅降 `1.85%/0.39%`。该定理进入 history，单独接口不触发 full。
- Test69 用 `(split root,attachment)` rank-1 profiles 将 pair join 化为两次 singleton transform。fast DBLP 显示 `13.3x` 共享，但 full 仍有 `53.17M/106.31M` profiles；profile-mask 乘积过大，已撤回。
- Test70 组合 one-third endpoint、junction upper 与 delayed progressive packing，fast20 创 A 线新低 `9.161s`、Toronto full `9.64s/58.1MiB`；DBLP 在 `531.831s` 仍无 weight，未越过 V3，全部撤回。
- Test71 的 quarter endpoint 禁止高层 branch publication 后出现 `DPBF=20 / candidate=21` 的完备性反例；Test72 恢复 exact D 后随机累计 `3000/3000`，但 fast20 `12.843s` 慢于正式 `12.380s`。只改 split orientation 不会删除状态族，两版均撤回且不触发 full。
- Test73 保留 offline D、只把 A 改成无 Hash global consumer；随机 `100/100` 精确，但 Toronto-fast g12 settled 仅降 `6.7%`、wall `1.791s -> 6.288s`。调度象限已补齐并撤回，不与 Test70 组合。
- Test74 将 V3 切换前的 exact half rows 注入 global；随机 `300/300`，但 fast20 每条 settled frontier 与 V3 完全相同，created 增加、wall `12.509s -> 13.176s`。half value 只是可重建路径缩写，已撤回。
- Test75 理论基底首次把 half state 改成 root-free paid subtree profile；`paid-half+D+D` completion 已证明 exact。小图累计 `1250/1250`，fixed g8 Pareto ratio `0.572%`、max front `117`。方向保留，下一门槛是 pair-path 大图生成，不直接写完整 solver。
- Test75 pair projection 又在 full DBLP 将 `106.31M` D2 roots 压成 `821` 个单-anchor skyline states（每对最多 `23`）；五库 fast 比例均低于 `1.7%`。全 singleton root-vector 在 Toronto 回升到约 `50%--62%`，所以下一步是内部多点 profile/列生成，而非高维 root 向量。
- Test75 paid pair-block completion 将 anchor 留在两个 half blocks 之一，五库 fast g12 全部 exact；先付 anchor 在三库留有 gap。早期 `g<=7` unrestricted 穷举为 `3000/3000`，但 fixed g13 已有 pair-only `160 -> 161` 反例；anchor skyline 另有 `59 -> 60`。因此 `821` 个 full skyline states 只作初始列，不能宣称一般完备。
- Test75 profiles 直接旁挂旧 A rows 时，Toronto/Toronto-new 的 A values/merges/checks 完全不变，g12 wall `1.791/3.609s -> 1.909/4.039s`；该集成已撤回。tree-target 必须替代 A row 输出，不能只做额外 upper 扫描。
- paid incidences 作为 global-A 唯一种子时，fast DBLP exact，但 `2,047` seeds 膨胀为 `1.483M/1.193M` created/settled labels、耗时 `18.039s`。逐 root 传播会重建已删除维度，模式已撤回。
- Test75 low-core 只保留 `D<=q=ceil(h/2)`，以真实 edge-union 从 pair 生成 `g-2q` core；unrestricted 穷举 `3000/3000`。DBLP-fast D1--D3 即 exact、core `15,779`，但 Toronto 两版仍有 gap 且 front 反弹，故当前只保留 partition-pricing 基底，不跑 full。
- Test75 strong lower 将 fast g12 exact 证书压到 `4--2253` 次 plan pricing；Toronto g13 的 skyline `3+5+5` plan 全 root 定价命中 exact，但临时 Test77 因仍需 D6/A5 证停而从 `20.788s` 退到 `29.113s`。代码已撤回；下一步只研究 pair-batched pricing 与完整停止证书，不优化该 upper 的局部常数。
- fixed g13 进一步给出 q5/core3 `106 -> 107` 反例；放开全部 attachment points 仍失败。q5/core4 的 g13 证据为 `720/720`，扩展 odd-g/Steiner/multi-group/high-cycle 后 targeted 共 `13,520/13,520`。双 pair-forest requested-profile 因子化把 Toronto `5,476` plans 从 `39.158s` 降到 `18.721s`；rooted-D4 又降到 `8.023s`，但 deterministic/all-tight-D4/六标签 DP 在 fast MovieLens 均留下约 `1.38e-6` gap，证明 minimum D4 geometry 不完备。core4 exchange 与 stopping certificate 仍缺，正式 Test21 不改、不跑 full DBLP。
- Test76 将完整六标签 fixed-plan DP 精确压成一侧 D0/D1、另一侧 D0/D1/D2 的 block-anchor 三函数交；singleton `500/500`、双候选组 `200/200`。但 Toronto-fast g12 对算法自身筛出的 `1,269` plans 全部定价后仍为 `0.9688685500 > 0.9616227800`，并需 `2.955s` 构造共享 rows。定理保留，core4 两块 family 否决；正式 Test21 不改，full DBLP 未运行。
- Test78 证明 3--5 macro labels 的 cherry 定价与固定三块的三臂 block-anchor 定价；fixed g13 singleton/双候选组分别 `1000/1000`、`500/500`。三度 12-token caterpillar 证明 simultaneous three-arm family 一般不完备；Toronto-fast 的 one-root-core family 也停在 `0.9688685500 > 0.9616227800`。保留 fixed-plan 定理，顺序提取转向第二接口/paid backbone。正式 Test21 不改，full DBLP 未运行。
- Test78 后续的 soft-terminal block transform 用每个 block 至多 `2^4` 个临时 rows 隐式表达接口；固定块数连续出现 `72->78`、`54->55` 反例，放开为任意有序 3/4-blocks + core<=4 后 g9 为 `200/200`，但无一般证明和大图规模界，只归档不进 Test。
- Test79 曾将 junction 接入 ReleaseV3/B，full 为 `521.055s / 3744.6MiB`。它不满足优化框架 A 的目标，活跃源码和 prepared 接口已撤出；有效数据冻结在 `archive/test79_v3_junction_control_20260713.md`。

## 5. 共同纪律

1. 关键计时使用 Release/O2。
2. 正确性以 DPBF、`1e-6` 和 Toronto 最后一次完整 run 为准。
3. 追加结果只比较最后一个 run header 后的记录。
4. 策略只能由证明、状态结构或理论工作量推出，不能拟合数据集、组数、层级、密度或 wall time。
5. 论文引用必须同时说明“文献机制”和“本仓库适配”；来源见 ReleaseV4 主文档。
6. ReleaseV3、Test80 与 ReleaseV4 的 full DBLP 证据只在有重要算法或发行节点时更新；纯格式调整不触发重复长跑。
7. 运行后清理临时目录、空结果目录和残留进程。

## 6. 历史入口

有效研究文档索引见 `history/README.md`；失败与撤回机制索引见 `archive/README.md`。需要追踪 ReleaseV3 的形成过程时，先读 `history/half_global_hybrid.md`，再按其中引用进入 archive 原始日志。
