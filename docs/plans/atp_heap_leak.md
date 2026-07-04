# ATP cross-run state leak: a heavy TFindProof run poisons the next run

Status: OPEN. Root cause REVISED 2026-07-04 (the original stale-KBO-memo
hypothesis below is DISPROVED by measurement). Filed 2026-07-04.

## CORRECTION (2026-07-04): the stale-KBO-memo hypothesis is wrong; it is the persisted GC semi-space swap

The "Root cause (leading hypothesis)" section further down is disproved:

- `thvm_atp_heap_reset`'s out-of-range no-op branch is UNREACHABLE on a fresh
  kernel: the first recycle captures `g_atp_heap_base = HEAP_NEXT` when
  `HEAP_NEXT = 0`, so `base = 0` and `base > HEAP_NEXT` never holds
  (instrumented `THVM_ATP_RECYCLE_DEBUG`: run1 `branch=init` then `branch=pop`;
  run2 does not call recycle at all -- a separate `$atpInRun`/bundle anomaly,
  ATP.wl:4461/4522).
- `thvm_atp_init` ALREADY invalidates the KBO weight memo (`_.c:6306`), plus
  LPO/orient/unf, so a stale KBO memo cannot survive into run2 regardless.
- Adding FtNfm invalidation to init (the one ordering memo it missed) does NOT
  fix the hang either (landed 3622b82a as a latent-gap fix, not this one).

MEASURED root cause:

1. `TReset[]` fixes it (run2 proves in 0.011 s); `init` alone does not.
2. run2 runs a DIVERGENT completion: `[atp_run] step=27466 n_rules=410
   n_cps=139118` and climbing, vs 13 steps fresh -- eventually `$Failed` on
   budget at ~143 s. So the poison corrupts the REDUCTION ORDER (rules stop
   simplifying -> critical-pair explosion), it is not GC thrashing.
3. `thvm_reset`'s ONLY ATP-relevant action is `gc_init` (heap/collect.c:91),
   which resets `GC_FROM_START = 0` and `HEAP_NEXT = 0` -- the canonical
   semi-space layout. `init` and the heap-recycle only ever MOVE `HEAP_NEXT`;
   neither re-inits the collector.
4. The ATP in-loop GC (`thvm_atp_gc_collect`, `_.c:6679`) SWAPS the two
   semi-spaces on every collect (heap/collect.c:436-440), and fires on its own
   `THVM_ATP_GC_MB` threshold regardless of total heap size (so a 4 GiB heap
   still hangs -- consistent with the doc's own observation). After an odd
   number of Automatic-run collects the active from-space is the UPPER half,
   and that state persists into run2 because nothing resets it.

Leading mechanism: run2 inherits Automatic's swapped/inconsistent semi-space,
so its allocations and its own GC operate against a from/to layout that no
longer matches where terms live -> corrupted term relocation/reads -> wrong KBO
verdicts -> the divergent search. `gc_init` restores the canonical layout,
which is why only `TReset` fixes it.

REMAINING FIX (open): a semi-space-consistent reset at each OUTERMOST ATP run
that is mixed-session safe (the recycle must still preserve non-ATP heap below
`g_atp_heap_base` -- a bare `gc_init` would nuke tensors). Candidates: compact
live sub-base heap into the canonical from-space; or make `g_atp_heap_base` a
GC-relocatable root (doc "option 3" below) AND reset the swap; or the arena
refactor (which subsumes the whole class -- see the end). Also fix the run2
recycle-skip. NOTE: the confirming semi-space probe must be added to
`thvm_wl_atp_run_proof` (the `wire_live` path TFindProof actually uses), NOT the
legacy `wl_kbo2` entry.

---
## Original hypothesis (DISPROVED -- retained for the reasoning trail)


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
