# Dual-cut independent verifier

`gst_dual_cut_probe` is a research verifier for the directed-cut lower bounds used by Test152/Test157. It does not call the Test80 solver and does not reuse its DP rows.

Build with Release/O2:

```powershell
cmake --build build --config Release --target gst_dual_cut_probe -- /m:1
```

The three correctness modes are:

```powershell
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe --self-check 15715704 1000 7 11
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe --packing-self-check 15715703 3000 7 11
.\build\tools\dual_cut_probe\Release\gst_dual_cut_probe.exe --progressive-self-check 15715706 2000 7 11
```

`--self-check` compares full lower bounds with exact rooted subset DP and checks edge consistency and splice inequalities. `--packing-self-check` additionally builds residual packing on random paid paths and directly checks terminal-zero potentials and both directed capacities. `--progressive-self-check` checks every partial sequential-dual prefix and verifies that the completed prefix equals the eager changed-arc construction.

Packing and progressive instances contain zero-weight edges, overlapping groups, multiple terminals per group, and graph-wide weight scales `1e-6`, `1`, and `1e6`. Audit comparisons use the scale-aware tolerance `1e-10 * max(1, |values|)`; this is a verifier tolerance, not a solver switch.
