# Test123：精确 rooted triple 扩展锚树（已撤回）

更新时间：2026-07-16。Test123 检查 Test121 是否受限于 junction 的 singleton-star triple roots。它在完整 D3 row 自然可用后，用每张精确 rooted triple 到永久锚路径的最优接入根扩展压缩父树，再运行相同的 D-block 树设施 DP。候选正确、拓扑确实扩大，但上界和状态没有改变，因此源码、父树接口和构建目标均已撤回。

## 1. 理论动机

Test48 已证明 triple 是普通组集合中第一次能在根处表达真实三分叉的最小 block，因此原 junction 用 `delta_P(v)+gd_i(v)+gd_j(v)+gd_k(v)` 的最优根构造 skeleton。Test123 将这个 singleton-star 代理替换为当前 A 框架实际生成的 `D({i,j,k},v)`：

```text
r_B = argmin_v D(B,v) + delta_P(v),  |B|=3.
```

所有 `r_B` 到锚路径的既有最短路父树与 Test121 skeleton 取并，再只保留候选点和分叉点。候选集合仍只有 `O(g^3)`，树设施复杂度保持 `O(g^3*3^(g-1))`，不按数据集、wall time 或状态密度选择。每个新树 DP 方案仍由真实 D 见证树和真实父树路径组成，所以只产生合法上界。

## 2. 短门结果

Release/O2 黑盒对拍通过宽范围 seed `717251` 的 `100/100` 与固定 `g=15` seed `717252` 的 `30/30`。Toronto `g13 q1--q5` D3 panel 中，扩展后的压缩树节点数分别为：

| query | Test121 tree | exact-triple tree |
| --- | ---: | ---: |
| q1 | `33` | `38` |
| q2 | `52` | `98` |
| q3 | `54` | `63` |
| q4 | `38` | `65` |
| q5 | `35` | `38` |

尽管 q2、q4 的拓扑显著扩大，五条询问的 D3 `U_tree`、incumbent、values、branches、pops 与 row bytes 均和 Test121 逐项相同。新增节点只把 q2 的树 DP probes 从 `41.45M` 增到 `78.12M`，没有进入最优方案。

因此 Test121 的限制不是缺少 exact triple attachment roots。结合 Test122 的全 rooted-row 节点投影负结果，继续向同一最短路父树添加 facilities 或候选根没有新的证据。按长跑纪律没有启动 DBLP q33；临时结果清理后只保留本文件。

本实验没有新增论文引用。
