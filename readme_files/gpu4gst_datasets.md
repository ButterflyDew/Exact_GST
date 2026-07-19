# GPU4GST 数据接入、查询协议与运行记录

> **版本边界（2026-07-18）：** 本文的数据转换与查询协议仍是当前规范；第 5 节求解记录是在 ReleaseV5 冻结期取得的历史接入证据，表内方法名和数字不代表当前 ReleaseV6 性能。ReleaseV6 的独立验证见 `release_v6.md`，后续完整实验应重新运行并单独成表。

## 1. 范围与结论

本仓库已接入 Jiayu Li 等人在 *Fast Optimal Group Steiner Tree Search using GPUs* 中使用的 8 个数据集。作者原始文件保留在 `data_origin`，仓库接口文件生成到 `data/GPU4GST_<name>`，不会覆盖原有的 `data/DBLP` 等同名数据。

当前接入分为两个明确独立的查询层：

- **作者查询复现层**：`query_author_g3.txt`、`query_author_g5.txt`、`query_author_g7.txt` 逐行转换作者 CSV 的前 300 条，不重新随机生成。
- **仓库扩展层**：`query_g4.txt` 到 `query_g16.txt` 每个文件固定 300 条，采用论文所引用的“相关组”共现 BFS 协议和公开固定种子生成。

第二层是为了覆盖本仓库主要研究的 `g=4..16`，**不是作者论文已经发布的查询输出**。GPU4GST 论文只实验 `g=3,5,7`，也没有公开其随机种子与查询生成代码；因此文档和文件名不把两层混称为同一批原始查询。

## 2. 仓库接口

每个目标目录包含：

```text
data/GPU4GST_<name>/
  graph.txt
  query_g4.txt ... query_g16.txt
  query_author_g3.txt
  query_author_g5.txt
  query_author_g7.txt
  query_*.group_ids.txt
  dataset_manifest.json
```

作者 `.in` 使用 0-based 顶点编号；`graph.txt` 保持相同的 `n m`、无向边和整数权重，只把两个端点统一加 1。作者 `.g` 的标签写成 `g1,g2,...`，顶点仍为 0-based；作者 CSV 则使用 **0-based 的 `.g` 行号**。作者 commit [`716a19c`](https://github.com/toziki/GPU4GST-sigmod/tree/716a19c240c480cb2d23435bbaca55163a48e174) 的读取代码提供了直接证据：`read_Group` 将 `gN` 存入 `group_graph[N-1]`，`read_inquire` 原样读取 CSV 整数，求解器再直接按该整数访问 `group_graph`。转换后的查询文件按当前 `query_io.cpp` 接口展开每个候选组，并把顶点统一加 1。

可直接运行：

```powershell
.\build\Release\gst_release_v5_main.exe GPU4GST_Twitch result g4 data 1 5
.\build\Release\gst_dpbf_main.exe GPU4GST_Twitch result query_author_g5 data 1 3
```

这里没有对图、组或查询做裁剪、压缩或求解相关预处理；`GPU4GST_` 前缀只解决数据版本命名冲突。

## 3. `g=4..16` 查询生成协议

### 3.1 论文依据

GPU4GST 论文第 8.1 节规定：每个数据集、每个参数设置随机生成 300 条实例，默认 `g=5`，实验 `g=3,5,7`，并称其“像相关工作 [61] 一样随机选择相关关键词/组”。论文没有继续定义“相关”。其引用 [61] 明确给出了可执行协议：把候选组视为兴趣属性；两个组只要至少在一个顶点上共现就在属性图中相邻；随机选择根属性，BFS 搜索近邻，再随机选择所需数量的近邻。

本仓库据此把相同协议扩展到 `g=4..16`。这比均匀抽取彼此无关的组更符合 GPU4GST 的实验文字，同时没有引入距离阈值、数据集特判或运行时超参数。

### 3.2 单条查询

对于给定数据集和 `g`，一条查询按以下步骤产生：

1. 构造候选组共现图：每个候选组是一个节点；两个组共享至少一个原图顶点时连边。
2. 从全部候选组中均匀随机选择一个根组。
3. 从根组逐层 BFS，完整扫描每一层，直到第一次累计找到至少 `g-1` 个其他组。这个层数就是满足数量要求的最小 BFS 深度。
4. 从截至该深度发现的全部近邻中，无放回均匀抽取 `g-1` 个，再加入根组。
5. 若不存在一个原图连通分量同时与全部 `g` 个组相交，则按论文 [61] 的要求丢弃该查询并重新采样。
6. 最后均匀打乱 `g` 个组的输出顺序，避免把生成根固定成求解器看到的第一组；这不改变 GST 实例语义。

若某个根所在的共现连通分量不足 `g` 个组，该根只会被判定一次并从后续根的拒绝采样中排除。每个 `g` 最终恰好写出 300 条可行查询。

每一行都是一次独立随机采样；[61] 没有要求不同实例的组集合必须互异，因此生成器**不做跨行去重**。这保留了原协议的采样分布，也避免增加一个文献未定义的条件。查询内部仍严格禁止重复组。

### 3.3 随机性与复现

基础种子固定为 `2025`。每个 `(dataset,g)` 通过数据集名、`g` 和基础种子派生独立的 64 位种子；随机引擎为标准 `std::mt19937_64`，有界整数使用显式拒绝映射，Fisher-Yates 和 reservoir sampling 也由工具自行实现，避免依赖不同标准库对 `uniform_int_distribution` 的实现差异。

逐 `g` 派生种子与实际 BFS 深度写在 `dataset_manifest.json`。`query_g*.group_ids.txt` 记录最终组号，因此后续即使重构查询展开代码，也能逐行核对实例身份。

## 4. 生成结果

2026-07-17 使用 Release/O2 工具完成全量生成。输出总大小为 `11.212 GiB`，成功退出后 `.tmp` 文件数为 0。

| 数据集目录 | 顶点 | 边 | 候选组 | 组成员关系 | `graph.txt` | 全部查询文件 | 最大 BFS 深度 | 不可行样本拒绝数 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `GPU4GST_Musae` | 19,109 | 400,497 | 13,183 | 1,060,946 | 5.3 MiB | 29.4 MiB | 2 | 0 |
| `GPU4GST_Twitch` | 34,118 | 429,113 | 3,163 | 687,900 | 5.8 MiB | 481.3 MiB | 1 | 240 |
| `GPU4GST_Github` | 37,700 | 289,003 | 4,005 | 690,358 | 3.8 MiB | 699.6 MiB | 2 | 0 |
| `GPU4GST_Youtube` | 1,134,890 | 2,987,624 | 5,000 | 72,959 | 44.8 MiB | 30.5 MiB | 5 | 0 |
| `GPU4GST_DBLP` | 2,497,782 | 12,786,329 | 127,726 | 76,920,675 | 211.2 MiB | 6,277.4 MiB | 1 | 51 |
| `GPU4GST_Orkut` | 3,072,441 | 117,185,083 | 5,000 | 1,078,576 | 2,020.0 MiB | 140.5 MiB | 6 | 0 |
| `GPU4GST_LiveJournal` | 3,997,962 | 34,681,189 | 664,414 | 7,168,359 | 575.5 MiB | 664.0 MiB | 3 | 0 |
| `GPU4GST_Reddit` | 4,262,834 | 12,502,767 | 1,146,657 | 17,471,829 | 212.5 MiB | 80.0 MiB | 3 | 3 |

“全部查询文件”包含生成查询、作者查询和 group-id 旁路清单。DBLP 与 Github 的查询文件较大，是因为当前仓库接口会在每条查询中完整展开组成员，而相关组协议又可能反复选中大组；这里没有改变查询分布来缩小文件。

论文 Table 1 与作者成品之间的 DBLP 顶点数、Orkut 顶点数以及 Youtube/Orkut 候选组数差异，统一以作者成品为准，详见 [`../data_origin/README.md`](../data_origin/README.md)。Orkut 的作者 CSR 二进制已知截断；本次从完整的 `Orkut.in` 生成文本接口，没有读取或修补截断二进制。

## 5. 测试运行

### 5.1 全量生成校验

转换器完整扫描了 8 个 `.in` 和 `.g`，并校验作者 `3/5/7.csv` 的前 300 条。最终得到 `8 × 13 × 300 = 31,200` 条 `g=4..16` 查询，以及 `8 × 3 × 300 = 7,200` 条作者查询转换记录。所有查询均通过原图连通分量可行性检查。

2026-07-18 又做了一次独立落盘审计：8 个 `GPU4GST_*` 目录各有 34 个文件；全部 `query_g4.txt` 至 `query_g16.txt` 和 3 个作者查询文件的头部均为 300，对应的 16 个 `group_ids.txt` 也都恰有 300 条记录；总大小仍为 `12,039,299,378` 字节（`11.212 GiB`），没有残留 `.tmp`。31,200 条派生查询按每个 `(dataset,g)` 内忽略组顺序后共有 31,166 个不同集合，即 34 行独立采样恰好重复，符合上一节不做跨行去重的协议。作者目录的 64 个文件逐字节 SHA-256 全部匹配 `SHA256SUMS.txt`；结构核验只报告作者原始 `Orkut_csr.bin` 与 `Orkut_weight.bin` 截断，转换后的 Orkut 图来自完整的 `Orkut.in`，不依赖这两个文件。

### 5.2 求解器配对检查

以下运行均使用当时构建的 ReleaseV5/O2 二进制、原始完整图和同一条生成查询。时间是 `weights.txt` 中的**查询求解时间**，不含进程启动时的文本图和全部查询文件加载；峰值是逐查询 peak RSS。小数据使用前 5 条，Youtube 使用前 3 条，大图使用第 1 条作为接入 smoke。共 22 条 `g4` 查询，DPBF 与 ReleaseV5 在 `1e-6` 下逐条一致。

| 数据集 | 查询数 | DPBF 总时间 | ReleaseV5 总时间 | DPBF 最大 peak | ReleaseV5 最大 peak | 权重 |
|---|---:|---:|---:|---:|---:|---|
| Musae | 5 | 0.535s | 0.360s | 42.6 MiB | 43.8 MiB | 164, 285, 196, 199, 34 |
| Twitch | 5 | 1.310s | 0.678s | 56.6 MiB | 53.8 MiB | 293, 183, 191, 180, 95 |
| Github | 5 | 1.447s | 0.678s | 57.1 MiB | 49.2 MiB | 0, 95, 199, 193, 390 |
| Youtube | 3 | 48.988s | 15.548s | 687.8 MiB | 399.3 MiB | 2, 10, 0 |
| DBLP | 1 | 40.852s | 14.022s | 1,979.0 MiB | 1,424.5 MiB | 95 |
| Orkut | 1 | 122.352s | 51.633s | 9,276.7 MiB | 10,161.0 MiB | 0 |
| LiveJournal | 1 | 92.790s | 39.583s | 3,945.0 MiB | 3,335.1 MiB | 5 |
| Reddit | 1 | 48.640s | 13.661s | 2,699.1 MiB | 1,581.1 MiB | 0 |

Twitch 的 `query_author_g5` 前 3 条也完成独立配对，两个求解器均得到 `379, 386, 573`。Github、Youtube、Orkut 和 Reddit 的测试包含最优权重为 0 的实例，验证了多个查询组共享同一顶点时的零边答案路径。

另外使用相同命令重新生成 Twitch，并在生成前后比较 `query_g4.txt`、`query_g16.txt`、`query_author_g5.txt` 与 `dataset_manifest.json` 的 SHA-256；四个文件均逐字节不变，确认固定种子复现不依赖本次文件写入顺序。

2026-07-18 再从原始 Musae 文件在独立临时目录生成 `g=4..16` 各 5 条查询，13 个文件的 group-id 记录均逐行等于正式 300 条数据的前 5 条，作者 `g=3/5/7` 三个转换文件也逐字节相同。用当天重新构建的 Release/O2 二进制求解 `g4` 前 5 条时，DPBF 与 ReleaseV5 都得到 `164, 285, 196, 199, 34`；这同时检查了转换器、仓库图／查询接口和两个求解器入口。临时目录已在核验后删除。

最终重新构建后又把同一组 5 条 Musae `g4` 查询写入永久复核目录 `result_snapshot/gpu4gst_validation/20260718_rebuild_check`。DPBF 与 ReleaseV5 的总查询时间分别为 `0.551193s` 和 `0.347024s`，五个权重仍逐条一致；该记录仅用于构建后回归，不替代上表的 8 库接入结果。

同日还用正式 `query_g16.txt` 对 Musae 第 1 条做了范围上界 smoke。ReleaseV5 在 `142.125133s` 内返回权重 `555`，query peak RSS 为 `444.508 MiB`。这说明 `g=16` 文件能够经由正式加载和求解入口完整运行；单条结果没有独立 oracle，也不能描述 `g=16` 的时间分布，因此不并入性能加速或正确性配对统计。

大图单条结果只证明**格式、加载、查询选择和求解链路可运行**，不能支持总体性能结论。论文级性能比较仍应运行每个参数点的完整 300 条，并报告总时间、分布统计和逐查询配对；本轮没有把 q1 当成替代品，也没有启动高 `g` 的长时间全量求解。

### 5.3 ReleaseV5 与 PrunedDP++ 扩展面板

在 Musae、Twitch、Github 上又运行了冻结相关查询的 `g=4..15` 配对曲线。低 `g` 面板使用每库 20 条；成本上升后依次缩为 5、3、2 条，并在 `g=13..15` 改用逐实例 1000 秒 cutoff。结果显示 ReleaseV5 的总体加速从 `g=8` 的约 `4.77x` 增长到 `g=9` 的约 `16.86x`；`g=13..15` 全部完成，而 PrunedDP++ Strict 分别只完成 `5/9、4/6、2/6`。

这仍不是 8 库、每点 300 条的论文实验，高 `g` 的完成对统计还有删失偏差。完整表、逐查询反例、结果目录和投稿缺口统一见 `release_v5_target_and_publication_gap.md`；runner 及 batch/instance timeout 语义见 `../tools/release_v5_vs_pruneddp/README.md`。

## 6. 复现命令

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_prepare_gpu4gst -- /m:1
.\build\tools\gpu4gst_data\Release\gst_prepare_gpu4gst.exe `
  data_origin data all --seed 2025 --queries 300 --min-g 4 --max-g 16
```

不加 `--force-graph` 时，头部匹配的已有 `graph.txt` 会被复用，但工具仍扫描作者 `.in` 来重建连通分量并校验查询。完整参数和生成保证见 [`../tools/gpu4gst_data/README.md`](../tools/gpu4gst_data/README.md)。

## 7. 引用

1. Jiayu Li et al. *Fast Optimal Group Steiner Tree Search using GPUs*. Proc. ACM Manag. Data 3(6), Article 327, 2025. 本地原文：[`../data_origin/Li 等 - 2025 - Fast Optimal Group Steiner Tree Search using GPUs.pdf`](../data_origin/Li%20等%20-%202025%20-%20Fast%20Optimal%20Group%20Steiner%20Tree%20Search%20using%20GPUs.pdf)。作者数据仓库：<https://github.com/toziki/GPU4GST-sigmod>。
2. Shuang Yang, Yahui Sun, Jiesong Liu, Xiaokui Xiao, Rong-Hua Li, and Zhewei Wei. *Approximating Probabilistic Group Steiner Trees in Graphs*. PVLDB 16(2):343-355, 2022. 查询共现 BFS 协议见第 4.2 节：<https://www.vldb.org/pvldb/vol16/p343-sun.pdf>。
