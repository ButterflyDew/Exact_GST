# Test21：高 D Row Generator 的否决边界

更新时间：2026-07-12。本文记录 merge-seed certificate 的理论审计与负实验。该机制未进入当前 Test21；当前算法见 `test21_anchor_half.md`。

## 1. 原始设想

令图距离闭包为：

```text
C(f)(v) = min_u f(u) + dist(u,v)
```

高 D row 可写为 `D(S)=C(M_S)`，其中 `M_S` 是同根 merge seeds。原设想希望 A 直接与 `M_S` 做有序交集，从而省掉高 D 的全图传播。

## 2. 被否决的闭包交换

曾误认为对已闭包 `F=C(F)` 有：

```text
C(F + C(M)) = C(F + M).        // 错误
```

两点图 `p--q`、边权 1 即给出反例：令 `F(p)=0,F(q)=1`，`M(q)=0,M(p)=INF`。在 `p`：

```text
C(F + C(M))(p) = 1
C(F + M)(p)    = 2
```

直观原因是左式允许 F 与 M 在中间点相遇，右式强制在 M 的 seed root 相遇。闭包不能穿过点加法自由交换。

固定 g13 随机实验也直接漏解：

| seed | iteration | DPBF | generator prototype |
| ---: | ---: | ---: | ---: |
| `712093` | 10 | `42` | `43` |
| `712093`，禁用 generator 保存剪枝 | 5 | `40` | `42` |

因此错误不来自 generator pruning，而来自状态代数本身。该源码已撤回。

## 3. 唯一成立的恒等式

若 `F` 已闭包，则全局最小值满足：

```text
min_v F(v) + C(M)(v) = min_u F(u) + M(u).
```

所以 generator 只能安全用于**最后一个全局 min 侧**，不能直接代替 A recurrence 中的 D row。

利用固定 pivot，选择“包含 pivot 的顶层 D_h masks”为 generator。两张不交 completion blocks 不可能都含 pivot，因此任一完成分解至多一侧是 generator；另一侧先与 A 做 transient bridge 闭包，再通过上述全局恒等式接 generator。该收缩版通过：

```text
random g=2..10  300/300, seed 712101
fixed g=13       30/30, seed 712103
```

但它相对当时的 full Toronto g13 路线从约 `29.8s` 退到 `32.79s`：D6 只从约 `7.52s` 降到 `7.20s`，新增 bridge `1.79s`，且早期上界变弱。说明 D6 的主要成本是 merge-seed 组合，不是最后的图传播。该正确版本也已撤回；后续四块 upper/witness 的当前成绩见主文档。

## 4. 对后续研究的约束

1. 不再使用错误的 `C(F+C(M))=C(F+M)`。
2. generator 不是 future lower；projected exact-D lower 已有 `39 -> 40` 随机反例。
3. 只删除高 row 传播不足以解决 DBLP，必须减少 seed partition/intersection 本身。
4. 若再次使用 generator，必须位于最终 global min，或显式计算目标化闭包；不能把截断 sparse row 当完整闭包函数。
5. 当前更有效的方向是 `test21_anchor_half.md` 中的 D_t 四块 early upper 与 witness lifting，它们在高层 seed 生成前收紧 `best`。
6. 整层 root-major ordered convolution 已证明传统交集存在大量重复 payload 扫描，但 fast20 退化；合法同根 pair 数本身才是下一状态设计必须减少的对象，详见实验归档第 7 节。
