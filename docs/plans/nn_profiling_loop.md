# NN Profiling Iteration Loop

Goal: profile the NN examples (mlp-mnist, lenet-mnist) under
THVM_BACKEND=metal + THVM_TILE=1 and improve compile-time / runtime
behaviour using the autotune-via-UOP_OPT path now wired in
[src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c).

Each iteration: pick the first unchecked item, do the work, capture
results in this file, commit, stop. The cron loop fires this prompt
every minute.

## Conventions

- Numbers go inline next to each item once measured.
- `bench/<example>/<phase>.txt` holds the raw stdout for
  reproducibility; commit those too.
- Use `wolframscript -f wl/Examples/...` from the repo root.
- `THVM_BACKEND=metal THVM_TILE=1 THVM_KGC=0` is the default env;
  document deviations.
- Each commit has a tight scope: one item per commit ideal.

## Tasks

### Setup
- [x] (2026-05-06) `TFromNet[net]` returns `TLamShape` with inferred
  input shape — landed in
  [wl/THVMLink/Kernel/NN.wl](../../wl/THVMLink/Kernel/NN.wl).
- [x] (2026-05-06) Autotune opts replay through `kernel_lift_to_uop`
  — landed in
  [src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c).

### MLP-MNIST baseline
- [x] (2026-05-06) Run `wl/Examples/mlp-mnist/forward.wls` under
  `THVM_BACKEND=metal THVM_TILE=1`; capture wall time, kernel count,
  dispatch-kind histogram. Save to `bench/mlp-mnist/forward.txt`.
  **warmup 10.5 ms, steady 8.0 ms (avg/5), 30 kernels, dispatch:
  metal-tile=24, metal-gemm=6.** (forward.wls itself fails the
  cross-entropy positive-loss assertion on this batch, but the
  forward pipeline produces a correct softmax; tracked separately.)
- [ ] Run `wl/Examples/mlp-mnist/train.wls` for `N_STEPS=10` BS=32 and
  capture wall + per-step stats. Save to
  `bench/mlp-mnist/train_bs32.txt`.
- [ ] Use `TFromNet[net]` (no input arg) to rewrite `forward.wls` so
  it hands a `TLam` to `TRealize` directly; verify numerics match
  the existing pipeline.

### LeNet-MNIST baseline
- [ ] Run `wl/Examples/lenet-mnist/forward.wls`; capture as
  `bench/lenet-mnist/forward.txt`.
- [ ] Run `wl/Examples/lenet-mnist/autotune.wls` with
  `MAX_TUNE_KERNELS=All`; capture autotune coverage + winners. Save
  to `bench/lenet-mnist/autotune.txt`.
- [ ] Run `wl/Examples/lenet-mnist/bench-train.wls` BS=32 N_STEPS=5;
  save to `bench/lenet-mnist/train_bs32.txt`.
- [ ] Diff dispatch-kind histogram pre-autotune vs post-autotune;
  document gains (or no-op) in this file.

### Autotune-via-UOP_OPT verification
- [ ] Pick one elementwise kernel in lenet-mnist that proposes
  UPCAST. Apply manually via `TKernelApplyOpt`. Dump rendered MSL
  with `THVM_DUMP_TILE_JIT_SRC=1` and confirm `#pragma unroll(N)`
  fires correctly above the inner for-loop.
- [ ] Same for `LOCAL` — confirm the rendered MSL binds the inner
  axis to `tt` and the outer loop covers `N/L` iterations.
- [ ] Same for `GLOBAL` — confirm `tg` bind.

### Cross-framework comparison
- [ ] Run tinygrad's `examples/beautiful_mnist.py` BS=32 N_STEPS=1 on
  the same hardware; capture wall + Metal kernel count. Cross-
  reference docs/plans/profiling_methodology.md §4.6 numbers.

### Phase G follow-on
- [ ] Once elementwise autotune effects show measurable wins, design
  and implement multi-axis UOP_REDUCE for the reduce-axis path so
  GROUP/GROUPTOP autotune flows through too. Currently skipped in
  `kernel_lift_to_uop` (`has_reduce_axis` short-circuit).
