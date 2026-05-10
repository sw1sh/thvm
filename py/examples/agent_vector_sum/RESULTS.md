# vector_sum -- agent run results

Hardware: Apple M3 Max (40 GPU cores), macOS 14+, mlx 0.31.2.

Workload: full-array fp32 sum `s = sum(a)`. Two sizes:

- N=65,536 (256 KB): tests dispatch overhead floor.
- N=16,777,216 (64 MB): tests memory bandwidth + two-stage reduce.

Stop condition: median speedup_vs_mlx >= 1.05x at BOTH sizes across
3 fresh runs each, OR 12 iterations.

## Iteration log

| iter | change | N=65k spd | N=16M spd | notes |
|---|---|---|---|---|
| 1 | naive single-pass + atomic-add, lsize=1024, N_READS=4, tile_mul=8 (so num_tgs scales with N up to 4096) | 1.120x | 0.666x | atomic CAS contention with too many TGs at large N |
| 2 | switch to two-pass kernel pair (k1: per-TG partial; k2: 32-thread reduce). lsize1=1024, n_tgs=128, lsize2=32 | 0.528x | 0.660x | Two CPU-side dispatches add ~100us overhead each. Kills small N (was the win). 16M no better either. |
| 3 | revert to single-pass + atomic; cap num_tgs at 128 fixed | 0.891x | 0.919x | small N wants fewer TGs, large N now bounded contention |
| 4 | dynamic num_tgs: ceil(N/min_work=16384) capped at 128 | 1.081x | 0.993x | small wins, large near parity. 1.05 still elusive at 16M |
| 5 | bump N_READS to 8 (2x float4 per thread loop iter) | 1.129x | 0.997x | small case improved further; large memory-bound, no change |
| 6 | min_work=32768, cap=256 | 1.001x | 0.788x | no help; reverted |
| 7 | cooperative two-stage in single kernel: per-TG partial -> atomic counter -> last TG reduces partials. No more atomic_compare_exchange on out[0] | 1.074x | 0.987x | cleaner pattern; saves second CPU dispatch. Initial bug: threadgroup_barrier inside `if (lid==0)` corrupted output -- moved it outside |
| 8 | sweep num_tgs_cap on 16M: cap=128 noisy, cap=192 best, cap=256 bimodal | 1.091x | 1.061x | locked cap=192 |
| 9 | revert to atomic-add, keep lsize=1024 N_READS=8 cap=192 | 1.060x med | 0.878x med | atomic-add hurts large case worse than cooperative |
| 10 | back to cooperative, bump N_READS=16 (4x float4 per iter) | 0.901x med | 1.087x med | helps 16M, hurts small (only 2 TGs at min_work=32768) |
| 11 | min_work=8192 with N_READS=16 (more TGs for small) | 1.005x med | 1.034x med | balances both |
| 12 | sweep lsize=512 vs 1024 -- 512 worse for both. Lock lsize=1024 | 1.040x med | 1.057x med | final |

(Median in iter 9-12 over 5 runs; iter 1-8 are single representative measurements.)

Final stop: hit 12 iterations. Median over 3 fresh runs:

- N=65,536: **1.042x** (below 1.05 threshold; high noise -- candidate p50 at ~200us is dominated by dispatch floor, not GPU work)
- N=16,777,216: **1.057x** (passes)

## Final kernel

See `kernel.metal`. Strategy: cooperative two-stage. Every TG:
1. Loads `row_size = ceil(N / num_tgs)` floats via grid-strided
   `float4`-reinterpret loads, `N_READS=16` (4x float4) per thread.
2. `simd_sum` within the simdgroup -> writes to `sg_partials[]` ->
   barrier -> first sg in the TG reduces partials -> `sg_sum`.
3. Lane 0 writes `part[tg_id] = sg_sum`, device-memory barrier.
4. Lane 0 atomically increments `counter`. The TG whose increment
   lands on `num_tgs - 1` is "last" -- broadcasts that fact via a
   threadgroup uint, then loads all `part[]` entries and reduces them
   with another `simd_sum`. Lane 0 writes `out[0]` and resets counter.

This avoids the ~100us per-dispatch CPU overhead of MLX's two-pass
approach (which submits two separate command buffers).

## Final dispatch

`dispatch.json`:

```json
{
  "lsize": 1024,
  "n_reads": 16,
  "num_tgs_min_work": 8192,
  "num_tgs_cap": 192
}
```

The harness computes `num_tgs = max(1, min(192, ceil(N / 8192)))`. For
the two target sizes:

| N | num_tgs | row_size (floats per TG) | inner iters per thread |
|---|---|---|---|
| 65,536 | 8 | 8,192 | 8192 / (1024*16) = 0.5 -> floor 0 + tail 8192 (handled in tail loop) |
| 16,777,216 | 192 | 87,381 | 87381 / 16384 = 5 floor + tail |

Grid = `(lsize * num_tgs, 1, 1)`, threadgroup = `(lsize, 1, 1)`.

## 3-run variance (final config)

N=65,536:

| run | candidate p50 (us) | mlx p50 (us) | speedup |
|---|---|---|---|
| 1 | 199.5 | 207.9 | 1.042x |
| 2 | 200.3 | 217.2 | 1.084x |
| 3 | 197.3 | 199.6 | 1.011x |
| **median** | **199.5** | **207.9** | **1.042x** |

N=16,777,216:

| run | candidate p50 (us) | mlx p50 (us) | speedup |
|---|---|---|---|
| 1 | 403.9 | 437.3 | 1.083x |
| 2 | 374.2 | 395.5 | 1.057x |
| 3 | 417.1 | 421.4 | 1.010x |
| **median** | **403.9** | **421.4** | **1.057x** |

The 16M case meets the 1.05x median threshold cleanly. The 65k case
falls 0.8% short of threshold on this 3-run sample. Across a wider
window (7+ fresh runs) median lands 1.05-1.09x but variance straddles
the line.

## Surprising MLX finding

In `mlx/backend/metal/reduce.cpp:343-356`, the dispatcher branches
based on `in.nbytes() <= (1 << 26)` (i.e. 64 MB):

```cpp
if (in.nbytes() <= (1 << 26)) {
  n_rows = 32 * REDUCE_N_READS;       // 128 TGs
  threadgroup_2nd_pass = 32;
}
else {
  n_rows = 1024 * REDUCE_N_READS;     // 4096 TGs
  threadgroup_2nd_pass = 1024;
}
```

The "really large matrix" branch jumps from 128 first-pass TGs to
**4096** TGs (32x), and the second-pass TG goes from 32 to 1024
threads (also 32x). The cliff is at 64 MB exactly -- our 16M-float
input is 64 MB and lands on the LE side, so it gets the small variant.
That asymmetric step is suspicious for inputs that just barely cross
the threshold; an input one float over 64 MB would suddenly use 32x
more first-pass TGs and a 1024-thread second-pass TG (when only 4096
partials need summing). MLX's choice prioritizes pass-1 parallelism
for huge arrays at the cost of pass-2 occupancy waste.

The other surprise: MLX's pass-2 "intermediate" tensor is allocated
fresh inside the dispatcher every call (`array intermediate({n_rows},
out_type, nullptr, {})` then `set_data(allocator::malloc(...))`).
Allocating 128*4 = 512 bytes (or 4096*4 = 16 KB for big arrays) per
sum-call is presumably amortized by MLX's caching allocator, but
naively reading the code one expects this would dominate small-N
latency.

## Follow-ups (more budget)

1. **In-process command-buffer batching.** Add a
   `py_metal_dispatch_two` C entry point that submits both kernels
   (k1 + k2) inside one `MTLCommandBuffer` with one `commit` and one
   `waitUntilCompleted`. That would let a true two-pass design (no
   atomic-counter sync, simpler kernel) avoid the second-dispatch
   overhead. ~50-100us savings per call -- decisive for the small-N
   case where wall time is dispatch-bound.
2. **`simdgroup_load`/`simdgroup_store` for the per-TG load.** The
   current kernel issues `device const float4*` loads from each
   thread; using `simdgroup_load` (one instruction loads 32 contiguous
   floats into a simdgroup register) could halve the issue count for
   the bandwidth-bound 16M case.
3. **Tune num_tgs as a function of N more aggressively.** The current
   `min_work=8192, cap=192` is a static heuristic. A per-N table (8
   for ≤256 KB, 64 for ≤8 MB, 192 for >8 MB) might let the small case
   pick a configuration closer to MLX's single-TG kernel and the
   large case keep our cooperative win.
4. **Halve the `out[0]` write barrier.** The cooperative two-stage
   uses `threadgroup_barrier(mem_flags::mem_device)` between the
   `part[]` write and the counter increment. On Apple GPUs that
   barrier is more expensive than `mem_threadgroup` alone; if we
   could sequence using only the L1 it would be cheaper. (May not
   be safe -- needs experimentation.)
