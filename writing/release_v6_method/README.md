# ReleaseV6 method manuscript

`release_v6_method.tex` is a standalone methods-only manuscript using the same `acmart`/VLDB two-column style as `../2026_VLDB_MonoGST_resource`. It contains a reading guide, notation and state roles, a running example, code walkthroughs for both algorithms, the pruning layer, correctness proofs, complexity analysis, and method-positioning references for `methods/Release/release_v6.cpp`.

Build from this directory with:

```powershell
latexmk -pdf -interaction=nonstopmode -halt-on-error release_v6_method.tex
```

The local TeX Live installation needs the standard `totpages`, `trimspaces`, `stringenc`, `oberdiek`, `caption`, `libertine`, and `kastrup` packages required by the current `acmart` class.

The deliverable is `release_v6_method.pdf`; it must remain at most six pages including references. The revised 2026-07-19 build is five pages and was checked for unresolved references, overfull boxes, pseudocode formatting, and page-level layout defects.
