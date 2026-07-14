# Test65：Recursive Anchor Ordering

更新时间：2026-07-13。Test65 把 permanent-anchor 分解递归应用到 Test21 的 ordinary lattice：选一个 secondary anchor，将原 `D` rows 精确分成不含 secondary anchor 的 `D0` 和包含它的 `B`，先生成全部 `D0`，再生成 `B`。该状态等价且不增加 mask，但若没有新的共享双-anchor几何，它只改变生成顺序；fast20 明显退化，因此代码与统计字段全部撤回。

## 1. 状态等价

令 primary anchor 为 `a`，secondary anchor 为 `b`，`K=U-{a,b}`：

```text
D0(S,v) = 覆盖 S、root 为 v 的最小树
B(S,v)  = 覆盖 {b} union S、root 为 v 的最小树
```

其中 `S subset K`。原 ordinary state `D(T,v)` 一一映射为：

```text
b notin T : D0(T,v)
b in T    : B(T-{b},v)
```

mask 数由 Pascal 恒等式保持不变：

```text
C(|K|,s) + C(|K|,s-1) = C(|K|+1,s).
```

把 secondary anchor 放在 ordinary mask 的最低位后，Test21 现有 pivot recurrence 自动变成：

```text
B(S-Y,v) + branch_D0(Y,v) -> B(S,v).
```

根处分支恰有一个包含 `b`；其余分支递归拆为 root-irreducible `D0` branches，因此 recurrence 完备。实现仍使用原有 sorted rows、branch bitset 和线性 merge，没有 Hash、顶点对或新状态类型。

## 2. 第一版：只改变顺序

primary anchor 仍为 farthest group，secondary anchor 为剩余组中最远者。ordinary rows 的顺序改为：

```text
all D0 sizes 1..h
then all B sizes 1..h
```

quarter/三块 upper 只在对应 B size 完成、该 size 的全部原 ordinary rows 可用后运行。扩大随机 `500/500`（seed `713331`, `n=8..16,g=4..13`）与 DPBF 在 `1e-6` 内一致。

fast20 快照 `20260713_001153`：

```text
formal Test21       12.380s
recursive ordering 13.755s
```

逐库 solver 时间与 ordinary payload 方向：

| dataset | time ratio | D values ratio | D pops ratio |
| --- | ---: | ---: | ---: |
| DBLP | `1.089x` | `1.031x` | `1.159x` |
| DBLP-new | `1.316x` | `1.432x` | `1.670x` |
| MovieLens | `1.164x` | `1.000x` | `1.000x` |
| Toronto | `1.055x` | `1.071x` | `1.083x` |
| Toronto-new | `1.080x` | `1.346x` | `1.420x` |

原因不是 recurrence 变重，而是 D0 高层先在旧 incumbent 下生成；原来由含 secondary rows 参与的 quarter/三块 upper 被整体推迟。

## 3. 二 Anchor Quarter

令 `q=ceil((g-2)/4)`。D0 的 size `q` 完成后，`K` 可分成至多四个不超过 `q` 的 blocks，因此在 root-star 根 `r` 可安全更新：

```text
gd_a(r) + gd_b(r) + D0(X1,r)+...+D0(X4,r).
```

该结算由分块容量推出，无固定 `g` 或经验阈值。临时版通过随机 `300/300`（seed `713341`），fast20 为 `13.617s`，只比第一版回收 `0.138s`。20 条中 16 条没有任何 incumbent 更新；Toronto-new 的三条更新也不足以抵消延后高层造成的 payload。

## 4. 结论

1. recursive anchor 是正确的离线状态重排，但没有降低 worst-case 或实际完整状态族。
2. 单纯把 `gd_a+gd_b` 当两棵独立路径，不会产生 B 式的共享 anchor 几何。
3. 若继续该方向，必须先构造一棵同时连接两个 anchors、只付费一次的公共 backbone，并让 remaining blocks 在该 backbone 上共享 attachment；否则 nested ordering 只会推迟有效上界。
4. 当前结果不运行 Toronto/full DBLP，也不保留 secondary anchor、secondary quarter 或 phase 统计。

