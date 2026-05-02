---
title: Path to tinygrad parity on beautiful-mnist
status: open
---

# Path to tinygrad parity on beautiful-mnist

After commits `e0029b5` (`grad_zero_at` shape fix), `4646189`
(boundary table caps), and `9c4f7f4` (per-realize kernel fire memo
+ arena cap bumps), `wl/Examples/lenet-mnist/train.wls` now runs
end-to-end:

```
training 4 steps:
  step 1: loss = 2.6071
  ...
  step 4: loss = 0.6789
  final:  loss = 0.3693
LeNet training PASSED
```

The old `wl/Examples/beautiful-mnist/train.wls` is still a tiny smoke
architecture (Conv 1->4 + Pool + FC at BS=1).  The current parity
target is `wl/Examples/beautiful-mnist/bench-train.wls`, which builds
the full architecture:

```
Conv 1->32 5  -> ReLU -> Conv 32->32 5 -> ReLU -> BN32
 -> MaxPool 2x2 -> Conv 32->64 3 -> ReLU -> Conv 64->64 3
 -> ReLU -> BN64 -> MaxPool 2x2 -> Flatten -> Linear 576->10
```

trained at BS=512 with Adam.  Tinygrad parity = competitive
per-step training time (~milliseconds-scale per step on Apple
Silicon) for this exact architecture.

Current measurement discipline: ignore first capture / first sample
overhead.  Compare steady TJit replay for the full BS=512 training
loop against tinygrad's captured Metal training step.  On this
machine, the local tinygrad checkout is around `30-33ms` per steady
BS=512 training step after warmup.

Current thvm status:

- Rank-4 forward is plumbed through Conv2D, maxpool, and batch norm.
- Generated Metal tile Conv2D handles rank-4 inputs and the
  single-channel first-conv patch-input form.
- BS=512 forward-only replay is `~66ms` without autotune over the
  measured steady window and `29.7ms` with post-autotune in the last
  cached-opt run; forward is no longer the main parity blocker.
- Generated Metal expression JIT now covers heavy f32
  movement/ALU backward materializers without preempting tile kernels.
  This makes BS=512 `grad-7` replay about `127ms` and cuts `grad-3`
  to about `2.6s`.
- Full BS=512 Adam replay now completes, but is still `~41-46s`
  steady versus tinygrad's `30-33ms`.  The gap is the repeated
  all-target early-conv backward graph (`3285` kernels, including
  `1048` generated Metal JIT kernels and `48` per-op Metal fallbacks),
  not first-sample overhead.

## Blockers, in priority order

### M1. Early Conv/BN/Pool backward tile coverage -- blocks training

Full forward now runs at BS=512, but training is blocked by the
backward path for early convolution weights.  Target-pruned `TGrad`
made final-layer and late-block gradients cheap enough to replay; W1
still walks and lowers a very large movement/reduction graph.

**Fix sketch:**
- Add tile recognition for Conv2D backward shapes, not just forward
  im2col-shaped Conv2D.
- Make BN train backward reductions use generated group/row
  reductions instead of generic movement-heavy `metal-op` kernels.
- Keep the benchmark target as steady replay, not first-capture time.

**Verify:** `BENCH_MODE=grad-1 BS=512` and `BENCH_MODE=grad-3 BS=512`
drop from seconds to the same class as `grad-7`, then rerun full
`BENCH_MODE=train`.

### M2. Full backward tile coverage -- training-loop parity

Forward-only BS=512 replay can hit `29.7ms` after autotune, and full
training replay now completes, but early backward paths are still far
too slow.  Current BS=512 single-gradient replays:

```text
grad-1: 4368.0ms
grad-3: 2578.1ms
grad-7:  126.9ms
grad-9:  186.1ms
```

**Verify:** `BENCH_MODE=train BS=512` reports a steady replay loop in
the same order of magnitude as tinygrad's `30-33ms`, with the repeated
early-conv backward program shapes fused or shared instead of emitted
hundreds of times.

### M3. Multi-grad structural sharing -- TGradMany walk-once

Target pruning handles independent irrelevant branches, but full
`TGradMany` still re-lowers target-specific backward graphs.  This is
now on the critical path: full Adam replay emits `3285` kernels even
though many are repeated early-conv backward shapes.  The next useful
design is either a structural-template `TGradMany` walk-once pass, or
producer-consumer fusion that prevents those partial-gradient
materializers from existing as standalone buffers.

### M4. Refcount-driven buffer free between Adam steps

Each Adam step accumulates ~6K kernels (LeNet: ~750/grad x 8
weights).  After 100 steps the kernel table holds 600K entries
-- past KERNELS_CAP=256K.  The kernel table itself is a flat
arena and doesn't reuse slots.

**Status (2026-04):** in flight, partial.  `schedule/kernel_gc.c`
runs at end of every `thvm_realize` and strips per-kernel
program/input arrays for kernels whose output buffer was already
released by `cpu_buf_pool_rollback_with_preserve` (refcount==0
liveness signal).  This bounds the **per-kernel array memory**
(KProgOp[] + input_tids[] etc.) across long training loops, but
does NOT recycle KernelEntry slot ids -- a slot reuse would
mis-dispatch when a future realize DFS-fires the old kid via
TENS[].producer_kid.

**Still open (root-cause now understood, 2026-04-30):**

Measurement: `wolframscript /tmp/measure_thvm.wls`:
- LeNet forward only: 64 kernel slots, 40 unique programs
- One TAdam step: **5,023 slots, 130 unique programs** (cache-warm: 36 new)
- Tinygrad LeNet equivalent: **57 scheduled kernels per step**

Two distinct gaps stack:

1. **Slot inflation (39x):** 5,023 slots backed by only 130 unique
   programs.  `emit_kernel_for_boundary` allocates a fresh
   `KernelEntry` + output `TenDesc` per boundary even when the
   program is already in the kernel-program cache.  Fix: at
   emit time, when the program signature matches a previously-
   emitted kernel for THIS REALIZE (or one whose output buffer
   is still alive), reuse that kid + output_tid instead of
   creating a new one.  Should drop slot count to roughly the
   unique-program count.

2. **Fusion gap (~2x vs tinygrad):** 130 unique programs vs
   tinygrad's 57.  Architectural -- tinygrad lowers the high-
   level UOp graph through RANGE / BUFFERIZE / INDEX / LOAD /
   STORE primitives, fusing entire elementwise+reduce chains
   into one BUFFERIZE point per kernel.  thvm has only the
   high-level UOps (tensor-level ADD / MUL / REDUCE etc.); the
   "kernel program" is built per-boundary by the realize
   classifier, which can fuse only what's in a single contig
   chain between boundaries.  Bigger lift: add a lowering pass
   that translates UOP graphs to scalar-level UOps with explicit
   ranges/indices, then schedule BUFFERIZE points based on
   read-after-write dependencies.

The TRealize-list bundling form (commit landing alongside this
update) routes per-param ASSIGNs through one realize call, so
Adam goes from 9 realizes/step (~5,087 slots) to 1 realize/step
(~5,023 slots) -- bookkeeping win, doesn't move the slot/program
ratio.  The slot-inflation fix is the next concrete step.

**Verify (current bar):** N_STEPS=5 LeNet trains; loss decreases
monotonically; per-step kernel array memory does not grow
unboundedly.  `THVM_KGC=0` falls back to pre-M4 leak-everything
behaviour.

**Verify (full bar):** beautiful-mnist train at N_STEPS=1000
completes without arena exhaustion.  Open.

### M5. Metal backend for the Conv kernels

Once M1 lands (im2col -> BLAS on CPU), the analogous Metal path
is a Metal Performance Shaders MPSCNNConvolution dispatch, or a
custom shader against MPS matrix-multiply.  Tinygrad defaults to
Metal on Apple Silicon; CPU-only parity isn't really parity.

**Defer until M1 is in.**

### M6. Autotune across the per-program-shape kernel cache

Phase 16's autotune infrastructure (`THVM_AUTOTUNE=1`,
`TKernelAutotuneAll[]`) exists.  Run it across beautiful-mnist's
distinct kernel program shapes, materialize the winning opts in
the kernel-program cache, and capture a baseline of opts that
travel across all kids of the same program.

## Operating discipline

- Every milestone lands behind a wlt regression test (correctness
  + kernel-count baseline + per-step wallclock cap).
- Each commit keeps 422/422 WL grid green.
- Per-step training time on LeNet is the canary: if it regresses,
  the milestone work doesn't land.
- Cumulative Adam-step wallclock on beautiful-mnist is the
  primary metric -- compare to tinygrad's `python beautiful_mnist.py`
  on the same machine for parity.

## Status

| Milestone | State | Notes |
|-----------|-------|-------|
| LeNet end-to-end | done | commits e0029b5, 4646189, 9c4f7f4 |
| M1 batched Conv2D | in flight, view-only path landed but JIT compile cost blocks default-on | TConv2DIm2Col landed alongside TConv2D (commit c1a6dac).  ShapeTracker chain (commits 58bb690, dca964d), codegen-time index UOP inlining (12b9cf3, 6b334e8), JIT warmup gate (0dd4003) all in.  THVM_JIT_STRIDED=1 enables the view-only path; default off because the cold-start clang compile (~74ms × N unique stride patterns) regresses LeNet 4-step from ~3min to ~6.5min on a fresh /tmp.  Microbench shows runtime is identical to interpreter pre-mat (within noise) -- both touch the same memory.  Need a faster JIT (libtcc dead on aarch64, libllvm orcjit ~1000 LOC, background-compile shaves ~10s) before flipping default. |
| M2 BN gradient | open | smaller; verifies existing TBatchNorm |
| M3 structural-template TGradMany | deferred | not on the critical path yet |
| M4 buffer-free between steps | partial | per-kernel arrays freed; slot reuse blocked on orphan-emission cleanup |
| M5 Metal Conv | open | depends on M1 |
| M6 autotune sweep | open | depends on M1 (autotune needs real kernel shapes) |

### M1 in-flight notes

#### Step 1: TConv2DIm2Col landed (commit c1a6dac)

- Builds `xCol : {cIn*kh*kw, hOut*wOut}` via PAD-and-sum (no STACK
  primitive in thvm) then runs a single TMatMul which dispatches
  through `cpu_blas_dispatch`'s MUL+REDUCE_SUM pattern -> cblas_sgemm.
- Forward + d/dw + d/dx + d/db all match TConv2D within 1e-4 f32 tolerance
  (`wl/THVMLink/Tests/conv_im2col.wlt`, 6 tests).

#### Step 2: bench (verdict: PAD-and-sum is the wrong design)

| Path             | conv1 fwd+grad | conv2 fwd+grad | LeNet 4-Adam-steps |
|------------------|----------------|----------------|--------------------|
| TConv2D          | 44.7 ms        | 41.2 ms        | ~3 s               |
| TConv2DIm2Col    | 23.9 ms (1.87x)| 23.8 ms (1.73x)| **341 s (~100x slowdown)** |

Isolated forward+grad is ~1.8x faster.  But end-to-end LeNet train
is 100x slower because:
- Each PAD kernel materialises a `cIn * kh*kw * hOut*wOut` buffer
  that's mostly zeros.  For LeNet conv1 that's 25 PADs each
  allocating ~14KB.  Plus the chain rule walks back through every
  PAD with a SHRINK adjoint, multiplying the per-grad kernel count.
- TConv2D's kh*kw partials each produce a smaller `cOut*hOut*wOut`
  buffer (`6*576*4 ≈ 14KB`) directly without the zero-pad detour.

#### M1 blocker: view-only im2col needs a view system extension

The tinygrad performance path is **stride-trick im2col**: build
`xCol` as a non-contiguous view of the input (no byte motion), and
let the matmul kernel pre-materialize the view into a contig buffer
exactly once before sgemm.  thvm's `cpu_interpret` already
pre-materializes non-contig inputs for kernel dispatch
(src/backend/cpu/interpret.c:31), so the dispatch side is ready.

The construction side is not.  All `view_apply_*` primitives in
`src/schedule/materialize.c` bail on non-contig sources (`if
(!src->contiguous) return 0`).  Worse, `view_apply_expand` can only
produce stride-0 broadcasts; the im2col stride trick needs a
non-zero stride on the kernel-patch axis (e.g. `stride(kh) = stride(H)`),
which can't be expressed via EXPAND from a unit axis.

The path forward is one of:

A. **Stride-aware reshape** -- relax `view_apply_reshape` to handle
   non-contig sources where the reshape decomposes to pure axis
   split/merge.  Plus relax `view_apply_permute` similarly.  Plus
   teach EXPAND to optionally inherit the source axis's stride
   (instead of 0) for the "as-strided" duplication pattern.
   ~150-300 LOC across `view_apply_*` and probably a new
   `UOP_AS_STRIDED` primitive.  Largest scope.

B. **Native conv kernel** -- C-side function that does its own
   im2col + sgemm in one kernel boundary, bypassing the WL chain
   rule entirely for the forward pass.  Backward pass still needs
   to express conv-adjoint somehow (could be a second native
   kernel for the bwd, or fall back to TConv2D's lowering for
   gradients).  Smaller scope but two implementations to maintain
   and the bwd-via-TConv2D defeats half the speedup.

C. **Defer M1, focus on what TConv2D can be made faster** --
   the 25 partials chain has obvious fusion opportunities (e.g.
   the kh*kw EXPANDs all read disjoint slices of the same input;
   the per-partial REDUCE outputs all add to the same accumulator).
   `cpu_blas_dispatch` could in principle recognize the whole
   kh*kw partial-sum pattern and lower it as a single sgemm
   internally, without changing the WL surface.

For now: (A) is the right tinygrad-faithful fix but big.  (C) might
be quicker to extract some of the perf without a view-system rewrite.
Defer M1's "flip the public TConv2D" until one of A/B/C lands;
TConv2DIm2Col stays available as an alternate lowering for users
who explicitly want it (mostly useful for testing the matmul
dispatch path).

---

## M1 attempt log (post-F-8 freeze)

Tried routing `TConv2D` through `TConv2DIm2Col` (alias the kh*kw
lowering as `TConv2DKhKw` for the conv-im2col reference tests).
Test run hung -- aborted before completion.

The hang likely indicates an infinite loop or deep recursion
introduced when more tests start exercising the im2col path.
Reverted the change pending investigation.

Next steps:
- Bisect which specific test hangs by routing TConv2D through
  TConv2DIm2Col only inside a single test file at a time.
- Once the hanging test is isolated, dump the kernel program
  structure and check whether the F-8e-7 S_RESHAPE iter coord
  transform fires for an unsupported shape pattern.
- Alternative: only route TConv2D through TConv2DIm2Col when
  the input is BS>1 (rank-4 with leading batch axis) -- that
  way the BS=1 tests stay on the kh*kw path, and only batched
  inference/training picks up the im2col speedup.

---

## M1 attempt 2 (post-linearizer-parity) -- still slow at BS=1

After the linearizer parity MVP shipped (5b1a216), retried routing
TConv2D -> TConv2DIm2Col.  The earlier "hang" turned out to be
just slowness: with the routing in place, beautiful_mnist.wlt's
2 forward tests took >3 minutes (vs ~10 seconds with the kh*kw
lowering).  Tests were dying mid-run (1 reported failure when
killed at 3m05s), confirming the path produces correct output but
each per-call work is much higher than expected.

Why im2col is slow at BS=1 small input:
- Each TConv2D call builds kh*kw=25 SHRINK + RESHAPE + PAD ops,
  sums them via Fold[Plus], reshapes, then matmuls.
- For input (1, 28, 28) the resulting im2col xCol = (25, 576)
  feeds a (32, 25) @ (25, 576) sgemm -- the actual sgemm is
  tiny (~1us) but the surrounding 25-op build chain dominates.
- For input (32, 24, 24) the chain is even bigger -- 25 SHRINKs
  on a 32x24x24 tensor, each producing 32x20x20 intermediates.
- The kh*kw lowering does 25 partial sums of (cOut, cIn, hOut, wOut)
  reductions but each is a single fused MUL+REDUCE kernel.
  Per-call kernel count is similar (~25) but each is "atomic"
  rather than a chain of 4-5 ops.
- Net: at BS=1 small, kh*kw wins on per-call overhead.  Im2col
  only wins when the sgemm cost dominates the per-call setup --
  which requires BS>1 OR much larger spatial dims.

Proper M1 fix needs:
- Either: route TConv2D -> TConv2DIm2Col only when the matmul
  cost-to-setup ratio favors it (a heuristic: input.numel >
  threshold, OR BS > 1).
- Or: actual batched conv -- input shape {BS, C_in, H, W} with
  one big im2col producing xCol = {BS*H_out*W_out, C_in*kh*kw}
  and ONE sgemm.  This is the real beautiful-mnist BS=512 win.
- The current TConv2D and TConv2DIm2Col both take rank-3 input
  ({C_in, H, W}) -- BS>1 today is a per-sample loop on the
  WL side, which is what makes BS=512 take "hundreds of seconds"
  per the M1 description.  Fixing requires a rank-4 conv API.

Reverted M1 attempt 2.  Bumping the priority of the rank-4 conv
work (the actual M1 win) above the simpler TConv2D->TConv2DIm2Col
routing.

Status: M1 deferred until rank-4 conv is plumbed through.
