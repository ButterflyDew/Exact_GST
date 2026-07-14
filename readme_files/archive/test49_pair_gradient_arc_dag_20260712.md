# Test49：Pair Gradient Arc-DAG

更新时间：2026-07-12。Test49 研究如何让共享 skeleton 直接承担 ordinary D2 的第二边界，而不是继续把 skeleton 只当 incumbent。核心结果有两面：pair predecessor forests 可以精确聚合成一个 `(directed edge, pair bits)` DAG，且对任意 metric-closed consumer 只需 roots 与 downhill sites；full DBLP exact-best 下 sites 仅为 pair states 的 `8.416%`。但把这些 sites 按每个 A/D consumer mask 显式展开后，fast 工作和内存重新膨胀，端到端退化。正式 Test21 与历史 pair probe 最终恢复，本文保留定理、full 结构证据和否决边界。

## 1. Gradient 支配定理

对 pair row `D` 的 predecessor forest 边 `p -> v`：

```text
D(v) = D(p) + w(p,v).
```

设 `A` 是任意已经完成图闭包的 rooted row。合并后待闭包的 source 为：

```text
f(v) = A(v) + D(v).
```

若 `A(p) <= A(v)`，则对任意目标 `x`：

```text
A(p)+D(p)+dist(p,x)
<= A(p)+D(p)+w(p,v)+dist(v,x)
<= A(v)+D(v)+dist(v,x).
```

所以 `v` 的整个位移锥被 parent `p` 支配。精确 source frontier 只需：

- pair forest roots；
- 满足 `A(v) < A(p)` 的 child endpoints；
- 若 consumer 的 parent value 被 incumbent pruning 删除，则保守保留 child。

这与 Test30 的错误 local split 替换不同。Test30 删除了 split root 到 attachment root 的第二边界；Test49 保留完整 predecessor path，只删除其对当前 closed consumer 可证明被 parent 支配的 attachment sites。

## 2. Full Frontier 规模

在历史 `pair_forest_probe` 上增加临时 `--gradient` 模式。选择与 Test21 相同的 root-star/farthest permanent anchor，仅测 66 个 nonanchor pairs；用已知精确 best `12.5936282853` 给出乐观结构下界。

五库 fast g12：

| dataset | settled | roots | anchor-gradient sites | site ratio |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `130,461` | `15,001` | `55,833` | `42.80%` |
| Toronto-new | `184,459` | `11,846` | `57,834` | `31.35%` |
| DBLP | `126,399` | `3,470` | `17,524` | `13.86%` |
| DBLP-new | `21,785` | `4,765` | `6,615` | `30.36%` |
| MovieLens | `19,206` | `357` | `510` | `2.66%` |

full DBLP g13 q1：

```text
nonanchor pairs       66
settled               106,310,433
forest roots              506,751  (0.477%)
anchor-gradient sites   8,947,182  (8.416%)
search                 201.945s
certificate errors       0
```

这是第一个证明 D2 的 split/attachment 第二边界在 full 上可缩小超过一个数量级的精确结果。

## 3. Arc-to-Pair Bitset DAG

逐 pair 保存 parent code 仍要求约 `O(k^2 n)` cells，并且 consumer 要逐 pair 扫描。Test49 改为联合表示：

```text
root_bits[v]                 哪些 pair forests 以 v 为 split root
arc_bits[parent -> child]    哪些 pair forests 使用该 predecessor arc
```

每个 pair-root state 恰好贡献一个 root bit 或一个 arc bit。对于 closed `A`：

1. 读取 `root_bits[v]`；
2. 只在 `A(v)<A(p)` 的有向边读取 `arc_bits[p->v]`；
3. 沿同一 pair bit 的 parent arcs 回溯到 root，恢复 `D(pair,v)`。

没有 Hash、pair-specific priority、顶点对状态或数据特判。固定 bit-array 字节数为：

```text
(2m+n+1) * ceil(C(k,2)/64) * 8.
```

可与所有 dense pair certificates 的精确字节数比较后购买；该规则不使用经验比例。

五库 fast 的 site 数与逐 pair gradient 完全一致，所有回溯误差为 0。full arc-only 结果：

```text
settled                 106,310,433
sites                     8,947,182
enumerated sites          8,947,182
representation bytes    449,127,056
decode steps             24,482,434
search                   193.036s
naive site decode         48.915s
errors                     0
```

pair 内 memoized traversal 在 fast Toronto/Toronto-new 上把 decode 从 `13.7/21.2ms` 降到 `6.6/6.8ms`。full 未为该常数优化重复长跑；它不改变下面的 consumer-expansion 否决。

## 4. Solver 原型

### 4.1 逐 Pair Gradient Join

第一版仍保存正式 double rows，只额外保存 pair parents；每次 `closed accumulator + pair` 扫 accumulator，并只保留 roots/downhill sites。扩大随机 `500/500`（seed `712951`, `n=8..16,g=6..12`）通过，但 fast20：

```text
wall                  16.834s
formal Test21         12.380s
gradient probes       69,019,565
gradient sites        21,335,004
```

frontier 小于交集，但发现 frontier 仍逐 pair 扫 consumer，入口撤回。

### 4.2 Arc-Batched A

第二版按一个 A accumulator 扫一次下降边，把所有 pair bits 前向分发到 `A(S union pair)`。arc 表示只在其固定字节小于所有 dense certificates 时购买；稠密 MovieLens 和 fast DBLP 保持正式路径。扩大随机 `500/500`（seed `712961`）通过。

fast20 为 `13.245s`，仍慢约 `7%`。它只减少 anchored pair joins，ordinary D recurrence 与 pair doubles 都未改变，因此不足以进入 full。

### 4.3 Arc-Batched Ordinary + A

第三版对每个 ordinary `D(S)` 也只扫描一次，把所有 disjoint pair bits 前向分发到当前 size layer；`3+3` 等非-pair splits 原样保留，所以不违反 Test37 的 rooted 反例。扩大随机 `500/500`（seed `712971`）通过。

fast20：

```text
wall                  14.941s
edge scans            28,106,145
emitted sites         28,953,309
Toronto-new g12 peak  109.7MiB
```

正式 Test21 为 `12.380s`，同条 Toronto-new g12 peak 约 `38MiB`。按 layer 显式保存 `(target mask,vertex,value)` events，把 full 的单消费者 `8.416%` 优势重新乘上了大量 A/D masks，时间与内存都退化。

## 5. 结论

Test49 证明：

1. pair second boundary 可以用共享有向边 DAG 精确表示；
2. 对一个 closed consumer，gradient frontier 在 full 上确实小一个数量级；
3. 但逐 consumer 或逐 target 展开 frontier 不能解决 `3^g` consumer 乘数；
4. 仅删除 pair doubles、仅优化解码或继续调整 rent-or-buy 都不是下一主线。

下一机制必须同时共享两个维度：`pair bits` 与 `A/D consumer masks`。可研究在每条下降 arc 上对 mask family 做符号 subset convolution，或直接生成 half-sized boundary states而不物化中间 pair-extension events。没有这种 mask 维共享前，不再运行 full solver。

Test49 没有直接采用新的论文算法；gradient 支配与 arc-to-pair DAG 是本轮状态表示推导。系统文献检索尚未完成，因此不宣称论文级原创性。
