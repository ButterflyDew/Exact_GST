# Test35：Anchor-Group Dual 替换实验

更新时间：2026-07-12。Test35 检查 permanent anchor 是否能直接成为 directed-cut dual 的根集合，从而让 Test21 的 lower bound 真正 anchor-aware。候选已被 fast 门槛否决；临时代码、结果目录和二进制改动均不保留，正式 Test21 仍使用 root-star 单根 dual。

## 1. 候选

正式 Test21 在 root-star 根 `r` 上构造 directed-cut dual。Test35 改为把 permanent anchor group `A` 的全部候选点接到一个隐式零代价 super-root，并在同一 residual 上依次抬升各 query group：

```text
root set = vertices(A)
future(v,R) = sum dual_potential[group][v], group in R
```

这不是第二套 dual，也没有数据集、`g`、层级、密度或运行时刻特判。第一版直接用 anchor-group dual 替换单根 dual，因此预处理数量级不变。

## 2. 合法性

任意 GST 至少命中一个 anchor terminal。给所有 anchor terminals 增加零代价 super-root arcs 后，原 GST 可扩展为该松弛问题的可行解；反向不成立，因为松弛允许多个 anchor terminals 通过 super-root 免费连通。因此：

```text
anchor-group dual objective <= super-root optimum <= GST optimum.
```

所以 objective 是自由根 GST 的合法全局下界。`At(v,R)` 仍满足 rooted admissibility、edge consistency 与 subset splice；已有 `dual_cut_probe` 自检扩展为同时核对全局 objective。Release/O2 随机结果：

```text
seed 712735, 300 instances, g<=8, n<=10
objective checks       300
admissibility checks   86,340
consistency checks     151,635
splice checks          8,817,882
result                 ALL_OK
```

Test21 替换版另通过 `g=2..12` 的 `500/500` 和固定 `g=13` 的 `50/50` DPBF 对拍，容差 `1e-6`。

## 3. Fast 否决

正式快照为 `result_snapshot/fast/20260712_050625`。只替换 dual 的 Test35 临时快照为 `20260712_130938`，提取数据后删除。

```text
formal Test21 fast20       12.380s
anchor-group replacement   27.792s
```

DBLP-fast g12 给出决定性分解：

| metric | formal root dual | anchor-group dual |
| --- | ---: | ---: |
| wall | `0.304s` | `2.537s` |
| ordinary retained values | `50,267` | `1,594,118` |
| ordinary pops | `15,270` | `2,101,280` |
| anchored retained values | `26,693` | `167,370` |
| dual time | `13.868ms` | `24.662ms` |

差异主要来自状态膨胀，不是 dual 构造常数。

## 4. 上界与 Lower 的隔离

正式单根 dual 的 residual primal 在 DBLP-fast g12 立即把 `best` 收到精确值 `12.166303`；anchor-group dual 没有对应的单根 primal，初始 `best` 为 `12.710614`。为排除只是上界丢失，又运行了一次诊断版：

1. 单根 dual 只负责提供相同的 primal `best=12.166303`；
2. 所有 future pruning 只读取 anchor-group dual；
3. 其余 Test21 操作不变。

结果仍为：

```text
wall                      2.225s
ordinary retained values 1,173,010
ordinary pops            1,437,816
```

因此即使上界完全相同，anchor-group potential 仍比单根 potential 弱一个数量级以上。原因是 super-root 松弛允许多个 anchor terminals 免费互联，丢失了“当前 rooted component 必须接入同一棵实际 anchor trunk”的几何约束。

## 5. 结论

- anchor-group objective 是合法的自由根全局下界，这一点保留在公共 dual 探针自检中；
- 它不能替换 Test21 的单根 future potential，也不能恢复单根 residual primal；
- 与单根 dual 取 max 的历史方案需要支付第二次 dual，过去已因收益不足被否决；
- 不触发 full DBLP g13。fast DBLP 已显示决定性的状态膨胀，长跑没有信息增益；
- Test35 不进入正式 Test21，不保留开关。

