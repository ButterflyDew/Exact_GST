# Test25--27：Anchor Full Closure、DBLP 分层与 Generated-Star 探针

更新时间：2026-07-12。本文记录三个已经撤出源码和构建入口的短期原型。当前 Test21 仍见 `../test21_anchor_half.md`。

## 1. Test25：删除 A rows 的错误 full-closure 定理

候选试图只保留 ordinary half D rows：把 `K=U-{anchor}` 分成两个、随后三个 half blocks，在同根合并后只做一次到 anchor group 的图闭包。目标是把 `D 2509 + A 1586` 个 masks 降为 `D 2509 + 1`。

两个 blocks 很快被固定随机例否决；恢复完整 D split、把 D heuristic 降为安全的 0 后仍失败。改成 token-centroid 的三个 blocks 也复现同一反例：

```text
seed       712501
iteration  35
exact      37
candidate  40
```

反例图的最优树为路径 `3-4-2`。anchor group 命中顶点 3；若干 nonanchor groups 也在 3、4、2 上被免费覆盖。固定 full nonanchor mask 在从 centroid 向 anchor 传播时不能吸收沿途新覆盖的 groups，于是 half seeds 会重复计算 anchor 路径。

结论：一次 fixed-mask full closure 不等价于 A 主干。修复必须允许 trunk 传播时改变 mask，或者保留 A+D recurrence；前者会回到跨 mask labels。Test25 未进入数据集 benchmark，源码和目标已删除。

## 2. Test26：DBLP ordinary D 分层

Test26 与正式 Test21 算法完全相同，只在每个 D 层结束时写一行 phase 日志。原始 DBLP g13 q1 直接运行约三分钟后停止，得到：

```text
D1 layer_ms=0.0207
D1 cumulative total_ms=53779.6
D1 logical values=29,973,384
```

停止时：

```text
CPU       252.45s
RSS       2,272,808,960 bytes（约 2.12GiB）
D2 marker 尚未出现
```

D1 rows 是 12 张隐式 group-distance arrays；`53.8s` 主要是 group distances、root-star 与 dual 公共预处理。决定性信息是：再运行约两分钟后 D2 仍未完成，A 阶段尚未开始。

这与历史 full pair probe 的 `125,637,681` settled pair states、`235.072s` pair search 一致。DBLP 退化的首要瓶颈是 ordinary D2，而不是当前 A rows 或最终 completion。后续若只优化 A，不可能把总时间压到 V3 的 `531.556s` 内。

## 3. Test27：ordinary generated-star upper

ReleaseV2 的 DBLP 日志显示 global labels 在约 `1.05M` settled 时把 best 从 `15.0174` 降到 `12.6821`。Test27 因此在每个 ordinary D settled value 上尝试：

```text
D(S,v) + sum(gd[group][v], group in U-S)
```

这是可行树上界。remaining groups 的距离和复用 H 已经进行的 group 扫描，不改变渐进复杂度。

正确性通过：

```text
g=2..12   500/500, seed 712511
fixed g13  50/50, seed 712513
```

Toronto-fast g12 和 Toronto full g13 的 `ordinary_star_updates` 都为 0；时间分别为 `1.764s` 和 `21.231s`，因此短测上没有正收益，也几乎没有额外状态成本。

原计划运行 full DBLP 到 D2 完成；该命令因当前 Codex 执行额度被工具层拒绝，solver 没有启动。随后使用同一已构建二进制补做 3500 点 fast DBLP g12 q1：

| dataset | updates | D2 values / pops | pair layer |
| --- | ---: | ---: | ---: |
| DBLP | `0` | `2,226 / 3,089` | `30.003ms` |
| DBLP-new | `0` | `10,615 / 13,495` | `33.015ms` |

两库的 D2 payload 与正式 Test21 完全相同，因此 single-pair generated-star 已被 fast 门槛否决。full DBLP 从未启动，不能把 fast 结论冒充 full 结果。Test27 代码已撤出。

## 4. 当前研究边界

1. Test25 证明 fixed-mask full closure 不能替代 A，因为 anchor trunk 会吸收 groups。
2. Test26 证明 DBLP 首要瓶颈在 D2，早于 A。
3. pair predecessor certificate 只压表示而不减少 `125.6M` states，旧 Test20 已否决重复解码形态。
4. Test27 说明单个 pair 加 singleton star 不足以提前收紧 fast DBLP 的 best。
5. 下一步必须避免物化绝大多数 pair roots，或构造允许共享内部主干的早期上界；只改 A 调度、completion 或 row 扫描不再是主线。
