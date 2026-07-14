# Goal-Root-Star Probe

本工具验证两项独立结构：

1. 任意 `g<=20` 时精确求 root-star 值 `min_v sum_a gd[a][v]`，用并行多源 Dijkstra 和 root-class 下界提前停止；仅在 `g<=3` 时该值等于 GST `OPT`；
2. 无向图上用多源双向 Dijkstra 精确统计组对距离的实际扫描工作。

算法证明、small 汇总、随机种子和论文归属见：

```text
readme_files/history/half_global_hybrid.md
readme_files/archive/half_global_probe_log_20260710.md
```

构建和运行：

```powershell
cmake --build build --config Release --target gst_goal_root_star_probe

.\build\tools\goal_root_star_probe\Release\gst_goal_root_star_probe.exe `
  --self-check 711281 2000 4 14

.\build\tools\goal_root_star_probe\Release\gst_goal_root_star_probe.exe `
  data_snapshot\generated_small\MovieLens_data_bfs g3 1

.\build\tools\goal_root_star_probe\Release\gst_goal_root_star_probe.exe `
  --pair-metric data_snapshot\generated_small\MovieLens_data_bfs g4 1
```

`--truncated` 只统计“先构造 greedy upper，再把组距离截到 upper”能省多少扫描；该路线的负结果已归档，不进入发行代码。
