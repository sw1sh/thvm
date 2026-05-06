# Collapse-based SAT solver (work in progress)

Goal: build a SAT solver that uses our SUP / DUP / cnf machinery to
explore all 2^V variable assignments simultaneously with optimal
sharing -- the canonical "supgen"-style use case from HVM4.

## Files

- `baseline.wls` -- working brute-force enumeration in WL: build the
  formula as a TTerm, evaluate per-assignment via `TWnf`, filter by
  result.  Correctness verified vs `SatisfiabilityInstances`.

- `bench.wls` -- bench of the brute-force baseline against
  `SatisfiabilityInstances` on random 3-CNF instances of growing
  size.  Currently 170-3940x slower (expected; brute without
  sharing vs optimised CDCL).

## The collapse-based path

The supgen idea: each variable becomes a `SUP^L_i{NUM(0), NUM(1)}`
with its OWN label.  The formula -- nested `OP2*` (AND) and
`OP2-` / `OP2*` (OR-via-NOT-AND-NOT) over those SUP-typed inputs --
distributes the SUPs through the operations via `cnf` lifts.  After
full collapse, the leaves are NUM(0) / NUM(1) tagged with which
branch path they came from; satisfying assignments are the NUM(1)
leaves.

This relies on optimal sharing: common subexpressions across the
2^V branches share via DUP.  For a CNF with structure (e.g., reused
literals, sub-formulas), the share factor can be substantial.

## Blocker

`TCollapse` is now wired to the C-side `thvm_collapse` walker
(commit on this branch).  It works on simple terms (single SUP,
nested same-label SUPs, SUP-of-NUM, ...).

It does NOT work on `OP2[*, SUP^a, SUP^b]` with **different labels** (a != b).
The cross-product commute path through `OP2-SUP` doesn't exist
(no `interact_op2_sup`), so cnf falls through to the lift-via-cnf_node2
path, which when nested through DUP-wrapped SUPs allocates
unboundedly.  Reproduces with `wl/Examples/sat_collapse/cnf_blowup.wls`.

The fix likely needs either:
1. An `interact_op2_sup` rule that commutes OP2 over SUPs at the
   strict-frame level (mirrors `interact_app_sup`).  Would handle
   the common case directly.
2. A guard in cnf_node2's SUP-lift path that detects the
   already-cnf'd-but-still-SUP children case and stops re-driving.

Until then, collapse-based SAT can use:
- Same-label SUPs for variables (loses cross-product enumeration -- only pairwise).
- Or eager assignment enumeration (the `baseline.wls` approach).

## Roadmap

- [x] `TCollapse` WL bridge over `thvm_collapse` C walker.
- [x] Correctness baseline (`baseline.wls`) + benchmark vs `SatisfiabilityInstances`.
- [x] Scout Victor Taelin's gist; reorient on Church-encoded booleans.
- [x] `v1_church.wls` -- sat-check via SUP-distribution.  Correct on
      6 manual + 4 small benched CNFs (up to V=4 / C=5).
- [x] **Land HVM4-style DP/BJ split** (option (2) from
      `CNF_DIVERGENCE.md`).  Plain DPs fire DUP-XXX eagerly at wnf;
      book templates carry BJ; alo_realize unfolds BJ -> fresh dyn DP.
      Removes the cnf_dp stuck-on-APP-of-DP wall.  v1 now correct
      through V=9 / C=15 (was V=4 / C=5).
- [ ] Add the collapser tree (Col_i wrappers + `Join`) to extract
      satisfying assignments, not just a sat boolean.
- [ ] Bench at scale: 30+ vars where Victor reports order-of-magnitude
      speedup vs Rust brute via subformula sharing.
- [x] **`v2_kernel_graph.wls`** -- compute-graph codegen + Metal
      rendering (NOT the TAOTRun AOT path).  Each var's column is
      a `[2^V]` constant tensor; the formula is a UOP graph
      (Add/Mul/Neg) over those tensors; TRealize materializes into
      UOP_KERNELs that the Metal backend renders.  Correct through
      V=20.  Trails WL CDCL by 8-600x (V=6..20) because each
      elementwise op currently dispatches as its own Metal launch
      -- needs kernel fusion across Add/Mul/Neg chains to close the
      gap (autotune-ladder territory).
- [ ] (Aside) The actual `TAOTRun + Method -> "Metal"` path is
      structurally wrong for batched SAT: `src/aot/metal_emit.c`
      compiles a TDef body of TNum/TOp2/TMat over N scalar args
      into one MSL kernel returning one scalar Term per launch, so
      brute force would pay 2^V launches at ~190us each.  Right
      tool for batching MANY *independent* scalar redexes (as in
      `TAOTBatchOp2Fold`) -- wrong tool for vectorising one program
      across an assignment matrix.
- [ ] (Tangential) `interact_op2_sup` rule for numeric search problems
      (Pythagorean triples, subset sum, ...).  Off the SAT critical path.

## v1 limit (resolved)

The `cnf_dp` wall described below was the symptom of an architectural
divergence from HVM4 (`CNF_DIVERGENCE.md`).  Resolved by landing the
DP/BJ split: plain DPs fire eagerly at wnf, book templates carry BJ,
alo_realize unfolds BJ -> fresh dyn DP per realize.  v1 now correct
through V=9 / C=15.

The original symptom for reference: when SUP-typed variables get used
many times across clauses (= the formula tree has many APP applications
at the var), auto-dup wraps each use in a DP^L_internal projection.
After APP-SUP commute fires across these, leaves used to shape like

    APP[APP[... APP[DP0[lab, APP[DP0[lab', APP[..., LAM[...]]], ...]],
                    LAM[...]] ...], NUM(1)], NUM(0)

with `cnf_dp` bailing on `default: heap_set(loc, body_cnf); return dp;`
when body cnf'd to another APP-of-DP chain.  HVM4 never hit this
because plain DPs fired at wnf before cnf saw them; the BJ tag was
HVM4's escape valve for book-time projections that needed to stay
opaque under wnf+cnf.  We now match that split.
