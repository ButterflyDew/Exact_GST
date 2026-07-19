# Test157 同源消融：从完整 `A` 到 `F/H` 与终端转置

更新时间：2026-07-18。本文记录 Test157 为 ReleaseV6 评审建立的同源消融入口和当前结果。所有 M0--M5 目标调用同一个 `methods/Test/test80_anchor_progressive.cpp::SolveOneQuery`，差异只由编译期宏决定；没有运行时回退、数据集/固定 `g`/查询/密度/wall-time 分支，也没有新增经验超参数。本文是评审证据，不是 ReleaseV6 实验章节。

## 1. 消融问题

相关工作排重后，Test152 的核心效果问题变成两项：

1. 用高层 `F/H` 后缀替代完整物化的高层锚定 `A`，是否独立带来稳定收益？
2. 把逐目标 completion 改写为 directed-cut 按根终端事件后，`F/H + transpose` 联合机制是否稳定优于完整 `A`？

M1--M3 只回答这两个问题。M4--M5 再判断生产剪枝包和 progressive dual 是否值得进入最终实现，不能用于证明核心新颖性。

## 2. 同源阶段

| 阶段 | 可执行文件 | 相对前一阶段的唯一算法变化 |
| --- | --- | --- |
| M0 | `gst_test157_m0_iwata_main` | Iwata-equivalent 全组半状态：`|S|<=floor(g/2)`，以规范三块同根终端结束 |
| M1 | `gst_test157_m1_full_a_main` | permanent group anchor + strict branch，物化平衡终端所需的完整 `A` 层 |
| M2 | `gst_test157_m2_adjoint_main` | 在 M1 上只启用 closure-aware `F/H` 和 eager boundary，不再物化高层 `A` |
| M3 | `gst_test157_m3_transpose_main` | 在 M2 上只启用 directed-cut transposed terminal |
| M4 | `gst_test157_m4_pruning_main` | 在 M3 上增加 changed-arc eager dual、anchor facility、anchor-tree facility 及其确定性摊销调度 |
| M5 | `gst_test157_m5_progressive_dual_main` | 保持 M4 其余模块不变，把 eager full dual 换成 Test152 progressive dual |

M1--M3 共用 root-star upper、tour lower、eager dual 和行布局，以免基础剪枝强度不同污染核心差分。M4 是生产辅助模块的整体增量，不把这些已有 primal/dual 技巧拆成论文贡献。M5 与 M4 的区别仅是 dual 构造时机；progressive 路线内部仍使用 changed-arc residual 初始化。

每个目标在 stats 行写入 `ablation_stage=0..5`，避免只靠目录名辨认二进制。全部结果由 MSVC Release `/O2 /Ob2` 构建生成，空间列使用逐询问 query peak RSS。

### 2.1 M0 实现审计

历史 `gst_half_dpbf_main` 不能继续冒充 M0。它一次性分配完整 `2^g * n` 的 `dp` 与 parent 稠密表，奇数 `g` 计算到 `ceil(g/2)`，并在每个根上用可取任意多个小 mask 的覆盖 DP 结束；Iwata--Shigemura 起点则只需要 `|S|<=floor(g/2)` 的 rooted 状态和规范的至多三块终端。两者计时、空间和终端工作都不同。

当前 `gst_test157_m0_iwata_main` 已按以下定义实现：

1. 与 M1 使用同一 ordinary-row 构造、稀疏/位图布局、图闭包、group distance、tour 和 eager dual；
2. 状态 mask 覆盖全部 `g` 个组，不选择永久锚点，且只建立 `|S|<=floor(g/2)` 的 `D(S,v)`；
3. 终端只枚举两两不交且并为全集的规范 `D(S,v)+D(L,v)+D(R,v)`，允许空块但不允许任意多块覆盖 DP；
4. 独立小图 verifier 比较三块终端、完整 canonical DP 和暴力 GST oracle，生产目标再与 DPBF 黑盒对拍；
5. 不加入数据集、固定 `g`、查询、密度、wall-time 或经验阈值分支。

五项均已满足。实现、正确性、两轮 20 查询结果和结论边界单独见 `test157_m0_fair_reference.md`；历史 Half_DPBF 仍不能进入 M0/M1 消融。

## 3. 正确性

当前所有同源阶段答案逐条一致：

- GPU4GST Musae `g=8` 前 5 条与 Toronto `g=10` 前 3 条：M1/M2/M3 共 8 组逐阶段比较，无差异；
- GPU4GST Musae、Toronto、Github `g=12`：每库 M1/M2/M3 分别比较 5、3、5 条，无差异；
- GPU4GST Musae `g=13` 前 20 条：M1--M5 的 100 个结果逐条一致。
- GPU4GST Twitch 与 Github `g=12` 前 20 条：首轮 M1--M4 和反序重复的 M1/M3/M4 共 280 个阶段结果逐条一致。
- 公平 M0：独立 verifier 的 1003 个手工加随机检查、生产 M0 对 DPBF 的 300 个随机实例，以及 101 组真实 M0/M1 查询比较均无差异。

既有 443 个 M1--M5 阶段结果加上 101 个 M0 真实结果，只验证同源配置没有改变答案。核心递推本身另由 Test157 independent verifier 的完整 DP、M0 三块终端、显式高层 `A`、`F/H`、逐 cut 终端和 oracle 证据覆盖，详见 `test152_core_correctness.md`。

## 4. 同源效果

### 4.1 M0 到 M1：永久锚定框架

Twitch 与 Github `g=12` 固定前 20 条各完成两个 M0 run 和两个重建后的 M1 run。两轮合并后，M1 的总时间相对公平 M0 分别快 `3.187x` 和 `2.615x`，逐查询赢 `39/40` 和 `38/40`；每个单独 run 的方向也一致。M0 的平均 query peak RSS 分别为 `126.91/233.91 MiB`，M1 为 `94.19/132.82 MiB`。低档收益较小：Musae `g=8` 为 `1.567x`，Toronto `g=10` 为 `1.119x`。完整数据和工作量解释见 `test157_m0_fair_reference.md`。

这项消融说明永久锚定确实降低了全组半状态的实际代价，但固定 anchor 本身已有先例，不能作为 Test152 的新颖性。核心候选仍由下面的 M1--M3 差分判断。

### 4.2 低层 M1--M3 smoke：尚无可见收益

| 数据集与查询 | M1 完整 `A` | M2 `F/H` | M3 `F/H + transpose` |
| --- | ---: | ---: | ---: |
| Musae `g=8`, q1--q5 | 1.393s | 1.384s | 1.445s |
| Toronto `g=10`, q1--q3 | 2.337s | 2.360s | 2.317s |

`g=8/10` 的差异处于几个百分点内，方向不一致。低层结果不能支持核心贡献，只说明新增机制没有在小状态空间中造成数量级固定成本。

### 4.3 `g=12` 多库面板

| 数据集与查询 | M1 | M2 | M3 | M1/M2 | M1/M3 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Musae q1--q5 | 17.118s | 15.267s | 14.079s | 1.121x | 1.216x |
| Toronto q1--q3 | 18.612s | 15.619s | 13.038s | 1.192x | 1.428x |
| Github q1--q5 | 63.736s | 68.940s | 60.531s | 0.925x | 1.053x |

M2 在 Musae/Toronto 上有收益，却在 Github 上回退约 `8.2%`。M3 相对 M1 在三个数据集上均更快，但幅度从 `1.053x` 到 `1.428x`，明显依赖查询工作量。这个结果支持把 **`F/H + transpose` 联合作为候选单位**，不支持把 adjoint 单独写成稳定加速。

### 4.4 Musae `g=13` 前 20 条

| 阶段 | 总时间 | 相对 M1 总时间 | 相对 M1 几何平均 | 逐查询胜 M1 | 平均 RSS | 最大 RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| M1 | 134.229s | 1.000x | 1.000x | - | 77.17 MiB | 154.68 MiB |
| M2 | 139.391s | 0.963x | 1.003x | 10/20 | 74.47 MiB | 141.76 MiB |
| M3 | 115.335s | 1.164x | 1.130x | 16/20 | 76.98 MiB | 146.09 MiB |
| M4 | 109.533s | 1.225x | 1.183x | 17/20 | 74.85 MiB | 133.18 MiB |
| M5 | 111.782s | 1.201x | 1.158x | 16/20 | 76.07 MiB | 133.12 MiB |

20 条询问的 M1 时间从 `1.26s` 到 `20.83s`，再次证明 q1 不能代表该档。M2 对 M1 恰好 10 胜 10 负，几何平均近似持平，总时间受长查询拖累而慢 `3.7%`。M3 赢 16 条，总时间快 `16.4%`，比 5 条 smoke 更稳定地支持联合机制。

M4 比 M3 再快约 `5.3%`，说明生产辅助剪枝在该面板有价值。M5 相对 M4 总时间为 `0.980x`，只赢 3/20，说明 progressive dual 在 `g=13` 不占优。后续冻结 `g=4..10` 面板仍选择 M5 作为唯一跨 `g` 路线；这里保留高档反例，不能写成“M5 逐档更快”。

### 4.5 GPU4GST `g=12` 前 20 条反序重复

Twitch 与 Github 使用同一批固定前 20 条查询各运行两轮。第一轮的阶段顺序分别为 Twitch `M3 -> M1 -> M2 -> M4`、Github `M2 -> M1 -> M3 -> M4`；第二轮反转 M1/M3 的相对顺序，并重复 M4。下表的 `M1/M3` 和 `M1/M4` 均为前者总时间除以后者总时间。

| 数据集 | 轮次 | M1 | M2 | M3 | M4 | M1/M3 | M3 胜 M1 | M1/M4 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Twitch | 1 | 74.941s | 78.454s | 68.201s | 65.736s | 1.099x | 12/20 | 1.140x |
| Twitch | 2 | 64.613s | - | 58.884s | 57.517s | 1.097x | 13/20 | 1.123x |
| Github | 1 | 240.626s | 244.248s | 187.903s | 159.217s | 1.281x | 20/20 | 1.511x |
| Github | 2 | 202.305s | - | 180.636s | 183.935s | 1.120x | 16/20 | 1.100x |

两轮合并后，Twitch 的 M1/M3 为 `1.098x`，Github 为 `1.202x`；M1/M4 分别为 `1.132x` 和 `1.291x`。Github 的绝对时间在两轮间波动明显，但 M3 相对 M1 的方向没有反转；Twitch 两轮的总时间倍率几乎相同。M2 第一轮在两库分别比 M1 慢 `4.5%` 和 `1.5%`，再次说明 **`F/H` 的状态消除本身不足，转置终端是把该结构变成实际收益的必要组成。**

工作量统计也符合这条解释。M1--M3 的 ordinary 状态数完全相同；Twitch 的高侧 M1 `A` 值为 5,585,312 个，M3 改为 1,749,511 个低层 `A` 值、527,379 个 `H` 值和 474,877 个终端事件。Github 对应地从 21,699,897 个 M1 `A` 值改为 6,347,540 个低层 `A` 值、1,025,378 个 `H` 值和 931,363 个终端事件。M4 又减少 ordinary 工作，但那是通用剪枝包的增量，不能用于证明 `F/H + transpose` 的新颖性。

### 4.6 M4/M5 跨 `g` 冻结

新增同源面板固定 Musae/Twitch/Github `g=4..10` 每点 20 条，共 420 对查询，并按单元交替运行顺序。M4/M5 权重 420/420 一致；M4/M5 总时间为 `212.569/207.016s`，M5 快 `2.68%`，逐询问胜数为 `149/271`，平均 query peak RSS 为 `45.79/43.46 MiB`。分档上 M5 在 `g=4..7` 更快，`g=9..10` 则由 M4 更快；Musae `g=13` 也由 M4 快约 `2.0%`。

最终不允许按 `g` 切换。综合总时间、低 `g` 固定预处理和平均 RSS，后续唯一候选冻结为 **M5**，M4 只保留为消融。`2m+n` 调度是无拟合常数的结构规则，但不是严格竞争比，也不属于核心新颖性。完整逐 `g` 表、partial-prefix verifier 和数值审计见 `test157_production_audit.md`。

## 5. 空间结论

首轮空间结果不支持“联合机制稳定降低实际内存”的表述：

- Musae `g=12`：M1/M2/M3 平均 RSS 为 `56.91/56.41/57.44 MiB`；
- Toronto `g=12`：`50.07/48.70/53.22 MiB`，M3 的终端事件表使 peak 上升；
- Github `g=12`：`140.54/138.68/141.69 MiB`；
- Musae `g=13` q1--q20：`77.17/74.47/76.98 MiB`，M3 与 M1 基本持平。
- Twitch `g=12` q1--q20 两轮平均 RSS：M1/M3/M4 为 `94.23/94.87/94.58 MiB`；
- Github `g=12` q1--q20 两轮平均 RSS：`132.65/130.92/125.38 MiB`。

`F/H` 确实删除了部分高层 `A`，但 M3 同时保存 `H` 行和转置终端事件；实际 RSS 取决于图规模、稀疏行密度和 incumbent 剪枝。当前只能报告测得值，不能宣称理论状态消除必然转化为更低的 query peak RSS。

## 6. 当前判定

1. **M2 单独未通过效果门。** 它在不同库和不同长查询上方向不稳，不能把“adjoint 本身更快”写进论文。
2. **M0/M1 基线效果子门通过。** 公平 M0 已实现；M1 在 Twitch/Github `g=12` 两轮合计快 `3.187x/2.615x`，但低 `g` 可以只有约 `1.1x`，且该差异不是新颖性证据。
3. **M3 联合机制通过当前内部时间效果子门。** Musae、Toronto、Twitch、Github 的 `g=12` 与 Musae `g=13` 均快于 M1；Twitch/Github 的 20 条反序重复没有发生方向反转。这个结论只支持联合结构，不支持 M2 单独加速，也不等于新颖性评审已经通过。
4. **M1--M3 空间门未通过。** query peak RSS 方向混合；M0/M1 的空间下降不能替代该结论。
5. **M4/M5 取舍子门通过。** 420 条同源面板冻结 M5 为唯一候选；高 `g` 回退仍需如实报告，不能加入运行时切换。
6. **ReleaseV6 仍不能创建。** 还需要补 Twitch/Github `g=13..16` 完成率、扩大的相关工作复核和同机 GPU4GST 对照。当前机器只有 2 GiB 的 GeForce GT 730，不能把缺失的 GPU 实验伪装成本机可完成项。

## 7. 结果位置

- `result_snapshot/v6_review/20260718_test157_same_source_smoke`
- `result_snapshot/v6_review/20260718_test157_same_source_g12`
- `result_snapshot/v6_review/20260718_test157_same_source_g12_github`
- `result_snapshot/v6_review/20260718_test157_same_source_g13_musae`
- `result_snapshot/v6_review/20260718_test157_same_source_g13_musae_q20`
- `result_snapshot/v6_review/20260718_test157_gpu4gst_g12_q20`
- `result_snapshot/v6_review/20260718_test157_gpu4gst_g12_q20_repeat2`
- `result_snapshot/v6_review/20260718_test157_m0_fair_reference`
- `result_snapshot/v6_review/20260718_test157_m0_fair_reference_q20`
- `result_snapshot/v6_review/20260718_test157_m4_m5_g4_g10_q20_current`

每个目录按 `m1`--`m5/<dataset>/Test80/<query>/` 保存 `weights.txt` 和 `test80_stats.txt`。所有比较读取各文件最后一次 run；本轮目录均只包含一次 run。

最大可运行 `g` 的独立冻结结果见 `test157_frozen_panel.md`，不与本页的同源机制差分混合。
