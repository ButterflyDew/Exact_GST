# ReleaseV6 发行实现：锚定双向半状态与渐进 dual

本文只说明 `release_v6.cpp` 的发行边界、代码结构、验证和运行方式。算法主线、状态语义、正确性和理论复杂度见 `release_v6_method_cn.md`；逐组件排重与完整方法贡献分别见 `test152_novelty_audit.md` 和 `test152_integrated_method_review.md`。

## 1. 发行定位

ReleaseV6 是 Test152/Test157 冻结 M5 的独立发行实现。它保留 permanent group anchor、ordinary `D`、低层 anchored `A`、高层 closure-aware `F/H`、transposed terminal 和渐进 dual；不调用 Test80、Test152、ReleaseV5 或任何旧 Release。

发行文件只有：

```text
methods/Release/release_v6.h
methods/Release/release_v6.cpp
```

`release_v6.cpp` 约 2556 行。长度主要来自三种有序行布局及其精确求交、普通／锚定／反向三类图闭包和按根转置终端；这些逻辑没有隐藏到 Test 开关中。源码只复用正式 Common helper：`anchor_junction_upper` 负责锚路径／锚树可行上界，`dual_cut_potential` 负责合法组势、changed-arc residual closure、渐进前缀与剩余容量打包。

## 2. 一次查询的代码主线

`SolveOneQuery` 只执行一条路线：

1. 检查查询可行性，计算每个组到全图的距离，以共同根星形方案初始化 `best`，并选择永久锚点组。
2. 构造组间路径下界、锚路径和压缩锚树；dual 此时尚未强制构造。
3. 按 `|S|=1..floor(g/2)` 生成 ordinary `D` 行。每张非单组行统计实际 seed、queue pop 和 edge relaxation 工作，并按 `2m+n` 的结构成本逐组推进 dual 前缀。
4. 在 ordinary 阶段使用三块可行解、锚路径 facility、锚树 facility 和可选 residual packing 收紧 `best`；这些模块只产生合法 upper/lower bound，不改变状态定义。
5. 生成 `|S|<=floor((floor(g/2)-1)/2)` 的低层 `A` 行，并直接结算低层 `A+D+D` 完整候选。
6. 按根读取 ordinary 行，将一块或两块不相交普通状态转置为高层 `F` 的稀疏终端事件。
7. 按 mask 大小递减生成高层 `H=C(F)`，每完成一行就结算所有第一条低／高跨切分边。
8. 全部高层依赖和边界处理完毕后返回 `best`。

这里没有“先跑 A，代价高时再从头跑 B”。低层 `A` 与高层 `H` 是同一锚定状态依赖图的 inside/outside 两侧，ordinary `D` 只计算一次并由两侧共享。

## 3. 数据表示

### 3.1 `OrdinaryRow`

普通、锚定和反向值共用同一种行容器。每张行按真实字节数在三种布局中选最小者：

- 递增 `vertices + distances` 稀疏表；
- 顶点域 `distances` 稠密数组；
- `vertex_bits + rank_before_word + distances` ranked bitmap。

ordinary 行另有 `branch_bits`，标出 root-irreducible 值。布局选择只比较容器实际字节，不使用数据集、固定 `g` 或经验密度阈值。行始终按顶点编号有序；共同根操作使用归并、二分或位图交集，不使用 Hash。

### 3.2 图闭包

每张非平凡状态行先离线形成同根 seed，再用 `std::priority_queue` 执行多源 Dijkstra。队列键为“当前真实成本 + admissible future lower bound”，弹出和松弛前都用当前 `best` 剪枝。源码保留 stale entry，理论复杂度文档按仓库约定采用与 Fibonacci heap 一致的标准图闭包口径，实际实现不伪装 decrease-key。

### 3.3 转置终端

源码按 64 个连续根为一批，从全部 ordinary 行流式收集当前根上的 `(mask,value)`。对固定根先计算 dual reduced cost 并排序，再依据精确候选数选择全局有序 pair 扫描或 complement-submask 扫描。两条路线枚举相同的不相交事件，最终写入 `<target,root>` 的是真实代价而非约化代价。

## 4. 渐进 dual 的发行实现

ReleaseV5 在查询开始前构造全部组势；ReleaseV6 改为冻结的 Test152 调度：

```text
bank += seed_candidates + queue_pops + edge_relax_attempts
while bank >= 2m+n and dual is incomplete:
    bank -= 2m+n
    build the next group potential
```

组顺序由 `dual_cut_potential` 按共同根距离确定。`DualAt` 和 `DualGroupAt` 在尚无势时返回零，在 partial 前缀时只累加已构造组；这始终是合法下界。只有全部 `g` 组构造完成后，源码才调用 `RecoverProgressivePrimal` 更新可行上界，并允许在二组 ordinary 行尚未结束时购买 residual packing。partial dual 绝不被当作 primal upper bound。

当完整 dual 在二组层之后才完成时，后续已经没有足够 ordinary 层摊销 packing，源码直接释放 residual；查询结束前也统一释放 residual。势数组仍保留给 `D/A/H` 的剪枝和 transposed terminal 使用。

该调度没有拟合常数。`2m+n` 对应一组势必须接触的双向 residual arc 域与顶点势域，是确定性的结构记账；文档不把它宣称为严格竞争比或最优购买策略。

## 5. 从研究源码删除的内容

ReleaseV6 没有以下研究设施：

- M0--M4 消融、M5 编译宏和 all-branches 变体；
- row validator、core oracle、阶段计时、事件计数和 probe 输出；
- `PROGRESS`、调试日志和只服务结果分析的数组；
- eager/partial dual 运行时切换；
- 已撤出的 quarter upper 路径；
- 数据集名、query id、固定 `g`、density、wall-time 或观测进度分支；
- 对 Test 或旧 Release 的函数调用。

保留的所有阈值都有结构来源：`floor(g/2)` 来自平衡三分解，锚定切分来自锚定格深度，`ceil((g-1)/3)` 来自三块覆盖，`2m+n` 来自 dual 的结构工作域。行布局和两条转置枚举路线比较的是本次操作的精确字节或候选数，不是经验超参数。

## 6. 正确性继承与发行验证

核心正确性不依赖发行代码中的统计设施。ReleaseV6 保留 Test157 已验证的四项接口：

1. strict branch 受限 seed 与 canonical ordinary seed 等价；
2. `F` 位于闭包后节点，`H=C(F)` 位于前驱合并根，二者不能混用；
3. 任一进入高层的正向推导有唯一第一条跨切分边；
4. transposed terminal 与逐目标普通二分之间存在逐事件双射，dual 只重排和安全过滤。

本次发行抽取完成以下验证，全部使用 MSVC Release `/O2 /Ob2`：

| 验证 | 结果 |
| --- | --- |
| 随机小图 V6 对 DPBF，seed `160001`，`n=4..11`、`g=2..9` | `500/500` 权重在 `1e-6` 内一致 |
| GPU4GST Twitch `g=10` q1--q20，V6 对 M5 | `20/20` 权重一致 |
| GPU4GST Musae `g=13` q1--q5，V6 对 M5 | `5/5` 权重一致 |
| Release/O2 构建 | `gst_release_v6_main` 成功 |

同源真实查询的抽取回归为：

| 数据集 | M5 总时间 | V6 总时间 | M5/V6 | M5/V6 最大 query peak RSS |
| --- | ---: | ---: | ---: | ---: |
| Twitch `g=10`, 20 条 | 28.475907s | 27.924329s | 1.020x | 88.773 / 88.977 MiB |
| Musae `g=13`, 5 条 | 12.956268s | 11.903326s | 1.088x | 69.812 / 70.184 MiB |

时间结果只证明清理没有回退，不作为新的算法消融。RSS 差异很小且方向混合，也再次说明 ReleaseV6 不声称稳定降低实际内存。原始结果位于 `result_snapshot/v6_release_validation/`。

## 7. 构建与运行

```powershell
cmake --build build --config Release --target gst_release_v6_main -- /m:1
```

运行格式与其他方法一致：

```powershell
.\build\Release\gst_release_v6_main.exe DBLP result g10 data 1 5
.\build\Release\gst_release_v6_main.exe GPU4GST_Twitch result query_g10.txt data 1 20
```

每个 `weights.txt` 数据行仍为：

```text
solver_seconds  best_weight  query_peak_rss_mib
```

第三列是该查询进入 solver 到返回期间的逐查询绝对 peak RSS，不是进程生存期前缀峰值。多查询仍在同一进程中加载一次图。

## 8. 代码审阅结论

ReleaseV6 当前满足发行边界：单一算法路线、独立源码、无研究开关、无调试输出、无数据特判、无 Hash 主表、无旧 Release/Test 调用。相对 ReleaseV5 的算法差异只有两项：删除已撤出的 quarter upper；把 eager full dual 替换为经 Test157 冻结的 progressive prefix dual。其余变化是命名和发行入口。

源码仍然较长，因为三类状态共享但不等价的求交与闭包逻辑必须显式存在。进一步缩短应通过经过独立验证的正式 row/closure 公共组件重构，而不能把代码重新藏到 Test80、删除边界情况或合并 `F/H` 语义。

## 9. 引用与贡献边界

ReleaseV6 的 rooted subset DP、半状态 MITM、一般 outside、subset packing、directed-cut dual、Dijkstra、有序列表和位图均有已知来源，不能逐项宣称原创。论文方法贡献是这些构件在 exact GST 中形成的完整锚定双向求值组织；渐进 dual、facility 和 packing 属于可消融的执行优化。

主要原文和允许／禁止的 claim 已集中在 `release_v6_method_cn.md` 与 `test152_integrated_method_review.md`，发行文档不再复制一套可能漂移的引用判断。投稿前仍需补传统五库冻结多询问和合适 Linux/CUDA 机器上的 GPU4GST 同机对照；这些是实验章节缺口，不改变 ReleaseV6 的方法定义。

