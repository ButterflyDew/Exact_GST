# Test31：Pair-Cherry A Lattice 反例

更新时间：2026-07-12。Test31 是已经撤出源码和构建入口的状态完备性探针。它试图用一个完整 A lattice 和仅 D1/D2 pendant branches 删除所有 D3+ rows。

## 1. 候选状态

固定 permanent anchor，令 `K` 为其余组。候选保留：

```text
A(S,v): 覆盖 anchor 与 S、root 为 v，S subset K
D({i},v), D({i,j},v): singleton / pair pendant branch
```

并只允许：

```text
A(S-Y,v) + D(Y,v), 1 <= |Y| <= 2
随后做图闭包
```

A masks 扩展到全部 `2^(g-1)`，ordinary rows 则从 D1--D6 缩成 D1/D2。候选没有 Hash、参数或数据特判。动机是把 anchor-rooted tree 二叉化后反复摘除最深的一或两个 token “cherry”。

## 2. 决定性反例

第一轮随机即失败：

```text
seed       712621
iteration  39
n / m / g  9 / 13 / 10
DPBF       74
Test31     75
```

复现命令的随机范围为 `n=4..10, g=2..10`。反例查询为：

```text
1: {1,4,6}   2: {7}       3: {2,7,9}   4: {3,4,9}   5: {2,5,7}
6: {1}       7: {8}       8: {5,6}     9: {3}       10:{7,9}
```

root-star 根为 7，farthest permanent anchor 是第 7 组 `{8}`。唯一最优树权重为 74，边为：

```text
8--2 (9)
2--1 (23)
1--7 (3)
2--3 (17)
3--4 (16)
4--5 (6)
```

它从 anchor 路径 `8--2` 分成左右两条均覆盖多个组的链。

## 3. 已排除的实现因素

以下版本都在同一反例上返回 75：

1. 只使用 Test21 的 root-irreducible D2 branch values；
2. 改为读取完整 D2 rows，包括 local split 与平价值；
3. 关闭 Test31 A 阶段的 future/best pruning；
4. 同时关闭 D2 阶段的 future/best pruning。

此外使用独立短程序重新实现完整公式，不复用 Test21 的 branch bit、剪枝、row 存储或 completion，也得到 75。失败属于 recurrence 本身。

## 4. Cherry 归纳为什么失效

左链可在顶点 1、7 命中多组，右链可在 3、4、5 命中多组。pair cherry 可以从叶端逐个摘除，但逆序接回时会遇到重新定根问题：

- `A(S,2)` 接入 rooted-at-2 的左侧 pair branch 后，树已经包含付费边 `2--1`；
- 为了在顶点 1 接入更深的下一只 pair cherry，需要状态把 root 改成 1；
- 单边界 rooted DP 的图闭包再次支付 `2--1`，因为 `A(S,v)` 不记录哪些内部顶点已经属于树。

因此“存在 pendant cherry”不足以推出只用 D1/D2 的 rooted recurrence。大 D block 的作用不仅是同时覆盖更多 groups，还把一整条多分叉 ordinary component 作为一个 attachment 单元，避免在其内部反复改根。

## 5. 结论

Test31 的源码、目标、诊断分支和临时目录均已删除；未运行任何数据集 benchmark 或 full DBLP。

Test30 与 Test31 共同证明：

- local split seed 丢失 split/attachment 路径；
- 即使保留完整 propagated D2，单根 A lattice 仍丢失“已付费内部 root”信息；
- 删除高阶 D rows 需要一个可表示已覆盖主干内部 attachment 集合的证书，而不只是第二个当前 root；
- 显式顶点对或 separator 枚举会带来不可接受的 `n^2`/`n^q` 因子。

下一机制应研究可合并的 path/attachment certificate，且必须先证明它比 ordinary D row 更小；否则只是把 D2 payload 换一种编码。
