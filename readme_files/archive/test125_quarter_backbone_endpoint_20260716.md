# Test125：Quarter-Backbone Endpoint 完备性反例

更新时间：2026-07-16。Test125 尝试把高层 ordinary `D` 改为 endpoint `P`，并把 anchored `A` 的普通分支限制到更小的精确 D blocks。Toronto `g13 q3` 给出确定性错误答案，候选源码、编译目标与结果目录均已撤回。本文只保存理论边界和已撤回 Release/O2 二进制的证据，不作为当前 solver 结果。

## 1. 候选动机

令 `h=floor(g/2)`、`q=ceil(h/2)`。任意至多含 `h` 个标号叶的 rooted tree，从根开始至多有一个 child component 含超过 `q` 个标号；沿该 heavy child 前进会得到一条 backbone，其离路径组件均不超过 `q`。候选据此定义：

```text
D(S,v)  精确 closed rooted tree，|S|<=q；
P(S,v)  ordinary backbone endpoint，q<|S|<=h；
A(S,v)  anchor backbone endpoint，|S|<=h-1。
```

与早期 Test71 的固定 pivot 版本不同，Test125 对高 P 枚举任意最后一个小 block：

```text
P(S) = Close(min P(S-B) + D(B)), 1<=|B|<=q。
```

A 同样只接 `|B|<=q` 的 D，最终用 `A+P+P` 完成。高 P 不发布 closed-D branch。该规则没有数据集、固定 `g`、层号、密度、wall time 或经验参数分支。

## 2. 为什么一度看似成立

Test96 的 fixed-anchor bounded-D4/full-A 反例为 `exact=71`、候选 `73`。独立 dense 复算将同一实例改为 unrestricted P/A endpoint recurrence 后得到 `71`，修复了旧反例。随后 Test80 编译分支通过宽范围 seed `717301` 的 `100/100` 和固定 `g=15` seed `717302` 的 `30/30` 黑盒 DPBF 对拍。

这些证据只说明旧反例和随机样本未覆盖新的缺口，不能替代完备性证明。

## 3. Toronto 跨询问反例

固定 q1--q5 panel 在同一进程内运行。q1、q2、q4、q5 与当前 Test121 权重一致，但 q3 出现：

```text
Test121 / exact   0.7985684033
Test125           0.8062097156
```

为排除 root-irreducible branch-only 接口过窄，又让高 P 和 A 读取低 D 的全部有序 rooted values；q3 仍为 `0.8062097156`，因此错误来自状态 family，不是 branch bit 或查找实现。

## 4. 理论缺口

ordinary centroid side 的 heavy backbone 可以自由选择方向，所以 unrestricted P 能重现任意 rooted ordinary tree；这也意味着它趋向于恢复完整 D，而不是形成真正更小的状态族。anchor side 则不同：backbone 必须连接 anchor 与最终 junction。该固定路径旁可以悬挂一个大于 `q` 的 ordinary component。让 A 只接小 D 会漏掉这类树；允许 A 接高 P/D 虽可恢复完备，却重新引入原高层接口，无法消维。

因此以下推论无效：

```text
每个 centroid side 存在 q-heavy path
=> 同一 q 足以限制 fixed-anchor A 的所有 ordinary consumers。
```

要突破该边界，状态必须表达 anchor path 上至少一个高 attachment 的第二接口或 paid geometry；只取消 pivot、改读全部低 D values、改变 block 顺序均不够。

## 5. 撤回结论

Test125 没有进入 DBLP，也没有触发 D4、D5 或完整 q33 长跑。该方向与 Test95/96 的 paid-attachment 障碍一致：**真正缺失的是 anchor 主干内部的可复用连接位置，而不是 endpoint 枚举顺序。** 后续不再实现单 endpoint quarter 变体；新候选必须先给出次二次的第二接口表示或可证明的 exchange theorem。

本轮没有新增论文算法或引用。heavy-path 论证是树上的直接观察；相关 separator 文献边界仍见 Test39，本文不宣称原创性。
