# Phase 6 bench — beautiful_mnist train pipeline (BS=1, tiny arch)

`wl/Examples/beautiful-mnist/train.wls` exercises the new Phase-6
ops end-to-end:

- `TConv2D` (forward) -- `Conv 1->4, 3x3` lowered as kh*kw=9 partial
  sums (current implementation; im2col + sgemm fast path is Phase 9).
- `TReLU`, `TMaxPool2d` (existing).
- `TUOpReshape` to flatten {4, 3, 3} -> {1, 36}.
- `TMatVec` + bias -> {10} logits.
- `TSparseCategoricalCrossEntropy[logits, oneHot]` -> scalar loss.
- `TGrad[loss, w]` for each of {W1, b1, W2, b2}.
- `TAdam[params, grads, m, v, t, lr, b1, b2, eps]` to apply the step
  in tensor-land via `TSet` over each weight + Adam moment buffer.

Architecture is intentionally trimmed (1 conv + 1 linear, BS=1) so a
5-step run finishes in a couple of seconds without depending on
batched conv (which lands in Phase 9 via im2col + sgemm).  The
plan's full `Conv 1->32 5 -> ReLU -> Conv 32->32 5 -> ReLU -> BN32 ->
MaxPool 2x2 -> Conv 32->64 3 -> ReLU -> Conv 64->64 3 -> ReLU ->
BN64 -> MaxPool 2x2 -> Flatten -> Linear 576->10` at BS=512 would
take hundreds of seconds per step on the current per-sample conv
lowering -- that's the Phase-9 ceiling.

## Per-step measurements (M3 Max, CPU backend, run from repo root)

In isolation each pipeline stage measured (with the same trimmed
architecture, BS=1):

| Stage                                             | Wall time |
|---------------------------------------------------|-----------|
| forward + `TRealize[loss]`                        | ~0.9 s    |
| TGrad[loss, W2] (the linear-tail weight)          | minutes   |
| TGrad[loss, W1] (through the conv lowering)       | minutes   |

The forward + loss path is fast (sub-second) because the conv
lowering only emits 9 kh*kw partials and the linear matmul lands
on `cblas_sgemm`.  The backward pass is the bottleneck: TGrad
expands the chain rule across 9 conv partials, then through ReLU
+ MaxPool (each MAX-reduce contributes a CMPEQ-mask grad), then
back through the matmul -- the resulting kernel program takes
many minutes per step on the current `cpu_interpret` path.

A 5-step end-to-end run with `N_STEPS=5
wolframscript -f wl/Examples/beautiful-mnist/train.wls` is therefore
expected to take 30+ minutes on the trimmed architecture.  Numbers
go in once Phase 7's TJit capture+replay is wired (the gradient
compute graph is identical across iterations -- only input bytes
change -- so JIT replay should drop per-step wallclock to seconds).

## What this validates

- The natural-WL ops (`Plus`, `Times`, `Power[..., -1]`, `Sqrt`, `Exp`,
  `Log`, `Total`) compose with the new layers without manual TUOp
  sprinkling.
- `TSet` (= the Set UpValue on TTerm) writes weight bytes in place
  while keeping each TenDesc id stable, so the next iteration's
  forward + backward graph rebinds against the SAME tensor handles.
  Phase 7's TJit can capture this kernel sequence verbatim.
- `TGCCollect` between steps keeps `THeapPos` bounded; without it
  the per-iter UOP graph + grad intermediates fill the 8M-cell
  semi-space within ~3 steps.

## Known limitations (cleared by later phases)

| Limitation                                                 | Phase  |
|------------------------------------------------------------|--------|
| Per-sample conv only (no batched conv)                     | 9      |
| `TGCCollect` called explicitly between steps               | 7      |
| Per-iter realize cost dominates (no graph reuse)           | 7      |
| Adam math materialises 3 fresh TenDescs per param per step | 7 + 8  |
| Conv lowering uses kh*kw partial sums (not im2col + sgemm) | 9      |
| Loss is computed but not extracted (no host round-trip)    | 7      |

## Phase 7 expectation

`@TinyJit`-style capture+replay should drop the per-step wallclock
~5-10x for this pipeline.  The kernel sequence is fully deterministic
across iterations (only the input tensor's bytes change), which is
the cleanest case for capture.
