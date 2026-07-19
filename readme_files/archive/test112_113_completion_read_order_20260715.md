# Test112--113：Completion 第二分量读序（已撤回）

更新时间：2026-07-15。Test112--113 在 Test107 的第三级 component minimum 上检查两个无经验参数的物理读序：只在未读侧为多组 D 行时启用证书，以及先读取直接访问分量、把较重的有序行查询留到证书之后。两版均通过正确性门禁，但没有降低 completion 时间，源码已恢复为 Test107；没有为它们运行 Toronto g13 或真实 DBLP。

## 1. Test112：只保护多组查询

Test107 在左右两侧都非空时，用已读精确分量与另一行最小值决定是否跳过第二次读取。若未读侧只是 singleton，则被省掉的是一次 group-distance 数组读取。Test112 只在未读侧为多组 D 行时执行该证书。

Release/O2 随机门禁为宽范围 seed `716961` 的 `100/100` 与固定 g13 seed `716962` 的 `50/50`。fast20 中 component rejects 从 `5,201,313` 降至 `5,175,827`，最终 checks 相应从 `2,271,708` 增至 `2,297,194`；completion 为 `219.063ms`，与 Test107 的 `219.400ms` 无可辨别差异。它只把一次比较换回一次直接数组读取，没有形成工作量优势。

## 2. Test113：直接分量优先

Test113 恢复完整第三级证书，并利用两个普通分量的对称性：当一侧为 singleton/dense、另一侧为非直接布局时，先读取直接侧，再用非直接侧的最小值拒绝，目标是把 sparse/bitmap 查询留到证书之后。该顺序不改变 partition、候选或答案，也没有数据集、`g`、层号或密度阈值。

Release/O2 随机门禁为宽范围 seed `716971` 的 `100/100` 与固定 g13 seed `716972` 的 `50/50`。fast20 的 final checks 降至 `2,205,022`，component rejects 增至 `5,273,261`，说明读序确实多避免了 `66,686` 次第二分量读取；但 completion 从 `219.400ms` 增至 `234.796ms`。交换左右行破坏了原 partition 顺序下递增位置的缓存与扫描局部性，逻辑工作下降没有转成时间收益。

## 3. 结论

Test112 证明 singleton 读取与一次 component 证书成本同阶；Test113 证明基于单次查询成本交换分量会损害跨 partition 的有序行局部性。Test107 保留原稳定读序。后续 completion planner 若要改变顺序，必须联合优化整批 partition 的行复用，而不能逐 partition 贪心交换；但当前 q32 completion 已只有 `7.05s`，优先级低于 ordinary D 的 `281.47s`。

Test112--113 没有新增外部论文引用。
