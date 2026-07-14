# Paid Pair Profile Probe

This research probe builds the same exact-best/TSP-pruned D2 roots used by the
Test21 structural measurements. For every nonanchor pair it reports the exact
two-dimensional projection:

```text
(D({i,j},v), gd_anchor(v)).
```

The probe also supports deterministic predecessor-path witnesses:

- `--witness-vector` computes singleton profiles on internal tree vertices,
  witness skylines, and a feasible paid-singleton upper bound.
- `--upper-only` skips the high-dimensional skyline scans but keeps the
  paid-singleton upper calculation.
- `--summary` suppresses per-pair output.
- `--tree-incidence` restores only anchor-skyline pair witnesses and reports
  their total/unique internal vertices without allocating dense profiles.

```powershell
cmake --build build --config Release --target gst_paid_pair_profile_probe

.\build\tools\paid_pair_profile_probe\Release\gst_paid_pair_profile_probe.exe `
  data_snapshot\generated_fast DBLP_data_bfs g12 12.166303 1 `
  --summary --witness-vector
```

The full DBLP g13 structural command is:

```powershell
.\build\tools\paid_pair_profile_probe\Release\gst_paid_pair_profile_probe.exe `
  data DBLP g13 12.5936282853 1 --summary --upper-only
```

This is not an end-to-end query solver. Its dense path-profile mode used about
`4.81 GiB` on full DBLP and is retained only as evidence; production work must
use sparse block columns. See
`readme_files/history/paid_attachment_half_profiles.md`.
