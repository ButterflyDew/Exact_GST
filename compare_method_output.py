#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from pathlib import Path


def read_result_file(path: Path):
    rows = []
    for idx, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 2:
            raise ValueError(f"{path} 第 {idx} 行格式错误：{raw}")
        weight = float(parts[1])
        rows.append(weight)
    return rows


def main():
    parser = argparse.ArgumentParser(
        description="比较同图同输出形式、同查询子目录下两种方法的 weights.txt 是否一致（允许误差）"
    )
    parser.add_argument("--graph", required=True, help="图名，例如 Toronto")
    parser.add_argument("--mode", required=True, choices=["weight", "tree", "virtual"], help="输出形式")
    parser.add_argument("--method-a", required=True, help="方法A，例如 DPBF")
    parser.add_argument("--method-b", required=True, help="方法B，例如 Half_DPBF")
    parser.add_argument("--base-dir", default="result", help="结果根目录，默认 result")
    parser.add_argument(
        "--query-run",
        default="default",
        help="查询子目录名，默认 default；query_g10.txt 对应 query_g10",
    )
    parser.add_argument("--tol", type=float, default=1e-6, help="允许误差，默认 1e-6")
    args = parser.parse_args()

    weights_name = "weights.txt"
    p_a = Path(args.base_dir) / args.mode / args.graph / args.method_a / args.query_run / weights_name
    p_b = Path(args.base_dir) / args.mode / args.graph / args.method_b / args.query_run / weights_name
    if not p_a.exists() or not p_b.exists():
        raise FileNotFoundError(f"结果文件不存在：\n- {p_a}\n- {p_b}")

    wa = read_result_file(p_a)
    wb = read_result_file(p_b)
    if len(wa) != len(wb):
        print(f"不一致：查询数量不同，A={len(wa)}，B={len(wb)}")
        return 1

    mismatch = []
    for i, (a, b) in enumerate(zip(wa, wb), 1):
        if a < 0 and b < 0:
            continue
        if abs(a - b) > args.tol:
            mismatch.append((i, a, b, abs(a - b)))

    if mismatch:
        print(f"不一致：共有 {len(mismatch)} 条查询超出误差 {args.tol}")
        print("前 20 条差异：")
        for qid, a, b, d in mismatch[:20]:
            print(f"query={qid} A={a:.10f} B={b:.10f} diff={d:.10f}")
        return 2

    print(f"一致：共 {len(wa)} 条查询，全部在误差 {args.tol} 内。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
