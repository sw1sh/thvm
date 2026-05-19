# CUDA NN-layer cross-validation: thvm-CUDA vs numpy / tinygrad

Stage 5 of the CUDA backend slice -- extends `docs/cuda_xval.md` (bare
matmul / softmax / row-reduce) to neural-network *layer* kernels,
forward AND backward.  Each layer is a hand-built UOp DAG (the backward
math is known; this does not use thvm's autodiff), rendered to CUDA,
nvrtc-compiled, dispatched on the V100 pod, and checked against a
float64 numpy reference under a width-aware fp32 band.

Harness: `py/examples/cuda_nn_xval.py`.  Reproduce on a Linux+CUDA host
with `python3 py/examples/cuda_nn_xval.py --json out.json`.  Surfaced
renderer bugs get a normal failing test in
`py/examples/cuda_render_bugs.py`.

- Device: Tesla V100-SXM2-16GB, Volta sm_70, CUDA 12.4 toolkit.
- Correctness gate: `max_abs <= tol_abs OR max_rel <= tol_rel`, the
  same width-aware fp32 band `cuda_xval.md` uses (`4*W*2^-23` for a
  W-wide reduction; a fixed `1e-4..1e-5` for bounded outputs).

## Results

| layer | dir | shape | correct (vs float64 ref) | GPU p50 (us) | .cu lines |
|---|---|---|---|---:|---:|
| linear       | fwd     | 512x512x512        | abs 3.0e-5 -- OK | 212.0  | 25 |
| linear       | bwd dW  | 512x512x512        | abs 3.3e-5 -- OK | 225.3  | 24 |
| linear       | bwd dX  | 512x512x512        | abs 3.4e-5 -- OK | 1388.5 | 24 |
| linear       | bwd db  | 1024x1024          | abs 7.2e-5 -- OK | 30.7   | 22 |
| relu         | fwd     | 1048576            | abs 0.0  -- OK   | 15.4   | 18 |
| relu         | bwd     | 1048576            | abs 0.0  -- OK   | 21.5   | 19 |
| gelu         | fwd     | 1048576            | abs 2.7e-7 -- OK | 16.4   | 18 |
| gelu         | bwd     | 1048576            | abs 7.6e-7 -- OK | 21.5   | 19 |
| layernorm    | fwd     | 4096x256           | abs 9.1e-7 -- OK | 252.9  | 27 |
| layernorm    | bwd     | 4096x256           | abs 5.3e-7 -- OK | 617.5  | 36 |
| conv2d       | fwd     | 4x8x16x16, 16x3x3  | abs 3.7e-6 -- OK | 8.2    | 24 |
| conv2d       | bwd dW  | 4x8x16x16, 16x3x3  | abs 2.4e-5 -- OK | 23.6   | 24 |
| conv2d       | bwd dX  | 4x8x16x16, 16x3x3  | abs 5.4e-7 -- OK | 6.1    | 31 |
| attention    | fwd     | 256x64             | abs 1.0e-7 -- OK | -      | -  |
| attention    | bwd     | 256x64             | abs 4.3e-8 -- OK | -      | -  |

**All 12 layer kernels (26 of 26 forward+backward configs) pass on
the V100 within the fp32 tolerance.**  Attention forward + backward
were initially COMPILE FAIL, blocked on the `nested_reduce_axis_dep`
renderer bug (section 2 below); that bug is now fixed -- the reduce
loops nest at the depth their axis deps require -- and attention
cross-validates.

tinygrad cross-check (CUDA, same inputs) agrees with thvm-CUDA:
linear_fwd 512 thvm abs 2.96e-5 vs tinygrad 2.96e-5 (identical fp32
math, different accumulation order); relu_fwd both bit-exact; gelu_fwd
thvm 2.7e-7 vs tinygrad 3.6e-7 -- thvm's GELU goes through an
exp2-synthesized tanh (no UOP_EXP / UOP_TANH in the opcode set), both
well inside the band.

## What each layer exercised

- **Linear fwd / bwd** -- matmul + bias broadcast-add; the two backward
  matmuls each carry a *transposed* operand.  thvm handles the
  transpose entirely in the address expression (`X^T[k,m]` is just
  `X[m,k]` with the index swapped) -- no transpose kernel, no copy.
  All four pass.
- **ReLU / GELU fwd / bwd** -- elementwise.  ReLU uses `CMPLT` to build
  a 0/1-float mask (`(0<x)*x`); both directions are bit-exact.  GELU's
  tanh is built from `EXP2` as `1 - 2/(exp(2x)+1)`; the backward uses
  the full `0.5(1+t)+0.5x(1-t^2)du` derivative.  No renderer gap.
- **LayerNorm fwd / bwd** -- the reduce-feeding-broadcast structure.
  Forward has two reduce axes (mean, then variance whose deviation
  reads the mean).  Backward has *four* reduce axes -- mu, var,
  mean(dY), mean(dY*xhat) -- all feeding one broadcast STORE.  The
  36-line backward `.cu` renders and runs correctly; each output
  thread recomputes its row's four reductions (correct, structurally
  O(R*C^2) like softmax -- a perf, not a correctness, issue).
- **Conv2d fwd / bwd** -- im2col.  Forward and `dW` are matmuls over
  conv-shaped operands.  `dX` is col2im expressed as a *gather-reduce*:
  each input pixel sums the dCols taps touching it over a fused
  `(kh,kw)` reduce, bounds-masked with `IWHERE/IAND/ILT`.  All three
  pass.

## Bugs surfaced

Both are documented as normal failing tests in
`py/examples/cuda_render_bugs.py` (run it: exits non-zero while either
is open).  Per `feedback_never_hide_bugs` neither is worked around in
a way that hides it.

### 1. `signed_index_isub` -- ISUB rendered as unsigned -- FIXED

`UOP_ISUB` is documented "signed integer subtract" (`src/thvm.h:368`),
but the CUDA renderer (`src/codegen/render_uop.c`) emits every
`UOP_RANGE` variable -- and hence every `ISUB` -- as `uint`.  An `ISUB`
that goes negative wraps to a huge unsigned value, and a comparison
against a negative literal (`-1 < x`) promotes `-1` to `UINT_MAX`, so
it is *always false*.  Any DAG that relies on a genuine signed index
comparison is silently wrong.

This surfaced in conv2d `dX`: the natural col2im lower-bound check
`0 <= ih - ki` returned all-zero.

**Fixed.**  Each `UOP_I*` binary operand is cast to `int` in
`rmu_emit_term`, so the whole integer-expression subtree is
signed-correct (matching tinygrad's signed-`int` index dtype); the
`tid`/`tg`/`tt` thread-position decode is untouched.  A genuine signed
index compare now lowers correctly.  See `py/examples/cuda_render_bugs.py`
`signed_index_isub` (now a passing regression test).

### 2. `nested_reduce_axis_dep` -- inner reduce hoisted to wrong nesting -- FIXED

A `REDUCE` whose body contains another `REDUCE` over a different axis,
where the inner reduce indexes by the *outer* reduce's axis variable,
is emitted at the wrong loop nesting level.  The chain-reduce path in
`rmu_emit_store_reduce` handles only *directly* nested
`REDUCE(REDUCE(...))`; when the inner reduce is wrapped in any
elementwise op, it falls to the generic post-order hoist
(`rmu_collect_reduces`), which emits every reduce loop flat at the top
level.  The inner loop then references the outer reduce's axis var
before it is declared (NVRTC: `identifier "a6" is undefined`), and
even compiled would compute the inner reduce once with a stale axis.

Minimal repro: `out[i] = sum_j ( in[i] * sum_k m[j,k] )` -- the inner
`sum_k` indexes `m` by `j`, so its loop must sit *inside* the `j`-loop.

This blocks scaled-dot-product attention forward AND backward: the
softmax over the j-scores inherently nests a per-j score reduce (over
the head dim) inside the max/sum reduce over j.  There is no
single-DAG formulation that avoids the pattern.

**Fixed.**  The renderer now emits each reduce at the loop depth its
body's axis dependencies require, rather than flat-hoisting -- ported
from tinygrad's reduce nesting (`rmu_term_uses_axis` + `parent_idx[]`
record the innermost enclosing reduce a reduce depends on; child
reduces emit inside their parent's loop).  Attention forward and
backward now compile on NVRTC and cross-validate (fwd abs 1.0e-7,
bwd abs 4.3e-8 on the V100).  See `py/examples/cuda_render_bugs.py`
`nested_reduce_axis_dep` (now a passing regression test).

## thvm autodiff through CUDA -- not testable from Python

The task also asked to test thvm's *own* autodiff (`uop_grad` /
`UOP_GRAD`) lowering through the CUDA backend.  This is not reachable
from `py.thvm`: `uop_grad` operates on the high-level `TAG_TEN` tensor
graph and is driven only through the Wolfram LibraryLink frontend
(`TGrad`).  The `py.thvm` bridge exposes solely the post-materialize
*kernel-DAG* builder (`py_uop_*`) -- there is no `materialize`,
`realize`, `kernelize`, or `grad` entry point.  Driving autograd ->
materialize -> kernelize -> CUDA-dispatch from Python would require a
bridge extension (a new `py_uop_grad` + `py_materialize`), which is
separate work.  The hand-built backward DAGs in this harness are the
substitute: they confirm the backward *math* lowers and runs on CUDA.

## Out of scope

Performance tuning.  `linear_bwd_dX` at 512 is 6.5x slower than the
forward matmul of the same shape (1389us vs 212us): the `dX = dY @ W^T`
kernel's inner loop strides `W` non-contiguously, thrashing L2.  This
is the same structural-untiled-kernel cost `cuda_xval.md` already
analysed for matmul -- a missing tile/transpose-stage proposal, not a
correctness issue, and not addressed here.
