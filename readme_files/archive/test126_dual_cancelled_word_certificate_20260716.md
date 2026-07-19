# Test126：Dual-Cancelled Bitmap Word 证书

更新时间：2026-07-16。Test126 已从源码、构建目标、统计字段和运行入口撤回。它证明了一个可复用的 directed-cut 消元恒等式，但按 bitmap word 分别保存三个最小值后，同根相关性过弱；Toronto g13 五询问完整 wall 与 peak 均回退，因此没有运行 DBLP。

## 1. 消元恒等式

令 `pi_i(v)` 是 Test107 directed-cut dual 对组 `i` 在根 `v` 的势，`pi_S(v)=sum_{i in S} pi_i(v)`。对互不相交的已覆盖集合 `L,R` 和全部组 `K`，有：

```text
D(L,v) + D(R,v) + pi_(K-(L union R))(v)
= [D(L,v)-pi_L(v)] + [D(R,v)-pi_R(v)] + pi_K(v).
```

锚点状态同理：把 `L` 替换为已经覆盖 permanent anchor 的 `A` 侧即可。右式把 target-dependent future mask 消掉，只剩两张 row 的 reduced value 与一张全局势数组。该等式依赖 dual 势按组可加，不改变 Test107 的状态、下界或精确停止条件。

Test126 对 bitmap accumulator 与 dense/bitmap branch 的每个 64-bit 根字保存：accumulator reduced minimum、branch reduced minimum和全局势 minimum。三者之和若大于 incumbent，则该字中的全部共同根都不可能通过 directed-cut，可以在展开 root bits 前整体跳过。实现没有 Hash、数据集判断、固定 `g`、层号、密度阈值或运行时刻条件；64-bit 字来自既有 ranked-bitmap 物理布局，不是拟合参数。

## 2. 正确性门

候选与 Test121 使用同一 permanent-anchor A/D 主线，只增加安全拒绝。Release/O2 黑盒 DPBF 对拍通过：

```text
seed 717401  g=2..8   100/100
seed 717402  fixed g15 30/30
```

所有差值均不超过 `1e-6`。Test121 和 ReleaseV4 默认目标在撤回后重新构建；本轮没有启动 DBLP。

## 3. Toronto g13 q1--q5

同一进程内依次运行 q1--q5。五条答案逐项一致：

```text
0.7048467020  0.7275272453  0.7985684033  0.6904989514  0.9105249531
```

| 指标 | Test121 | Test126 | 变化 |
| --- | ---: | ---: | ---: |
| 五询问 wall 合计 | `46.424960s` | `47.548889s` | `+2.42%` |
| q5 wall | `25.200243s` | `26.200426s` | `+3.97%` |
| q5 query peak RSS | `146.434MiB` | `159.996MiB` | `+9.26%` |
| q5 ordinary direct work | `273,188,839` | `271,288,723` | `-0.70%` |
| q5 anchored direct work | `125,573,335` | `125,045,376` | `-0.42%` |

q5 普通阶段访问 `22,594,257` 个证书字，只拒绝 `613,562` 个字并跳过 `1,900,116` 个共同根；A 阶段访问 `14,043,943` 个字，只拒绝 `238,164` 个字并跳过 `527,959` 个共同根。q2/q4 没有进入兼容的 bitmap-word 路径。所有 D/A values 与基线逐项相同，说明该机制只减少少量 root 展开，没有改变状态空间。

## 4. 否决结论

三项独立最小值都可能在 word 内不同顶点取得，因此它们的和远低于真正的同根最小和。证书虽然安全，却只减少 q5 约 `0.4%--0.7%` 的 direct work；持久 minima 的构造和空间已经超过收益。缩小 word、保存更多独立标量或按数据集启用只能继续交换常数与内存，不解决相关性缺口，因此不再沿这些变体调参。

保留的理论信息是 dual 消元恒等式。若以后重访，必须提供能共享 `min_v reduced_L(v)+reduced_R(v)+pi_K(v)` **同根相关性**的联合表示，例如可证明的小型联合 profile 或批量 min-plus 接口；不能再把三张 row 各自的 minimum 相加。

本轮没有新增论文引用。消元只使用本项目已有 directed-cut 势的按组可加性；进入论文前仍需检索相近的 reduced-cost join pruning 工作，当前不宣称新颖性。
