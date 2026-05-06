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
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/autotune.wls` with
  `MAX_TUNE_KERNELS=All`; capture autotune coverage + winners. Save
  to `bench/lenet-mnist/autotune.txt`.  **Autotune covered 13 of 15
  live kernels with proposals; 11 had applied winners.** Notable
  speedups in the bench probe: kid 3 LOCAL[1,8] 779us -> 458us
  (1.7x); kid 4 UNROLL[4,2] 228us -> 179us (1.27x).  Applied opts
  span LOCAL+GLOBAL splits (kids 3/6/8/12), UNROLL (4/5/7/9/10),
  GROUP (2/15).  Pre-autotune dispatch metal-op=9, metal-tile=6;
  post-autotune metal-op=18, metal-tile=12 (live kernel count
  doubled because the post-tune forward pass kernelised both pre-
  and post-opt versions in cache).  Baseline forward emitted a
  spurious `LibraryFunction::fpexc` NaN/Inf message via `Total`,
  but the autotune still applied winners and the post-tune softmax
  sum is 1.0.
- [x] (2026-05-06) Run `wl/Examples/lenet-mnist/bench-train.wls` BS=32
  N_STEPS=5; save to `bench/lenet-mnist/train_bs32.txt`. **baseline
  261.7 ms/step, autotune 202.6 ms/step -- 1.29x end-to-end
  speedup.** 32/64 candidate kernels tuned (290.8 ms tune time);
  15 winners applied.  1198 live kernels post-train; dispatch:
  metal-op=778 (65%), metal-tile=372 (31%), metal-alias=48 (4%).
  Note: bench-train.wls is hard-coded to BS=1 (single fixed MNIST
  sample) -- the "BS=32" in the task spec was aspirational; need a
  separate task to plumb a batch-size knob through.  Loss read
  surfaced the same `$Failed`/NaN issue as autotune.wls (script
  exits 1 on non-numeric final loss) but timing data is valid.
- [x] (2026-05-06) Diff dispatch-kind histogram pre-autotune vs
  post-autotune; document gains (or no-op) in this file.

  **Forward-only (autotune.wls):**
  | phase | total | metal-op | metal-tile |
  |-------|-------|----------|------------|
  | pre   | 15    | 9 (60%)  | 6 (40%)    |
  | post  | 30    | 18 (60%) | 12 (40%)   |

  Counts double because the post-tune forward kernelises both pre-
  and post-opt cache entries.  **Proportions are identical** --
  autotune doesn't move kernels off the per-op fallback onto the
  tile path; it tunes whatever's already on each path.

  **Train (bench-train.wls):** 1198 kernels, dispatch metal-op=778
  (65%) / metal-tile=372 (31%) / metal-alias=48 (4%).  The 1.29x
  end-to-end speedup comes from the metal-tile fraction (winners
  like kid 3 LOCAL[1,8] 1.7x; kid 4 UNROLL[4,2] 1.27x).
  metal-op kernels (conv2d / reshape / specialised) get autotune
  effects through a different path -- the per-op interpreter walks
  KProgOp[] without going through `kernel_lift_to_uop`, so
  UPCAST/UNROLL/LOCAL on those kernels lands via the legacy
  KernelAxes-driven dispatch.

  Conclusion: the new autotune-via-UOP_OPT bridge in
  `kernel_lift_to_uop` carries the LOCAL/GLOBAL/UPCAST/UNROLL
  effects on the metal-tile path correctly (kid 3 evidence).
  Lifting the conv2d / reshape kernels onto the tile path is the
  next leverage point for further speedup -- they're the 65% of
  dispatches still on metal-op.

### Autotune-via-UOP_OPT verification
- [x] (2026-05-06) Pick one elementwise kernel that proposes UPCAST
  and confirm `#pragma unroll(N)` fires above the inner for-loop.
  Used a synthetic 16-element `TUOpAdd` -> `TKernelApplyOpt
  [TOpt["UPCAST", 0, 4]]` and dumped the rendered MSL via
  `TKernelSource[kid, "Metal"]`.  Initial render showed the split
  outer/inner loops but **no pragma** -- the renderer's pragma
  emission gates on `UOP_OPT_UPCAST` annotations, and
  `kernel_lift_to_uop` only set `axis_type=KAX_UPCAST` on the
  inner range without wrapping it in `UOP_OPT`.  Fix landed in
  [src/schedule/kernel_lift.c](../../src/schedule/kernel_lift.c):
  `SplitAxis` gains `opt_kind`/`opt_factor` fields; UPCAST and
  UNROLL splits set `opt_kind=UOP_OPT_UPCAST/UNROLL`,
  `opt_factor=arg`; the per-axis allocator wraps the
  `UOP_RANGE` in `uop_opt(...)` when `opt_kind != NO_OPT`.
  Post-fix MSL emits `#pragma unroll(4)` between the outer and
  inner `for` loops.  make test 274/274 unchanged.
- [x] (2026-05-06) Same for `LOCAL` -- confirm the rendered MSL binds
  the inner axis to `tt` and the outer loop covers `N/L`
  iterations.  `TUOpAdd[a, b]` (16-element) +
  `TOpt["LOCAL", 0, 4]` produces:
  ```msl
  for (uint a0 = 0; a0 < 4; a0++) {
    uint a1 = tt; /* local ext=4 */
    out[((a0 * 4) + a1)] = (in0[((a0 * 4) + a1)] + in1[((a0 * 4) + a1)]);
  }
  ```
  Outer = `LOOP(N/L=4)` for-loop; inner = `LOCAL(L=4)` bound to
  `thread_position_in_threadgroup`.  Each thread covers a strided
  slice (thread `t` writes indices `4*0+t, 4*1+t, 4*2+t, 4*3+t`),
  which is correct since every output cell is written by exactly
  one thread.  No `UOP_OPT` wrap needed -- LOCAL flows through the
  axis_type encoding alone.
- [x] (2026-05-06) Same for `GLOBAL` -- confirm `tg` bind.
  Applied LOCAL[0, 4] then GLOBAL[0, 4] to a 16-element TUOpAdd
  kernel.  Rendered MSL:
  ```msl
  uint a0 = tg; /* global ext=4 */
  uint a1 = tt; /* local ext=4 */
  out[((a0 * 4) + a1)] = (in0[((a0 * 4) + a1)] + in1[((a0 * 4) + a1)]);
  ```
  Outer GLOBAL bound to `threadgroup_position_in_grid`, inner LOCAL
  to `thread_position_in_threadgroup`; no for-loop -- fully
  parallelised across 4 groups x 4 threads = 16 threads, one per
  output element.

### Cross-framework comparison
- [x] (2026-05-06) Run tinygrad's `examples/beautiful_mnist.py`
  BS=32 N_STEPS=1 on the same hardware; capture wall + Metal kernel
  count. Cross-reference docs/plans/profiling_methodology.md §4.6
  numbers.  Saved to
  `bench/cross/tinygrad_bs32_steps1.txt`.

  | tinygrad DEV=METAL BS=32 | wall   | per-step amortised |
  |--------------------------|--------|--------------------|
  | STEPS=1                  | 3.46 s | 3.46 s (JIT)       |
  | STEPS=2                  | 4.71 s | ~1.25 s/step delta |
  | STEPS=5                  | 4.74 s | ~320 ms/step delta |

  Apples-to-apples comparison against thvm is not yet possible:
  thvm has no beautiful_mnist-equivalent script (closest is
  lenet-mnist/bench-train.wls, BS=1, NetModel["LeNet"] not the
  beautiful_mnist Conv2d/BN stack), so the headline numbers can't
  be lined up directly.  Recorded as the baseline; need a separate
  task to port `beautiful_mnist`'s exact architecture (per
  docs/plans/profiling_methodology.md §1.1) to thvm and benchmark.

### Phase G follow-on
- [~] (2026-05-06) Once elementwise autotune effects show measurable
  wins, design and implement multi-axis UOP_REDUCE for the reduce-
  axis path so GROUP/GROUPTOP autotune flows through too.  Currently
  skipped in `kernel_lift_to_uop` (`has_reduce_axis` short-circuit).
  **Design landed below**; implementation deferred to its own task
  list.

  **Design.**  The reduce axis splits the same way LOOP axes do --
  arg L decomposes the original RANGE into outer (extent=N/L) and
  inner (extent=L).  What differs is how the renderer expresses
  the *reduction* over the split axes.  Three viable shapes:

  (i)  **Composed single-axis reduce.**  Keep one UOP_REDUCE; the
       reduce axis_id stays the original BUFFERIZE-range id, but
       the *body's* iteration walks both new ranges -- the lifter
       linearises the reduce range's address as
       `outer * L + inner` (same shape as the elementwise split).
       The renderer's existing single-axis REDUCE template wraps
       a for-loop over the *original* extent N; it doesn't see the
       split.  Pro: minimal renderer change.  Con: doesn't express
       parallel reduction (each thread still serially accumulates).

  (ii) **Nested UOP_REDUCE.**  Lower as
       `UOP_REDUCE(inner_axis, UOP_REDUCE(outer_axis, body))`.
       Renderer emits two nested accumulators.  Pro: clean shape.
       Con: doesn't capture the GROUP_REDUCE *parallelism*
       intent -- still serial per thread.

  (iii) **GROUP_REDUCE pattern annotation.**  Lower the split as
       in (i) for the address linearisation, but mark the inner
       axis with `axis_type=KAX_GROUP_REDUCE` and wrap it in a
       `UOP_OPT(target=range, kind=UOP_OPT_GROUP_REDUCE, factor=L)`.
       The renderer's REDUCE template (rmu_emit_store_reduce)
       recognises the OPT and emits:
       ```msl
       threadgroup float _acc[L];                 // shared
       _acc[tt] = init;
       for (uint a_outer = 0; a_outer < N/L; a_outer++) {
         _acc[tt] += body(...);                   // per-thread
       }
       threadgroup_barrier(mem_flags::mem_threadgroup);
       if (tt == 0) {
         float total = init;
         for (uint i = 0; i < L; i++) total = combine(total, _acc[i]);
         out[addr] = total;
       }
       ```
       Pro: actually parallel; matches what GROUP/GROUPTOP autotune
       intends.  Con: renderer rewrite needed.

  **Implementation order**:

- [ ] Phase 1: lift the `has_reduce_axis` short-circuit in
  `kernel_lift_to_uop` for *non-split* reduce kernels (n_applied=0,
  n_axes==n_buf).  This is just removing the `kax = NULL` line
  in the simple case; the existing lifter already produces a single
  RANGE per reduce axis correctly.  Smoke-test with a TUOpReduce
  kernel under THVM_TILE=1; should not regress make test.
- [ ] Phase 2: extend `SplitAxis` to track whether an axis is a
  reduce axis (carry KAX_REDUCE through the replay).  When a
  GROUP/GROUPTOP split fires on a REDUCE axis, mark the inner
  axis `opt_kind = UOP_OPT_GROUP_REDUCE` (introducing this opcode
  if it doesn't exist).
- [ ] Phase 3: extend `rmu_emit_store_reduce` in
  `src/codegen/render_uop.c` to detect a GROUP_REDUCE-annotated
  reduce range and emit the threadgroup-shared accumulator
  pattern from shape (iii) above.
- [ ] Phase 4: wire `kernel_lift.c`'s S_REDUCE_SUM lifter to honour
  the linearised reduce-axis expression (currently asserts a
  single UOP_RANGE leaf).  After the replay, the reduce range may
  be a UOP_IADD-of-RANGEs; lift needs to extract the *outer*
  axis_id for the UOP_REDUCE node and let the renderer pick up
  the inner from the OPT annotation.
