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

这只是随机证据。当前没有一般分解证明，也没有 g13/大图共享规模结论；递归枚举还可能退化为另一种 subset DP。因此 soft-pendant 保留为理论候选，不进入正式 Test。

## 3. 与 Test79 的边界

soft transform 试图让一个 row 隐式携带 paid second interface；Test79 采用的是另一条已证明安全的路线：branch-junction 只构造可行 upper，再把实际工作计入 ReleaseV3 rent-or-buy。后者已经通过 full DBLP，不能反向证明 soft-pendant 完备。

本轮没有新增论文引用。Dreyfus--Wagner 只作为局部 subset recurrence 背景；soft-terminal 组合尚未完成系统新颖性检索。
