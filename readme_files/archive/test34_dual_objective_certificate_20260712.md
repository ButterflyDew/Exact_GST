# Test34：Dual Objective 直接返回反例

更新时间：2026-07-12。Test34 是一个已经撤出的 exact-stop 候选，没有独立源码文件；它曾在 Test21 构造 directed-cut dual 后检查 `dual.Objective() == incumbent` 并尝试直接返回。

## 1. 动机

fast DBLP g12 q1 中：

```text
dual objective = dual primal = final best = 12.166303
```

如果 objective 是自由根 GST 的全局下界，上下界相等即可零额外工作证明最优，而且 Test21 已经支付 dual 构造成本。

## 2. 随机反例

该假设在第五个随机例即失败：

```text
seed       712701
iteration  5
n / m / g  10 / 25 / 8
DPBF       27
incumbent  28
objective  28
returned   28
```

查询组为：

```text
{3,4}, {1,5}, {8,10}, {1,10}, {6,9}, {1,4,10}, {8}, {6,7,10}
```

完整图可由 seed、iteration 与随机参数 `n=4..10,g=2..10` 确定复现。

## 3. 原因

当前 dual 以 root-star 根构造。`dual.At(v,R)` 已证明满足 Test21 所需的 rooted future admissibility/splice 条件，但 `Objective()` 不能据此当作不要求包含该根的自由根 GST 全局下界。反例中 objective 比真实 optimum 大 1。

因此：

```text
dual.Objective() == feasible incumbent
```

不是合法的全局停止证书。fast DBLP 的数值相等只是实例现象。

## 4. 结论

提前返回、统计字段和文档候选均已删除。Test21 继续只使用 dual 的 primal upper 与 rooted future potentials，不使用 `Objective()` 证明全局最优。
