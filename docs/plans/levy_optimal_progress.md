# Levy-optimal port progress

Status: Phase 1 + Phase 2 + Phase 3 landed.  Phase 4 deferred -- see
"Phase 4 blocker" below.

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

## Phase 4 blocker: recursive auto-dup blows up under cnf

The Phase 4 goal -- drop the `TAG_REF` / `TAG_ALO` bail in
`auto_dup_collect` so auto-dup fires for recursive bodies too --
remains unsafe.  Lifting the bail and re-running the WL test suite
hits two problems:

1. **Heap exhaustion** on `TLazyPermutations[Range[20]]`: each
   recursive call of `lazyPermsLex` creates fresh DUP cells around
   its non-linear binders.  When `cnf` drives those DPs at MAT
   dispatch, `interact_dup_ctr` commutes the dup through the list
   CTR by wrapping each child cell in a fresh DUP.  The next
   recursive call's auto-dup chain layers on top.  After ~5
   recursive levels the heap fills up.

2. **Native crash** (signal 11) on `TLazyPermutations[{a, b, c}]`
   when the auto-dup'd recursive body reduces enough levels to
   walk through `term_clone`-style siblings whose dup cell was
   already heap_subst_cop'd by a sibling projection.

The traditional cure is **Levy-optimal sharing with memoised
dup-bodies across recursive calls**: when two recursive invocations
construct a DUP wrapping the same heap-loc body, share the same
cell so DUP-XXX fires once instead of N times.  Our existing
`alo_dup_share` mechanism provides this for `alo_realize` (REF
unfold) but doesn't apply to per-call auto-dup chains constructed
by `lam_seal_ext_with_auto_dup`.  Bridging the two -- key the
auto-dup chain on the def-id + binder + arg -- is a separate
substantial piece of work.

Until that lands, the bail in `auto_dup_collect` stays:

```c
if (tag == TAG_REF || tag == TAG_ALO) {
  return LAM_AUTODUP_BAIL;
}
```

`tests/test_auto_dup.c` carries a regression test
(`auto-dup/recursive-body-bails-conservatively`) that confirms the
walker bails on a `TAG_REF`-bearing body so recursive non-linear
lambdas continue to require manual `TDup`.  All 21 auto-dup tests
green; full C suite 274/274 + 27 cnf cases.

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
