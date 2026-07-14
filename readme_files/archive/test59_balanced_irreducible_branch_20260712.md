# Test59：Balanced Irreducible Branch 定向

更新时间：2026-07-12。Test59 检查 ordinary `D(S,v)` 的高层 recurrence 能否通过“总把较小根分支作为 branch”减少大 branch row 的读取。该定向有完整证明，随机 `500/500` 精确；但 fast20 的 rooted payload 几乎不变，总时间从正式基准 `12.380s` 增至 `13.016s`，因此代码撤回，不触发 Toronto full 或 DBLP full。

## 1. 定理

固定一个在根 `v` 可拆的最优 `D(S,v)` 树。删除 `v` 后，把每个含 group token 的连通分支递归拆到根不可拆分分支。若至少有两个分支，则其中最小者 `B` 满足：

```text
|B| <= |S| / 2.
```

其余分支的 union 在同一根给出一个合法 `D(S-B,v)`，所以：

```text
D(S,v) = D(S-B,v) + branch_D(B,v)
```

对某个 `|B|<=|S-B|` 的根不可拆分 `B` 成立。反向每个候选仍是两棵同根真实树的 union；随后做普通图闭包，因此 recurrence 精确。

枚举时每个无序 split 只取较小侧；等分时取不含固定 pivot 的一侧消歧。这不是按 `g`、层、密度或数据集设置的规则，也不增加 split 数量。

## 2. 实现消融

实验只替换正式 Test21 的 ordinary split orientation：

```text
旧：accumulator 固定包含 pivot，branch 可接近 |S|-1
新：branch 取较小侧，等分时由 pivot 消歧
```

dual、incumbent、row storage、root-irreducible bits、A recurrence、三/四块上界和完成式均不变。所有运行使用 Release/O2。

正确性：

```text
DPBF random compare    500/500
seed                   713231
n                      4..14
g                      2..13
tolerance              1e-6
```

## 3. Fast20

候选快照为 `result_snapshot/fast/20260712_221701`：

| metric | formal Test21 | Test59 | ratio |
| --- | ---: | ---: | ---: |
| total query time | `12.380s` | `13.016s` | `1.051x` |
| ordinary values | `6,465,679` | `6,465,801` | `1.00002x` |
| ordinary branch values | `5,485,471` | `5,484,667` | `0.99985x` |
| ordinary pops | `10,516,333` | `10,515,571` | `0.99993x` |
| anchored values | `1,291,783` | `1,291,755` | `0.99998x` |
| completion checks | `60,501,896` | `60,501,168` | `0.99999x` |

微小 payload 差异来自相同浮点候选的访问顺序与 incumbent 到达时刻，不构成状态缩减。ordinary 累计时间反而由 `5962.762ms` 增至 `6162.300ms`。

## 4. 结论

balanced branch 定理成立，但当前 `D` row 仍计算同一批 exact rooted values并做同一图闭包。它只更换等价 split 的 orientation，没有改变状态语义、消费者或输出规模，因此不可能解决 DBLP 的高层 exact-state 瓶颈。

后续不得再把 pivot、较小侧、较大侧或 split 枚举顺序调整包装成新主线。下一候选必须删除一族 rooted states、把它们隐式因子化，或改变 half completion 所需的接口。

撤回后正式 Test21 已重新通过 Release/O2 编译及随机 DPBF 对拍 `100/100`（seed `713241`）；候选源码、开关和实验统计字段均未保留。

Test59 没有采用新的论文算法。其背景仍是 Dreyfus--Wagner rooted subset recurrence；balanced irreducible orientation 是本轮仓库内的简单结构观察，不宣称论文级原创性。
