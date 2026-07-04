# ATP cross-run state leak: a heavy TFindProof run poisons the next run

Status: OPEN, root cause identified (leading hypothesis, one confirmation check
left). Filed 2026-07-04.

Running two `TFindProof` proofs in one kernel session should be correct and, if
anything, faster than a fresh kernel per proof (warm library, reusable state).
Instead a heavy first run breaks a subsequent run. This document identifies the
cause for a fixing agent; see "Connection to the heap-overhead refactor" at the
end for why the planned refactor subsumes the fix.

## Symptom (minimal reproduction)

```wolfram
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
ax   = AxiomaticTheory["GroupAxioms"];
goal = AxiomaticTheory["GroupAxioms", "NotableTheorems"]["InverseOfInverse"];

TFindProof[goal, ax, Method -> Automatic]       (* ProofObject, 13 steps, 0.02 s *)
TFindProof[goal, ax, Method -> "Waldmeister"]   (* HANGS -- never returns *)
```

The identical `Method -> "Waldmeister"` call solves this problem in **0.02 s /
13 steps** in a fresh kernel (`ProofFunction` verifies). Only the second call in
the same session hangs.

This artifact silently corrupted a benchmark: running Automatic, Waldmeister,
and `FindEquationalProof` in one kernel produced fake Waldmeister "timeouts" and
even shifted `FindEquationalProof`'s reported proof length (8 vs 13 for the same
input). Benchmarks MUST use one method per fresh kernel until this is fixed.

## Confirmed characterization (empirical)

- Order matters, not repetition. `Automatic` alone repeated -> fine.
  `Waldmeister` alone repeated -> fine. `Waldmeister` after `Automatic` -> hangs.
  Confirmed both directions: WM, WM, Automatic, WM prints ProofObject, ProofObject,
  ProofObject, then hangs on the post-Automatic WM. So a heavier run (Automatic
  is a portfolio) poisons a following Waldmeister run.
- `TReset[]` or `TInit[]` between the two calls FIXES it.
- The ATP heap-recycle called manually between them does NOT fix it:
  `WolframInstitute`THVMLink`Private`$atpHeapRecycleFn[]` (i.e.
  `thvm_wl_atp_heap_recycle`) leaves the hang in place.
- NOT a heap-size problem. Forcing a 4 GiB heap for the whole process
  (`THVM_HEAP_CELLS=536870912 wolframscript ...`) still hangs.

`TReset[]` (`wl/THVMLink/Kernel/THVMLink.wl`) is `$labelCounters[...] = 1;
ensureInit[]; $resetFn[]`, where `$resetFn[]` is the C `thvm_reset()` which
"rewinds the dyn heap AND drops every cache" (`wl/THVMLink/CSource/thvmlink.c`
comment). The heap-recycle only rewinds; it does not drop the caches. That gap
is the bug.

## Root cause (leading hypothesis, from the code)

The ATP heap-recycle uses a raw absolute `HEAP_NEXT` snapshot that goes stale
across a garbage collection (GC):

- `wl/THVMLink/CSource/thvmlink.c:299-309` -- `thvm_wl_atp_heap_recycle`: the
  FIRST outermost run saves `g_atp_heap_base = HEAP_NEXT`; every later run calls
  `thvm_atp_heap_reset(g_atp_heap_base)`.
- `src/atp/_.c:6620` -- `thvm_atp_heap_reset(checkpoint)`: pops the bump pointer
  ONLY when `checkpoint <= HEAP_NEXT`, and is a **"silent no-op on out-of-range"**
  otherwise. On a real pop it calls `thvm_kbo_invalidate()` to bump the epoch of
  the Knuth-Bendix-ordering (KBO) weight memo `g_kbo_wmemo`, which is keyed by
  `(epoch, Term loc)`. Its own comment states a GC relocation breaks the
  one-loc-per-logical-term invariant, "which is why `thvm_atp_gc_collect`
  invalidates the memo."
- `wl/THVMLink/Kernel/ATP/ATP.wl:4461` -- `atpHeapRecycleOuter[] := If[ !
  TrueQ[$atpInRun], $atpHeapRecycleFn[]]`, called at the start of the outermost
  `atpProveBundle` (`ATP.wl:4522`); the `$atpInRun` guard makes only the
  outermost run recycle, so a portfolio's inner runs keep siblings' live terms.

The chain:

1. `Automatic` runs a portfolio (heavier), which trips the Cheney semi-space
   limit and triggers a GC. The GC relocates cells and moves `HEAP_NEXT`, so the
   absolute `g_atp_heap_base` saved before it is now stale (points past the
   post-GC `HEAP_NEXT`, i.e. `g_atp_heap_base > HEAP_NEXT`).
2. The following `Waldmeister` run's recycle calls
   `thvm_atp_heap_reset(g_atp_heap_base)` with `g_atp_heap_base > HEAP_NEXT` ->
   the out-of-range branch -> **silent no-op**: no pop, and crucially no
   `thvm_kbo_invalidate()`.
3. So Waldmeister inherits `Automatic`'s stale KBO weight memo (old
   loc -> weight entries for locs that now hold different terms). Wrong KBO
   verdicts flip order-gated rewrites (the failure mode the reset comment
   describes: "the flatterm mixed path's unorientable order-gate firing a
   non-decreasing step, diverging from the tree NF"), so the completion never
   terminates -> the observed hang.

`TReset` works because `thvm_reset()` fully rewinds and drops every cache
(including `g_kbo_wmemo`), sidestepping the stale base entirely. The manual
heap-recycle does not, because it hits the same out-of-range no-op.

### One check to confirm before fixing

Instrument `thvm_wl_atp_heap_recycle` (or `thvm_atp_heap_reset`) to log
`g_atp_heap_base` vs `HEAP_NEXT` on the post-Automatic Waldmeister call. Expect
`g_atp_heap_base > HEAP_NEXT` (the no-op branch taken) and the KBO memo epoch
unchanged from Automatic's run. If instead the base is in range and the pop
fires, the poison is a different cache the pop leaves intact (candidates: the
feature-vector subsumption index state, or another `(epoch, loc)`-keyed memo) --
same class of bug, different specific cache.

## Fix directions (targeted)

Any of these makes the outermost-run reset GC-safe:

- Unconditionally `thvm_kbo_invalidate()` (and drop the other ATP loc-keyed
  caches) on each outermost recycle, regardless of whether the pop fires. Cheap
  and directly closes the observed hole.
- Have `thvm_atp_gc_collect` reset `g_atp_heap_base_set = 0` so the next
  outermost run re-checkpoints against the post-GC layout.
- Store the recycle checkpoint as a GC-relocatable root instead of a raw
  `HEAP_NEXT` integer, so it survives relocation.

## Connection to the heap-overhead refactor

This bug is a direct symptom of ATP sharing the garbage-collected Cheney
semi-space heap and threading run state (the KBO weight memo, the feature-vector
index, the `g_atp_heap_base` checkpoint, the `$atpInRun` recycle guard, the
`atpEnsureWmHeap` size seeding) through it. That machinery IS the heap overhead
the refactor targets.

If ATP instead allocated into a dedicated bump arena reset trivially at the start
of each outermost run -- no shared GC, no relocation, no loc-keyed memo epoch
dance, no absolute-checkpoint bookkeeping -- then both the overhead and this
entire class of stale-checkpoint / stale-memo bugs disappear, and multiple
proofs per session become correct by construction (and faster, since each run
starts from a clean arena with no GC pressure from prior runs). The targeted
fixes above are a stopgap until that refactor lands.
