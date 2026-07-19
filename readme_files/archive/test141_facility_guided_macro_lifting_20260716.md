# Test141：设施分块引导的 Macro Lifting

更新时间：2026-07-16。Test141 已从活动源码、CMake 目标和构建产物撤回。它把 Test121 选中的 rooted D-block 分区压成较少的 macro labels，再在全图上运行一次精确 macro subset DP，使所有 blocks 联合选择 attachment geometry。方法具有无参数的平方根指数界，也确实减少了一部分后续 D/A states；但 fast20 的状态收益远小于 macro closure 本身的工作，总 wall 回退 `52.86%`，因此没有进入 Toronto g13 或 DBLP。

## 1. 方法与支配关系

在某个自然 D 层结束后，Test121 的压缩锚树 DP 给出一个确定性最优设施方案，并把全部非锚 groups 分成 blocks `P={B1,...,Bp}`。每个 block 都有一张已经可用的 rooted row：singleton 使用组距离，较大 block 使用现有 `D(Bi,v)`。Test141 恢复这个分区，但不恢复具体 witness edge；它增加 permanent anchor label，并用下列 seed rows 运行标准 rooted subset DP：

```text
f_anchor(v) = dist(anchor_group,v)
f_i(v)      = D(Bi,v).
```

每个 macro derivation 都是若干真实 rooted witnesses 与连接路径的并，因此其结果 `U_macro` 是合法 GST 上界。反过来，Test121 选中的 block witnesses、压缩锚树路径和 anchor path 本身就是这些 macro labels 的一个可行连接方案，所以：

```text
OPT <= U_macro <= U_tree.
```

与 Test140 的事后边并集相比，macro DP 不再把 attachment 限制在原设施节点或固定锚树上，而是在全图中联合选择所有 block 的连接位置。

## 2. 无经验参数的运行边界

设非锚 group 数为 `k`。候选只在设施方案把标签数压到至多 `ceil(k/2)+1` 时启动；这保证第一次 macro DP 的复杂度为：

```text
time   O(3^(ceil(k/2)+1) n + 2^(ceil(k/2)+1)(m+n log n))
space  O(2^(ceil(k/2)+1) n).
```

后续自然 D 层只有在恢复出的 macro label 数严格下降时才重新定价。因为标签数每次至少减一，多次运行的指数项形成几何级数，仍由第一次运行主导。这个规则只由状态维数和完备复杂度推出，不读取数据集、固定 `g`、固定层、密度、wall time、完成进度或已知 optimum；实现使用 dense arrays、`priority_queue` 和固定 pivot subset joins，没有 Hash。

## 3. 正确性门

Release/O2 下与 DPBF 按 `1e-6` 对拍：

```text
seed 718101  g=2..8      100/100
seed 718102  fixed g15    30/30
```

全部通过。fast20 的 20 条最终权重也逐条与同机 Test121 相同。该候选只写回合法 incumbent，不改变 permanent-anchor A、root-irreducible D branch、正式状态语义或 exact completion。

## 4. 固定 Fast20 面板

同机 Release/O2 配对使用五个固定数据版本的 g9--g12 首询问。表中时间是 `test80_stats.total_ms` 的四询问合计；macro 时间只统计新增定价器。

| dataset | Test121 | Test141 | 变化 | macro 时间 | D values 变化 | A values 变化 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| DBLP | `349.388ms` | `538.308ms` | `+54.07%` | `214.028ms` | `-5,005` | `-316` |
| DBLP-new | `419.679ms` | `570.917ms` | `+36.04%` | `151.937ms` | `-16,945` | `-30` |
| MovieLens | `3098.064ms` | `6524.419ms` | `+110.60%` | `3507.972ms` | `-663` | `-452` |
| Toronto | `1361.606ms` | `1500.869ms` | `+10.23%` | `37.000ms` | `-6,058` | `-1,176` |
| Toronto-new | `2801.258ms` | `3170.480ms` | `+13.18%` | `220.121ms` | `-74,431` | `-950` |
| **合计** | **`8029.995ms`** | **`12304.993ms`** | **`+53.24%`** | **`4131.058ms`** | **`-103,102`** | **`-2,924`** |

逐查询 `weights.txt` wall 合计为 `8.090463s -> 12.366960s`，回退 `52.86%`。ordinary values 从 `2,774,477` 降到 `2,671,375`，即 `-3.72%`；anchored values 从 `1,262,600` 降到 `1,259,676`，即 `-0.23%`。两类正式 pops 的合计都没有变化，而 macro DP 自己执行了 `23,754,500` 次 dense join probes 和 `4,441,772` 次 queue pops。

## 5. 结论

Test141 证明了“设施分块 -> 少量 macro labels -> 全图联合 attachment”是一条合法、理论上不弱于 Test121 同方案且具有 `3^(g/2)` 型界的上界算子。被否决的是把它作为附加 oracle 接到当前 A 上：它仍需完成数百万次图传播，却只删除约 `3.72%/0.23%` 的 D/A values，未改变正式 pops，因而不是 8,000 秒目标需要的状态级突破。

即使把 dense joins 改成离线稀疏有序相交，也消不掉已经实际发生的 `4.44M` macro pops；继续优化容器或设置运行门槛只会转为常数工程和数据选择。后续若复用 macro coarsening，必须让 macro rows**替代**一段高层 A/D 输出并给出完备性证明，而不是作为额外上界预处理。本轮没有运行 Toronto g13、DBLP q5/q32/q25 或 g15 q33。

Test141 的计算内核来自 [Dreyfus--Wagner](https://doi.org/10.1002/net.3230010302) rooted subset DP；设施分块与复杂度受控调度是仓库适配。本文不把经典 macro DP 称为原创，也不据此宣称已经形成论文方法。
