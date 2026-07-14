# Test39：One-Third Backbone States

更新时间：2026-07-12。Test39 把 Test21 高于 one-third 的 ordinary rows 从“任意 rooted tree”改为“backbone endpoint tree”，只允许大小不超过 `q=ceil((g-1)/3)` 的 closed components 沿路径接入。该状态定义精确，随机与数据集权重均通过；但它不减少首要瓶颈 D2，fast 基本持平、Toronto full 高层只缩减约 `4%`，因此不触发 DBLP 长跑，也不进入当前 Test21。三处原型限制已撤回。

## 1. 分隔结构

令 `k=g-1` 为 nonanchor token 数，

```text
q = ceil(k/3)
h = floor(g/2).
```

对一棵带 token 权重的树，从任意 root 沿唯一一个包含超过 `q` 个 token 的 child component 前进；因为当前待处理子树至多含 `h<=2q` 个 token，这样的 heavy child 至多一个。所得 root-to-leaf backbone 的每个离路径组件至多含 `q` 个 token。

当前 Test21 的 token centroid 已把最优树在某个 vertex `v` 分为：

```text
anchored side  <= h-1 nonanchor tokens
ordinary side <= h tokens
ordinary side <= h tokens.
```

分别在三个 rooted sides 上使用上述 heavy backbone，即可让所有离 backbone 的 closed components 大小不超过 `q`。

## 2. 三类状态

Test39 重新解释现有 rows，而不增加 mask 数：

```text
D(S,v)   exact closed rooted component, |S|<=q
P(S,v)   ordinary backbone tree, endpoint v, q<|S|<=h
A(S,v)   anchor-containing backbone tree, endpoint v, |S|<=h-1
```

递推为：

- `D` 在 one-third 以内仍运行完整同根 split 与图闭包；
- `P` 的累计侧含固定 pivot，只接入 `|B|<=q` 的 root-irreducible closed `D(B)`；
- `A` 从 anchor component 开始，也只接入 `|B|<=q` 的 closed `D(B)`；
- 每次接入后运行原图闭包，表示继续延伸 backbone endpoint；
- 最终仍使用 `A+P+P` completion。

高 `P` rows 不发布 branch bits，因为它们不是可以离开 backbone 独立接入的 closed components。所有 row values 仍对应真实可行树，因此 quarter/三块 upper 和 incumbent pruning 均安全。

## 3. 完备性

在 centroid 的三个 rooted sides 中，选择 heavy backbone。每个离路径 component 的 token 数不超过 `q`，可由精确 `D` 表示；沿 backbone 从远端向 centroid 依次接入这些 components，可由 `P` 或 `A` recurrence 重现。三个 endpoint states 最后在 centroid 同根合并，得到原最优树。

反向方向直接成立：每个状态只做真实树的同根并、边延伸与图闭包。因此 completion 最小值既不高于最优树，也不会低于真实 GST optimum。

独立 Python dense prototype还验证了更一般的“small anchored component + 左右两条完整 backbone”公式，随机 group-query `2000/2000` 一致。

## 4. 正确性

Release/O2 完整 Test21 原型：

```text
g=2..12    500/500, seed 712811
fixed g13    50/50, seed 712813
```

均与 DPBF 在 `1e-6` 内一致。fast20 与 Toronto full 的全部权重也和正式 Test21 一致。

## 5. Fast20

临时快照 `20260712_143551`，提取后删除：

```text
Test39 fast20    12.695s
正式 Test21      12.380s（历史正式快照）
```

同机运行环境约为 `12.7s`，因此总体只能视为持平，不能宣称加速。变化集中在 size 6：

| dataset g12 | formal D6 values | Test39 P6 values | reduction |
| --- | ---: | ---: | ---: |
| Toronto | `110,307` | `106,064` | `3.85%` |
| Toronto-new | `514,393` | `503,432` | `2.13%` |
| DBLP | `1,323` | `1,238` | `6.42%` |
| DBLP-new | `5,099` | `5,096` | `0.06%` |
| MovieLens | `2,216` | `2,112` | `4.69%` |

size 5 的 values/pops 不变：对 `|S|=5,q=4`，固定 pivot 后任意 proper branch 本来就不超过 4。Test39 真正删除的是 size-6 的 size-5 closed branch 入口，以及 anchored A5 对 D5 branch 的读取。

## 6. Toronto Full g13

```text
weight             0.7048467020
wall               21.403s
formal wall        20.788s
formal D6 values   223,812
Test39 P6 values   214,174
formal D6 pops     312,399
Test39 P6 pops     298,410
peak RSS           102.9MiB
```

P6 values/pops 约减少 `4.3%/4.5%`，但端到端没有改善。D2--D5 的主要工作基本不变。

## 7. 为什么不跑 DBLP Full

Test39 不改变 ordinary D2 的状态定义、seeds、图闭包或 retained values。Test26 已把 full DBLP 的首要瓶颈定位到约 `125.6M` pair-root states；Test38 又证明只减少 Dijkstra sources 仍无法跨过 V3 时间线。Test39 只削减后续 size-6 的少量入口，缺乏触发另一轮 DBLP 长跑的数量级依据。

## 8. 文献边界

树上放置少量 separator vertices 并限制 full-component terminal 数不是新结论：

- [Vygen, *Splitting trees at vertices*](https://doi.org/10.1016/j.disc.2010.09.024)：给出移除 `s` 个 vertices 后 component weight 至多 `1/(s+1)` 的紧界，并讨论 Steiner full components；
- [Fuchs et al., *Dynamic Programming for Minimum Steiner Trees*](https://doi.org/10.1007/s00224-007-1324-4)：通过猜测 separator terminals 获得低于经典 `3^k` 的终端指数，代价含 `n^q`；
- [Iwata and Shigemura, *Separator-Based Pruned Dynamic Programming for Steiner Tree*](https://doi.org/10.1609/aaai.v33i01.33011520)：以 separator 必要条件删除不重要 DP states。

本轮可单独保留的是：把 one-third closed components、pivot-oriented endpoint rows 与现有 `A+P+P` half completion 组合成不增加 mask 数的 GST 状态解释。由于尚未得到 DBLP 性能优势，文档不宣称论文级原创贡献。

