# Paid Half-State Probe

The probe now also checks whether exact rooted paid-pair witnesses completed by
two half blocks recover the optimum. It separately measures the permanent-anchor
two-dimensional skyline restriction; that restriction has counterexamples and is
reported only as an initial-column experiment.

It additionally checks low-core growth: rooted paid pairs absorb only blocks of
size at most `q=ceil(floor(g/2)/2)` through real edge unions, then complete with
two q-blocks. Both unrestricted and anchor-skyline seed families are reported.

For odd high-q instances it also compares `core3+5+5`, `core4+4+5`, and
`core5+4+4`. The optional final argument `high-core4` runs only the
`pair+2-block -> core4` generated family; this is useful for fixed g13
counterexample searches without rebuilding every comparison family. The exit
condition checks that selected core4 field directly; skipped generic fields do
not count as success.

`high-core4-groups` gives every group two candidate vertices using a balanced
derangement and compares minimum-only with all-attachment core4 growth.
`rooted-core4-groups` instead makes the rooted-optimal D4 paid-core family the
selected exit condition. These modes test group overlap, extra Steiner
vertices, and alternate graph paths; they do not change Test21.

该工具穷举小图 connected paid subtrees，验证 `cost + attachment profile` 的 Pareto dominance 与 `paid-half + D + D` exact completion。算法定义、证明和当前结果见 `readme_files/history/paid_attachment_half_profiles.md`。

```powershell
cmake --build build --config Release --target gst_paid_half_state_probe

.\build\tools\paid_half_state_probe\Release\gst_paid_half_state_probe.exe `
  713671 1000 8 7 11 5 3

.\build\tools\paid_half_state_probe\Release\gst_paid_half_state_probe.exe `
  713691 50 9 8 13 9 8

.\build\tools\paid_half_state_probe\Release\gst_paid_half_state_probe.exe `
  713961 20 13 13 13 13 13 high-core4

.\build\tools\paid_half_state_probe\Release\gst_paid_half_state_probe.exe `
  714271 10 14 13 16 14 13 high-core4-groups
```

参数依次为：

```text
seed iterations max_n max_g max_edges min_n min_g
```

Fixed g13 currently contains counterexamples to pair+half and to
`core3+5+5`: `160 -> 161` and `106 -> 107`. Both sequential-singleton and
single-2-block `core4+4+5` recover all tested g13 instances (`720/720`, including
extra Steiner vertices, multi-terminal groups, and up to three independent
cycles). Targeted odd-g searches over g7/g9/g11/g13 total `13,520/13,520`.
The min/all-attachment comparison is `1,410/1,410`. These are exhaustive and
adversarial-search evidence, not a decomposition theorem.

`macro-plan` and `macro-plan-groups` also validate the Test76 first-order
block-anchor factorization. For a fixed two-block/core4 plan, they compare the
explicit connected-subgraph value, the full six-label macro DP, full anchored
rows, and the reduced formula using only D0/D1 rows on one block and D0/D1/D2
rows on the other. Current Release/O2 evidence is `500/500` and `200/200`.

`cherry-plan[-groups]` validates exact 3--5 macro-label cherry pricing.
`three-anchor-plan[-groups]` validates the Test78 fixed three-block identity:
core labels are partitioned among three full block-anchor rows and the rows
meet at one junction. Fixed g13 evidence is `1000/1000` singleton and
`500/500` two-candidate-group instances. These are fixed-plan identities; the
Toronto counterexample in Test78 shows that three sequential pendant blocks
cannot in general be treated as three simultaneous macro leaves.

`nested-pendant-family[-groups]` is a deliberate falsification mode for the
naive ordered recurrence `row <- C(D(B)+row)`. Seed `7801` fails immediately
at g9 with `72 -> 78`.

`soft-pendant-family[-groups]` treats the accumulated row as a soft terminal
and runs a local subset DP inside each 3/4-block. The core is limited to three
labels by the 3/4-pendant tree lemma. The outer search is an
exact subset DAG: for every covered mask it stores the pointwise minimum row
over all block orders, then applies every disjoint 3/4-block transform once.
This replaces factorial order recursion without changing the represented
family. The tightened core-3 implementation passes g9 singleton `200/200`
(seed `715621`), fixed-g13 singleton `20/20` (seed `715622`), and fixed-g13
two-candidate groups `20/20` (seed `715623`). These checks do not prove
scalar-state completeness: seed `715613` iteration 3 has a fixed-anchor
`71 -> 73` counterexample because later blocks need different paid attachment
points. The tree lemma and interface barrier are documented in
`readme_files/test95_soft_pendant_decomposition.md`; the exact witness is in
`readme_files/archive/test96_bounded_d4_full_a_20260715.md`. Nothing is
integrated into Test21, Test80, or a release.

工具只做结构证明和代表族计数，不修改 Test21，不读取数据集名，也不使用经验阈值。
