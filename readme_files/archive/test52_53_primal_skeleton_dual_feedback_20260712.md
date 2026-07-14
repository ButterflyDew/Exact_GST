# Test52--53：Primal Skeleton 对 Dual 的结构反馈

更新时间：2026-07-12。Test52--53 检查 Test48 的 paid branch-junction skeleton 能否不只提供 incumbent，还能决定 directed-cut future 的 orientation。Test52 恢复 facility assignment 与实际付费父树，取 attachment-token centroid；随后分别测试 centroid 单根、root-star/centroid 双 dual 取 max。Test53 再按各组 attachment 到 centroid 的付费树距离重排一次 root-star dual ascent。三个版本都精确，但没有同时通过 fast 与 full-D2 时间门禁，全部源码、公共 API、统计字段和探针目标已撤回。

## 1. Attachment-Token Centroid

Test48 的压缩树 DP 可恢复：

- 每个 nonanchor group 由哪个 facility root 服务；
- 哪些 compressed child edges 实际付费；
- 已付费 anchor path `P`。

把每个 group token 放在其 facility，并把 anchor token 放在 `P` 的 anchor 端点；服务于同一 facility 的 tokens 视作独立虚叶。Test52 在恢复的 paid tree 上取 weighted centroid，使删除该 graph vertex 后任一 attachment-token component 不超过半数。该规则只依赖已恢复 primal witness，没有数据集、`g`、时间或经验阈值。

full DBLP g13 q1 的结构为：

```text
root-star root          24492
attachment centroid    211842
paid upper       13.0199888930
path vertices               4
candidates / compressed 25 / 25
```

## 2. Centroid 单根

用 centroid 替换 root-star 作为唯一 dual root，通过随机 `100/100`。在相同 Test48 upper 下，fast Toronto g12 q1 的 D2 明显减少：

```text
Test48 root dual values/pops   54,653 / 89,803
centroid dual values/pops      47,315 / 76,036
query time                     1.119s -> 0.957s
```

但收益不跨库：五库 g12 q1 中另外三库的 centroid 与 root-star 相同，Toronto-new 只减少 23 个 D2 values；DBLP-new 的另一 fast 层还出现状态反向。完整 fast20 为 `9.840s`，慢于 Test48 的 `9.758s`，因此单根替换不保留。

## 3. 两个合法 Dual 取 Max

两个 rooted future 都满足 admissibility、edge consistency 与 subset splice，因此逐 state 取 max 仍合法。随机 `300/300`（seed `713111`）通过。full DBLP 在 Test48 best 下对 66 个 nonanchor pairs、`164,853,612` 个 pair-root positions 的精确扫描为：

```text
centroid dual stronger positions  69,018,493
root dual stronger positions      77,046,839
root surviving seeds               1,078,354
max surviving seeds                  913,008  (-15.3%)
```

说明两种 orientation 确实互补。但 D2-only 完整 replay 否决了时间收益：

```text
                         root dual       max dual
settled                  10,065,111      9,096,751   (-9.6%)
pushes                   13,494,916     12,000,580  (-11.1%)
pair replay wall             40.313s         43.615s
```

第二次 dual 构造另需 `39.121s`，root dual 为 `38.574s`。即使忽略构造成本，逐 state 计算第二个 subset sum 已使 replay 变慢。fast20 也从 Test48 `9.758s` 退到 `10.130s`。所以不能用 deferred purchase 包装：候选的消费本身不回本，问题不只是购买时刻。

## 4. Skeleton-Ordered 单 Dual

Test53 试图把互补信息压进一次 residual allocation：保持 root-star root，但按每组实际 attachment 到 paid-tree centroid 的距离从远到近处理 groups，平局按 group id。任意 group order 的 sequential directed-cut ascent仍给出合法 potentials；随机 `300/300`（seed `713121`）通过。

结果反而更差：fast20 为 `11.565s`。primal attachment 深度不能替代原来按 root group-distance 排序的 dual order；更改 residual charge 顺序会使 Toronto-new 和 MovieLens 明显膨胀。该公共 `BuildOrdered` API 已删除。

## 5. 结论

1. paid skeleton 能给出与 root-star 不同且局部更强的合法 dual orientation；primal-to-dual feedback 本身是真实结构，不是计时噪声。
2. 两个 orientation 的 max 在 full D2 上删除约一成 states，但第二次 dual 构造和双 subset-sum 消费超过收益。
3. 用 attachment depth 直接重排一次 dual 不能无损吸收互补信息。
4. 下一机制若继续 primal/dual coupling，必须复用一次 residual construction或产生可 `O(1)` 合并的共同证书；不能再建多个完整 potentials，也不能只换 group order。

Test52--53 没有采用新的论文算法。directed-cut dual 的文献来源仍是 Wong；attachment centroid 与 skeleton-order 是本轮仓库候选，不新增引用，也不宣称论文级原创性。
