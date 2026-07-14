# Test71--72：Quarter Endpoint 反例与 Exact-D Heavy Orientation

更新时间：2026-07-13。本轮研究 `q=ceil(h/2)` 是否能把 half-sized ordinary rows 改写成更小分支沿 backbone 接入的状态。Test71 把高层 row 降格为不可继续作为 branch 的 endpoint `P`，随机反例证明该状态不完备；Test72 保留 exact `D` 语义后恢复正确性，但只改变等价 split 的方向，fast20 退化，因此两版均已撤回。

## 1. 候选状态

令 `h=floor(g/2)`，`q=ceil(h/2)`。任意含至多 `h` 个 token 的 rooted tree 在根处至多有一个大于 `q` 的 child component。沿该 heavy child 递归可得到一条 root-to-leaf path，所有离路径组件大小至多 `q`。

Test71 据此尝试：

```text
D(S,v)  exact closed rooted component, |S|<=q
P(S,v)  backbone endpoint, q<|S|<=h
A(S,v)  anchor endpoint
```

高层 `P` 只接 `|B|<=q` 的 branch，不发布 branch bits；`A` 也只消费不超过 `q` 的 ordinary side。随后与既有 `A+P+P` completion 组合。

## 2. Test71 反例

组合版在随机对拍 seed `713581` 的 iteration `428` 失败：`n=15, m=49, g=9`，DPBF 为 `20`，Test71 为 `21`。隔离时曾保留 `case_428` 复跑，完成取证后已按仓库纪律删除临时目录；该实例仍可由相同 seed、iteration 与 `n=4..15, g=2..10` 参数重生。隔离结果如下：

| variant | result | observation |
| --- | ---: | --- |
| Test71 + junction + packing | `21` | mismatch |
| 禁用 packing，保留 junction | `21` | packing 不是根因 |
| junction upper | `30` | 高于当时 incumbent `29`，没有参与剪枝 |
| 恢复完整 exact D branch/A 接口 | `20` | 与 DPBF 一致 |

错误不在 heavy-path 存在性，而在接口：一个高层 feasible endpoint tree 仍可能是 anchored recurrence 所需的完整可接入组件。把它标记为“不再发布 branch”，并限制 A 只能消费小 D，会删除最优树。此前 `1000/1000` 小图通过只说明样本未覆盖该结构，不能替代完备性。

## 3. Test72 修正

修正版不再引入 `P` 语义，所有 rows 继续是 exact `D(S,v)`，继续发布 root-irreducible branch bits，也允许 A 消费任意可用 ordinary side。唯一变化是 split orientation：

- 若一侧大于 `q`，强制另一较小侧作为 branch；
- 若两侧都不大于 `q`，沿用最低 pivot 去重；
- 每个无序 partition 仍只枚举一次。

该 recurrence 可由 rooted heavy path 归纳：取离根第一段的小 off-path component 作为 branch，剩余 heavy side 作为 accumulator；两侧都小时退化为原 exact split。反向方向只组合真实可行树，因此仍是 exact D。

正确性证据：

| check | result |
| --- | ---: |
| Test71 iteration 428 反例 | `20 == DPBF` |
| seed `713591`, `n=4..15`, `g=2..10` | `2000/2000` |
| 清除组合代码后 seed `713601` | `1000/1000` |

## 4. 性能否决

Release/O2 fast20 snapshot：`result_snapshot/fast/20260713_035748`。

| implementation | total query time |
| --- | ---: |
| 正式 Test21 | `12.380s` |
| Test72 heavy orientation | `12.843s` |

Test72 慢 `3.7%`。原因是 `q=ceil(h/2)` 只保证每个 partition 至少有一侧不大于 `q`；修正版仍为每个无序 partition 做一次 join，不减少 mask、row values、graph closure 或 D2 输出。它只更换 branch/accumulator operand，无法形成数量级变化，因此不运行 Toronto full 或 DBLP g13。

## 5. 与既有工作的关系

- Test39 的 one-third endpoint 也曾删除高层 branch publication；本反例说明把更激进的 quarter 阈值直接套入相同接口并不完备。
- Test59 已表明“为每个 split 选择较小的 root-irreducible side”正确但不改变状态族；Test72 是同一边界的 heavy-path 表述，实验结论一致。
- 本轮没有采用新的论文算法。heavy-child 论证是树上的直接归纳；文献边界继续沿用 Test39 记录，不宣称新的论文原创性。

## 6. 结论

`P` 真正消维必须同时给出完整的 anchored consumer 接口，不能靠禁止 branch publication 获得表面 payload 降低。保持 exact D 虽可修复完备性，却只剩等价 split 定向。后续应研究能隐式共享 D2 与 consumer masks 的新状态/算子，或让 anchor skeleton 直接承担连接信息；不继续微调 split orientation。
