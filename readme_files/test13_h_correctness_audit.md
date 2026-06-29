# Test13 的 h 剪枝正确性审计

## 结论

当前 Test13 的 h 剪枝不正确。问题不是集合单调性本身，而是实现不能保证所有进入
`confirmed` 的值都是真实的 rooted DP 最优值。

存在小图实例满足：

```text
DPBF                 55
Test13               56
Test13（仅关闭 h）   55
```

因此错误由 h 门控实际触发，而不只是证明中缺少一个尚可补充的引理。

固定实例保存在：

```text
readme_files/test13_h_counterexample.txt
```

说明：早期用于定位污染链的 `gst_test13_fuzz` 是白盒审计工具，依赖
`SolveOneQuery` 的调试参数；当前工程已删除该接口和工具。固定实例仍作为反例证据保留。
后续随机正确性回归使用 `tools/random_compare` 中的黑盒对拍工具，直接生成数据并运行主程序与 DPBF。

## 1. h 在什么前提下正确

记真实 rooted DP 为：

```text
D*(S,v)
```

若 h 只使用真实值：

```text
h(R,v)=max { D*(T,v) | T subset R }
```

则由集合单调性：

```text
D*(T,v) <= D*(R,v)
```

所以 `h(R,v)<=D*(R,v)`，h 是合法下界。

但是 Test13 实际使用的是算法当前存储值 `Dhat(T,v)`。同根合并缺失或图搜索提前
停止时，只能保证：

```text
Dhat(T,v) >= D*(T,v)
```

从：

```text
D*(T,v) <= D*(R,v)
```

不能推出：

```text
Dhat(T,v) <= D*(R,v).
```

所以 h 的安全性完全依赖额外不变量：

```text
所有 confirmed(T,v) 都满足 Dhat(T,v)=D*(T,v).
```

该不变量在当前算法中为假。

## 2. 为什么 target 弹出不能证明全局精确

对固定 mask 的 Dijkstra，target 弹出只证明其值相对于“本轮已生成的初始种子”已经
定型。真实 `D*(S,v)` 的最优分解可能依赖某个此前被 h 排除、没有执行 `Modify` 的
子状态；该子状态不会更新超集，因此本轮种子集合可能缺少真实最优种子。

于是可能发生：

1. 小状态 A 被 h 排除，不执行 `Modify(A)`；
2. 某超集 T 缺少由 A 产生的最优种子；
3. T 仍因较弱的 h 成为 target；
4. Dijkstra 将 T 相对于不完整种子集合的偏大值弹出；
5. `MarkConfirmed(T)` 把偏大值标记为可信；
6. 后续 h 把该上界当成下界使用。

这不是普通的 Dijkstra 一致性可以修复的问题：Dijkstra 只保证给定源集合下的最短
距离，不能补回未生成的源。

## 3. 固定反例中的污染链

在固定反例中，程序直接用完整 DP 表核对每次 confirmed：

```text
confirmed_overestimate = 29
首次污染：
mask=144, vertex=4
Test13 value=25
exact value=23
```

随后出现 13 次 h 超过真实补集 rooted DP。第一次为：

```text
current mask=11, vertex=7
h=32
exact complement DP=28
```

一个直接由 h 错误排除的状态为：

```text
current S=73, vertex=10
Dhat(S,v)=29
D*(S,v)=29
current best=56

h(U-S,v)=33
D*(U-S,v)=27
```

因此代码执行：

```text
29+33 > 56
```

并把该状态排除；但使用真实补集值时：

```text
29+27 = 56
```

按照 Test13 保留等号的规则，它本应成为 target。

这个错误 h 的最大值证据来自：

```text
witness T=144
Dhat(T,10)=33
D*(T,10)=27
T subset U-S
```

也就是说，集合包含关系没有问题；问题恰好是 witness 自身已经被偏大值 confirmed。

最终：

```text
DPBF                 55
Test13               56
仅关闭 h 的 Test13   55
```

## 4. 随机对拍

历史上曾使用白盒 fuzz 工具直接检查 `confirmed` 和 h witness，并观察到：

```text
seed=7, mode=1:
iteration 113 首次发现 confirmed 污染

seed=7, mode=2:
iteration 295 首次发现 h 超过真实补集 DP

seed=314159265, mode=0:
iteration 67145 发现最终答案 55 / 56 不一致

同一随机序列使用 mode=3（关闭 h）继续检查 100000 个实例：

```text
ALL_OK seed=314159265 iterations=100000
```

这不能替代关闭 h 后的完整正确性证明，但支持固定反例中的因果隔离结论。

当前工程已删除该白盒工具。后续回归使用独立黑盒工具：

```text
gst_random_compare <method_exe> <dpbf_exe> <method_name> ...
```

它只检查最终答案与 DPBF 是否在 `1e-6` 内一致。对 h witness 是否精确的内部断言不再放在
solver 正式接口中；若后续需要重新做白盒证明审计，应作为单独审计工具实现，不应污染
`SolveOneQuery` 接口。

## 5. 后续修正方向

不能继续把“被 target 弹出”直接等同于“全局精确”。安全选择只有两类：

1. h 只读取由独立方式证明精确的状态；
2. 改变剪枝/依赖闭包，使所有可能成为 h witness 的状态都先完成精确求解。

在给出新的闭包证明前，最直接的正确性修复是禁用当前 h target 门控；固定反例表明
仅关闭 h 后即可恢复正确答案。但这只是安全回退，不代表性能上已经得到最终方案。
