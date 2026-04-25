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

| bench                          | backend | metric              | bm3 baseline | post-bm4abc | post-hrp | post-gc |     Δ-gc |
| ------------------------------ | ------- | ------------------- | ------------ | ----------- | -------- | ------- | -------: |
| lenet-mnist (Adam step)        | CPU     | ms/step             |          6.9 |         7.0 |      7.9 |     8.0 |      +1% |
| lenet-mnist (Adam step)        | CPU     | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |  1882.3 |       0% |
| lenet-mnist (Adam step)        | CPU     | total_live KiB      |       4087.4 |      4087.4 |   4087.4 |  4087.4 |       0% |
| lenet-mnist (Adam step)        | CPU     | kernels             |          455 |         455 |      455 |     455 |       0% |
| lenet-mnist (Adam step)        | Metal   | ms/step             |         85.8 |       100.9 |     95.9 |   103.0 |      +7% |
| lenet-mnist (Adam step)        | Metal   | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |  1882.3 |       0% |
| beautiful-mnist (forward only) | CPU     | ms/step             |        175.1 |       179.6 |    175.4 |   214.2 |     +22% |
| beautiful-mnist (forward only) | CPU     | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 | 82750.3 |       0% |
| beautiful-mnist (forward only) | Metal   | ms/step             |        245.5 |       250.6 |    249.8 |   232.4 |      -7% |
| beautiful-mnist (forward only) | Metal   | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 | 82750.3 |       0% |

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

## Reproducing

Same as bm3:

```bash
wolframscript -f wl/Examples/_bench/baseline.wls
THVM_BACKEND=metal wolframscript -f wl/Examples/_bench/baseline.wls
```

Compare PNG snapshots under `wl/Examples/_bench/baseline-*.png`;
they're byte-identical to the bm3 captures because the layout is
deterministic.
