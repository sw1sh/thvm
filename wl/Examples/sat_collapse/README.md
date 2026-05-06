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
- [x] Correctness baseline + benchmark vs `SatisfiabilityInstances`.
- [ ] Diagnose / fix `OP2[SUP^a, SUP^b]` cnf blowup.  Add `interact_op2_sup`?
- [ ] Re-implement SAT via SUP-distribution; benchmark.
- [ ] Compile formula evaluator as a `TDef`, `TAOTRun[..., Method -> "Metal"]` for batched per-assignment evaluation on GPU.
