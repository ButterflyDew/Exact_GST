#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Random hack tester for DPBF vs Test4.

The script generates small Group Steiner Tree instances, runs two configured
executables, and stops at the first case whose output weights differ.
"""

import argparse
import random
import shutil
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class Instance:
    n: int
    edges: list[tuple[int, int, int]]
    groups: list[list[int]]


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_exe(name: str) -> Path:
    root = project_root()
    candidates = [
        root / "build" / "Release" / name,
        root / "build" / name,
        root / name,
    ]
    for path in candidates:
        if path.exists():
            return path
    return candidates[0]


def generate_connected_sparse_instance(
    rng: random.Random,
    n: int,
    extra_edges: int,
    max_weight: int,
    group_count: int,
    group_size: int,
) -> Instance:
    """Generate a connected sparse integer-weight graph and one GST query."""
    used_edges: set[tuple[int, int]] = set()
    edges: list[tuple[int, int, int]] = []

    def add_edge(u: int, v: int) -> bool:
        if u == v:
            return False
        a, b = sorted((u, v))
        if (a, b) in used_edges:
            return False
        used_edges.add((a, b))
        edges.append((a, b, rng.randint(1, max_weight)))
        return True

    # A random tree guarantees every generated query is feasible.
    vertices = list(range(1, n + 1))
    rng.shuffle(vertices)
    for i in range(1, n):
        add_edge(vertices[i], vertices[rng.randrange(i)])

    target_m = min(n * (n - 1) // 2, (n - 1) + extra_edges)
    while len(edges) < target_m:
        add_edge(rng.randint(1, n), rng.randint(1, n))

    groups = []
    actual_group_size = min(group_size, n)
    for _ in range(group_count):
        group = sorted(rng.sample(range(1, n + 1), actual_group_size))
        groups.append(group)

    return Instance(n=n, edges=edges, groups=groups)


def write_instance(instance: Instance, graph_dir: Path) -> None:
    graph_dir.mkdir(parents=True, exist_ok=True)

    graph_lines = [f"{instance.n} {len(instance.edges)}"]
    graph_lines.extend(f"{u} {v} {w}" for u, v, w in instance.edges)
    (graph_dir / "Graph.txt").write_text("\n".join(graph_lines) + "\n", encoding="utf-8")

    query_lines = ["1", str(len(instance.groups))]
    query_lines.extend(f"{len(group)} {' '.join(map(str, group))}" for group in instance.groups)
    (graph_dir / "Query.txt").write_text("\n".join(query_lines) + "\n", encoding="utf-8")


def read_weight(path: Path) -> float:
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(f"expected exactly one result row in {path}, got {len(lines)}")
    parts = lines[0].split()
    if len(parts) < 2:
        raise RuntimeError(f"bad result row in {path}: {lines[0]}")
    return float(parts[1])


def run_solver(exe: Path, graph_name: str, result_root: Path, debug_root: Path, timeout_sec: float) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(exe), graph_name, "weight", str(result_root), str(debug_root)],
        cwd=project_root(),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout_sec,
        check=False,
    )


def weights_differ(a: float, b: float, tol: float) -> bool:
    if a < 0 and b < 0:
        return False
    return abs(a - b) > tol


def copy_case(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return parsed


def non_negative_int(value: str) -> int:
    parsed = int(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be non-negative")
    return parsed


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Randomly hack DPBF vs Test4 on small GST instances.")

    # Defaults requested by prompt317. Change these knobs to target other shapes.
    parser.add_argument("--n", type=positive_int, default=12, help="vertex count, default: 12")
    parser.add_argument("--extra-edges", type=non_negative_int, default=6, help="edges beyond a random spanning tree, default: 6")
    parser.add_argument("--max-weight", type=positive_int, default=20, help="maximum integer edge weight, default: 20")
    parser.add_argument("--groups", type=positive_int, default=6, help="group count g, default: 6")
    parser.add_argument("--group-size", type=positive_int, default=2, help="vertices per group, default: 2")

    # Defaults compare DPBF and Test4, but both executable paths and method names
    # are exposed so later experiments can reuse the same harness.
    parser.add_argument("--dpbf-exe", type=Path, default=default_exe("gst_dpbf_main.exe"), help="first solver executable")
    parser.add_argument("--test4-exe", type=Path, default=default_exe("gst_test4_main.exe"), help="second solver executable")
    parser.add_argument("--method-a", default="DPBF", help="result folder name for the first solver, default: DPBF")
    parser.add_argument("--method-b", default="Test4", help="result folder name for the second solver, default: Test4")

    parser.add_argument("--graph-name", default="hack_dpbf_vs_test4", help="temporary graph folder name under data/")
    parser.add_argument("--max-iters", type=positive_int, default=10000, help="maximum random tests before giving up")
    parser.add_argument("--seed", type=int, default=None, help="random seed; omitted means current RNG seed")
    parser.add_argument("--tol", type=float, default=1e-6, help="allowed absolute weight difference")
    parser.add_argument("--timeout-sec", type=float, default=30.0, help="timeout per solver run")
    parser.add_argument("--keep-all", action="store_true", help="also save every generated case under hacktests/cases/all/")
    return parser.parse_args(list(argv))


def main(argv: Iterable[str]) -> int:
    args = parse_args(argv)
    root = project_root()
    graph_dir = root / "data" / args.graph_name
    result_root = root / "hacktests" / "work" / "result"
    debug_root = root / "hacktests" / "work" / "debug"
    cases_root = root / "hacktests" / "cases"

    if not args.dpbf_exe.exists():
        print(f"DPBF executable not found: {args.dpbf_exe}", file=sys.stderr)
        return 1
    if not args.test4_exe.exists():
        print(f"Test4 executable not found: {args.test4_exe}", file=sys.stderr)
        return 1

    rng = random.Random(args.seed)
    print(f"Start hacking: seed={args.seed}, max_iters={args.max_iters}, graph={args.graph_name}")

    for iteration in range(1, args.max_iters + 1):
        case_seed = rng.randrange(0, 2**63)
        case_rng = random.Random(case_seed)
        instance = generate_connected_sparse_instance(
            case_rng,
            n=args.n,
            extra_edges=args.extra_edges,
            max_weight=args.max_weight,
            group_count=args.groups,
            group_size=args.group_size,
        )
        write_instance(instance, graph_dir)

        if result_root.exists():
            shutil.rmtree(result_root)
        if debug_root.exists():
            shutil.rmtree(debug_root)

        first = run_solver(args.dpbf_exe, args.graph_name, result_root, debug_root, args.timeout_sec)
        if first.returncode != 0:
            print(f"First solver failed at iter={iteration}, case_seed={case_seed}")
            print(first.stdout)
            return 1

        second = run_solver(args.test4_exe, args.graph_name, result_root, debug_root, args.timeout_sec)
        if second.returncode != 0:
            print(f"Second solver failed at iter={iteration}, case_seed={case_seed}")
            print(second.stdout)
            return 1

        path_a = result_root / "weight" / args.graph_name / args.method_a / "1.txt"
        path_b = result_root / "weight" / args.graph_name / args.method_b / "1.txt"
        weight_a = read_weight(path_a)
        weight_b = read_weight(path_b)

        if args.keep_all:
            copy_case(graph_dir, cases_root / "all" / f"iter_{iteration:06d}_seed_{case_seed}")

        if iteration == 1 or iteration % 100 == 0:
            print(f"iter={iteration} case_seed={case_seed} {args.method_a}={weight_a:.10f} {args.method_b}={weight_b:.10f}")

        if weights_differ(weight_a, weight_b, args.tol):
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            saved_dir = cases_root / f"diff_{stamp}_iter_{iteration}_seed_{case_seed}"
            copy_case(graph_dir, saved_dir)
            print("Found mismatch!")
            print(f"iter={iteration}")
            print(f"case_seed={case_seed}")
            print(f"{args.method_a}={weight_a:.10f}")
            print(f"{args.method_b}={weight_b:.10f}")
            print(f"saved_case={saved_dir}")
            print(f"re-run first:  {args.dpbf_exe} {args.graph_name} weight {result_root} {debug_root}")
            print(f"re-run second: {args.test4_exe} {args.graph_name} weight {result_root} {debug_root}")
            return 2

    print(f"No mismatch found in {args.max_iters} iterations.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
