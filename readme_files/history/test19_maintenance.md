# Test19：代码维护与拆分记录（历史）

> 冻结口径：正文中的“当前”仅指 Test19 冻结时状态，不覆盖 ReleaseV3。

Test19 独立入口已经建立，迁移阶段结束。本文件只记录尚未完成的结构工作，避免与算法说明和效果报告重复。

## 1. 当前状态

已完成：

- Test18 保持历史可复现，Test19 独立构建和运行。
- Test18/Test19 统计输出共用 `AppendTest18FamilyStats`。
- exact group-tour TSP/2 已拆到 `test19_future_bounds.*`。
- 默认 order+save、回退模式和随机对拍均已验证。

仍需处理：`methods/Test/test19.cpp` 同时包含 reductions、row storage、Complete 和 per-mask search，阅读与局部验证成本偏高。

## 2. 拆分顺序

| step | target | 内容 | 风险 |
| ---: | --- | --- | --- |
| 1 | `test19_rows.*` | `Row`、`Lookup`、`TrySet`、compact | 中 |
| 2 | `test19_bounds.*` | group distances、current LB、future-bound bridge | 低 |
| 3 | `test19_complete.*` | Complete、complement cache、上界拼接 | 高 |
| 4 | `test19_reductions.*` | exact query reductions | 中 |
| 5 | `test19_search.*` | per-mask graph search 和 ordering | 高 |

主文件最终只保留 query-level orchestration。拆分优先依据依赖边界和可独立测试性，不按代码行数硬切。

## 3. 每步守门

1. 只移动代码，不同时改变算法或统计口径。
2. Release/O2 构建 `gst_test19_main` 和 `gst_random_compare`。
3. 随机小图与 DPBF 对拍，误差 `1e-6`。
4. 比较默认和 `ORDER=0,SAVE=0` 两条路径。
5. 必要时跑 Toronto q1 或 fast snapshot 单条；结构拆分不触发 full DBLP g13 q1。
6. 清理 `.tmp_random_compare*`、`result_tmp*` 和无效结果目录。

## 4. 不进入拆分的内容

- 已撤回的固定 rows、k=3/k=4 frontier、组数/层级/数据集特判。
- 只为一次探针服务的统计字段和日志。
- one-tree、separator 等降级原型代码；rooted-group 也必须先保持独立入口，不能混入纯拆分补丁。
- baseline 同样能使用、且没有本方法特有论证的普通图或 query 压缩。

Test19 的纯拆分不再与算法实验混做。pair certificate 的 Test20 生产原型已因 fast 重复解码退化撤出；在新的 row schedule 或低开销 random-access 证明出现前，不再复制 ReleaseV1。rooted-group 继续留在 archive。
