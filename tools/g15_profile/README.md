# Original five-dataset g15 profiler

This tool profiles the 40 `g=15` queries from each original graph: Toronto, DBLP, DBpedia, LinkedMDB, and MovieLens. A dataset is loaded once for a contiguous query range. It does not transform the graph or query.

`Test145D2` and `Test145D3` compile the exact Test145 configuration and stop only after the corresponding ordinary-D layer has naturally completed. `Test146`, `Test146D2`, and `Test146D3` add the retained work-amortized anchor-tree schedule. `Test149`, `Test149D2`, and `Test149D3` additionally initialize each dual residual closure only from arcs changed by earlier potentials. `ReleaseV5` is the clean standalone release frozen from the complete Test149 path. A prefix method's `best=-1` is intentional: a prefix is not a complete GST answer. The per-layer incumbent remains available as `best_after_d_s2` or `best_after_d_s3`.

```powershell
python tools/g15_profile/g15_profile.py metadata
python tools/g15_profile/g15_profile.py run --method Test145D2 --datasets all --build
python tools/g15_profile/g15_profile.py run --method Test145 --datasets Toronto --begin 1 --limit 40
python tools/g15_profile/g15_profile.py run --method Test146D3 --datasets MovieLens --panel result_snapshot/g15_profile/runs/20260716_d2_all/Test145D2/frozen_d3_panel.csv
python tools/g15_profile/g15_profile.py run --method Test149D2 --datasets MovieLens --begin 1 --limit 40
python tools/g15_profile/g15_profile.py run --method ReleaseV5 --datasets Toronto --begin 1 --limit 40
```

Each run writes raw solver output, `per_query.csv`, `summary.csv`, `extrema_top5.csv`, and `run_status.csv` under `result_snapshot/g15_profile/runs/<tag>/<method>/`. `summary.csv` reports total, median, P90, sample standard deviation, CV, and mechanism shares only for the queries actually recorded. `extrema_top5.csv` is a deterministic rank report, not an empirical outlier threshold.

The complete 200-query Test145D2 interpretation and all retained/withdrawn follow-ups are documented in `readme_files/g15_five_dataset_profile.md`. Test149's invariant and implementation are documented separately in `readme_files/test149_changed_arc_dual.md`. Do not infer a dataset-wide mean from the frozen five-query D3 panel.
