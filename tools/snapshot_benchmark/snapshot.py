#!/usr/bin/env python3
"""Run reproducible cross-dataset GST snapshot benchmarks.

Default command:
  python tools/snapshot_benchmark/snapshot.py --method Test16 --suite fast --build
"""

from __future__ import annotations

import argparse
import csv
import datetime as _dt
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PLAN = ROOT / "data_snapshot" / "snapshot_plan.json"


def method_exe_name(method: str) -> str:
    table = {
        "DPBF": "gst_dpbf_main",
        "Half_DPBF": "gst_half_dpbf_main",
        "PrunedDP": "gst_pruned_dp_main",
    }
    if method in table:
        base = table[method]
    elif method in {"Test16", "Test17", "Test18"}:
        base = f"gst_{method.lower()}_main"
    else:
        raise SystemExit(f"Unknown method name: {method}")
    return base + (".exe" if os.name == "nt" else "")


def method_stats_name(method: str) -> str:
    table = {
        "DPBF": "dpbf_stats.txt",
        "Half_DPBF": "half_dpbf_stats.txt",
        "PrunedDP": "pruneddp_stats.txt",
    }
    if method in table:
        return table[method]
    if method in {"Test16", "Test17", "Test18"}:
        return f"{method.lower()}_stats.txt"
    raise SystemExit(f"Unknown method name: {method}")


def exe_path(name: str) -> Path:
    candidates = [
        ROOT / "build" / "Release" / name,
        ROOT / "build" / name,
        ROOT / "build" / "tools" / "snapshot_benchmark" / "Release" / name,
        ROOT / "build" / "tools" / "snapshot_benchmark" / name,
    ]
    for p in candidates:
        if p.exists():
            return p
    return candidates[0]


def run(cmd: list[str], *, timeout: int | None = None, quiet: bool = False) -> subprocess.CompletedProcess[str]:
    if not quiet:
        print(">", " ".join(cmd), flush=True)
    return subprocess.run(
        cmd,
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE if quiet else None,
        stderr=subprocess.STDOUT if quiet else None,
        timeout=timeout,
        check=False,
    )


def build_targets(method: str) -> None:
    target = method_exe_name(method).removesuffix(".exe")
    code = run(["cmake", "-S", ".", "-B", "build"]).returncode
    if code != 0:
        raise SystemExit(f"configure failed: exit={code}")
    cmd = ["cmake", "--build", "build", "--config", "Release", "--target", target, "gst_snapshot_prepare"]
    code = run(cmd).returncode
    if code != 0:
        raise SystemExit(f"build failed: exit={code}")


def load_plan(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def suite_generated_root(plan: dict, suite_name: str) -> Path:
    suite = plan["suites"][suite_name]
    return ROOT / suite.get("generated_root", plan["generated_root"])


def suite_groups(plan: dict, suite_name: str) -> list[int]:
    return list(plan["suites"][suite_name].get("groups", plan["groups"]))


def suite_dataset(plan: dict, suite_name: str, ds: dict) -> dict:
    cur = dict(ds)
    suite = plan["suites"][suite_name]
    cur.update(suite.get("prepare_overrides", {}))
    cur.update(suite.get("dataset_overrides", {}).get(ds["name"], {}))
    return cur


def ensure_snapshot_data(plan: dict, suite_name: str, *, force: bool) -> Path:
    generated_root = suite_generated_root(plan, suite_name)
    generated_root.mkdir(parents=True, exist_ok=True)
    prep = exe_path("gst_snapshot_prepare.exe" if os.name == "nt" else "gst_snapshot_prepare")
    if not prep.exists():
        raise SystemExit(f"snapshot prepare executable not found: {prep}")

    groups = suite_groups(plan, suite_name)
    g_csv = ",".join(str(x) for x in groups)
    for base_ds in plan["datasets"]:
        ds = suite_dataset(plan, suite_name, base_ds)
        dest = generated_root / ds["name"]
        expected = [dest / "graph.txt"] + [dest / f"query_g{g}.txt" for g in groups]
        if not force and all(p.exists() for p in expected):
            print(f"[prepare] reuse {ds['name']}")
            continue
        if dest.exists():
            shutil.rmtree(dest)
        src = ROOT / ds["source_root"] / ds["graph"]
        max_vertices = str(ds.get("max_vertices", 0))
        cmd = [
            str(prep),
            str(src),
            str(dest),
            ds["mode"],
            max_vertices,
            str(ds.get("seed", 1)),
            str(ds.get("queries_per_g", 40)),
            g_csv,
            ds["query_pattern"],
        ]
        code = run(cmd).returncode
        if code != 0:
            raise SystemExit(f"prepare failed for {ds['name']}: exit={code}")
    return generated_root


def read_weights(path: Path) -> tuple[int, float, float | None]:
    count = 0
    total_time = 0.0
    last_weight: float | None = None
    if not path.exists():
        return 0, 0.0, None
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                count += 1
                total_time += float(parts[0])
                last_weight = float(parts[1])
    return count, total_time, last_weight


def read_peak_mb(stats_file: Path) -> float | None:
    if not stats_file.exists():
        return None
    peak = None
    with stats_file.open("r", encoding="utf-8") as f:
        for line in f:
            for token in line.split():
                if token.startswith("peak_rss_mb="):
                    try:
                        peak = float(token.split("=", 1)[1])
                    except ValueError:
                        pass
    return peak


def run_suite(plan: dict, method: str, suite_name: str, generated_root: Path) -> Path:
    suite = plan["suites"][suite_name]
    groups = suite_groups(plan, suite_name)
    limit = int(suite["query_limit_per_g"])
    timeout = int(suite["timeout_seconds_per_run"])
    ts = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    out_root = ROOT / plan["result_root"] / suite_name / ts
    out_root.mkdir(parents=True, exist_ok=True)

    exe = exe_path(method_exe_name(method))
    if not exe.exists():
        raise SystemExit(f"method executable not found: {exe}")

    wall_begin = time.perf_counter()
    stats_name = method_stats_name(method)
    rows: list[dict[str, str]] = []
    for ds in plan["datasets"]:
        for g in groups:
            query = f"g{g}"
            cmd = [
                str(exe),
                ds["name"],
                str(out_root),
                query,
                str(generated_root),
                "1",
                str(limit),
            ]
            try:
                proc = run(cmd, timeout=timeout, quiet=False)
                exit_code = proc.returncode
                timed_out = False
            except subprocess.TimeoutExpired:
                exit_code = -1
                timed_out = True

            result_dir = out_root / ds["name"] / method / f"query_g{g}"
            weights = result_dir / "weights.txt"
            stats = result_dir / stats_name
            count, total_time, last_weight = read_weights(weights)
            peak = read_peak_mb(stats)
            rows.append(
                {
                    "dataset": ds["name"],
                    "g": str(g),
                    "limit": str(limit),
                    "exit": str(exit_code),
                    "timeout": "1" if timed_out else "0",
                    "queries_done": str(count),
                    "time_sec": f"{total_time:.6f}",
                    "last_weight": "" if last_weight is None else f"{last_weight:.10f}",
                    "peak_rss_mb": "" if peak is None else f"{peak:.3f}",
                }
            )

    summary = out_root / "snapshot_summary.csv"
    with summary.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else [])
        writer.writeheader()
        writer.writerows(rows)

    total = sum(float(r["time_sec"]) for r in rows)
    wall = time.perf_counter() - wall_begin
    print(f"[summary] {summary}")
    print(f"[summary] total_query_time_sec={total:.3f}")
    print(f"[summary] wall_seconds={wall:.3f} target={suite.get('target_wall_seconds', '')}")
    return out_root


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--method", default="Test16")
    parser.add_argument("--suite", choices=["fast", "normal", "large"], default="fast")
    parser.add_argument("--plan", type=Path, default=DEFAULT_PLAN)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--no-prepare", action="store_true")
    parser.add_argument("--force-prepare", action="store_true")
    args = parser.parse_args()

    if args.build:
        build_targets(args.method)

    plan = load_plan(args.plan)
    generated_root = suite_generated_root(plan, args.suite)
    if not args.no_prepare:
        generated_root = ensure_snapshot_data(plan, args.suite, force=args.force_prepare)

    if args.prepare_only:
        print(f"[prepare] generated_root={generated_root}")
        return 0

    run_suite(plan, args.method, args.suite, generated_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
