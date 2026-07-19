# Test108：普通有序连接的分量最小值预检

更新时间：2026-07-15。该探针已完整撤回，Test80 当前仍为 Test107。ReleaseV4 未修改，DBLP g13 q5/q32/q25/q1 均未为 Test108 运行。

## 1. 目标与方法

Test105–107 的 completion 级联证明，`精确已知分量 + 另一行最小值` 能在第二张行读取前安全拒绝大量工作。Test108 尝试把同一思想前移到普通 D 的 pivot/branch 有序连接，但不重复 Test82 的整行证书：当某个物理 join driver 已给出精确值 `D(X,v)`、另一张行尚未查找时，先判断

```text
D(X,v) + μD(Y) + cut_future(v, remaining) > best.
```

若成立，就不查询 `D(Y,v)`。该式由 `μD(Y)<=D(Y,v)` 和现有 directed-cut future 的 admissibility 直接保证安全。探针没有参数、Hash、数据集、固定 `g`、层号或时间特判；bitmap-bitmap 独立内核没有改动。

## 2. 为什么失败

普通 join 与 completion 的关键差别是：很多 driver 顶点最终根本不属于另一张行。Test108 为了在成员查询前拒绝，必须先对这些后来会被结构交集自然删除的顶点读取 directed-cut future。于是它把便宜的有序成员过滤推迟到较贵的下界之后，新增工作超过省掉的第二行查询。

Release/O2 fast20 临时快照在统计归档后已清理，Test107 对照为 `result_snapshot/fast/20260715_095356`：

| 指标 | Test107 | Test108 |
| --- | ---: | ---: |
| query wall sum | `6.466501s` | `6.685800s` |
| solver total | `6400.056ms` | `6620.166ms` |
| ordinary time | `2139.984ms` | `2164.848ms` |
| precheck probes | `0` | `14,074,554` |
| precheck rejects | `0` | `1,337,970` |
| reject rate | - | `9.51%` |
| ordinary values/pops | `2,893,969 / 3,599,523` | 完全相同 |
| direct/binary/linear join work | baseline | 完全相同 |

宽范围随机 DPBF 对拍在 seed `716801` 上为 `100/100`，说明拒绝式正确；但状态、物理 join work 和渐进结构都不变，ordinary 增加约 `25ms`，总 wall 回退 `3.39%`。因此没有启动 fixed-g13 或真实 DBLP 门槛，源码、头文件与输出字段全部撤回。

## 3. Test109–110 的操作顺序复核

Test109 把同一 precheck 移到有序成员相交之后，只对已经确认的共同根付费。宽范围 seed `716911` 为 `100/100`；fast20 共探测 `13,193,842` 个共同根，只由 cut 拒绝 `907,898` 个，拒绝率 `6.88%`。ordinary states/pops 仍完全不变；ordinary 为 `2.090s`，wall 为 `6.447s`，相对 Test107 的差只有一次运行约 `20ms`，不足以证明稳定收益，而且机制仍只省常数距离读取。

Test110 又在 cut 前加入更便宜的旧值证书：若 `known+μD(other)>=row_distance[v]`，完整候选必被当前输出 seed 支配。宽范围 seed `716921` 为 `100/100`；fast20 的 `13,193,842` 个共同根中，旧值只拒绝 `293,051` 个，随后 cut 拒绝 `812,678` 个，总拒绝率 `8.38%`。ordinary states/pops 依旧不变，ordinary 为 `2.125s`，wall 回退到 `6.614s`。因此 Test109 的微小正差被判定为噪声，Test109/110 的源码、统计和临时快照均撤回。

## 4. 保留结论

Test108–110 与 Test82 共同给出更清晰的边界：整行证书太弱；顶点级证书放在成员相交前会为非共同根付费，放在相交后又只拒绝约 `7%--8%` 且不改变状态。下一次若继续研究物化前剪枝，必须让**结构相交和合法下界形成无需逐根 future 读取的批量证书**，或直接改变保留状态的数量级；不能再次调整当前 cut 与第二行查询的先后顺序。

本探针没有新增外部论文引用。
