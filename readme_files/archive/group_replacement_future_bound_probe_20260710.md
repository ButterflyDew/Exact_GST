# Group Replacement 与 Future Bound 探针归档

本文记录 2026-07-10 对三类精确剪枝候选的独立验证：候选点替换上界、exact-pair future bound，以及代表点 degree packing。三者都不包含数据集、组数、层级或运行时特判，也不修改 Test19/ReleaseV1。最终结论是：它们有正确的理论含义，但没有解决 full DBLP g13 的目标瓶颈，因此不进入 solver，也不建立 Test20。

## 1. Candidate Portal Replacement Cap

对已覆盖组集 `I`、未覆盖组 `b` 和候选点 `x in A_b`，令 `F_I(x)` 是一棵以 `x` 为根并覆盖 `I` 的可行树的代价。定义：

```text
C_b(I) = max_{x in A_b} F_I(x)
C(I)   = min_{b notin I} C_b(I)
```

若 rooted state `(I,v)` 的代价大于 `C(I)`，它不可能属于严格最优 GST。任一完成树都必须命中所选组 `A_b` 中的某个实际候选点 `x`；把原来的 `I` 子树替换成定义 `C_b(I)` 时为同一个 `x` 准备的 witness，仍在 `x` 与余下部分连接，并严格降低总代价。

探针使用无需额外 Steiner 求解的 star witness：

```text
F_I(x) = sum_{a in I} dist(A_a, x)
```

预处理由已有 group-to-vertex distances 得到，满足当前复杂度上界，也没有经验参数。

## 2. 正确性检查

cap-on 与 cap-off 的稠密 Dreyfus-Wagner 在随机小图上逐实例比较：

| seed | group range | instances | mismatches |
| --- | --- | ---: | ---: |
| `710501` | `g=2..8` | `5000` | `0` |
| `710513` | fixed `g=13` | `1000` | `0` |

这只证明实现与替换论证一致，不代表它在目标数据上有收益。

## 3. Fast 数据信号

以下统计 exact pair rows 中先通过已知 OPT 与 Test19 TSP/2 的状态，再看 replacement cap 能否继续删除。均为 Release/O2。

| dataset, g12 q1 | TSP/2 survivors | cap pruned | cap pruned % |
| --- | ---: | ---: | ---: |
| Toronto | `155,570` | `13` | `0.008%` |
| Toronto-new | `218,517` | `149,279` | `68.31%` |
| DBLP | `139,044` | `56,673` | `40.76%` |
| DBLP-new | `26,892` | `0` | `0%` |
| MovieLens | `19,340` | `0` | `0%` |

五个 fast 数据版本的 `g=9..12` 合计为 `1,337,133` 个基线状态，cap 删除 `431,766` 个，即 `32.29%`。这个横向信号足以支持一次受控 full g13 结构探针，但不足以直接进入 solver。

## 4. Full DBLP g13 的决定性负结果

已知最优值使用已有结果 `12.5936282853`。这里没有运行完整 solver；seed signal 只枚举 pair seeds，pair-layer signal 逐 pair 重放 A*，每行结束即释放，不保留 78 个稠密 rows。

```text
seed_signal:
all=194826996
baseline=49203879
baseline_cap=49203879
cap_pruned=0
total_ms=36568

pair_layer:
pairs=78
settled=125637681
cap_kept=125637681
cap_pruned=0
queue_pushes=225221651
total_ms=252640
```

78 个 pair mask 每一个都是 `0%` cap prune。pair-layer 的外部观测峰值约 `1.34GB`；运行正常结束，bounded runner 持有实际 PID，没有遗留进程。

因此 fast 上的平均收益不能外推到目标查询。replacement cap 在 full DBLP g13 的真实 pair-layer 瓶颈上严格没有作用，至此撤回，不再做完整 solver 长跑。

## 5. Exact-Pair Future Bound

对 rooted state `(S,v)` 的未来组集 `R`，定义：

```text
h_pair(v,R) = max_{B subseteq R, |B|=2} D(B,v)
```

任何从 `v` 覆盖 `R` 的未来树都覆盖其中每一对组，所以该式 admissible。每个 `D(B,v)` 沿图边满足 1-Lipschitz，取最大值仍 edge-consistent。

fast g12 exact-row 信号如下：

| dataset | TSP/2 survivors | exact-pair survivors | pruned % |
| --- | ---: | ---: | ---: |
| Toronto | `155,570` | `24,205` | `84.44%` |
| Toronto-new | `218,517` | `65,066` | `70.22%` |
| DBLP | `139,044` | `139,044` | `0%` |
| DBLP-new | `26,892` | `26,892` | `0%` |
| MovieLens | `19,340` | `18,528` | `4.20%` |

它在 DBLP g12 已经是零信号，而且需要持有或反复生成全部 pair rows；因此没有理由升级到 full g13。

## 6. Representative Degree Packing

group metric 允许同一组的两条 tour incident edges 选择不同候选点。为部分恢复代表点一致性，对 `a,b,c` 定义：

```text
q_a(b,c) = min_{x in A_a} (dist(A_b,x) + dist(A_c,x))
```

对未来组集 `R`，令 `r_a(R)` 是不同 `b,c in R-{a}` 上的最小 `q_a(b,c)`。root 加所有组的 Hamiltonian cycle 中至多两个组与 root 相邻，其余 `|R|-2` 个组都需要两条 group-neighbor 边。由此得到：

```text
h_degree(R) = 1/4 * sum(the |R|-2 smallest r_a(R))
```

系数 `1/4` 来自 incident charge 至多重复计算 tour edge 两次，再由 double-tree 的 `tour/2` 转为树下界。它 admissible、root-independent、edge-consistent，预处理无超参数。

但 fast g12 仅 Toronto 从 `155,570` 降到 `155,265`，删除 `305` 个状态（`0.196%`）；Toronto-new、DBLP、DBLP-new、MovieLens 均删除 `0`。该 relaxation 太松，不做 full g13。

## 7. Representative-Consistent Second-Order Path

degree packing 没有协调哪些组同时成为某个内部组的邻居。更强的候选对完整访问顺序 `a1,...,ak` 计费：

```text
L(v; a1,...,ak) = dist(A_a1,v)
                 + 1/2 * sum_{i=2..k-1} q_ai(a{i-1},a{i+1})
                 + dist(A_ak,v)
h_path(v,R) = 1/2 * min_order L(v; order)
```

在任何使用实际代表点的 root-group Hamiltonian cycle 中，内部组 `ai` 的两条 incident edges 之和不小于对应 `q_ai`。全部内部 charge 的一半不超过 group path 边和；两个 endpoint group distance 分别不超过 root incident edge。因此 `L` 不超过该 cycle，cycle 又不超过 future tree 的 double-tree closed walk，故 `h_path` admissible。root 沿边移动时两个 endpoint distance 的和至多变化两倍边权，最后除二，因此 edge-consistent。

探针用状态 `(mask,first,previous,last)` 做二阶 Held-Karp DP。复杂度为 `O(2^g g^4)` 时间、`O(2^g g^3)` 峰值空间；折叠后持久 endpoint 表为 `O(2^g g^2)`。

随机机械检查：

```text
SECOND_ORDER_ALL_OK seed=710521 iterations=1000
```

每个随机实例都对所有非空 mask、所有 root vertex 检查 `h_path<=exact rooted DP`，并对每条图边检查双向 consistency。

Release/O2 fast g12 的增量信号：

| dataset | TSP/2 survivors | path survivors | pruned % |
| --- | ---: | ---: | ---: |
| Toronto | `155,570` | `117,348` | `24.57%` |
| Toronto-new | `218,517` | `205,813` | `5.81%` |
| DBLP | `139,044` | `139,044` | `0%` |
| DBLP-new | `26,892` | `26,840` | `0.19%` |
| MovieLens | `19,340` | `18,342` | `5.16%` |

五条查询的预处理均约 `81--84ms`。g12 峰值工作区为 `61,355,520` bytes（约 `58.5MiB`），折叠后仍常驻 `4,718,592` bytes。它比 degree packing 明显更强，但仍没有触及 DBLP pair states，故不做 full g13，也不进入 solver。

## 8. 当前结论

1. full DBLP g13 的主要已知事实是：仅 pair layer 就有 `125,637,681` 个 TSP/2 可定型状态；replacement cap 对其中一个都无效。
2. 只加强“小子集 rooted cost”不能自动改善 DBLP：exact-pair future 在 DBLP g12 已经为零。
3. 二阶路径 DP 已经协调整条访问顺序，但 DBLP g12 仍为零；继续沿 group-tour 局部 tightening 搜索的优先级降低。
4. 下一候选转为 pair-row 的 exact predecessor certificate：压缩长期表示，并把线性解码计入原有 `O(3^g n)` join 预算。
5. 只有该表示先在 fast 上证明 exact、显著省空间且解码代价可控，才考虑新的 bounded full g13 pair-layer 探针。

## 9. 复现入口

```text
cmake --build build --config Release --target gst_group_replacement_probe
build\tools\group_replacement_probe\Release\gst_group_replacement_probe.exe --self-check 710501 5000 2 8 10
build\tools\group_replacement_probe\Release\gst_group_replacement_probe.exe --second-order-self-check 710521 1000 2 7 10
tools\group_replacement_probe\run_bounded.ps1 -Mode seed
tools\group_replacement_probe\run_bounded.ps1 -Mode pair
```

相关原始文献：

- [Dijkstra meets Steiner](https://arxiv.org/abs/1406.0492)：rooted labels、feasible lower bounds 与 replacement pruning 的经典框架。
- [DS*](https://arxiv.org/abs/2011.04593)：一般 admissible lower bound 与 label reprocessing。
- [PrunedDP](https://ronghuali.github.io/paper/sigmod2016gst.pdf)：当前比较基线与 GST 动态规划剪枝语义。
