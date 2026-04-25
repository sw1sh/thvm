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

- [ ] confirm baseline: `make test` and the full WL suite both green at
      current HEAD; if not, that's the first thing to fix.

## Phase 1 — Adam optimizer

Mirror the SGD-as-recursive-lambda pattern in `wl/THVMLink/Tests/sgd.wlt`.
Adam keeps two extra per-parameter buffers (m, v) plus a step counter.

  w_t  = w_{t-1} - lr * m_hat / (sqrt(v_hat) + eps)
  m_t  = β1 m_{t-1} + (1-β1) g
  v_t  = β2 v_{t-1} + (1-β2) g²
  m_hat = m_t / (1 - β1^t)
  v_hat = v_t / (1 - β2^t)

- [ ] write `wl/THVMLink/Kernel/Optim.wl` with `TOptim["SGD", lr]` (just
      delegating to the existing sgd lambda) so the API surface exists.
- [ ] add `TOptim["Adam", lr, β1, β2, ε]` returning the recursive lambda
      term that takes (w, m, v, t, n_steps_remaining) -> w_final.
- [ ] add `wl/THVMLink/Tests/optim.wlt` covering one-step + two-step
      Adam against a hand-computed expected value on a tiny quadratic
      loss.

## Phase 2 — NN layer expansion

Audit `wl/THVMLink/Kernel/NN.wl` first to see what's there. Add what's
missing for LeNet (Conv2D, MaxPool2D, Flatten, Softmax, CrossEntropy).
Each layer is its own `[ ]` item. Each must come with a numeric test in
`nn.wlt` checked against a hand-computed reference (or against
`NetTrain` for a single forward pass).

- [ ] audit existing NN.wl; list missing layers in this section as
      `[ ]` sub-items.

## Phase 3 — NetModel → TTerm converter

There's already a `Wolfram layer -> TUOp converter` per the git log
(commit 54e716e). Extend it to handle LeNet end-to-end.

- [ ] inspect `NetModel["LeNet"]["Layers"]` and list each layer type
      that needs converter support.
- [ ] for each missing layer type, add a converter clause + a test
      that round-trips the layer through the converter and checks
      forward-pass numerics against `NetApply`.

## Phase 4 — MNIST loader

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
