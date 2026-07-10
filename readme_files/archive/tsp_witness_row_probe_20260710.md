# TSP Endpoint Witness Row 探针归档

本文记录 2026-07-10 对 `need` 精确压缩的一次独立生产探针。候选利用 Test19/ReleaseV1 已有 TSP/2 下界的 argmin endpoint pair，把持久 `need` double 改为一个 byte；它不使用 forest、不重复 Dijkstra，也不包含数据集、g、层级或运行时间特判。最终空间收益真实，但热路径重建使 fast wall 明显变慢，因此不进入 Test19/ReleaseV1，不运行 full DBLP g13。

## 1. Exact Witness

对 row mask `S`、未来组 `R=U-S`：

```text
h(v,R) = 1/2 * min_{a,b in R}
         (gd[a][v] + path[R][a][b] + gd[b][v])
```

保存 row 时 `h` 已经计算。只需保存一个达到最小值的 endpoint index `w`，以后即可由 `distance`、`gd` 和 `path` 精确恢复：

```text
need(v) = distance(v)
        + 1/2 * (gd[a_w][v] + path_w + gd[b_w][v])
```

当前发行边界 `g<=20`，非 singleton row 的未来组最多 18 个，endpoint pairs 至多 `C(18,2)=153`；加一个 unreachable sentinel 仍可放入 `uint8_t`。

对含 `s` 个状态、图有 `n` 个点的 row：

```text
ReleaseV1 = min(16(n+1), 20s) bytes
witness   = min( 9(n+1), 13s) bytes
```

dense/sparse 仍按真实字节比较。lookup、join、Complete 与 compact 都使用 witness 重建原来完全相同的 `need<=best` 条件。

## 2. 独立实现

复现入口：`tools/tsp_witness_solver_probe`。它从 ReleaseV1 机械复制，只替换 row need 表示，并直接在同一进程调用 DPBF/ReleaseV1 比较。主 solver、main dispatch 和 ReleaseV1 均未修改。

正确性：

| check | result |
| --- | --- |
| DPBF random `g=2..10` | `ALL_OK seed=710711 iterations=1000` |
| DPBF fixed `g=13` | `ALL_OK seed=710713 iterations=100` |
| fast suite | 20/20 与 ReleaseV1 权重一致 |

另有 `MetricTspLowerBound` API 级随机 `5000` 实例检查；非连通图上的 unreachable endpoint 使用独立 sentinel 后，重建错误为 0。该 API 扩展在候选失败后已从 Test19 shared code 撤回，只保留在独立工具中。

## 3. Fast 时间

同进程依次运行 ReleaseV1 和 witness solver；表中为各 solver 内部 `total_ms`，不含图加载。

| dataset | ReleaseV1 | witness | ratio |
| --- | ---: | ---: | ---: |
| Toronto | `6.498s` | `8.988s` | `1.38x` |
| Toronto-new | `10.308s` | `17.001s` | `1.65x` |
| DBLP | `2.725s` | `3.472s` | `1.27x` |
| DBLP-new | `1.952s` | `2.483s` | `1.27x` |
| MovieLens | `10.780s` | `10.774s` | `1.00x` |
| **total** | **`32.263s`** | **`42.718s`** | **`1.324x`** |

全部 20 条权重一致。witness 没有 forest 的重复线性解码；退化来自每次 row alive check 原本读取一个 `need` double，现在要读取两个 `gd`、一个 path 并做加法。

## 4. g12 空间–时间交换

| dataset | row bytes old -> witness | compression | time ratio |
| --- | ---: | ---: | ---: |
| Toronto | `49,765,440 -> 32,209,406` | `1.55x` | `1.30x` |
| Toronto-new | `96,354,384 -> 57,073,385` | `1.69x` | `1.65x` |
| DBLP | `24,234,104 -> 14,623,940` | `1.66x` | `1.34x` |
| DBLP-new | `26,669,692 -> 16,907,074` | `1.58x` | `1.30x` |
| MovieLens | `5,964,640 -> 3,877,016` | `1.54x` | `1.15x` |

空间收益跨数据存在且无参数，但时间代价使 ReleaseV1 相对 PrunedDP 的一个数量级总时间目标失效。没有理由为它启动 full g13。

## 5. 结论

1. TSP argmin witness 是 exact 的 method-specific row certificate，可作为后续静态/冷数据设计的信息。
2. 逐 lookup 重建 `need` 位于最热循环，当前形式不可用。
3. 不能按数据、`g==13` 或状态密度只在大查询启用；该做法违反 `agent.md` 第六条。
4. 只有能让 witness 重建按 row/block 摊销，而不是按 join lookup 重复发生，才值得重启该方向。

