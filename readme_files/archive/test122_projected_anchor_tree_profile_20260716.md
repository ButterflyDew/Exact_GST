# Test122：rooted row 的锚树投影 profile（已撤回）

更新时间：2026-07-16。Test122 尝试加强 Test121：不再只读取压缩锚树节点 `x` 本身的 `D(B,x)`，而把每个普通 row 顶点沿 junction 的最短路父树投影到最近压缩节点。机制正确，但在预先声明的跨询问 panel 上与 Test121 的候选逐项相同，源码和构建目标已撤回。

## 1. 方法与理论关系

对图顶点 `v`，令 `owner(v)` 是其到永久锚路径的父树路径上第一个压缩节点，`delta(v,owner(v))` 是对应树路径长度。对每个 block `B` 和压缩节点 `x` 定义：

```text
c_x(B) = min { D(B,v) + delta(v,x) : owner(v)=x }.
```

Test121 的 `D(B,x)` 是其中 `v=x` 的特例。任意 Test119 attachment 根也会被投影到某个 `x`，随后由 Test121 的压缩树卷积接到锚路径；因此该 profile 理论上同时不弱于 Test119 标量设施和 Test121 定点设施。实现把归约融合进已有 sorted row 存储扫描，每个 value 只更新一个 owner cell；额外空间为 `O(n+t*2^h)`，树 DP 仍为 `O(t*3^h)`。

## 2. 证据与否决

Release/O2 黑盒对拍通过宽范围 seed `717231` 的 `100/100` 和固定 `g=15` seed `717232` 的 `30/30`。随后使用与 Test121 相同的 Toronto `g13 q1--q5` D3 panel。五条询问在 D1、D2、D3 的 `U_tree`、incumbent、values、branches、pops 与 row bytes **全部和 Test121 相同**；例如 q1 的 D2/D3 上界仍为 `0.813762/0.804423`，q5 仍为 `1.10158/1.09767`。

投影 profile 没有改变任何状态，却需要 `owner/distance` 全图数组、每个 block-node 的 attachment 表和额外 Store-row 写入。由于跨询问短门没有新增结构信号，按长跑纪律没有启动 DBLP q33。保留的负结论是：在当前 junction 压缩树上，Test121 最优方案所需的有效 rooted block 已能在压缩节点直接取得；扩展到整棵父树的最近节点投影没有价值。

本实验没有新增论文引用。临时输出不作为主线 benchmark，清理后只保留本文件的结论。
