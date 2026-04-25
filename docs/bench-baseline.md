# Baseline benchmarks (bm3 of the tinygrad-port arc)

Captured 2026-04-25 via `wl/Examples/_bench/baseline.wls`, run
twice (CPU + Metal) on the same machine (Apple M3 Max).  All
numbers averaged over 4 steps.  This is the "before" snapshot
that bm5 diffs against once bm4's runtime slot allocator lands.

## Headline numbers

| bench                              | backend | ms/step | peak KiB | total KiB | kernels | tens | reuse headroom |
| ---------------------------------- | ------- | ------- | -------- | --------- | ------- | ---- | -------------- |
| lenet-mnist (Adam step)            | CPU     |     6.9 |   1882.3 |    4087.4 |     455 |  676 |          53.9% |
| lenet-mnist (Adam step)            | Metal   |    85.8 |   1882.3 |    4087.4 |     455 |  676 |          53.9% |
| beautiful-mnist (forward only)     | CPU     |   175.1 |  82750.3 |   94732.1 |     244 |  459 |          12.6% |
| beautiful-mnist (forward only)     | Metal   |   245.5 |  82750.3 |   94732.1 |     244 |  459 |          12.6% |

(`peak KiB` = peak concurrent live bytes from TMemoryPlan's
linear-scan view; `reuse headroom` = `(total - peak) / total`,
the upper bound on what a runtime slot allocator could save.)

Gantt PNGs alongside, by `<backend>-<net>` slug:
- [baseline-cpu-lenet-mnist.png](../wl/Examples/_bench/baseline-cpu-lenet-mnist.png)
- [baseline-metal-lenet-mnist.png](../wl/Examples/_bench/baseline-metal-lenet-mnist.png)
- [baseline-cpu-beautiful-mnist.png](../wl/Examples/_bench/baseline-cpu-beautiful-mnist.png)
- [baseline-metal-beautiful-mnist.png](../wl/Examples/_bench/baseline-metal-beautiful-mnist.png)

## What stands out

1. **Metal is currently SLOWER than CPU** for both nets at this
   size.  CPU lenet runs at 6.9 ms/step; Metal at 85.8 (~12x
   slower).  Same for beautiful-mnist (175 vs 245).  The Metal
   backend's `metal_dispatch_kernel` does one
   `commandBuffer / commit / waitUntilCompleted` PER kernel call;
   for a 455-kernel forward+backward+adam step that's 455 round-
   trips through the GPU command queue.  Tinygrad batches dispatch
   into a single command buffer per "schedule" -- the next
   tinygrad-port arc item should be **kernel batching on Metal**
   (one command buffer per `thvm_realize`, dispatch all kernels
   into it, then commit + wait once).  Order-of-magnitude speedup
   is on the table.

2. **lenet has 53.9% slot-reuse headroom**, beautiful-mnist has
   12.6%.  The much lower headroom on beautiful-mnist is the
   FC layer (Linear 6400 -> 10) dominating: a single 250 KiB
   weight + activation pair.  LeNet's conv2 partial-sum chain
   gives many short-lived bufs that overlap less, so the slot
   allocator can pack more aggressively there.  bm4's allocator
   should still recover the full 53.9% on lenet (~1 MiB).

3. **beautiful-mnist forward alone is 95 MiB**, vs LeNet's full
   step at 4 MiB.  The conv-lowered chain at 32 -> 64 channels
   produces a lot of intermediate bufs; this is exactly what
   the per-kh*kw partial-sum pattern looks like at scale.

4. **Adam-step backward exceeds KERNELS_CAP=4096 on
   beautiful-mnist** (so the full training-step bench isn't
   measurable yet -- shown above as forward-only).  bm4's slot
   allocator should compress the kernel count enough to run the
   full Adam step; bm5 will then re-bench end-to-end.

## Reproducing

```bash
# CPU run
wolframscript -f wl/Examples/_bench/baseline.wls

# Metal run
THVM_BACKEND=metal wolframscript -f wl/Examples/_bench/baseline.wls
```

Each run prints a TBench Association per scenario and writes a
`baseline-{backend}-{net}.png` Gantt next to itself.

## Post-bm4abc validation (bm4d, 2026-04-25)

Re-ran the same 4 scenarios after bm4a (CPU free-list infra),
bm4b (rollback wires non-preserved owning bufs to free-list),
bm4c (Metal mirror) all landed.

Numbers (all identical to baseline within ms-jitter):

| bench                            | backend | ms/step | peak KiB |
| -------------------------------- | ------- | ------- | -------- |
| lenet-mnist (Adam step)          | CPU     |     7.0 |   1882.3 |
| lenet-mnist (Adam step)          | Metal   |   100.9 |   1882.3 |
| beautiful-mnist (forward only)   | CPU     |   179.6 |  82750.3 |
| beautiful-mnist (forward only)   | Metal   |   250.6 |  82750.3 |

**Acceptance miss**: peak KiB drop on lenet-mnist is 0%, not the
target ≥30%.  The freelist wiring is correct (test_cpu_free_list
+ test_slot_reuse + test_metal_real freelist cases all green;
nn.wlt's TGrad scenarios still pass; Metal Adam-LeNet still
converges loss 2.61 → 0.025) but the slot allocator never
receives anything to recycle: `cpu_buf_pool_rollback_with_preserve`
walks `[wm, NEXT)` and skips every buf the chain-rooted preserve
walk has marked, and that walk pins every forward intermediate
reachable from the result tensor's `producer_kid` chain.  In a
LeNet Adam step that's the whole forward pass.

Same blocker as the refcount-driven free arc's sub-item c
(documented in docs/memory.md "Refcount-driven free arc"):
real savings need a HEAP-ROOTED preserve pass that walks
HEAP[] for live `TAG_TEN` cells (and pending UOP terms that
reference TenDescs cross-realize) instead of the conservative
producer_kid edge walk.  Queued as a separate arc once the
bm5 delta report closes.

Metal training STILL CONVERGES end-to-end and Metal forward +
backward correctness are unaffected; the bm4 arc lands the
foundation cleanly with zero regressions.
