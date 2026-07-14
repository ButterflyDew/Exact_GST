# Paid Block Completion Probe

This research-only probe tests the constructive side of the Test75 state:

```text
cost(T) + phi_T(B) + phi_T(C),
phi_T(B) = min_{x in T} D(B,x).
```

It builds exact dense ordinary half rows on a fast graph, constructs deterministic
paid pair skeletons, and evaluates every complementary two-block completion both
before and after paying for an anchor path. The known optimum is used only to prune D2 roots with the same
admissible TSP lower bound as the pair-profile probe. The reported completion is
a feasible upper bound, not an end-to-end solver result.

The probe also orders pair/block plans by component lower bounds. The strongest
reported bound combines the three component optima with the two pair-union
optima. `strong_certificate_calls` is the number of exact plan prices needed
before the next lower bound reaches the incumbent; the accompanying pair and
column counters expose whether those calls can be batched by the offline pair
forests.

```powershell
cmake --build build --config Release --target gst_paid_block_completion_probe

.\build\tools\paid_block_completion_probe\Release\gst_paid_block_completion_probe.exe `
  data_snapshot\generated_fast DBLP_data_bfs g12 12.166303 1
```

Do not run this dense-row probe on full DBLP. A production implementation must
generate requested block columns from Test21's sparse ordered rows.
