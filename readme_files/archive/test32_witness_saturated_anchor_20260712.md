# Test32：Witness-Saturated A 状态反例

更新时间：2026-07-12。Test32 是已经撤出源码和构建入口的状态语义探针。它延续 Test31 的完整 A lattice + D1/D2 cherries，但尝试显式修复树内免费改根。

## 1. 候选语义

仍令：

```text
A(S,v) = 覆盖 anchor 与 S、并包含顶点 v 的最小树成本
```

Test31 只在 attachment root `v` 写入 `A+D` seed，因此已经付费的 ordinary path 在下一次改根时会被重复计算。Test32 在接入 D1/D2 后重建一棵确定 witness：

- D1：从 attachment 沿 group-distance predecessor path 到组终端；
- D2：沿 pair closure path 到 local split root，再沿两条 group-distance paths 到组终端。

若合并得到成本 `c`，则在 witness tree 的每个顶点都以同一成本 `c` 播种新的 A row。状态仍只有 `(mask,vertex)`，不显式保存顶点对。

## 2. 诊断边界

为只检验完备性，原型使用：

- 完整未剪 D2 rows；
- 完整 `2^(g-1)` A lattice；
- 仅 D1/D2 ordinary components；
- A/D 阶段不使用 future/best pruning；
- 确定性最小编号 predecessor tie-break。

因此失败不能归因于下界、branch bit、稀疏 row 或参数门控。

## 3. 结果

Test32 在 Test31 的第一个反例上仍然得到：

```text
seed       712621
iteration  39
DPBF       74
Test32     75
```

没有继续运行随机批次或数据集 benchmark；同一决定性反例已经否决状态语义。

## 4. 为什么饱和一棵 witness 仍不够

当 `A(S,u)=c` 经图边传播到 `v` 时，新树成本为 `c+d(u,v)`，并同时包含传播路径上的 `u` 与 `v`。若把这个较高成本再写回 `u`，标量状态会保留更小的旧值 `c`，丢弃“更贵但已经包含 v”的 witness。

后续在 v 接入一侧分支、再回到 u 接入另一侧分支时，恰恰可能需要这个被标量支配掉的 witness。成本较低并不意味着 attachment 集合更强：

```text
lower cost + smaller paid attachment set
vs.
higher cost + larger paid attachment set
```

两者是 Pareto 不可比的。只饱和新接入的 D component，无法恢复 A 自身历史传播路径带来的 attachment 信息。

## 5. 结论

Test32 的源码、目标和临时目录均已删除，正式 Test21 未修改。

Test30--32 形成递进边界：

1. split seed 丢失 ordinary split/attachment 路径；
2. 完整 D2 cherry 仍无法在单根 A 中免费改根；
3. 保存并饱和一棵确定 witness 仍会被标量 cost dominance 删除必要历史。

因此下一状态若继续删除高阶 D rows，必须维护关于 paid attachment sets 的代表族或 Pareto certificate，并证明其规模严格小于 ordinary rooted rows。只增加 predecessor、单个第二 root 或单棵 witness 都没有完备性依据。
