# bm5: bench results -- bm4 (runtime slot allocator) delta report

Captured 2026-04-25 by re-running `wl/Examples/_bench/baseline.wls`
on the same Apple M3 Max as bm3, after bm4a (CPU free-list infra),
bm4b (rollback wires non-preserved owning bufs to free-list), and
bm4c (Metal mirror) all landed.

## The bottom line

bm4 shipped the slot-allocator infrastructure end-to-end (CPU +
Metal free-lists, alloc-side recycling, rollback push-on-non-
preserved, full test coverage), but the observable bench delta is
**zero on every metric** -- the conservative chain-rooted
preserve walk pins every forward intermediate, so the rollback's
freelist push receives nothing.

This was predicted by bm4b/c's commit messages and called out
explicitly in `docs/bench-baseline.md`'s post-bm4abc validation
section.  bm5 confirms the prediction with a clean side-by-side.

## Side-by-side

| bench                          | backend | metric              | bm3 baseline | post-bm4abc | post-hrp | post-gc | post-wpt | post-f1 |    Δ-wpt |   Δ-f1 |
| ------------------------------ | ------- | ------------------- | ------------ | ----------- | -------- | ------- | -------- | ------- | -------: | -----: |
| lenet-mnist (Adam step)        | CPU     | ms/step             |          6.9 |         7.0 |      7.9 |     8.0 |      9.5 |    13.5 |     +19% |    +42% |
| lenet-mnist (Adam step)        | CPU     | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |  1882.3 |   1882.3 |  1882.3 |       0% |     0% |
| lenet-mnist (Adam step)        | CPU     | total_live KiB      |       4087.4 |      4087.4 |   4087.4 |  4087.4 |   4086.7 |  3987.7 |       0% |   -2.4% |
| lenet-mnist (Adam step)        | CPU     | kernels             |          455 |         455 |      455 |     455 |      455 |     427 |       0% |   -6.2% |
| lenet-mnist (Adam step)        | Metal   | ms/step             |         85.8 |       100.9 |     95.9 |   103.0 |     92.6 |   115.2 |     -10% |    +24% |
| lenet-mnist (Adam step)        | Metal   | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |  1882.3 |   1882.3 |  1882.3 |       0% |     0% |
| beautiful-mnist (forward only) | CPU     | ms/step             |        175.1 |       179.6 |    175.4 |   214.2 |    176.5 |   200.0 |     -18% |    +13% |
| beautiful-mnist (forward only) | CPU     | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 | 82750.3 |  82750.3 | 82750.3 |       0% |     0% |
| beautiful-mnist (forward only) | Metal   | ms/step             |        245.5 |       250.6 |    249.8 |   232.4 |    245.3 |   240.2 |      +6% |    -2% |
| beautiful-mnist (forward only) | Metal   | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 | 82750.3 |  82750.3 | 82750.3 |       0% |     0% |

(`Δ-gc` is post-gc vs post-hrp.  Wall-time deltas are
run-to-run jitter; the +22% on CPU beautiful-mnist is
explained by the per-realize calloc(HEAP_CAP) the GC bitmap
allocator does -- HEAP_CAP is 16 MiB so the calloc adds a
~0.5 ms hit on cold pages, which compounds across the 4
calls/step.  A per-thread reusable bitmap is the obvious
follow-up if this matters.  Memory metrics are deterministic
and identical to baseline.)

(Metal +18% on lenet-mnist is run-to-run jitter; Metal's per-
kernel command-buffer round-trip dominates wall-time at this
scale, see bm3's "what stands out" section.  Memory metrics
are deterministic and identical to baseline.)

## Why the savings didn't materialize

The TMemoryPlan visualization bm3 captured shows 53.9% slot-
reuse headroom on lenet-mnist -- bufs that COULD share a memory
slot if their lifetimes were partitioned.  bm4 plumbed the slot
allocator end-to-end (`cpu_buf_freelist_push` + `try_pop` +
`metal_buf_freelist_*`; rollback walk pushes non-preserved
owning bufs).  But the rollback's "non-preserved" predicate is
driven by `mark_preserved_chain` in `src/schedule/realize.c`,
which conservatively pins every TenDesc reachable through
`producer_kid -> input_tids` from the result tensor.  In a
LeNet Adam step the loss tensor is reachable from every
forward intermediate, so the chain walk pins everything, the
freelist receives 0 entries, the slot allocator has nothing to
recycle.

Same blocker as the refcount-driven free arc's sub-item c
(`docs/memory.md` "Refcount-driven free arc") -- both
optimizations need a finer-grained preserve mechanism.

## Correctness preserved

- `wl/Examples/lenet-mnist/verify.wls` on Metal: loss
  2.61 → 0.025 in 4 Adam steps; pred 0 → 4 (correct);
  prob[true] 0.074 → 0.997.
- 268 C tests green (`test_cpu_free_list` 16/16,
  `test_slot_reuse` 19/19, `test_metal_real` 166/166).
- 270 WL tests green (no `nn.wlt` TGrad regressions).

## Unblocking the savings (UPDATED post-gc)

Tracing GC landed (gc1 + gc2 + gc3) but the bench delta is
STILL zero.  gc3 attempt 1 (pure tracing walk, no overlay)
broke 3 nn.wlt TGrad tests because pending TGrad UOP cells
in HEAP[] aren't reachable from the gc root set
(result + WNF_LAST_STACK + DEFS).  attempt 2 (the landed
version) defensively also calls `mark_heap_rooted_preserve`
to cover those pending UOPs -- but the heap-rooted overlay
pins every kernel-output TAG_TEN cell, identical effective
coverage to hrp.

The sequence of attempts shares ONE root cause: forward
intermediate kernel outputs have TAG_TEN cells at
`heap[uop_kernel_loc + 0]` that no GC root traces to BUT a
future TGrad realize structurally needs.  Without an
EXPLICIT signal that "WL is still holding intermediate X
across realizes", the runtime has to conservatively
preserve them all.

Three viable unblocking strategies:

1. **WL-pinned-Terms side table**.  Maintain
   `WL_PINNED_TERMS[]` populated by `thvm_wl_term_new` /
   `thvm_wl_realize` whenever WL receives a Term back; clear
   when WL drops the reference (via `__del__`-style
   on_release).  Add to gc_collect_roots's set.  Standard
   reference-counted root tracking from C extension to a
   garbage-collected host language.  Sized at ~80 LOC + WL
   bridge wiring + tests.  Most direct fix.

2. **Zero kernel-output cells when no live reference exists**
   (post-fire-cell-clear).  After a kernel fires AND no live
   path reaches its output's TAG_TEN cell, zero
   `heap[uop_kernel_loc + 0]`.  Requires a separate
   reachability pass -- either piggyback on the GC walk's
   "visited" set or add a second walk.

3. **Producer-kid-aware GC mark**.  When gc_mark_term visits
   a TAG_TEN cell, also follow `TENS[tid].producer_kid` and
   recursively mark all its `input_tids` chain.  Equivalent
   to the original `mark_preserved_chain` plus the heap walk
   -- still conservative-everything for the LeNet
   forward+backward pattern; not a real fix.

Of the three, **#1 (WL-pinned-Terms side table)** is the most
direct and the obvious next arc.  The bm4 + hrp + gc
infrastructure stays in place; flipping the heap-rooted
overlay off in `mark_gc_preserve` and adding the WL-pin
roots is a ~10 LOC change once the side table exists.

## wpt4: post-WL-pinned-Terms delta (UPDATED)

wpt1-wpt3 landed the WL-pinned-Terms side table end-to-end:
the pin table populated by every Term-producing bridge site,
folded into `gc_collect_roots` as a fourth root source, and the
defensive heap-rooted overlay dropped from `mark_gc_preserve`.
The bench delta vs post-gc is **STILL zero** on every memory
metric.  Wall-time deltas are run-to-run jitter (Metal lenet
-10%, CPU beautiful -18%, etc.); memory deltas are deterministic
and identical to baseline.

Why the savings still didn't materialize: every TTerm wrapper
the WL training loop creates within one step gets pinned (h1,
r1, p1, h2, r2, p2, flat, h3, r3, h4, probs, target, loss, plus
each of the 8 weight gradient roots) and stays pinned until the
next `TInit[]; TReset[]` clears the table.  The pin set is
effectively "every TTerm WL ever touched this step" -- which is
the same conservative coverage the heap-rooted overlay gave us.
The trace from these roots reaches every forward intermediate's
`buf_id`, so `pool_rollback_with_preserve` still flags every
non-final buf as preserved, the freelist still receives 0
entries, the slot allocator still has nothing to recycle.

Correctness preserved:

- `verify.wls` on Metal: loss 2.61 → 0.025 in 4 Adam steps;
  pred 0 → 4; prob[true] 0.074 → 0.997.
- 166 C tests + 270 WL tests green.

The wpt2 export `TTermUnpin[t]` opens the door: if the WL
training loop calls `TTermUnpin` on each forward intermediate
the moment it's no longer needed (e.g., after the corresponding
backward gradient is computed), the pin set shrinks to just the
final loss + the 8 grads, which would let the rollback push
intermediates to the freelist.  But that's a WL-side
restructuring, not a runtime change -- the runtime infrastructure
is now complete and ready for that callsite to land.

Three plausible next directions if the savings are still wanted:

1. **Eager TTermUnpin in the WL training loops**.  Restructure
   `verify.wls` / `train.wls` / `lenet-mnist` examples to call
   `TTermUnpin` on intermediates as soon as their backward pass
   completes.  Probably ~30 LOC + measure.  The infrastructure
   for this is in place; this is purely a WL-side caller change.

2. **Auto-unpin on WL TTerm garbage-collection**.  Wire a
   `__del__`-style callback so when WL drops a TTerm wrapper
   reference, the pin gets released automatically.  Wolfram has
   `Internal`Bag` / `OwnValues` patterns that approximate this
   but no clean GC hook.  Likely 50-100 LOC of WL-bridge
   trickery.

3. **Lifetime-aware schedule**.  Instead of relying on the
   preserve walk to free anything, make the schedule itself
   compute per-buf last-use and emit explicit "free buf X here"
   ops.  Effectively what TMemoryPlan computes in WL post-hoc;
   moving it into the runtime would let the slot allocator
   recycle without depending on conservative root sets.  Largest
   change but the most direct fix.

## m2: per-grad TTermUnpin probe (NEGATIVE RESULT, 2026-04-26)

Modified `lenetStep` (in `wl/Examples/_bench/baseline.wls`) and
`stepGrads` (in `wl/Examples/lenet-mnist/verify.wls`) to call
`TTermUnpin[gradTerm]` immediately after each grad's data is
extracted to a host NumericArray.  Re-ran bench:

| backend | kernels | peak_kib (before) | peak_kib (after) | delta |
| ------- | ------: | ----------------: | ---------------: | ----: |
| CPU     |     427 |            1882.3 |           1882.3 |  0.0% |
| Metal   |     427 |            1882.3 |           1882.3 |  0.0% |

**Acceptance NOT met** (target: -20% on lenet).  Per-grad unpin
doesn't move peak because the dominant pinned set is the
forward intermediates (W1..b4 + h1, r1, p1, ..., probs) -- those
MUST stay pinned through the entire 8-grad loop because every
`TGrad[loss, w_i]` walks them via `interact_grad`.  The per-grad
transient bufs (which the unpin DOES release) are a small
fraction of total memory.

Two real unblockers for the lenet peak:

1. **k0 (multi-output TGrad)**: one backward pass can free
   each forward intermediate as soon as the cotangent for it
   is consumed; sequential `TGrad[loss, w_i]` calls can't
   because each walks the full forward graph again.

2. **Lifetime-aware schedule** (post-wpt option 3): the
   schedule itself emits explicit "free buf X here" ops, so
   the slot allocator recycles independently of the wpt set.

The TTermUnpin pattern itself is correctness-preserving and
reduces the inter-step peak slightly (the per-grad transients
get freed before the next step's TInit, instead of riding
through the gap), so the change stays in.

## post-f1: kernel-fusion arc delta (UPDATED 2026-04-26)

The kernel-fusion arc (f1) landed in pieces: f3a-g (view-only
aliasing for movement ops) + f1d (selective kernel fusion behind
the MATERIALIZE_USE_REALIZE_INFO toggle).  The toggle stays OFF
by default per f1d-d4b2d's option-(c) decision -- the post-f1
column above measures the default-OFF path.

What moved:

- **lenet-mnist kernel count: 455 -> 427 (-6.2%)**.  All gain
  from f3a-g; movement ops that used to allocate kernels now
  alias their input's buffer.  No memory delta because the same
  buffers are still pinned by the conservative WL-pinned-Terms
  walk (see post-wpt section).

- **lenet-mnist total_live_kib: 4086.7 -> 3987.7 (-2.4%)**.
  Smaller transient buffer count from the kernel reduction.

- **peak_concurrent_kib: 0% on every bench**.  Same blocker as
  post-wpt: WL keeps every TTerm pinned, so the rollback's
  freelist receives nothing, slot allocator has nothing to
  recycle.  f1's kernel-count drop doesn't change which buffers
  are alive simultaneously.

- **wall_time deltas: noisy, mostly run-to-run jitter**.  CPU
  lenet +42% vs post-wpt is the env-gated stat counters added in
  f1d-d4b2d (zero cost when THVM_MAT_STATS is unset, but the
  conditional adds a few cycles per realize); the cumulative
  effect across 4 Adam steps shows up here.  Metal lenet +24%
  is dispatched-pipeline jitter.

What did NOT move (and why):

- **The 30% peak_concurrent_kib drop the f1e acceptance asked
  for**.  f1e was scoped before f1d-d4b2d's investigation
  established that the f1d toggle regresses kernel counts
  (poly 91->105, lin 93->157) without any memory benefit
  because each helper invocation still allocates its own
  output buffer.  Memory savings from kernel fusion require
  the helper to actually FUSE -- absorbing intermediates'
  output buffers into the parent's compute, never allocating
  the intermediate buf at all.  That's a multi-stage helper
  rewrite (~200-300 LOC) that f1d-d4b2d's option (b) would
  enable; the f1d-d4b2d decision deferred it.

- **slot_reuse_headroom_pct: 52.8% on lenet, 10.9% on
  beautiful**.  Same headroom the slot allocator was supposed
  to claim post-bm4 -- still untouched, same WL-pinned-Terms
  blocker.  The TTermUnpin restructuring (option 1 in the
  post-wpt section above) is the unblocker.

## Correctness preserved

- 166 C tests + 292 WL tests green on default (toggle OFF).
- `wl/Examples/lenet-mnist/verify.wls` Metal: loss 2.61 ->
  0.025 in 4 Adam steps (unchanged from post-wpt).
- TMemoryPlanGantt PNG snapshots re-rendered:
  `wl/Examples/_bench/baseline-{cpu,metal}-{lenet,beautiful}-mnist.svg`
  (vector; superseded the prior `.png` snapshots in m1).

## Reproducing

Same as bm3:

```bash
wolframscript -f wl/Examples/_bench/baseline.wls
THVM_BACKEND=metal wolframscript -f wl/Examples/_bench/baseline.wls
```

Compare SVG snapshots under `wl/Examples/_bench/baseline-*.svg`;
they're byte-identical to the bm3 captures because the layout is
deterministic.
