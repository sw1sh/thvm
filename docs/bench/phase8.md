# Phase 8 bench — memory plan feedback (gated)

Phase 8 wires per-kernel `(alloc_depth, last_use_depth)` into the
emit loop and pushes a kernel's output buf onto `cpu_buf_freelist`
once every later-depth boundary has emitted, so subsequent
allocations within the same materialize pass can pop instead of
calloc'ing fresh.

The plumbing is in place, gated behind `THVM_REUSE_BUFS=1` (default
off).  The grid is green either way (404/404).

## Why default-off

The conservative analysis baked in (`BOUNDARY_LAST_USE` over realized
parents only) is:
- **Correct for forward-only flat graphs.**  Each consumer is a
  realized boundary at known depth; `last_use` is the max parent
  depth.
- **Unsafe for chain-rule + Phase-3 fusion-relaxation graphs.**  A
  boundary can be consumed by:
  - A Phase-3-relaxed REDUCE that's no longer a realize boundary
    (DUP/SUP-aware lifetime walk needed).
  - A `UOP_ASSIGN` reading the buf for its in-place write -- ASSIGN
    fires during WNF, not during dispatch, and isn't reflected in
    `BOUNDARY_LAST_USE`.
  - A future-pass UOP that the chain rule emits during WNF expansion
    (`thvm_realize` loops materialize+wnf to fixed point).

Repro on `softmax-cross-entropy-equals-probs-minus-target`:

```
$ THVM_NO_REUSE=1 make wl-test  (default)
404 passed, 0 failed

$ THVM_REUSE_BUFS=1 make wl-test (opt-in)
3 failed -- softmax-cross-entropy + lenet end-to-end + softmax sum
```

The 3 failures all reuse a 1-float scalar buf (a REDUCE_SUM result)
that the chain rule re-references through DUP_GRAD-flagged
projections after the kernel emit pass completes.  `mem_plan_drain_freelist`
at end-of-pass mitigates inter-pass leak; the within-pass corruption
remains.

Phase 9's lifetime tracking will need:
- DUP/SUP traversal (the Phase-3 relaxation path).
- `UOP_ASSIGN`-aware lifetime extension.
- A "frozen lifetime" check before push (don't push if any non-emit
  consumer reads via SUB-bit chains).

## What does land

Code paths that ARE safe with `THVM_REUSE_BUFS=1`:
- 64x64 matmul + bias × 50 iters under TJit -- the captured kernel
  sequence is stable, replays use stable buf_ids.  Bench numbers
  unchanged from Phase 7 (the bottleneck was already the scheduler,
  not allocation).
- The simpler benches in `docs/bench/phase7.md` -- no regression.

The conv lowering (`Fold[Plus, partial_n]`) collapses every
intermediate sum into a single fused kernel (the realize pass
inlines them as program ops).  All partials end up with
`last_use = the-realized-final-sum's-depth`, which is the LAST
boundary in the topo order, so `mem_plan_push_dead` never fires
during the emit loop.  The peak buf count for a 28x28 conv is
unchanged whether reuse is on or off.

## Phase-9 path

Two parts (per the parity plan):
1. **Smarter dispatch** -- matmul+bias fused, conv-as-im2col,
   movement-prefix BLAS recognition.
2. **Lifetime tracking that handles the corner cases above.**  Once
   that's in, flip `THVM_REUSE_BUFS` to default-on and remove the
   gate.

The bench claim ("2-4x peak buffer drop") that motivated Phase 8
needs both the reusable-output identification (which we have) AND
boundaries actually emitting at multiple depths past those
last_uses.  In thvm's current scheduler, ELEMENTWISE chains fuse
into one big kernel and don't expose multi-depth boundaries; the
2-4x happens specifically when the chain rule emits separate
forward+backward kernels that share intermediate buffers.  Phase 9's
batched-conv via im2col + sgemm changes the emit shape enough
to expose those boundaries; benching at THAT point should show
the predicted savings.
