# Build And Run

当前主要入口：

```text
ReleaseV1   small g 稳健、half-DP 单路径发行版
ReleaseV2   dual-anchored global-label 大 g 发行版
ReleaseV3   ordered half rows 与 farthest-goal global 的统一发行版
ReleaseV4   anchor-aware ordered rows 的首个纯 A 冻结版
ReleaseV5   eager dual 的历史纯 A 冻结版
ReleaseV6   anchored D/A/F/H + transpose + progressive dual 当前发行版
Test152     ReleaseV6 的研究统计来源；Test157 提供同源消融与 verifier
```

先读 `readme_files/test_series_overview.md`。当前发行入口为 `gst_release_v6_main`；算法方法见 `readme_files/release_v6_method_cn.md`，发行实现与验证见 `readme_files/release_v6.md`。

`gst_release_v5_main` 保留为 eager dual 历史对照，`gst_release_v4_main` 保留为较早的纯 A 冻结版，`gst_release_v3_main` 保留为框架 B 对照。ReleaseV6 从冻结 M5 独立清理得到，不调用 Test 或任何旧 Release。

## Build

计时只使用 Release/O2；CMake 已显式设置 MSVC `/O2 /Ob2` 或其他编译器 `-O2`：

```powershell
cmake -S . -B build
cmake --build build --config Release --target gst_release_v6_main gst_release_v5_main gst_release_v3_main
```

常用目标：

```text
gst_dpbf_main             正确性 oracle
gst_pruned_dp_main        主 baseline
gst_release_v1_main       ReleaseV1
gst_release_v2_main       ReleaseV2
gst_release_v3_main       ReleaseV3
gst_release_v4_main       ReleaseV4
gst_release_v5_main       ReleaseV5
gst_release_v6_main       ReleaseV6
gst_test16_main ... gst_test19_main
gst_test21_main / gst_test80_main
gst_test80_progress_main  Test80 分层观测目标，仅增加 stderr 日志
gst_test80_d1_probe_main  Test80 D1 自然边界快速诊断，不产生最终答案
gst_test80_d2_probe_main  Test80 D2 自然边界快速诊断，不产生最终答案
gst_test80_d3_probe_main  Test80 D3 截止诊断，不产生最终答案
gst_test80_anchor_facility_probe_main  Test80 锚路径设施上界 D3 探针
gst_test80_anchor_facility_main  Test119 锚路径设施上界候选
gst_test80_anchor_facility_d3_main  Test119 D3 截止门
gst_test80_anchor_facility_d4_main  Test119 D4 截止门
gst_test80_anchor_tree_probe_main  压缩锚树 D-block 设施上界 D3 只读探针
gst_test80_anchor_tree_main  压缩锚树 D-block 设施上界候选
gst_test80_anchor_tree_d3_main  压缩锚树 D-block 设施上界 D3 截止门
gst_test142_adjoint_anchor_main  Test142 伴随锚点格活动候选
gst_test142_adjoint_anchor_progress_main  Test142 同算法分层日志目标
gst_test144_adjoint_eager_boundary_main  Test144 逐 H 行即时边界候选
gst_test145_transposed_terminal_main  Test145 directed-cut 转置终端候选
gst_test146_amortized_anchor_tree_main  Test146 按实际工作量摊销锚树上界
gst_test146_amortized_anchor_tree_d2_main / _d3_main  Test146 D2/D3 前缀诊断
gst_test149_changed_arc_dual_main  Test149 只从已改写弧启动 dual 闭包
gst_test149_changed_arc_dual_d2_main / _d3_main  Test149 D2/D3 前缀诊断
gst_test157_production_audit_main  冻结 M5 同算法的三类 row 不变量审计目标
gst_test158_all_branches_main  冻结 M5 的“发布全部普通 branch”单变量消融
gst_dual_cut_probe        full/packing/progressive dual 独立 verifier
gst_random_compare        黑盒随机对拍
gst_snapshot_prepare      snapshot 数据生成
```

`gst_test149_changed_arc_dual_main` 是 ReleaseV5 的研究来源：它完整保留 Test146，只把 sequential dual 的残量闭包初始化从“每组扫描全部有向弧”改为“只扫描此前势函数可能降低过的有向弧”。未改写弧由原始组距离的三角不等式证明不可能成为初始违反点；Dijkstra 传播仍读取完整邻接表。该主线已经冻结进 ReleaseV5，Test149 继续保留研究统计，ReleaseV4 则保持历史冻结状态。

`gst_test152_progressive_dual_main` / `gst_test157_m5_progressive_dual_main` 是 ReleaseV6 的研究来源：它们把 eager dual 改为按 ordinary 实际工作量逐组购买的合法势前缀。ReleaseV6 删除统计和消融开关，只保留同一单一路线。

`gst_test80_progress_main` 与 `gst_test80_main` 使用同一算法源码，只在编译期启用 D/A 层完成日志。它用于长查询前的分层进度门，不允许据 wall time 或进度改变算法策略。建议串行构建该诊断目标，避免 Windows 下多个同源目标竞争中间对象：

```powershell
cmake --build build --config Release --target gst_test80_progress_main -- /m:1
```

`gst_test80_d1_probe_main`、`gst_test80_d2_probe_main` 与 `gst_test80_d3_probe_main` 分别在对应 D 层自然完成后返回，用于跨 query 筛查状态增长并尽早否决弱候选。`weights.txt` 会写无解标记，**不能作为 GST 答案或完整时间引用**；有效数据只有 progress 日志和 stats 中已完成层的字段。探针只改变外部停止边界，不按 wall time、数据集或观测值切换求解策略。

Test157 的 M0--M5 同源消融目标为 `gst_test157_m0_iwata_main`、`gst_test157_m1_full_a_main`、`gst_test157_m2_adjoint_main`、`gst_test157_m3_transpose_main`、`gst_test157_m4_pruning_main` 和 `gst_test157_m5_progressive_dual_main`。六者调用同一 Test80 solver 源码，stats 以 `ablation_stage=0..5` 标识配置；阶段定义和当前结果见 `readme_files/test157_same_source_ablation.md`，公平 M0 的算法与验证见 `readme_files/test157_m0_fair_reference.md`。冻结 M5 的三图 `g=11..16` 协议、查询散列和结果见 `readme_files/test157_m5_g11_g16_frozen.md`。历史 `gst_half_dpbf_main` 使用稠密半层表和任意小集合覆盖终端，仍不属于该消融。

Test158 只把冻结 M5 的 strict root-irreducible branch 改为发布全部 settled 普通状态，用于测量该规范化基的独立作用。它是编译期研究消融，不是候选运行时开关；500 个 DPBF 随机实例与两轮 240 对真实查询均保持答案一致。完整结果和“保留为工程辅助、不能作为论文核心”的评审见 `readme_files/test158_root_irreducible_branch_ablation.md`。

生产审计目标与独立 dual verifier：

```powershell
cmake --build build --config Release --target `
  gst_test157_production_audit_main gst_dual_cut_probe -- /m:1

.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe `
  --packing-self-check 15715703 3000 7 11
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe `
  --progressive-self-check 15715706 2000 7 11
```

`gst_test157_production_audit_main` 只增加编译期 row invariant 检查，不作为性能结果；`gst_dual_cut_probe` 不调用 Test80。当前检查范围、计数和 M5 单一路线判定见 `readme_files/test157_production_audit.md`。

`gst_test80_anchor_facility_probe_main` 在同一 D3 截止边界额外测量锚路径设施上界。它复用 junction 已计算的锚路径距离，将每张现有 D 行归约为到该路径的最小合法接入代价，再用无块数限制的规范化 subset partition DP 组合全部非锚组。该目标只输出候选上界，**不把候选写回 incumbent**，因此用于判断机制价值而不是报告正式答案。

## Solver CLI

```text
<exe> [graph_selector] [result_root] [query_selector] [data_root] [query_begin] [query_limit]
```

```text
graph_selector  图名、data_root 下的 1-based 编号或图目录路径
result_root     结果根目录，默认 result
query_selector  query.txt，或 g13 表示 query_g13.txt
data_root       数据根目录，默认 data
query_begin     1-based 起始编号，默认 1
query_limit     运行条数，默认 -1 表示到文件末尾
```

PrunedDP++ baseline 在六个公共参数之后接受三个独立复现开关：

```text
--state-storage=hash|dense  (default: hash)
--mst-upper=on|off          (default: on)
--lb2-pathmax=on|off        (default: on)
```

默认配置尽量复现论文完整 Algorithm 4，但 `lb2-pathmax=on` 保留原文的正确性不确定性；关闭它时实现使用原始 admissible `lb_2` 并允许更小 `g` 重开状态。MST 的严格实现也会增加论文复杂度分析未计入的 witness 恢复与 Kruskal 成本。定义、证明边界和验证见 `readme_files/pruneddp_reproduction.md`。

示例：

```powershell
.\build\Release\gst_release_v2_main.exe Toronto result query data 1 1
.\build\Release\gst_release_v1_main.exe Toronto result g10 data 1 1
.\build\Release\gst_pruned_dp_main.exe DBLP result g10 data 1 1
.\build\Release\gst_pruned_dp_main.exe Toronto result g10 data 1 5 `
  --state-storage=dense --mst-upper=off --lb2-pathmax=off
.\build\Release\gst_release_v3_main.exe Toronto result g13 data 1 1
.\build\Release\gst_release_v4_main.exe DBLP result g13 data 1 1
.\build\Release\gst_release_v6_main.exe DBLP result g13 data 1 1
```

## Output

```text
<result_root>/<graph>/<method>/<query_subdir>/weights.txt
<result_root>/<graph>/<method>/<query_subdir>/<method_lower>_stats.txt
```

`weights.txt` 每个非 header 行为：

```text
time_seconds best_weight query_peak_rss_mb
```

第三列是该条询问从进入 solver 到返回期间，以 `1ms` 间隔采样当前进程 RSS 得到的最大值，单位为 MiB。它是**逐询问的绝对 peak RSS**，包含已经加载的图、组与求解器状态；不是询问结束瞬间的 RSS，不减去 `rss_before`，也不是进程启动以来的历史峰值。图和全部 query 仍然只加载一次，多询问运行不会为测量空间而重启进程。

选择这一口径有三个原因。第一，结束瞬间 RSS 会漏掉已经释放的中间状态，不能表达求解期间所需的最大物理内存。第二，Windows `PeakWorkingSetSize` 和 Linux `ru_maxrss` 都是进程生存期峰值，同一进程连续运行 query 时会退化为前缀最大值，不能作为逐询问结果。第三，DB 论文通常报告查询执行期间的 peak memory，而不是结束快照；例如 PVLDB 的 [SimPush](https://www.vldb.org/pvldb/vol13/p966-shi.pdf) 明确把输入图、索引和其他查询结构计入 peak memory，[TripleBit](https://www.vldb.org/pvldb/vol6/p517-yuan.pdf) 比较查询执行期间的 peak physical/virtual memory。系统字段定义见 [Microsoft `PROCESS_MEMORY_COUNTERS`](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters) 与 [Linux `getrusage(2)`](https://man7.org/linux/man-pages/man2/getrusage.2.html)。因此论文主空间指标采用 **query peak RSS**；stats 中的 `rss_before_mb` 和 `rss_after_mb` 仅用于诊断。

2026-07-14 以前生成的历史 `weights.txt` 只有前两列，读取器继续兼容。文件按 run header 追加；比较结果时必须定位最后一个 header，并读取该次 run 的行，不能默认读取文件开头。无解权重为 `-1`。

## Random Compare

```powershell
.\build\Release\gst_random_compare.exe `
  .\build\Release\gst_release_v6_main.exe `
  .\build\Release\gst_dpbf_main.exe `
  ReleaseV6 160001 300 4 11 2 9 `
  .tmp_random_compare_release_v6 0
```

完整参数：

```text
gst_random_compare <method_exe> <dpbf_exe> <method_name>
                   [seed] [iterations] [min_n] [max_n] [min_g] [max_g]
                   [work_root] [keep=0] [method_extra_args...]
```

工具为每个实例分别运行目标方法和 DPBF，以 `1e-6` 比较最后一次权重。`method_extra_args` 只传给目标方法，可用于对拍 PrunedDP++ 开关。`keep=0` 删除 case 子目录；运行结束仍需删除空 work root。

## Snapshot

```powershell
python tools\snapshot_benchmark\snapshot.py `
  --method ReleaseV6 --suite small --no-prepare

python tools\snapshot_benchmark\snapshot.py `
  --method ReleaseV6 --suite fast --no-prepare
```

suite 与结果格式见 `readme_files/snapshot_benchmark.md`。

## GPU4GST Datasets

`data_origin` 中的 GPU4GST 作者数据可转换为仓库原生的 1-based 文本图与查询接口。作者 `g=3/5/7` 查询和按相关组共现 BFS 生成的 `g=4..16` 查询分别保存，不混用：

```powershell
cmake --build build --config Release --target gst_prepare_gpu4gst -- /m:1
.\build\tools\gpu4gst_data\Release\gst_prepare_gpu4gst.exe `
  data_origin data all --seed 2025 --queries 300 --min-g 4 --max-g 16

.\build\Release\gst_release_v6_main.exe GPU4GST_Twitch result query_g4.txt data 1 5
.\build\Release\gst_dpbf_main.exe GPU4GST_Twitch result query_author_g5 data 1 3
```

数据版本差异、查询生成定义、逐数据集规模与当前测试结果见 `readme_files/gpu4gst_datasets.md`；工具参数见 `tools/gpu4gst_data/README.md`。

## Run Discipline

1. 正确性以 DPBF、`1e-6` 和 Toronto 现有最后一次结果为准。
2. benchmark 使用 Release/O2，并在文档中标出旧非 O2 数字。
3. 运行后清理 `.tmp_random_compare*`、`result_tmp*`、空结果目录和残留 solver 进程。
4. DBLP g13 `q1` 只作为 correctness/smoke 与历史锚点，不能单独支持整体性能结论。阶段门使用固定跨询问 panel；发行或论文结论使用完整查询集，并报告逐询问配对结果、总时间、中位数与 P90。
5. panel 只决定外部评测覆盖范围，不参与算法内部决策。没有结构性突破时不重复长门；已有 q1 结果也不因纯文档或格式调整重跑。
6. 不使用数据集、固定 `g`、层级、密度或运行时刻特判，也不把 baseline 可用的普通压缩记作方法贡献。
