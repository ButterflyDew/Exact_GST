# Dual-Anchored Global Labels Probe

这是形成 ReleaseV2 和 ReleaseV3 的历史 half/global 研究探针，不是当前发行入口。它保留多种研究模式；干净单路径实现位于 `methods/Release/release_v3.cpp`。

完整算法、证明、复杂度、small/fast 边界与 DBLP g13 结果见：

```text
readme_files/release_v2.md
readme_files/release_v2_evidence.md
readme_files/release_v3.md
readme_files/history/half_global_hybrid.md
readme_files/archive/half_global_probe_log_20260710.md
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

本轮新增模式只用于研究：

```text
--dual-half / --dual-half-offline    强下界下的 half labels 与离线三块
--bare-half / --bare-anchored        无重预处理的 rental 工作量
--metric-half / --metric-anchored    组度量 TSP/2 与缺块 threshold envelope
```

`--bare-half` 也启用 completion 下界取零时仍成立的 `base+k*lambda` 精确停止包络；它不构造组度量。对应 dataset 模式在名称后加 `-dataset`。这些模式的负结果不写入 ReleaseV3，也不按 `g` 或数据集自动启用。

## Dataset

```powershell
.\build\tools\global_half_probe\Release\gst_global_half_probe.exe `
  --dual-anchored-dataset data_snapshot\generated_fast\DBLP_data_bfs g12 1 1
```

最后一个参数是 1-based anchor group。该入口用于显式 anchor A/B；ReleaseV3 的生产规则是选择离 root-star 根最远的组，不试跑多个搜索。

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
