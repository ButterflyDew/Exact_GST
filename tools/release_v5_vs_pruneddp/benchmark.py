#!/usr/bin/env python3
"""Paired ReleaseV5 versus PrunedDP++ benchmark runner.

The runner extracts an exact query prefix into a shared panel, hard-links the
unmodified graph, and writes paired time, answer, and query-peak RSS summaries.
Batch mode loads each graph once. Instance mode isolates every query so a
timeout is attached to one instance rather than to the remaining batch.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DATASETS = ["GPU4GST_Musae", "GPU4GST_Twitch", "GPU4GST_Github"]
DEFAULT_GROUPS = list(range(4, 11))
DEFAULT_METHODS = ["ReleaseV5", "PrunedDPStrict", "PrunedDPReference"]

METHODS = {
    "ReleaseV5": {
        "exe": "gst_release_v5_main",
        "result_method": "ReleaseV5",
        "stats": "releasev5_stats.txt",
        "args": [],
    },
    "PrunedDPStrict": {
        "exe": "gst_pruned_dp_main",
        "result_method": "PrunedDP",
        "stats": "pruneddp_stats.txt",
        "args": [
            "--state-storage=hash",
            "--mst-upper=on",
            "--lb2-pathmax=on",
        ],
    },
    "PrunedDPReference": {
        "exe": "gst_pruned_dp_main",
        "result_method": "PrunedDP",
        "stats": "pruneddp_stats.txt",
        "args": [
            "--state-storage=hash",
            "--mst-upper=on",
            "--lb2-pathmax=off",
        ],
    },
    "DPBF": {
        "exe": "gst_dpbf_main",
        "result_method": "DPBF",
        "stats": "dpbf_stats.txt",
        "args": [],
    },
}


def parse_csv_strings(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def parse_groups(value: str) -> list[int]:
    groups: list[int] = []
    for item in parse_csv_strings(value):
        if "-" in item:
            begin_text, end_text = item.split("-", 1)
            begin = int(begin_text)
            end = int(end_text)
            groups.extend(range(begin, end + 1))
        else:
            groups.append(int(item))
    groups = sorted(set(groups))
    if not groups or groups[0] < 2:
        raise argparse.ArgumentTypeError("groups must contain integers >= 2")
    return groups


def executable(name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    candidates = [
        ROOT / "build" / "Release" / f"{name}{suffix}",
        ROOT / "build" / f"{name}{suffix}",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def extract_query_prefix(source: Path, destination: Path, count: int) -> int:
    with source.open("r", encoding="utf-8") as input_file:
        header = input_file.readline()
        if not header:
            raise RuntimeError(f"empty query file: {source}")
        available = int(header.strip())
        selected = min(count, available)
        rows: list[str] = []
        for query_index in range(selected):
            group_line = input_file.readline()
            if not group_line:
                raise RuntimeError(f"truncated query {query_index + 1}: {source}")
            group_count = int(group_line.strip())
            rows.append(group_line)
            for _ in range(group_count):
                member_line = input_file.readline()
                if not member_line:
                    raise RuntimeError(f"truncated group in query {query_index + 1}: {source}")
                fields = member_line.split()
                if not fields or int(fields[0]) != len(fields) - 1:
                    raise RuntimeError(f"invalid line-oriented group in {source}")
                rows.append(member_line)

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output_file:
        output_file.write(f"{selected}\n")
        output_file.writelines(rows)
    temporary.replace(destination)
    return selected


def prepare_panel(
    run_root: Path,
    datasets: list[str],
    groups: list[int],
    query_count: int,
    source_data_root: Path,
) -> Path:
    panel_root = run_root / "input"
    for dataset in datasets:
        source_dir = source_data_root / dataset
        if not source_dir.is_dir():
            raise RuntimeError(f"dataset directory not found: {source_dir}")
        destination_dir = panel_root / dataset
        destination_dir.mkdir(parents=True, exist_ok=True)
        source_graph = source_dir / "graph.txt"
        destination_graph = destination_dir / "graph.txt"
        if not destination_graph.exists():
            os.link(source_graph, destination_graph)
        for group_count in groups:
            source_query = source_dir / f"query_g{group_count}.txt"
            destination_query = destination_dir / f"query_g{group_count}.txt"
            if not destination_query.exists():
                selected = extract_query_prefix(source_query, destination_query, query_count)
                if selected != query_count:
                    raise RuntimeError(
                        f"requested {query_count} queries but {source_query} has {selected}"
                    )
    return panel_root


def read_latest_weights(path: Path) -> list[dict[str, float]]:
    rows: list[dict[str, float]] = []
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8") as input_file:
        for raw_line in input_file:
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("#"):
                rows = []
                continue
            fields = line.split()
            if len(fields) >= 2:
                rows.append(
                    {
                        "time_s": float(fields[0]),
                        "weight": float(fields[1]),
                        "peak_rss_mb": float(fields[2]) if len(fields) >= 3 else math.nan,
                    }
                )
    return rows


def parse_key_values(line: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for token in line.split():
        if "=" in token:
            key, value = token.split("=", 1)
            values[key] = value
    return values


def read_latest_stats(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8") as input_file:
        for raw_line in input_file:
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("#"):
                rows = []
                continue
            rows.append(parse_key_values(line))
    return rows


def load_status(path: Path) -> dict[tuple[str, str, int, str], dict[str, str]]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8", newline="") as input_file:
        return {
            (
                row["config"],
                row["dataset"],
                int(row["g"]),
                row.get("query", ""),
            ): row
            for row in csv.DictReader(input_file)
        }


STATUS_FIELDS = [
    "config",
    "dataset",
    "g",
    "query",
    "status",
    "exit_code",
    "wall_s",
    "completed_queries",
    "requested_queries",
    "log",
]


def append_status(path: Path, row: dict[str, object]) -> None:
    exists = path.exists()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8", newline="") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=STATUS_FIELDS)
        if not exists:
            writer.writeheader()
        writer.writerow(row)


def result_paths_from_root(
    result_root: Path, config: str, dataset: str, group_count: int
) -> tuple[Path, Path]:
    method = METHODS[config]
    result_dir = (
        result_root
        / dataset
        / method["result_method"]
        / f"query_g{group_count}"
    )
    return result_dir / "weights.txt", result_dir / method["stats"]


def batch_result_root(run_root: Path, config: str) -> Path:
    return run_root / "results" / config


def instance_result_root(run_root: Path, config: str, query_index: int) -> Path:
    return run_root / "instance_results" / config / f"q{query_index:04d}"


def run_one(
    run_root: Path,
    panel_root: Path,
    config: str,
    dataset: str,
    group_count: int,
    timeout_seconds: int,
    query_count: int,
    query_index: int | None = None,
) -> dict[str, object]:
    method = METHODS[config]
    exe = executable(method["exe"])
    if not exe.exists():
        raise RuntimeError(f"executable not found: {exe}")
    result_root = (
        batch_result_root(run_root, config)
        if query_index is None
        else instance_result_root(run_root, config, query_index)
    )
    log_name = (
        f"g{group_count}.log"
        if query_index is None
        else f"g{group_count}_q{query_index:04d}.log"
    )
    log_path = run_root / "raw" / config / dataset / log_name
    log_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(exe),
        dataset,
        str(result_root),
        f"g{group_count}",
        str(panel_root),
        str(query_index or 1),
        "1" if query_index is not None else "-1",
        *method["args"],
    ]
    begin = time.perf_counter()
    status = "error"
    exit_code: int | str = ""
    with log_path.open("w", encoding="utf-8") as log_file:
        log_file.write("> " + " ".join(command) + "\n")
        log_file.flush()
        try:
            process = subprocess.run(
                command,
                cwd=ROOT,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                timeout=timeout_seconds,
                check=False,
                text=True,
            )
            exit_code = process.returncode
            status = "ok" if process.returncode == 0 else "error"
        except subprocess.TimeoutExpired:
            status = "timeout"
            exit_code = "timeout"
            log_file.write(f"\n# benchmark timeout after {timeout_seconds}s\n")
    wall_s = time.perf_counter() - begin
    weights_path, _ = result_paths_from_root(
        result_root, config, dataset, group_count
    )
    completed = len(read_latest_weights(weights_path))
    return {
        "config": config,
        "dataset": dataset,
        "g": group_count,
        "query": query_index or "",
        "status": status,
        "exit_code": exit_code,
        "wall_s": f"{wall_s:.6f}",
        "completed_queries": completed,
        "requested_queries": query_count,
        "log": str(log_path.relative_to(run_root)),
    }


def run_benchmarks(
    run_root: Path,
    panel_root: Path,
    configs: list[str],
    datasets: list[str],
    groups: list[int],
    timeout_seconds: int,
    query_count: int,
    resume: bool,
    execution_mode: str,
) -> None:
    status_path = run_root / "run_status.csv"
    previous = load_status(status_path) if resume else {}
    for dataset in datasets:
        for group_count in groups:
            for config in configs:
                query_indices: list[int | None] = (
                    [None]
                    if execution_mode == "batch"
                    else list(range(1, query_count + 1))
                )
                for query_index in query_indices:
                    query_key = "" if query_index is None else str(query_index)
                    key = (config, dataset, group_count, query_key)
                    label = f"{config} {dataset} g={group_count}"
                    if query_index is not None:
                        label += f" q={query_index}"
                    if key in previous and previous[key]["status"] == "ok":
                        print(f"[skip] {label}", flush=True)
                        continue
                    print(f"[run] {label}", flush=True)
                    row = run_one(
                        run_root,
                        panel_root,
                        config,
                        dataset,
                        group_count,
                        timeout_seconds,
                        query_count if query_index is None else 1,
                        query_index,
                    )
                    append_status(status_path, row)
                    previous[key] = {
                        field: str(value) for field, value in row.items()
                    }
                    print(
                        f"[{row['status']}] completed={row['completed_queries']}/"
                        f"{row['requested_queries']} wall={row['wall_s']}s",
                        flush=True,
                    )


def safe_float(value: str | None) -> float:
    if value is None:
        return math.nan
    try:
        return float(value)
    except ValueError:
        return math.nan


PER_QUERY_FIELDS = [
    "config",
    "dataset",
    "g",
    "query",
    "time_s",
    "weight",
    "peak_rss_mb",
    "rss_before_mb",
    "incremental_peak_mb",
]


def collect_per_query(
    run_root: Path,
    configs: list[str],
    datasets: list[str],
    groups: list[int],
    query_count: int,
    execution_mode: str,
) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for config in configs:
        for dataset in datasets:
            for group_count in groups:
                result_sets: list[tuple[int, list[dict[str, float]], list[dict[str, str]]]]
                if execution_mode == "batch":
                    weights_path, stats_path = result_paths_from_root(
                        batch_result_root(run_root, config),
                        config,
                        dataset,
                        group_count,
                    )
                    result_sets = [
                        (1, read_latest_weights(weights_path), read_latest_stats(stats_path))
                    ]
                else:
                    result_sets = []
                    for query_index in range(1, query_count + 1):
                        weights_path, stats_path = result_paths_from_root(
                            instance_result_root(run_root, config, query_index),
                            config,
                            dataset,
                            group_count,
                        )
                        result_sets.append(
                            (
                                query_index,
                                read_latest_weights(weights_path),
                                read_latest_stats(stats_path),
                            )
                        )
                for first_query, weights, stats in result_sets:
                    for index, weight_row in enumerate(weights):
                        stats_row = stats[index] if index < len(stats) else {}
                        rss_before = safe_float(stats_row.get("rss_before_mb"))
                        peak = weight_row["peak_rss_mb"]
                        incremental = (
                            max(0.0, peak - rss_before)
                            if math.isfinite(peak) and math.isfinite(rss_before)
                            else math.nan
                        )
                        rows.append(
                            {
                                "config": config,
                                "dataset": dataset,
                                "g": group_count,
                                "query": first_query + index,
                                "time_s": weight_row["time_s"],
                                "weight": weight_row["weight"],
                                "peak_rss_mb": peak,
                                "rss_before_mb": rss_before,
                                "incremental_peak_mb": incremental,
                            }
                        )
    path = run_root / "per_query.csv"
    with path.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=PER_QUERY_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    return rows


SUMMARY_FIELDS = [
    "scope",
    "dataset",
    "g",
    "baseline",
    "release_completed",
    "baseline_completed",
    "paired_queries",
    "answer_mismatches",
    "release_total_s",
    "baseline_total_s",
    "sum_speedup",
    "median_query_speedup",
    "geomean_query_speedup",
    "release_max_peak_rss_mb",
    "baseline_max_peak_rss_mb",
    "absolute_peak_ratio",
    "release_max_incremental_mb",
    "baseline_max_incremental_mb",
    "incremental_peak_ratio",
]


def geomean(values: list[float]) -> float:
    positive = [value for value in values if value > 0 and math.isfinite(value)]
    if not positive:
        return math.nan
    return math.exp(sum(math.log(value) for value in positive) / len(positive))


def summarize_pair(
    release_rows: list[dict[str, object]], baseline_rows: list[dict[str, object]]
) -> dict[str, object]:
    release_by_key = {
        (str(row["dataset"]), int(row["g"]), int(row["query"])): row
        for row in release_rows
    }
    baseline_by_key = {
        (str(row["dataset"]), int(row["g"]), int(row["query"])): row
        for row in baseline_rows
    }
    keys = sorted(release_by_key.keys() & baseline_by_key.keys())
    release = [release_by_key[key] for key in keys]
    baseline = [baseline_by_key[key] for key in keys]
    speedups = [
        float(base["time_s"]) / float(rel["time_s"])
        for rel, base in zip(release, baseline)
        if float(rel["time_s"]) > 0
    ]
    release_total = sum(float(row["time_s"]) for row in release)
    baseline_total = sum(float(row["time_s"]) for row in baseline)
    release_peaks = [float(row["peak_rss_mb"]) for row in release]
    baseline_peaks = [float(row["peak_rss_mb"]) for row in baseline]
    release_incremental = [
        float(row["incremental_peak_mb"])
        for row in release
        if math.isfinite(float(row["incremental_peak_mb"]))
    ]
    baseline_incremental = [
        float(row["incremental_peak_mb"])
        for row in baseline
        if math.isfinite(float(row["incremental_peak_mb"]))
    ]
    release_peak = max(release_peaks, default=math.nan)
    baseline_peak = max(baseline_peaks, default=math.nan)
    release_incremental_peak = max(release_incremental, default=math.nan)
    baseline_incremental_peak = max(baseline_incremental, default=math.nan)
    return {
        "release_completed": len(release_rows),
        "baseline_completed": len(baseline_rows),
        "paired_queries": len(keys),
        "answer_mismatches": sum(
            abs(float(rel["weight"]) - float(base["weight"])) > 1e-6
            for rel, base in zip(release, baseline)
        ),
        "release_total_s": release_total,
        "baseline_total_s": baseline_total,
        "sum_speedup": baseline_total / release_total if release_total > 0 else math.nan,
        "median_query_speedup": statistics.median(speedups) if speedups else math.nan,
        "geomean_query_speedup": geomean(speedups),
        "release_max_peak_rss_mb": release_peak,
        "baseline_max_peak_rss_mb": baseline_peak,
        "absolute_peak_ratio": baseline_peak / release_peak if release_peak > 0 else math.nan,
        "release_max_incremental_mb": release_incremental_peak,
        "baseline_max_incremental_mb": baseline_incremental_peak,
        "incremental_peak_ratio": (
            baseline_incremental_peak / release_incremental_peak
            if release_incremental_peak > 0
            else math.nan
        ),
    }


def write_summaries(
    run_root: Path,
    rows: list[dict[str, object]],
    configs: list[str],
    datasets: list[str],
    groups: list[int],
) -> None:
    if "ReleaseV5" not in configs:
        return
    summary_rows: list[dict[str, object]] = []
    for baseline_config in configs:
        if baseline_config == "ReleaseV5":
            continue
        for dataset in datasets:
            for group_count in groups:
                release = [
                    row
                    for row in rows
                    if row["config"] == "ReleaseV5"
                    and row["dataset"] == dataset
                    and row["g"] == group_count
                ]
                baseline = [
                    row
                    for row in rows
                    if row["config"] == baseline_config
                    and row["dataset"] == dataset
                    and row["g"] == group_count
                ]
                summary_rows.append(
                    {
                        "scope": "dataset-g",
                        "dataset": dataset,
                        "g": group_count,
                        "baseline": baseline_config,
                        **summarize_pair(release, baseline),
                    }
                )
        for group_count in groups:
            release = [
                row
                for row in rows
                if row["config"] == "ReleaseV5" and row["g"] == group_count
            ]
            baseline = [
                row
                for row in rows
                if row["config"] == baseline_config and row["g"] == group_count
            ]
            summary_rows.append(
                {
                    "scope": "all-datasets-g",
                    "dataset": "ALL",
                    "g": group_count,
                    "baseline": baseline_config,
                    **summarize_pair(release, baseline),
                }
            )
    path = run_root / "summary.csv"
    with path.open("w", encoding="utf-8", newline="") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        writer.writerows(summary_rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default=dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    parser.add_argument("--datasets", default=",".join(DEFAULT_DATASETS))
    parser.add_argument("--groups", type=parse_groups, default=DEFAULT_GROUPS)
    parser.add_argument("--methods", default=",".join(DEFAULT_METHODS))
    parser.add_argument("--query-count", type=int, default=20)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    parser.add_argument(
        "--execution-mode", choices=("batch", "instance"), default="batch"
    )
    parser.add_argument("--data-root", type=Path, default=ROOT / "data")
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--summarize-only", action="store_true")
    parser.add_argument("--resume", action="store_true")
    options = parser.parse_args()

    datasets = parse_csv_strings(options.datasets)
    configs = parse_csv_strings(options.methods)
    unknown = [config for config in configs if config not in METHODS]
    if unknown:
        raise SystemExit(f"unknown method configurations: {unknown}")
    if options.query_count <= 0 or options.timeout_seconds <= 0:
        raise SystemExit("query-count and timeout-seconds must be positive")

    run_root = ROOT / "result_snapshot" / "release_v5_vs_pruneddp" / "runs" / options.tag
    if run_root.exists() and not (options.resume or options.summarize_only):
        raise SystemExit(f"run root already exists; use --resume: {run_root}")
    run_root.mkdir(parents=True, exist_ok=True)

    metadata = {
        "tag": options.tag,
        "created": dt.datetime.now().isoformat(timespec="seconds"),
        "datasets": datasets,
        "groups": options.groups,
        "configs": {config: METHODS[config] for config in configs},
        "query_count": options.query_count,
        "execution_mode": options.execution_mode,
        "timeout_seconds_per_process": options.timeout_seconds,
        "source_data_root": str(options.data_root),
        "memory_primary": "absolute per-query peak RSS from weights.txt",
        "memory_diagnostic": "peak RSS minus rss_before from stats",
    }
    metadata_path = run_root / "metadata.json"
    if not metadata_path.exists():
        metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")

    panel_root = run_root / "input"
    if not options.summarize_only:
        panel_root = prepare_panel(
            run_root, datasets, options.groups, options.query_count, options.data_root
        )
        if not options.prepare_only:
            run_benchmarks(
                run_root,
                panel_root,
                configs,
                datasets,
                options.groups,
                options.timeout_seconds,
                options.query_count,
                options.resume,
                options.execution_mode,
            )
    rows = collect_per_query(
        run_root,
        configs,
        datasets,
        options.groups,
        options.query_count,
        options.execution_mode,
    )
    write_summaries(run_root, rows, configs, datasets, options.groups)
    print(f"results: {run_root}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
