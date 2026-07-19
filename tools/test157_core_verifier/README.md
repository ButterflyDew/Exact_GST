# Test157 Core Verifier

This is an independent small-instance verifier for the Test152 proof chain. It
does not call the Test152 solver and does not reuse its row containers,
pruning code, or transposed-terminal implementation.

For each handcrafted or random undirected instance it checks:

1. the global `floor(g/2)` half-state three-block terminal against a complete
   canonical rooted subset DP;
2. a standard canonical rooted subset DP against the strict
   root-irreducible branch recurrence at every ordinary mask and root;
3. the complete permanent-anchor forward lattice against a brute-force GST
   oracle that enumerates vertex subsets and computes their MST costs;
4. the balanced `A+D+D` decomposition on an oracle optimum tree, including
   overlapping groups and zero-weight edges;
5. raw suffix rows `F`, closed pullback rows `H=Close(F)`, and every possible
   low/high cut against explicit forward suffix evaluation;
6. target-wise completion rows against exhaustive, sorted-pair, and
   complement-submask transposed event generation under additive potentials.

Build and run with Release/O2:

```powershell
cmake --build build --config Release --target gst_test157_core_verifier -- /m:1
.\build\Release\gst_test157_core_verifier.exe 157001 1000 10 9 18 2
```

Arguments are `seed iterations max_n max_g max_edges min_g`. A successful run
prints one `TEST157_OK` line with the number of checked cells, suffix rows,
cuts, transposed cells, M0/full-DP comparisons, and brute-force oracles.
