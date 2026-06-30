# Snapshot benchmark data

`data_snapshot` stores the reproducible benchmark plan for cross-dataset regression tests.

- `snapshot_plan.json` is tracked and describes the 5 dataset versions and the fast/normal/large suites.
- `generated_fast/`, `generated_normal/`, and `generated_large/` are produced by
  `tools/snapshot_benchmark/snapshot.py` and are intentionally ignored by git.

Run from the repository root:

```powershell
python tools/snapshot_benchmark/snapshot.py --method Test16 --suite fast --build
```

The runner prepares missing snapshot data, executes the suite's configured `g` values for each dataset version,
and writes results under `result_snapshot/<suite>/<timestamp>/`.

See `readme_files/snapshot_benchmark.md` for the exact graph scale and query count of each suite.
