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
- [x] (2026-05-06) Run `wl/Examples/mlp-mnist/train.wls` for
  `N_STEPS=10` BS=32 and capture wall + per-step stats. Save to
  `bench/mlp-mnist/train_bs32.txt`. **Wall 6.02 s for 11 stepGrads
  calls (10 train + 1 final eval), ~547 ms/step including TInit
  reset.** Note: train.wls is BS=1 (single fixed MNIST sample), not
  BS=32; the script's loss-decrease assertion fails (loss climbs
  from 2.25 to 3.60 due to lr=0.05 over a single sample) but
  performance numbers are still valid for forward+backward
  throughput. Lift-reject diagnostics fire on per-output-grad
  reductions (not currently lifted by `kernel_lift_to_uop`).
- [~] (2026-05-06) Use `TFromNet[net]` (no input arg) to rewrite
  `forward.wls` so it hands a `TLam` to `TRealize` directly; verify
  numerics match the existing pipeline. **Blocked**: the
  `TApp[lam, ten]` path is broken on main -- `lam_shape.wlt`
  tests `tlam-jit-on-first-apply` and `tlam-shape-inferred-from-arg`
  both fail with `Missing[NotATensor, UOP]`.  The issue is upstream
  of NN.wl; needs a separate fix in the APP-LAM JIT path before the
  TFromNet[net] form can be exercised end-to-end.

### TLam regression fix (blocking the previous task)
- [x] (2026-05-06) Investigate why `TApp[TLam[w, TUOpAdd[w, w]], ten]`
  materializes to `Missing[NotATensor, UOP]` instead of running the
  JIT-rendered kernel.  **Root cause located** in
  `interact_app_lam`/`thvm_materialize` interaction:
  - Step trace via `TStep`: `APP[LAM[UOP[ADD, DP1, DP0]], TEN[1]]
    → UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]` after one
    APP-LAM step.  Subsequent steps do NOT progress.  The VAR is
    SUB'd to TEN by `heap_subst_var(loc, arg)` but the DP0/DP1 over
    that VAR don't fire to produce the two tensor copies.
  - `TMaterialize[app]` returns `APP[LAM[TEN[1]], TEN[1]]` --
    materialize was called on the body
    `UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]` BEFORE the
    VAR was SUB'd, producing a degenerate TEN[1] (no input slot
    bound) instead of a UOP_KERNEL with a TVAR input.
  - Regression is older than Phase G: the same failure reproduces
    on commit 0fa9d8e (pre-Phase-G).  Likely from earlier in the
    bufferize / kernel-program-cache rework -- needs a wider
    `git bisect` window to localise.

### TLam regression follow-on tasks
- [x] (2026-05-06) Bisect `lam_shape.wlt` `tlam-jit-on-first-apply`
  failure back to the commit that broke it. **First-bad commit:
  `397464e2` "feat: enable auto-dup default-on for non-recursive
  LAMs"** (2026-05-03).  The commit wires
  `thvm_wl_lam_seal_ext` to `lam_seal_ext_with_auto_dup`, so every
  TLam with a non-linear binder gets a DUP chain inserted at C
  construction time -- producing the
  `LAM[UOP[ADD, DP1[65536, VAR[0]], DP0[65536, VAR[0]]]]` shape
  observed in the step trace.  Materialize then sees DP0/DP1 over
  the unbound VAR and collapses to a degenerate TEN[1] without an
  input slot.  Bisect range: 4646189 (good) -> 397464e2 (first bad)
  -> main HEAD (still bad), 8 steps via `git bisect run`.
- [x] (2026-05-06) Design + implement a fix.  **Path (a) landed in
  `lam_seal_ext_with_auto_dup`**: skip auto-dup when the LAM body is
  a non-KERNEL TAG_UOP.  Materialise's kernel input-slot indirection
  handles multiple reads of the bound VAR naturally; the LAM-level
  DUP chain was redundant and actively broke materialise.
  - `lam_shape.wlt` 10/10 green (was 8/10).
  - `make test` 274/274 still green.
  - LinearLayer round-trip via `TFromNet[net]` -> `TApp[lam, x]` ->
    `TRealize` produces correct numerics matching native Wolfram
    forward.

### TFromNet[net] follow-on
- [x] (2026-05-06) NetChain + multi-rank input via `TFromNet[net]`
  works.  Added a TAG_VAR case to `tUopShape` in
  [Shape.wl](../../wl/THVMLink/Kernel/Shape.wl) that reads the
  lam_shape side table via `$termShapeInFn`.  FlattenLayer (and
  every layer-helper that calls tUopShape on a possibly-VAR input)
  now sees the right shape.  Verified: `TFromNet[net] -> TApp -> TRealize`
  on the MLP-MNIST 2-layer NetChain matches `TFromNet[net, x] ->
  TRealize` to FP roundoff (max abs diff ~3e-8).  make test 274/274
  green.

### LeNet-MNIST baseline
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/forward.wls`; capture
  as `bench/lenet-mnist/forward.txt`.  **warmup 185.6 ms, steady
  18.5 ms (avg/4), 75 kernels, dispatch: metal-op=45 (60%),
  metal-tile=30 (40%).** Notable: more than half the kernels still
  fall back through the per-op interpreter path (probably conv2d /
  reshape / specialised layers that don't yet fit the tile path).
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
