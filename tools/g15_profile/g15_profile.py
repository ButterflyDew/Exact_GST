#!/usr/bin/env python3
"""Profile the 40 original g=15 queries without reloading a graph per query."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import math
import os
import statistics
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATA_ROOT = ROOT / "data"
PROFILE_ROOT = ROOT / "result_snapshot" / "g15_profile"
DATASETS = ("Toronto", "DBLP", "DBpedia", "LinkedMDB", "MovieLens")

METHODS = {
    "Test121": ("gst_test80_anchor_tree_main", "test80_stats.txt"),
    "Test142": ("gst_test142_adjoint_anchor_main", "test80_stats.txt"),
    "Test144": ("gst_test144_adjoint_eager_boundary_main", "test80_stats.txt"),
    "Test145": ("gst_test145_transposed_terminal_main", "test80_stats.txt"),
    "Test145D2": ("gst_test145_d2_profile_main", "test80_stats.txt"),
    "Test145D3": ("gst_test145_d3_profile_main", "test80_stats.txt"),
    "Test145NoTree": ("gst_test145_no_tree_main", "test80_stats.txt"),
    "Test145NoTreeD3": ("gst_test145_no_tree_d3_profile_main", "test80_stats.txt"),
    "Test146": ("gst_test146_amortized_anchor_tree_main", "test80_stats.txt"),
    "Test146D2": ("gst_test146_amortized_anchor_tree_d2_main", "test80_stats.txt"),
    "Test146D3": ("gst_test146_amortized_anchor_tree_d3_main", "test80_stats.txt"),
    "Test145NoDualD2": ("gst_test145_no_dual_d2_profile_main", "test80_stats.txt"),
    "Test149": ("gst_test149_changed_arc_dual_main", "test80_stats.txt"),
    "Test149D2": ("gst_test149_changed_arc_dual_d2_main", "test80_stats.txt"),
    "Test149D3": ("gst_test149_changed_arc_dual_d3_main", "test80_stats.txt"),
    "ReleaseV4": ("gst_release_v4_main", "releasev4_stats.txt"),
    "ReleaseV5": ("gst_release_v5_main", "releasev5_stats.txt"),
    "PrunedDP": ("gst_pruned_dp_main", "pruneddp_stats.txt"),
}

PREFIX_METHODS = {
    "Test145D2",
    "Test145D3",
    "Test145NoTreeD3",
    "Test146D2",
    "Test146D3",
    "Test145NoDualD2",
    "Test149D2",
    "Test149D3",
}


def executable(name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    candidates = (ROOT / "build" / "Release" / f"{name}{suffix}",
                  ROOT / "build" / f"{name}{suffix}")
    for path in candidates:
        if path.exists():
            return path
    return candidates[0]


def parse_datasets(value: str) -> list[str]:
    names = DATASETS if value == "all" else tuple(x.strip() for x in value.split(","))
    unknown = [name for name in names if name not in DATASETS]
    if unknown:
        raise SystemExit(f"Unknown datasets: {', '.join(unknown)}")
    return list(names)


def read_queries(path: Path) -> list[list[list[int]]]:
    tokens = (int(token) for token in path.read_text(encoding="utf-8").split())
    count = next(tokens)
    queries: list[list[list[int]]] = []
    for _ in range(count):
        group_count = next(tokens)
        groups: list[list[int]] = []
        for _ in range(group_count):
            size = next(tokens)
            groups.append([next(tokens) for _ in range(size)])
        queries.append(groups)
    return queries


def write_queries(path: Path, queries: list[list[list[int]]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        handle.write(f"{len(queries)}\n")
        for groups in queries:
            handle.write(f"{len(groups)}\n")
            for group in groups:
                handle.write(f"{len(group)} {' '.join(str(vertex) for vertex in group)}\n")


def query_metadata(dataset: str, query_id: int, groups: list[list[int]]) -> dict[str, object]:
    sizes = [len(group) for group in groups]
    memberships: dict[int, int] = {}
    for group in groups:
        for vertex in group:
            memberships[vertex] = memberships.get(vertex, 0) + 1
    shared = [count for count in memberships.values() if count > 1]
    return {
        "dataset": dataset,
        "query": query_id,
        "g": len(groups),
        "group_vertices": sum(sizes),
        "distinct_group_vertices": len(memberships),
        "duplicate_memberships": sum(sizes) - len(memberships),
        "shared_vertices": len(shared),
        "max_memberships_per_vertex": max(memberships.values(), default=0),
        "min_group_size": min(sizes),
        "median_group_size": statistics.median(sizes),
        "mean_group_size": statistics.mean(sizes),
        "max_group_size": max(sizes),
    }


def write_csv(path: Path, rows: list[dict[str, object]], fields: list[str] | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if fields is None:
        fields = []
        seen: set[str] = set()
        for row in rows:
            for key in row:
                if key not in seen:
                    seen.add(key)
                    fields.append(key)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def build_metadata() -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for dataset in DATASETS:
        query_path = DATA_ROOT / dataset / "query_g15.txt"
        for query_id, groups in enumerate(read_queries(query_path), 1):
            rows.append(query_metadata(dataset, query_id, groups))
    write_csv(PROFILE_ROOT / "query_metadata_g15.csv", rows)
    return rows


def load_metadata() -> dict[tuple[str, int], dict[str, str]]:
    path = PROFILE_ROOT / "query_metadata_g15.csv"
    if not path.exists():
        build_metadata()
    with path.open(newline="", encoding="utf-8") as handle:
        return {(row["dataset"], int(row["query"])): row for row in csv.DictReader(handle)}


def parse_value(value: str) -> object:
    try:
        if value.lower() in {"inf", "+inf", "-inf", "nan"}:
            return float(value)
        if any(char in value for char in ".eE"):
            return float(value)
        return int(value)
    except ValueError:
        return value


def last_run_stats(path: Path) -> list[dict[str, object]]:
    if not path.exists():
        return []
    rows: list[dict[str, object]] = []
    with path.open(encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#"):
                rows = []
                continue
            row: dict[str, object] = {}
            for token in line.split():
                if "=" not in token:
                    continue
                key, value = token.split("=", 1)
                row[key] = parse_value(value)
            if "query" in row:
                rows.append(row)
    return rows


def build_targets(method: str) -> None:
    target = METHODS[method][0]
    commands = (["cmake", "-S", ".", "-B", "build"],
                ["cmake", "--build", "build", "--config", "Release", "--target", target])
    for command in commands:
        print(">", " ".join(command), flush=True)
        result = subprocess.run(command, cwd=ROOT, check=False)
        if result.returncode:
            raise SystemExit(result.returncode)


def run_profile(args: argparse.Namespace) -> Path:
    datasets = parse_datasets(args.datasets)
    if args.build:
        build_targets(args.method)
    exe_name, stats_name = METHODS[args.method]
    exe = executable(exe_name)
    if not exe.exists():
        raise SystemExit(f"Executable not found: {exe}")

    tag = args.tag or dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    run_root = PROFILE_ROOT / "runs" / tag / args.method
    run_root.mkdir(parents=True, exist_ok=True)
    metadata = load_metadata()
    combined: list[dict[str, object]] = []
    run_status: list[dict[str, object]] = []

    panel_ids: dict[str, list[int]] = {}
    if args.panel:
        with args.panel.open(newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                panel_ids.setdefault(row["dataset"], []).append(int(row["query"]))

    for dataset in datasets:
        selected_ids = panel_ids.get(dataset, []) if args.panel else []
        if args.panel:
            original = read_queries(DATA_ROOT / dataset / "query_g15.txt")
            panel_path = run_root / "input" / dataset / "query_g15_panel.txt"
            write_queries(panel_path, [original[query_id - 1] for query_id in selected_ids])
            query_selector = str(panel_path.resolve())
            query_begin = 1
            query_limit = len(selected_ids)
            run_subdir = panel_path.stem
        else:
            query_selector = "g15"
            query_begin = args.begin
            query_limit = args.limit
            run_subdir = "query_g15"
        command = [str(exe), dataset, str(run_root), query_selector, str(DATA_ROOT),
                   str(query_begin), str(query_limit)]
        print(">", " ".join(command), flush=True)
        timed_out = False
        try:
            result = subprocess.run(command, cwd=ROOT, timeout=args.timeout, check=False)
            exit_code = result.returncode
        except subprocess.TimeoutExpired:
            timed_out = True
            exit_code = -1

        result_dir = run_root / dataset / ("Test80" if args.method.startswith("Test") else args.method) / run_subdir
        stats_path = result_dir / stats_name
        stats_rows = last_run_stats(stats_path)
        for stats in stats_rows:
            local_query_id = int(stats["query"])
            query_id = selected_ids[local_query_id - 1] if args.panel else local_query_id
            row: dict[str, object] = {
                "dataset": dataset,
                "method": args.method,
                "query": query_id,
                "panel_query": local_query_id if args.panel else "",
                "complete": int(args.method not in PREFIX_METHODS),
            }
            row.update(metadata.get((dataset, query_id), {}))
            row.update(stats)
            row["dataset"] = dataset
            row["method"] = args.method
            row["query"] = query_id
            row["panel_query"] = local_query_id if args.panel else ""
            combined.append(row)
        run_status.append({
            "dataset": dataset,
            "method": args.method,
            "begin": query_begin,
            "limit": query_limit,
            "original_query_ids": ",".join(str(query_id) for query_id in selected_ids),
            "queries_recorded": len(stats_rows),
            "exit": exit_code,
            "timeout": int(timed_out),
            "stats_file": str(stats_path.relative_to(ROOT)),
        })
        write_csv(run_root / "per_query.csv", combined)
        write_csv(run_root / "run_status.csv", run_status)

    summarize_rows(combined, run_root)
    print(f"[profile] {run_root}")
    return run_root


def numeric(row: dict[str, object], key: str) -> float | None:
    value = row.get(key)
    return float(value) if isinstance(value, (int, float)) and math.isfinite(float(value)) else None


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = fraction * (len(ordered) - 1)
    low = math.floor(position)
    high = math.ceil(position)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def pearson(rows: list[dict[str, object]], left: str, right: str) -> float | None:
    pairs = [(numeric(row, left), numeric(row, right)) for row in rows]
    values = [(x, y) for x, y in pairs if x is not None and y is not None]
    if len(values) < 2:
        return None
    xs, ys = zip(*values)
    if statistics.pstdev(xs) == 0 or statistics.pstdev(ys) == 0:
        return None
    return statistics.correlation(xs, ys)


def summarize_rows(rows: list[dict[str, object]], out_root: Path) -> None:
    summaries: list[dict[str, object]] = []
    extrema: list[dict[str, object]] = []
    for dataset in DATASETS:
        current = [row for row in rows if row.get("dataset") == dataset]
        if not current:
            continue
        wall = [value for row in current if (value := numeric(row, "wall_ms")) is not None]
        peak = [value for row in current if (value := numeric(row, "peak_rss_mb")) is not None]
        if wall:
            mean = statistics.mean(wall)
            sample_std = statistics.stdev(wall) if len(wall) > 1 else 0.0
            summaries.append({
                "dataset": dataset,
                "queries": len(current),
                "wall_total_sec": sum(wall) / 1000.0,
                "wall_mean_sec": mean / 1000.0,
                "wall_median_sec": statistics.median(wall) / 1000.0,
                "wall_p90_sec": percentile(wall, 0.9) / 1000.0,
                "wall_sample_std_sec": sample_std / 1000.0,
                "wall_cv": sample_std / mean if mean else 0.0,
                "wall_min_sec": min(wall) / 1000.0,
                "wall_max_sec": max(wall) / 1000.0,
                "peak_max_mb": max(peak) if peak else "",
                "group_dist_share": sum(numeric(row, "group_dist_ms") or 0 for row in current) / sum(wall),
                "dual_share": sum(numeric(row, "dual_ms") or 0 for row in current) / sum(wall),
                "junction_share": sum(numeric(row, "junction_ms") or 0 for row in current) / sum(wall),
                "packing_share": sum(numeric(row, "packing_ms") or 0 for row in current) / sum(wall),
                "ordinary_share": sum(numeric(row, "ordinary_ms") or 0 for row in current) / sum(wall),
                "anchored_share": sum(numeric(row, "anchored_ms") or 0 for row in current) / sum(wall),
                "adjoint_share": sum(numeric(row, "adjoint_ms") or 0 for row in current) / sum(wall),
                "anchor_tree_sec": sum(
                    sum(numeric(row, f"anchor_tree_ms_s{size}") or 0 for size in range(1, 8))
                    for row in current
                ) / 1000.0,
                "corr_group_vertices_wall": pearson(current, "group_vertices", "wall_ms"),
                "corr_d2_values_wall": pearson(current, "d_values_s2", "wall_ms"),
                "corr_d3_values_wall": pearson(current, "d_values_s3", "wall_ms"),
                "corr_ordinary_values_wall": pearson(current, "ordinary_values", "wall_ms"),
            })
        for metric in ("wall_ms", "peak_rss_mb", "d_values_s2", "d_values_s3", "ordinary_values"):
            ranked = sorted((row for row in current if numeric(row, metric) is not None),
                            key=lambda row: numeric(row, metric) or -math.inf, reverse=True)
            if not ranked or (numeric(ranked[0], metric) or 0) == 0:
                continue
            for rank, row in enumerate(ranked[:5], 1):
                extrema.append({
                    "dataset": dataset,
                    "metric": metric,
                    "rank": rank,
                    "query": row["query"],
                    "value": row[metric],
                    "group_vertices": row.get("group_vertices", ""),
                })
    write_csv(out_root / "summary.csv", summaries)
    write_csv(out_root / "extrema_top5.csv", extrema)
    write_frozen_panel(rows, out_root)


def write_frozen_panel(rows: list[dict[str, object]], out_root: Path) -> None:
    """Select distribution landmarks without a data-dependent threshold."""
    panel: list[dict[str, object]] = []
    for dataset in DATASETS:
        current = [row for row in rows if row.get("dataset") == dataset and
                   numeric(row, "d_values_s2") is not None]
        current.sort(key=lambda row: (numeric(row, "d_values_s2") or 0, int(row["query"])))
        if not current:
            continue
        positions = {
            "minimum": 0,
            "lower_quartile": round((len(current) - 1) * 0.25),
            "median": round((len(current) - 1) * 0.50),
            "upper_quartile": round((len(current) - 1) * 0.75),
            "maximum": len(current) - 1,
        }
        for role, position in positions.items():
            row = current[position]
            panel.append({
                "dataset": dataset,
                "role": role,
                "query": row["query"],
                "d2_rank_ascending": position + 1,
                "d_values_s2": row["d_values_s2"],
                "wall_ms": row.get("wall_ms", ""),
                "peak_rss_mb": row.get("peak_rss_mb", ""),
                "group_vertices": row.get("group_vertices", ""),
            })
    write_csv(out_root / "frozen_d3_panel.csv", panel)


def summarize_existing(path: Path) -> None:
    with path.open(newline="", encoding="utf-8") as handle:
        rows: list[dict[str, object]] = [dict(row) for row in csv.DictReader(handle)]
    for row in rows:
        for key, value in list(row.items()):
            row[key] = parse_value(value)
    summarize_rows(rows, path.parent)


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("metadata")

    run_parser = subparsers.add_parser("run")
    run_parser.add_argument("--method", choices=METHODS, required=True)
    run_parser.add_argument("--datasets", default="all")
    run_parser.add_argument("--begin", type=int, default=1)
    run_parser.add_argument("--limit", type=int, default=40)
    run_parser.add_argument("--timeout", type=int, default=None,
                            help="Per-dataset process timeout in seconds.")
    run_parser.add_argument("--panel", type=Path,
                            help="CSV containing dataset/query rows to run in listed order.")
    run_parser.add_argument("--tag")
    run_parser.add_argument("--build", action="store_true")

    summary_parser = subparsers.add_parser("summarize")
    summary_parser.add_argument("per_query_csv", type=Path)
    args = parser.parse_args()

    if args.command == "metadata":
        rows = build_metadata()
        print(f"[metadata] {len(rows)} queries -> {PROFILE_ROOT / 'query_metadata_g15.csv'}")
    elif args.command == "run":
        run_profile(args)
    else:
        summarize_existing(args.per_query_csv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
