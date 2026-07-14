# Test51：共享路径的边际 Junction 端点

更新时间：2026-07-12。Test51 检查 Test48 的压缩父树是否因为每个 triple 只保留一个“独自支付接入路径”的最优 root 而漏掉共享路径协同。原型同时保留路径付费系数 `lambda=1` 与路径已付费系数 `lambda=0` 的精确最优 root，再运行同一棵压缩父树上的 subset facility DP。fast 两库有更强上界，但 full DBLP g13 完全不变，因此代码、CMake 目标和二进制已撤回。

## 1. 结构动机

固定 Test48 的 paid anchor path `P`，令 `d_P(x)` 为到 `P` 的距离，triple `S` 在 root `x` 的同根 star 成本为：

```text
q_S(x) = sum(i in S) gd_i(x).
```

Test48 只保留：

```text
x_1(S) = argmin_x q_S(x) + d_P(x).
```

这对应该 block 是某个父树分支的首个使用者，需要支付完整接入路径。当同一分支已经被另一个 block 激活，共享前缀的边际费用为零；另一个由付费状态严格推出的端点是：

```text
x_0(S) = argmin_x q_S(x).
```

Test51 对每个 triple 同时加入 `x_0,x_1`，把它们到 `P` 的确定性 shortest-path parent union 压缩，再由 tree DP 对每条实际启用的父树边只收费一次。两个端点来自“未付费/已付费”二值状态，不是经验 lambda、数据集或层级特判。

## 2. Test48 复现校验

独立 Release/O2 探针在 full DBLP g13 q1 的 `lambda=1` 路径得到：

```text
paid upper          13.0199888930
Test48 archive      13.0199888930
unique candidates   24
compressed vertices 20
convolutions        10,628,820
```

数值逐位复现 Test48 的已归档 upper，说明路径、候选和压缩树 DP 口径一致。本次只运行约 58 秒的结构探针，没有启动 Test21 rows、D2 或 full solver。

## 3. Fast 与 Full 结果

五个 generated-fast g12 q1：

| dataset | paid-only | marginal endpoints | 变化 |
| --- | ---: | ---: | ---: |
| Toronto | `0.97058290` | `0.96886855` | 改善 |
| Toronto-new | `4.00328729` | `4.00328729` | 不变 |
| DBLP | `12.22614400` | `12.22614400` | 不变 |
| DBLP-new | `11.00340480` | `10.94716280` | 改善 |
| MovieLens | `0.0202272822` | `0.0202272822` | 不变 |

但 Toronto 的压缩树从 `28` 扩到 `129` 个 vertices，tree convolutions 从 `4.96M` 增至 `22.85M`。full DBLP g13 q1 为：

```text
paid-only upper        13.0199888930
marginal upper         13.0199888930
paid candidates/tree   24 / 20
marginal candidates    84
marginal tree          86
marginal convolutions  45,703,926
```

新增 60 个 exact marginal roots、压缩树扩大 `4.3x` 后，决定性 full upper 没有任何改善。故 fast 的局部正信号不足以触发 solver 集成或 DBLP 长跑。

## 4. 结论

Test51 排除了“Test48 只缺少共享路径免费时的 facility roots”这一解释。在 full DBLP 上，原来的 `lambda=1` triple roots 已足以达到该 parent-tree facility family 的最优值；继续加入 lambda breakpoints、局部 minima 或更多同树 facilities 没有新的数量级依据。

下一候选应改变 skeleton 与 exact search 的接口，例如让 primal skeleton 决定一个有证明的 lower-bound orientation，或直接承担 D2 的连接证书；不能继续扩同一父树上的 facility 集合。Test51 没有采用新的论文算法，也不新增引用。
