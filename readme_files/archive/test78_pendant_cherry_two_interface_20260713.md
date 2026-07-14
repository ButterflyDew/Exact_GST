# Test78: Pendant Cherry 与第二接口边界

更新时间：2026-07-13。本文记录 Test76 之后对“多个 `D3/D4` 块拼接 best”的结构研究。正式 `Test21`、`ReleaseV3` 均未修改；本轮没有运行 full DBLP。

## 1. 结论

本轮得到两个精确的 fixed-plan 恒等式，但也找到了阻止它们直接成为完整 solver 的边界：

1. 3--5 个独立 rooted macro labels 可以用 cherry 枚举精确定价；四标签只需要闭包其中一个 cherry。
2. 固定三个 block 与至多四个 core singleton 时，把 core labels 分配给三条 block-anchor arms，再在同一 junction 相交，也是精确的。
3. 三度 caterpillar 给出纯组合反例：并非每棵 12/13-token tree 都含有三个互不相交的 3/4-token 单接口块。因此 simultaneous three-arm family 一般不完备。
4. 顺序提取仍然成立，但后取 block 可能承载先前 attachment；若沿这条路继续，状态必须保存第二接口、已付费主干或等价的 attachment geometry。

这不是局部实现常数问题。下一步必须研究可共享的 two-interface 表示，不能继续扩张 single-root row 数量。

## 2. Cherry 定价恒等式

设每个 macro label `i` 已有 metric-closed rooted row `f_i(v)`，并定义

```text
P_ij(v) = C(f_i + f_j)(v).
```

则：

```text
3 labels: min_v f1(v)+f2(v)+f3(v)

4 labels: enumerate the three pairings
          min_v P_ab(v)+f_c(v)+f_d(v)

5 labels: choose the unmatched label e and pair the other four
          min_v P_ab(v)+P_cd(v)+f_e(v)
```

四标签公式只闭包一个 pair。对任意四叶 macro tree，取另一个 cherry 的分叉点为 `v`；闭包 pair 包含 central path，另外两棵 rooted trees 在 `v` 直接相交。反向每个候选都是合法完整树，所以等式精确。

Release/O2 随机验证：

| family | singleton groups | two-candidate groups |
| --- | ---: | ---: |
| 3--5 label cherry | `1000/1000` fixed g13；另有 `200/200` g9--g13 | `500/500` fixed g13；另有 `100/100` g9--g13 |
| four-label one-closure form | `1000/1000` fixed g12 | `500/500` fixed g12 |

这些结果验证的是 fixed-plan 代数，不证明某个 macro partition family 完备。

## 3. 三臂 Block-Anchor 恒等式

固定三个互不相交 blocks `B1,B2,B3` 与 core `Q`。令

```text
A_i(J,v) = 覆盖 Bi 与 J、root 为 v 的最小 macro tree，J subset Q。
```

完整 fixed-plan 价格为

```text
min over Q = J1 disjoint-union J2 disjoint-union J3, v
    A_1(J1,v) + A_2(J2,v) + A_3(J3,v).
```

证明方法是取三个 block subtrees 的 tree median。每个 core branch 分配给它所在的 arm；在 median 处分出的 core-only branch 可分配给任意一条 arm。三条 rooted trees 除 median 外边不重叠。反向三行相交显然给出合法 macro tree。

该恒等式与完整 macro DP 的 Release/O2 对拍为：

```text
g=9..13 singleton groups        300/300
g=9..13 two-candidate groups    150/150
fixed g13 singleton groups     1000/1000
fixed g13 two-candidate groups  500/500
```

实现位于 `tools/paid_half_state_probe/paid_half_state_probe.cpp`，只使用 dense semantic oracle，不进入正式 solver。

## 4. 单接口完整 Family 的反例

先看不依赖图权和数据集的组合反例。取一棵有 12 个 token leaves 的三度 caterpillar：spine 两端的内部点各挂两个 leaves，其余八个内部点各挂一个 leaf。

在没有内部 token 的三度树中，一个大小为 3 或 4 的单接口 leaf block 必须是某条边一侧的 leaf set。caterpillar 上这样的集合只能是 spine 的 3/4-token 前缀或 3/4-token 后缀；两个前缀彼此嵌套，两个后缀也彼此嵌套。因此最多只能同时选择一个前缀和一个后缀，不能得到三个两两不交的 3/4-token 单接口块。

所以“连续取三次 pendant block”不能改写成“原树同时存在三个 pendant leaves”。顺序提取时，较晚 block 所在的余树可能已经承载较早 block 的 attachment。

曾尝试枚举三个大小为 3--4 的 blocks，并把剩余至多四个 groups 当成一个 rooted core atom。g12 一共有 `275,275` 个 plans。即使用 `q=6` 的完整 dense rooted rows 对全部 plans 精确定价，也得到：

| fast graph | family best | exact |
| --- | ---: | ---: |
| Toronto | `0.9688685500` | `0.9616227800` |
| DBLP | `12.1663030000` | `12.1663030000` |

Toronto 的差值与 Test76 的 single-interface family 边界一致，不是筛选遗漏。四标签 one-closure 恒等式已经独立通过随机验证，因此缺口来自 plan 状态。

进一步从 Toronto `q=6` 的 exact three-block witness 机械恢复树：

```text
exact witness unique-edge cost  0.9616227800
ordinary regrouped price        1.0658278500
three-arm anchored price        1.1662525000
```

DBLP 同一诊断三者均为 `12.1663030000`。Toronto 是上述 caterpillar 边界在真实 GST witness 上的数值体现：只按 token 数连续提取 pendant 子树，不足以让恢复出的 blocks 同时成为独立 macro leaves。

## 5. 状态规模审计

错误地把小 core 拆成 singleton labels 后，own-upper 下需要的唯一 fixed-pair rows 已达到：

| fast graph | pair rows | dense double payload at n=3500 |
| --- | ---: | ---: |
| Toronto | `29,994` | about `0.78 GiB` |
| DBLP | `46,902` | about `1.22 GiB` |

改成“三 blocks + 一个 core atom”后，候选可以整理成有序的 `(half mask, two raw atoms)` targets：Toronto/DBLP 分别有 `14,925/18,290` 个去重 targets、`1,532/1,652` 个 half masks，每个 mask 最多 35 个 consumers。但 Toronto 全量精确定价仍有 gap，所以这些较小数字不能作为 stopping certificate，也不能接入 Test21。

所有 key 都通过排序、去重和二分处理；实验没有引入 Hash、数据集判断、固定运行时刻或密度阈值。

## 6. 保留与撤回

保留：

- 3--5 macro-label cherry 恒等式；
- 四标签 single-closure 形式；
- 三臂 block-anchor fixed-plan 恒等式；
- 三度 caterpillar 对 simultaneous three-arm family 的组合反例；
- Toronto 对 token-only 顺序提取规则的数值反例；
- 第二接口或 paid-backbone geometry 是必要信息的边界。

撤回：

- 把三个顺序 pendant blocks 同时视为三个独立 leaves；
- 把剩余 core 压成一个 one-interface rooted row；
- 为该不完备 family 物化数万 fixed-pair rows；
- 据此删除正式 Test21 的 D6/A5 或触发 full DBLP。

## 7. 下一状态

若继续顺序 pendant 路线，下一候选应显式表达一个 block 在“自身 attachment”与“已付费主干上的上游 attachment”之间的关系。可接受的实现必须把第二接口隐式共享在有序 predecessor/path 结构中；显式 `n^2` endpoint table、每个 consumer 展开事件或 Hash frontier 均已被此前实验否决。

## 8. 论文关系

本轮没有引入新的论文算法或引用。Rooted subset DP 与 metric closure 的背景仍属于 Dreyfus--Wagner 系列；cherry 与三臂 block-anchor 恒等式是本仓库内推导，目前只作为研究候选，不宣称已经达到论文级原创性或完整复杂度结论。
