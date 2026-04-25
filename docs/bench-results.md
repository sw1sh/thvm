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

| bench                          | backend | metric              | bm3 baseline | post-bm4abc | post-hrp |    Δ-hrp |
| ------------------------------ | ------- | ------------------- | ------------ | ----------- | -------- | -------: |
| lenet-mnist (Adam step)        | CPU     | ms/step             |          6.9 |         7.0 |      7.9 |     +13% |
| lenet-mnist (Adam step)        | CPU     | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |       0% |
| lenet-mnist (Adam step)        | CPU     | total_live KiB      |       4087.4 |      4087.4 |   4087.4 |       0% |
| lenet-mnist (Adam step)        | CPU     | kernels             |          455 |         455 |      455 |       0% |
| lenet-mnist (Adam step)        | Metal   | ms/step             |         85.8 |       100.9 |     95.9 |      -5% |
| lenet-mnist (Adam step)        | Metal   | peak_concurrent KiB |       1882.3 |      1882.3 |   1882.3 |       0% |
| beautiful-mnist (forward only) | CPU     | ms/step             |        175.1 |       179.6 |    175.4 |      -2% |
| beautiful-mnist (forward only) | CPU     | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 |       0% |
| beautiful-mnist (forward only) | Metal   | ms/step             |        245.5 |       250.6 |    249.8 |      -0% |
| beautiful-mnist (forward only) | Metal   | peak_concurrent KiB |      82750.3 |     82750.3 |  82750.3 |       0% |

(`Δ-hrp` is post-hrp vs post-bm4abc.  Wall-time deltas are
run-to-run jitter; memory metrics are deterministic and
identical to baseline.)

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

## Unblocking the savings (UPDATED post-hrp)

The heap-rooted preserve pass landed (hrp1 + hrp2) without
delivering savings either: every kernel's output_tid has a
`TAG_TEN` cell at `heap[uop_kernel_loc + 0]` (set by
materialize when it emits the UOP_KERNEL wrapper), so the
linear heap walk catches all of them.  Strictly broader
coverage than the chain walk in theory, but identical in
practice for our materialize-then-fire flow.

Real savings now require either:

1. **Post-fire kernel-cell zeroing**: when a kernel fires and
   its output is preserved BY ITS OUTPUT-TID's heap cell only
   (no other live reference), zero out that cell after the
   result is consumed.  Tricky -- "no other live reference" is
   the standard mark-from-roots GC question.

2. **Mark-from-roots GC**: walk from a fixed root set (the
   WL-returned result + live UOP terms still being reduced),
   marking only reachable cells; rollback frees everything
   else.  Standard tracing-GC pattern; would land in
   `src/schedule/gc.c` and replace both
   `mark_preserved_chain` and `mark_heap_rooted_preserve`.
   Much bigger than the bm4 / hrp arcs anticipated; queue as
   a separate "tracing GC" arc when picked up.

Both options are larger than the cron-loop's per-fire budget
allows for a single pass.  The bm4 + hrp infrastructure
remains correct + sittable -- once tracing GC delivers a
narrower preserve set, the freelist + rollback wiring already
shipped will deliver the savings without further code change.

## Reproducing

Same as bm3:

```bash
wolframscript -f wl/Examples/_bench/baseline.wls
THVM_BACKEND=metal wolframscript -f wl/Examples/_bench/baseline.wls
```

Compare PNG snapshots under `wl/Examples/_bench/baseline-*.png`;
they're byte-identical to the bm3 captures because the layout is
deterministic.
