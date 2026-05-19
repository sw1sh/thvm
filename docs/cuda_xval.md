# CUDA cross-validation: thvm-CUDA vs tinygrad-CUDA vs numpy

Stage 4 of the CUDA backend slice (`docs/plans/cuda_backend.md`).  This
measures, honestly, the gap between thvm's CUDA codegen and tinygrad's
on the same GPU.  It is a measurement task, not an optimization task:
the headline result is the size of the gap, and what would close it.

Harness: `py/examples/cuda_xval.py`.  Reproduce with
`python3 py/examples/cuda_xval.py --json out.json` on a Linux+CUDA host.

## Setup

- Device: Tesla V100-SXM2-16GB, Volta SM70, CUDA 12.4 toolkit.
- thvm-CUDA: build the UOp DAG via `py.thvm`, `render_cuda`, nvrtc-compile
  (`--gpu-architecture=compute_70`), dispatch via the `Cuda` class.
- tinygrad-CUDA: the equivalent op (`@`, `.sum(axis=1)`, `.softmax`)
  with `CUDA=1`, run both default and with `BEAM=2` (the autotuner).
- numpy: the correctness reference, computed in float64 (the true value).

### Measurement methodology

Both timings are GPU-event time, so the comparison is apples-to-apples:

- thvm-CUDA: `Cuda.dispatch_timed`'s `gpu_ns` -- `cuEventElapsedTime`
  between CUDA events bracketing the launch.
- tinygrad: parsed from its own `DEBUG=2` per-kernel `tm` field, which
  is also CUDA-event time.  Host-to-device `copy` lines also carry a
  `tm`; the harness stages inputs onto device buffers once, outside the
  timed loop, and filters copy lines, so only the compute kernel(s) are
  counted.  tinygrad wall time is never compared.
- 200 thvm reps (20 warmup) and 60 tinygrad reps after 8 warmup; p50
  and p10 reported.  The whole sweep was run 3 times; the table gives
  the median of the three.  Run-to-run variance was below ~5% (p10 and
  p50 nearly coincide), well under the noise floor -- the numbers are
  stable.
- Shared pod: the V100 is shared with the `brain/` experiments.  The
  sweep was run during a window when the brain job's GPU utilization
  was 0% (it was between training phases); GPU was quiesced for all
  three runs.

### Correctness gate

A GPU reduction over W elements accumulates serially in fp32; that
legitimately differs from numpy's pairwise fp32 sum.  The reference is
therefore computed in float64 and compared under a width-aware fp32
band: `tol_abs = 4 * W * 2^-23` for matmul (dot of W terms) and
row-reduce (sum of W terms); softmax outputs are in [0,1] so a fixed
`1e-4` abs band applies.  The pass test is `max_abs <= tol_abs OR
max_rel <= tol_rel`.  The *same* band is applied to thvm and tinygrad,
so the gate is fair.

## Results (median of 3 runs)

| op + shape | correctness (vs float64 ref) | thvm-CUDA GPU p50 / p10 (us) | tinygrad GPU p50 / p10 (us) | tinygrad BEAM2 p50 / p10 (us) | gap thvm / tg-default | gap thvm / tg-BEAM2 |
|---|---|---|---:|---:|---:|---:|---:|
| matmul 256x256x256   | abs 1.4e-5, rel 4.2e-2 -- OK | 26.6 / 26.6     | 55.3 / 54.3   | 31.7 / 30.7  | **0.48x** | 0.84x |
| matmul 512x512x512   | abs 2.9e-5, rel 5.2e-2 -- OK | 191.5 / 187.4   | 129.0 / 128.0 | 54.3 / 54.3  | 1.48x | 3.53x |
| matmul 1024x1024x1024| abs 8.3e-5, rel 4.3e-1 -- OK | 1515.5 / 1503.2 | 696.3 / 695.3 | 213.0 / 210.9| 2.18x | 7.12x |
| softmax 4096x4096    | abs 7.2e-9, rel 4.0e-6 -- OK | 68179 / 68062   | 827.4 / 823.3 | 363.5 / 361.5| **82.4x** | 187.6x |
| row_reduce 4096x4096 | abs 3.0e-4, rel 1.6e-3 -- OK | 827.4 / 825.3   | 259.1 / 258.1 | 94.2 / 93.2  | 3.19x | 8.78x |

Both thvm-CUDA and tinygrad produce *bit-identical* error vs the
float64 reference at every shape -- they execute the same fp32 math in
(at most) a different accumulation order.  **Correctness held
everywhere: thvm-CUDA matches numpy within the fp32 tolerance for all
five op+shape configurations.**

The matmul `rel` figures (up to 4.3e-1) look alarming but are not: they
are the relative error on individual matmul output elements that happen
to land near zero, where a 1e-4 absolute error becomes a large
relative one.  The absolute error (<=1e-4) is the meaningful number and
it is identical for both implementations; the pass gate is satisfied
on the abs arm.

## Where thvm-CUDA stands

- **Closest -- matmul 256:** thvm-CUDA is actually *faster* than
  tinygrad-default (0.48x) and within 16% of tinygrad-BEAM2.  At this
  size the matmul is tiny (~34 MFLOP) and dispatch/occupancy effects
  dominate; thvm's single flat `extern "C" __global__` kernel with one
  thread per output element launches cleanly and tinygrad's default
  schedule is not yet tuned.
- **Mid-range -- matmul 512 / 1024, row-reduce:** thvm is 1.5-3.2x
  slower than tinygrad-default and 3.5-8.8x slower than BEAM2.  This is
  the structural/scalar path showing its cost: thvm emits one thread
  per output element with a plain serial accumulator loop (the rendered
  matmul is a 24-line `.cu` with a `for (k...) acc += A[..]*B[..]`).
  There is no shared-memory tiling, no register blocking, no
  vectorized load.  tinygrad-default already tiles; BEAM2 additionally
  searches upcast/local/unroll factors.
- **Furthest -- softmax 4096x4096: 82x slower than tinygrad-default,
  188x slower than BEAM2.**  This is not noise and not contention -- it
  reproduced to within 0.2% across all 3 runs.  It is a genuine
  asymptotic blowup in the rendered kernel, explained below.

### Why softmax is 82x off: the O(R*C^2) structural softmax

The thvm row-wise softmax DAG has the output axes `r, c` as `LOOP`
axes and two `REDUCE` axes that recompute the row max and the row sum.
The CUDA structural renderer flattens both output LOOP axes onto `tid`
-- so it emits **one thread per output element** (16.7M threads for
4096x4096), and *each* thread runs both reduce loops in full:

```
uint a0 = (tid / 4096u) % 4096u;        // the row
float _acc2 = -INFINITY;
for (uint a2 = 0; a2 < 4096; a2++)      // recompute the row MAX
  _acc2 = fmax(_acc2, in0[a0*4096 + a2]);
float _acc3 = 0.0f;
for (uint a3 = 0; a3 < 4096; a3++)      // recompute the row SUM
  _acc3 = _acc3 + exp2(...);
uint a1 = tid % 4096u;                  // the column
out[a0*4096 + a1] = exp2(...) * (1.0f/_acc3);
```

All 4096 threads in a row compute the *identical* `_acc2` and `_acc3`.
The row reduction is done 4096x redundantly: total work is O(R*C^2) =
4096 * 4096^2 ~ 2.7e11 reads, versus the O(R*C) ~ 1.7e7 that the
problem actually requires.  That ~4096x redundancy is exactly the
observed 82x gap order (less than 4096x because the redundant reads hit
L2 cache).  tinygrad realizes softmax as 3 fused kernels that compute
each row's max and sum *once* and share them across the row.

This is the single most important finding of the cross-validation: the
structural lowering of a reduce that feeds a broadcast is correct but
asymptotically wrong when the reduce result is consumed by many output
threads.

### Why row-reduce is 3.2x off: one thread per row, no parallel reduce

The rendered row-reduce is one thread per output row -- 4096 threads
total, a serial 4096-iteration `acc += in[..]` loop each.  4096 threads
is half of one wave on the V100's 80 SMs at 2048 threads/SM, so the
machine is ~20x underpopulated, and within a thread the reduction is
serial.  tinygrad parallelizes the reduction across threads and tiles
the rows.  The gap (3.2x default, 8.8x BEAM2) is smaller than softmax's
because there is no quadratic redundancy here -- just poor occupancy
and a serial inner reduce.

## What would close the gap, prioritized

1. **Share a reduce result across the threads that broadcast it
   (fixes softmax, the 82x case).**  When a `REDUCE` feeds a value that
   is stored across an output axis the reduce does not depend on, the
   reduce must be hoisted: computed once per row, not once per output
   element.  Concretely, the renderer (or a scheduling pass) should
   either (a) put the row on a thread *block* and the column reduction
   in shared memory / a warp `__shfl_down_sync` reduction, computing
   the max+sum once per block; or (b) split softmax into separate
   reduce and elementwise kernels as tinygrad does.  This alone turns
   the 82x into a low single-digit gap and is by far the highest-value
   change.

2. **Parallelize the reduce, don't serialize it (fixes row-reduce, and
   helps softmax).**  The `OPT_SIMD_REDUCE` lowering already exists and
   the CUDA renderer already emits the `__shfl_down_sync` warp
   butterfly for it (`test_render_uop_cuda` covers this).  It is simply
   not being *proposed* for these DAGs.  Wiring `propose.c` to offer a
   warp-reduce KOpt for `REDUCE` axes -- and a `LOCAL` split so a row
   maps to a block, not a thread -- would let the reduce run across 32
   lanes instead of one, and raise occupancy from 4096 threads to a
   full GPU.

3. **Tile the matmul (closes the 512/1024 matmul gap).**  thvm holds
   ~1.3-1.4 TFLOP/s across matmul sizes -- a flat untiled-kernel
   number.  tinygrad-BEAM2 reaches ~10 TFLOP/s at 1024 by tiling into
   shared memory, register-blocking the accumulator, and vectorizing
   loads.  thvm already has the `OPT_UPCAST` / `OPT_LOCAL` /
   `OPT_VEC_LOAD` lowerings and the CUDA renderer emits `float4` loads
   for `OPT_VEC_LOAD`; they need to be *proposed* for the matmul DAG.
   The Volta WMMA path is out of scope here (WMMA is fp16-only on
   Volta, marginal for an fp32 corpus) -- the win is a tiled scalar
   kernel with shared-memory staging, not tensor cores.

4. **Autotune the proposals (matches BEAM2).**  Even with tiling,
   tinygrad's edge at 512/1024 is partly the BEAM search picking the
   best upcast/local/unroll factors per shape.  thvm's `autotune.c`
   BEAM machinery exists; once `propose.c` emits CUDA-appropriate tile
   proposals, running them through the existing search would track
   BEAM2 instead of tinygrad-default.

In short: the gap is dominated by *missing tile/reduce proposals*, not
by the renderer.  The CUDA renderer already emits warp reductions,
`float4` loads, and (for fp16) WMMA -- it correctly lowers whatever
`OPT_*` annotations the DAG carries.  The structural path is being used
because nothing proposes the better lowerings.  Priority 1 (hoist the
shared reduce) is both the biggest single win and a correctness-of-
*performance* issue, not just a tuning knob.

## Render-side fix landed during this stage

The CUDA renderer emitted `-INFINITY` for the `REDUCE_MAX` accumulator
init, but nvrtc's device compilation predefines no `<math.h>` macros,
so the softmax kernel failed to compile (`identifier "INFINITY" is
undefined`).  Fixed in `render_uop.c`: the CUDA preamble now emits
`#define INFINITY __int_as_float(0x7f800000)` (the fp32 +inf bit
pattern as a device constant, the same `__*_as_float` idiom the
renderer already uses for the fp32 bitcast else-arm).  This was a
genuine Stage-2 render caveat that only surfaced once a `REDUCE_MAX`
DAG actually reached nvrtc.

## Out of scope

`propose.c` WMMA tile-proposal wiring was explicitly deferred: Volta
WMMA is fp16-only, marginal for this fp32 corpus.  The prioritized list
above (shared-reduce hoist, warp reduce, scalar tiling) is the fp32
path and is where the measured gap actually lives.
