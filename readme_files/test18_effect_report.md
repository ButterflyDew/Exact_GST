# Test18：效果报告

这份报告只保留当前需要引用的关键实测。旧长表和实时日志已归档到 `readme_files/archive/test18_effect_report_full_legacy_20260710.md`。

读法：这里的结果只描述 Test18 合规快照。Test19 的 one-tree LB、DS* 候选和后续代码拆分不在本文件记录。

## 1. 构建口径

`CMakeLists.txt` 已显式保证 Release / RelWithDebInfo 使用 O2：

- MSVC Release 默认 `/O2 /Ob2`，当前 CMake 也显式补 `/O2`。
- GCC / Clang Release 路径显式补 `-O2`。
- 下表标注为 O2 后的关键时间已在该守门写入后重跑。
- 更早长跑和失败探针保留为历史证据，未全部重跑；引用时必须标明不是当前 O2 后完整成绩。

## 2. 正确性

| 项目 | 结果 |
| --- | --- |
| complement row cache 接入后随机对拍 | `seed=303033`，80 组 `ALL_OK` |
| O2 守门后随机对拍 | `seed=404041`，80 组 `ALL_OK` |
| 已撤出 frontier k=3/k=4 的历史对拍 | `seed=707071`，80 组 `ALL_OK`；只作历史证据 |
| 固定 g13 小图对拍 | `seed=717273`，40 组 `ALL_OK` |
| A* ordering 默认路径 | `seed=555661`，80 组 `ALL_OK` |
| A* ordering 开启路径 | `seed=555662`，80 组 `ALL_OK` |

## 3. Complement row cache，O2 后关键 A/B

同一份当前源码，差别只在是否启用补侧 Complete row 物化。三组都是 40k `g=12 query 1`，最终权重一致。

| dataset | weight | direct wall | row-cache wall | 判断 |
| --- | ---: | ---: | ---: | --- |
| `DBLP_data_bfs` | `17.1766480000` | `176.505s` | `103.110s` | 明确正收益 |
| `DBLP_data_new_bfs` | `8.8385900000` | `49.917s` | `38.144s` | 明确正收益 |
| `Toronto_data_new` | `14.9921859260` | `135.753s` | `64.341s` | 明确正收益 |

结论：这是当前最稳定的 Test18 内生优化。它不是图/询问压缩，也没有数据集、组数、层数或时间特判。

## 4. 已撤出：Frontier k=3/k=4 partition

该机制曾用“新 k=3/k=4 row ready”触发合法完整上界。虽然没有固定 rows、数据集或 g13 特判，但 `3/4` 本身仍是没有统一成本/理论判据的硬编码层级，按 `agent.md` 第六条已从 Test18/Test19 当前源码和统计字段撤出。

以下 O2 后数字只保留为撤出前历史证据，不代表当前源码：

| dataset | weight | row-cache wall | frontier wall | 判断 |
| --- | ---: | ---: | ---: | --- |
| `DBLP_data_bfs` | `17.1766480000` | `103.110s` | `105.866s` | 略慢 |
| `DBLP_data_new_bfs` | `8.8385900000` | `38.144s` | `35.967s` | 略快 |
| `Toronto_data_new` | `14.9921859260` | `64.341s` | `66.259s` | 略慢 |

结论：它能前移合法 best，但端到端 wall 混合，启用条件又不合规；不再作为当前 g13 探针候选。旧 g13 输出中关于“早拿到强上界可降低后续洪峰”的信息仍保留在归档。

## 5. A* ordering 短探针

`GST_TEST18_ASTAR_ORDER=1` 默认关闭。它用 `d + LowerBound(v,R)` 改变单层图搜索 heap key，并在 relax 点用同一个一致下界做安全跳过。

| dataset | default wall | A* wall | pq_pop 变化 | 判断 |
| --- | ---: | ---: | ---: | --- |
| Toronto query 1 | `0.0929s` | `0.0963s` | 未形成收益 | 略慢 |
| `DBLP_data_bfs` g9 | `4.290s` | `3.971s` | `8.037M -> 3.779M` | 有收益 |
| `DBLP_data_new_bfs` g12 | `33.979s` | `32.261s` | `42.021M -> 18.254M` | 有收益 |
| `DBLP_data_bfs` g12 | `102.721s` | `102.658s` | `115.591M -> 59.499M` | wall 基本持平 |
| `Toronto_data_new` g12 | `62.049s` | `64.131s` | `66.059M -> 68.875M` | 变慢 |

结论：A* ordering 是有理论依据的 DS* 入口，但当前结果不稳定，不能默认启用，也不值得单独触发 full DBLP g13 query 1。

## 6. DBLP g13 query 1 状态

| run | 来源 | 结果 | 可引用范围 |
| --- | --- | --- | --- |
| two-portal 完整 run | 上一合规主线 | final weight `12.5936282853`，wall `20193.624s`，peak `25185.023 MiB` | 可作为上一完整成绩 |
| three-portal 完整探针 | 撤回前二进制 | 同样得到 `12.5936282853`，pair finite `121.365M -> 119.455M`，peak 低约 `245 MiB` | 只作为结构收益和瓶颈定位 |
| 当前源码 p4 guarded 中止探针 | 无固定 rows、无固定组数/层级、无非 dense 高阶 light 特判 | k=5 masks `1316` 到达 `best=12.9949`，masks `1625` 手动停止，无 final weights | 只说明撤回不合规特判后仍能到达关键上界路径 |

执行策略：除非有突破性理论进展或非常重要的短探针输出，不再用单个混合结果启动 5 小时级 full DBLP g13 query 1。

## 7. Future LB probe

`tools/future_lb_probe` 用 DPBF 精确求 `opt_future(v,R)`，先检查候选下界的 admissibility 和 edge consistency，再考虑是否进入保存、compact、relax 或 DS* 搜索语义。

当前有效信息：

- `current_lb` 在已有随机 probe 中没有 admissibility / consistency violation，平均 exact-future ratio 约 `0.91--0.98`。
- `rooted_metric_mst_half` 可采纳但出现 consistency violation，不适合直接进 A* relax。
- `far_plus_near` 是负例，会产生多处 violation。

后续若提出新下界，先过这个工具，再进入 Test18/Test19。
