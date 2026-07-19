# Test131：最终 A Consumer 的整 Mask 下界

更新时间：2026-07-16。Test131 已从源码和 CMake 撤回，没有运行 Toronto g13 或 DBLP 大图。

## 1. 方法

对每个最终 anchored consumer mask `S`，Test131 在生成该状态前计算：

```text
producer lower(S)
  = min over S=P disjoint-union Y of min A(P) + min D(Y)

completion lower(complement S)
  = min over complement S=L disjoint-union R of min D(L) + min D(R).
```

两部分允许各行最小值在不同根取得，因此总和不大于任何真实 `A+D --path-- D+D` 完成代价，是安全下界。只有严格大于 incumbent 时才整张跳过最终 consumer；没有经验参数、数据集、固定 `g`、密度或运行时刻判断。实现保持 permanent-anchor A、root-irreducible D branch、离线有序行和流式依赖顺序不变。

## 2. 正确性

Release/O2 以 DPBF 和 `1e-6` 对拍：

```text
wide g=5..15   seed 717531   200/200
fixed g=15     seed 717532   200/200
```

## 3. Fast20 结果

固定 fast20 一共有 `4,270` 个最终 consumers，只拒绝 `147` 个，即 `3.44%`。命中分布为：

```text
Toronto_data / Toronto_data_new     0
DBLP_data_bfs g9                   42 / 56
MovieLens_data_bfs g9              28 / 56
MovieLens_data_bfs g10             47 / 126
MovieLens_data_bfs g11             30 / 210
其余实例                            0
```

所有较重的 Toronto 查询均零命中；拒绝集中在本来就只有极少顶层 states 的易剪实例。该下界虽然从 completion 前移到了状态生成前，但独立行最小值仍丢失共同根与 attachment 相关性，不能改变 A 层主要工作量。

## 4. 结论

Test131 不满足“减少有实际重量的 D/A 状态生成”门槛，因此撤回，不触发更长门。下一候选不能继续叠加独立标量 minimum；需要保留 producer 与 completion 两端的联合根 profile，或直接计算二者之间的 exact scalar connection。本轮没有新增论文引用。
