# Phase 9 — smarter dispatch + lifetime tracking

Phase 9 in the parity plan called for four bundles:

1. matmul+bias fused dispatch
2. movement-prefix BLAS recognition (TAttention K^T)
3. conv-as-im2col + sgemm
4. lifetime tracking that ungates Phase 8's THVM_REUSE_BUFS

I worked through each and the conclusions below justify deferring
the actual code change. The state is committed in this note rather
than as new shipped code.

## (1) matmul+bias fused dispatch — small win, scheduler-bound

`TMatMul[A, B] + bias` today produces TWO kernels:

```
$ TKernelInfo on TRealize @ (TMatMul[A, B] + bias)
kid=1 (blas-gemm): MUL + REDUCE_SUM, dispatches via cblas_sgemm
kid=2 (jit):       ADD,              dispatches via the JIT'd inner loop
```

The realize-classify rule "REDUCE always realizes" creates the
boundary between matmul and the bias-add. Fusing them into a single
sgemm-with-beta-and-bias-load:

- saves one kernel dispatch per call (microseconds)
- doesn't reduce the actual sgemm work (Apple Accelerate already
  amortizes the bias-add into its per-tile epilogue when called
  with `beta != 0`, BUT only if we present it that way)

Two paths considered:

- **Scheduler relaxation**: extend Phase 3's "REDUCE doesn't realize
  when consumer chain bottoms out at EXPAND" to also cover the
  ADD-with-broadcast-bias case. Net effect: the matmul kernel
  becomes 4-op `MUL + REDUCE + EXPAND + ADD`, which `blas_try_gemm`
  doesn't recognize (it requires `n_ops == 2`) and the JIT path
  rejects (cg_supports doesn't accept post-REDUCE ops). Falls to
  interpreter -- a regression vs the current 2-kernel split.

- **BLAS-side recognition**: extend `cpu_blas_dispatch` to detect
  the 4-op `MUL + REDUCE + EXPAND + ADD` shape and call sgemm with
  `beta = 1` after pre-loading C with the broadcast bias. Doable
  but the win on hot training paths (BS=512) is < 1% because the
  per-call dispatch overhead is already negligible vs the sgemm
  itself.

Decision: defer until the inner-loop dispatch overhead becomes a
measurable bottleneck. With Phase 7's TJit replay the overhead is
already in the noise.

## (2) movement-prefix BLAS — TAttention K^T case

Today `TMatMul[Q, TUOpPermute[K, {1, 0}]]` produces a 4-op kernel:

```
op[0] = RESHAPE(in1)        -- RESHAPE of permuted K (non-contig view)
op[1] = EXPAND(op[0], ...)
op[2] = MUL(in0, op[1])
op[3] = REDUCE_SUM(op[2])
```

`blas_try_gemm` rejects (`n_ops != 2`), so this falls to the
interpreter -- the original Phase 5 limitation noted in commit
434f60d.

The fix would be to detect the RESHAPE+EXPAND prefix as pure view
manipulation and walk back to the underlying contiguous buffer of
K, then dispatch `sgemm` with `TransB = true`. Two complications:

- The kernel's input TenDesc carries the PERMUTE'd view, with
  swapped strides. `blas_try_gemm` would need to inspect
  `TENS[input_tids[i]].view.strides` to determine the right
  trans flag, then call sgemm against `TENS[input_tids[i]].buf_id`'s
  raw bytes. The general "is this view a clean transpose vs a
  more complex permutation?" check is a non-trivial axis-aware
  comparison.
- The RESHAPE op materializes through the interpreter's strided
  copy when its source is non-contig. To skip that we have to
  RECOGNIZE the kernel emitted RESHAPE-of-non-contig and assert
  the result is bytes-equivalent to the underlying buf with a
  trans flag.

Decision: defer. The TAttention K^T path runs through the
interpreter today and produces correct output -- it's just slower
than it could be. Land im2col conv first (bigger bench win) then
revisit.

## (3) conv-as-im2col + sgemm — biggest win, requires a new op

The current TConv2D lowering emits `kh*kw` partial-sum kernels (25
for a 5x5 conv). Each partial:
- SHRINK(input, all C_in, [ki, ki+H_out), [kj, kj+W_out))
- SHRINK(weights, all C_out, all C_in, [ki, ki+1), [kj, kj+1))
- EXPAND + RESHAPE + MUL + REDUCE_SUM
- Add to the running sum.

For LeNet's 5x5 conv on a 28x28 input that's 25 BLAS-gemv calls.
Apple Accelerate's per-call overhead dominates -- each gemv operates
on tiny C_in=6 columns.

Im2col + one big sgemm:
- Reshape input into shape `{C_in*kh*kw, H_out*W_out}` (the
  "lowered" matrix; each column is a flattened receptive-field
  patch).
- Reshape weights into shape `{C_out, C_in*kh*kw}`.
- One sgemm: `(C_out, C_in*kh*kw) @ (C_in*kh*kw, H_out*W_out)`
  -> `(C_out, H_out*W_out)` -> reshape to `(C_out, H_out, W_out)`.
- Add bias.

The im2col step is the new piece. Tinygrad implements it via
`Tensor._pool` which uses STACK + SHRINK + RESHAPE + STRIDE_TRICKS.
thvm doesn't have STACK, and the strided-tricks variant requires
non-trivial view-stride math that the current view system doesn't
cover (overlapping windows = view with stride < shape per axis).

Three implementation options considered:

- **New `UOP_IM2COL` opcode**: dedicated C op that does the strided
  copy directly. Cleanest performance result. Cost: new opcode + WL
  constructor + materialize handler + cpu interpreter + Metal
  shader + autograd rule (im2col grad is col2im, also non-trivial).
  Estimated ~600 LOC.

- **Pure-IR im2col via SHRINK + assemble**: walk over kh*kw,
  SHRINK each window, copy into an output slab via TAssign. Avoids
  new opcodes but emits kh*kw assigns + the matmul -- still many
  kernels. Likely SLOWER than the current partial-sum form.

- **Stride-trick view**: extend the `View` struct to allow
  per-axis stride < shape (overlapping windows), then express
  im2col as `RESHAPE -> EXPAND-with-strided-view`. The view-aware
  dispatchers (CPU interpreter, Metal pre-mat) would need to
  honor the new stride semantics. Estimated ~400 LOC + audit
  pass.

Decision: defer. The third option (stride-trick view) is the most
elegant and likely lands as part of Phase 10 (BEAM via SUP) since
that work also needs richer view semantics.

## (4) Lifetime tracking that ungates THVM_REUSE_BUFS — redundant

Phase 8's planner records per-boundary `(alloc_depth, last_use_depth)`
and pushes output bufs onto `cpu_buf_freelist` once their last
realized consumer has emitted. With `THVM_REUSE_BUFS=1` three
softmax+grad cases broke because the chain rule reads bufs through
DUP_GRAD-flagged DP0/DP1 projections that materialize in a
SUBSEQUENT thvm_realize fixed-point iteration (the planner's
within-pass push corrupts the buf before the chain rule's pass
reaches it).

The plan's proposed fix was to "defer the freelist push to
end-of-realize". Tracing through `src/schedule/realize.c` shows
that path already exists: `cpu_buf_pool_rollback_with_preserve(wm)`
runs at the end of every `thvm_realize`, walking from the begin
watermark to `CPU_BUFS_NEXT` and freelist-pushing every buf that
wasn't marked preserved by `mark_gc_preserve(res)` (which walks
the result's producer chain).

So the cross-realize savings the planner aimed at are already
provided by the existing rollback. The deltas:

- The rollback pushes **after** the realize completes, not during
  emit. Within-pass reuse is the genuine new savings.
- Within-pass reuse is exactly what corrupts under chain-rule.

Net: keeping Phase 8 gated behind `THVM_REUSE_BUFS=1` is the right
call. The planner's data (`MEM_PLAN`, `BOUNDARY_LAST_USE`) stays
useful as introspection for future tools (e.g. visualising the
realize-time memory peak in TMemoryPlanGantt). The actual
reduction in peak nbytes during a training step would come from
combining within-pass reuse with chain-rule lifetime extension --
which is a Phase 10+ design problem (the IC-flavored solution
might be using SUP/DUP with explicit lifetime annotations on the
DUP cell, so the planner can see the chain rule's reach
statically).

## What lands this phase

Nothing -- all four bundles defer. The honest accounting for the
phase is in this doc. The plan stays the same; Phase 10 (BEAM via
SUP-collapse) is up next, and it pulls in the stride-trick view
work as a prereq, which then unblocks (3).

## Phase 10 expectation

BEAM via SUP-collapse picks the kernel variant with the lowest
microsecond time among a small superposition of candidates. Per
the parity plan, the variants enumerated are (tile size, vector
width, unroll factor, threadgroup shape) -- the same axes
tinygrad's BEAM searches.

The IC-native angle: encode candidates as a SUP, dispatch + time
each, DUP-collapse the surviving variant. The collapse rewrites
the SUP head to the winner; the kernel-program hash-cons cache
keys on `(KProgOp[] hash, input shapes hash)` so subsequent calls
hit the winner directly.

Strides come in as a new candidate axis (different access patterns
for the same kernel can win at different shapes). That's where the
view richness needed for im2col lands organically.
