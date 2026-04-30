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
| M1 batched Conv2D | in flight, view-only path landed but JIT compile cost blocks default-on | TConv2DIm2Col landed alongside TConv2D (commit c1a6dac).  ShapeTracker chain (commits 58bb690, dca964d), codegen-time index UOP inlining (12b9cf3, 6b334e8), JIT warmup gate (0dd4003) all in.  THVM_JIT_STRIDED=1 enables the view-only path; default off because the cold-start clang compile (~74ms × N unique stride patterns) regresses LeNet 4-step from ~3min to ~6.5min on a fresh /tmp.  Microbench shows runtime is identical to interpreter pre-mat (within noise) -- both touch the same memory.  Need a faster JIT (libtcc dead on aarch64, libllvm orcjit ~1000 LOC, background-compile shaves ~10s) before flipping default. |
| M2 BN gradient | open | smaller; verifies existing TBatchNorm |
| M3 structural-template TGradMany | deferred | not on the critical path yet |
| M4 buffer-free between steps | open | becomes critical at N_STEPS > 100 |
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
