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

- [x] add `UOP_CONV2D` opcode + `uop_conv2d` constructor in
      `src/uop/conv2d.c` -- heap layout `[input, weights, bias]`
      with stride / kernel size encoded in `arg`.  Plus thvm.h
      opcode constant.  Mirrors how existing UOPs are wired.
      <!-- Skipped the `arg` encoding -- recover kernel size from
      weights.shape at materialize time.  Construction stays
      shape-agnostic. -->

- [x] plumb UOP_CONV2D through `materialize_in_env.c` (output
      shape calc: {C_out, H_out, W_out} for valid conv2d) and
      `wl/THVMLink/Kernel/THVMLink.wl` `uopCellCount`.  Plus the
      `$Uop*` constant and `$uopNames` entry.
      <!-- Bumped MAX_UOP_SRC 2 -> 3 (CONV2D needs input/weights/
      bias).  Added uop_arity (3), alo_node_arity (3), dyn_arity
      (3) cases.  Output shape derived from weights.shape (C_out
      = dim 0, kh = dim 2, kw = dim 3) -- the constructor stays
      shape-agnostic, materialize discovers everything from the
      input shapes. -->

- [x] `cpu_op_conv2d` kernel in `src/backend/cpu/op/conv2d.c`:
      stride-1, no-padding, channels-first {C_in, H, W} input;
      weights {C_out, C_in, kh, kw}; bias {C_out}.  Output
      {C_out, H-kh+1, W-kw+1}.  Standard 6-loop nested impl.
      Wire into `interpret.c` dispatch.  Add a smoke test in
      tests/test_uop.c (or a new test_conv2d.c).
      <!-- arg packing: bits 24..31 = kh, 16..23 = kw, 0..15 = W_out;
      kernel reverse-derives C_in / H_out / H from src_numels.
      Materialize-shape regression in test_materialize.c covers
      output {2,3,3} for {1,5,5} input + {2,1,3,3} weights.  End-
      to-end numeric test lands with the WL constructor in the
      next item -- writing input data + reading output buffer
      from C is end-to-end-y enough that going through TRealize
      is shorter. -->

- [x] WL `TUOpConv2D[input, weights, bias, kSize]` constructor +
      `fromLayer[ConvolutionLayer, ...]` dispatch + nn.wlt test
      against `NetApply` on a small initialised conv layer.
      <!-- TUOpConv2D drops the kSize arg (recovered from
      weights.shape at materialize time, same as the C
      constructor).  Cross-check via NetApply hit
      NetChain::badbackend in the local Mathematica runtime, so
      the numeric reference is a hand-derived 1-ch 2-outch 2x2
      kernel example instead.  Dispatch test verifies output
      shape end-to-end. -->

<!-- design-question: chose UOP_CONV2D primitive over the
     im2col-via-PERMUTE route because PERMUTE / SHRINK don't
     have runtime support yet and would require ~5 file edits
     each before im2col could even be assembled.  A primitive
     ships sooner and matches what production runtimes do for
     conv anyway (e.g. tinygrad's CONV2D is a primitive UOp). -->

## Phase 3 — NetModel → TTerm converter

There's already a `Wolfram layer -> TUOp converter` per the git log
(commit 54e716e). Extend it to handle LeNet end-to-end.

- [x] inspect `NetModel["LeNet"]["Layers"]` and list each layer type
      that needs converter support.
      <!-- Audit result: every LeNet layer (ConvolutionLayer,
      ElementwiseLayer Ramp, PoolingLayer Max non-overlap,
      FlattenLayer, LinearLayer, SoftmaxLayer) has a converter
      clause from Phase 2.  Tried TFromNet on the full chain on
      synthetic input -- two real integration gaps surfaced (see
      sub-items below). -->
- [x] **WL-side shape inference for UOP graphs** so layer dispatch
      can size operations without hitting TAG_TEN.  The current
      `PoolingLayer` / `FlattenLayer` reads of `TTensorShape[x]`
      fail when `x` is an intermediate UOP (e.g. the post-ReLU
      output of a conv).  Options:
      (a) thread shape info host-side through the NetChain Fold
          (each fromLayer takes `(layer, x_TTerm, in_shape)` and
          returns `{out_term, out_shape}`); or
      (b) add a `TUOpShape[x_TTerm]` walker that traces the UOP
          graph in WL and returns the result shape statically; or
      (c) materialize at every step (slow; rebuilds kernels too
          eagerly).
      Pick one and document.
      <!-- Picked option (b): added `tUopShape` in Shape.wl --
      a static walk over heap cells mirroring
      materialize_in_env's output-shape branch.  Covers TEN,
      KERNEL, CONST, ADD/MUL/CMPLT, NEG/RECIP/EXP2/LOG2/SQRT,
      REDUCE, RESHAPE, EXPAND, CONV2D.  7 isolated tests in
      shape.wlt.  Layer dispatchers will be migrated in a
      follow-up item below if the LeNet smoke test reveals they
      still need it. -->
- [x] migrate `PoolingLayer` / `FlattenLayer` dispatchers in
      NN.wl to use `tUopShape` instead of `TTensorShape` so they
      work on intermediate UOP terms in a chain.
      <!-- Also fixed `asRowVec` (LinearLayer's rank-1 promoter):
      switched it to `tUopShape` + `TUOpReshape` so it stops
      doing a TTensorData copy that only worked on TAG_TEN.
      Smoke probe: a small Conv->ReLU->MaxPool->Flatten chain on
      {1,6,6} input now runs through TFromNet end-to-end,
      producing the expected {8} output. -->

- [x] **NetModel["LeNet"] weight extraction**.  Probe shows the
      NetModel-loaded layer's "Weights" comes back as
      `Automatic` (paclet version 15.0.2 vs runtime 15.0.3
      mismatch warning was printed at load).  Either re-train /
      re-save in the local version, fall back to NetInitialize
      with the published architecture, or load weights from a
      separate file.  Pick the lowest-friction option.
      <!-- Picked option (b): NetInitialize the canonical LeNet
      architecture in `TLeNet[]`.  Random weights are correct
      for the goal anyway -- TOptim["Adam"] training starts from
      scratch.  TLeNet[] returns NetChain with 11 layers, all
      weights as concrete NumericArray. -->

- [x] end-to-end TFromNet on LeNet: build it from the official
      architecture (with proper weights from the prior item),
      feed a synthetic 1x28x28 input, verify TRealize produces a
      length-10 probability vector that sums to 1.
      <!-- TLeNet[] + TFromNet runs the full chain (Conv->ReLU->
      MaxPool->Conv->ReLU->MaxPool->Flatten->Linear->ReLU->
      Linear->Softmax) on synthetic {1,28,28} input, returns
      shape {10} that sums to exactly 1.0.  Phase 3 closes. -->


## Phase 4 — MNIST loader

Use `ResourceData["MNIST", "TrainingData"]` and `ResourceData["MNIST",
"TestData"]` -- both return a list of `image -> label` rules.  Convert
images to `TTensor` (shape {1, 28, 28} per sample, batched as
{N, 1, 28, 28}) and labels to int32 `TTensor`s.

- [x] add `TMnistLoad[]` returning a tagged dataset (training images
      + labels as TTensors).
- [x] add a minibatch sampler that produces fresh batches per iter.
      <!-- TMnistBatch[n] / TMnistBatch[n, "test"]: returns
      <|"images" -> TTerm{n,1,28,28}, "labels" -> TTerm{n}|>.
      Calls ResourceData internally each time -- ResourceData
      caches the underlying download/decode, so per-call cost
      is dominated by the n random samples + tensor allocation.
      4 mnist.wlt tests cover both splits, randomness across
      consecutive draws, and Failure on bad split arg. -->


## Phase 5 — Metal backend

Big one. The runtime today is CPU-only (`backend/cpu.c`). Mirror that
file as `backend/metal.c` (or `.m` if Objective-C is needed).

- [x] decide on the embedding strategy: pure-C via Metal C bindings,
      or Objective-C++ helpers. Document the choice in
      `docs/metal.md`. This is the only research-y task; if it stalls
      3 fires, mark `[blocked]` and proceed to Phase 6.
      <!-- Decision: Objective-C `.m` files exposing a C API,
      mirroring `backend/cpu/`.  Metal-cpp drags in C++ runtime;
      direct objc_msgSend is fragile.  MSL shaders compile via
      `xcrun metal` to a default.metallib loaded at backend init.
      Backend selection via THVM_BACKEND=metal env var.  Full
      design in docs/metal.md. -->

- [x] stub `backend/metal.c` exposing the same `Backend` vtable as
      `backend/cpu.c`, all functions returning errors for now. Wire
      it into `thvm_init` behind a `THVM_BACKEND=metal` env switch.
      <!-- Lives at src/backend/metal/_.c (plain C until the .m
      glue lands).  init/shutdown succeed (no resources yet);
      buf_alloc returns 0 (no-buffer sentinel); buf_read/write/
      dispatch_kernel return -1 with a stderr note on dispatch.
      thvm_init reads THVM_BACKEND env var; "metal" picks the
      stub, anything else (including unset) defaults to CPU.
      tests/test_metal_stub.c covers the swap. -->

- [x] **Metal build pipeline**.  Makefile rules to compile
      `src/backend/metal/_.m` with `clang -fobjc-arc` into
      `build/backend_metal.o`, Darwin-gated.  Link `-framework
      Metal -framework Foundation` into every test binary and
      the WL dylib.  Use `THVM_HAS_METAL` define so `src/thvm.c`
      stops `#include`-ing the C stub when the .m is linked in.
      Deliverable: `make` and `make test` still green, plus a
      new `bin/test_metal_real` that #defines THVM_HAS_METAL +
      links the .o to confirm the dual-TU build works.
      <!-- WL dylib link with the .o is deferred -- it's CPU-
      only today and the Metal init+kernels aren't reachable from
      the WL surface yet.  Wire when the first real Metal kernel
      lands.  test_metal_real covers the dual-TU shape. -->

- [x] **Real metal_init in _.m**.  Convert the stub in
      `src/backend/metal/_.m` (currently created by the prior
      item) into one that opens `MTLCreateSystemDefaultDevice()`
      + a `MTLCommandQueue`, stores both in file-scope statics,
      and `metal_shutdown` releases.  Buffer / dispatch ops stay
      stubbed (they ship in the next two items).  Test:
      `THVM_BACKEND=metal` thvm_init returns 0 with the device
      name printable to stderr (confirms the framework wiring).
      <!-- Apple M3 Max device opens and prints to stderr;
      MTLCommandQueue created on top.  ARC owns both via static
      file-scope handles, nilled in metal_shutdown.  Test cycles
      init/shutdown twice to verify clean re-open. -->

- [x] **MSL shader compilation pipeline**.  Makefile rule that
      runs `xcrun -sdk macosx metal -c src/backend/metal/shaders/
      *.metal -o build/shaders.air` then `xcrun -sdk macosx
      metallib build/shaders.air -o build/default.metallib`.
      Empty shaders directory ships with a single trivial
      shader (e.g. a no-op kernel) so the rule has something to
      compile.  metal_init loads the metallib via
      `[device newLibraryWithFile:...]`.  This unblocks the per-
      kernel items below by giving them a place to drop shader
      sources.
      <!-- Per-shader .metal->.air via xcrun, all .air linked
      into build/default.metallib.  Path threaded into _.m via
      -DTHVM_METAL_METALLIB= at compile time so callers can
      override.  metal_init now reports loaded function count
      to stderr ("1 function" with the placeholder).  Switched
      to newLibraryWithURL:error: per the macOS 13+
      deprecation. -->

- [x] **Metal buffer ops**.  `metal_buf_alloc` returns an
      `id<MTLBuffer>` (cast through u32 buf_id); `metal_buf_free`
      releases it.  `metal_buf_write` / `metal_buf_read` blit
      between host bytes and the buffer.  Test: parity with the
      CPU backend on a write-then-read round-trip.
      <!-- Implemented as a parallel METAL_BUFS table indexed by
      u32 buf_id (mirrors CpuBuf table in cpu/init.c).  Each
      slot holds an id<MTLBuffer> + capacity + refcount.  ARC
      owns the MTLBuffer reference.  buf_alloc uses
      MTLResourceStorageModeShared so buf_read/write are direct
      memcpy through `[buffer contents]` -- no blit encoder
      needed on Apple Silicon.  metal_shutdown nils every
      outstanding buffer.  test_metal_real grew from 5 to 27
      checks: round-trip parity, refcount semantics, valid
      buf_id post-free returns -1. -->

- [x] **CONST kernel + dispatch entry point**.  First real Metal
      kernel; establishes the `metal_dispatch_kernel` routing
      table (op -> pipeline state).  `const.metal` shader fills
      the output buffer with a constant.  Parity test: same
      output as CPU's `cpu_op_const`.
      <!-- Pipeline-state cache keyed by opcode (lazy-built on
      first dispatch via [device newComputePipelineState
      WithFunction:]).  Buffer-binding convention: out at
      buffer(0), per-op arg via setBytes at buffer(1), inputs at
      buffer(2..n+1).  Threadgroup size = MIN(numel, pso
      maxTotalThreadsPerThreadgroup); single dispatchThreads call
      with synchronous waitUntilCompleted.  Parity test runs
      UOP_CONST(3.14) under both backends and asserts bit-exact
      output match. -->

- [x] **Binary elementwise Metal kernels** (ADD, MUL, CMPLT).
      All three share the broadcast-aware template (each input
      either repeated from index 0 if numel=1, or indexed by tid).
      One MSL file per op or one templated file.  Per-op parity
      test vs CPU.  Establishes the binary dispatch shape that
      the next item reuses.
      <!-- shaders/binary.metal: single file with a BIN_ELEMENT
      WISE macro instantiating thvm_add / thvm_mul / thvm_cmplt.
      Buffer convention extended: per-input src_numels[] go into
      buffer(2 + n_in + i) via setBytes (one uint each); shader
      uses (na == 1u) ? 0u : tid for the broadcast index.  Three
      parity tests in test_metal_real (each runs the same
      uop_binary on CPU + Metal, bit-compares output buffers). -->

- [x] **Unary elementwise Metal kernels** (NEG, RECIP, SQRT,
      EXP2, LOG2).  Single input, broadcast same way.  Mirrors
      the binary item with one fewer input slot.  Per-op parity
      test vs CPU.
      <!-- shaders/unary.metal: UNARY_ELEMENTWISE macro
      instantiates the five kernels.  Buffer convention same as
      binary, just one input slot (buffer 2) + one numel slot
      (buffer 3).  Parity test loops over the 5 ops; uses 1e-5
      absolute tolerance for transcendentals (Metal's SIMD
      exp2/log2 fast-math path differs from libm in the last
      few ulps). -->

- [x] **Reduction Metal kernel** (REDUCE_SUM + REDUCE_MAX).
      Mirror the recent CPU stride fix: KProgOp.arg packs
      (kind << 24) | inner; the shader loops over axis_size =
      in_numel / out_numel, indexing as
      `outer * (axis_size * inner) + k * inner + inner_idx`.
      Two parity tests (one per kind) on rank-2 inputs with
      axis=0 (non-innermost) to exercise the stride path.
      <!-- Single thvm_reduce shader handles both kinds via the
      arg-packed kind bit.  Uses [[threads_per_grid]] to
      recover out_numel inside the shader so axis_size can be
      computed from in_numel without an extra binding. -->

- [x] **Movement Metal kernels** (EXPAND, RESHAPE).  Both are
      basically memcpy shapes; EXPAND handles the scalar->N
      broadcast and the cycle case (`out[tid] = in[tid %
      in_numel]`); RESHAPE is `out[tid] = in[tid]` with bounds.
      Per-op parity test.  After this lands the MUL+REDUCE
      matmul shape that LinearLayer hits hot is automatically
      covered by the existing MUL + REDUCE kernels chained --
      no separate matmul kernel needed.
      <!-- shaders/movement.metal: thvm_expand uses
      `tid % in_numel` (subsumes scalar->N, copy, cycle); thvm
      _reshape is `out[tid] = in[tid]`.  Two parity tests
      (scalar->5 broadcast, 1D-6 -> 2D-2x3) bit-exact vs CPU.
      Phase 5 is now complete -- all kernels LeNet forward + Adam
      training need have CPU-parity Metal implementations:
      CONST, ADD, MUL, NEG, RECIP, SQRT, EXP2, LOG2, CMPLT,
      REDUCE, EXPAND, RESHAPE.  The remaining LinearLayer
      MUL+REDUCE shape works via op chaining; CONV2D is the
      only LeNet op without a Metal shader yet but the cron
      path can dispatch it via CPU fallback (or land it as a
      v2 task). -->


## Phase 6 — end-to-end

The original two items assumed the goal "TOptim[\"Adam\"] training
LeNet on MNIST" is reachable from here.  It is not, yet:
`interact_grad` ships rules for ADD / MUL / NEG / REDUCE_SUM /
KERNEL only.  Training LeNet end-to-end needs grad rules for at
least CONV2D, RESHAPE, REDUCE_MAX, EXP2 (used by softmax), RECIP
(used in Adam's denom + softmax), and the chain through ReLU /
Pool.  That's a Phase 7+ concern.

Realistic close-out for the overnight cron loop:

- [x] **WL dylib links the Metal backend**.  Today the dylib is
      CPU-only -- `THVM_BACKEND=metal` from WL would pick the C
      stub.  Add the dylib build rule to `-DTHVM_HAS_METAL` +
      link `build/backend_metal.o` and the Metal frameworks on
      Darwin.  Verify a WL forward pass runs with the env var
      set (look for the "metal_init -- device:" line on stderr).
      <!-- Plus a real bug surfaced + fixed: thvm_wl_tensor_from_
      na hardcoded &CPU_BACKEND, so even with THVM_BACKEND=metal
      every WL-created tensor lived in CPU memory while
      dispatch hit Metal -- a buf_id collision crashed the
      kernel.  Switched it to CURRENT_BACKEND, with a memcpy +
      MNumericArray_disown for non-CPU backends (zero-copy
      external buffer remains the CPU fast path).  ADD now
      returns {11, 22, 33, 44} via Metal end-to-end. -->

- [x] **Forward-only LeNet demo**: `wl/Examples/lenet-mnist/
      forward.wls` that loads a few MNIST samples via
      `TMnistBatch[]`, runs `TFromNet[TLeNet[], img]` per sample,
      reports softmax probabilities + which digit class wins.
      Skip training (backprop blocker).  Document the gap in a
      README so future iteration knows what's missing.
      <!-- forward.wls + README.md.  CPU run prints 5 samples
      with random-init predictions (~10% accuracy as expected).
      Metal run (THVM_BACKEND=metal ...) also works end-to-end:
      every kernel except CONV2D (opcode 19) has a Metal
      pipeline, so the conv layer falls through with a stderr
      "no pipeline for opcode 19" note + zeros propagate to a
      uniform softmax.  Graceful degradation rather than a
      crash. -->

- [x] **Document the backprop gap**: list every UOP that needs a
      grad rule in `interact_grad` for Adam-on-LeNet to actually
      train, plus the order to land them.  Goes in
      `docs/grad-roadmap.md`.
      <!-- docs/grad-roadmap.md: enumerates the 8 currently-handled
      rules + the 8 missing ones (RESHAPE, EXPAND, CMPLT, EXP2,
      RECIP, LOG2, REDUCE_MAX, CONV2D), each with the chain-rule
      formula, an LOC estimate, and what it unblocks.  Lands in
      that order so the small unaries + softmax/cross-entropy
      stack get parity-tested on a fully-connected MLP before
      attacking max-pool (REDUCE_MAX needs a new UOP_CMPEQ
      primitive) and finally CONV2D (multi-fire arc, needs FLIP
      and PAD kernels too). -->

- [x] **Grad rule: UOP_RESHAPE** in `interact_grad` per
      `docs/grad-roadmap.md` step 1.  Rule is `GRAD[RESHAPE(a,
      shape), gy, t] = GRAD[a, RESHAPE(gy, a.shape), t]`.  ~15
      LOC + one parity test in `tests/test_grad.c` (analytic
      via interact_grad vs finite-difference, ε=1e-3,
      tolerance 1e-3).
      <!-- Implemented as PASSTHROUGH (uop_grad(a, gy, target))
      not an explicit RESHAPE on the cotangent.  Rationale:
      RESHAPE preserves numel and is identity on data in the
      CPU/Metal kernel, so the leaf rule's expand_to_target
      hits the in_numel == out_numel memcpy branch of
      cpu_op_expand and reconciles shape correctly without an
      explicit cotangent reshape.  Tests: structural in
      tests/test_grad.c (cascades to leaf-EXPAND), numerical
      in wl/THVMLink/Tests/grad.wlt (identity 1d->2d, plus a
      MUL+RESHAPE+REDUCE chain that hits 2a). -->


- [x] **Grad rule: UOP_CMPLT** in `interact_grad` per
      `docs/grad-roadmap.md` step 3.  Rule is
      `GRAD[CMPLT(a, b), gy, t] = grad_zero(t)` -- comparison
      is non-differentiable; the surrounding MUL rule already
      passes the mask through correctly.  ~5 LOC + one parity
      test that verifies a ReLU pattern `MUL[x, CMPLT(0, x)]`
      backprops correctly (gradient = mask).
      <!-- Implemented: 4-line case branch in interact_grad
      returns grad_zero(target).  Tests: structural in
      tests/test_grad.c (CMPLT-direct + ReLU pattern unfolds
      to ADD via MUL rule); numerical in
      wl/THVMLink/Tests/grad.wlt -- TReLU on {-1,2,-3,4}
      yields {0,1,0,1}, plus a direct TUOpCmplt grad = zeros.
      ReLU backprop now works end-to-end.  -->

- [x] **Grad rule: UOP_EXPAND** in `interact_grad` per
      `docs/grad-roadmap.md` step 2.  Rule is
      `GRAD[EXPAND(a, new_shape), gy, t] = GRAD[a,
      REDUCE_SUM along expanded axes (gy), t]`.  ~25 LOC + one
      parity test (e.g. broadcast a scalar to {3,4} then sum;
      grad wrt scalar should equal numel of expanded shape).
      <!-- Implemented with three branches: (1) CONST/NUM source
      short-circuits to grad_zero (constants have no gradient).
      (2) Source shape known and rank > 0: emit REDUCE_SUM along
      each axis where src.dim == 1 < new.dim, in reverse axis
      order so indices stay valid as REDUCE drops axes.  Uses
      term_shape_in (already in src/schedule/shape_env.c) for
      source shape lookup -- handles TAG_TEN and UOP_KERNEL out
      of the box.  (3) Source rank-0 or shape-unknown:
      passthrough; cpu_op_expand's numel-cycling reconciles at
      the leaf.  Tests: structural in tests/test_grad.c
      (CONST short-circuit + shape-{1}->shape-{3} reduce);
      numerical in wl/THVMLink/Tests/grad.wlt (broadcast
      scalar-tensor to {3} with ones cotangent yields {3.0};
      EXPAND-of-CONST yields zero). -->

- [x] **Grad rule: UOP_RECIP** in `interact_grad` per
      `docs/grad-roadmap.md` step 5.  `d(1/x)/dx = -1/x^2`, so
      rule is `GRAD[RECIP(a), gy, t] = GRAD[a, MUL[gy,
      NEG[MUL[RECIP(a), RECIP(a)]]], t]`.  ~15 LOC + one
      structural test in `tests/test_grad.c` and one numerical
      test in `wl/THVMLink/Tests/grad.wlt` (e.g. `1/x` at
      x={2,4} should yield grad = -1/x^2 = {-0.25, -0.0625}).
      <!-- Allocates two independent RECIP(a) nodes for the
      x*x term so the diagram has no shared references (mirrors
      the per-branch lift convention used by the MUL rule).
      Tests: structural (recip cascades to leaf-EXPAND wrapping
      MUL); numerical for 1/{2,4,5} = {-0.25, -0.0625, -0.04}
      with 1e-5 tolerance. -->


- [x] **Grad rule: UOP_EXP2** in `interact_grad` per
      `docs/grad-roadmap.md` step 4.  `d(2^x)/dx = 2^x * ln(2)`,
      so rule is `GRAD[EXP2(a), gy, t] = GRAD[a, MUL[gy,
      MUL[EXP2(a), CONST(ln 2)]], t]`.  ~15 LOC + structural
      test + numerical test (e.g. `2^x` at x={1,2,3} yields
      grad = ln(2) * {2, 4, 8}).
      <!-- ln(2) bits embedded as a CONST via memcpy (no
      runtime float-cast).  Tests: structural (cascades to
      leaf-EXPAND wrapping MUL); numerical d(2^{1,2,3})/dx =
      ln(2) * {2,4,8} within 1e-5. -->


- [x] **Grad rule: UOP_LOG2** in `interact_grad` per
      `docs/grad-roadmap.md` step 6.  `d(log2 x)/dx = 1/(x ln 2)`,
      so rule is `GRAD[LOG2(a), gy, t] = GRAD[a, MUL[gy,
      MUL[RECIP(a), CONST(1/ln 2)]], t]`.  ~15 LOC + structural
      test + numerical test (e.g. `log2(x)` at x={1,2,4} yields
      grad = 1/(x ln 2)).  This is the last piece needed for
      cross-entropy loss (TLog) to backprop end-to-end.
      <!-- 1/ln(2) embedded as a CONST via memcpy of f32 bits.
      Tests: structural (cascades to leaf-EXPAND wrapping MUL);
      numerical d(log2{1,2,4})/dx = 1/(x*ln 2) within 1e-5.
      With this, all 6 unary/movement grad rules (RESHAPE,
      EXPAND, CMPLT, EXP2, RECIP, LOG2) are in -- a Conv-free
      MLP with softmax + cross-entropy can now backprop
      end-to-end.  Next milestones per the roadmap: REDUCE_MAX
      and CONV2D. -->

- [x] **MLP-on-MNIST forward smoke test**: at
      `wl/Examples/mlp-mnist/forward.wls`, build a 2-layer FC net
      (Flatten -> Linear -> ReLU -> Linear -> Softmax) on input
      {1,28,28}, run `TFromNet` on a single MNIST sample, verify
      the output is a 10-vector that sums to ~1.0 and that
      `TCrossEntropyLoss` against a one-hot target produces a
      finite scalar loss.  Pure forward, no training.  Plus a
      minimal README.  Catches any forward-path issues before
      backprop is layered on.
      <!-- forward.wls: 5-layer MLP (Flatten/Linear[32]/Ramp/
      Linear[10]/Softmax), runs on a TMnistBatch[1] sample,
      asserts softmax sum ~ 1.0, all probs numeric, and a
      finite positive cross-entropy loss.  Loss returns as a
      length-1 list (REDUCE drops the axis but the materializer
      reports {} as length-1 buffer); unwrap with First.  Both
      CPU and Metal backends pass. -->


- [ ] **EXPAND heap layout: store ndim explicitly**.  Current
      layout is `[src, NUM(d0), ..., NUM(d_{ndim-1})]` and the
      materializer recovers `ndim` from the SOURCE tensor's
      shape (`expand_output_shape` in
      `src/schedule/materialize.c` and the inline EXPAND case
      in `src/schedule/materialize_in_env.c`).  That works for
      same-rank expansion (the original tinygrad
      MovementOps.EXPAND contract) but is wrong when EXPAND is
      used to add rank -- as `expand_to_target` does in
      `src/interact/uop_grad.c`.  Change layout to
      `[NUM(ndim), src, NUM(d0), ..., NUM(d_{ndim-1})]` so
      ndim is authoritative.  Touches: `src/uop/expand.c`
      (constructor), the two materializers, the EXPAND case
      in `src/interact/uop_grad.c` (heap reads shift by 1),
      `wl/THVMLink/Kernel/Shape.wl` (`tUopShape` walker), and
      every test that touches `uop_expand`'s heap directly
      (currently only `tests/test_grad.c`).  Pure layout
      change; no semantic change for same-rank uses.

- [ ] **Make expand_to_target rank-aware**.  Once EXPAND
      stores ndim explicitly, update
      `src/interact/uop_grad.c::expand_to_target` to call
      `uop_expand` with `target.shape.ndim` regardless of
      source rank -- the materializer will trust the stored
      ndim and produce a correctly-shaped output.  Add a
      regression parity test: a rank-2 leaf-target (e.g.
      `TGrad[t, t]` where `t` has shape {2,3}) should yield
      a {2,3} ones tensor end-to-end through `TRealize`.

- [ ] **RESHAPE heap layout: store ndim explicitly** (same
      rationale, but for RESHAPE).  Current materializer
      recovers ndim by walking dim cells until the running
      product equals input numel -- breaks early when any
      prefix product hits numel (e.g. RESHAPE to a shape
      containing leading 1s).  Same layout change as EXPAND;
      ~50-80 LOC.  Lower priority than the EXPAND fix but
      needed before RESHAPE-prepending becomes a viable
      strategy for cross-rank cotangents.

- [ ] **MLP-on-MNIST single-step gradient check**: extend the
      forward smoke test by computing `TGrad[loss, W]` for each
      of the 4 weight tensors (W1, b1, W2, b2) and asserting
      that `TRealize` produces finite, correctly-shaped
      gradients (no NaN, no shape mismatch).  Pure structural
      sanity; no parameter updates.  **Blocked on the
      rank-changing EXPAND fix above** -- attempt 1 here showed
      rank-2 grads collapse silently / SIGBUS.  Reattempt only
      after that lands.
      <!-- attempt 1: blocked on rank-changing EXPAND.
      grad chain materializes for rank-1 targets (b1, b2 grad
      shapes match) but rank-2 targets (W1, W2) silently
      collapse to a wrong shape (e.g. W2={10,32} -> grad
      shape={10}) and W1 grad crashes with SIGBUS.  Root cause:
      interact_grad's expand_to_target calls uop_expand to lift
      gy to target's rank, but the materializer's
      expand_output_shape reads ndim from the source tensor
      (which has lower rank), losing the extra axes.  Fix
      requires either (a) storing EXPAND's ndim explicitly in
      the heap rather than inferring from source, or
      (b) prepending a RESHAPE in expand_to_target when source
      rank < target rank to add size-1 leading axes -- but (b)
      needs a C-side shape walker for arbitrary UOP gy chains
      (term_shape_in only handles TEN/UOP_KERNEL).  Both fixes
      are bounded but exceed one fire's LOC budget.  Queued
      the fix as the prerequisite item above. -->

- [ ] **MLP-on-MNIST training loop**: with forward + grads
      validated, do K manual SGD steps in pure WL (compute
      grads, update each weight = w - lr * g, recompute loss),
      assert the loss curve trends down on the same fixed batch.
      Won't use TOptim["Adam"] yet (Adam threads state through a
      single weight; multi-tensor MLPs need a per-tensor loop).
      Lives at `wl/Examples/mlp-mnist/train.wls`.

- [ ] **Land grad rule 7: UOP_REDUCE with kind=MAX** in
      `interact_grad`, per `docs/grad-roadmap.md` step 7.  Needs
      a new `UOP_CMPEQ` primitive (mirror of `UOP_CMPLT`) for
      the one-hot argmax mask: `MASK[i] = (a[i] == max)`.
      Rule: `GRAD[REDUCE_MAX(a, axis), gy, t] = GRAD[a,
      MUL[gy_lifted, MASK], t]`.  Likely 60-100 LOC across
      CMPEQ kernel (CPU + Metal) + REDUCE_MAX grad branch +
      tests; will decompose into per-piece sub-items on next
      fire.

- [ ] **Land grad rule 8: UOP_CONV2D** in `interact_grad`, per
      `docs/grad-roadmap.md` step 8.  Three sub-gradients
      (grad_input via transposed conv, grad_weights via
      cross-correlation, grad_bias via REDUCE_SUM).  Needs
      `UOP_FLIP` + `UOP_PAD` kernels (currently opcode-only).
      Multi-fire arc; will decompose into FLIP kernel, PAD
      kernel, and the three grad branches.
