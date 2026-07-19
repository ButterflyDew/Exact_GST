# ReleaseV5 vs PrunedDP++ 配对基准

该工具用于回答 ReleaseV5 是否形成“低 `g` 持平，随后随 `g` 增大取得 10--100 倍时间和空间优势”的完整曲线。它不修改图、组或单条查询，只从随机生成的 300 条查询中提取固定前缀作为探索面板，并让全部方法读取同一面板。

```powershell
python tools\release_v5_vs_pruneddp\benchmark.py `
  --tag gpu4gst_small_pilot `
  --datasets GPU4GST_Musae,GPU4GST_Twitch,GPU4GST_Github `
  --groups 4-10 --query-count 20 --timeout-seconds 1800 `
  --execution-mode batch
```

默认比较三种配置：

- `ReleaseV5`：当前发行实现。
- `PrunedDPStrict`：`hash + MST upper + lb2 pathmax on`，即最接近论文 Algorithm 4 的默认复现。
- `PrunedDPReference`：只关闭理论上有正确性疑问的 `lb2 pathmax` 并允许 reopen，作为正确性敏感性对照。

可用 `--methods ReleaseV5,PrunedDPStrict,PrunedDPReference,DPBF` 增加 DPBF oracle。

`--execution-mode batch` 让每个 `(dataset,g,config)` 在一个进程中连续运行全部面板查询，图只加载一次，适合预计都能完成的低 `g` 面板。它的 timeout 属于整个批次；若批次中途超时，尚未写入 `weights.txt` 的查询不能判断各自耗时。

`--execution-mode instance` 为每条固定查询启动独立进程和结果目录，timeout 因而严格属于该查询。它会重复加载图，进程 wall 不能直接作为论文求解时间；已完成查询仍以 `weights.txt` 的 solver time 为正式时间。高 `g` 或预计基线会超时的面板应使用该模式，例如：

```powershell
python tools\release_v5_vs_pruneddp\benchmark.py `
  --tag gpu4gst_high_g `
  --datasets GPU4GST_Musae,GPU4GST_Twitch,GPU4GST_Github `
  --groups 13-15 --query-count 3 --timeout-seconds 1000 `
  --execution-mode instance `
  --methods ReleaseV5,PrunedDPStrict
```

无论哪种模式，`weights.txt` 都输出逐查询 query peak RSS。若发生 timeout，`summary.csv` 只比较双方都完成的查询，不把 cutoff 填成伪造的求解时间；同时必须报告每种方法的完成数，避免 completed-pair speedup 的删失偏差。

输出位于：

```text
result_snapshot/release_v5_vs_pruneddp/runs/<tag>/
  metadata.json
  input/
  raw/
  results/
  run_status.csv
  per_query.csv
  summary.csv
```

`run_status.csv` 在 instance 模式下额外记录查询编号。`summary.csv` 同时报告：逐查询答案差异、双方都完成查询上的总时间比、中位数与几何平均 speedup、绝对 query peak RSS 比，以及仅用于诊断的 `peak-rss_before` 增量比。论文空间主口径仍是绝对 query peak RSS；出现超时时，completed-pair 指标只能描述可观测子集。
