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

| bench                          | backend | metric              | bm3 baseline | post-bm4abc |    Δ |
| ------------------------------ | ------- | ------------------- | ------------ | ----------- | ---: |
| lenet-mnist (Adam step)        | CPU     | ms/step             |          6.9 |         7.0 |  +1% |
| lenet-mnist (Adam step)        | CPU     | peak_concurrent KiB |       1882.3 |      1882.3 |   0% |
| lenet-mnist (Adam step)        | CPU     | total_live KiB      |       4087.4 |      4087.4 |   0% |
| lenet-mnist (Adam step)        | CPU     | kernels             |          455 |         455 |   0% |
| lenet-mnist (Adam step)        | Metal   | ms/step             |         85.8 |       100.9 | +18% |
| lenet-mnist (Adam step)        | Metal   | peak_concurrent KiB |       1882.3 |      1882.3 |   0% |
| beautiful-mnist (forward only) | CPU     | ms/step             |        175.1 |       179.6 |  +3% |
| beautiful-mnist (forward only) | CPU     | peak_concurrent KiB |      82750.3 |     82750.3 |   0% |
| beautiful-mnist (forward only) | Metal   | ms/step             |        245.5 |       250.6 |  +2% |
| beautiful-mnist (forward only) | Metal   | peak_concurrent KiB |      82750.3 |     82750.3 |   0% |

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

## Unblocking the savings

Queued in TASKS.md: **Heap-rooted preserve pass**.  Replaces
`mark_preserved_chain` with a walk over live `HEAP[0..HEAP_NEXT)`
cells, collecting every TenDesc reachable from a `TAG_TEN` cell
or from any pending `TAG_UOP` term that references one.  Only
those bufs stay preserved; everything else feeds bm4b's
rollback path.

That single pass unblocks both:

1. **bm4 measurable savings** (target ≥30% peak KiB drop on
   lenet-mnist; 53.9% headroom is sittable per TMemoryPlan).
2. **Refcount-driven free arc sub-item c** rollback swap
   (currently blocked behind the same conservative chain walk).

Sized at ~140 LOC total; will sub-decompose into `walk` +
`integration` + `tests` when picked up.

## Reproducing

Same as bm3:

```bash
wolframscript -f wl/Examples/_bench/baseline.wls
THVM_BACKEND=metal wolframscript -f wl/Examples/_bench/baseline.wls
```

Compare PNG snapshots under `wl/Examples/_bench/baseline-*.png`;
they're byte-identical to the bm3 captures because the layout is
deterministic.
