# Test37：固定小 Branch Recurrence 的 Rooted 反例

更新时间：2026-07-12。Test37 是独立短程序中的状态定理检查，没有进入仓库 solver。目标是判断 ordinary `D(S,v)` 是否只需反复接入 singleton 或 pair branch，从而删除高阶同根 subset splits。

## 1. 候选递推

标准 row seed 枚举所有非平凡二分：

```text
seed(S,v) = min D(X,v) + D(S-X,v).
```

候选只保留 `min(|X|,|S-X|)<=b`，随后仍运行完整图最短路闭包。`b=1` 对应 singleton-extension，`b=2` 对应只剥离 singleton/pair pendant branches。

## 2. Singleton 反例

在 singleton terminals 的随机图上，seed `712740` 很快得到：

```text
n=9, g=8
exact=36
singleton-extension=38
```

根本原因是从一个 leaf group 向指定 root 做闭包时，路径可能与已经支付的主干重叠；标量 row 不记 paid path，逐个剥离会重复计边。这与 Test31--32 的 attachment 缺口一致。

## 3. Pair 宽度的假正信号与 Rooted 否决

只比较自由根最终值时，`b=2` 曾在 10,000 个随机 group-query 上全部一致。这是误导性弱口径：Test21 后续需要每个 `(mask,root)` 的精确值。

改为逐 mask、逐 root 比较后，第 11 个实例立即失败：

```text
seed=712761
n=9, g=7
mask=63, root=6
exact D=44
pair-bounded D=45
```

精确值在该 root 的最优 seed 是 `3+3` split；pair-bounded recurrence 无法由其它 root 的闭包补回。该值在当前 root 可拆分，因此也说明“只保留小 branch”不能直接生成供更高 row 使用的完整 accumulator。

## 4. 结论

- 自由根最终值一致不能证明 rooted row recurrence 完备；
- 固定 `b=1/2` 都不足以替代 ordinary D 的一般同根 splits；
- 若只保留 root-irreducible branches，还必须提供不物化 decomposable accumulator 的新组合语义；当前 A+D+D completion 不具备该能力；
- 不建立 Test 入口，不运行数据集 benchmark，不保留短程序。

