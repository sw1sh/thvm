# Iter Z+2: deterministic parallel cnf+collapse on Metal

## Problem (from iter Z+1 profile)

Iter Z+1's `aot_ic_collapse.metal` dispatches kernel-2 with grid=2^depth
threads, each walking one leaf path through the SUP-tree built by iter
Z's per-def kernel.  Two correctness limits surfaced in
`bench/sat_collapse/profile_aot_metal.txt`:

1. **Non-determinism.**  N parallel threads share dup-cells in the
   book heap.  `heap_subst_var` and `heap_subst_cop` write SUB-bit
   marked Terms to those shared cells; without 64-bit slot atomics
   on Apple GPU two threads can both see "no SUB" simultaneously,
   both alloc, both write -- last-writer-wins.  Re-running the same
   case with the same seed gives different `num=` counts and
   sometimes different sat results.  Worst observed: an invalid
   tag (0x37, 0x68) appearing among leaf tags -- a torn 64-bit
   write splitting one thread's tag with another's val.

2. **Stuck APP-of-DP leaves.**  90%+ of threads at V>=4 return
   non-NUM stuck terms (TAG_APP, TAG_SUP, TAG_DP).  Same pathology
   `wl/Examples/sat_collapse/CNF_DIVERGENCE.md` describes for the
   CPU path.  The per-thread wnf state machine inherits the
   limit: it drives APP-LAM/APP-SUP/DUP-XXX but stops when an APP
   frame's body resolves to a non-redex partner (like APP-of-DP).
   CPU `cnf_at` walks deeper, recursively reducing compound
   children; the per-thread wnf doesn't.

## Constraints

- **No 64-bit device atomics on Apple GPU.**  `atomic_uint` works;
  `atomic_ulong` does not (verified at iter Z+1 bring-up).  Term is
  64 bits, so single-instruction atomic Term writes aren't possible.
- **Per-thread budget on private memory.**  Apple GPU is ~32 KiB
  thread-private at typical threadgroup configs.  At 8B per Term
  the per-thread wnf stack at AOT_COLLAPSE_STACK_CAP=1024 is
  already 8 KiB; bigger arenas in private memory aren't viable.
- **Book heap is shared device memory.**  Allocations via
  `aot_book_alloc` (`atomic_fetch_add` on book_next) are per-call
  unique, but writes to existing cells are not synchronized.

## Two viable approaches

### (A) Per-thread book-heap arenas

Pre-allocate N slices of the book heap up front.  Each thread
allocates exclusively within its slice; substitution writes that
target a cell INSIDE the thread's slice are safe (private), writes
to cells OUTSIDE the slice (the shared SUP-tree from iter Z) get
skipped.

Implementation sketch:
- Host pre-allocates `n_threads * thread_arena_size` cells past
  iter Z's book_next; passes `(arena_base_per_thread, arena_size)`
  via constant buffer.
- Kernel-2 uses thread-local `thread_book_next` initialized to
  `arena_base + tid * arena_size`.
- New helper `aot_thread_book_alloc(thread_book_next, n)` bumps
  the local counter and returns `arena_base + tid * arena_size +
  local_offset`.
- `aot_subst_var` and `aot_subst_cop` check `loc >= my_arena_base
  && loc < my_arena_base + arena_size`; write only if true.
- Plumbed via per-call args (`my_base`, `my_size`) -- every IC
  function takes them.

Tradeoffs:
- Substitutions to shared cells get **dropped**, not deferred.  A
  thread firing DUP-LAM on a shared LAM cell can't write the
  binder substitution; downstream reads of `VAR(lam_loc)` see the
  pre-substitution body.  For Church-bool the LAM_ERA_MASK fast
  path covers most cases (binder unused), but LAMs whose binder IS
  referenced (the `(\x.x)` inside `Church AND`) lose binder
  resolution -- thread's reduction stalls earlier.
- Memory cost: `n_threads * arena_size`.  At grid=4096 (V=11) and
  arena=256 cells = 1 MiB per arena pool round.  Fits in current
  BOOK_CAP=4M but doesn't have much headroom.
- No synchronization needed.

### (B) 32-bit slot atomics with split Term encoding

Re-encode the heap as `device atomic_uint *`, two 32-bit slots per
Term.  Term = (upper:32 bits with tag/SUB/upper-ext, lower:32 bits
with val/lower-ext).  SUB bit is bit 31 of the upper slot.

Implementation sketch:
- `aot_heap_load_term(heap, loc) = ((u64)atomic_load(heap[2*loc+1])
  << 32) | atomic_load(heap[2*loc])`
- `aot_subst_var` becomes a CAS loop on the upper slot:
  ```
  loop {
    upper = atomic_load(heap[2*loc + 1]);
    if (upper & SUB_BIT_HIGH) return; // already substituted
    if (CAS(heap[2*loc + 1], upper, upper | SUB_BIT_HIGH)) {
      atomic_store(heap[2*loc + 0], lower);
      return;
    }
  }
  ```
  Reader: `if (atomic_load(heap[2*loc + 1]) & SUB_BIT_HIGH)
  follow_chain;`
- All Term reads in the IC functions use the helper that loads both
  halves; non-atomic reads can stay non-atomic since SUB-bit is
  the synchronization point.

Tradeoffs:
- Heap size doubles (each Term occupies two slots).
- Touch count: every Term I/O in the shader needs the wrapper.
- Race-free: deterministic on every run.
- Compatible with single-thread iter Z if we keep both slot sizes
  (heap-as-Term* for iter Z, heap-as-atomic_uint* for iter Z+1).
  Or convert iter Z to the same 32-bit slot encoding.

## Recommendation

Land **(A) first** as a smaller delta -- the plumbing is contained
to `aot_ic_collapse.metal` plus the host orchestrator.  Accept the
"stalls earlier on Church-bool LAMs with active binders" limit;
document in the iter Z+1 README.  The non-determinism goes away,
which is the more visible bug.

Then iter Z+3 lands **(B)** as a follow-up to recover the lost
correctness on active-binder LAMs.  At that point the per-thread
wnf can perform full sub-bit substitution without races.

Stuck APP-of-DP chains (issue 2 above) are a separate axis that
needs a per-thread cnf-style deep walker -- iter Z+4.  Bounded
re-entry on stuck (tried in this revision and reverted) was too
aggressive: each re-entry allocs new cells and the depth blows
through book heap.  The right fix is structural -- mark already-
walked compound positions and stop, like CPU cnf does.

## Status (as of step 7)

The recommended (A) was upgraded mid-flight to **(A)+per-thread
substitution map** -- instead of dropping shared-cell SUB writes
(which broke active-binder Church-bool LAMs entirely), each
thread caches them in a private map (loc -> term|SUB) consulted
by VAR / DP enter cases before the device heap.  That fix landed
in commit fabf370 (iter Z+2 step 5).

Iter Z+2 step 6 (commit 6530f4e) added the deep walker for
issue 2: APP-of-DP and DP-of-DP redirects push the outer frame
back, descend into the inner DP, and let it resolve.  This is
the structural fix the original recommendation called out for
iter Z+4 -- pulled forward because the per-thread reducer's
arena cap already bounds runaway, so re-entry is safe.

Iter Z+2 step 7 (commit 4c4ec32) found and fixed the actual
non-determinism source: `aot_arena_alloc` returned 0 on overflow
without flagging, so downstream IC fires wrote to heap[0..N]
racing across overflowing threads.  Sticky `overflow` flag now
forces an ERA-sentinel bail before any racing writes.

After step 7: V=2/V=3 fully OK + deterministic.  V>=4 SAT
discriminant degraded vs the (non-deterministic) intermediate
runs because the deterministic version no longer gets lucky
garbage-NUM hits from arena overflow races.  Recovering V>=4
requires iter Z+3 (option (B) in this doc) so threads can SHARE
substitutions via heap atomics instead of each thread redoing
the entire reduction in its own arena.

Bumping arena cap from 1024 -> 8192 was tried (between steps 7
and 8) and made zero difference -- the per-thread redirect
cascade exhausts even 8K cells per thread.  The bottleneck is
algorithmic (per-thread duplicate work), not arena sizing.

## Why iter Z+3 (option B) is also the wrong direction for SAT

Tried and reverted.  Implementation: re-cast heap as `device
atomic_uint *`, two 32-bit slots per Term, LOCK + SUB bits in
the high slot, 2-stage publish via CAS.  Built and ran clean.

But the architecture is fundamentally wrong for SAT: each
collapse thread takes a DIFFERENT path through the SUP-tree
and applies the formula's shared LAMs (AND / OR / NOT
combinators) with DIFFERENT arg values per path.  e.g., the
shared AND-combinator LAM gets app_lam'd with arg=T_for_x1 by
one thread and arg=F_for_x1 by another.  Each thread NEEDS its
own substitution on that LAM cell.

Iter Z+3's heap-shared SUB lets only one thread's arg propagate
to all threads -- semantically wrong for SAT.  Empirically
visible: V=2/V=3 NUM count dropped from 128/256 (step 7
deterministic) to 60-90 (iter Z+3 with both lock-on-give-up
and lock-on-spin variants), and the SAT discriminant degraded
across V=4..7.

The per-thread substitution map (step 5) was always the right
model for SAT.  Determinism via step 7's overflow guard is the
ceiling for the SUP-tree-walk approach.

## What actually works for SAT on Metal at scale: per-leaf curried-LAM

Different architecture entirely.  The SUP-tree machinery
(kernel-1 builds it, kernel-2 walks it) was a generalization for
arbitrary IC reductions, but SAT has a much simpler structure:
each leaf is a formula evaluation with concrete variable values.

New plan -- post iter Z+2 step 7:

1. TDef body: instead of `TLam[ig, boolToNum[formulaWithSUPs]]`
   (vars baked as `TSup[label, T_, F_]`), use a fully curried
   form `TLam[x1, TLam[x2, ..., TLam[xN, boolToNum[formula]]]]`
   where the formula references the binders via VAR.  No SUPs,
   no DUPs.

2. Per-leaf dispatch: for tid in 0..2^N-1, derive the
   assignment from tid bits (bit i picks T or F for var i).
   Pass the assignments as args[0..N-1] to a single kernel
   dispatched with grid=N.

3. Each thread:
   - Builds APP-chain `APP(...APP(def_root, args[0])..., args[N-1])`
     in its private arena.
   - Runs the iter Z wnf state machine.  All reductions are
     APP-LAM (vars are concrete T_/F_ LAMs, no DUP fires).
   - Returns the NUM result via result[tid].

4. Host scans result[] for NUM=1.  SAT True iff any.

Why this scales:
- Per-thread cost: O(formula_size) APP-LAM applications.
  No DUP-SUP commutes, no DUP-LAM cell allocations, no
  redirect cascade.  Arena pressure drops by 100x+ vs the
  SUP-tree-walk approach.
- No shared state during reduction -- each thread fully
  private.  No race conditions, fully deterministic across
  runs without relying on atomics.
- Scales to V=20 (1M threads) because per-thread arena needs
  only the APP-chain's own cells (~3*N cells = ~60 cells for
  V=20).  At BOOK_CAP=4M and N=2^20, per-thread budget = 4
  cells -- tight but workable; larger BOOK_CAP needed for
  V>=22.

This is the next iter (Z+4 or Z+5; numbering deferred).
Implementation: extends `aot_ic_def_run.metal` to dispatch with
grid=N and read per-thread args from a [N, n_args] buffer.
Host wrapper builds the args matrix from a CNF + var count.
WL surface: `TAOTSatEval[name, nVars]` returns the boolean
SAT answer.

## Critical files

- `src/backend/metal/shaders/aot_ic_collapse.metal` -- the parallel
  collapse kernel; iter Z+2 changes go here.
- `src/backend/metal/_.m` -- host orchestrator
  `thvm_aot_metal_ic_collapse`; (A) needs new arena-base setup
  before dispatch, (B) needs heap re-typing.
- `bench/sat_collapse/profile_aot_metal.wls` -- regression bench;
  run before/after each fix to confirm determinism + correctness
  improvements.
- `wl/Examples/sat_collapse/v3_aot_metal.wls` -- end-to-end SAT
  example exercising the full chain.

## Verification

Both approaches:
1. `wolframscript -f bench/sat_collapse/profile_aot_metal.wls`
   3-5 times in a row -- num counts must be identical across runs.
2. `make test` + `aot_metal.wlt` -- iter Z + iter Z+1 unit tests
   stay green.
3. Curated SAT v1 testCases (sat-3var / unsat-3var / etc.)
   match WL via path C.
