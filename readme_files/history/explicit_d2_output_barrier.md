# Explicit D2 Output Barrier

更新时间：2026-07-12。本文把 Test20、26、36、38、39 的共同边界写成下一原型的硬门槛。它不是 GST 的一般复杂度下界，只针对“先完整物化 pair rows，再由 A/高层 rows 消费”的状态接口。

## 1. 接口下界

对 `k=g-1` 个 nonanchor groups，显式接口要求为每个 pair 和每个 surviving root 保存或至少输出：

```text
D({i,j},v), 1<=i<j<=k.
```

若一条查询有 `L2` 个 surviving pair-root values，则任何先完成该接口再进入消费者的实现至少需要：

```text
Omega(L2) value productions
Omega(L2) persistent cells or equivalent repeated reconstruction
```

最坏 `L2=Theta(C(k,2)n)`。这不排除隐式 distance oracle 或 demand-driven target queries；它只说明压 heap、换扫描顺序或压每个 value 的字节数不能删除输出本身。

## 2. DBLP 证据

full DBLP g13 q1 的独立 pair 层已经复现：

```text
surviving pair-root states  125,637,681
queue pushes                225,221,651
pair search                 235.072s
```

Test26 又在当前 Test21 中把首要瓶颈定位到 D2，A 尚未开始。后续实验分别触及接口的不同部分：

- Test20 predecessor certificate：每个 value 字节数缩小 `3.226x`，但 `L2` 不变，重复解码预计约 `4.06h`；
- Test36 multi-pair vector wave：共享 source events，但 pair-specific priorities 导致 fast 慢 `3.8x--45x`，最终 pair values 仍需区分；
- Test38 local seed cones：减少 heap sources/pops，retained values 不变，DBLP gate 仍未完成；
- Test39 one-third backbone：减少 D6/anchored 高层约 `4%`，D2 定义与数量完全不变。

## 3. 下一机制的验收条件

下一原型在写 C++ solver 前必须明确满足至少一项：

1. **consumer-driven targets：** 只对已经由 anchor/path states 证明会读取的 roots 求 D2；
2. **implicit exact oracle：** 不逐 root 存值，并能批量回答消费者而不重复线性解码；
3. **state elimination：** 完备性证明不再引用任意-root D2 row；
4. **shared algebraic representation：** 一个对象同时代表多个 pair/root values，且不引入 Test36 的多 priority frontier。

仅减少 initial seeds、queue pushes、单值字节、D3+ merge probes 或最终 completion checks，不再构成 full DBLP 长跑依据。

## 4. Consumer-Driven 调度仍需证明什么

简单“先生成 A roots，再查询 D2”存在循环依赖：新的 D2 值会生成新的 A/P roots，而这些 roots 又可能成为新的 D2 targets。精确调度必须证明：

- target 集单调扩张并在有限状态内闭合；
- Dijkstra 可以暂停/恢复而不丢失 future-consistent pruning；
- 未被请求的 root 不可能通过图传播生成一个随后必要的 A/P state；
- 总 settled pair roots 在 fast/full 结构探针中确实比 `L2` 小一个数量级。

## 5. Test40 对 Consumer-Driven 路线的结论

Test40 先在 exact incumbent 下生成 A1 roots，再为每个 pair 构造首批 `A1+D2` 消费目标。fast DBLP/DBLP-new 的 target union 只有 `10.8%/6.2% n`，因此触发一次不求解完整 query 的 full 结构探针；full DBLP 上 union 已升到平均 `59.1% n`。

更严格的 multi-target Dijkstra 为每个 root 使用 `best-A1-future` 的 exact consumer threshold，并保留正式 D2 future pruning。full min/median/max 三对仍分别 settle 完整 row 的 `99.95%/99.85%/99.73%`，wall 慢 `1.33x/1.29x/1.03x`。所以首批 target 数不能转化为搜索数量级下降；higher-A 消费只会继续扩张 targets。该路线已撤回，详见 `../archive/test40_anchor_consumer_d2_20260712.md`。

consumer-driven targets 现有接口已经否决。剩余可行方向收窄为：在 D2 前显著收紧 incumbent、从完备性中删除 D2，或找到保留 future ordering 的隐式共享表示。没有新的结构证明前，不再次运行 full DBLP solver。

后续 Test41 的 dual zero-residual half/三块 upper 在 full 上没有改善 `17.360814`；Test42 的 work-triggered ordinary greedy 虽把 fast20 降到 `12.182s`，full 仍在 `558.7 CPU-s` 无最终结果。因而“显著收紧 incumbent”特指新的 anchor-aware 可行树结构，不再包括复用通用 greedy、零残量 restricted tree 或按经验时刻运行 B warm start。

Test43 又表明，绕过 D2 后显式生成 early A1/C1 rows 也会在 full 上发生全图传播爆炸；即使先给出合法 `15.0174`，`234.8 CPU-s` 仍未完成 size 1。三个独立 anchor-containing blocks 还会重复支付 trunk，fast completion 为 0 次更新。故下一接口必须共享一条 anchor backbone 并在其上消维，不能把 D2 barrier 简单搬到 A1。

Test44--47 随后验证 paid backbone scalarization 确实能消掉 block root：full early upper 降到 `13.1619`、sampled peak 约减半，fast20 最低 `10.112s`。但单路径 skeleton 只有 4 个 vertices，四个 full 集成版仍在 `543--555 CPU-s` 无最终结果。它不是无效方向，而是尚未满足时间门槛；下一步只考虑受控共享分叉 skeleton，不再靠更多 block columns 或更深经验停止点。
