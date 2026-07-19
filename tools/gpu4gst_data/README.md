# GPU4GST 数据准备工具

`gst_prepare_gpu4gst` 将 `data_origin` 中 GPU4GST 作者发布的数据转换为当前仓库可以直接读取的目录。完整的数据来源、查询协议和测试结果见 [`../../readme_files/gpu4gst_datasets.md`](../../readme_files/gpu4gst_datasets.md)。

## 构建与运行

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_prepare_gpu4gst -- /m:1
.\build\tools\gpu4gst_data\Release\gst_prepare_gpu4gst.exe `
  data_origin data all --seed 2025 --queries 300 --min-g 4 --max-g 16
```

第三个位置参数可以从 `all` 改为 `Musae`、`Twitch`、`Github`、`Youtube`、`DBLP`、`Orkut`、`LiveJournal` 或 `Reddit`。已存在且头部匹配的 `graph.txt` 默认复用；只有明确需要重写图时才加 `--force-graph`。

`--queries` 只控制独立生成的 `query_g<g>.txt` 数量；作者 `query_author_g3/g5/g7.txt` 始终逐行转换 CSV 的前 300 条，与论文实验脚本的 `0..299` 范围一致。使用非默认 `--min-g/--max-g` 做局部调试时应指定新的输出根目录，避免旧目录中其他 `g` 文件与新 manifest 混在一起。

## 工具保证

工具在生成过程中完成以下检查：

1. `.in` 的实际边数、端点范围和非负整数权重与头部一致。
2. `.g` 的组号连续、组非空、顶点范围合法且组内没有重复顶点。
3. 作者 CSV 的组号范围、组数和前 300 行数量正确。
4. 作者查询和新生成查询都存在一个同时触及所有查询组的原图连通分量。
5. 所有写入先落到同目录临时文件，完成后再替换正式文件；成功退出后不保留 `.tmp`。

每个目标目录还会生成 `dataset_manifest.json` 和 `query_*.group_ids.txt`。前者记录图规模、组成员数、逐 `g` 派生种子、BFS 深度与拒绝采样次数；后者保留查询对应的 1-based 候选组编号，便于不展开顶点列表地审核查询。

作者 CSV 的整数是 0-based `.g` 行号。编号依据是作者 commit [`716a19c`](https://github.com/toziki/GPU4GST-sigmod/tree/716a19c240c480cb2d23435bbaca55163a48e174)：其 `read_Group` 将标签 `gN` 放入 `group_graph[N-1]`，而 `read_inquire` 不改变 CSV 整数，随后求解器直接按该整数访问 `group_graph`。
