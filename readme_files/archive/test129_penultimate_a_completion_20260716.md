# Test129：删除最终 A 层的结构反例

更新时间：2026-07-16。Test129 已从源码和 CMake 撤回，没有运行 Toronto 或 DBLP。

## 1. 被检查的假设

DBLP g15 q33 的历史轨迹在日志中的 `best_after_a_s5` 已等于 exact，而 A6 耗时 `1730.497s`。Test129 保留全部普通层 `D_1..D_h`，只在生成 `A_{h-2}` 后停止，希望删除最终 `A_{h-1}`。探针没有数据集、固定 `g`、密度、wall time 或经验阈值；它只改变一个自然状态边界。

Release/O2 随机门最初为宽范围 `100/100`（seed `717501`）和固定 g15 `300/300`（seed `717502`）。这些结果没有证明完备性，因为已有 primal upper 可能在 A 阶段前偶然等于 exact。

## 2. 7/4/4 反例

构造一棵 34 顶点、33 边的树。中心 `r` 连接三个分支：

```text
left junction:  edge weight 10, followed by 7 terminal leaves of weight 1
right junction 1: edge weight 1, followed by 4 terminal leaves of weight 1
right junction 2: edge weight 1, followed by 4 terminal leaves of weight 1
```

15 个组各有两个候选：一个是上述对应的真实 terminal leaf，另一个是该组独有、通过权重 3 的边直接接到 `r` 的 decoy leaf。图本身仍是树，组之间不重叠。最优解选择三个共享分支，成本为：

```text
10 + 7 + (1 + 4) + (1 + 4) = 27.
```

DPBF、Test129 与完整 Test80 的 Release/O2 结果为：

| solver | result |
| --- | ---: |
| DPBF | `27` |
| Test129，停于 A5 | `29` |
| 完整 Test80，保留 A6 | `27` |

两版共同的初始轨迹为 `root-star=37`、`dual-primal=37`、`junction=31`，D5 首次得到 `29`，D6/D7 均不再改善。Test129 的 A1--A5 仍保持 `29`；完整 Test80 只有最终 A6 消费者把答案降到 `27`。

## 3. 为什么历史日志会误导

流式实现交错生成 A5 producers 与 A6 consumers，结束后把同一个最终 best 同时写入 `best_after_a_s5` 和 `best_after_a_s6`。因此 q33 日志中二者都等于 exact，不能推出“A5 自己已经得到 exact”。Test129 将 A5 单独运行后，反例清楚地区分了 producer 与 consumer 的贡献。

## 4. 结论

最终 `A_{h-1}` 层承载真实的 paid attachment 结构，不能依据共享 best 日志或随机通过率删除。Test130 同时删除最高 D/A 层的反例与本结论一致，但 Test129 进一步证明：**即使完整保留 `D_h`，最终 A 层仍然不可直接省略。**

下一步不能再从层级日志猜测冗余性；需要让 producer/consumer 分别记录首次更新来源，并把状态删除建立在完备性证明上。本轮没有新增论文引用，DPBF 仅作离线核验器。
