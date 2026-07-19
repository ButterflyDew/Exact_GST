# Test157 公平 M0：Iwata 半状态参考

更新时间：2026-07-18。本文定义并验证 Test157 的 M0。它的用途是给永久组锚定框架提供一个可公平比较的已知方法起点，不是论文贡献，也不替代 PrunedDP++ 或 GPU4GST baseline。

## 1. 为什么不能使用历史 Half_DPBF

历史 `gst_half_dpbf_main` 与 Iwata--Shigemura 的半状态 MITM 有三处实质差异：它预先分配完整 `2^g*n` 的稠密 `dp` 和 parent 表；奇数 `g` 计算到 `ceil(g/2)`；终端在每个根上运行可选择任意多个小 mask 的覆盖 DP。因此它的状态空间、内存布局和终端工作都与 M1 不同，不能标为同源 M0，也不能用它的慢结果抬高新方法。

## 2. M0 算法

M0 由 `gst_test157_m0_iwata_main` 构建。它与 M1 调用同一个 `methods/Test/test80_anchor_progressive.cpp::SolveOneQuery`，只由编译期宏 `GST_TEST157_IWATA_M0` 改变状态域和终端；没有运行时策略切换。

### 2.1 公共预处理

M0 与 M1 共用 group distance、root-star 可行解、tour lower bound、eager directed-cut dual、junction 可行解，以及 ordinary 行的 sparse、ranked-bitmap、dense 三种精确布局。用于 junction 可行解的组选择只是一项公共 primal heuristic，M0 的状态中没有永久锚点；因此 M0 stats 输出 `anchor_group=0`。

### 2.2 全组半状态

令查询组全集为 `G`，`h=floor(g/2)`。M0 对全部 `g` 个组建立 ordinary mask，并且只构造 `|S|<=h` 的 rooted 状态

`D(S,v) =` 覆盖 `S` 中每个组且以 `v` 为根的最小连通子图代价。

单组状态由组到各顶点的最短距离隐式表示。多组状态先在同一根合并两个真子集，再做一次图最短路闭包。M0 直接复用 M1 的 strict branch basis、离线有序行求交、incumbent 剪枝和 future lower bound，因此 M0/M1 不会被 Hash、稠密表或基础剪枝强度差异污染。

### 2.3 规范三块终端

ordinary 层完成后，M0 枚举满足下列条件的三元组 `(S,L,R)`：

- `S`、`L`、`R` 两两不交且并为 `G`；
- 每块大小不超过 `h`；
- 空块允许，因此同一枚举也覆盖一块或两块退化情形；
- 以 `S<=L<=R` 的确定性 mask 次序去重。

对每个规范三元组，M0 通过三张有序行的同根交集计算

`min_v D(S,v)+D(L,v)+D(R,v)`。

全部三元组处理完毕即返回最小值，不进入 `A`、`F/H` 或转置终端阶段。组合枚举为 `O(3^g)`，最坏时间为 `O(poly(g)(n3^g+2^g C_G))`；只保存半层 ordinary 行，行的实际字节数由三种精确布局决定。

## 3. 正确性验证

正确性依据是 Iwata--Shigemura 的平衡三 rooted-subtree 分解：存在一个最优树和一个根，使三个部分各包含至多 `floor(g/2)` 个组。ordinary 递推精确给出每个部分的 rooted 最优值，规范终端枚举又覆盖该三分解，因此 M0 不漏掉最优解；任意终端候选的三棵 rooted 子图之并都是可行解，因此也不会低估最优值。

独立工具 `gst_test157_core_verifier` 不调用 Test80。它分别计算完整 canonical DP、全组半状态三块终端和枚举顶点子集 MST 的 GST oracle。Release/O2 运行

```powershell
.\build\Release\gst_test157_core_verifier.exe 157001 1000 10 9 18 2
```

得到 `1003/1003` 个手工加随机实例的 M0/full-DP 一致检查，未发现反例。生产 M0 还通过 300 个随机实例的 DPBF 黑盒对拍；随机图包含零权边、重叠组和每组多个候选顶点。真实数据上又完成 101 组 M0/M1 查询权重比较，全部在 `1e-6` 内一致。

## 4. 实际效果

低档与 5 条 smoke 只说明量级，M0 和 M1 的运行时并非每点都相差很大：

| 数据集与查询 | M0 | M1 | M0/M1 | M0/M1 平均 RSS |
| --- | ---: | ---: | ---: | ---: |
| Musae `g=8`, q1--q5 | 2.182s | 1.393s | 1.567x | 47.30 / 47.38 MiB |
| Toronto `g=10`, q1--q3 | 2.615s | 2.337s | 1.119x | 27.43 / 25.73 MiB |
| Musae `g=12`, q1--q5 | 21.508s | 17.118s | 1.256x | 63.92 / 56.91 MiB |
| Toronto `g=12`, q1--q3 | 23.413s | 18.612s | 1.258x | 62.15 / 50.07 MiB |
| Github `g=12`, q1--q5 | 163.735s | 63.736s | 2.569x | 235.83 / 140.54 MiB |

Twitch 与 Github `g=12` 固定前 20 条各重复两次。重建后的两个 M1 run 与两个 M0 run 使用同一源码和 Release `/O2 /Ob2`；下表合并 40 次查询，时间倍率为两次 M0 总时间除以两次 M1 总时间。

| 数据集 | 两次 M0 | 两次 M1 | M0/M1 | M1 逐询问胜数 | M0/M1 平均 RSS | M0/M1 最大 RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Twitch q1--q20 | 382.358s | 119.989s | 3.187x | 39/40 | 126.91 / 94.19 MiB | 379.35 / 136.90 MiB |
| Github q1--q20 | 1114.007s | 426.059s | 2.615x | 38/40 | 233.91 / 132.82 MiB | 646.21 / 286.32 MiB |

两次单独运行的总时间方向均未反转：Twitch 的 M0/M1 为 `3.520x/2.854x`，Github 为 `3.178x/2.159x`。第一轮工作量中，M0/M1 的 ordinary 值分别为 Twitch `108,639,453/29,706,635`、Github `357,999,806/105,159,914`；M0 的规范终端又分别执行约 `1.600B` 和 `4.042B` 次同根检查。这解释了收益来自 mask 维度与终端组织的真实变化，而不是历史 Half_DPBF 的稠密预分配。

## 5. 结论边界

公平 M0 已实现并通过当前正确性与效果子门。结果支持“永久组锚定的 M1 框架在中档大图上明显优于已知全组半状态起点”，但不支持以下推论：

- 不说明固定 anchor 本身是原创；固定根终端消去一维已有相关工作；
- 不说明 M1 在所有低 `g` 都有数量级收益；Toronto `g=10` 当前只有 `1.119x`；
- 不说明 `F/H + transpose` 已通过新颖性评审；该联合结构仍由 M1--M3 差分和外部相关工作复核判断；
- 不说明 M3 稳定省空间；M1--M3 的 RSS 方向仍然混合。

结果目录为 `result_snapshot/v6_review/20260718_test157_m0_fair_reference` 和 `result_snapshot/v6_review/20260718_test157_m0_fair_reference_q20`。历史 Half_DPBF 继续保留为历史实现，但不再出现在 M0 消融中。

主要理论起点：Yoichi Iwata and Takuto Shigemura, *Separator-Based Pruned Dynamic Programming for Steiner Tree*, AAAI 2019，第 4--5 页，<https://ojs.aaai.org/index.php/AAAI/article/download/3965/3843>。
