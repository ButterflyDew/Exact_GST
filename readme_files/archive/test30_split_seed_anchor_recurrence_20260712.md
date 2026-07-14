# Test30：Split-Seed A Recurrence 反例

更新时间：2026-07-12。Test30 是已经撤出源码和构建入口的状态语义探针。当前 Test21 仍见 `../test21_anchor_half.md`。

## 1. 假设

Test21 的 A recurrence 当前使用：

```text
A(X,v) + D(Y,v)
```

其中 ordinary `D(Y,v)` 只读取 root-irreducible branch values，即图闭包把局部 split 值沿一条 degree-1 路径传播到 `v` 后得到的值。

Test30 检查一个更强的假设：能否只读取 D row 在图闭包前的 local split seeds。直觉是先把其它 ordinary branches 并入 A 主干，再让 A 沿连接路径走到 D 的第一个分叉点，从而不必在 A 侧保存 propagated D branch。

原型仅在独立编译宏下为每张 D row 保存排序的 split `(root,value)` 列表，并把 A+D join 的 ordinary 侧替换为该列表。D recurrence、上界、下界和 completion 均保持不变；正式 Test21 从未修改。

## 2. 随机证据为何不充分

第一轮较小随机图全部通过：

```text
g=2..10    1000/1000, seed 712601, n=4..10
fixed g13    100/100, seed 712603, n=4..10
```

扩大图规模和组数后很快出现反例：

```text
command    gst_random_compare ... Test30 712611 1000 8 16 6 12 ...
iteration  55
n / m / g  16 / 55 / 11
DPBF       35
Test30     36
```

随机生成器的完整反例由上述 seed、参数和 iteration 可确定复现；失败运行保留目录 `.tmp_random_compare_test30_wide` 只用于提取信息，归档完成后删除。

## 3. 数据集反例

在发现小反例前，Test30 已完成一次 fast20 质量检查；临时快照编号为 `20260712_121527`，提取下列证据后已删除。其中 Toronto g12 q1 给出：

```text
Test30       0.9688685500
Test21       0.9616227800
ReleaseV3    0.9616227800
```

Test21 的同机临时复跑编号为 `20260712_121649`，提取证据后同样删除；历史正式 Test21 与 ReleaseV3 仍均保存 `0.9616227800`。因此 Test30 丢失了合法更优树，不是浮点或运行波动。

Test30 的 fast20 wall 为 `13.553s`；同机 Test21 为 `13.059s`。虽然探针把 anchored values 从 `1,291,783` 降到 `1,161,937`，该下降来自删除必要状态，不能作为性能收益引用。

## 4. 反例解释

ordinary branch 的 local split root 与它接入 A 主干的 attachment root 可以不同。两者之间的路径已经属于 ordinary branch。只保留 split seed 时，单边界状态 `A(S,v)` 只能选择：

1. 把 A 的 root 移到 split root，但可能失去后续仍需在 attachment root 接入的兄弟分支；或
2. 返回 attachment root，从而重复计算已经使用的连接路径。

小图和 Toronto 反例证明，不能总通过改变合并顺序消除这个第二边界。propagated D branch 正是在一个 root 坐标中隐式保存“split root 到 attachment root”的开放连接。

因此以下替换不完备：

```text
propagated D branch -> local D split seed
```

若要完全删除 ordinary D closure，状态必须额外记住 attachment/split 两个边界，或提供与之等价的共享主干证书。显式 `(u,v)` 会产生 `n^2` 状态，不符合 DBLP 目标。Fuchs 等人的小组件算法通过猜测 separator terminals 获得更低的终端指数，但引入 `n^q` 边界枚举；Test21 不把这条已有路线包装成新机制。

## 5. 结论

Test30 的源码宏、CLI、snapshot 方法和构建目标已删除，未运行 Toronto full 或 DBLP full。

下一候选必须同时满足：

- 表达 split root 与 attachment root 之间可共享且只计一次的主干；
- 不显式枚举任意顶点对；
- 仍可由离线有序列表生成，不退回全局 Hash；
- 在 fast 结构探针中先证明减少 D2 rooted components，再考虑 full DBLP。

相关原始文献：

- [Dijkstra Meets Steiner](https://arxiv.org/abs/1406.0492)：一边界 rooted states、future costs 与 label-setting 框架。
- [Fuchs et al.](https://doi.org/10.1007/s00224-007-1324-4)：猜测 separator terminals 并拼接小组件的参数化 exact 路线。
