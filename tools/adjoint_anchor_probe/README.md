# Adjoint Anchor Probe

This probe validates an exact forward/backward cut of the permanent-anchor
subset DP. For a nonempty ordinary block `Y`, the forward transition is

```text
T_Y(f) = Close(f + D_Y).
```

Under the min-plus inner product and an undirected graph metric, its adjoint is

```text
T_Y*(h) = D_Y + Close(h).
```

The probe compares the complete forward anchored lattice with every possible
cut: anchored rows at or below the cut are built forward, while all higher
consumers are propagated backward through the adjoint transitions. It also
checks the fixed-anchor Test96 counterexample and random graphs with zero
weights, overlapping groups, and one or two candidate vertices per group.

For each random instance, the probe also constructs every terminal row twice:
first by enumerating the complement partitions of each target mask, and then
by sorting all ordinary values at a vertex by their dual-reduced value, and
finally by enumerating only submasks of each mask complement. All three
constructions are compared under several incumbent budgets. This separately
checks Test145's additive directed-cut identity, sorted early stop, and bounded
fallback route.

```powershell
cmake --build build --config Release --target gst_adjoint_anchor_probe
./build/Release/gst_adjoint_anchor_probe.exe 718201 500 9 9 15 3
```

Arguments are `seed iterations max_n max_g max_edges min_g`.

The production Test142 implementation applies the same adjoint construction to
Test80's root-irreducible ordinary branch rows and prunes backward values with
lower bounds for the already included anchor-and-mask prefix. The dense probe
checks the algebra independently; end-to-end equivalence of the adapted row
semantics is checked by `gst_random_compare` against DPBF.
