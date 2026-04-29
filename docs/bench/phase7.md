# Phase 7 bench — TJit capture/replay

Tinygrad's `@TinyJit` captures the kernel sequence on first call and
replays it on subsequent calls, skipping the scheduler entirely.
thvm equivalent: [`TJit[fn]`](../../wl/THVMLink/Kernel/Jit.wl) wraps a
function as a closure that records every kernel dispatch + every
in-place `TAssign` memcpy through
[`src/jit/capture.c`](../../src/jit/capture.c).  Replay walks the
recorded sequence and dispatches each `KernelEntry` directly against
the captured `(in_buf_ids, out_buf_id)` tuple -- no
`realize_classify`, no `materialize`, no kernel-program hash-cons
lookup.

## Two anchors

### (a) Scheduler-bound: 64x64 matmul + bias, 50 iters

Eager:

```
50 iters: 440.7 ms total, 8.8 ms/iter
```

JIT:

```
50 iters:   0.5 ms total, 0.0 ms/iter
captured ops: 1
```

**Speedup: ~928x.**  Scheduler walks the UOp graph + emits a kernel
+ runs `cblas_sgemm` once per eager iter.  The actual sgemm is fast;
the per-iter scheduler cost dominates.  Replay skips everything but
the dispatch.

### (b) Pipeline-bound: linear regression train step (no conv)

Forward (`TMatVec[W, x] + b`) + sparse-CE + per-weight `TGrad` +
`TAdam`, step n=5:

```
step 1 wall=2602.6ms       <- capture pass (full TGrad expansion + dispatch)
step 2 wall=   0.2ms       <- replay
step 3 wall=   0.1ms
step 4 wall=   0.1ms
step 5 wall=   0.1ms
captured ops=114
```

**Speedup: ~26,000x steady-state.**  Step 1 absorbs the cost of
expanding `TGrad` through the chain rule, materialising every
intermediate, and emitting all the kernels.  Steps 2+ replay 114
ops (kernel dispatches + Adam's TAssigns) in ~0.1ms each.

## What's captured

`JitCaptureOp` carries two op kinds:

- `JIT_OP_DISPATCH`  -- kernel dispatch through `Backend.dispatch_kernel`,
  recording `(kid, in_buf_ids[], out_buf_id)`.
- `JIT_OP_ASSIGN`    -- in-place memcpy from `interact_assign_with`,
  recording `(dst_tid, src_tid)` so the replay can walk the
  `TenDesc.backend.buf_read/buf_write` round-trip exactly as the
  eager path would.

The hooks fire in `kernel_fire_by_id` (just before
`Backend.dispatch_kernel`) and `interact_assign_with` (just before
the host-side memcpy).  Both no-op when no capture is active
(`JIT_ACTIVE_SLOT == 0`), so the hot path of the eager evaluator
pays only a single integer-load branch on each fire.

## How TJit decides "first call vs replay"

A side-store `$tJitState` keyed by `Hash[closureAssoc]` records the
slot id for each captured closure.  First invocation: `MissingQ` ->
allocate slot, run fn under capture, stash the slot.  Subsequent
invocations: hash hit -> `jit_replay(slot)`.

Pros: no symbol mutation needed.  The user can keep the closure as a
bare WL value.  Hashing the whole association content (which
includes the held `Function[args, ...]`) means structurally
identical closures share state -- not a concern in practice because
each use site allocates a fresh closure.

Cons: re-creating the closure with `TJit[fn]` on every call would
cycle through capture every time.  Allocate once at use-site
top-level and reuse.

## Caveats / known limitations

- **Shape changes are silently broken.**  TJit doesn't compare input
  shapes between calls.  If the wrapped fn's GRAPH SHAPE depends on
  inputs (e.g. a Variable controlling a slice), the user must
  `TJitDrop[closure]` before re-shaping.  Phase 8 ("memory plan
  feedback") doesn't touch this; tinygrad's TinyJit handles it via
  per-shape capture sub-slots.
- **Capture buffer cap = 65,536 ops**, slot cap = 16.  Plenty for
  beautiful_mnist scale; needs a callout if a larger model exceeds.
- **Heap state still grows on capture pass.**  The first call still
  allocates intermediate TenDescs through the materialize pipeline;
  the captured kernels reference those persistent buf_ids.  Phase 8's
  per-kernel `(alloc_depth, last_use_depth)` will let the freelist
  reuse them across captures of different functions.

## Beautiful_mnist

`wl/Examples/beautiful-mnist/train.wls` is rewired with `jittedStep
= TJit[stepFn]` + per-step `TSet[imgT, ...]` / `TSet[tgtT, ...]`
into pre-allocated input slots.  The capture pass on step 1 is
still slow (per-sample 5x5 conv lowering -> 9 partials + grad
chain) -- Phase 9's im2col + sgemm replaces the partial-sum form
with a single sgemm dispatch and lifts that ceiling.  Steps 2+
replay at sub-ms speed.

## Phase 8 expectation

Phase 8 (memory plan feedback) lets the freelist coalesce per-iter
buffers across replays.  Expected drop in `TTotalBufBytes` of 2-4x
on a real training loop (matches tinygrad's "reuses ~80% of
buffers" claim).
