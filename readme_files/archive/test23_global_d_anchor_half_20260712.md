# Test23：Global D 转 Ordered Rows 实验归档

更新时间：2026-07-12。Test23 是已经撤回的结构原型。它只把 Test21 的 ordinary D 半格改为全局 key 调度，随后冻结为 mask-major ordered rows，并继续运行原封不动的离线 A+D recurrence。

## 1. 流程

```text
nonanchor singleton terminals
          |
          v
global label-setting over D(S,v), |S|<=h
          |
          | online gd(anchor)+D+D+D upper
          | stop when min open key >= best
          v
transpose settled labels to ordered D rows
          |
          v
offline A(S,v) via A+D
          |
          v
A+D+D completion
```

global D 使用 Test22 已验证的无 Hash 结构：root-local ordered index、disjoint bitmap、direct-directory rent/buy，以及 bitmap/submask 理论工作量二选一。任何未 settled 的 D label 都满足 `cost+full-completion lower>=best`，因此冻结 rows 时省略它不会影响后续 A 阶段的精确性。

## 2. 正确信号

Release/O2 相对 DPBF `1e-6`：

```text
random g=2..8    200/200, seed 712301
random g=2..10   300/300, seed 712303
fixed g=13        30/30, seed 712305
```

Toronto fast g12 中，ordinary row payload 从 Test21 的 `837,468` 降到 `584,461`，说明全局高层上界确实能回头删除一部分低层 D 状态。

## 3. 时间否决

同一 Toronto fast g12：

```text
Test21 offline D        0.895s
Test23 global D         3.613s
Test21 A                0.806s
Test23 A                0.725s
Test23 total            4.374s
Test21 total            1.739s
```

global D settled `565,191` labels、检查 `23.67M` disjoint merges。减少约 30% payload 不能抵消动态 label 管理；A 阶段的小幅收益不足以回本。

fast20 最终为：

| dataset | Test23 | Test21 | ReleaseV3 |
| --- | ---: | ---: | ---: |
| Toronto | `5.093s` | `2.313s` | `1.493s` |
| Toronto-new | `15.123s` | `6.050s` | `4.704s` |
| DBLP | `0.675s` | `0.626s` | `0.581s` |
| DBLP-new | `1.110s` | `0.862s` | `0.484s` |
| MovieLens | `3.511s` | `3.587s` | `5.247s` |
| **total** | **`25.511s`** | **`13.438s`** | **`12.509s`** |

只有 MovieLens 略正，DBLP snapshot 也没有数量级状态或时间收益。按长跑纪律，没有运行 Toronto full 或 DBLP full。

## 4. 结论

- Test22 证明同时全局化 D/A 会展开更多 orientation states；Test23 证明只全局化 D 仍被动态 merge 成本压倒。
- B 的优势不能只靠调度迁移到 A。A 的下一机制必须在图传播前减少 rooted components 或改变其完备分解，而不是把相同 labels 换成另一种 frontier 容器。
- Test23 源码、构建入口、临时结果和 snapshot 均已撤出。

