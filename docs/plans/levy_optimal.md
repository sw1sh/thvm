# Levy-optimal sharing on thvm

Status: proposal.

## Why

`lam_seal_ext_with_auto_dup` (src/lam/auto_dup.c, default-on for the
WL bridge) inserts a DUP chain when a `TLam` body uses its binder
non-linearly.  This works fine for *non-recursive* bodies: tests
in `tests/test_auto_dup.c` cover linear identity, ERA-mask, 2/3/5-use
atomic non-linearity, and `wl/THVMLink/Tests/lazy.wlt`'s
non-recursive cases pass.

It does NOT work for *recursive* bodies (those containing `TAG_REF` /
`TAG_ALO`).  The current implementation bails out in that case so
the WL bridge stays green, but that means recursive functions like
the splits TDef in `wl/THVMLink/Kernel/Lazy.wl` -- which we need
for any non-trivial enumerator -- still need manual `TDup`.

The root cause is well-known: thvm's IC reduces DUPs eagerly.  When
the same recursive body fires `n` times, each invocation creates
its own dup chain, which then chains through `dup_app`/`dup_op2`/
`dup_mat` commute as the arg flows through APP/OP2/MAT inside the
body.  Without sharing, that's at least linear per chain depth per
call site, and easily compounds super-linearly in mutually-recursive
helper graphs (Lazy.wl's splits + prependEach + concat ...).

HVM4 solves this with **Levy-optimal sharing**: DUP cells stay as
*opaque atoms* during reduction (they sit in WHNF, not as redexes),
and a separate **CNF readback** layer enumerates final values by
quoting DUPs away and lifting/collapsing SUPs into branches.  The
cost of duplicating shared work is paid *once per shared subterm*,
not per use site.

## What HVM4 ships

- `clang/wnf/_.c`: `BJ0`/`BJ1` (== thvm's `TAG_DP0`/`TAG_DP1`) are
  in the WHNF group.  Entering them returns `whnf = next; goto
  apply` -- no DUP-XXX firing.
- `clang/cnf/_.c` (~310 lines): `cnf_at(term, depth, par_depth)`
  reduces to WNF, then lifts the first SUP to the top.  `ERA` and
  `INC` propagate.  Multi-threaded variant uses a CnfPool with
  work-stealing deques (`clang/data/wspq.c`).  Single-threaded
  fallback exists.
- `clang/eval/collapse.c` (~225 lines): breadth-first traversal
  with a work-stealing key queue.  Lower numeric keys popped first.
  SUP increases key; INC decreases key.  Single-thread T=1 pops
  FIFO within each key bucket for deterministic ordering.  When a
  branch has no SUP, it prints `cnf(term)`.
- `clang/eval/normalize.c` (~215 lines): SNF (full normal form);
  walks every position post-WHNF.

Same-label SUPs annihilate pairwise; different-label SUPs commute
into a cross product when collapsed.  See
[TinyHVM/HVM4/test/collapse_9.hvm](../../TinyHVM/HVM4/test/collapse_9.hvm)
for the canonical lazy-stream example.

## thvm today

- `src/wnf/_.c`: `TAG_DP0`/`TAG_DP1` are *redex roots*.  Entering
  them does `heap_take(loc)` and either follows the SUB chain or
  pushes a DP frame and descends into the body.  Apply pops the DP
  frame and dispatches DUP-XXX based on the body's WHNF tag.
- `dup_lam`, `dup_sup`, `dup_num`, `dup_ctr`, `dup_uop`, `dup_era`,
  `dup_any`, `dup_ten`, `dup_bri` cover atomic / WHNF cases.
- The new `dup_app`, `dup_op2`, `dup_mat` (Phase 1) cover commute
  through compound nodes -- modeled on HVM4's `wnf_dup_nod`.

The eager-DUP design works for everything thvm shipped before
auto-dup landed: ATP arc, Tensor, UOP, all linear or
manually-TDup'd bodies.  It only breaks down when a recursive
non-linear body's auto-dup chain compounds with itself across
recursive calls.

## Proposed phased port

Splitting Levy-optimal into landable slices.  Each one ships with
its own tests; the runtime stays useful at every step.

### Phase 1: keep DP cells opaque during WNF

The smallest change.  Move `TAG_DP0`/`TAG_DP1` from "enter as
redex" to "WHNF root".  Apply phase doesn't fire DUP-XXX.  WHNF
stops at the DP cell.

This breaks every existing thvm test that relies on eager
DUP-firing.  The ATP arc, Tensor pipeline, autograd all use DUPs
during reduction (via TDup or grad-flavored DPs).  Without a
readback layer, a forced term that contains DPs is *useless* to
the user.

Phase 1 alone is **not landable**.  It's a stepping stone.

### Phase 2: CNF readback (single-threaded)

Add `src/cnf/_.c`: `cnf_at(term)` reduces to WHNF (now keeps DPs
opaque per Phase 1), then lifts the first SUP to the top.  Returns
either a pure (DP-free, SUP-free) term or `SUP{a, b}` whose
children are themselves CNF-able.

Add `src/eval/collapse.c`: walks a term, recursively cnf-ing
subterms, returning a list of pure terms (one per SUP branch path).
Single-threaded for now -- skip work-stealing, do FIFO.

Add `tests/test_cnf.c`: small fixtures that exercise the lift,
ERA propagation, same-label DUP-SUP annihilation, different-label
cross product.

This is the bulk of the work: roughly 400-600 lines of C, plus
testing infrastructure.

### Phase 3: hook reducers to use cnf as needed

Where today's code does `wnf(t)` and expects a final value, audit
each call site:

- WL bridge `TWnf` / `TStep`: stays as-is (head-only).  Caller can
  invoke `TCnf` for full readback.
- WL bridge `TNf`: rewire to use CNF on the post-WNF result so
  user-visible terms are DP-free.
- ATP arc's `thvm_atp_step` / collapse: replace ad-hoc collapse
  helpers with the canonical `cnf_at` + `collapse`.  Stage 8.1's
  SUP-encoded CP enumeration becomes a direct user.
- Lazy.wl's consumer combinators: their `TWnf` calls today force
  one Cons cell; under Phase 1, those Cons cells may have DP
  children that need cnf-resolving before tlazyDecode renders the
  WL value.  Either decode-with-cnf, or thread cnf through the
  walk.

This phase touches a lot of files but each touch is small.
~200-300 lines spread across maybe 15 files.

### Phase 4: enable auto-dup default-on for recursive bodies

Drop the `TAG_REF`/`TAG_ALO` bail in `auto_dup_collect`.  Add a
test (`tests/test_auto_dup.c`) for recursive non-linear bodies:
`countdown(5)`, `sum_to_n(5)`, the splits TDef body shape.  All
should reduce to clean numeric values via cnf.

This is the user-visible payoff: the auto-dup machinery now covers
every non-linear case the user can write in WL, including the
existing `Lazy.wl` recursive helpers.  The specialized helpers
(prependEach, prependHToFirstEach, ...) become unnecessary --
generic `lazyMap` / `lazyConcatMap` with closure-LAM args work.

### Phase 5: parallel cnf

Optional.  Port the work-stealing deque (`clang/data/wspq.c`) and
the parallel CnfPool atomics.  Buys the multi-core scaling HVM4
advertises.  Substantial atomics work and a real concurrency
model.  Skip until single-threaded Levy-optimal is stable.

## Cost / risk

Single-threaded Phase 1+2+3+4: ~1000-1500 LOC C, plus tests, plus
audit of every existing wnf consumer.  Touches the runtime's
hottest path (DP dispatch); regression risk is real and broad
(274 C tests, 41 lazy.wlt, plus everything ATP / Tensor / UOP).

Parallel Phase 5 is a separate undertaking: atomics, deques,
memory model -- not casual.

## Alternative: leave eager DUP, optimize differently

Other ways to address the auto-dup recursive-body case without
going Levy-optimal:

a) **Memoize DUP-NOD by (label, val) pair.**  When the same dup
   body has already been commuted, reuse the result.  Adds a small
   hash table to the wnf inner loop.  Doesn't unlock the parallel
   sharing HVM4 advertises but does break the
   recursive-multiplicative blowup.  Cheap to implement, hard to
   get right under heap GC.

b) **Substitution-based eta for recursive bodies.**  When auto-dup
   detects a recursive body, expand the recursion N levels at
   construction (loop unrolling) up to a budget, then leave the
   tail bound manually.  Bounded; users who exceed the unroll
   write manual TDup.  Pragmatic, not foundational.

c) **Selective auto-dup at the use site.**  Only fire DUP commute
   when the dup-body's value is *actually* shared (multiple
   active references at force time).  Requires reachability
   tracking.  Closer to optimal in practice but requires
   reference-counting or escape analysis.

None of these are "the proper foundation" -- they're heuristics
that paper over the eager-DUP design.  Levy-optimal is the
foundation that closes the loop.

## Recommendation

Two paths to choose from:

1. **Levy-optimal port**, phases 1-4 (single-threaded).  Real
   foundation; auto-dup works for everything; pattern engine and
   ATP arc benefit deeply.  ~1500 LOC, multi-day, real regression
   risk.
2. **Leave the REF-bail** and ship Pattern.wl on top of the
   current foundation.  The pattern engine doesn't need recursive
   non-linear bodies; sequence patterns build via `TLazySplits` /
   `TLazyPermutations` which are already linearity-friendly under
   thvm's reducer.  Pragmatic.  Levy-optimal becomes the next
   foundational arc when a downstream user demands it.

Path 1 is the proper foundation.  Path 2 is the pragmatic
incremental step.  The current commit (`397464e`) leaves us on
path 2 by default; flipping to path 1 means committing to the
phased port above.
