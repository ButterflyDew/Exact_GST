# Test138：Quarter 2+2 双汇合点上界

更新时间：2026-07-16。Test138 在 Test121 的 permanent-anchor A 框架中，把已有 quarter witness 从 `1+3` 单最终汇合点扩展为 `2+2` 双汇合点。候选只用已经完成的普通 D rows 和原图最短路构造真实可行树，因此只降低 incumbent，不改变 D/A 状态定义、root-irreducible branch、离线有序行或 exact completion。固定 fast20 与 Toronto g13 q1--q5 均没有产生状态收益，故实现、CMake 目标和临时结果已撤回，没有启动 DBLP。

## 1. 结构动机

令非锚组被当前 quarter 阶段选出的四个互不相交 blocks `B1,B2,B3,B4` 覆盖，每个 block 的大小不超过 `ceil((g-1)/4)`。原 Test80 quarter lifting 选择其中一个 block 与 permanent anchor 在根 `u` 汇合，做一次图闭包后，再让其余三个 blocks 在同一个最终根 `v` 汇合。它表达的是：

```text
anchor + B1  -- shortest path -->  B2 + B3 + B4
```

Test138 允许两侧各自先共享一个汇合点：

```text
anchor + B1 + B2 at u
          |
          | shortest path(u,v)
          |
       B3 + B4 at v
```

对一种锚侧二块选择 `{Bi,Bj}`，其精确定价为：

```text
min over u,v {
    dist_anchor(u) + D(Bi,u) + D(Bj,u)
    + dist(u,v)
    + D(Bk,v) + D(Bl,v)
}.
```

四个 blocks 共有六种锚侧二块选择，全部枚举；这不是固定数据集、固定 `g`、层号、密度、wall time 或经验宽度特判。四块 partition 仍由原 quarter 的 root-star 标量 DP 确定，Test138 只扩大该 witness 的连接拓扑，没有枚举或拟合额外 partitions。

## 2. 求值与正确性

对固定方向，先把左侧真实树代价作为多源种子，做一次最短路闭包；闭包在右侧两张 D rows 的共同根上结算。搜索使用“右侧两张 row 的独立精确最小值”与现有 directed-cut/tour future 的最大值作为一致下界，只跳过不可能严格改善 incumbent 的队列项。

任意有限左种子对应 anchor、`Bi`、`Bj` 的真实树；右侧同根和对应覆盖 `Bk`、`Bl` 的两棵真实树；闭包路径是原图真实最短路。三部分并集连通并覆盖全部组，真实并集成本不超过公式中的求和。因此每个 Test138 候选都是合法 GST 上界，写回后仍可安全用于原有 `partial+lower>best` 剪枝。

Release/O2 与 DPBF 在 `1e-6` 内一致：

```text
wide g=5..15   seed 717701   300/300
fixed g=15     seed 717702   100/100
```

## 3. 固定短面板

fast20 覆盖五个数据版本、g9--g12 的预先固定首询问。只有 Toronto g9 的中间 incumbent 从 `0.904839` 降到 `0.899369`；其余已执行方向均无更新。二十条的普通 D values 与锚定 A values逐项完全不变：

| 指标 | Test121 | Test138 | 变化 |
| --- | ---: | ---: | ---: |
| solver time 合计 | `8.056957s` | `8.109168s` | `+0.65%` |
| D values | `2,774,477` | `2,774,477` | `0` |
| A values | `1,262,600` | `1,262,600` | `0` |

Toronto g13 q1--q5 在同一进程内运行。五条询问共执行 `18` 个非空双汇合方向，逐条均为零更新；D/A 状态继续与 Test121 逐项相同。合计 query wall 为 `47.409514s -> 48.734021s`，回退 `2.79%`。逐询问权重全部一致，分别为 `0.7048467020、0.7275272453、0.7985684033、0.6904989514、0.9105249531`。

## 4. 否决结论

`2+2` 拓扑严格扩展了原 witness 的连接形状，但仍只对**一个由 root-star 标量选择的四块 partition**定价。两个固定跨库/跨询问面板都没有减少任何正式状态，说明当前缺口不是把同一四块从 `1+3` 改成 `2+2` 就能补足，而是需要同时保留 partition、block 根与后续共享路径的联合相关性。

按长门纪律，本候选没有运行 DBLP g13、DBLP g15 D4 或完整 q33。后续不再枚举同一 witness 的固定汇合方向；若重访双接口上界，必须提供可批量共享多个 partitions 的无参数 profile 或停止证书，不能增加固定数量的拓扑模板。

本轮没有新增论文引用。双汇合公式只是已有真实 rooted D trees 与最短路的并集，不据此声明论文原创性。
