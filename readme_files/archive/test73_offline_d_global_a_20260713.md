# Test73：Offline D + Goal-Directed Global A

更新时间：2026-07-13。Test73 检查一个此前未被 Test22/23 单独覆盖的正交组合：ordinary D 仍由 Test21 离线生成有序 rows，只有 anchored A 改成 goal-directed global label search，并直接把 D 的 root-irreducible values 当作宏分支。该状态精确且不使用 Hash，但 Toronto-fast g12 只减少少量 settled A labels，端到端显著退化，因此源码、CLI、CMake 与临时结果均已撤回。

## 1. 状态与接口

普通侧完全沿用正式 Test21：

```text
D(S,v), 1<=|S|<=h
```

D row 写入后，Test73 额外建立按 root 倒排的：

```text
(v -> [(branch mask, exact D cost)])
```

其中只包含正式 branch-bit 定理允许被 A 接入的 root-irreducible values。A 不再按 mask/size 逐 row 运行 Dijkstra，而维护：

```text
A(S,v): 覆盖 permanent anchor 与 S、root 为 v 的最小成本
```

单一全局 heap 的 settled label 执行：

1. 原图边传播；
2. 接入 root `v` 倒排表中的不交 D branch；
3. 接入 singleton group-distance branch；
4. 与补集的两张 ordinary D rows 做 `A+D+D` exact completion。

root-local label table 按 mask 有序并二分查找，没有 `unordered_map` 或经验阈值。所有 lower、anchor 选择和 best 更新与正式 Test21 相同。

## 2. 正确性

D rows 在 A 开始前已固定且精确。将每个 `(S,v)` 看作图上的 label：边传播和 `A+D` 都是非负转移，future bound 与正式版相同且 edge-consistent；因此按 `cost+lower` settle 后不会再出现更小值。root-irreducible branch 完备性沿用 Test21 的递归分解证明。

Release/O2 随机对拍：

```text
seed          713621
iterations    100/100
n             4..12
g             2..10
tolerance     1e-6
```

## 3. 决定性快测

Toronto-fast g12 q1：

| metric | formal Test21 | Test73 |
| --- | ---: | ---: |
| weight | `0.9616227800` | `0.9616227800` |
| wall | `1.791s` | `6.288s` |
| ordinary phase | `0.953s` | `0.930s` |
| A settled | `195,516` | `182,431` |
| A created | about `199k` stored values | `220,259` |
| A merge probes | `8.106M` | `6.756M` |
| completion scans/probes | `14.207M` | `19.397M` |
| peak RSS | `18.2MiB` | `51.2MiB` |

global order 只把 settled A 降低 `6.7%`，但 created labels、root-local containers、stale heap nodes 和逐 label point completion 抵消了全部收益。即使把这些接口常数优化到离线版水平，D2 rows 与绝大多数 A 状态仍未删除，不具备与 Test70 组合或运行 full DBLP 的结构依据。

## 4. 与既有路线的关系

- Test22 同时 global 化 D/A，Test23 只 global 化 D；Test73 补齐“D 离线、A global”的最后一个调度象限。三个结果共同说明，单纯迁移调度不改变 rooted state family。
- 本轮窄检索未发现能直接删除一般图 root 维的 exact half/three-block变换。[Iwata--Shigemura](https://ojs.aaai.org/index.php/AAAI/article/view/3965) 依赖实例 separator 做 DP pruning；[Fuchs et al.](https://doi.org/10.1007/s00224-007-1324-4) 的更快 exact 路线需要猜测额外 separator terminals。二者都不是本候选的调度来源，也不能绕过当前 D2 输出屏障。
- Test73 没有采用新的论文算法，不宣称原创贡献。

## 5. 结论

offline D + global A 是精确组合，但 B 的优先级优势不能靠只替换 A 的执行顺序移入 Test21。下一机制仍必须在 D2 进入图传播前改变状态定义，或者提供同时共享 pair/root 与 consumer masks 的 exact representation；不继续优化 global-A 容器、point completion 或 heap 常数。
