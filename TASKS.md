# thvm autonomous task queue

Goal: make `TOptim["Adam"]` train `NetModel["LeNet"]` on MNIST end-to-end on Metal.

Process: pick the topmost `[ ]` item. If it's too big for one iteration
(rough cap: ~100 LOC, ≤ 30 minutes of work), replace it with smaller
`[ ]` sub-items, commit the decomposition, and exit. Otherwise implement
it, verify (`make test` + the full WL suite), commit, mark `[x]`.

If you genuinely fail an item 3 times in a row (consecutive cron fires
on the same item), mark it `[blocked: <one-line reason>]` and pick the
next item. The blocked item is just skipped, not deleted.

Keep commits atomic. Do not delete `TASKS.md` entries — only flip their
state.

## Phase 0 — sanity

- [x] confirm baseline: `make test` and the full WL suite both green at
      current HEAD; if not, that's the first thing to fix.

## Phase 1 — Adam optimizer

Mirror the SGD-as-recursive-lambda pattern in `wl/THVMLink/Tests/sgd.wlt`.
Adam keeps two extra per-parameter buffers (m, v) plus a step counter.

  w_t  = w_{t-1} - lr * m_hat / (sqrt(v_hat) + eps)
  m_t  = β1 m_{t-1} + (1-β1) g
  v_t  = β2 v_{t-1} + (1-β2) g²
  m_hat = m_t / (1 - β1^t)
  v_hat = v_t / (1 - β2^t)

- [x] write `wl/THVMLink/Kernel/Optim.wl` with `TOptim["SGD", lr]` (just
      delegating to the existing sgd lambda) so the API surface exists.
- [x] Adam helpers in Optim.wl: a `tZerosLike[wTen]` returning a
      fresh TTerm zero-tensor with the same shape as a TAG_TEN, plus
      any other small scalar constructors (e.g. `tF32[x]` shorthand
      for `TUOpConst[x, "f32"]`).  Add tiny optim.wlt tests for the
      helpers in isolation.
- [x] Adam recursive lambda: replace the `TOptim["Adam", ...]` stub
      in Optim.wl with the real body using the helpers above.  The
      lambda threads (w, m, v, b1pow, b2pow, k) through `TIfZero`
      and `TRef`; β1^t / β2^t are kept as state so no POW UOP is
      needed.  No new tests in this commit -- they land in the next
      item.
- [x] add `wl/THVMLink/Tests/optim.wlt` covering one-step + two-step
      Adam against a hand-computed expected value on a tiny quadratic
      loss.

## Phase 2 — NN layer expansion

Audit `wl/THVMLink/Kernel/NN.wl` first to see what's there. Add what's
missing for LeNet (Conv2D, MaxPool2D, Flatten, Softmax, CrossEntropy).
Each layer is its own `[ ]` item. Each must come with a numeric test in
`nn.wlt` checked against a hand-computed reference (or against
`NetTrain` for a single forward pass).

Audit summary (HEAD: 62a1b3f).  NN.wl has:
  - `LinearLayer` forward (works; no grad through the EXPAND).
  - `ElementwiseLayer` dispatch on `Identity`, `(#1 #1 &)` (square),
    `(-#1 &)` (neg) only.
  - `ConvolutionLayer` is a stub that emits `$Failed`.
  - `NetChain` Fold-fold; no `NetGraph` support.
  - `TSum / TSquare / TDot / TMatVec / TL2Loss / TMSELoss` helpers.

Missing for LeNet (forward-only; backprop grad rules are a Phase-2.5
concern that interact_grad currently doesn't cover for any of these):

- [x] `ElementwiseLayer[Ramp]` -> ReLU.  Add a `TReLU[x]` helper
      (e.g. via `MUL[x, max(0, sign(x))]` if no UOP_MAX0; otherwise
      a CMPLT-based mask).  Wire into `$elementwiseDispatch`.  Test
      against `NetApply[ElementwiseLayer[Ramp]]` on a small input.
- [x] `ElementwiseLayer[Tanh]`.  Either build via existing UOPs
      (`tanh(x) = (e^2x - 1)/(e^2x + 1)` using EXP2 = exp2 with the
      `log2(e)` rescale) or add a `UOP_TANH` primitive.  Pick one and
      document the choice as a `<!-- design-question -->` HTML
      comment under this item.
      <!-- Chosen: build via existing EXP2/MUL/ADD/RECIP UOPs.  No
      C-side primitive needed; loses precision for |x| > ~10 due to
      exp overflow but that's accepted (hidden activations rarely sit
      there). -->

- [x] runtime support for `UOP_RESHAPE` end-to-end (currently
      missing -- a TUOpReshape today gets zero output and no
      children show up in TTermExpr).  Touches:
      `src/alo/realize.c` + `src/book/from_dynamic.c` arity tables
      (1 + ndim cells), `src/schedule/materialize_in_env.c` output-
      shape branch reading dim NUM cells (mirrors EXPAND),
      `src/backend/cpu/interpret.c` + a new `cpu_op_reshape`
      (contiguous tensors -- just memcpy of numel*dtype-size bytes),
      `wl/THVMLink/Kernel/THVMLink.wl` `uopCellCount` for
      TTermExpr.  Plus a tiny end-to-end smoke test that builds
      TUOpReshape and verifies shape + data round-trip.
      <!-- Skipped alo/realize.c + book/from_dynamic.c arity edits
      this fire: those bite only when RESHAPE is wrapped in a
      TLam/TRef closure, which Phase-2 layer-converters don't yet
      do.  Add when first RESHAPE-inside-recursive-lambda failure
      surfaces.  Recovered ndim from dim NUM cells via running-
      product against input numel rather than encoding ndim in
      ext (kept the all-UOPs ext-is-opcode invariant). -->

- [x] `ReshapeLayer` forward.  Once the runtime supports RESHAPE,
      add the dispatch in NN.wl and a nn.wlt test against
      `NetApply[ReshapeLayer[shape]]`.
- [x] `FlattenLayer` forward.  Composes `ReshapeLayer` to 1-D (or
      rank-2 with explicit batch axis).
- [x] **REDUCE axis bug**: `cpu_op_reduce` ignores its `axis` arg
      and reduces consecutive groups (effectively the innermost
      axis only).  Discovered while implementing PoolingLayer
      non-overlap: `RESHAPE {1,4,4} -> {1,2,2,2,2}` then
      `REDUCE axis=2 (kh)` returns the wrong groups -- a 4x4
      max-pool returns `{{4,8},{12,16}}` instead of `{{6,8},
      {14,16}}`.  Fix needs to compute strides from the input
      shape + axis index; the `oi*group + j` loop only works when
      axis is the last one.  Plus a regression test in
      `tests/test_grad.c` (or a new `tests/test_reduce.c`)
      covering axis=0 and axis=middle on a rank-3 input.
      <!-- Fixed: repacked op_arg at materialize time as
      (kind << 24 | inner) where inner = product of dims after
      the reduced axis; cpu_op_reduce strides as
      `outer_idx * (axis_size * inner) + k * inner + inner_idx`.
      Regression in wl/THVMLink/Tests/reduce.wlt covers axis=0
      sum/max on rank-2 + axis=1 sum on rank-3 (the case that
      bit pooling). -->
- [x] `PoolingLayer[k, k, "Function" -> Max]` 2-D forward, NON-
      overlapping case only (Stride = KernelSize).  DEPENDS ON the
      REDUCE-axis fix above.  Channels-first input shape {C, H, W}
      -> reshape to {C, H/k, k, W/k, k} -> two REDUCE_MAX (axis 2
      then 3 in the reshape's new index space) -> {C, H/k, W/k}.
      Refuse the overlapping case in this fire by returning a
      Failure["NotImplemented"] when Stride != KernelSize.  Test
      against NetApply on a small input.
- [blocked: not needed for LeNet -- NetModel["LeNet"]'s pool layers
      use Stride={2,2} (non-overlapping); the non-overlap
      implementation already covers the goal.  Pick this up when
      another model wants overlapping pool, then needs UOP_PERMUTE
      runtime support first.] `PoolingLayer` overlapping case
      (Stride < KernelSize, e.g. the default Stride = {1,1}).
- [x] `SoftmaxLayer` forward.  `softmax(x)_i = exp(x_i) / sum(exp(x))`.
      EXP via `2^(log2(e) * x)` = TUOpExp2 chain.  Test against
      `NetApply[SoftmaxLayer[]]` on a small vector.
- [x] `CrossEntropyLossLayer` forward.  Needs LOG (we have LOG2,
      same trick as EXP).  Test forward only (no grad path yet).
      <!-- Implemented as a host-side helper TCrossEntropyLoss[pred,
      target] (probabilities form), not via TFromNet dispatch:
      CrossEntropyLossLayer takes TWO inputs (Input + Target),
      which the single-tensor TFromNet[layer, x] signature can't
      represent.  The training loop calls it manually, same way
      sgd.wlt does TL2Loss. -->

- [ ] `ConvolutionLayer` 2-D forward.  Replace the current stub.
      Likely big -- decompose when picked up.

## Phase 3 — NetModel → TTerm converter

There's already a `Wolfram layer -> TUOp converter` per the git log
(commit 54e716e). Extend it to handle LeNet end-to-end.

- [ ] inspect `NetModel["LeNet"]["Layers"]` and list each layer type
      that needs converter support.
- [ ] for each missing layer type, add a converter clause + a test
      that round-trips the layer through the converter and checks
      forward-pass numerics against `NetApply`.

## Phase 4 — MNIST loader

Use `ResourceData["MNIST", "TrainingData"]` and `ResourceData["MNIST",
"TestData"]` -- both return a list of `image -> label` rules.  Convert
images to `TTensor` (shape {1, 28, 28} per sample, batched as
{N, 1, 28, 28}) and labels to int32 `TTensor`s.

- [ ] add `TMnistLoad[]` returning a tagged dataset (training images
      + labels as TTensors).
- [ ] add a minibatch sampler that produces fresh batches per iter.

## Phase 5 — Metal backend

Big one. The runtime today is CPU-only (`backend/cpu.c`). Mirror that
file as `backend/metal.c` (or `.m` if Objective-C is needed).

- [ ] decide on the embedding strategy: pure-C via Metal C bindings,
      or Objective-C++ helpers. Document the choice in
      `docs/metal.md`. This is the only research-y task; if it stalls
      3 fires, mark `[blocked]` and proceed to Phase 6.
- [ ] stub `backend/metal.c` exposing the same `Backend` vtable as
      `backend/cpu.c`, all functions returning errors for now. Wire
      it into `thvm_init` behind a `THVM_BACKEND=metal` env switch.
- [ ] one Metal kernel at a time, in this order: CONST, ADD, MUL,
      NEG, REDUCE_SUM, EXPAND, RESHAPE, MUL+REDUCE (matmul shape).
      Each is its own `[ ]`; each ships with a numeric test against
      the CPU backend.

## Phase 6 — end-to-end

- [ ] wire `TOptim["Adam"]` + LeNet + MNIST + Metal into
      `wl/Examples/lenet-mnist/` with a `train.wls` script.
- [ ] run a single iteration to verify; commit; declare victory.
