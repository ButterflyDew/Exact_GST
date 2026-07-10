# Dual-Anchored Global Labels Probe

这是形成 ReleaseV2 的历史精确原型，不是当前发行入口。它保留 half/anchored/dual-anchored 模式和完整诊断，供复核原始实验；干净单路径实现位于 `methods/Release/release_v2.cpp`。

完整算法、证明、复杂度、small/fast 边界与 DBLP g13 结果见：

```text
readme_files/release_v2.md
readme_files/release_v2_evidence.md
```

## Build

关键结果使用 CMake `Release` 配置，即 MSVC `/O2`：

```powershell
cmake --build build --config Release --target gst_global_half_probe
```

## Correctness

随机实例在同一进程内与 DPBF 比较，容差为 `1e-6`：

```powershell
.\build\tools\global_half_probe\Release\gst_global_half_probe.exe `
  --dual-anchored 710971 1000 4 14 2 10

.\build\tools\global_half_probe\Release\gst_global_half_probe.exe `
  --dual-anchored 710973 50 8 14 13 13
```

当前结果分别为 `1000/1000` 和 `50/50`，`mismatches=0`。

## Dataset

```powershell
.\build\tools\global_half_probe\Release\gst_global_half_probe.exe `
  --dual-anchored-dataset data_snapshot\generated_fast\DBLP_data_bfs g12 1 1
```

最后一个参数是 1-based anchor group。anchor 只改变等价状态表示，不改变最优值；当前实验统一使用第一组，不按数据集、`g`、层级、状态密度或运行进度切换策略。

## Bounded Full Run

全量运行必须由 runner 持有真实 solver PID：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tools\global_half_probe\run_bounded.ps1 `
  -GraphFolder data\DBLP -QuerySelector g13 -QueryIndex 1 `
  -AnchorGroup 1 -Seconds 1800
```

runner 每 250ms 采样 RSS；正常退出和 timeout 都会回收子进程。`GST_GLOBAL_LABEL_PROGRESS=1` 只在 settled labels 达到二次幂时输出诊断，不参与策略。

2026-07-10 的 Release/O2 运行在 `1459.076s` runner wall 内正常退出，solver wall `1427.625s`，peak RSS `9231.500MiB`，精确结果为 `12.5936282853`。
