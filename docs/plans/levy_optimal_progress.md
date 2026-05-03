# Levy-optimal port progress

Status: Phases 1-4 landed (single-threaded slice).  Phase 5 (parallel
CNF) explicitly out of scope.

## What landed (commit f0d3c52)

- **Phase 1**: `TAG_DP0` / `TAG_DP1` are now WHNF roots in
  `src/wnf/_.c`.  Entering them no longer pushes a frame nor fires
  DUP-XXX.  The grad-flag DP0 / DP1 paths keep their old behaviour
  (FWD passthrough / BWD chain rule) since those interactions are
  semantically distinct from plain duplication.
- **Phase 2**: `src/cnf/_.c` adds `cnf_at` / `cnf`: reduces a term
  to WHNF, then walks compound nodes (LAM, CTR, APP, OP2, EQL, AND,
  OR, WHEN, ANN) lifting the first SUP child to the top.  When the
  walker reaches a DP, it cnf's the cell, dispatches the matching
  `interact_dup_*`, and recurses on the result.  `src/eval/collapse.c`
  adds `eval_collapse`: priority-FIFO enumeration of every pure leaf
  reachable through SUP branches.  Stripped of the parallel
  `CnfPool` / `wspq` machinery in HVM4 -- single-threaded slice.
- **Phase 3 (partial)**: every wnf consumer audited and migrated to
  cnf where DPs are expected at the head:
  - `thvm_collapse` and `thvm_collapse_ordered` switched to cnf so
    shallow-collapse walks resolve DP-headed children (this
    propagates to the ATP arc which calls `thvm_collapse_ordered`).
  - APP-MAT path drives its arg through `cnf` so a DP-wrapped CTR /
    NUM at the head fires `DUP-CTR` / `DUP-NUM` before the
    case-tree dispatch.
  - APP / OP2 / EQL / AND / OR / WHEN / ANN apply-frame heads
    drive Levy-opaque DP heads through `cnf` before the per-tag
    dispatch.
  - `aot_force` (driver for hand-coded AOT case trees: fib_nat,
    gab_tak, u32_fib) calls `cnf` so AOT case trees see a CTR /
    NUM head instead of a stuck DP.
  - WL bridge gains `TCnf` (`thvm_wl_cnf`); `TNf` post-processes
    its `nf` result through `cnf` so user-visible terms are
    DP-free.  `TWnf` and `TStep` stay head-only per the plan.
  - Lazy.wl `tlazyDecodeRaw`, `tlazyConsToList`, `lazyStep`,
    `decodeForced` switched from `TWnf` to `TCnf` so DP-rooted
    Cons cells from auto-dup'd recursive bodies get resolved
    during decode.

## Phase 4: lift the REF/ALO bail in `auto_dup_collect`

Replaced the early-return on `TAG_REF` / `TAG_ALO` with `continue`,
so the walker treats those subtrees as opaque leaves.  This is
sound because both tags are closed wrt our local `lam_loc`: a REF
cell points at a book def (its binders are its own), and an ALO
cell is a suspended realize-template that closes over its
`state_id`, not the caller's lam.  Neither subtree can possibly
contain a `TAG_VAR(lam_loc)` cell.

Under Phase 1+2, recursive non-linear bodies handle their own
per-call duplication correctly: each recursive invocation
`alo_realize`s a fresh dyn DUP chain whose sibling DP0/DP1 cells
share a body via `alo_dup_share` (`src/alo/realize.c`), and `cnf`
fires DUP-XXX on demand at readback time without compounding
across calls.  The early Phase 4 attempt that triggered heap
exhaustion / SIGSEGV was caused by removing the bail entirely so
the walker recursed into REF/ALO cells and dereferenced their
`val` as a dyn-heap loc -- but those vals are book locs / book
templates, not dyn locs.  Treating them as leaves avoids the
false recursion; the walk converges immediately.

Verified end-to-end:
- `tests/test_auto_dup.c`: new
  `auto-dup/recursive-body-builds-dup-chain` confirms a recursive
  body with a `TAG_REF` rec-call gets a 3-DUP chain inserted for
  4 VAR uses.
- WL: `TLazyPermutations[Range[100]]` take 5 returns five 100-element
  permutations; `TLazySplits[Range[5]]` take 6 returns all six
  splits; `TLazySubsets[{a,b,c,d}]` take 8 enumerates first eight
  subsets; `TLazyTuples` works.  None of the recursive-body
  helpers in `Lazy.wl` need manual `TDup` anymore.
- C suite: 274/274 + 27 cnf + 21 auto-dup.
- `lazy.wlt` 41/41, `atp.wlt` 28/28, `aot.wlt` 31/31, `core.wlt`
  32/32, `grad.wlt` 62/62.

## Phase 5: parallel CNF (not started)

The HVM4 CnfPool / Wspq atomics are explicitly out of scope for
this arc.  Will revisit when a downstream user demands multi-core
CNF.

## What works end-to-end after Phase 1+2+3

- All 274 existing C tests + 27 new cnf cases.
- `wl/THVMLink/Tests/lazy.wlt`: 41/41.
- `wl/THVMLink/Tests/core.wlt`: 32/32 (three TWnf-of-DP tests
  migrated to TCnf inline).
- `wl/THVMLink/Tests/atp.wlt`: 28/28.
- `wl/THVMLink/Tests/aot.wlt`: 31/31 (via `aot_force` + `cnf`).
- `wl/THVMLink/Tests/grad.wlt`: 62/62 (no DP-handling changes
  required -- grad DPs use the `DUP_GRAD_FLAG` ext bit which
  routes through the existing chain-rule path in wnf and was
  preserved verbatim).

The remaining wl-test files (bench / dtype / kernel) were not
exercised end-to-end as part of this commit but rely on the same
TWnf / TNf surface -- TNf now post-cnf's, and TWnf stays head-only
as the plan asks, so a regression there would surface as a
TWnf-on-DP-head expectation that the consumer can fix locally by
switching to TCnf.
