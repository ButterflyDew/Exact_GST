# Paid Core Growth Probe

This fast-graph research probe removes high rooted rows from Test75's candidate
construction. It builds exact rooted rows only through
`q = ceil(floor(g/2)/2)`, restores their edge witnesses, and then:

1. extracts permanent-anchor skyline paid-pair trees;
2. attaches disjoint blocks of at most `q` groups at internal tree vertices;
3. unions witness edges so already-paid edges are not charged twice;
4. Pareto-prunes states by cost and all remaining low-block profiles;
5. completes a core of size `g-2q` with two blocks of size at most `q`.

Growth uses two balanced canonical blocks after the pair (`2+2` for g12 and
`3+2` for g13). Attachment is restricted to internal points attaining the
current low-block profile minimum.

```powershell
cmake --build build --config Release --target gst_paid_core_growth_probe

.\build\tools\paid_core_growth_probe\Release\gst_paid_core_growth_probe.exe `
  data_snapshot\generated_fast DBLP_data_bfs g12 12.166303 1
```

`--profile-seeds` is a diagnostic pricing bound: it starts from every D2 root
and Pareto-prunes by cost plus all low-block profiles instead of using only the
anchor skyline. It is intentionally not a production path and can use several
GiB even on fast Toronto.

`--price-best-plan` keeps the skyline solve, then performs two diagnostics: it
scans all pair roots for the winning paid-core plan, and separately prices the
partition selected by same-root three-block completion after turning its
smallest block into a paid tree. Neither diagnostic is an exact stopping
certificate by itself.

`--q value` changes the low-row/core tradeoff for diagnostics. The canonical
growth sizes are balanced under that q; for g12, `--q 2` uses
`pair+2+2+2`, followed by `2+2` completion.

`--core-size value` decouples paid-core size from `g-2q`. In particular,
`--q 5 --core-size 4` tests `pair+2-block -> core4`, followed by `4+5` on
g13. `--price-strong-plans` is an oracle diagnostic: using the supplied known
optimum, it prices every plan whose strong lower bound is still smaller. It
must not be used as an end-to-end solver or as evidence that the optimum was
known internally.

`--price-factorized-plans` prices the same core4 plan family without restoring
every added-pair witness. For an origin pair `P` rooted at `r`, an added pair
`A` attached at `x`, and a completion block `B`, it uses

```text
base(P,A,r,x) = D(P,r) + phi_P(A,r)
phi_(P union A_x)(B) = min(phi_P(B,r), phi_A(B,x)).
```

Each `phi` is evaluated only at requested vertices by walking the two singleton
parent forests and the paid-pair predecessor forest. Equal-cost attachment
points are represented by persistent predecessor lists; a dense stamp removes
duplicates when the two singleton paths overlap. This path uses ordered arrays
and lists, not a hash table.

No q currently passes the cross-dataset gate. In particular, q5 collapses to
pair+half and still requires a very large all-root paid front on Toronto-new.
For Toronto g13 q1, q5 skyline completion is `0.7401396340`; pricing the same
`3+5+5` plan over all roots reaches the exact `0.7048467020` in `1.062s` after
the dense low rows exist. Component-only lower bounds still leave
`28,150 / 36,036` plans below the optimum, so this result is a strong upper
oracle, not permission to delete the high exact rows.

The later core3 completeness search found a fixed-g13 `106 -> 107`
counterexample, even when singleton attachment may use every pair-tree point.
Core4 repairs the tested counterexample family and reaches the Toronto g13
exact value. Targeted g13 evidence is now `720/720`, but this remains evidence
rather than a decomposition theorem. Across the five fast databases,
the factorized and explicit implementations agree on all `4,387,627` checked
attachment-root sets and all `8,250` plan prices. Toronto g13 prices the same
`5,476` strong plans as the explicit implementation:

```text
explicit witness pricing       39.158s
factorized requested pricing   18.721s
factorized records             6,631,397
factorized completion probes   22,157,960
best / first exact rank        0.7048467020 / 1,552
```

The complete dense-oracle probe falls from about `145.7s` to `111.0s`, but the
oracle still uses the supplied optimum to select plans. Core4 therefore remains
a research boundary, not a Test21 integration or a stopping certificate.

Three later diagnostics isolate whether the pair-grown core can be replaced by
the existing D4 row:

- `--price-rooted-core-plans` restores one deterministic rooted-optimal D4
  witness per root;
- `--price-optimal-core-plans` propagates all tight D1--D4 witness derivations
  with the exact three-scalar family `(min phi_B, min phi_C,
  min(phi_B+phi_C))`;
- `--price-macro-core-plans` treats the four core groups and two completion
  blocks as six macro terminals and runs a 64-state Steiner DP.

The first path prices Toronto g13 in `8.023s`, reaches `0.7048467020`, and keeps
first exact rank `1,552`; it is simpler and `2.33x` faster than pair-forest
pricing. It is nevertheless incomplete across datasets. On fast MovieLens g12
all three diagnostics remain above the known exact value:

```text
deterministic D4 witness        0.0202203614
all optimal D4 witnesses       0.0202203614
six-label macro DP             0.0202203613
known exact                    0.0202189774
```

Thus the missing information is a non-minimum paid-core geometry that is
Pareto-relevant to future attachments, not an equal-cost predecessor tie or a
different reassociation of minimum D4. None of these diagnostics permits
deleting D6/A5.

The implementation intentionally uses dense low rows as a semantic oracle. Do
not run it on full DBLP; production work must replace those rows with Test21's
sparse ordered representation.

## First-order block-anchor diagnostic

`--price-first-order-anchor-plans` exactly prices each selected
`two blocks + core4` plan with a path decomposition that needs D0/D1 rows on
one block and D0/D1/D2 rows on the other. Required `(block, subset)` keys are
sorted and deduplicated, and every dependency is found by binary search; this
path does not use a hash table.

The identity passed explicit connected-subgraph checks on `500/500` singleton
and `200/200` two-candidate-group instances. It is not a production solver:
Toronto-fast g12 prices all `1,269` plans below its own three-block upper but
stops at `0.9688685500`, above exact `0.9616227800`, while row construction and
pricing add `2.955s + 0.263s`. The fixed-plan identity is retained; the
incomplete core4 plan family is rejected. See
`readme_files/archive/test76_first_order_block_anchor_20260713.md`.

## Pendant-cherry boundary diagnostic

`--measure-pendant-cherry-plans` is the withdrawn-family diagnostic recorded
by Test78. It enumerates three size-3/4 blocks plus a core of at most four
groups, sorts and deduplicates their half-row consumers, and can use `q=half`
as a dense oracle. Toronto prices all `275,275` plans but remains at
`0.9688685500 > 0.9616227800`; DBLP-fast happens to hit exact. The option is
retained only to reproduce the single-interface counterexample. It is not a
solver path and must not be run on full DBLP.
