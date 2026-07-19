# Test91：补集成对的顶层 D 顺序（已撤回）

更新时间：2026-07-15。Test91 是 Test90 的顺序扩展：当 `g` 为奇数、A0 completion 的两侧均为 `D_h` 时，把每个 mask 与其补集连续生成，使第一个 partition 在第二张顶层 row 后就能完成。机制正确但跨库时间回退，故只撤回顺序改动，保留 Test90 的流式 completion。

## 1. 机制与正确性

同层 D_h rows 只依赖更小的 ordinary rows，彼此没有状态依赖，因此可按任意拓扑顺序生成。Test91 按较小整数 mask 枚举无序补集 pairs，并依次运行 `S,N-S`。每个 pair 的第二张 row 立即触发 Test90 completion；状态定义、单 row 候选与最终答案不变。

Release/O2 随机 seed `715411` 为 `200/200`。Toronto g13 q1 单次 wall 为 `8.038066s`；相对 Test90 中位运行，D6 values 从 `198,478` 降到 `161,284`，pops 从 `275,078` 降到 `221,836`。这说明更早 completion 确实可能加强后续剪枝。

## 2. 跨库否决

两次 fast20 分别为 `8.495s/8.501s`，均慢于保留数值 mask 顺序的 Test90 `8.334301s`，退化约 `1.9%--2.0%`。多数 odd-g 查询没有任何 A0 更新，D_h 状态也完全相同，但 MovieLens/Toronto-new g11 出现稳定的访问顺序成本；补集跳跃破坏了原来相邻 masks 对低层 row 和工作数组的局部性。

Toronto 的单例正收益不足以覆盖跨库反向，因此不能按数据集启用成对顺序。代码已恢复为原整数 mask 递增，不运行 DBLP gate；两份失败 fast 快照、Toronto 临时结果和随机目录均已清理。

本轮没有新增论文引用。
