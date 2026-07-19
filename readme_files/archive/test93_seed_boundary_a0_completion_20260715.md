# Test93：Seed 边界的批量 A0 Completion（已否决）

更新时间：2026-07-15。Test93 接续已否决的 Test92，但不再为每个 label 做补集二分。它利用“当前 row 的全部 merge seed 已生成、Dijkstra 尚未启动”这一自然阶段边界，把 seed 顶点排序后与已经完成的补集 row 做一次批量有序相交。该改动已完整撤回，当前 Test80 仍是 Test90。

## 1. 方法与正确性

每个 seed 都是两张较小普通 row 在同一根的可行并，因而与补集 row 和 anchor 距离相加后是合法全解。对同一顶点只保留 merge 后的最小 seed；若批量 completion 降低 `best`，建堆时再用原有合法下界过滤 seed。被过滤 seed 满足 `d+H>best`，不能导出更优完整解。row 完成后的 Test90 精确 completion 仍然保留，所以 Test93 既不会漏掉原候选，也不改变状态定义。

实现没有 Hash 和经验批大小。singleton/dense 补集直接访问；sparse 补集与排序后的 seed 顶点做两指针归并。每个 closing partition 只在算法固有的 seed/propagation 边界执行一次。

## 2. 验证与否决

Release/O2 黑盒对拍通过宽范围随机 `200/200`（seed `715511`）和固定 g13 `50/50`（seed `715512`）。fast20 与 Test90 `result_snapshot/fast/20260715_015858` 对比：

| 指标 | Test90 | Test93 | 变化 |
| --- | ---: | ---: | ---: |
| solver 总时间 | `8.261585s` | `8.443499s` | `+2.20%` |
| 顶层 D values | `426,848` | `426,840` | `-8` |
| 顶层 D pops | `680,409` | `680,394` | `-15` |
| seed/partner 扫描工作 | `0` | `674,564` | 新增 |
| seed completion 更新 | `0` | `3` | 新增 |
| 建堆前剪去 seed | `0` | `9` | 新增 |

20 条中只有 2 条减少状态，绝大多数查询完全不变。自然边界消除了 Test92 的随机二分，却仍需扫描大量 seed；这些 seed 形成的上界通常不优于已有 junction、early completion 和前序 Test90 上界。收益不足以支付排序与归并，因此未运行 Toronto g13 或 DBLP。Test92 与 Test93 共同说明：继续细调同一 A0 completion 的触发时机不是当前主线，下一步应寻找能改变候选或状态数量级的新界或依赖结构。
