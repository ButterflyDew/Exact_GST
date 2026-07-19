# Test78 后续：Soft-Pendant 隐式接口探针

更新时间：2026-07-13。本文归档 Test78 后对顺序 pendant blocks 的最后一轮小图探针。它没有进入 Test21、Test79 或 ReleaseV3。

## 1. 朴素嵌套反例

候选把 ordered blocks 依次写成：

```text
row <- C(D(B) + row)
```

并枚举 `core<=4` 与三个有序 `3/4` blocks。seed `7801` 的第一个 g9 singleton 实例即得到：

```text
exact  72
nested 78
```

最优树可先抽出一个 4-block，但余树只剩 5 groups；强制固定三个 blocks 不是合法的结构定理。

## 2. Soft-terminal block transform

为隐式保存第二接口，把已付 row `h` 当作一个 soft terminal。对新 block `B` 的全部局部子集运行小型 rooted DP：

```text
F(empty,v) = h(v)
F(X) = C(min_{Y nonempty subset X} D(Y) + F(X-Y))
```

每个 3/4-block 只产生至多 `2^4` 个临时 rows，不显式保存 `(u,v)`，也不使用 Hash。固定三块版本修复 seed `7801`，但 seed `7802` iteration 33 又得到 `54 -> 55`；该 witness 需要 3-block、4-block 和 2-group core，而非三个 3-block。

放开为“任意数量的有序 3/4-blocks + core<=4”后，g9 singleton 为：

```text
seed 7802  200/200
```

这是 2026-07-13 当时的随机证据；当时尚无一般分解证明。2026-07-15 的 Test95 已证明 core 可以进一步收紧到 3，完整定理与新的验证见 `../test95_soft_pendant_decomposition.md`。本段保留用于说明猜想到定理的演化，不再代表当前理论结论。

## 3. 与 Test79 的边界

soft transform 试图让一个 row 隐式携带 paid second interface；Test79 采用的是另一条已证明安全的路线：branch-junction 只构造可行 upper，再把实际工作计入 ReleaseV3 rent-or-buy。后者已经通过 full DBLP，不能反向证明 soft-pendant 完备。

本轮没有新增论文引用。Dreyfus--Wagner 只作为局部 subset recurrence 背景；soft-terminal 组合尚未完成系统新颖性检索。

## 4. 2026-07-15：外层顺序的精确子集 DAG

原探针递归枚举全部 3/4-block 顺序，块数增加时有阶乘重复。由于 `SoftBlockTransform(B, row)` 对输入 row 保持逐点 `min`：

```text
T_B(min(row1,row2)) = min(T_B(row1),T_B(row2)),
```

所有到达同一 covered mask 的顺序可以先逐顶点取最小，再继续扩展。工具现为每个 `covered` 保存一条 pointwise-min row，并沿每个不相交 3/4-block 的子集 DAG 边做一次 transform。该改写精确表示原任意顺序 family，不是近似剪枝，也没有经验参数；最坏外层从排列枚举改为 subset-DAG 枚举。

Release/O2 验证为 g9 singleton seed `7802` 的 `200/200`，fixed g13 singleton seed `715121` 的 `20/20`，fixed g13 双候选组 seed `715122` 的 `20/20`；此前两组 g13 smoke 各为 `5/5`。这部分当时只增强了候选 family 的计算方式。

## 5. 2026-07-15：Test95 完备分解

Test95 随后证明了纯树结构的 3/4 pendant lemma：将 group witnesses 变成零长标号叶并二叉化后，可反复剥离 3/4 单接口 blocks，留下 `core<=3`。但 Test96 的 g13 双候选组反例表明，连续 blocks 的 attachment points 可位于 paid tree 的不同内部位置，标量 rooted row 会重复支付重根路径；固定 group 0 的 soft/bounded-D4 价格为 `73`，exact 为 `71`。所以保留的是树分解引理，不是 soft 标量 family 的完备性。完整边界见 `../test95_soft_pendant_decomposition.md` 与 `test96_bounded_d4_full_a_20260715.md`；Test80 与发行版均未修改。
