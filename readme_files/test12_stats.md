# Test12 输出字段

Test12 是带诊断计数的半集合 DP 求解器，用于分析 Dijkstra 前缀入堆与 promoted target 的同根处理。

## 算法要点

**前缀入堆更严**：非目标点除 `dp < mx_used` 外，还要求 `dp + LB(v, U^mask) <= best` 才入堆。目标点规则不变。

**promoted target 延迟**：Dijkstra 中由边松弛新成为 target 的点（非初始 target_seed）不立即 `Modify`，记入 `pending_promoted`；当前 mask 的 Dijkstra 结束后，按顶点去重、取最小 dp，再批量 `Modify`。

**Modify 快速路径**：正式枚举前做 precheck；若 best / live-dp / future-h 三类均无候选，直接返回（`fast_no_candidate`）。

**Modify 分类**：`modify_kind = 0/1/2` 分别对应初始化、原始 target、延迟后的 promoted target，便于统计各来源的效果。

## 字段

### 规模与合并

```text
valid_total          调用 Modify 的状态数
up_subset            live-dp + future-h 实际候选数
inqueue              Dijkstra 中实际处理的弹出次数
bucket_scan          扫描 root bucket 总长度
live_dp_checks       live-dp 候选数
future_h_checks      future-h 候选数
prune_ge_best        cand >= best 剪枝
prune_far            cand + far >= best 剪枝
```

### 入堆

```text
target_seed          初始目标入堆数
prefix_considered    满足 dp<mx_used 的非目标候选数
prefix_pushed        实际入堆的前缀数
prefix_pruned_lb     因 dp+LB>best 未入堆的前缀数
pq_push / pq_pop
pop_target           弹出时为 target
pop_prefix           弹出时为前缀点
pop_skip_stale       stale 弹出
pop_skip_lb          dp+LB>best 弹出
```

### target 传播

```text
target_promoted      由 target 路径传播得到的新 target
target_demoted       原 target 被非 target 更短路径覆盖
modify_original      原始 target 触发 Modify
modify_promoted      promoted target 触发 Modify（延迟后）
modify_init          初始化 Modify 次数
```

### Modify 效果

```text
modify_calls
modify_no_effect     三类均无实际更新
modify_best_effect / modify_live_effect / modify_h_effect
modify_only_h        仅更新 h
modify_only_live     仅更新 live-dp
modify_only_best     仅更新 best
modify_multi         同时更新多类
```

按来源细分：`init_no_effect / init_only_h`，`original_*`，`promoted_*`。

### 候选预判

```text
no_best_cand / no_live_cand / no_h_cand
no_cand_all          三类均无候选
fast_no_candidate    precheck 后直接返回
precheck_calls / precheck_scans / precheck_no_candidate
```

### promoted 延迟

```text
pending_promoted         pending 总条数
pending_unique           去重后 Modify 次数
pending_layer_roots      同一 popcount 层出现 pending 的 root 数
pending_layer_repeat     同层重复 root 次数
max_pending_roots_layer  单层最大 pending root 数
```

### 时间与松弛

```text
prep_ms / dp_ms
relax_try / relax_ok
```

分桶字段：`valid_k / total_k / active_k / inqueue_k / merge_k`（按 mask 的 popcount 分桶）。

## 读法

- `prefix_pruned_lb` 大：前缀 `dp+LB>best` 过滤有效，可减少无效入堆。
- `modify_no_effect` 或 `fast_no_candidate` 大：大量状态可跳过同根枚举。
- `pending_layer_repeat` 大：同层 promoted target 可按 root 进一步批处理。
- `promoted_only_h` 高而 `promoted_no_effect` 也高：延迟 Modify 主要产生 h 证据，对 dp 贡献有限。
