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

The next target is `wl/Examples/beautiful-mnist/train.wls`,
currently a tiny smoke architecture (Conv 1->4 + Pool + FC at
BS=1).  The full target architecture is:

```
Conv 1->32 5  -> ReLU -> Conv 32->32 5 -> ReLU -> BN32
 -> MaxPool 2x2 -> Conv 32->64 3 -> ReLU -> Conv 64->64 3
 -> ReLU -> BN64 -> MaxPool 2x2 -> Flatten -> Linear 576->10
```

trained at BS=512 with Adam.  Tinygrad parity = competitive
per-step training time (~milliseconds-scale per step on Apple
Silicon) for this exact architecture.

## Blockers, in priority order

### M1. Batched Conv2D (im2col + sgemm) -- blocks BS>1

`TConv2D` lowers as a kh*kw partial-sum chain with `Fold[Plus]`,
re-evaluated per batch element.  At BS=512, kernel 1 alone emits
512 * 25 = 12,800 partials.  Wallclock at BS=512 with the current
lowering is hundreds of seconds per step.

**Fix sketch:**
- Add `TConv2D` lowering that produces an im2col reshape of the
  input + a single batched `cblas_sgemm` dispatch.
- Keep the existing kh*kw lowering as a fallback when the input
  isn't BLAS-eligible (non-contiguous, unusual dtype, etc.).
- `cpu_blas_dispatch` already pattern-matches a specific
  `MUL + REDUCE_SUM` shape; extend it to recognize the im2col
  pattern.

**Verify:** beautiful-mnist train at BS=32 completes one step in
under a second.

### M2. BatchNorm gradient

`TBatchNorm` exists (forward).  The gradient through the
`(x - mean) / sqrt(var + eps)` chain needs verifying:
- mean / var reduce + broadcast pattern interacts with the chain
  rule's REDUCE_SUM adjoints.
- Need a unit test in `nn.wlt` that grad-checks BN against a
  finite-difference reference.

**Verify:** numerical grad-check on BN matches symbolic grad to
3-4 digits f32 precision.

### M3. Multi-grad structural sharing -- TGradMany walk-once

Each TGrad on LeNet now allocates ~108K cells.  For
beautiful-mnist's 8 weights x BS=512, the cumulative per-step
allocation is ~10M cells -- well under HEAP_CAP/2 (32M) but with
no sharing across the 8 grad walks.  The structural-template
fix (cache `interact_grad(uop, target)` -> template parametric
in `gy`) would cut this 8x.  See `docs/bench/phase16.md` for the
sketch + the SUP-projector experiment that didn't pan out.

**Defer until M1 is in:** measuring shows the chain-rule walk is
~5% of LeNet wallclock today.  Won't move the needle until the
forward + Conv backward are kernel-dispatch-bound, not
chain-rule-bound.

### M4. Refcount-driven buffer free between Adam steps

Each Adam step accumulates ~6K kernels (LeNet: ~750/grad x 8
weights).  After 100 steps the kernel table holds 600K entries
-- past KERNELS_CAP=256K.  The kernel table itself is a flat
arena and doesn't reuse slots.

**Fix sketch:**
- Per-step "clear all kernels reachable only from this step's
  outputs" pass.  KernelEntry has consumer_count via
  `kernel_compute_consumer_counts`; extend it to a free-when-
  zero discipline.
- Or: run `gc_collect` between Adam steps with the params +
  optimizer state as roots.  Cheney evacuates only what's live;
  step-scoped kernels die.

**Verify:** beautiful-mnist train at N_STEPS=1000 completes
without arena exhaustion.

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
| M1 batched Conv2D | in flight | TConv2DIm2Col landed alongside TConv2D; verified numerical + grad equivalence in `wl/THVMLink/Tests/conv_im2col.wlt` (6 tests).  TConv2D not yet switched over -- next ticks measure perf delta on LeNet/beautiful-mnist before flipping the public surface. |
| M2 BN gradient | open | smaller; verifies existing TBatchNorm |
| M3 structural-template TGradMany | deferred | not on the critical path yet |
| M4 buffer-free between steps | open | becomes critical at N_STEPS > 100 |
| M5 Metal Conv | open | depends on M1 |
| M6 autotune sweep | open | depends on M1 (autotune needs real kernel shapes) |

### M1 in-flight notes

- `TConv2DIm2Col` builds `xCol : {cIn*kh*kw, hOut*wOut}` via PAD-and-sum
  (no STACK primitive in thvm) then runs a single TMatMul which dispatches
  through `cpu_blas_dispatch`'s MUL+REDUCE_SUM pattern -> cblas_sgemm.
- Forward + d/dw + d/dx + d/db all match TConv2D within 1e-4 f32 tolerance.
- Next ticks: (1) bench TConv2DIm2Col vs TConv2D on LeNet w/ N_STEPS=4
  to confirm parity or speedup; (2) flip the public TConv2D over once
  measurements look clean; (3) extend to BS>1 (input shape {N, C_in, H, W})
  -- the PAD-and-sum slot trick generalises since the batch axis is
  the leading dim.
