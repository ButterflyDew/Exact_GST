# Test69：Pair Path-Profile Factorization

更新时间：2026-07-13。Test69 检查能否把 ordinary D2 的 pair 维从每个 `(pair,attachment)` value 改为共享 `(split root,attachment)` path profile，再利用 pair seed 的 rank-1 结构同时处理 consumer masks。该表示与代数恒等式均精确；fast DBLP 一度显示 `13.3x` 状态共享，但 full DBLP 只有约 `2.0x`，不足以承担 profile 级 subset transform。临时模式已撤回，正式 Test21 未修改。

## 1. Exact Path Profile

pair row 满足：

```text
D({i,j},v) = min_x [dist(x,v) + gd_i(x) + gd_j(x)].
```

对某个 pair predecessor state，沿 parent path 恢复其 split root `x`。把所有拥有相同 `(x,v)` 的 pair states 合并为一个 profile：

```text
P(x,v) = (dist(x,v), gd_1(x), ..., gd_k(x)).
```

该 profile 可同时给出任意 pair 的合法值。对每个 exact `D({i,j},v)`，至少一个实现其最小值的 profile 被收集；反过来每个 profile 给出的都是一棵真实 pair attachment。因此对 profiles 取 min 与完整 D2 row 逐 pair等价。

## 2. 两次 Singleton Transform

固定 profile，令 `a_i=gd_i(x)`、`c=dist(x,v)`，并令 `F(S)` 是同一 attachment `v` 上的 consumer mask 值。向 `S` 加入两个不同 groups 的 pair transform 为：

```text
G(T) = min(i in T) F(T-{i}) + a_i
H(T) = min(j in T) G(T-{j}) + a_j + c
```

展开第二式即：

```text
H(T) = min({i,j} subset T) F(T-{i,j}) + c + a_i + a_j.
```

所以一个 profile 可把 naive `O(k^2 2^k)` pair-mask join 降为两次 `O(k 2^k)` singleton transform；这正面同时共享 pair bits 与 consumer masks，不需要 Hash。

## 3. 有序 Profile 统计

probe 对每个 pair 按 attachment vertex 产生有序 `(v,x)` list，再与全局 unique list 线性归并；full 不保存 `106M` 个全局 keys，也不使用 Hash。随机 predecessor decode `1000/1000`（seed `713471`）无错误。

五库 fast g12 q1：

| dataset | pair states | unique profiles | profiles / states | states / profile |
| --- | ---: | ---: | ---: | ---: |
| Toronto | `130,461` | `62,185` | `47.67%` | `2.10` |
| Toronto-new | `184,459` | `96,950` | `52.56%` | `1.90` |
| DBLP | `126,399` | `9,499` | `7.52%` | `13.31` |
| DBLP-new | `21,785` | `5,427` | `24.91%` | `4.01` |
| MovieLens | `19,206` | `2,231` | `11.62%` | `8.61` |

fast DBLP 的强信号满足一次 full 结构测量门槛；该运行不进入 Test21 solver，也不产生 weight。

## 4. Full DBLP g13 q1

Release/O2，known exact `12.5936282853`，与 Test21 相同的 root-star/farthest anchor，只统计 66 个 nonanchor pairs：

```text
settled pair states       106,310,433
unique path profiles       53,171,209  (50.015%)
states per profile              1.9994
unique split roots            142,495
forest roots                   506,751
average / max depth             2.780 / 7
search                        216.032s
decode                         14.964s
total                         265.656s
certificate errors                  0
```

full 的 profile 共享仅约 `2x`，与 fast DBLP 的 `13.3x` 明显不同。即使只按 unique profiles 运行两次 singleton transform，`53.17M * O(k2^k)` 也远大于当前可接受工作；profile 数本身也没有达到后续 exact family 所需的数量级下降。

## 5. 结论

rank-1 pair profile 与双 singleton transform 是干净的 exact 代数，但 split-root identity 在 full 上高度分散。它压缩 pair 数值维，却把昂贵的 mask transform 绑定到 5300 万个几何 profiles；不能解决 Test49/50 的 consumer 乘数。

因此不集成、不运行 full Test21，并撤出 `--root-profiles`、root labels、统计字段和临时输出。后续候选若继续 factorization，必须先在 profile 之上再共享 attachment geometry，而不能仅依赖同 split root 碰巧复用。

Test69 没有采用新的论文算法；path-profile rank-1 分解和双 singleton transform 是本轮仓库内推导，尚未完成系统文献检索，不宣称论文级原创性。
