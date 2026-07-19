# Test128：全 Site 的 Paid-Pair 标量包络

更新时间：2026-07-16。Test128 已从 paid-pair 探针、构建依赖和命令行入口撤回。它由 Test127 的 q3/q5 反例直接推出，不使用数据集、固定 `g`、层号、密度、wall time 或经验宽度参数；但在第一个预声明反例 Toronto g13 q3 上就比 Test127 更弱，因此没有继续运行 q5 或任何 DBLP 查询。

## 1. 理论问题

Test127 只把每个 D2 根分配给最近的 anchor-tree site，再在该 site 内维护 `(D2 cost, attachment distance)` skyline。这样会丢失“虽然不以 site `x` 为最近点，但接到 `x` 仍有竞争力”的根。Test128 改为对每个非锚组对 `{i,j}` 和每个 anchor-tree site `x` 精确计算：

```text
E_x({i,j}) = min_v D({i,j},v) + dist(v,x).
```

每个 `<组对,site>` 都恢复一个达到最小值的 D2 见证树及其到 `x` 的最短路。所有见证、永久锚树和锚路径的顶点形成诱导子图，再用 DPBF 精确求该子图中的最佳 GST。所有边都来自原图，因此结果是合法可行上界。

这一包络与 Test122/124 不同：Test122 每个 row 顶点只投影到最近 owner，Test124 只放宽既有 junction sites 间的连接拓扑；Test128 允许每个 D2 根参与所有 sites 的 min-plus attachment。D2 根仍由已知 exact optimum 和安全 TSP 下界过滤，所以这是对结构假设有利的 oracle 筛查，不是在线求解器。

## 2. q3 反例

Toronto g13 q3 的 exact 为 `0.7985684033`。Test128 使用 `54` 个 anchor-tree sites，对 `66` 个非锚组对恢复 `3,564` 个 site choices；大量 choices 取到相同根，最终诱导子图只有 `619` 个顶点和 `794` 条边。结果为：

```text
induced upper   0.8525860548
exact           0.7985684033
gap             0.0540176515  (6.764%)
site prepare    0.592s
pair rows       2.240s
induced DPBF    2.907s
total           5.909s
```

同一询问的 Test127 最近-site skyline 诱导上界是 `0.8011140378`，只高于 exact `0.319%`。因此 Test128 虽然让每个根可以面向所有 sites，却因每个 `<组对,site>` 只保留一个**独立加法最优根**而删除了大量全局有用的次优根，结果反而显著变差。

## 3. 否决结论

`E_x(B)` 只回答“单个 block 独立接到 site `x` 的最小代价”。完整 GST 中，一个较贵的 D2 根可能与另一个 paid block 在到达 `x` 前共享边，或为后续大组件提供更好的第二 attachment；这种收益不会出现在独立的 `D2+dist` 标量中。由此得到两条边界：

1. 最近-site 标签不完备，但把每个 block 扩为所有 sites 的单一标量也不完备。
2. 下一状态接口必须保留**block 之间可共享的 paid attachment profile**，不能继续把每个 block 独立最小化后再相加。

由于 q3 已是决定性反例，按短门协议没有运行 q5。源码撤回后重新构建原 paid-pair 探针；本轮没有启动 DBLP，也没有新增论文引用。DPBF 只作离线核验器，不属于候选贡献。
